#include "Watermark.h"

#include <cstring>
#include <memory>

#include "CKAll.h"

#include "Logger.h"
#include "ReedSolomon.h"
#include "SplitMix64.h"

const uint8_t Watermark::kMasterKey[16] = {
    0x9E, 0x57, 0xB2, 0x6D,
    0x3F, 0xEB, 0x74, 0xA3,
    0xDA, 0x46, 0xF0, 0x09,
    0xCD, 0xB1, 0x1C, 0x88
};

namespace {
    constexpr uint64_t kBlockSeedStride = UINT64_C(0x517CC1B727220A95);

    struct TileRenderState {
        uint64_t messageSeed;
        uint64_t syncSeed;
        const watermark::MessagePermutation *messagePermutation;
        const watermark::MessageSignMask *messageSignMask;
    };

    uint64_t SeedForTemplate(uint64_t baseSeed, int templateIndex) {
        return baseSeed ^ (static_cast<uint64_t>(templateIndex) * kBlockSeedStride);
    }

    TileRenderState BuildTileRenderState(int tileIndexX, int tileIndexY,
                                         uint64_t messageSeed, uint64_t syncSeed) {
        const watermark::TileClass tileClass = watermark::GetTileClassForTile(tileIndexX, tileIndexY);
        return {
            watermark::GetMessageSeedForClass(messageSeed, tileClass),
            watermark::GetSyncSeedForClass(syncSeed, tileClass),
            &watermark::GetMessagePermutation(tileClass),
            &watermark::GetMessageSignMask(tileClass),
        };
    }

    uint8_t GetEncodedMessageBit(const uint8_t coded[utils::kRsCodeLen], const TileRenderState &tileState,
                                 int templateIndex) {
        const int canonicalIndex = (*tileState.messagePermutation)[templateIndex];
        const int byteIdx = canonicalIndex / 8;
        const int bitIdx = 7 - (canonicalIndex % 8);

        uint8_t codedBit = (coded[byteIdx] >> bitIdx) & 1;
        if ((*tileState.messageSignMask)[canonicalIndex]) {
            codedBit ^= 1;
        }
        return codedBit;
    }

    void RenderBlockPixels(uint8_t *pixelsAdd, uint8_t *pixelsSub,
                           int width, int height, int blockOriginX, int blockOriginY,
                           uint64_t baseSeed, int templateIndex, uint8_t codedBit, uint8_t delta) {
        uint64_t pnState = SeedForTemplate(baseSeed, templateIndex);
        for (int py = 0; py < watermark::kBlockSize; ++py) {
            const int screenY = blockOriginY + py;
            if (screenY >= height) break;

            for (int px = 0; px < watermark::kBlockSize; ++px) {
                const int screenX = blockOriginX + px;
                if (screenX >= width) break;

                const uint64_t rng = utils::SplitMix64(pnState);
                const bool pnSign = (rng >> 63) != 0;
                const bool bright = (codedBit != 0) != pnSign;

                const size_t idx = (static_cast<size_t>(screenY) * width + screenX) * 4;
                if (bright) {
                    pixelsAdd[idx + 0] = delta;
                    pixelsAdd[idx + 1] = delta;
                    pixelsAdd[idx + 2] = delta;
                    pixelsAdd[idx + 3] = 255;
                } else {
                    pixelsSub[idx + 0] = delta;
                    pixelsSub[idx + 1] = delta;
                    pixelsSub[idx + 2] = delta;
                    pixelsSub[idx + 3] = 255;
                }
            }
        }
    }

    void RenderTilePixels(uint8_t *pixelsAdd, uint8_t *pixelsSub,
                          int width, int height, int tileX, int tileY,
                          const uint8_t coded[30], uint64_t messageSeed, uint64_t syncSeed) {
        const int tileIndexX = tileX / watermark::kTileWidth;
        const int tileIndexY = tileY / watermark::kTileHeight;
        const TileRenderState tileState = BuildTileRenderState(tileIndexX, tileIndexY, messageSeed, syncSeed);

        for (int blockIdx = 0; blockIdx < watermark::kTileBlockCount; ++blockIdx) {
            const bool isPilot = watermark::IsPilotBlock(blockIdx);
            const int blockCol = blockIdx % watermark::kTileCols;
            const int blockRow = blockIdx / watermark::kTileCols;
            const int blockOriginX = tileX + blockCol * watermark::kBlockSize;
            const int blockOriginY = tileY + blockRow * watermark::kBlockSize;

            const int templateIndex = isPilot
                ? watermark::GetPilotTemplateIndex(blockIdx)
                : watermark::GetMessageTemplateIndex(blockIdx);
            if (templateIndex < 0) continue;

            const uint8_t delta = isPilot ? watermark::kPilotDelta : watermark::kMessageDelta;
            const uint64_t baseSeed = isPilot ? tileState.syncSeed : tileState.messageSeed;
            const uint8_t codedBit = isPilot ? 0 : GetEncodedMessageBit(coded, tileState, templateIndex);

            RenderBlockPixels(pixelsAdd, pixelsSub, width, height, blockOriginX, blockOriginY,
                              baseSeed, templateIndex, codedBit, delta);
        }
    }

} // namespace

CKTexture *Watermark::CreateUploadTexture(CKContext *ctx, CKRenderContext *rc,
                                          const char *name,
                                          const uint8_t *pixels, int width, int height) {
    auto *tex = static_cast<CKTexture *>(ctx->CreateObject(CKCID_TEXTURE, (CKSTRING)name));
    if (!tex) return nullptr;

    tex->ModifyObjectFlags(CK_OBJECT_NOTTOBESAVED | CK_OBJECT_NOTTOBEDELETED, 0);
    if (!tex->Create(width, height)) {
        ctx->DestroyObject(tex);
        return nullptr;
    }

    CKBYTE *surf = tex->LockSurfacePtr();
    if (!surf) {
        ctx->DestroyObject(tex);
        return nullptr;
    }
    memcpy(surf, pixels, static_cast<size_t>(width) * height * 4);
    tex->ReleaseSurfacePtr();

    tex->SetDesiredVideoFormat(_32_ARGB8888);

    if (!rc || !tex->SystemToVideoMemory(rc, TRUE)) {
        ctx->DestroyObject(tex);
        return nullptr;
    }
    return tex;
}

void Watermark::GenerateTextures(CKContext *ctx, int width, int height) {
    if (m_TexAdd) { ctx->DestroyObject(m_TexAdd); m_TexAdd = nullptr; }
    if (m_TexSub) { ctx->DestroyObject(m_TexSub); m_TexSub = nullptr; }

    m_TexWidth = width;
    m_TexHeight = height;

    const size_t pixelBytes = static_cast<size_t>(width) * height * 4;
    auto pixelsAdd = std::make_unique<uint8_t[]>(pixelBytes);
    auto pixelsSub = std::make_unique<uint8_t[]>(pixelBytes);
    memset(pixelsAdd.get(), 0, pixelBytes);
    memset(pixelsSub.get(), 0, pixelBytes);

    for (int tileY = 0; tileY < height; tileY += watermark::kTileHeight) {
        for (int tileX = 0; tileX < width; tileX += watermark::kTileWidth) {
            RenderTilePixels(pixelsAdd.get(), pixelsSub.get(),
                             width, height, tileX, tileY, m_Coded, m_MessageSeed, m_SyncSeed);
        }
    }

    CKRenderContext *rc = ctx->GetPlayerRenderContext();
    m_TexAdd = CreateUploadTexture(ctx, rc, "BML_WatermarkAdd", pixelsAdd.get(), width, height);
    m_TexSub = CreateUploadTexture(ctx, rc, "BML_WatermarkSub", pixelsSub.get(), width, height);
}

void Watermark::Init(CKContext *ctx) {
    if (!m_PayloadBuilt) {
        watermark::WatermarkIdentity identity = watermark::BuildDefaultIdentity();
        if (watermark::HasZeroTraceId(identity)) {
            if (Logger *logger = Logger::GetDefault()) {
                logger->Warn(
                    "Watermark trace_id fallback is all-zero because hardware fingerprint acquisition failed.");
            }
        }

        uint8_t payload[14] = {};
        watermark::BuildPayload(identity, payload);
        utils::RsEncode(payload, m_Coded);

        watermark::DerivedWatermarkKeys keys = watermark::DeriveWatermarkKeys(kMasterKey, identity.buildId);
        m_MessageSeed = keys.messageSeed;
        m_SyncSeed = keys.syncSeed;
        m_PayloadBuilt = true;
    }

    CKRenderContext *rc = ctx->GetPlayerRenderContext();
    if (rc) {
        GenerateTextures(ctx, rc->GetWidth(), rc->GetHeight());
    }
}

void Watermark::RegenerateTexture(CKContext *ctx) {
    CKRenderContext *rc = ctx->GetPlayerRenderContext();
    if (rc) {
        GenerateTextures(ctx, rc->GetWidth(), rc->GetHeight());
    }
}

void Watermark::Shutdown(CKContext *ctx) {
    if (m_TexAdd) { ctx->DestroyObject(m_TexAdd); m_TexAdd = nullptr; }
    if (m_TexSub) { ctx->DestroyObject(m_TexSub); m_TexSub = nullptr; }
}

void Watermark::Draw(CKRenderContext *dev) {
    if (!m_TexAdd || !m_TexSub) return;


    // Save all render states we modify
    struct { VXRENDERSTATETYPE type; CKDWORD value; } savedStates[] = {
        {VXRENDERSTATE_FILLMODE, dev->GetState(VXRENDERSTATE_FILLMODE)},
        {VXRENDERSTATE_SHADEMODE, dev->GetState(VXRENDERSTATE_SHADEMODE)},
        {VXRENDERSTATE_CULLMODE, dev->GetState(VXRENDERSTATE_CULLMODE)},
        {VXRENDERSTATE_WRAP0, dev->GetState(VXRENDERSTATE_WRAP0)},
        {VXRENDERSTATE_ALPHABLENDENABLE, dev->GetState(VXRENDERSTATE_ALPHABLENDENABLE)},
        {VXRENDERSTATE_SRCBLEND, dev->GetState(VXRENDERSTATE_SRCBLEND)},
        {VXRENDERSTATE_DESTBLEND, dev->GetState(VXRENDERSTATE_DESTBLEND)},
        {VXRENDERSTATE_BLENDOP, dev->GetState(VXRENDERSTATE_BLENDOP)},
        {VXRENDERSTATE_ALPHATESTENABLE, dev->GetState(VXRENDERSTATE_ALPHATESTENABLE)},
        {VXRENDERSTATE_ZWRITEENABLE, dev->GetState(VXRENDERSTATE_ZWRITEENABLE)},
        {VXRENDERSTATE_ZENABLE, dev->GetState(VXRENDERSTATE_ZENABLE)},
        {VXRENDERSTATE_FOGENABLE, dev->GetState(VXRENDERSTATE_FOGENABLE)},
        {VXRENDERSTATE_SPECULARENABLE, dev->GetState(VXRENDERSTATE_SPECULARENABLE)},
        {VXRENDERSTATE_STENCILENABLE, dev->GetState(VXRENDERSTATE_STENCILENABLE)},
        {VXRENDERSTATE_CLIPPING, dev->GetState(VXRENDERSTATE_CLIPPING)},
        {VXRENDERSTATE_LIGHTING, dev->GetState(VXRENDERSTATE_LIGHTING)},
    };
    VxRect savedViewport;
    dev->GetViewRect(savedViewport);

    // Setup render state (mirrors ImGui backend, except SRCBLEND=ONE for additive)
    const float fw = static_cast<float>(m_TexWidth);
    const float fh = static_cast<float>(m_TexHeight);
    VxRect viewport(0, 0, fw, fh);
    dev->SetViewRect(viewport);

    dev->SetState(VXRENDERSTATE_FILLMODE, VXFILL_SOLID);
    dev->SetState(VXRENDERSTATE_SHADEMODE, VXSHADE_GOURAUD);
    dev->SetState(VXRENDERSTATE_CULLMODE, VXCULL_NONE);
    dev->SetState(VXRENDERSTATE_WRAP0, 0);
    dev->SetState(VXRENDERSTATE_ALPHABLENDENABLE, TRUE);
    dev->SetState(VXRENDERSTATE_SRCBLEND, VXBLEND_ONE);
    dev->SetState(VXRENDERSTATE_DESTBLEND, VXBLEND_ONE);
    dev->SetState(VXRENDERSTATE_BLENDOP, VXBLENDOP_ADD);
    dev->SetState(VXRENDERSTATE_ALPHATESTENABLE, FALSE);
    dev->SetState(VXRENDERSTATE_ZWRITEENABLE, FALSE);
    dev->SetState(VXRENDERSTATE_ZENABLE, FALSE);
    dev->SetState(VXRENDERSTATE_FOGENABLE, FALSE);
    dev->SetState(VXRENDERSTATE_SPECULARENABLE, FALSE);
    dev->SetState(VXRENDERSTATE_STENCILENABLE, FALSE);
    dev->SetState(VXRENDERSTATE_CLIPPING, TRUE);
    dev->SetState(VXRENDERSTATE_LIGHTING, FALSE);

    dev->SetTextureStageState(CKRST_TSS_ADDRESS, VXTEXTURE_ADDRESSCLAMP);
    dev->SetTextureStageState(CKRST_TSS_TEXTUREMAPBLEND, VXTEXTUREBLEND_MODULATEALPHA);
    dev->SetTextureStageState(CKRST_TSS_STAGEBLEND, 0, 1);
    dev->SetTextureStageState(CKRST_TSS_MINFILTER, VXTEXTUREFILTER_LINEAR);
    dev->SetTextureStageState(CKRST_TSS_MAGFILTER, VXTEXTUREFILTER_LINEAR);

    // Full-screen quad with UV covering content area of (possibly padded) texture
    VxImageDescEx texDesc;
    float uMax = 1.0f, vMax = 1.0f;
    if (m_TexAdd->GetVideoTextureDesc(texDesc) && texDesc.Width > 0 && texDesc.Height > 0) {
        uMax = fw / static_cast<float>(texDesc.Width);
        vMax = fh / static_cast<float>(texDesc.Height);
    }

    VxDrawPrimitiveData *data = dev->GetDrawPrimitiveStructure(CKRST_DP_CL_VCT, 4);
    if (data) {
        XPtrStrided<VxVector4> pos(data->PositionPtr, data->PositionStride);
        XPtrStrided<CKDWORD> col(data->ColorPtr, data->ColorStride);
        XPtrStrided<VxUV> uv(data->TexCoordPtr, data->TexCoordStride);

        pos[0].Set(0,  0,  0, 1); col[0] = 0xFFFFFFFF; uv[0].u = 0;    uv[0].v = 0;
        pos[1].Set(fw, 0,  0, 1); col[1] = 0xFFFFFFFF; uv[1].u = uMax; uv[1].v = 0;
        pos[2].Set(fw, fh, 0, 1); col[2] = 0xFFFFFFFF; uv[2].u = uMax; uv[2].v = vMax;
        pos[3].Set(0,  fh, 0, 1); col[3] = 0xFFFFFFFF; uv[3].u = 0;    uv[3].v = vMax;

        CKWORD indices[] = {0, 1, 2, 0, 2, 3};

        // Pass 1: Additive -- output = bg + src_rgb
        dev->SetTexture(m_TexAdd);
        dev->DrawPrimitive(VX_TRIANGLELIST, indices, 6, data);

        // Pass 2: Reverse-subtract -- output = bg - src_rgb
        dev->SetState(VXRENDERSTATE_BLENDOP, VXBLENDOP_REVSUBTRACT);
        dev->SetTexture(m_TexSub);
        dev->DrawPrimitive(VX_TRIANGLELIST, indices, 6, data);
    }

    // Restore all saved states
    for (const auto &s : savedStates)
        dev->SetState(s.type, s.value);
    dev->SetViewRect(savedViewport);
}

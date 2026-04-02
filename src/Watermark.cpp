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
                           uint64_t baseSeed, int templateIndex, uint8_t codedBit, uint8_t alpha) {
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
                    pixelsAdd[idx + 0] = 255;
                    pixelsAdd[idx + 1] = 255;
                    pixelsAdd[idx + 2] = 255;
                    pixelsAdd[idx + 3] = alpha;
                } else {
                    pixelsSub[idx + 0] = 255;
                    pixelsSub[idx + 1] = 255;
                    pixelsSub[idx + 2] = 255;
                    pixelsSub[idx + 3] = alpha;
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

            const uint8_t alpha = isPilot ? watermark::kPilotDelta : watermark::kMessageDelta;
            const uint64_t baseSeed = isPilot ? tileState.syncSeed : tileState.messageSeed;
            const uint8_t codedBit = isPilot ? 0 : GetEncodedMessageBit(coded, tileState, templateIndex);

            RenderBlockPixels(pixelsAdd, pixelsSub, width, height, blockOriginX, blockOriginY,
                              baseSeed, templateIndex, codedBit, alpha);
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

    const int w = dev->GetWidth();
    const int h = dev->GetHeight();

    // Save all render states we touch
    const CKDWORD sFillMode   = dev->GetState(VXRENDERSTATE_FILLMODE);
    const CKDWORD sShadeMode  = dev->GetState(VXRENDERSTATE_SHADEMODE);
    const CKDWORD sCullMode   = dev->GetState(VXRENDERSTATE_CULLMODE);
    const CKDWORD sAlphaBlend = dev->GetState(VXRENDERSTATE_ALPHABLENDENABLE);
    const CKDWORD sSrcBlend   = dev->GetState(VXRENDERSTATE_SRCBLEND);
    const CKDWORD sDestBlend  = dev->GetState(VXRENDERSTATE_DESTBLEND);
    const CKDWORD sBlendOp    = dev->GetState(VXRENDERSTATE_BLENDOP);
    const CKDWORD sZEnable    = dev->GetState(VXRENDERSTATE_ZENABLE);
    const CKDWORD sZWrite     = dev->GetState(VXRENDERSTATE_ZWRITEENABLE);
    const CKDWORD sLighting   = dev->GetState(VXRENDERSTATE_LIGHTING);
    const CKDWORD sFog        = dev->GetState(VXRENDERSTATE_FOGENABLE);

    // Set 2D overlay states
    dev->SetState(VXRENDERSTATE_FILLMODE, VXFILL_SOLID);
    dev->SetState(VXRENDERSTATE_SHADEMODE, VXSHADE_GOURAUD);
    dev->SetState(VXRENDERSTATE_CULLMODE, VXCULL_NONE);
    dev->SetState(VXRENDERSTATE_ALPHABLENDENABLE, TRUE);
    dev->SetState(VXRENDERSTATE_SRCBLEND, VXBLEND_SRCALPHA);
    dev->SetState(VXRENDERSTATE_DESTBLEND, VXBLEND_ONE);
    dev->SetState(VXRENDERSTATE_BLENDOP, VXBLENDOP_ADD);
    dev->SetState(VXRENDERSTATE_ZENABLE, FALSE);
    dev->SetState(VXRENDERSTATE_ZWRITEENABLE, FALSE);
    dev->SetState(VXRENDERSTATE_LIGHTING, FALSE);
    dev->SetState(VXRENDERSTATE_FOGENABLE, FALSE);
    dev->SetTextureStageState(CKRST_TSS_TEXTUREMAPBLEND, VXTEXTUREBLEND_MODULATEALPHA);
    dev->SetTextureStageState(CKRST_TSS_ADDRESS, VXTEXTURE_ADDRESSCLAMP);
    dev->SetTextureStageState(CKRST_TSS_MINFILTER, VXTEXTUREFILTER_NEAREST);
    dev->SetTextureStageState(CKRST_TSS_MAGFILTER, VXTEXTUREFILTER_NEAREST);

    // Full-screen quad
    VxDrawPrimitiveData *data = dev->GetDrawPrimitiveStructure(CKRST_DP_CL_VCT, 4);
    if (data) {
        XPtrStrided<VxVector4> pos(data->PositionPtr, data->PositionStride);
        XPtrStrided<CKDWORD> col(data->ColorPtr, data->ColorStride);
        XPtrStrided<VxUV> uv(data->TexCoordPtr, data->TexCoordStride);

        const float fw = static_cast<float>(w);
        const float fh = static_cast<float>(h);

        pos[0].Set(0,  0,  0, 1); col[0] = 0xFFFFFFFF; uv[0].u = 0; uv[0].v = 0;
        pos[1].Set(fw, 0,  0, 1); col[1] = 0xFFFFFFFF; uv[1].u = 1; uv[1].v = 0;
        pos[2].Set(fw, fh, 0, 1); col[2] = 0xFFFFFFFF; uv[2].u = 1; uv[2].v = 1;
        pos[3].Set(0,  fh, 0, 1); col[3] = 0xFFFFFFFF; uv[3].u = 0; uv[3].v = 1;

        CKWORD indices[] = {0, 1, 2, 0, 2, 3};

        // Pass 1: Additive — output = bg + src * srcAlpha
        dev->SetTexture(m_TexAdd);
        dev->DrawPrimitive(VX_TRIANGLELIST, indices, 6, data);

        // Pass 2: Reverse-subtract — output = bg - src * srcAlpha
        dev->SetState(VXRENDERSTATE_BLENDOP, VXBLENDOP_REVSUBTRACT);
        dev->SetTexture(m_TexSub);
        dev->DrawPrimitive(VX_TRIANGLELIST, indices, 6, data);
    }

    // Restore all saved states
    dev->SetState(VXRENDERSTATE_FILLMODE, sFillMode);
    dev->SetState(VXRENDERSTATE_SHADEMODE, sShadeMode);
    dev->SetState(VXRENDERSTATE_CULLMODE, sCullMode);
    dev->SetState(VXRENDERSTATE_ALPHABLENDENABLE, sAlphaBlend);
    dev->SetState(VXRENDERSTATE_SRCBLEND, sSrcBlend);
    dev->SetState(VXRENDERSTATE_DESTBLEND, sDestBlend);
    dev->SetState(VXRENDERSTATE_BLENDOP, sBlendOp);
    dev->SetState(VXRENDERSTATE_ZENABLE, sZEnable);
    dev->SetState(VXRENDERSTATE_ZWRITEENABLE, sZWrite);
    dev->SetState(VXRENDERSTATE_LIGHTING, sLighting);
    dev->SetState(VXRENDERSTATE_FOGENABLE, sFog);
}

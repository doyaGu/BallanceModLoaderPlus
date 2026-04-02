#include "Watermark.h"

#include <cstring>
#include <memory>

#include "CKAll.h"
#include "imgui.h"

#include "Logger.h"
#include "ReedSolomon.h"
#include "SplitMix64.h"

// Embedded master key bytes used by the offline blind detector path.
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

    void RenderBlockPixels(uint8_t *pixels, int width, int height, int blockOriginX, int blockOriginY,
                           uint64_t baseSeed, int templateIndex, uint8_t codedBit, uint8_t alpha) {
        uint64_t pnState = SeedForTemplate(baseSeed, templateIndex);
        for (int py = 0; py < watermark::kBlockSize; ++py) {
            const int screenY = blockOriginY + py;
            if (screenY >= height) {
                break;
            }

            for (int px = 0; px < watermark::kBlockSize; ++px) {
                const int screenX = blockOriginX + px;
                if (screenX >= width) {
                    break;
                }

                const uint64_t rng = utils::SplitMix64(pnState);
                const bool pnSign = (rng >> 63) != 0;
                // codedBit is 0 for pilot blocks, so (false != pnSign) == pnSign
                const bool bright = (codedBit != 0) != pnSign;
                const uint8_t rgb = bright ? 255 : 0;

                const size_t idx = (static_cast<size_t>(screenY) * width + screenX) * 4;
                pixels[idx + 0] = rgb;
                pixels[idx + 1] = rgb;
                pixels[idx + 2] = rgb;
                pixels[idx + 3] = alpha;
            }
        }
    }

    void RenderTilePixels(uint8_t *pixels, int width, int height, int tileX, int tileY, const uint8_t coded[30],
                          uint64_t messageSeed, uint64_t syncSeed) {
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
            if (templateIndex < 0) {
                continue;
            }

            const uint8_t alpha = isPilot ? watermark::kPilotDelta : watermark::kMessageDelta;
            const uint64_t baseSeed = isPilot ? tileState.syncSeed : tileState.messageSeed;
            const uint8_t codedBit = isPilot ? 0 : GetEncodedMessageBit(coded, tileState, templateIndex);

            RenderBlockPixels(pixels, width, height, blockOriginX, blockOriginY,
                              baseSeed, templateIndex, codedBit, alpha);
        }
    }
} // namespace

void Watermark::GenerateTexture(CKContext *ctx, int width, int height) {
    if (m_Texture) {
        ctx->DestroyObject(m_Texture);
        m_Texture = nullptr;
    }

    m_TexWidth = width;
    m_TexHeight = height;

    // Allocate CPU pixel buffer
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    auto pixels = std::make_unique<uint8_t[]>(pixelCount * 4);
    memset(pixels.get(), 0, pixelCount * 4); // fully transparent black

    // For each tile copy that fits on screen
    for (int tileY = 0; tileY < height; tileY += watermark::kTileHeight) {
        for (int tileX = 0; tileX < width; tileX += watermark::kTileWidth) {
            RenderTilePixels(pixels.get(), width, height, tileX, tileY, m_Coded, m_MessageSeed, m_SyncSeed);
        }
    }

    // Create CKTexture
    m_Texture = static_cast<CKTexture *>(
        ctx->CreateObject(CKCID_TEXTURE, (CKSTRING)"BML_WatermarkTex"));
    if (!m_Texture) {
        return;
    }

    m_Texture->ModifyObjectFlags(CK_OBJECT_NOTTOBESAVED | CK_OBJECT_NOTTOBEDELETED, 0);
    if (!m_Texture->Create(width, height)) {
        ctx->DestroyObject(m_Texture);
        m_Texture = nullptr;
        return;
    }

    CKBYTE *surf = m_Texture->LockSurfacePtr();
    if (!surf) {
        ctx->DestroyObject(m_Texture);
        m_Texture = nullptr;
        return;
    }
    memcpy(surf, pixels.get(), pixelCount * 4);
    m_Texture->ReleaseSurfacePtr();

    m_Texture->SetDesiredVideoFormat(_32_ARGB8888);

    CKRenderContext *rc = ctx->GetPlayerRenderContext();
    if (!rc || !m_Texture->SystemToVideoMemory(rc, TRUE)) {
        ctx->DestroyObject(m_Texture);
        m_Texture = nullptr;
        return;
    }
}

void Watermark::Init(CKContext *ctx) {
    // Build payload once per session (timestamp fixed at first Init)
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

    // Generate texture at current render resolution
    CKRenderContext *rc = ctx->GetPlayerRenderContext();
    if (rc) {
        int w = rc->GetWidth();
        int h = rc->GetHeight();
        GenerateTexture(ctx, w, h);
    }
}

void Watermark::RegenerateTexture(CKContext *ctx) {
    CKRenderContext *rc = ctx->GetPlayerRenderContext();
    if (rc) {
        GenerateTexture(ctx, rc->GetWidth(), rc->GetHeight());
    }
}

void Watermark::Shutdown(CKContext *ctx) {
    if (m_Texture) {
        ctx->DestroyObject(m_Texture);
        m_Texture = nullptr;
    }
}

void Watermark::Draw() {
    if (!m_Texture) return;

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImDrawList *fg = ImGui::GetForegroundDrawList();
    ImTextureID texID = reinterpret_cast<ImTextureID>(m_Texture);
    fg->AddImage(texID, ImVec2(0, 0), displaySize);
}

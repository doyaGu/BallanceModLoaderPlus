#include "Watermark.h"

#include <cstring>
#include <memory>

#include "CKAll.h"
#include "imgui.h"

#include "ReedSolomon.h"
#include "SplitMix64.h"

// Obfuscated master key (XOR with 0xA5 for basic obfuscation)
const uint8_t Watermark::kMasterKey[16] = {
    0x3B ^ 0xA5, 0xF2 ^ 0xA5, 0x17 ^ 0xA5, 0xC8 ^ 0xA5,
    0x9A ^ 0xA5, 0x4E ^ 0xA5, 0xD1 ^ 0xA5, 0x06 ^ 0xA5,
    0x7F ^ 0xA5, 0xE3 ^ 0xA5, 0x55 ^ 0xA5, 0xAC ^ 0xA5,
    0x68 ^ 0xA5, 0x14 ^ 0xA5, 0xB9 ^ 0xA5, 0x2D ^ 0xA5
};

namespace {
    constexpr uint64_t kBlockSeedStride = UINT64_C(0x9E3779B97F4A7C15);

    uint64_t SeedForTemplate(uint64_t baseSeed, int templateIndex) {
        return baseSeed ^ (static_cast<uint64_t>(templateIndex) * kBlockSeedStride);
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
            // For each block in the tile
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
                const uint64_t baseSeed = isPilot ? m_SyncSeed : m_MessageSeed;
                uint64_t pnState = SeedForTemplate(baseSeed, templateIndex);

                uint8_t codedBit = 0;
                if (!isPilot) {
                    const int byteIdx = templateIndex / 8;
                    const int bitIdx = 7 - (templateIndex % 8);
                    codedBit = (m_Coded[byteIdx] >> bitIdx) & 1;
                }

                for (int py = 0; py < watermark::kBlockSize; ++py) {
                    int screenY = blockOriginY + py;
                    if (screenY >= height) break;
                    for (int px = 0; px < watermark::kBlockSize; ++px) {
                        int screenX = blockOriginX + px;
                        if (screenX >= width) break;

                        uint64_t rng = utils::SplitMix64(pnState);
                        const bool pnSign = (rng >> 63) != 0;
                        const bool codedBitSet = codedBit != 0;

                        const bool bright = isPilot ? pnSign : (codedBitSet != pnSign);
                        const uint8_t rgb = bright ? 255 : 0;

                        size_t idx = (static_cast<size_t>(screenY) * width + screenX) * 4;
                        pixels[idx + 0] = rgb;   // B (BGRA)
                        pixels[idx + 1] = rgb;   // G
                        pixels[idx + 2] = rgb;   // R
                        pixels[idx + 3] = alpha; // A
                    }
                }
            }
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

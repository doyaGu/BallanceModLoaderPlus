#ifndef BML_WATERMARK_H
#define BML_WATERMARK_H

#include <cstdint>

#include "WatermarkData.h"

class CKContext;
class CKRenderContext;
class CKTexture;

class Watermark {
public:
    Watermark() = default;
    ~Watermark() = default;

    Watermark(const Watermark &) = delete;
    Watermark &operator=(const Watermark &) = delete;

    void Init(CKContext *ctx);
    void Shutdown(CKContext *ctx);
    void Draw(CKRenderContext *dev);

    void OnResolutionChanged(CKContext *ctx);

    bool HasTextures() const { return m_TexAdd != nullptr && m_TexSub != nullptr; }

private:
    void DestroyTextures(CKContext *ctx);
    bool GenerateTextures(CKContext *ctx, int width, int height);
    static CKTexture *CreateUploadTexture(CKContext *ctx, CKRenderContext *rc,
                                          const char *name,
                                          const uint8_t *pixels, int width, int height);

    CKTexture *m_TexAdd = nullptr;
    CKTexture *m_TexSub = nullptr;
    int m_TexWidth = 0;
    int m_TexHeight = 0;
    bool m_PayloadBuilt = false;
    uint64_t m_MessageSeed = 0;
    uint64_t m_SyncSeed = 0;

    uint8_t m_Coded[30] = {};

    static const uint8_t kMasterKey[16];
};

#endif // BML_WATERMARK_H

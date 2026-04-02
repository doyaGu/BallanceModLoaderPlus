#ifndef BML_WATERMARK_H
#define BML_WATERMARK_H

#include <cstdint>

#include "WatermarkData.h"

class CKContext;
class CKTexture;

class Watermark {
public:
    void Init(CKContext *ctx);
    void RegenerateTexture(CKContext *ctx);
    void Shutdown(CKContext *ctx);
    void Draw();

private:
    void GenerateTexture(CKContext *ctx, int width, int height);

    CKTexture *m_Texture = nullptr;
    int m_TexWidth = 0;
    int m_TexHeight = 0;
    bool m_PayloadBuilt = false;
    uint64_t m_MessageSeed = 0;
    uint64_t m_SyncSeed = 0;

    uint8_t m_Coded[30] = {};

    static const uint8_t kMasterKey[16];
};

#endif // BML_WATERMARK_H

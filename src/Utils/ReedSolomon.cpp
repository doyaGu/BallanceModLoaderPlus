#include "ReedSolomon.h"

#include <cstring>

namespace utils {

// Note: This implementation is NOT thread-safe. All functions must be called
// from a single thread (the game's main thread in BML's case).

// Precomputed log/exp tables for GF(2^8) with primitive polynomial 0x11D
static uint8_t g_Exp[512]; // exp table (doubled for convenience)
static uint8_t g_Log[256]; // log table
static bool g_TablesInit = false;

static void InitTables() {
    if (g_TablesInit) return;
    int x = 1;
    for (int i = 0; i < 255; ++i) {
        g_Exp[i] = static_cast<uint8_t>(x);
        g_Log[x] = static_cast<uint8_t>(i);
        x <<= 1;
        if (x & 0x100) x ^= 0x11D;
    }
    for (int i = 255; i < 512; ++i)
        g_Exp[i] = g_Exp[i - 255];
    g_Log[0] = 0; // convention: log(0) = 0, guarded in usage
    g_TablesInit = true;
}

uint8_t GfMul(uint8_t a, uint8_t b) {
    if (a == 0 || b == 0) return 0;
    InitTables();
    return g_Exp[g_Log[a] + g_Log[b]];
}

uint8_t GfPow(uint8_t base, int exp) {
    InitTables();
    if (base == 0) return 0;
    int logBase = g_Log[base];
    int logResult = (logBase * exp) % 255;
    if (logResult < 0) logResult += 255;
    return g_Exp[logResult];
}

uint8_t GfInv(uint8_t a) {
    InitTables();
    if (a == 0) return 0;
    return g_Exp[255 - g_Log[a]];
}

// Generator polynomial for RS(30,14): product of (x - alpha^i) for i = 0..15
// alpha = 2 (primitive element)
static void BuildGeneratorPoly(uint8_t *gen) {
    InitTables();
    // gen starts as [1]
    uint8_t tmp[kRsParityLen + 1];
    memset(gen, 0, (kRsParityLen + 1));
    gen[0] = 1;
    int genLen = 1;

    for (int i = 0; i < kRsParityLen; ++i) {
        uint8_t root = GfPow(2, i); // alpha^i
        memset(tmp, 0, genLen + 1);
        for (int j = 0; j < genLen; ++j) {
            tmp[j] ^= GfMul(gen[j], root);
            tmp[j + 1] ^= gen[j];
        }
        genLen++;
        memcpy(gen, tmp, genLen);
    }
}

void RsEncode(const uint8_t *data, uint8_t *out) {
    InitTables();

    static uint8_t gen[kRsParityLen + 1];
    static bool genInit = false;
    if (!genInit) {
        BuildGeneratorPoly(gen);
        genInit = true;
    }

    // Copy data to output (systematic encoding: data first, parity appended)
    memcpy(out, data, kRsDataLen);
    memset(out + kRsDataLen, 0, kRsParityLen);

    // Polynomial division
    uint8_t feedback;
    for (int i = 0; i < kRsDataLen; ++i) {
        feedback = out[i];
        if (feedback != 0) {
            for (int j = 1; j <= kRsParityLen; ++j) {
                out[i + j] ^= GfMul(gen[j], feedback);
            }
        }
    }

    // Restore data (division overwrote positions 0..kRsDataLen-1)
    memcpy(out, data, kRsDataLen);
}

} // namespace utils

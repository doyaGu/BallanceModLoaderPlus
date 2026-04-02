#ifndef BML_REED_SOLOMON_H
#define BML_REED_SOLOMON_H

#include <cstddef>
#include <cstdint>

// RS(30, 14) over GF(2^8) with primitive polynomial 0x11D.
// 14 data bytes -> 30 coded bytes. Corrects up to 8 byte errors.

namespace utils {
    uint8_t GfMul(uint8_t a, uint8_t b);
    uint8_t GfPow(uint8_t base, int exp);
    uint8_t GfInv(uint8_t a);

    void RsEncode(const uint8_t *data, uint8_t *out);

    constexpr int kRsDataLen = 14;
    constexpr int kRsCodeLen = 30;
    constexpr int kRsParityLen = 16; // kRsCodeLen - kRsDataLen
} // namespace utils

#endif // BML_REED_SOLOMON_H

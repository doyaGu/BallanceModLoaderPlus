#ifndef BML_SPLITMIX64_H
#define BML_SPLITMIX64_H

#include <cstdint>

namespace utils {
    inline uint64_t SplitMix64(uint64_t &state) {
        state += UINT64_C(0x9E3779B97F4A7C15);
        uint64_t z = state;
        z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
        z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
        return z ^ (z >> 31);
    }
} // namespace utils

#endif // BML_SPLITMIX64_H

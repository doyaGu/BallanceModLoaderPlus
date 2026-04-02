#ifndef BML_SPLITMIX64_H
#define BML_SPLITMIX64_H

#include <cstdint>

namespace utils {
    // Pure finalizer: mixes a 64-bit value without stepping state.
    [[nodiscard]] constexpr uint64_t SplitMix64Once(uint64_t z) {
        z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
        z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
        return z ^ (z >> 31);
    }

    // Stateful PRNG: increments state then finalizes.
    inline uint64_t SplitMix64(uint64_t &state) {
        state += UINT64_C(0x9E3779B97F4A7C15);
        return SplitMix64Once(state);
    }
} // namespace utils

#endif // BML_SPLITMIX64_H

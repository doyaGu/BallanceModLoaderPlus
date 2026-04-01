#include "HashUtils.h"

#include <cstdio>

namespace utils {
    uint64_t Fnv1a64(const void *data, size_t size) {
        constexpr uint64_t kOffsetBasis = 14695981039346656037ull;
        constexpr uint64_t kPrime = 1099511628211ull;

        uint64_t hash = kOffsetBasis;
        const auto *p = static_cast<const uint8_t *>(data);
        for (size_t i = 0; i < size; ++i) {
            hash ^= p[i];
            hash *= kPrime;
        }
        return hash;
    }

    uint32_t Fnv1a32(const void *data, size_t size) {
        constexpr uint32_t kOffsetBasis = 2166136261u;
        constexpr uint32_t kPrime = 16777619u;

        uint32_t hash = kOffsetBasis;
        const auto *p = static_cast<const uint8_t *>(data);
        for (size_t i = 0; i < size; ++i) {
            hash ^= p[i];
            hash *= kPrime;
        }
        return hash;
    }

    std::string ToHex64(uint64_t value) {
        char buf[17];
        std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(value));
        return buf;
    }

    std::string ToHex32(uint32_t value) {
        char buf[9];
        std::snprintf(buf, sizeof(buf), "%08x", static_cast<unsigned>(value));
        return buf;
    }
} // namespace utils

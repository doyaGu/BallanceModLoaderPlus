#include "HashUtils.h"

#include <cstdio>

namespace {
    constexpr uint64_t kFnv1a64OffsetBasis = 14695981039346656037ull;
    constexpr uint64_t kFnv1a64Prime = 1099511628211ull;
    constexpr uint32_t kFnv1a32OffsetBasis = 2166136261u;
    constexpr uint32_t kFnv1a32Prime = 16777619u;
}

namespace utils {
    uint64_t Fnv1a64Append(uint64_t hash, const void *data, size_t size) {
        const auto *p = static_cast<const uint8_t *>(data);
        for (size_t i = 0; i < size; ++i) {
            hash ^= p[i];
            hash *= kFnv1a64Prime;
        }
        return hash;
    }

    uint32_t Fnv1a32Append(uint32_t hash, const void *data, size_t size) {
        const auto *p = static_cast<const uint8_t *>(data);
        for (size_t i = 0; i < size; ++i) {
            hash ^= p[i];
            hash *= kFnv1a32Prime;
        }
        return hash;
    }

    uint64_t Fnv1a64(const void *data, size_t size) {
        return Fnv1a64Append(kFnv1a64OffsetBasis, data, size);
    }

    uint32_t Fnv1a32(const void *data, size_t size) {
        return Fnv1a32Append(kFnv1a32OffsetBasis, data, size);
    }

    size_t Fnv1aSizeInit() {
        if constexpr (sizeof(size_t) == sizeof(uint64_t)) {
            return static_cast<size_t>(kFnv1a64OffsetBasis);
        } else {
            return static_cast<size_t>(kFnv1a32OffsetBasis);
        }
    }

    size_t Fnv1aSizeAppend(size_t hash, const void *data, size_t size) {
        if constexpr (sizeof(size_t) == sizeof(uint64_t)) {
            return static_cast<size_t>(Fnv1a64Append(static_cast<uint64_t>(hash), data, size));
        } else {
            return static_cast<size_t>(Fnv1a32Append(static_cast<uint32_t>(hash), data, size));
        }
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

#ifndef BML_HASHUTILS_H
#define BML_HASHUTILS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace utils {
    // -- FNV-1a raw hash values --

    uint64_t Fnv1a64Append(uint64_t hash, const void *data, size_t size);
    uint32_t Fnv1a32Append(uint32_t hash, const void *data, size_t size);

    uint64_t Fnv1a64(const void *data, size_t size);
    uint32_t Fnv1a32(const void *data, size_t size);

    size_t Fnv1aSizeInit();
    size_t Fnv1aSizeAppend(size_t hash, const void *data, size_t size);

    inline size_t Fnv1aSizeAppendByte(size_t hash, uint8_t byte) {
        return Fnv1aSizeAppend(hash, &byte, 1);
    }

    inline size_t Fnv1aSize(std::string_view sv) {
        return Fnv1aSizeAppend(Fnv1aSizeInit(), sv.data(), sv.size());
    }

    inline uint64_t Fnv1a64(const std::vector<uint8_t> &bytes) {
        return Fnv1a64(bytes.data(), bytes.size());
    }
    inline uint32_t Fnv1a32(const std::vector<uint8_t> &bytes) {
        return Fnv1a32(bytes.data(), bytes.size());
    }

    inline uint64_t Fnv1a64(std::string_view sv) {
        return Fnv1a64(sv.data(), sv.size());
    }
    inline uint32_t Fnv1a32(std::string_view sv) {
        return Fnv1a32(sv.data(), sv.size());
    }

    // -- Hex formatting --

    std::string ToHex64(uint64_t value);
    std::string ToHex32(uint32_t value);

    // -- Convenience: hash-then-hex --

    inline std::string Fnv1a64Hex(const void *data, size_t size) {
        return ToHex64(Fnv1a64(data, size));
    }
    inline std::string Fnv1a32Hex(const void *data, size_t size) {
        return ToHex32(Fnv1a32(data, size));
    }

    inline std::string Fnv1a64Hex(const std::vector<uint8_t> &bytes) {
        return ToHex64(Fnv1a64(bytes));
    }
    inline std::string Fnv1a32Hex(const std::vector<uint8_t> &bytes) {
        return ToHex32(Fnv1a32(bytes));
    }

    inline std::string Fnv1a64Hex(std::string_view sv) {
        return ToHex64(Fnv1a64(sv));
    }
    inline std::string Fnv1a32Hex(std::string_view sv) {
        return ToHex32(Fnv1a32(sv));
    }
} // namespace utils

#endif // BML_HASHUTILS_H

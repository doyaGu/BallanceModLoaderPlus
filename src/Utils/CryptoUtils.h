#ifndef BML_CRYPTO_UTILS_H
#define BML_CRYPTO_UTILS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace utils {
    [[nodiscard]] bool HmacSha256(const uint8_t *key, size_t keyLength,
                                  const uint8_t *data, size_t dataLength,
                                  uint8_t outDigest[32]);
    [[nodiscard]] bool Sha256(const uint8_t *data, size_t dataLength, uint8_t outDigest[32]);
    [[nodiscard]] bool Sha256File(const std::wstring &path, uint8_t outDigest[32]);
    [[nodiscard]] std::vector<uint8_t> Sha256Bytes(const uint8_t *data, size_t dataLength);
    [[nodiscard]] std::vector<uint8_t> Sha256FileBytes(const std::wstring &path);
    [[nodiscard]] bool VerifyEcdsaP256Sha256RawSignature(const uint8_t publicKeyX[32],
                                                         const uint8_t publicKeyY[32],
                                                         const uint8_t digest[32],
                                                         const uint8_t signature[64]);
    [[nodiscard]] bool HkdfSha256(const uint8_t *ikm, size_t ikmLength,
                                  const uint8_t *salt, size_t saltLength,
                                  const uint8_t *info, size_t infoLength,
                                  uint8_t *outBytes, size_t outLength);
    [[nodiscard]] uint64_t KeyToSeed(const uint8_t *key, size_t keyLength);
} // namespace utils

#endif // BML_CRYPTO_UTILS_H

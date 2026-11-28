#include "CryptoUtils.h"
#include <p256-m.h>
#include <array>
#include <cstring>

namespace utils {
bool VerifyEcdsaP256Sha256RawSignature(const uint8_t x[32], const uint8_t y[32],
                                    const uint8_t digest[32], const uint8_t signature[64]) {
    if (!x || !y || !digest || !signature) return false;
    std::array<uint8_t, 64> point{};
    std::memcpy(point.data(), x, 32);
    std::memcpy(point.data() + 32, y, 32);
    return p256_ecdsa_verify(signature, point.data(), digest, 32) == P256_SUCCESS;
}
}

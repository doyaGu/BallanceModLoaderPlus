#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace bmlupdater {
// Publisher-only key material. Never linked into the Loader or Updater.
class SigningKey {
public:
    explicit SigningKey(std::span<const uint8_t> encoded);
    ~SigningKey();
    SigningKey(const SigningKey &) = delete;
    SigningKey &operator=(const SigningKey &) = delete;
    std::array<uint8_t, 64> SignDigest(const uint8_t digest[32]) const;
private:
    std::array<uint8_t, 32> m_Scalar{};
};
}

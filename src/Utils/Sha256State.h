#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

#if defined(__APPLE__) && !defined(BML_CRYPTO_PORTABLE)
#include <CommonCrypto/CommonDigest.h>
#else
#include <sha256.h>
#endif

namespace utils::detail {
class Sha256State {
public:
    Sha256State() {
#if defined(__APPLE__) && !defined(BML_CRYPTO_PORTABLE)
        CC_SHA256_Init(&m_State);
#else
        rhash_sha256_init(&m_State);
#endif
    }
    void Update(const uint8_t *data, size_t size) {
        if (!size) return;
#if defined(__APPLE__) && !defined(BML_CRYPTO_PORTABLE)
        while (size) {
            const auto count = static_cast<CC_LONG>(std::min<size_t>(size, std::numeric_limits<CC_LONG>::max()));
            CC_SHA256_Update(&m_State, data, count);
            data += count;
            size -= count;
        }
#else
        rhash_sha256_update(&m_State, data, size);
#endif
    }
    void Finish(uint8_t out[32]) {
#if defined(__APPLE__) && !defined(BML_CRYPTO_PORTABLE)
        CC_SHA256_Final(out, &m_State);
#else
        rhash_sha256_final(&m_State, out);
#endif
    }
private:
#if defined(__APPLE__) && !defined(BML_CRYPTO_PORTABLE)
    CC_SHA256_CTX m_State{};
#else
    sha256_ctx m_State{};
#endif
};
}

#include "CryptoUtils.h"
#include "StringUtils.h"
#include <SDL3/SDL_iostream.h>
#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/ecdsa.h>
#include <openssl/param_build.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace utils {
namespace {
constexpr size_t kSha256Length = 32;
template<class T, auto Free> using Owned = std::unique_ptr<T, decltype(Free)>;
std::string BytesToLowerHex(const uint8_t *bytes, size_t size) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result(size * 2, '\0');
    for (size_t i = 0; i < size; ++i) { result[i * 2] = digits[bytes[i] >> 4]; result[i * 2 + 1] = digits[bytes[i] & 15]; }
    return result;
}
}
bool HmacSha256(const uint8_t *key, size_t keyLength, const uint8_t *data, size_t dataLength, uint8_t out[32]) {
    if (!out || (!key && keyLength) || (!data && dataLength)) return false;
    const uint8_t empty = 0;
    size_t length = 0;
    return EVP_Q_mac(nullptr, "HMAC", nullptr, "SHA256", nullptr, key ? key : &empty, keyLength,
                     data ? data : &empty, dataLength, out, 32, &length) && length == 32;
}
bool Sha256(const uint8_t *data, size_t length, uint8_t out[32]) {
    if (!out || (!data && length)) return false;
    size_t written = 0;
    return EVP_Q_digest(nullptr, "SHA256", nullptr, data, length, out, &written) == 1 && written == 32;
}
bool Sha256File(const std::wstring &path, uint8_t out[32]) {
    if (!out) return false;
    Owned<SDL_IOStream, SDL_CloseIO> file(SDL_IOFromFile(Utf16ToUtf8(path).c_str(), "rb"), SDL_CloseIO);
    Owned<EVP_MD_CTX, EVP_MD_CTX_free> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!file || !ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) return false;
    std::array<uint8_t, 16384> buffer;
    for (;;) {
        size_t read = SDL_ReadIO(file.get(), buffer.data(), buffer.size());
        if (read && EVP_DigestUpdate(ctx.get(), buffer.data(), read) != 1) return false;
        if (read < buffer.size()) break;
    }
    unsigned int length = 0;
    return SDL_GetIOStatus(file.get()) != SDL_IO_STATUS_ERROR && EVP_DigestFinal_ex(ctx.get(), out, &length) == 1 && length == 32;
}
bool VerifyEcdsaP256Sha256RawSignature(const uint8_t x[32], const uint8_t y[32], const uint8_t digest[32], const uint8_t signature[64]) {
    if (!x || !y || !digest || !signature) return false;
    std::array<uint8_t, 65> point{};
    point[0] = 4;
    std::memcpy(point.data() + 1, x, 32);
    std::memcpy(point.data() + 33, y, 32);
    char group[] = "prime256v1";
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, group, 0),
        OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY, point.data(), point.size()),
        OSSL_PARAM_construct_end()
    };
    Owned<EVP_PKEY_CTX, EVP_PKEY_CTX_free> import(EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr), EVP_PKEY_CTX_free);
    EVP_PKEY *rawKey = nullptr;
    if (!import || EVP_PKEY_fromdata_init(import.get()) != 1 ||
        EVP_PKEY_fromdata(import.get(), &rawKey, EVP_PKEY_PUBLIC_KEY, params) != 1) return false;
    Owned<EVP_PKEY, EVP_PKEY_free> key(rawKey, EVP_PKEY_free);
    Owned<ECDSA_SIG, ECDSA_SIG_free> sig(ECDSA_SIG_new(), ECDSA_SIG_free);
    Owned<BIGNUM, BN_free> r(BN_bin2bn(signature, 32, nullptr), BN_free), s(BN_bin2bn(signature + 32, 32, nullptr), BN_free);
    if (!sig || !r || !s || ECDSA_SIG_set0(sig.get(), r.get(), s.get()) != 1) return false;
    r.release(); s.release();
    const int size = i2d_ECDSA_SIG(sig.get(), nullptr);
    if (size <= 0) return false;
    std::vector<uint8_t> der(size);
    auto *position = der.data();
    if (i2d_ECDSA_SIG(sig.get(), &position) != size) return false;
    Owned<EVP_PKEY_CTX, EVP_PKEY_CTX_free> verify(EVP_PKEY_CTX_new(key.get(), nullptr), EVP_PKEY_CTX_free);
    return verify && EVP_PKEY_verify_init(verify.get()) == 1 &&
        EVP_PKEY_CTX_set_signature_md(verify.get(), EVP_sha256()) == 1 &&
        EVP_PKEY_verify(verify.get(), der.data(), der.size(), digest, 32) == 1;
}
    std::vector<uint8_t> Sha256Bytes(const uint8_t *data, size_t dataLength) {
        std::vector<uint8_t> digest(kSha256Length);
        if (!Sha256(data, dataLength, digest.data())) {
            digest.clear();
        }
        return digest;
    }

    std::vector<uint8_t> Sha256FileBytes(const std::wstring &path) {
        std::vector<uint8_t> digest(kSha256Length);
        if (!Sha256File(path, digest.data())) {
            digest.clear();
        }
        return digest;
    }

    std::string Sha256Hex(const uint8_t *data, size_t dataLength) {
        std::array<uint8_t, kSha256Length> digest{};
        if (!Sha256(data, dataLength, digest.data()))
            return {};
        return BytesToLowerHex(digest.data(), digest.size());
    }

    std::string Sha256Hex(std::string_view data) {
        return Sha256Hex(reinterpret_cast<const uint8_t *>(data.data()), data.size());
    }

    std::string Sha256FileHex(const std::wstring &path) {
        std::array<uint8_t, kSha256Length> digest{};
        if (!Sha256File(path, digest.data()))
            return {};
        return BytesToLowerHex(digest.data(), digest.size());
    }

    bool HkdfSha256(const uint8_t *ikm, size_t ikmLength,
                    const uint8_t *salt, size_t saltLength,
                    const uint8_t *info, size_t infoLength,
                    uint8_t *outBytes, size_t outLength) {
        if (!outBytes) {
            return false;
        }

        if (outLength == 0) {
            return true;
        }

        if (outLength > 255 * kSha256Length) {
            return false;
        }

        std::array<uint8_t, kSha256Length> zeroSalt = {};
        const uint8_t *effectiveSalt = saltLength == 0 ? zeroSalt.data() : salt;
        const size_t effectiveSaltLength = saltLength == 0 ? zeroSalt.size() : saltLength;

        std::array<uint8_t, kSha256Length> prk = {};
        if (!HmacSha256(effectiveSalt, effectiveSaltLength, ikm, ikmLength, prk.data())) {
            return false;
        }

        std::array<uint8_t, kSha256Length> previous = {};
        size_t produced = 0;
        uint8_t counter = 1;
        size_t previousLength = 0;

        while (produced < outLength) {
            std::vector<uint8_t> blockInput;
            blockInput.reserve(previousLength + infoLength + 1);
            blockInput.insert(blockInput.end(), previous.begin(), previous.begin() + previousLength);
            if (info && infoLength > 0) {
                blockInput.insert(blockInput.end(), info, info + infoLength);
            }
            blockInput.push_back(counter);

            if (!HmacSha256(prk.data(), prk.size(),
                            blockInput.data(), blockInput.size(),
                            previous.data())) {
                return false;
            }

            size_t copySize = outLength - produced;
            if (copySize > previous.size()) {
                copySize = previous.size();
            }
            std::memcpy(outBytes + produced, previous.data(), copySize);
            produced += copySize;
            previousLength = previous.size();
            ++counter;
        }

        return true;
    }

    uint64_t KeyToSeed(const uint8_t *key, size_t keyLength) {
        uint64_t seed = 0;
        size_t bytesToFold = keyLength;
        const size_t maxBytes = sizeof(uint64_t) * 2;
        if (bytesToFold > maxBytes) {
            bytesToFold = maxBytes;
        }
        for (size_t i = 0; i < bytesToFold; ++i) {
            const size_t shift = (i % sizeof(uint64_t)) * 8;
            seed ^= static_cast<uint64_t>(key[i]) << shift;
        }
        return seed;
    }
} // namespace utils

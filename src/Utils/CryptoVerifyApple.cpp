#include "CryptoUtils.h"
#include <Security/Security.h>
#include <array>
#include <cstring>
#include <memory>
#include <type_traits>
#include <vector>

namespace utils {
namespace {
template<class T> using Owned = std::unique_ptr<std::remove_pointer_t<T>, decltype(&CFRelease)>;
std::vector<uint8_t> EncodeSignature(const uint8_t raw[64]) {
    std::vector<uint8_t> der{0x30, 0};
    for (size_t offset : {size_t{0}, size_t{32}}) {
        const uint8_t *begin = raw + offset, *end = begin + 32;
        while (begin + 1 < end && *begin == 0) ++begin;
        const bool prefix = (*begin & 0x80) != 0;
        der.push_back(2);
        der.push_back(static_cast<uint8_t>(end - begin + prefix));
        if (prefix) der.push_back(0);
        der.insert(der.end(), begin, end);
    }
    der[1] = static_cast<uint8_t>(der.size() - 2);
    return der;
}
}
bool VerifyEcdsaP256Sha256RawSignature(const uint8_t x[32], const uint8_t y[32],
                                    const uint8_t digest[32], const uint8_t signature[64]) {
    if (!x || !y || !digest || !signature) return false;
    std::array<uint8_t, 65> point{4};
    std::memcpy(point.data() + 1, x, 32);
    std::memcpy(point.data() + 33, y, 32);
    Owned<CFDataRef> encoded(CFDataCreate(nullptr, point.data(), point.size()), CFRelease);
    const void *names[] = {kSecAttrKeyType, kSecAttrKeyClass};
    const void *values[] = {kSecAttrKeyTypeECSECPrimeRandom, kSecAttrKeyClassPublic};
    Owned<CFDictionaryRef> attributes(CFDictionaryCreate(nullptr, names, values, 2,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), CFRelease);
    if (!encoded || !attributes) return false;
    Owned<SecKeyRef> key(SecKeyCreateWithData(encoded.get(), attributes.get(), nullptr), CFRelease);
    const auto der = EncodeSignature(signature);
    Owned<CFDataRef> hash(CFDataCreate(nullptr, digest, 32), CFRelease);
    Owned<CFDataRef> sig(CFDataCreate(nullptr, der.data(), der.size()), CFRelease);
    return key && hash && sig && SecKeyVerifySignature(key.get(),
        kSecKeyAlgorithmECDSASignatureDigestX962SHA256, hash.get(), sig.get(), nullptr);
}
}

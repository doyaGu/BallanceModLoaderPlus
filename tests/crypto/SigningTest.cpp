#include "UpdaterSigning.h"
#include "Utils/CryptoUtils.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <string>
#include <vector>
#include <stdexcept>

namespace {
std::vector<uint8_t> HexKey(std::string_view text) {
    std::vector<uint8_t> result;
    for (size_t i = 0; i < text.size(); i += 2) result.push_back(static_cast<uint8_t>(std::stoul(std::string(text.substr(i, 2)), nullptr, 16)));
    return result;
}
// Public TEST key: private scalar 1, public point is the P-256 generator.
const auto Key = HexKey("30770201010420" + std::string(63, '0') +
    "1a00a06082a8648ce3d030107a14403420004"
    "6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296"
    "4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5");
std::vector<uint8_t> Wrap(uint8_t tag, const std::vector<uint8_t> &body) {
    std::vector<uint8_t> result{tag};
    if (body.size() >= 128) result.push_back(0x81);
    result.push_back(static_cast<uint8_t>(body.size()));
    result.insert(result.end(), body.begin(), body.end());
    return result;
}
void RoundTrip(const std::vector<uint8_t> &encoded) {
    bmlupdater::SigningKey key(encoded);
    std::array<uint8_t, 32> hash{};
    ASSERT_TRUE(utils::Sha256(reinterpret_cast<const uint8_t *>("test payload"), 12, hash.data()));
    const auto signature = key.SignDigest(hash.data());
    const auto *point = Key.data() + Key.size() - 64;
    EXPECT_TRUE(utils::VerifyEcdsaP256Sha256RawSignature(point, point + 32, hash.data(), signature.data()));
}
std::vector<uint8_t> Pem(const std::vector<uint8_t> &der, std::string_view label) {
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string text = "-----BEGIN " + std::string(label) + "-----\r\n";
    for (size_t i = 0; i < der.size(); i += 3) {
        const size_t count = std::min<size_t>(3, der.size() - i);
        const uint32_t value = uint32_t{der[i]} << 16 |
            (count > 1 ? uint32_t{der[i + 1]} << 8 : 0) | (count > 2 ? der[i + 2] : 0);
        text += alphabet[(value >> 18) & 63];
        text += alphabet[(value >> 12) & 63];
        text += count > 1 ? alphabet[(value >> 6) & 63] : '=';
        text += count > 2 ? alphabet[value & 63] : '=';
        if (i % 48 == 45) text += "\r\n";
    }
    text += "\r\n-----END " + std::string(label) + "-----\r\n";
    return {text.begin(), text.end()};
}
}

TEST(SigningTest, ImportsSec1AndPkcs8DerAndPem) {
    RoundTrip(Key);
    RoundTrip(Pem(Key, "EC PRIVATE KEY"));
    // The public point is optional in both standard private-key containers.
    auto privateOnly = HexKey("0201010420" + std::string(63, '0') + "1a00a06082a8648ce3d030107");
    RoundTrip(Wrap(0x30, privateOnly));
    auto body = HexKey("020100301306072a8648ce3d020106082a8648ce3d030107");
    const auto privateKey = Wrap(4, Key);
    body.insert(body.end(), privateKey.begin(), privateKey.end());
    RoundTrip(Wrap(0x30, body));
    RoundTrip(Pem(Wrap(0x30, body), "PRIVATE KEY"));
}

TEST(SigningTest, RejectsMalformedKeysAndMismatchedPoints) {
    for (size_t size = 0; size < Key.size(); ++size) {
        const auto truncated = std::vector(Key.begin(), Key.begin() + size);
        EXPECT_THROW({ bmlupdater::SigningKey key(truncated); }, std::runtime_error);
    }
    auto changed = Key; changed.push_back(0);
    EXPECT_THROW({ bmlupdater::SigningKey key(changed); }, std::runtime_error);
    changed = Key; changed[38] = 2; // public point still belongs to scalar 1
    EXPECT_THROW({ bmlupdater::SigningKey key(changed); }, std::runtime_error);
    changed = Key; std::fill(changed.begin() + 7, changed.begin() + 39, 0);
    EXPECT_THROW({ bmlupdater::SigningKey key(changed); }, std::runtime_error);
    changed = Key; std::fill(changed.begin() + 7, changed.begin() + 39, 0xff);
    EXPECT_THROW({ bmlupdater::SigningKey key(changed); }, std::runtime_error);
    changed = Key; changed.back() ^= 1;
    EXPECT_THROW({ bmlupdater::SigningKey key(changed); }, std::runtime_error);
    changed = Pem(Key, "ENCRYPTED PRIVATE KEY");
    EXPECT_THROW({ bmlupdater::SigningKey key(changed); }, std::runtime_error);
    changed = Pem(Key, "EC PRIVATE KEY"); changed.push_back('X');
    EXPECT_THROW({ bmlupdater::SigningKey key(changed); }, std::runtime_error);
    changed = Pem(Key, "EC PRIVATE KEY"); changed[35] = '!';
    EXPECT_THROW({ bmlupdater::SigningKey key(changed); }, std::runtime_error);
}

#include "Utils/CryptoUtils.h"
#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"
#include <SDL3/SDL.h>
#include <gtest/gtest.h>
#include <array>
#include <algorithm>
#include <string>
#include <vector>
#include <stdexcept>

namespace {
std::vector<uint8_t> Hex(std::string_view text) {
    if (text.size() % 2) throw std::invalid_argument("Odd hex test vector");
    auto nibble = [](char c) { return c <= '9' ? c - '0' : (c | 32) - 'a' + 10; };
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < text.size(); i += 2) bytes.push_back(static_cast<uint8_t>((nibble(text[i]) << 4) | nibble(text[i + 1])));
    return bytes;
}
// RFC 6979, Appendix A.2.5, SHA-256("sample").
const auto Public = Hex("60fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6"
                        "7903fe1008b8bc99a41ae9e95628bc64f2f1b20c2d7e9f5177a3c294d4462299");
const auto Signature = Hex("efd48b2aacb6a8fd1140dd9cd45e81d69d2c877b56aaf991c34d0ea84eaf3716"
                           "f7cb1c942d657c41d436c7a1b6e29f65f3e900dbb9aff4064dc4ab2f843acda8");
}

TEST(CryptoTest, Sha256KnownVectorsAndBlockBoundaries) {
    EXPECT_EQ(utils::Sha256Hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(utils::Sha256Hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(utils::Sha256Hex(std::string(1000000, 'a')), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    EXPECT_EQ(utils::Sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST(CryptoTest, Rfc4231HmacIncludingLongKey) {
    const auto key = std::vector<uint8_t>(20, 0x0b);
    const std::string text = "Hi There";
    std::array<uint8_t, 32> out{};
    ASSERT_TRUE(utils::HmacSha256(key.data(), key.size(), reinterpret_cast<const uint8_t *>(text.data()), text.size(), out.data()));
    EXPECT_EQ(std::vector(out.begin(), out.end()), Hex("b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"));
    const auto longKey = std::vector<uint8_t>(131, 0xaa);
    const std::string message = "Test Using Larger Than Block-Size Key - Hash Key First";
    ASSERT_TRUE(utils::HmacSha256(longKey.data(), longKey.size(), reinterpret_cast<const uint8_t *>(message.data()), message.size(), out.data()));
    EXPECT_EQ(std::vector(out.begin(), out.end()), Hex("60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54"));
}

TEST(CryptoTest, Rfc5869HkdfAndOutputLimit) {
    const std::vector<uint8_t> ikm(22, 0x0b);
    const auto salt = Hex("000102030405060708090a0b0c"), info = Hex("f0f1f2f3f4f5f6f7f8f9");
    std::array<uint8_t, 42> out{};
    ASSERT_TRUE(utils::HkdfSha256(ikm.data(), ikm.size(), salt.data(), salt.size(), info.data(), info.size(), out.data(), out.size()));
    EXPECT_EQ(std::vector(out.begin(), out.end()), Hex("3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865"));
    ASSERT_TRUE(utils::HkdfSha256(ikm.data(), ikm.size(), nullptr, 0, nullptr, 0, out.data(), out.size()));
    EXPECT_EQ(std::vector(out.begin(), out.end()), Hex("8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d9d201395faa4b61a96c8"));
    EXPECT_FALSE(utils::HkdfSha256(nullptr, 0, nullptr, 0, nullptr, 0, out.data(), 255 * 32 + 1));
}

TEST(CryptoTest, Rfc6979P256SignatureAndTampering) {
    const auto hash = utils::Sha256Bytes(reinterpret_cast<const uint8_t *>("sample"), 6);
    ASSERT_TRUE(utils::VerifyEcdsaP256Sha256RawSignature(Public.data(), Public.data() + 32, hash.data(), Signature.data()));
    for (size_t byte = 0; byte < Signature.size(); ++byte) {
        auto changed = Signature; changed[byte] ^= 1;
        EXPECT_FALSE(utils::VerifyEcdsaP256Sha256RawSignature(Public.data(), Public.data() + 32, hash.data(), changed.data()));
    }
    auto changedHash = hash; changedHash[0] ^= 1;
    EXPECT_FALSE(utils::VerifyEcdsaP256Sha256RawSignature(Public.data(), Public.data() + 32, changedHash.data(), Signature.data()));
}

TEST(CryptoTest, RejectsInvalidPointsAndScalars) {
    const auto hash = utils::Sha256Bytes(reinterpret_cast<const uint8_t *>("sample"), 6);
    for (const uint8_t fill : {uint8_t{0}, uint8_t{0xff}}) {
        std::array<uint8_t, 64> invalid; invalid.fill(fill);
        EXPECT_FALSE(utils::VerifyEcdsaP256Sha256RawSignature(invalid.data(), invalid.data() + 32, hash.data(), Signature.data()));
        EXPECT_FALSE(utils::VerifyEcdsaP256Sha256RawSignature(Public.data(), Public.data() + 32, hash.data(), invalid.data()));
    }
    auto invalid = Public; invalid[63] ^= 1;
    EXPECT_FALSE(utils::VerifyEcdsaP256Sha256RawSignature(invalid.data(), invalid.data() + 32, hash.data(), Signature.data()));
}

TEST(CryptoTest, NullArgumentsFailWithoutWriting) {
    std::array<uint8_t, 32> out; out.fill(0xa5);
    EXPECT_FALSE(utils::Sha256(nullptr, 1, out.data()));
    EXPECT_TRUE(std::all_of(out.begin(), out.end(), [](uint8_t x) { return x == 0xa5; }));
    EXPECT_FALSE(utils::Sha256(nullptr, 0, nullptr));
    EXPECT_FALSE(utils::HmacSha256(nullptr, 1, nullptr, 0, out.data()));
    EXPECT_FALSE(utils::HmacSha256(nullptr, 0, nullptr, 1, out.data()));
    EXPECT_FALSE(utils::VerifyEcdsaP256Sha256RawSignature(nullptr, nullptr, nullptr, nullptr));
}

TEST(CryptoTest, SdlFileHashHandlesUnicodeEmptyAndMultipleBlocks) {
    const auto path = utils::CombinePathW(utils::GetTempPathW(), L"bml-crypto-\u6d4b\u8bd5-" + std::to_wstring(SDL_GetTicksNS()));
    const auto utf8 = utils::Utf16ToUtf8(path);
    for (size_t size : {size_t{0}, size_t{3}, size_t{55}, size_t{56}, size_t{64}, size_t{16384}, size_t{16385}}) {
        const std::string data(size, 'a');
        SDL_IOStream *file = SDL_IOFromFile(utf8.c_str(), "wb");
        ASSERT_NE(file, nullptr);
        ASSERT_EQ(SDL_WriteIO(file, data.data(), data.size()), data.size());
        ASSERT_TRUE(SDL_CloseIO(file));
        EXPECT_EQ(utils::Sha256FileHex(path), utils::Sha256Hex(data));
    }
    ASSERT_TRUE(SDL_RemovePath(utf8.c_str()));
    EXPECT_TRUE(utils::Sha256FileHex(path).empty());
}

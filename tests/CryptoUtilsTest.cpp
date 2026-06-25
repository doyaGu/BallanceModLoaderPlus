#include <gtest/gtest.h>

#include "CryptoUtils.h"
#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"

TEST(CryptoUtilsTest, Sha256HexFormatsKnownDigests) {
    EXPECT_EQ("e3b0c44298fc1c149afbf4c8996fb924"
              "27ae41e4649b934ca495991b7852b855",
              utils::Sha256Hex(""));
    EXPECT_EQ("ba7816bf8f01cfea414140de5dae2223"
              "b00361a396177a9cb410ff61f20015ad",
              utils::Sha256Hex("abc"));
}

TEST(CryptoUtilsTest, Sha256FileHexFormatsKnownDigest) {
    const std::wstring root = utils::CombinePathW(utils::GetTempPathW(), L"BMLCryptoUtilsTest");
    utils::DeleteDirectoryW(root);
    ASSERT_TRUE(utils::CreateFileTreeW(root));

    const std::wstring path = utils::CombinePathW(root, L"payload.txt");
    ASSERT_TRUE(utils::WriteTextFileUtf8(utils::Utf16ToUtf8(path), "abc"));

    EXPECT_EQ("ba7816bf8f01cfea414140de5dae2223"
              "b00361a396177a9cb410ff61f20015ad",
              utils::Sha256FileHex(path));

    utils::DeleteDirectoryW(root);
}

/**
 * @file LocaleManagerTests.cpp
 * @brief Unit tests for LocaleManager
 */

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "bml_locale.h"

#include "Core/LocaleManager.h"
#include "Core/Context.h"
#include "TestKernel.h"

using namespace BML::Core;
using BML::Core::Testing::TestKernel;

class LocaleManagerTest : public ::testing::Test {
protected:
    TestKernel kernel_;

    void SetUp() override {
        kernel_->context = std::make_unique<Context>();
        kernel_->locale  = std::make_unique<LocaleManager>();
        Context::Instance().Initialize(bmlMakeVersion(0, 4, 0));
        LocaleManager::Instance().Shutdown();

        // Create a temporary directory for locale files
        m_TempDir = std::filesystem::temp_directory_path() / "bml_locale_test";
        std::filesystem::create_directories(m_TempDir / "locale");

        m_TempDirW = m_TempDir.wstring();
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(m_TempDir, ec);
    }

    void WriteLocaleFile(const std::string &code,
                         const std::string &content) {
        std::filesystem::path file = m_TempDir / "locale" / (code + ".toml");
        std::ofstream out(file);
        out << content;
    }

    std::filesystem::path m_TempDir;
    std::wstring m_TempDirW;
};

TEST_F(LocaleManagerTest, LoadAndGetBasic) {
    WriteLocaleFile("en", "greeting = \"Hello\"\nfarewell = \"Goodbye\"\n");

    auto result = LocaleManager::Instance().Load("test.mod", m_TempDirW, "en");
    ASSERT_EQ(result, BML_RESULT_OK);

    const char *val = LocaleManager::Instance().Get("test.mod", "greeting");
    EXPECT_STREQ(val, "Hello");

    val = LocaleManager::Instance().Get("test.mod", "farewell");
    EXPECT_STREQ(val, "Goodbye");
}

TEST_F(LocaleManagerTest, GetReturnsKeyWhenNotFound) {
    WriteLocaleFile("en", "greeting = \"Hello\"\n");
    LocaleManager::Instance().Load("test.mod", m_TempDirW, "en");

    const char *key = "nonexistent.key";
    const char *val = LocaleManager::Instance().Get("test.mod", key);
    // Should return the key pointer itself
    EXPECT_EQ(val, key);
}

TEST_F(LocaleManagerTest, GetReturnsKeyWhenModuleNotLoaded) {
    const char *key = "some.key";
    const char *val = LocaleManager::Instance().Get("unknown.mod", key);
    EXPECT_EQ(val, key);
}

TEST_F(LocaleManagerTest, GetReturnsNullForNullKey) {
    const char *val = LocaleManager::Instance().Get("test.mod", nullptr);
    EXPECT_EQ(val, nullptr);
}

TEST_F(LocaleManagerTest, FallbackToEnglish) {
    // Only provide "en" locale, request "fr"
    WriteLocaleFile("en", "greeting = \"Hello\"\n");

    auto result = LocaleManager::Instance().Load("test.mod", m_TempDirW, "fr");
    ASSERT_EQ(result, BML_RESULT_OK);

    const char *val = LocaleManager::Instance().Get("test.mod", "greeting");
    EXPECT_STREQ(val, "Hello");
}

TEST_F(LocaleManagerTest, NoFallbackReturnsNotFound) {
    // No locale files at all
    auto result = LocaleManager::Instance().Load("test.mod", m_TempDirW, "de");
    EXPECT_EQ(result, BML_RESULT_NOT_FOUND);
}

TEST_F(LocaleManagerTest, PreferRequestedOverFallback) {
    WriteLocaleFile("en", "greeting = \"Hello\"\n");
    WriteLocaleFile("ja", "greeting = \"Konnichiwa\"\n");

    auto result = LocaleManager::Instance().Load("test.mod", m_TempDirW, "ja");
    ASSERT_EQ(result, BML_RESULT_OK);

    const char *val = LocaleManager::Instance().Get("test.mod", "greeting");
    EXPECT_STREQ(val, "Konnichiwa");
}

TEST_F(LocaleManagerTest, SetAndGetLanguage) {
    auto result = LocaleManager::Instance().SetLanguage("zh-CN");
    ASSERT_EQ(result, BML_RESULT_OK);

    const char *lang = nullptr;
    result = LocaleManager::Instance().GetLanguage(&lang);
    ASSERT_EQ(result, BML_RESULT_OK);
    EXPECT_STREQ(lang, "zh-CN");
}

TEST_F(LocaleManagerTest, SetLanguageRejectsNull) {
    auto result = LocaleManager::Instance().SetLanguage(nullptr);
    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(LocaleManagerTest, GetLanguageRejectsNull) {
    auto result = LocaleManager::Instance().GetLanguage(nullptr);
    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(LocaleManagerTest, DefaultLanguageIsEnglish) {
    const char *lang = nullptr;
    LocaleManager::Instance().GetLanguage(&lang);
    EXPECT_STREQ(lang, "en");
}

TEST_F(LocaleManagerTest, CleanupModuleRemovesData) {
    WriteLocaleFile("en", "greeting = \"Hello\"\n");
    LocaleManager::Instance().Load("test.mod", m_TempDirW, "en");

    // Verify loaded
    EXPECT_STREQ(LocaleManager::Instance().Get("test.mod", "greeting"), "Hello");

    LocaleManager::Instance().CleanupModule("test.mod");

    // After cleanup, should return the key
    const char *key = "greeting";
    EXPECT_EQ(LocaleManager::Instance().Get("test.mod", key), key);
}

TEST_F(LocaleManagerTest, LoadReplacesExistingData) {
    WriteLocaleFile("en", "greeting = \"Hello\"\nold_key = \"old\"\n");
    WriteLocaleFile("de", "greeting = \"Hallo\"\n");

    LocaleManager::Instance().Load("test.mod", m_TempDirW, "en");
    EXPECT_STREQ(LocaleManager::Instance().Get("test.mod", "greeting"), "Hello");
    EXPECT_STREQ(LocaleManager::Instance().Get("test.mod", "old_key"), "old");

    // Reload with "de" - should replace all data
    LocaleManager::Instance().Load("test.mod", m_TempDirW, "de");
    EXPECT_STREQ(LocaleManager::Instance().Get("test.mod", "greeting"), "Hallo");

    // old_key should no longer exist
    const char *key = "old_key";
    EXPECT_EQ(LocaleManager::Instance().Get("test.mod", key), key);
}

TEST_F(LocaleManagerTest, ShutdownClearsAll) {
    WriteLocaleFile("en", "greeting = \"Hello\"\n");
    LocaleManager::Instance().Load("test.mod", m_TempDirW, "en");
    LocaleManager::Instance().SetLanguage("ja");

    LocaleManager::Instance().Shutdown();

    // Language resets to "en"
    const char *lang = nullptr;
    LocaleManager::Instance().GetLanguage(&lang);
    EXPECT_STREQ(lang, "en");

    // Data gone
    const char *key = "greeting";
    EXPECT_EQ(LocaleManager::Instance().Get("test.mod", key), key);
}

TEST_F(LocaleManagerTest, DottedKeysAreFlat) {
    WriteLocaleFile("en", "\"config.speed\" = \"Ball Speed\"\n\"ui.title\" = \"Game Menu\"\n");

    auto result = LocaleManager::Instance().Load("test.mod", m_TempDirW, "en");
    ASSERT_EQ(result, BML_RESULT_OK);

    EXPECT_STREQ(LocaleManager::Instance().Get("test.mod", "config.speed"), "Ball Speed");
    EXPECT_STREQ(LocaleManager::Instance().Get("test.mod", "ui.title"), "Game Menu");
}

TEST_F(LocaleManagerTest, MultipleModulesIsolated) {
    WriteLocaleFile("en", "greeting = \"Hello\"\n");

    LocaleManager::Instance().Load("mod.a", m_TempDirW, "en");
    LocaleManager::Instance().Load("mod.b", m_TempDirW, "en");

    EXPECT_STREQ(LocaleManager::Instance().Get("mod.a", "greeting"), "Hello");
    EXPECT_STREQ(LocaleManager::Instance().Get("mod.b", "greeting"), "Hello");

    // Clean up one, other unaffected
    LocaleManager::Instance().CleanupModule("mod.a");

    const char *key = "greeting";
    EXPECT_EQ(LocaleManager::Instance().Get("mod.a", key), key);
    EXPECT_STREQ(LocaleManager::Instance().Get("mod.b", "greeting"), "Hello");
}

TEST_F(LocaleManagerTest, InvalidTomlReturnsError) {
    WriteLocaleFile("en", "this is not valid toml {{{\n");

    auto result = LocaleManager::Instance().Load("test.mod", m_TempDirW, "en");
    EXPECT_EQ(result, BML_RESULT_FAIL);
}

TEST_F(LocaleManagerTest, NonStringValuesAreIgnored) {
    WriteLocaleFile("en",
        "greeting = \"Hello\"\n"
        "count = 42\n"
        "flag = true\n"
        "ratio = 3.14\n"
    );

    auto result = LocaleManager::Instance().Load("test.mod", m_TempDirW, "en");
    ASSERT_EQ(result, BML_RESULT_OK);

    // String value works
    EXPECT_STREQ(LocaleManager::Instance().Get("test.mod", "greeting"), "Hello");

    // Non-string values are ignored, key returned
    const char *key_count = "count";
    EXPECT_EQ(LocaleManager::Instance().Get("test.mod", key_count), key_count);

    const char *key_flag = "flag";
    EXPECT_EQ(LocaleManager::Instance().Get("test.mod", key_flag), key_flag);
}

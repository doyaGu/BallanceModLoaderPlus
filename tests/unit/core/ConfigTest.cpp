#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <array>
#include <chrono>
#include <fstream>
#include <limits>

#include <windows.h>

#include "BML/IMod.h"

#include "Config/Config.h"
#include "Config/ConfigStore.h"
#include "Logging/Logger.h"
#include "Mods/BMLConfigMigration.h"

#include "PathUtils.h"

Logger *Logger::m_DefaultLogger = nullptr;

Logger *Logger::GetDefault() {
    return m_DefaultLogger;
}

void Logger::SetDefault(Logger *logger) {
    m_DefaultLogger = logger;
}

Logger::Logger(const char *modName) : m_ModName(modName) {}

void Logger::Info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Log("INFO", fmt, args);
    va_end(args);
}

void Logger::Warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Log("WARN", fmt, args);
    va_end(args);
}

void Logger::Error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Log("ERROR", fmt, args);
    va_end(args);
}

void Logger::Log(const char *level, const char *fmt, va_list args) {
    SYSTEMTIME sys;
    GetLocalTime(&sys);

    FILE *out_files[] = {
        stdout,
    };

    for (FILE *file : out_files) {
        fprintf(file, "[%02d/%02d/%d %02d:%02d:%02d.%03d] ", sys.wMonth, sys.wDay,
                sys.wYear, sys.wHour, sys.wMinute, sys.wSecond, sys.wMilliseconds);
        fprintf(file, "[%s/%s]: ", m_ModName, level);
        vfprintf(file, fmt, args);
        fputc('\n', file);
        fflush(file);
    }
}

ILogger *IMod::GetLogger() {
    if (m_Logger == nullptr)
        m_Logger = new Logger(GetID());
    return m_Logger;
}

IConfig *IMod::GetConfig() {
    if (m_Config == nullptr) {
        auto *config = new Config(this);
        m_Config = config;
    }
    return m_Config;
}

IMod::~IMod() {
    if (m_Logger)
        delete m_Logger;
    if (m_Config)
        delete m_Config;
}

// Mock implementation of IMod for testing
class MockMod : public IMod {
public:
    explicit MockMod(IBML *bml) : IMod(bml) {}

    const char *GetID() override {
        ++idReadCount;
        return "MockMod";
    }
    const char *GetName() override {
        ++nameReadCount;
        return "MockModName";
    }
    const char *GetVersion() override {
        ++versionReadCount;
        return "1.0";
    }
    const char *GetAuthor() override { return "Tester"; }

    const char *GetDescription() override {
        return "Test description for the mock mod.";
    }

    DECLARE_BML_VERSION;

    void OnModifyConfig(const char *category, const char *key, IProperty *prop) override {
        modifiedCount++;
        lastCategory = category ? category : "";
        lastKey = key ? key : "";
        lastProp = prop;
    }

    std::atomic<int> modifiedCount{0};
    std::atomic<int> idReadCount{0};
    std::atomic<int> nameReadCount{0};
    std::atomic<int> versionReadCount{0};
    std::string lastCategory;
    std::string lastKey;
    IProperty *lastProp = nullptr;
};

// Test fixture for Config tests
class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockMod = new MockMod(nullptr);
        config = new Config(mockMod);
    }

    void TearDown() override {
        delete config;
        delete mockMod;
    }

    MockMod *mockMod = nullptr;
    Config *config = nullptr;
};

// Basic creation and destruction
TEST_F(ConfigTest, ConstructionDestruction) {
    ASSERT_NE(nullptr, config);
    EXPECT_EQ(mockMod, config->GetMod());

    // Test with null mod
    Config *nullModConfig = new Config(nullptr);
    EXPECT_EQ(nullptr, nullModConfig->GetMod());
    delete nullModConfig;
}

TEST(ConfigStoreTest, FindRejectsDifferentOwnerWithSameModId) {
    MockMod registered(nullptr);
    MockMod duplicate(nullptr);
    ConfigStore store;

    auto config = std::make_unique<Config>(&registered);
    Config *registeredConfig = config.get();
    ASSERT_EQ(store.Add("MockMod", &registered, std::move(config)), registeredConfig);

    EXPECT_EQ(store.Find("MockMod", &registered), registeredConfig);
    EXPECT_EQ(store.Find("MockMod", &duplicate), nullptr);
}

TEST(ConfigStoreTest, RemovalMaintainsOwnerIdentityAndRemainingIndices) {
    MockMod first(nullptr);
    MockMod second(nullptr);
    ConfigStore store;

    auto firstConfig = std::make_unique<Config>(&first);
    auto secondConfig = std::make_unique<Config>(&second);
    Config *firstRaw = firstConfig.get();
    Config *secondRaw = secondConfig.get();
    ASSERT_EQ(store.Add("first", &first, std::move(firstConfig)), firstRaw);
    ASSERT_EQ(store.Add("second", &second, std::move(secondConfig)), secondRaw);

    EXPECT_EQ(store.Remove("first", &second, firstRaw), nullptr);
    EXPECT_EQ(store.Find("first", &first), firstRaw);

    std::unique_ptr<Config> removed = store.Remove("first", &first, firstRaw);
    ASSERT_EQ(removed.get(), firstRaw);
    EXPECT_EQ(store.Find("first", &first), nullptr);
    EXPECT_EQ(store.Find("second", &second), secondRaw);
}

// Category management
TEST_F(ConfigTest, CategoryManagement) {
    // Test HasCategory with non-existent category
    EXPECT_FALSE(config->HasCategory("TestCategory"));
    EXPECT_FALSE(config->HasCategory(nullptr));

    // Get non-existent category should create it
    Category *cat = config->GetCategory("TestCategory");
    ASSERT_NE(nullptr, cat);
    EXPECT_STREQ("TestCategory", cat->GetName());

    // Now HasCategory should return true
    EXPECT_TRUE(config->HasCategory("TestCategory"));

    // Get existing category should return same instance
    Category *cat2 = config->GetCategory("TestCategory");
    EXPECT_EQ(cat, cat2);

    // Test category comment
    config->SetCategoryComment("TestCategory", "Test Comment");
    EXPECT_STREQ("Test Comment", config->GetCategoryComment("TestCategory"));

    // Test null comment
    config->SetCategoryComment("TestCategory", nullptr);
    EXPECT_STREQ("", config->GetCategoryComment("TestCategory"));

    // Test getting category by index
    EXPECT_EQ(cat, config->GetCategory((size_t)0));
    EXPECT_EQ(nullptr, config->GetCategory(99)); // Out of bounds
}

// Property management
TEST_F(ConfigTest, PropertyManagement) {
    // Test HasKey with non-existent property
    EXPECT_FALSE(config->HasKey("TestCategory", "TestKey"));

    // Test null parameters
    EXPECT_FALSE(config->HasKey(nullptr, "TestKey"));
    EXPECT_FALSE(config->HasKey("TestCategory", nullptr));
    EXPECT_FALSE(config->HasKey(nullptr, nullptr));

    // Get non-existent property should create it
    IProperty *prop = config->GetProperty("TestCategory", "TestKey");
    ASSERT_NE(nullptr, prop);

    // Now HasKey should return true
    EXPECT_TRUE(config->HasKey("TestCategory", "TestKey"));

    // Get existing property should return same instance
    IProperty *prop2 = config->GetProperty("TestCategory", "TestKey");
    EXPECT_EQ(prop, prop2);

    // Test null parameters for GetProperty
    EXPECT_EQ(nullptr, config->GetProperty(nullptr, "TestKey"));
    EXPECT_EQ(nullptr, config->GetProperty("TestCategory", nullptr));
    EXPECT_EQ(nullptr, config->GetProperty(nullptr, nullptr));
}

TEST_F(ConfigTest, RemovingPropertyClearsPendingNotificationAndInvalidatesSchema) {
    IProperty *retained = config->GetProperty("TestCategory", "Retained");
    IProperty *obsolete = config->GetProperty("TestCategory", "Obsolete");
    retained->SetDefaultString("retained");
    obsolete->SetString("old value");

    const std::uint64_t schemaRevision = config->GetSchemaRevision();
    ASSERT_TRUE(config->RemoveProperty("TestCategory", "Obsolete"));

    EXPECT_FALSE(config->HasKey("TestCategory", "Obsolete"));
    EXPECT_EQ(config->GetCategory("TestCategory")->GetPropertyCount(), 1u);
    EXPECT_EQ(config->GetCategory("TestCategory")->GetProperty(std::size_t{0}), retained);
    EXPECT_TRUE(config->TakePendingNotifications().empty());
    EXPECT_GT(config->GetSchemaRevision(), schemaRevision);
    EXPECT_TRUE(config->IsDirty());
    EXPECT_FALSE(config->RemoveProperty("TestCategory", "Obsolete"));
}

TEST_F(ConfigTest, Migrates013FontSettingsAndPersistsOnlyCurrentKeys) {
    config->GetProperty("GUI", "FontFilename")->SetString("primary.ttf");
    config->GetProperty("GUI", "FontSize")->SetFloat(42.0f);
    config->GetProperty("GUI", "FontRanges")->SetString("ChineseFull");
    config->GetProperty("GUI", "EnableSecondaryFont")->SetBoolean(true);
    config->GetProperty("GUI", "SecondaryFontFilename")->SetString("secondary.ttf");
    config->GetProperty("GUI", "SecondaryFontSize")->SetFloat(26.0f);
    config->GetProperty("GUI", "SecondaryFontRanges")->SetString("Japanese");
    config->GetProperty("CommandBar", "WindowBackgroundAlpha")->SetFloat(0.8f);
    config->GetProperty("HUD", "ShowFPS")->SetBoolean(false);

    MigrateBMLConfig(*config);

    EXPECT_STREQ(config->GetProperty("GUI", "FontFilename")->GetString(), "primary.ttf");
    EXPECT_FLOAT_EQ(config->GetProperty("GUI", "FontSize")->GetFloat(), 42.0f);
    EXPECT_STREQ(config->GetProperty("GUI", "FontFallbacks")->GetString(), "secondary.ttf");
    EXPECT_FLOAT_EQ(config->GetProperty("GUI", "FontFallbackSize")->GetFloat(), 26.0f);
    EXPECT_FALSE(config->HasKey("GUI", "FontRanges"));
    EXPECT_FALSE(config->HasKey("GUI", "EnableSecondaryFont"));
    EXPECT_FALSE(config->HasKey("GUI", "SecondaryFontFilename"));
    EXPECT_FALSE(config->HasKey("GUI", "SecondaryFontSize"));
    EXPECT_FALSE(config->HasKey("GUI", "SecondaryFontRanges"));
    EXPECT_FALSE(config->HasKey("CommandBar", "WindowBackgroundAlpha"));
    EXPECT_FALSE(config->GetProperty("HUD", "ShowFPS")->GetBoolean());

    std::array<wchar_t, MAX_PATH> tempDirectory{};
    ASSERT_GT(GetTempPathW(static_cast<DWORD>(tempDirectory.size()), tempDirectory.data()), 0u);
    std::array<wchar_t, MAX_PATH> path{};
    ASSERT_NE(GetTempFileNameW(tempDirectory.data(), L"BML", 0, path.data()), 0u);
    ASSERT_TRUE(config->Save(path.data()));
    Config reloaded(mockMod);
    ASSERT_TRUE(reloaded.Load(path.data()));
    EXPECT_TRUE(DeleteFileW(path.data()));

    EXPECT_FALSE(reloaded.HasKey("GUI", "SecondaryFontFilename"));
    EXPECT_STREQ(reloaded.GetProperty("GUI", "FontFallbacks")->GetString(), "secondary.ttf");
    EXPECT_FLOAT_EQ(reloaded.GetProperty("GUI", "FontFallbackSize")->GetFloat(), 26.0f);
    const std::uint64_t revision = reloaded.GetSchemaRevision();
    MigrateBMLConfig(reloaded);
    EXPECT_EQ(reloaded.GetSchemaRevision(), revision);
    EXPECT_FALSE(reloaded.IsDirty());
}

TEST_F(ConfigTest, CurrentFontSettingsTakePrecedenceOver013Settings) {
    config->GetProperty("GUI", "FontFallbacks")->SetString("");
    config->GetProperty("GUI", "FontFallbackSize")->SetFloat(20.0f);
    config->GetProperty("GUI", "EnableSecondaryFont")->SetBoolean(true);
    config->GetProperty("GUI", "SecondaryFontFilename")->SetString("obsolete.ttf");
    config->GetProperty("GUI", "SecondaryFontSize")->SetFloat(45.0f);

    MigrateBMLConfig(*config);

    EXPECT_STREQ(config->GetProperty("GUI", "FontFallbacks")->GetString(), "");
    EXPECT_FLOAT_EQ(config->GetProperty("GUI", "FontFallbackSize")->GetFloat(), 20.0f);
    EXPECT_FALSE(config->HasKey("GUI", "SecondaryFontFilename"));
}

TEST_F(ConfigTest, Disabled013SecondaryFontLeavesFallbackListEmpty) {
    config->GetProperty("GUI", "FontSize")->SetFloat(40.0f);
    config->GetProperty("GUI", "EnableSecondaryFont")->SetBoolean(false);
    config->GetProperty("GUI", "SecondaryFontFilename")->SetString("secondary.ttf");

    MigrateBMLConfig(*config);

    EXPECT_FALSE(config->HasKey("GUI", "FontFallbacks"));
    EXPECT_FLOAT_EQ(config->GetProperty("GUI", "FontFallbackSize")->GetFloat(), 40.0f);
}

TEST_F(ConfigTest, CurrentConfigDoesNotInheritPrimarySizeForFallbacks) {
    config->GetProperty("GUI", "FontSize")->SetFloat(40.0f);

    MigrateBMLConfig(*config);

    EXPECT_FALSE(config->HasKey("GUI", "FontFallbackSize"));
}

TEST_F(ConfigTest, OldNonPositiveFontSizesRetainThe013Default) {
    config->GetProperty("GUI", "FontRanges")->SetString("ChineseFull");
    config->GetProperty("GUI", "FontSize")->SetFloat(0.0f);
    config->GetProperty("GUI", "SecondaryFontSize")->SetFloat(-1.0f);

    MigrateBMLConfig(*config);

    EXPECT_FLOAT_EQ(config->GetProperty("GUI", "FontSize")->GetFloat(), 32.0f);
    EXPECT_FLOAT_EQ(config->GetProperty("GUI", "FontFallbackSize")->GetFloat(), 32.0f);
}

TEST_F(ConfigTest, Migrates013RelativeFontPathToAbsolutePath) {
    const std::string file = utils::CombinePathUtf8(BML_TEST_FONT_DIRECTORY, "unifont.otf");
    ASSERT_TRUE(utils::FileExistsUtf8(file));
    const std::string relative = utils::MakeRelativePathUtf8(
        file, utils::GetCurrentDirectoryUtf8());
    ASSERT_FALSE(utils::IsAbsolutePathUtf8(relative));
    ASSERT_TRUE(utils::FileExistsUtf8(relative));

    config->GetProperty("GUI", "FontFilename")->SetString(relative.c_str());
    config->GetProperty("GUI", "FontRanges")->SetString("ChineseFull");
    MigrateBMLConfig(*config);

    const std::string resolved = config->GetProperty("GUI", "FontFilename")->GetString();
    EXPECT_TRUE(utils::IsAbsolutePathUtf8(resolved));
    EXPECT_TRUE(utils::FileExistsUtf8(resolved));
    EXPECT_FALSE(config->HasKey("GUI", "FontRanges"));
}

TEST_F(ConfigTest, Unsupported013FontsSubpathIsKeptForManualSelection) {
    const std::string oldPath = "subdirectory/secondary.ttf";
    ASSERT_FALSE(utils::FileExistsUtf8(oldPath));
    config->GetProperty("GUI", "EnableSecondaryFont")->SetBoolean(true);
    config->GetProperty("GUI", "SecondaryFontFilename")->SetString(oldPath.c_str());
    config->GetProperty("GUI", "SecondaryFontSize")->SetFloat(24.0f);

    EXPECT_FALSE(MigrateBMLConfig(*config));

    EXPECT_FALSE(config->HasKey("GUI", "FontFallbacks"));
    EXPECT_STREQ(config->GetProperty("GUI", "SecondaryFontFilename")->GetString(), oldPath.c_str());
    EXPECT_FLOAT_EQ(config->GetProperty("GUI", "SecondaryFontSize")->GetFloat(), 24.0f);
    EXPECT_FLOAT_EQ(config->GetProperty("GUI", "FontFallbackSize")->GetFloat(), 24.0f);
    EXPECT_FALSE(MigrateBMLConfig(*config));
}

TEST_F(ConfigTest, Unrepresentable013FallbackNameIsNotDeleted) {
    config->GetProperty("GUI", "EnableSecondaryFont")->SetBoolean(true);
    config->GetProperty("GUI", "SecondaryFontFilename")->SetString("old;face.ttf");

    EXPECT_FALSE(MigrateBMLConfig(*config));

    EXPECT_FALSE(config->HasKey("GUI", "FontFallbacks"));
    EXPECT_STREQ(config->GetProperty("GUI", "SecondaryFontFilename")->GetString(), "old;face.ttf");
}

// Property types and values
TEST_F(ConfigTest, PropertyValues) {
    // String property
    IProperty *strProp = config->GetProperty("TestCategory", "StringProp");
    strProp->SetString("Test String");
    EXPECT_EQ(IProperty::STRING, strProp->GetType());
    EXPECT_STREQ("Test String", strProp->GetString());

    // Test null string
    strProp->SetString(nullptr);
    EXPECT_STREQ("", strProp->GetString());

    // Boolean property
    IProperty *boolProp = config->GetProperty("TestCategory", "BoolProp");
    boolProp->SetBoolean(true);
    EXPECT_EQ(IProperty::BOOLEAN, boolProp->GetType());
    EXPECT_TRUE(boolProp->GetBoolean());

    // Integer property
    IProperty *intProp = config->GetProperty("TestCategory", "IntProp");
    intProp->SetInteger(42);
    EXPECT_EQ(IProperty::INTEGER, intProp->GetType());
    EXPECT_EQ(42, intProp->GetInteger());

    // Float property
    IProperty *floatProp = config->GetProperty("TestCategory", "FloatProp");
    floatProp->SetFloat(3.14f);
    EXPECT_EQ(IProperty::FLOAT, floatProp->GetType());
    EXPECT_FLOAT_EQ(3.14f, floatProp->GetFloat());

    // Key property
    IProperty *keyProp = config->GetProperty("TestCategory", "KeyProp");
    keyProp->SetKey(static_cast<CKKEYBOARD>(123));
    EXPECT_EQ(IProperty::KEY, keyProp->GetType());
    EXPECT_EQ(static_cast<CKKEYBOARD>(123), keyProp->GetKey());

    // Test cross-type access returns default values
    EXPECT_STREQ("", intProp->GetString());
    EXPECT_FALSE(strProp->GetBoolean());
    EXPECT_EQ(0, strProp->GetInteger());
    EXPECT_FLOAT_EQ(0.0f, strProp->GetFloat());
    EXPECT_EQ(static_cast<CKKEYBOARD>(0), strProp->GetKey());
}

TEST_F(ConfigTest, PropertyEditorIsSchemaMetadata) {
    IProperty *property = config->GetProperty("Appearance", "Accent");
    property->SetDefaultString("#61AFEF");
    EXPECT_EQ(BML_CONFIG_EDITOR_DEFAULT, BML_GetConfigPropertyEditor(nullptr));
    EXPECT_EQ(0, BML_SetConfigPropertyEditor(nullptr, BML_CONFIG_EDITOR_COLOR));
    EXPECT_EQ(BML_CONFIG_EDITOR_DEFAULT, BML_GetConfigPropertyEditor(property));

    const std::uint64_t schemaBeforeEditor = config->GetSchemaRevision();
    const std::uint64_t valueBeforeEditor = config->GetValueRevision();
    EXPECT_EQ(1, BML_SetConfigPropertyEditor(property, BML_CONFIG_EDITOR_COLOR));
    EXPECT_EQ(BML_CONFIG_EDITOR_COLOR, BML_GetConfigPropertyEditor(property));
    EXPECT_GT(config->GetSchemaRevision(), schemaBeforeEditor);
    EXPECT_EQ(valueBeforeEditor, config->GetValueRevision());
    EXPECT_STREQ("#61AFEF", property->GetString());

    EXPECT_EQ(1, BML_SetConfigPropertyEditor(property, BML_CONFIG_EDITOR_CHOICE));
    EXPECT_EQ(BML_CONFIG_EDITOR_CHOICE, BML_GetConfigPropertyEditor(property));

    EXPECT_EQ(0, BML_SetConfigPropertyEditor(
                     property, static_cast<BML_ConfigPropertyEditor>(99)));
    EXPECT_EQ(BML_CONFIG_EDITOR_CHOICE, BML_GetConfigPropertyEditor(property));

    const std::uint64_t stableSchema = config->GetSchemaRevision();
    EXPECT_EQ(1, BML_SetConfigPropertyEditor(property, BML_CONFIG_EDITOR_CHOICE));
    EXPECT_EQ(stableSchema, config->GetSchemaRevision());
}

TEST_F(ConfigTest, PropertyChoicesAreSchemaMetadata) {
    IProperty *property = config->GetProperty("Appearance", "Font");
    property->SetDefaultString("unifont.otf");
    EXPECT_EQ(0U, BML_GetConfigPropertyChoiceCount(nullptr));
    EXPECT_EQ(nullptr, BML_GetConfigPropertyChoice(nullptr, 0));
    EXPECT_EQ(0, BML_SetConfigPropertyChoices(property, nullptr, 1));
    const char *nullChoice[] = {nullptr};
    EXPECT_EQ(0, BML_SetConfigPropertyChoices(property, nullChoice, 1));

    const char *choices[] = {"", "unifont.otf", "symbols.ttf"};
    const std::uint64_t schemaBeforeChoices = config->GetSchemaRevision();
    const std::uint64_t valueBeforeChoices = config->GetValueRevision();
    EXPECT_EQ(1, BML_SetConfigPropertyChoices(property, choices, 3));
    EXPECT_GT(config->GetSchemaRevision(), schemaBeforeChoices);
    EXPECT_EQ(valueBeforeChoices, config->GetValueRevision());
    ASSERT_EQ(3U, BML_GetConfigPropertyChoiceCount(property));
    EXPECT_STREQ("", BML_GetConfigPropertyChoice(property, 0));
    EXPECT_STREQ("unifont.otf", BML_GetConfigPropertyChoice(property, 1));
    EXPECT_STREQ("symbols.ttf", BML_GetConfigPropertyChoice(property, 2));
    EXPECT_EQ(nullptr, BML_GetConfigPropertyChoice(property, 3));
    EXPECT_STREQ("unifont.otf", property->GetString());

    const std::uint64_t stableSchema = config->GetSchemaRevision();
    EXPECT_EQ(1, BML_SetConfigPropertyChoices(property, choices, 3));
    EXPECT_EQ(stableSchema, config->GetSchemaRevision());

    const char *duplicates[] = {"same", "same"};
    EXPECT_EQ(0, BML_SetConfigPropertyChoices(property, duplicates, 2));
    EXPECT_EQ(3U, BML_GetConfigPropertyChoiceCount(property));

    EXPECT_EQ(1, BML_SetConfigPropertyChoices(property, nullptr, 0));
    EXPECT_EQ(0U, BML_GetConfigPropertyChoiceCount(property));
}

TEST_F(ConfigTest, VariantValueKeepsIntegerAndKeySemanticsDistinct) {
    auto *intProp = static_cast<Property *>(config->GetProperty("TestCategory", "IntProp"));
    intProp->SetDefaultInteger(10);
    EXPECT_EQ(Property::Value{10}, intProp->GetValue());

    intProp->SetValue(Property::Value{20});
    EXPECT_EQ(IProperty::INTEGER, intProp->GetType());
    EXPECT_EQ(20, intProp->GetInteger());

    auto *keyProp = static_cast<Property *>(config->GetProperty("TestCategory", "KeyProp"));
    keyProp->SetDefaultKey(static_cast<CKKEYBOARD>(30));
    EXPECT_EQ(Property::Value{30}, keyProp->GetValue());

    keyProp->SetValue(Property::Value{40});
    EXPECT_EQ(IProperty::KEY, keyProp->GetType());
    EXPECT_EQ(static_cast<CKKEYBOARD>(40), keyProp->GetKey());
}

// Default values
TEST_F(ConfigTest, DefaultValues) {
    // String default
    IProperty *strProp = config->GetProperty("DefaultCategory", "StringProp");
    strProp->SetDefaultString("Default String");
    EXPECT_EQ(IProperty::STRING, strProp->GetType());
    EXPECT_STREQ("Default String", strProp->GetString());

    // Test null default string
    IProperty *nullStrProp = config->GetProperty("DefaultCategory", "NullStringProp");
    nullStrProp->SetDefaultString(nullptr);
    EXPECT_EQ(IProperty::STRING, nullStrProp->GetType());
    EXPECT_STREQ("", nullStrProp->GetString());

    // Boolean default
    IProperty *boolProp = config->GetProperty("DefaultCategory", "BoolProp");
    boolProp->SetDefaultBoolean(true);
    EXPECT_EQ(IProperty::BOOLEAN, boolProp->GetType());
    EXPECT_TRUE(boolProp->GetBoolean());

    // Integer default
    IProperty *intProp = config->GetProperty("DefaultCategory", "IntProp");
    intProp->SetDefaultInteger(42);
    EXPECT_EQ(IProperty::INTEGER, intProp->GetType());
    EXPECT_EQ(42, intProp->GetInteger());

    // Float default
    IProperty *floatProp = config->GetProperty("DefaultCategory", "FloatProp");
    floatProp->SetDefaultFloat(3.14f);
    EXPECT_EQ(IProperty::FLOAT, floatProp->GetType());
    EXPECT_FLOAT_EQ(3.14f, floatProp->GetFloat());

    // Key default
    IProperty *keyProp = config->GetProperty("DefaultCategory", "KeyProp");
    keyProp->SetDefaultKey(static_cast<CKKEYBOARD>(123));
    EXPECT_EQ(IProperty::KEY, keyProp->GetType());
    EXPECT_EQ(static_cast<CKKEYBOARD>(123), keyProp->GetKey());

    // Test setting value after default
    strProp->SetString("New Value");
    EXPECT_STREQ("New Value", strProp->GetString());
}

TEST_F(ConfigTest, RevisionsDistinguishSchemaAndValueChanges) {
    const std::uint64_t initialSchema = config->GetSchemaRevision();
    const std::uint64_t initialValue = config->GetValueRevision();

    Category *category = config->GetCategory("Revision");
    ASSERT_NE(nullptr, category);
    EXPECT_GT(config->GetSchemaRevision(), initialSchema);
    EXPECT_EQ(initialValue, config->GetValueRevision());

    const std::uint64_t categorySchema = config->GetSchemaRevision();
    auto *property = static_cast<Property *>(config->GetProperty("Revision", "Value"));
    ASSERT_NE(nullptr, property);
    EXPECT_GT(config->GetSchemaRevision(), categorySchema);
    EXPECT_EQ(initialValue, config->GetValueRevision());

    const std::uint64_t propertySchema = config->GetSchemaRevision();
    property->SetDefaultInteger(10);
    EXPECT_GT(config->GetSchemaRevision(), propertySchema);
    EXPECT_EQ(initialValue, config->GetValueRevision());

    const std::uint64_t typedSchema = config->GetSchemaRevision();
    property->SetDefaultInteger(20);
    EXPECT_EQ(typedSchema, config->GetSchemaRevision());
    EXPECT_EQ(initialValue, config->GetValueRevision());

    property->SetInteger(11);
    EXPECT_EQ(typedSchema, config->GetSchemaRevision());
    EXPECT_EQ(initialValue + 1, config->GetValueRevision());

    property->SetString("changed type");
    EXPECT_GT(config->GetSchemaRevision(), typedSchema);
    EXPECT_EQ(initialValue + 1, config->GetValueRevision());

    const std::uint64_t typeSchema = config->GetSchemaRevision();
    category->SetComment("Category help");
    EXPECT_GT(config->GetSchemaRevision(), typeSchema);
    const std::uint64_t categoryCommentSchema = config->GetSchemaRevision();
    property->SetComment("Property help");
    EXPECT_GT(config->GetSchemaRevision(), categoryCommentSchema);
    const std::uint64_t propertyCommentSchema = config->GetSchemaRevision();
    property->SetComment("Property help");
    EXPECT_EQ(propertyCommentSchema, config->GetSchemaRevision());
}

TEST_F(ConfigTest, ReassigningNanDoesNotCreateAValueChange) {
    auto *property = static_cast<Property *>(config->GetProperty("Revision", "Nan"));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    property->SetDefaultFloat(nan);
    const std::uint64_t valueRevision = config->GetValueRevision();

    property->SetFloat(nan);

    EXPECT_EQ(valueRevision, config->GetValueRevision());
    EXPECT_FALSE(config->IsDirty());
    EXPECT_TRUE(config->TakePendingNotifications().empty());
}

TEST_F(ConfigTest, ApplyEditsCommitsInCallerOrderAndCoalescesNotifications) {
    auto *first = static_cast<Property *>(config->GetProperty("Batch", "First"));
    auto *second = static_cast<Property *>(config->GetProperty("Batch", "Second"));
    first->SetDefaultInteger(1);
    second->SetDefaultInteger(2);

    const std::uint64_t schemaRevision = config->GetSchemaRevision();
    const std::uint64_t valueRevision = config->GetValueRevision();
    const std::vector<Config::Edit> edits = {
        {"Batch", "Second", IProperty::INTEGER, 2, 20},
        {"Batch", "First", IProperty::INTEGER, 1, 10},
    };

    const Config::ApplyResult result = config->ApplyEdits(mockMod, schemaRevision, edits);
    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(2u, result.ChangedCount);
    EXPECT_EQ(10, first->GetInteger());
    EXPECT_EQ(20, second->GetInteger());
    EXPECT_EQ(schemaRevision, config->GetSchemaRevision());
    EXPECT_EQ(valueRevision + 2, config->GetValueRevision());
    EXPECT_TRUE(config->IsDirty());

    std::vector<Config::PendingNotification> notifications = config->TakePendingNotifications();
    ASSERT_EQ(2u, notifications.size());
    EXPECT_EQ("Second", notifications[0].Key);
    EXPECT_EQ(second, notifications[0].ChangedProperty);
    EXPECT_EQ("First", notifications[1].Key);
    EXPECT_EQ(first, notifications[1].ChangedProperty);

    second->SetInteger(21);
    const std::vector<Config::Edit> coalesced = {
        {"Batch", "Second", IProperty::INTEGER, 21, 22},
    };
    const Config::ApplyResult coalescedResult = config->ApplyEdits(mockMod, schemaRevision, coalesced);
    ASSERT_TRUE(coalescedResult.Succeeded());
    EXPECT_EQ(22, second->GetInteger());

    notifications = config->TakePendingNotifications();
    ASSERT_EQ(1u, notifications.size());
    EXPECT_EQ(second, notifications[0].ChangedProperty);
}

TEST_F(ConfigTest, ApplyEditsRejectsStaleOwnerAndSchema) {
    auto *property = static_cast<Property *>(config->GetProperty("Batch", "Value"));
    property->SetDefaultInteger(1);
    const std::uint64_t schemaRevision = config->GetSchemaRevision();
    const std::vector<Config::Edit> edits = {
        {"Batch", "Value", IProperty::INTEGER, 1, 2},
    };

    MockMod otherOwner(nullptr);
    Config::ApplyResult result = config->ApplyEdits(&otherOwner, schemaRevision, edits);
    EXPECT_EQ(Config::ApplyError::OwnerChanged, result.Error);
    EXPECT_EQ(1, property->GetInteger());

    config->GetProperty("Batch", "AddedAfterSnapshot")->SetDefaultBoolean(false);
    result = config->ApplyEdits(mockMod, schemaRevision, edits);
    EXPECT_EQ(Config::ApplyError::SchemaChanged, result.Error);
    EXPECT_EQ(1, property->GetInteger());
    EXPECT_EQ(0u, config->GetValueRevision());
    EXPECT_TRUE(config->TakePendingNotifications().empty());
}

TEST_F(ConfigTest, ApplyEditsUsesPerPropertyBaselinesInsteadOfGlobalValueRevision) {
    auto *edited = static_cast<Property *>(config->GetProperty("Batch", "Edited"));
    auto *external = static_cast<Property *>(config->GetProperty("Batch", "External"));
    edited->SetDefaultInteger(1);
    external->SetDefaultInteger(2);
    const std::uint64_t schemaRevision = config->GetSchemaRevision();

    external->SetInteger(3);
    ASSERT_EQ(1u, config->GetValueRevision());

    const Config::ApplyResult result = config->ApplyEdits(mockMod, schemaRevision, {
        {"Batch", "Edited", IProperty::INTEGER, 1, 10},
    });

    EXPECT_TRUE(result.Succeeded());
    EXPECT_EQ(10, edited->GetInteger());
    EXPECT_EQ(3, external->GetInteger());
    EXPECT_EQ(2u, config->GetValueRevision());
}

TEST_F(ConfigTest, ApplyEditsRejectsTypeBaseAndDuplicateConflicts) {
    auto *property = static_cast<Property *>(config->GetProperty("Batch", "Value"));
    property->SetDefaultInteger(1);
    const std::uint64_t schemaRevision = config->GetSchemaRevision();

    Config::ApplyResult result = config->ApplyEdits(mockMod, schemaRevision, {
        {"Batch", "Value", IProperty::STRING, std::string("1"), std::string("2")},
    });
    EXPECT_EQ(Config::ApplyError::TypeChanged, result.Error);

    result = config->ApplyEdits(mockMod, schemaRevision, {
        {"Batch", "Value", IProperty::INTEGER, 0, 2},
    });
    EXPECT_EQ(Config::ApplyError::BaseChanged, result.Error);

    result = config->ApplyEdits(mockMod, schemaRevision, {
        {"Batch", "Value", IProperty::INTEGER, 1, 2},
        {"Batch", "Value", IProperty::INTEGER, 1, 3},
    });
    EXPECT_EQ(Config::ApplyError::DuplicateTarget, result.Error);
    EXPECT_EQ(1u, result.EditIndex);
    EXPECT_EQ(1, property->GetInteger());
    EXPECT_EQ(0u, config->GetValueRevision());
    EXPECT_TRUE(config->TakePendingNotifications().empty());
}

TEST_F(ConfigTest, ApplyEditsValidatesEntireBatchBeforeWriting) {
    auto *first = static_cast<Property *>(config->GetProperty("Batch", "First"));
    auto *second = static_cast<Property *>(config->GetProperty("Batch", "Second"));
    first->SetDefaultInteger(1);
    second->SetDefaultInteger(2);
    const std::uint64_t schemaRevision = config->GetSchemaRevision();

    const Config::ApplyResult result = config->ApplyEdits(mockMod, schemaRevision, {
        {"Batch", "First", IProperty::INTEGER, 1, 10},
        {"Batch", "Second", IProperty::INTEGER, 99, 20},
    });

    EXPECT_EQ(Config::ApplyError::BaseChanged, result.Error);
    EXPECT_EQ(1u, result.EditIndex);
    EXPECT_EQ(1, first->GetInteger());
    EXPECT_EQ(2, second->GetInteger());
    EXPECT_EQ(0u, config->GetValueRevision());
    EXPECT_TRUE(config->TakePendingNotifications().empty());
}

// Modification notification
TEST_F(ConfigTest, ModificationNotification) {
    mockMod->modifiedCount = 0;
    IProperty *prop = config->GetProperty("TestCategory", "TestProp");

    // Set initial value
    prop->SetString("Initial");
    EXPECT_EQ(0, mockMod->modifiedCount);
    EXPECT_TRUE(config->IsDirty());

    auto notifications = config->TakePendingNotifications();
    ASSERT_EQ(1u, notifications.size());
    mockMod->OnModifyConfig(
        notifications[0].Category.c_str(),
        notifications[0].Key.c_str(),
        notifications[0].ChangedProperty);
    EXPECT_EQ(1, mockMod->modifiedCount);
    EXPECT_EQ("TestCategory", mockMod->lastCategory);
    EXPECT_EQ("TestProp", mockMod->lastKey);
    EXPECT_EQ(prop, mockMod->lastProp);

    // Setting same value shouldn't trigger notification
    prop->SetString("Initial");
    EXPECT_EQ(1, mockMod->modifiedCount);

    // Setting different value should trigger notification
    prop->SetString("Changed");
    prop->SetString("Changed Again");
    notifications = config->TakePendingNotifications();
    ASSERT_EQ(1u, notifications.size());
    mockMod->OnModifyConfig(
        notifications[0].Category.c_str(),
        notifications[0].Key.c_str(),
        notifications[0].ChangedProperty);
    EXPECT_EQ(2, mockMod->modifiedCount);
    EXPECT_STREQ("Changed Again", prop->GetString());

    // Test with null mod (shouldn't crash)
    Config *nullModConfig = new Config(nullptr);
    IProperty *nullModProp = nullModConfig->GetProperty("TestCategory", "TestProp");
    nullModProp->SetString("Test"); // Shouldn't crash
    delete nullModConfig;
}

// Property utility functions
TEST_F(ConfigTest, PropertyUtilityFunctions) {
    // Test GetStringSize
    Property *strProp = static_cast<Property *>(config->GetProperty("TestCategory", "StringProp"));
    strProp->SetString("Test String");
    EXPECT_EQ(11u, strProp->GetStringSize());

    strProp->SetString("");
    EXPECT_EQ(0u, strProp->GetStringSize());

    // Test GetHash
    strProp->SetString("Test String");
    size_t hash1 = strProp->GetHash();

    // Same string should have same hash
    strProp->SetString("Test String");
    EXPECT_EQ(hash1, strProp->GetHash());

    // Different string should have different hash
    strProp->SetString("Different String");
    EXPECT_NE(hash1, strProp->GetHash());

}

// Property value copying
TEST_F(ConfigTest, PropertyCopy) {
    Property *srcProp = static_cast<Property *>(config->GetProperty("SourceCategory", "SourceProp"));
    Property *destProp = static_cast<Property *>(config->GetProperty("DestCategory", "DestProp"));

    // Test string copy
    srcProp->SetString("Test String");
    destProp->CopyValue(srcProp);
    EXPECT_EQ(IProperty::STRING, destProp->GetType());
    EXPECT_STREQ("Test String", destProp->GetString());

    // Test boolean copy
    srcProp->SetBoolean(true);
    destProp->CopyValue(srcProp);
    EXPECT_EQ(IProperty::BOOLEAN, destProp->GetType());
    EXPECT_TRUE(destProp->GetBoolean());

    // Test null copy (should not crash)
    destProp->CopyValue(nullptr);
    EXPECT_EQ(IProperty::BOOLEAN, destProp->GetType()); // Should remain unchanged
}

// File I/O
TEST_F(ConfigTest, FileIO) {
    // Create test config file
    const wchar_t *filename = L"test_config.cfg";

    // Set up some properties
    IProperty *strProp = config->GetProperty("TestCategory", "StringProp");
    strProp->SetString("Test String");
    strProp->SetComment("String Property Comment");

    IProperty *boolProp = config->GetProperty("TestCategory", "BoolProp");
    boolProp->SetBoolean(true);

    config->SetCategoryComment("TestCategory", "Test Category Comment");

    // Save the config
    EXPECT_TRUE(config->Save(filename));

    // Create a new config and load the file
    MockMod *newMockMod = new MockMod(nullptr);
    Config *newConfig = new Config(newMockMod);
    EXPECT_TRUE(newConfig->Load(filename));

    // Check that properties were loaded correctly
    EXPECT_TRUE(newConfig->HasCategory("TestCategory"));
    EXPECT_TRUE(newConfig->HasKey("TestCategory", "StringProp"));
    EXPECT_TRUE(newConfig->HasKey("TestCategory", "BoolProp"));

    IProperty *loadedStrProp = newConfig->GetProperty("TestCategory", "StringProp");
    EXPECT_EQ(IProperty::STRING, loadedStrProp->GetType());
    EXPECT_STREQ("Test String", loadedStrProp->GetString());
    EXPECT_STREQ("String Property Comment", static_cast<Property*>(loadedStrProp)->GetComment());

    IProperty *loadedBoolProp = newConfig->GetProperty("TestCategory", "BoolProp");
    EXPECT_EQ(IProperty::BOOLEAN, loadedBoolProp->GetType());
    EXPECT_TRUE(loadedBoolProp->GetBoolean());

    EXPECT_STREQ("Test Category Comment", newConfig->GetCategoryComment("TestCategory"));

    // Clean up
    delete newConfig;
    delete newMockMod;

    // Test error handling
    EXPECT_FALSE(config->Save(nullptr));
    EXPECT_FALSE(config->Save(L""));
    EXPECT_FALSE(config->Save(L"/invalid/path/file.cfg"));

    EXPECT_FALSE(config->Load(nullptr));
    EXPECT_FALSE(config->Load(L""));
    EXPECT_FALSE(config->Load(L"nonexistent_file.cfg"));
}

TEST_F(ConfigTest, LoadingDuplicateEntriesUpdatesOneStableProperty) {
    const wchar_t *filename = L"test_config_duplicates.cfg";
    {
        std::ofstream output("test_config_duplicates.cfg", std::ios::binary);
        output << "# First category\n"
                  "General {\n"
                  "# First value\n"
                  "I Count 1\n"
                  "}\n"
                  "# Updated category\n"
                  "General {\n"
                  "# Updated value\n"
                  "I Count 2\n"
                  "}\n";
    }

    ASSERT_TRUE(config->Load(filename));
    ASSERT_EQ(config->GetCategoryCount(), 1U);
    Category *category = config->GetCategory(static_cast<std::size_t>(0));
    ASSERT_NE(category, nullptr);
    EXPECT_STREQ(category->GetComment(), "Updated category");
    ASSERT_EQ(category->GetPropertyCount(), 1U);
    Property *property = category->GetProperty(static_cast<std::size_t>(0));
    ASSERT_NE(property, nullptr);
    EXPECT_EQ(property, category->GetProperty("Count"));
    EXPECT_EQ(property->GetInteger(), 2);
    EXPECT_STREQ(property->GetComment(), "Updated value");
    EXPECT_TRUE(config->TakePendingNotifications().empty());
    EXPECT_FALSE(config->IsDirty());

    _wremove(filename);
}

TEST_F(ConfigTest, SaveUsesSnapshottedModMetadata) {
    const wchar_t *filename = L"test_config_metadata.cfg";
    mockMod->nameReadCount = 0;
    mockMod->versionReadCount = 0;
    config->SnapshotModMetadata();
    EXPECT_EQ(1, mockMod->nameReadCount);
    EXPECT_EQ(1, mockMod->versionReadCount);

    mockMod->idReadCount = 0;
    mockMod->nameReadCount = 0;
    mockMod->versionReadCount = 0;

    ASSERT_TRUE(config->Save(filename));
    EXPECT_EQ(0, mockMod->idReadCount);
    EXPECT_EQ(0, mockMod->nameReadCount);
    EXPECT_EQ(0, mockMod->versionReadCount);

    _wremove(filename);
}

// Performance test
TEST_F(ConfigTest, PropertyLookupPerformance) {
    const int NUM_CATEGORIES = 10;
    const int PROPS_PER_CATEGORY = 100;
    const int NUM_LOOKUPS = 10000;

    // Create properties
    for (int c = 0; c < NUM_CATEGORIES; c++) {
        std::string category = "PerfCategory" + std::to_string(c);
        for (int p = 0; p < PROPS_PER_CATEGORY; p++) {
            std::string key = "Prop" + std::to_string(p);
            IProperty *prop = config->GetProperty(category.c_str(), key.c_str());
            prop->SetInteger(c * 1000 + p);
        }
    }

    // Time lookup operations
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_LOOKUPS; i++) {
        int c = i % NUM_CATEGORIES;
        int p = i % PROPS_PER_CATEGORY;
        std::string category = "PerfCategory" + std::to_string(c);
        std::string key = "Prop" + std::to_string(p);
        IProperty *prop = config->GetProperty(category.c_str(), key.c_str());
        int expected = c * 1000 + p;
        EXPECT_EQ(expected, prop->GetInteger());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Output for informational purposes
    std::cout << "Performed " << NUM_LOOKUPS << " property lookups in " << duration << " ms" << std::endl;

    // This is primarily a performance test, but we can set a reasonable upper bound
    EXPECT_LT(duration, 5000); // Should be well under 5 seconds on any modern system
}

// Main function to run all tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

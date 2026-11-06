#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <locale>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#endif
#include "Platform/FileError.h"

#include "Loader/ModRecord.h"

#include "Config/Config.h"
#include "Config/ConfigStore.h"
#include "Logging/Logger.h"
#include "Mods/BMLConfigMigration.h"

#include "PathUtils.h"
#include "StringUtils.h"

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
    FILE *out_files[] = {
        stdout,
    };

    for (FILE *file : out_files) {
        fprintf(file, "[%s/%s]: ", m_ModName.c_str(), level);
        vfprintf(file, fmt, args);
        fputc('\n', file);
        fflush(file);
    }
}

class MockMod {
public:
    explicit MockMod(ModContext *context) : Registration(context) { Registration.Bind(*this); }

    const char *GetID() {
        ++idReadCount;
        return "MockMod";
    }

    const char *GetName() {
        ++nameReadCount;
        return "MockModName";
    }

    const char *GetVersion() {
        ++versionReadCount;
        return "1.0";
    }

    const char *GetAuthor() { return "Tester"; }

    const char *GetDescription() { return "Test description for the mock mod."; }

    BMLVersion GetBMLVersion() { return {}; }

    void OnModifyConfig(const char *category, const char *key, Property *prop) {
        modifiedCount++;
        lastCategory = category ? category : "";
        lastKey = key ? key : "";
        lastProp = prop;
    }

    ModRecord Registration;
    std::atomic<int> modifiedCount{0};
    std::atomic<int> idReadCount{0};
    std::atomic<int> nameReadCount{0};
    std::atomic<int> versionReadCount{0};
    std::string lastCategory;
    std::string lastKey;
    Property *lastProp = nullptr;
};

// Test fixture for Config tests
class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        configPath = utils::CreateTempFileW(L"BML-config-");
        ASSERT_FALSE(configPath.empty());
        mockMod = new MockMod(nullptr);
        config = new Config(&mockMod->Registration);
    }

    void TearDown() override {
        delete config;
        delete mockMod;
        utils::DeleteFileW(configPath);
    }

    MockMod *mockMod = nullptr;
    Config *config = nullptr;
    std::wstring configPath;

    void WriteConfig(const std::string &text) {
        std::ofstream output(std::filesystem::path(configPath), std::ios::binary);
        ASSERT_TRUE(output);
        output << text;
        ASSERT_TRUE(output);
    }

    void ReloadConfig() {
        ASSERT_TRUE(config->Save(configPath.c_str()));
        auto reloaded = std::make_unique<Config>(&mockMod->Registration);
        ASSERT_TRUE(reloaded->Load(configPath.c_str()));
        delete config;
        config = reloaded.release();
    }
};

TEST_F(ConfigTest, InvalidValueDoesNotDiscardFollowingSettings) {
    WriteConfig("General {\nI Invalid not-a-number\nI\nI MissingValue\nI Count 42\nB Flag 1\n}\n");
    ASSERT_TRUE(config->Load(configPath.c_str()));
    EXPECT_FALSE(config->HasKey("General", "Invalid"));
    EXPECT_FALSE(config->HasKey("General", "MissingValue"));
    EXPECT_EQ(config->GetProperty("General", "Count")->GetInteger(), 42);
    EXPECT_TRUE(config->GetProperty("General", "Flag")->GetBoolean());
}

TEST_F(ConfigTest, FailedLoadCannotOverwriteExistingDocument) {
    const std::string document = "General {\nI Count 42\n";
    WriteConfig(document);
    EXPECT_FALSE(config->Load(configPath.c_str()));
    config->GetProperty("General", "Count")->SetDefaultInteger(7);
    EXPECT_FALSE(config->Save(configPath.c_str()));
    std::string preserved;
    ASSERT_TRUE(utils::ReadFileBytesW(configPath, preserved));
    EXPECT_EQ(preserved, document);
}

TEST_F(ConfigTest, FailedLoadRemainsProtectedAfterTheFileDisappears) {
    WriteConfig("General {\n");
    EXPECT_FALSE(config->Load(configPath.c_str()));
    ASSERT_TRUE(utils::DeleteFileW(configPath));
    config->GetProperty("General", "Count")->SetInteger(7);
    EXPECT_FALSE(config->Save(configPath.c_str()));
    EXPECT_TRUE(config->IsDirty());
    EXPECT_FALSE(utils::FileExistsW(configPath));
}

TEST_F(ConfigTest, MissingDocumentCanBeGeneratedFromCodeDefaults) {
    ASSERT_TRUE(utils::DeleteFileW(configPath));
    EXPECT_FALSE(config->Load(configPath.c_str()));
    EXPECT_EQ(config->GetLoadStatus(), Config::LoadStatus::Missing);
    EXPECT_TRUE(config->CanSave());
    config->GetProperty("General", "Count")->SetDefaultInteger(7);
    ASSERT_TRUE(config->Save(configPath.c_str()));
    Config loaded(&mockMod->Registration);
    ASSERT_TRUE(loaded.Load(configPath.c_str()));
    EXPECT_EQ(loaded.GetProperty("General", "Count")->GetInteger(), 7);
}

TEST_F(ConfigTest, MissingDocumentCreatedAfterLoadIsNotOverwritten) {
    ASSERT_TRUE(utils::DeleteFileW(configPath));
    EXPECT_FALSE(config->Load(configPath.c_str()));
    config->GetProperty("General", "Count")->SetInteger(7);
    const std::string document = "General {\nI Count 42\n}\n";
    WriteConfig(document);
    EXPECT_FALSE(config->Save(configPath.c_str()));
    EXPECT_TRUE(config->IsDirty());
    std::string preserved;
    ASSERT_TRUE(utils::ReadFileBytesW(configPath, preserved));
    EXPECT_EQ(preserved, document);
}

#ifdef _WIN32
TEST_F(ConfigTest, ReadFailureCannotBeSavedAfterTheSharingLockIsReleased) {
    const std::string document = "General {\nI Count 42\n}\n";
    WriteConfig(document);
    HANDLE file = CreateFileW(configPath.c_str(), GENERIC_READ, 0, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(file, INVALID_HANDLE_VALUE);
    EXPECT_FALSE(config->Load(configPath.c_str()));
    EXPECT_EQ(config->GetLoadStatus(), Config::LoadStatus::Failed);
    EXPECT_EQ(config->GetLoadError(), ERROR_SHARING_VIOLATION);
    CloseHandle(file);
    config->GetProperty("General", "Count")->SetInteger(7);
    EXPECT_FALSE(config->CanSave());
    EXPECT_FALSE(config->Save(configPath.c_str()));
    std::string preserved;
    ASSERT_TRUE(utils::ReadFileBytesW(configPath, preserved));
    EXPECT_EQ(preserved, document);
}
#endif

TEST_F(ConfigTest, SuccessfulReloadReenablesPersistence) {
    WriteConfig("General {\n");
    EXPECT_FALSE(config->Load(configPath.c_str()));
    EXPECT_FALSE(config->CanSave());
    WriteConfig("General {\nI Count 42\n}\n");
    ASSERT_TRUE(config->Load(configPath.c_str()));
    EXPECT_TRUE(config->CanSave());
    EXPECT_EQ(config->GetLoadError(), BML::Platform::FileError::Success);
    Property *count = config->GetProperty("General", "Count");
    EXPECT_EQ(count->GetInteger(), 42);
    count->SetInteger(7);
    ASSERT_TRUE(config->Save(configPath.c_str()));
    Config loaded(&mockMod->Registration);
    ASSERT_TRUE(loaded.Load(configPath.c_str()));
    EXPECT_EQ(loaded.GetProperty("General", "Count")->GetInteger(), 7);
}

#ifdef _WIN32
TEST_F(ConfigTest, FailedReplacementPreservesOriginalAndDirtyState) {
    config->GetProperty("General", "Count")->SetInteger(7);
    ASSERT_TRUE(config->Save(configPath.c_str()));
    std::string original;
    ASSERT_TRUE(utils::ReadFileBytesW(configPath, original));
    config->GetProperty("General", "Count")->SetInteger(42);
    HANDLE file = CreateFileW(configPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(file, INVALID_HANDLE_VALUE);
    EXPECT_FALSE(config->Save(configPath.c_str()));
    EXPECT_TRUE(config->IsDirty());
    CloseHandle(file);
    std::string preserved;
    ASSERT_TRUE(utils::ReadFileBytesW(configPath, preserved));
    EXPECT_EQ(preserved, original);
    ASSERT_TRUE(config->Save(configPath.c_str()));
    Config loaded(&mockMod->Registration);
    ASSERT_TRUE(loaded.Load(configPath.c_str()));
    EXPECT_EQ(loaded.GetProperty("General", "Count")->GetInteger(), 42);
}
#endif

#ifdef _WIN32
TEST_F(ConfigTest, FailedAutomaticSaveDoesNotRetryWithoutAnotherChange) {
    Property *count = config->GetProperty("General", "Count");
    count->SetInteger(7);
    ASSERT_TRUE(config->Save(configPath.c_str()));
    count->SetInteger(42);
    ASSERT_TRUE(config->TakeSaveRequest());
    HANDLE file = CreateFileW(configPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(file, INVALID_HANDLE_VALUE);
    EXPECT_FALSE(config->Save(configPath.c_str()));
    CloseHandle(file);

    EXPECT_TRUE(config->IsDirty());
    EXPECT_FALSE(config->TakeSaveRequest());
    count->SetInteger(42);
    EXPECT_FALSE(config->TakeSaveRequest());
    count->SetInteger(43);
    EXPECT_TRUE(config->TakeSaveRequest());
    ASSERT_TRUE(config->Save(configPath.c_str()));
    Config loaded(&mockMod->Registration);
    ASSERT_TRUE(loaded.Load(configPath.c_str()));
    EXPECT_EQ(loaded.GetProperty("General", "Count")->GetInteger(), 43);
}
#endif

TEST_F(ConfigTest, ExplicitSaveCanRetryUnchangedDirtyValues) {
    config->GetProperty("General", "Count")->SetInteger(42);
    ASSERT_TRUE(config->TakeSaveRequest());
    EXPECT_FALSE(config->Save(nullptr));
    EXPECT_TRUE(config->IsDirty());
    EXPECT_FALSE(config->TakeSaveRequest());
    EXPECT_TRUE(config->TakeSaveRequest(true));
    ASSERT_TRUE(config->Save(configPath.c_str()));
    EXPECT_FALSE(config->IsDirty());
    EXPECT_FALSE(config->TakeSaveRequest());
}

TEST_F(ConfigTest, DetachedDirtyValuesReattachWithoutRetiredDeclarationMetadata) {
    auto *count = config->GetProperty("General", "Count");
    count->SetDefaultInteger(7);
    count->SetInteger(42);
    count->SetComment("Old declaration");
    config->GetProperty("Retired", "OldKey")->SetString("unsaved");
    EXPECT_FALSE(config->Save(nullptr));
    ASSERT_TRUE(config->IsDirty());
    config->DetachMod();
    EXPECT_EQ(config->GetMod(), nullptr);
    config->AttachMod(&mockMod->Registration);
    EXPECT_EQ(config->GetCategoryCount(), 0u);
    auto *newCount = config->GetProperty("General", "Count");
    newCount->SetDefaultInteger(9);
    EXPECT_EQ(newCount->GetInteger(), 42);
    EXPECT_STREQ(static_cast<Property *>(newCount)->GetComment(), "");
    ASSERT_TRUE(config->Save(configPath.c_str()));
    std::string saved;
    ASSERT_TRUE(utils::ReadFileBytesW(configPath, saved));
    EXPECT_EQ(saved.find("Retired"), std::string::npos);
}

TEST_F(ConfigTest, SaveRequestIsConsumedBeforeMetadataOrSerialization) {
    config->GetProperty("General", "Count")->SetInteger(42);
    ASSERT_TRUE(config->TakeSaveRequest());
    // The caller can throw before reaching Save, for example in Mod metadata.
    EXPECT_TRUE(config->IsDirty());
    EXPECT_FALSE(config->TakeSaveRequest());
    ASSERT_TRUE(config->RemoveProperty("General", "Count"));
    EXPECT_TRUE(config->TakeSaveRequest());
}

TEST_F(ConfigTest, SaveFailureDoesNotSuppressConfigNotifications) {
    Property *count = config->GetProperty("General", "Count");
    count->SetInteger(42);
    ASSERT_TRUE(config->TakeSaveRequest());
    EXPECT_FALSE(config->Save(nullptr));
    const auto pending = config->TakePendingNotifications();
    ASSERT_EQ(pending.size(), 1u);
    EXPECT_EQ(pending.front().ChangedProperty, count);
    EXPECT_FALSE(config->TakeSaveRequest());
}

TEST_F(ConfigTest, ValidReloadRearmsAnUnsuccessfulSave) {
    config->GetProperty("General", "Count")->SetInteger(42);
    ASSERT_TRUE(config->TakeSaveRequest());
    EXPECT_FALSE(config->Save(nullptr));
    EXPECT_FALSE(config->TakeSaveRequest());
    WriteConfig("General {\nI Count 7\n}\n");
    ASSERT_TRUE(config->Load(configPath.c_str()));
    EXPECT_TRUE(config->TakeSaveRequest());
    ASSERT_TRUE(config->Save(configPath.c_str()));
    EXPECT_FALSE(config->TakeSaveRequest());
}

TEST_F(ConfigTest, TruncatedDocumentDoesNotReplaceLoadedOrDeclaredValues) {
    config->GetProperty("General", "Count")->SetDefaultInteger(42);
    ASSERT_TRUE(config->Save(configPath.c_str()));
    Config loaded(&mockMod->Registration);
    ASSERT_TRUE(loaded.Load(configPath.c_str()));
    Property *count = loaded.GetProperty("General", "Count");
    count->SetDefaultInteger(1);
    const std::uint64_t revision = loaded.GetValueRevision();

    WriteConfig("General {\nI Count 99\nI Other 10\n");
    EXPECT_FALSE(loaded.Load(configPath.c_str()));
    EXPECT_EQ(count->GetInteger(), 42);
    EXPECT_FALSE(loaded.HasKey("General", "Other"));
    EXPECT_EQ(loaded.GetValueRevision(), revision);
}

TEST_F(ConfigTest, Utf8BomDoesNotBecomePartOfCategoryName) {
    WriteConfig("\xEF\xBB\xBFGeneral {\r\nS Text 中文 value\r\n}\r\n");
    ASSERT_TRUE(config->Load(configPath.c_str()));
    EXPECT_TRUE(config->HasCategory("General"));
    EXPECT_STREQ(config->GetProperty("General", "Text")->GetString(), "中文 value");
}

TEST_F(ConfigTest, EmbeddedNullDoesNotReplaceLoadedValues) {
    config->GetProperty("General", "Count")->SetDefaultInteger(42);
    ASSERT_TRUE(config->Save(configPath.c_str()));
    Config loaded(&mockMod->Registration);
    ASSERT_TRUE(loaded.Load(configPath.c_str()));
    const char invalid[] = "General {\nS Text before\0after\n}\n";
    WriteConfig(std::string(invalid, sizeof(invalid) - 1));
    EXPECT_FALSE(loaded.Load(configPath.c_str()));
    EXPECT_EQ(loaded.GetProperty("General", "Count")->GetInteger(), 42);
}

TEST_F(ConfigTest, NumericEntriesRequireCompleteFiniteValues) {
    WriteConfig("General {\nI Prefix 12junk\nB Flag 2\nF Huge 1e999\nF Scale 2.5\nK Key 30\nI Count -42\n}\n");
    ASSERT_TRUE(config->Load(configPath.c_str()));
    EXPECT_FALSE(config->HasKey("General", "Prefix"));
    EXPECT_FALSE(config->HasKey("General", "Flag"));
    EXPECT_FALSE(config->HasKey("General", "Huge"));
    EXPECT_FLOAT_EQ(config->GetProperty("General", "Scale")->GetFloat(), 2.5f);
    EXPECT_EQ(config->GetProperty("General", "Key")->GetType(), PropertyType::KEY);
    EXPECT_EQ(config->GetProperty("General", "Count")->GetInteger(), -42);
}

TEST_F(ConfigTest, UnknownEntryDoesNotConsumeFollowingKnownEntries) {
    WriteConfig("General\n{\n# Comment\nFutureType Old value\nS Text braces { } # remain text\nI Count 42\n}\n");
    ASSERT_TRUE(config->Load(configPath.c_str()));
    EXPECT_FALSE(config->HasKey("General", "Old"));
    EXPECT_STREQ(config->GetProperty("General", "Text")->GetString(), "braces { } # remain text");
    EXPECT_EQ(config->GetProperty("General", "Count")->GetInteger(), 42);
}

TEST_F(ConfigTest, SavingPreservesFloatPrecision) {
    const float value = std::nextafter(1.0f, 2.0f);
    config->GetProperty("General", "Scale")->SetDefaultFloat(value);
    ASSERT_TRUE(config->Save(configPath.c_str()));
    Config reloaded(&mockMod->Registration);
    ASSERT_TRUE(reloaded.Load(configPath.c_str()));
    EXPECT_EQ(reloaded.GetProperty("General", "Scale")->GetFloat(), value);
}

TEST_F(ConfigTest, NumericPersistenceDoesNotDependOnGlobalLocale) {
    class DecimalComma : public std::numpunct<char> {
    protected:
        char do_decimal_point() const { return ','; }
    };
    class ScopedLocale {
    public:
        ScopedLocale() : previous(std::locale::global(std::locale(std::locale::classic(), new DecimalComma))) {}
        ~ScopedLocale() { std::locale::global(previous); }
    private:
        std::locale previous;
    };

    config->GetProperty("General", "Scale")->SetDefaultFloat(2.5f);
    {
        ScopedLocale locale;
        ASSERT_TRUE(config->Save(configPath.c_str()));
        Config loaded(&mockMod->Registration);
        ASSERT_TRUE(loaded.Load(configPath.c_str()));
        EXPECT_FLOAT_EQ(loaded.GetProperty("General", "Scale")->GetFloat(), 2.5f);
    }
    Config loaded(&mockMod->Registration);
    ASSERT_TRUE(loaded.Load(configPath.c_str()));
    EXPECT_FLOAT_EQ(loaded.GetProperty("General", "Scale")->GetFloat(), 2.5f);
}

TEST_F(ConfigTest, InvalidStructureLeavesLoadedValuesUnchanged) {
    config->GetProperty("General", "Count")->SetDefaultInteger(42);
    ASSERT_TRUE(config->Save(configPath.c_str()));
    Config loaded(&mockMod->Registration);
    ASSERT_TRUE(loaded.Load(configPath.c_str()));
    const char embeddedNull[] = "General {\nS Text abc\0def\n}\n";
    const std::string invalidDocuments[] = {
        "}\n", "General {\nNested {\n}\n}\n", "General\n", "{\n}\n",
        std::string(embeddedNull, std::size(embeddedNull) - 1),
    };
    for (const std::string &document : invalidDocuments) {
        WriteConfig(document);
        EXPECT_FALSE(loaded.Load(configPath.c_str()));
        EXPECT_EQ(loaded.GetProperty("General", "Count")->GetInteger(), 42);
    }
}

TEST_F(ConfigTest, UpdatingModKeepsOnlyDeclaredSettingsAndUsesDefaultsForNewOnes) {
    config->GetProperty("General", "A")->SetInteger(10);
    config->GetProperty("General", "B")->SetInteger(20);
    config->GetProperty("RemovedCategory", "Old")->SetBoolean(true);
    ASSERT_TRUE(config->Save(configPath.c_str()));

    Config updated(&mockMod->Registration);
    ASSERT_TRUE(updated.Load(configPath.c_str()));
    EXPECT_EQ(updated.GetCategoryCount(), 0u);
    Property *retained = updated.GetProperty("General", "A");
    retained->SetDefaultInteger(1);
    Property *added = updated.GetProperty("General", "C");
    added->SetDefaultInteger(30);

    EXPECT_EQ(retained->GetInteger(), 10);
    EXPECT_EQ(added->GetInteger(), 30);
    ASSERT_EQ(updated.GetCategoryCount(), 1u);
    Category *category = updated.GetCategory(std::size_t{0});
    ASSERT_NE(category, nullptr);
    ASSERT_EQ(category->GetPropertyCount(), 2u);
    EXPECT_EQ(category->GetProperty(std::size_t{0}), retained);
    EXPECT_EQ(category->GetProperty(std::size_t{1}), added);
    EXPECT_FALSE(category->HasKey("B"));
    EXPECT_TRUE(updated.TakePendingNotifications().empty());
    const Config::ApplyResult obsoleteEdit =
        updated.ApplyEdits(&mockMod->Registration, updated.GetSchemaRevision(),
                           {{"General", "B", PropertyType::INTEGER, 20, 21}});
    EXPECT_EQ(obsoleteEdit.Error, Config::ApplyError::PropertyMissing);
    EXPECT_FALSE(category->HasKey("B"));

    ASSERT_TRUE(updated.Save(configPath.c_str()));
    Config reloaded(&mockMod->Registration);
    ASSERT_TRUE(reloaded.Load(configPath.c_str()));
    EXPECT_FALSE(reloaded.HasKey("General", "B"));
    EXPECT_FALSE(reloaded.HasCategory("RemovedCategory"));
    EXPECT_EQ(reloaded.GetProperty("General", "A")->GetInteger(), 10);
    EXPECT_EQ(reloaded.GetProperty("General", "C")->GetInteger(), 30);
}

TEST_F(ConfigTest, SavedValueQueriesDoNotDeclareSettingsOrChangeSchema) {
    config->GetProperty("General", "Value")->SetDefaultInteger(42);
    ASSERT_TRUE(config->Save(configPath.c_str()));

    Config loaded(&mockMod->Registration);
    const std::uint64_t schema = loaded.GetSchemaRevision();
    ASSERT_TRUE(loaded.Load(configPath.c_str()));
    EXPECT_TRUE(loaded.HasCategory("General"));
    EXPECT_TRUE(loaded.HasKey("General", "Value"));
    EXPECT_FALSE(loaded.HasKey("General", "Missing"));
    EXPECT_EQ(loaded.GetCategoryCount(), 0u);
    EXPECT_EQ(loaded.GetSchemaRevision(), schema);
    EXPECT_EQ(loaded.GetValueRevision(), 0u);
    EXPECT_FALSE(loaded.IsDirty());
    EXPECT_TRUE(loaded.TakePendingNotifications().empty());

    ASSERT_TRUE(loaded.Save(configPath.c_str()));
    EXPECT_EQ(loaded.GetCategoryCount(), 0u);
    EXPECT_EQ(loaded.GetSchemaRevision(), schema);
    // A Mod may declare a setting later, even after an unrelated config save.
    Property *property = loaded.GetProperty("General", "Value");
    property->SetDefaultInteger(1);
    EXPECT_EQ(property->GetInteger(), 42);
}

TEST_F(ConfigTest, SettingsFollowDeclarationOrderAndIgnoreSavedComments) {
    config->SetCategoryComment("General", "Old category description");
    Property *first = config->GetProperty("General", "First");
    first->SetComment("Old property description");
    first->SetDefaultInteger(10);
    config->GetProperty("General", "Second")->SetDefaultInteger(20);
    ASSERT_TRUE(config->Save(configPath.c_str()));

    Config loaded(&mockMod->Registration);
    ASSERT_TRUE(loaded.Load(configPath.c_str()));
    loaded.SetCategoryComment("General", "Current category description");
    Property *second = loaded.GetProperty("General", "Second");
    first = loaded.GetProperty("General", "First");
    EXPECT_STREQ(static_cast<Property *>(first)->GetComment(), "");
    first->SetComment("Current property description");
    Category *category = loaded.GetCategory(std::size_t{0});
    ASSERT_NE(category, nullptr);
    EXPECT_EQ(category->GetProperty(std::size_t{0}), second);
    EXPECT_EQ(category->GetProperty(std::size_t{1}), first);

    const std::uint64_t schema = loaded.GetSchemaRevision();
    ASSERT_TRUE(loaded.Load(configPath.c_str()));
    EXPECT_EQ(loaded.GetSchemaRevision(), schema);
    EXPECT_EQ(loaded.GetProperty("General", "First"), first);
    EXPECT_STREQ(static_cast<Property *>(first)->GetComment(), "Current property description");
    EXPECT_STREQ(category->GetComment(), "Current category description");
}

TEST_F(ConfigTest, DeclaredTypesAndEditorMetadataSurviveReload) {
    config->GetProperty("Values", "Text")->SetDefaultString("saved text");
    config->GetProperty("Values", "Flag")->SetDefaultBoolean(true);
    config->GetProperty("Values", "Count")->SetDefaultInteger(42);
    config->GetProperty("Values", "Scale")->SetDefaultFloat(2.5f);
    config->GetProperty("Values", "Key")->SetDefaultKey(CKKEY_A);
    ASSERT_TRUE(config->Save(configPath.c_str()));

    Config loaded(&mockMod->Registration);
    Property *text = loaded.GetProperty("Values", "Text");
    text->SetDefaultString("default text");
    const char *choices[] = {"default text", "saved text"};
    ASSERT_EQ(BML::ConfigMetadata::SetChoices(text, choices, std::size(choices)), 1);
    ASSERT_EQ(BML::ConfigMetadata::SetEditor(text, BML_CONFIG_EDITOR_CHOICE), 1);
    Property *flag = loaded.GetProperty("Values", "Flag");
    flag->SetDefaultBoolean(false);
    Property *count = loaded.GetProperty("Values", "Count");
    count->SetDefaultInteger(1);
    Property *scale = loaded.GetProperty("Values", "Scale");
    scale->SetDefaultFloat(1.0f);
    Property *key = loaded.GetProperty("Values", "Key");
    key->SetDefaultKey(CKKEY_B);
    const std::uint64_t schema = loaded.GetSchemaRevision();

    ASSERT_TRUE(loaded.Load(configPath.c_str()));
    EXPECT_STREQ(text->GetString(), "saved text");
    EXPECT_TRUE(flag->GetBoolean());
    EXPECT_EQ(count->GetInteger(), 42);
    EXPECT_FLOAT_EQ(scale->GetFloat(), 2.5f);
    EXPECT_EQ(key->GetKey(), CKKEY_A);
    EXPECT_EQ(BML::ConfigMetadata::GetEditor(text), BML_CONFIG_EDITOR_CHOICE);
    EXPECT_EQ(BML::ConfigMetadata::ChoiceCount(text), std::size(choices));
    EXPECT_EQ(loaded.GetSchemaRevision(), schema);
    EXPECT_EQ(loaded.GetValueRevision(), 5u);
    EXPECT_TRUE(loaded.TakePendingNotifications().empty());
    EXPECT_FALSE(loaded.IsDirty());

    Config changedType(&mockMod->Registration);
    ASSERT_TRUE(changedType.Load(configPath.c_str()));
    Property *changed = changedType.GetProperty("Values", "Count");
    changed->SetDefaultString("new default");
    EXPECT_STREQ(changed->GetString(), "new default");
    ASSERT_TRUE(changedType.Load(configPath.c_str()));
    EXPECT_EQ(changed->GetType(), PropertyType::STRING);
    EXPECT_STREQ(changed->GetString(), "new default");
    ASSERT_TRUE(changedType.Save(configPath.c_str()));
    Config reloaded(&mockMod->Registration);
    ASSERT_TRUE(reloaded.Load(configPath.c_str()));
    EXPECT_STREQ(reloaded.GetProperty("Values", "Count")->GetString(), "new default");
}

TEST_F(ConfigTest, AppliedSettingsSurviveShutdownAndStartupSaves) {
    config->GetProperty("Values", "Text")->SetDefaultString("default text");
    config->GetProperty("Values", "Flag")->SetDefaultBoolean(false);
    config->GetProperty("Values", "Count")->SetDefaultInteger(1);
    config->GetProperty("Values", "Scale")->SetDefaultFloat(1.0f);
    config->GetProperty("Values", "Key")->SetDefaultKey(CKKEY_A);
    ASSERT_TRUE(config->Save(configPath.c_str()));

    const std::vector<Config::Edit> edits = {
        {"Values", "Text", PropertyType::STRING, std::string("default text"),
         std::string("edited text")},
        {"Values", "Flag", PropertyType::BOOLEAN, false, true},
        {"Values", "Count", PropertyType::INTEGER, 1, 42},
        {"Values", "Scale", PropertyType::FLOAT, 1.0f, 2.5f},
        {"Values", "Key", PropertyType::KEY, static_cast<int>(CKKEY_A), static_cast<int>(CKKEY_B)},
    };
    const Config::ApplyResult result =
        config->ApplyEdits(&mockMod->Registration, config->GetSchemaRevision(), edits);
    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.ChangedCount, edits.size());
    EXPECT_TRUE(config->IsDirty());
    ASSERT_TRUE(config->TakeSaveRequest());
    ASSERT_TRUE(config->Save(configPath.c_str()));
    delete config;
    config = nullptr;
    delete mockMod;
    mockMod = nullptr;

    for (int restart = 0; restart < 2; ++restart) {
        MockMod owner(nullptr);
        Config reloaded(&owner.Registration);
        ASSERT_TRUE(reloaded.Load(configPath.c_str()));
        reloaded.GetProperty("Values", "Text")->SetDefaultString("default text");
        reloaded.GetProperty("Values", "Flag")->SetDefaultBoolean(false);
        reloaded.GetProperty("Values", "Count")->SetDefaultInteger(1);
        reloaded.GetProperty("Values", "Scale")->SetDefaultFloat(1.0f);
        reloaded.GetProperty("Values", "Key")->SetDefaultKey(CKKEY_A);
        ASSERT_TRUE(reloaded.TakeSaveRequest(true));
        ASSERT_TRUE(reloaded.Save(configPath.c_str()));
        for (const Config::Edit &edit : edits) {
            auto *property = static_cast<Property *>(reloaded.GetProperty(edit.Category.c_str(), edit.Key.c_str()));
            EXPECT_TRUE(ConfigValuesEqual(edit.ExpectedType, property->GetValue(), edit.NewValue)) << edit.Key;
        }
    }
}

TEST_F(ConfigTest, RemovingLoadedSettingPreventsLaterReimport) {
    config->GetProperty("General", "Old")->SetDefaultInteger(42);
    ASSERT_TRUE(config->Save(configPath.c_str()));
    Config loaded(&mockMod->Registration);
    ASSERT_TRUE(loaded.Load(configPath.c_str()));
    ASSERT_TRUE(loaded.RemoveProperty("General", "Old"));
    EXPECT_FALSE(loaded.HasKey("General", "Old"));
    Property *replacement = loaded.GetProperty("General", "Old");
    replacement->SetDefaultInteger(1);
    EXPECT_EQ(replacement->GetInteger(), 1);
    ASSERT_TRUE(loaded.RemoveProperty("General", "Old"));
    EXPECT_FALSE(loaded.RemoveProperty("General", "Old"));
    EXPECT_EQ(loaded.GetCategory("General")->GetPropertyCount(), 0u);
}

TEST_F(ConfigTest, Loaded013FontMigrationDoesNotDeclareObsoleteSettings) {
    config->GetProperty("GUI", "FontFilename")->SetDefaultString("primary.ttf");
    config->GetProperty("GUI", "FontSize")->SetDefaultFloat(42.0f);
    config->GetProperty("GUI", "EnableSecondaryFont")->SetDefaultBoolean(true);
    config->GetProperty("GUI", "SecondaryFontFilename")->SetDefaultString("secondary.ttf");
    config->GetProperty("GUI", "SecondaryFontSize")->SetDefaultFloat(26.0f);
    ASSERT_TRUE(config->Save(configPath.c_str()));

    Config updated(&mockMod->Registration);
    ASSERT_TRUE(updated.Load(configPath.c_str()));
    ASSERT_TRUE(MigrateBMLConfig(updated.GetLoadedValues()));
    EXPECT_EQ(updated.GetCategoryCount(), 0u);
    EXPECT_FALSE(updated.HasKey("GUI", "SecondaryFontFilename"));
    Property *primary = updated.GetProperty("GUI", "FontFilename");
    primary->SetDefaultString("unifont.otf");
    Property *fallbacks = updated.GetProperty("GUI", "FontFallbacks");
    fallbacks->SetDefaultString("");
    Property *size = updated.GetProperty("GUI", "FontFallbackSize");
    size->SetDefaultFloat(32.0f);
    EXPECT_STREQ(primary->GetString(), "primary.ttf");
    EXPECT_STREQ(fallbacks->GetString(), "secondary.ttf");
    EXPECT_FLOAT_EQ(size->GetFloat(), 26.0f);
    EXPECT_EQ(updated.GetCategory("GUI")->GetPropertyCount(), 3u);
    EXPECT_TRUE(updated.TakePendingNotifications().empty());
    ASSERT_TRUE(updated.Save(configPath.c_str()));

    Config reloaded(&mockMod->Registration);
    ASSERT_TRUE(reloaded.Load(configPath.c_str()));
    EXPECT_FALSE(reloaded.HasKey("GUI", "EnableSecondaryFont"));
    EXPECT_FALSE(reloaded.HasKey("GUI", "SecondaryFontFilename"));
    EXPECT_FALSE(reloaded.HasKey("GUI", "SecondaryFontSize"));
    EXPECT_STREQ(reloaded.GetProperty("GUI", "FontFallbacks")->GetString(), "secondary.ttf");
}

// Basic creation and destruction
TEST_F(ConfigTest, ConstructionDestruction) {
    ASSERT_NE(nullptr, config);
    EXPECT_EQ(&mockMod->Registration, config->GetMod());

    // Test with null mod
    Config *nullModConfig = new Config(nullptr);
    EXPECT_EQ(nullptr, nullModConfig->GetMod());
    delete nullModConfig;
}

TEST(ConfigStoreTest, FindRejectsDifferentOwnerWithSameModId) {
    MockMod registered(nullptr);
    MockMod duplicate(nullptr);
    ConfigStore store;

    auto config = std::make_unique<Config>(&registered.Registration);
    Config *registeredConfig = config.get();
    ASSERT_EQ(store.Add("MockMod", &registered.Registration, std::move(config)), registeredConfig);

    EXPECT_EQ(store.Find("MockMod", &registered.Registration), registeredConfig);
    EXPECT_EQ(store.Find("MockMod", &duplicate.Registration), nullptr);
}

TEST(ConfigStoreTest, OwnerLookupUsesRegistrationIdentityAndIsRemovedWithTheConfig) {
    MockMod owner(nullptr);
    ConfigStore store;
    auto config = std::make_unique<Config>(&owner.Registration);
    auto *raw = config.get();
    ASSERT_EQ(store.Add(std::string(160, 'x'), &owner.Registration, std::move(config)), raw);
    owner.idReadCount = 0;
    for (int i = 0; i < 1000; ++i)
        EXPECT_EQ(store.Find(&owner.Registration), raw);
    EXPECT_EQ(owner.idReadCount, 0);
    auto removed = store.Remove(std::string(160, 'x'), &owner.Registration, raw);
    EXPECT_EQ(removed.get(), raw);
    EXPECT_EQ(store.Find(&owner.Registration), nullptr);
}

TEST(ConfigStoreTest, RemovalMaintainsOwnerIdentityAndRemainingIndices) {
    MockMod first(nullptr);
    MockMod second(nullptr);
    ConfigStore store;

    auto firstConfig = std::make_unique<Config>(&first.Registration);
    auto secondConfig = std::make_unique<Config>(&second.Registration);
    Config *firstRaw = firstConfig.get();
    Config *secondRaw = secondConfig.get();
    ASSERT_EQ(store.Add("first", &first.Registration, std::move(firstConfig)), firstRaw);
    ASSERT_EQ(store.Add("second", &second.Registration, std::move(secondConfig)), secondRaw);

    EXPECT_EQ(store.Remove("first", &second.Registration, firstRaw), nullptr);
    EXPECT_EQ(store.Find("first", &first.Registration), firstRaw);

    std::unique_ptr<Config> removed = store.Remove("first", &first.Registration, firstRaw);
    ASSERT_EQ(removed.get(), firstRaw);
    EXPECT_EQ(store.Find("first", &first.Registration), nullptr);
    EXPECT_EQ(store.Find("second", &second.Registration), secondRaw);
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
    Property *prop = config->GetProperty("TestCategory", "TestKey");
    ASSERT_NE(nullptr, prop);

    // Now HasKey should return true
    EXPECT_TRUE(config->HasKey("TestCategory", "TestKey"));

    // Get existing property should return same instance
    Property *prop2 = config->GetProperty("TestCategory", "TestKey");
    EXPECT_EQ(prop, prop2);

    // Test null parameters for GetProperty
    EXPECT_EQ(nullptr, config->GetProperty(nullptr, "TestKey"));
    EXPECT_EQ(nullptr, config->GetProperty("TestCategory", nullptr));
    EXPECT_EQ(nullptr, config->GetProperty(nullptr, nullptr));
}

TEST_F(ConfigTest, RemovingPropertyClearsPendingNotificationAndInvalidatesSchema) {
    Property *retained = config->GetProperty("TestCategory", "Retained");
    Property *obsolete = config->GetProperty("TestCategory", "Obsolete");
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

    ReloadConfig();
    MigrateBMLConfig(config->GetLoadedValues());

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

    ASSERT_TRUE(config->Save(configPath.c_str()));
    Config reloaded(&mockMod->Registration);
    ASSERT_TRUE(reloaded.Load(configPath.c_str()));

    EXPECT_FALSE(reloaded.HasKey("GUI", "SecondaryFontFilename"));
    EXPECT_STREQ(reloaded.GetProperty("GUI", "FontFallbacks")->GetString(), "secondary.ttf");
    EXPECT_FLOAT_EQ(reloaded.GetProperty("GUI", "FontFallbackSize")->GetFloat(), 26.0f);
    const std::uint64_t revision = reloaded.GetSchemaRevision();
    MigrateBMLConfig(reloaded.GetLoadedValues());
    EXPECT_EQ(reloaded.GetSchemaRevision(), revision);
    EXPECT_FALSE(reloaded.IsDirty());
}

TEST_F(ConfigTest, CurrentFontSettingsTakePrecedenceOver013Settings) {
    config->GetProperty("GUI", "FontFallbacks")->SetString("");
    config->GetProperty("GUI", "FontFallbackSize")->SetFloat(20.0f);
    config->GetProperty("GUI", "EnableSecondaryFont")->SetBoolean(true);
    config->GetProperty("GUI", "SecondaryFontFilename")->SetString("obsolete.ttf");
    config->GetProperty("GUI", "SecondaryFontSize")->SetFloat(45.0f);

    ReloadConfig();
    MigrateBMLConfig(config->GetLoadedValues());

    EXPECT_STREQ(config->GetProperty("GUI", "FontFallbacks")->GetString(), "");
    EXPECT_FLOAT_EQ(config->GetProperty("GUI", "FontFallbackSize")->GetFloat(), 20.0f);
    EXPECT_FALSE(config->HasKey("GUI", "SecondaryFontFilename"));
}

TEST_F(ConfigTest, Disabled013SecondaryFontLeavesFallbackListEmpty) {
    config->GetProperty("GUI", "FontSize")->SetFloat(40.0f);
    config->GetProperty("GUI", "EnableSecondaryFont")->SetBoolean(false);
    config->GetProperty("GUI", "SecondaryFontFilename")->SetString("secondary.ttf");

    ReloadConfig();
    MigrateBMLConfig(config->GetLoadedValues());

    EXPECT_FALSE(config->HasKey("GUI", "FontFallbacks"));
    EXPECT_FLOAT_EQ(config->GetProperty("GUI", "FontFallbackSize")->GetFloat(), 40.0f);
}

TEST_F(ConfigTest, CurrentConfigDoesNotInheritPrimarySizeForFallbacks) {
    config->GetProperty("GUI", "FontSize")->SetFloat(40.0f);

    ReloadConfig();
    MigrateBMLConfig(config->GetLoadedValues());

    EXPECT_FALSE(config->HasKey("GUI", "FontFallbackSize"));
}

TEST_F(ConfigTest, OldNonPositiveFontSizesRetainThe013Default) {
    config->GetProperty("GUI", "FontRanges")->SetString("ChineseFull");
    config->GetProperty("GUI", "FontSize")->SetFloat(0.0f);
    config->GetProperty("GUI", "SecondaryFontSize")->SetFloat(-1.0f);

    ReloadConfig();
    MigrateBMLConfig(config->GetLoadedValues());

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
    ReloadConfig();
    MigrateBMLConfig(config->GetLoadedValues());

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

    ReloadConfig();
    EXPECT_FALSE(MigrateBMLConfig(config->GetLoadedValues()));

    EXPECT_FALSE(config->HasKey("GUI", "FontFallbacks"));
    EXPECT_STREQ(config->GetProperty("GUI", "SecondaryFontFilename")->GetString(), oldPath.c_str());
    EXPECT_FLOAT_EQ(config->GetProperty("GUI", "SecondaryFontSize")->GetFloat(), 24.0f);
    EXPECT_FLOAT_EQ(config->GetProperty("GUI", "FontFallbackSize")->GetFloat(), 24.0f);
    EXPECT_FALSE(MigrateBMLConfig(config->GetLoadedValues()));
}

TEST_F(ConfigTest, Unrepresentable013FallbackNameIsNotDeleted) {
    config->GetProperty("GUI", "EnableSecondaryFont")->SetBoolean(true);
    config->GetProperty("GUI", "SecondaryFontFilename")->SetString("old;face.ttf");

    ReloadConfig();
    EXPECT_FALSE(MigrateBMLConfig(config->GetLoadedValues()));

    EXPECT_FALSE(config->HasKey("GUI", "FontFallbacks"));
    EXPECT_STREQ(config->GetProperty("GUI", "SecondaryFontFilename")->GetString(), "old;face.ttf");
}

// Property types and values
TEST_F(ConfigTest, PropertyValues) {
    // String property
    Property *strProp = config->GetProperty("TestCategory", "StringProp");
    strProp->SetString("Test String");
    EXPECT_EQ(PropertyType::STRING, strProp->GetType());
    EXPECT_STREQ("Test String", strProp->GetString());

    // Test null string
    strProp->SetString(nullptr);
    EXPECT_STREQ("", strProp->GetString());

    // Boolean property
    Property *boolProp = config->GetProperty("TestCategory", "BoolProp");
    boolProp->SetBoolean(true);
    EXPECT_EQ(PropertyType::BOOLEAN, boolProp->GetType());
    EXPECT_TRUE(boolProp->GetBoolean());

    // Integer property
    Property *intProp = config->GetProperty("TestCategory", "IntProp");
    intProp->SetInteger(42);
    EXPECT_EQ(PropertyType::INTEGER, intProp->GetType());
    EXPECT_EQ(42, intProp->GetInteger());

    // Float property
    Property *floatProp = config->GetProperty("TestCategory", "FloatProp");
    floatProp->SetFloat(3.14f);
    EXPECT_EQ(PropertyType::FLOAT, floatProp->GetType());
    EXPECT_FLOAT_EQ(3.14f, floatProp->GetFloat());

    // Key property
    Property *keyProp = config->GetProperty("TestCategory", "KeyProp");
    keyProp->SetKey(static_cast<CKKEYBOARD>(123));
    EXPECT_EQ(PropertyType::KEY, keyProp->GetType());
    EXPECT_EQ(static_cast<CKKEYBOARD>(123), keyProp->GetKey());

    // Test cross-type access returns default values
    EXPECT_STREQ("", intProp->GetString());
    EXPECT_FALSE(strProp->GetBoolean());
    EXPECT_EQ(0, strProp->GetInteger());
    EXPECT_FLOAT_EQ(0.0f, strProp->GetFloat());
    EXPECT_EQ(static_cast<CKKEYBOARD>(0), strProp->GetKey());
}

TEST_F(ConfigTest, PropertyEditorIsSchemaMetadata) {
    Property *property = config->GetProperty("Appearance", "Accent");
    property->SetDefaultString("#61AFEF");
    EXPECT_EQ(BML_CONFIG_EDITOR_DEFAULT, BML::ConfigMetadata::GetEditor(nullptr));
    EXPECT_EQ(0, BML::ConfigMetadata::SetEditor(nullptr, BML_CONFIG_EDITOR_COLOR));
    EXPECT_EQ(BML_CONFIG_EDITOR_DEFAULT, BML::ConfigMetadata::GetEditor(property));

    const std::uint64_t schemaBeforeEditor = config->GetSchemaRevision();
    const std::uint64_t valueBeforeEditor = config->GetValueRevision();
    EXPECT_EQ(1, BML::ConfigMetadata::SetEditor(property, BML_CONFIG_EDITOR_COLOR));
    EXPECT_EQ(BML_CONFIG_EDITOR_COLOR, BML::ConfigMetadata::GetEditor(property));
    EXPECT_GT(config->GetSchemaRevision(), schemaBeforeEditor);
    EXPECT_EQ(valueBeforeEditor, config->GetValueRevision());
    EXPECT_STREQ("#61AFEF", property->GetString());

    EXPECT_EQ(1, BML::ConfigMetadata::SetEditor(property, BML_CONFIG_EDITOR_CHOICE));
    EXPECT_EQ(BML_CONFIG_EDITOR_CHOICE, BML::ConfigMetadata::GetEditor(property));

    EXPECT_EQ(0, BML::ConfigMetadata::SetEditor(
                     property, static_cast<BML_ConfigPropertyEditor>(99)));
    EXPECT_EQ(BML_CONFIG_EDITOR_CHOICE, BML::ConfigMetadata::GetEditor(property));

    const std::uint64_t stableSchema = config->GetSchemaRevision();
    EXPECT_EQ(1, BML::ConfigMetadata::SetEditor(property, BML_CONFIG_EDITOR_CHOICE));
    EXPECT_EQ(stableSchema, config->GetSchemaRevision());
}

TEST_F(ConfigTest, PropertyChoicesAreSchemaMetadata) {
    Property *property = config->GetProperty("Appearance", "Font");
    property->SetDefaultString("unifont.otf");
    EXPECT_EQ(0U, BML::ConfigMetadata::ChoiceCount(nullptr));
    EXPECT_EQ(nullptr, BML::ConfigMetadata::Choice(nullptr, 0));
    EXPECT_EQ(0, BML::ConfigMetadata::SetChoices(property, nullptr, 1));
    const char *nullChoice[] = {nullptr};
    EXPECT_EQ(0, BML::ConfigMetadata::SetChoices(property, nullChoice, 1));

    const char *choices[] = {"", "unifont.otf", "symbols.ttf"};
    const std::uint64_t schemaBeforeChoices = config->GetSchemaRevision();
    const std::uint64_t valueBeforeChoices = config->GetValueRevision();
    EXPECT_EQ(1, BML::ConfigMetadata::SetChoices(property, choices, 3));
    EXPECT_GT(config->GetSchemaRevision(), schemaBeforeChoices);
    EXPECT_EQ(valueBeforeChoices, config->GetValueRevision());
    ASSERT_EQ(3U, BML::ConfigMetadata::ChoiceCount(property));
    EXPECT_STREQ("", BML::ConfigMetadata::Choice(property, 0));
    EXPECT_STREQ("unifont.otf", BML::ConfigMetadata::Choice(property, 1));
    EXPECT_STREQ("symbols.ttf", BML::ConfigMetadata::Choice(property, 2));
    EXPECT_EQ(nullptr, BML::ConfigMetadata::Choice(property, 3));
    EXPECT_STREQ("unifont.otf", property->GetString());

    const std::uint64_t stableSchema = config->GetSchemaRevision();
    EXPECT_EQ(1, BML::ConfigMetadata::SetChoices(property, choices, 3));
    EXPECT_EQ(stableSchema, config->GetSchemaRevision());

    const char *duplicates[] = {"same", "same"};
    EXPECT_EQ(0, BML::ConfigMetadata::SetChoices(property, duplicates, 2));
    EXPECT_EQ(3U, BML::ConfigMetadata::ChoiceCount(property));

    EXPECT_EQ(1, BML::ConfigMetadata::SetChoices(property, nullptr, 0));
    EXPECT_EQ(0U, BML::ConfigMetadata::ChoiceCount(property));
}

TEST_F(ConfigTest, VariantValueKeepsIntegerAndKeySemanticsDistinct) {
    auto *intProp = static_cast<Property *>(config->GetProperty("TestCategory", "IntProp"));
    intProp->SetDefaultInteger(10);
    EXPECT_EQ(Property::Value{10}, intProp->GetValue());

    intProp->SetValue(Property::Value{20});
    EXPECT_EQ(PropertyType::INTEGER, intProp->GetType());
    EXPECT_EQ(20, intProp->GetInteger());

    auto *keyProp = static_cast<Property *>(config->GetProperty("TestCategory", "KeyProp"));
    keyProp->SetDefaultKey(static_cast<CKKEYBOARD>(30));
    EXPECT_EQ(Property::Value{30}, keyProp->GetValue());

    keyProp->SetValue(Property::Value{40});
    EXPECT_EQ(PropertyType::KEY, keyProp->GetType());
    EXPECT_EQ(static_cast<CKKEYBOARD>(40), keyProp->GetKey());
}

// Default values
TEST_F(ConfigTest, DefaultValues) {
    // String default
    Property *strProp = config->GetProperty("DefaultCategory", "StringProp");
    strProp->SetDefaultString("Default String");
    EXPECT_EQ(PropertyType::STRING, strProp->GetType());
    EXPECT_STREQ("Default String", strProp->GetString());

    // Test null default string
    Property *nullStrProp = config->GetProperty("DefaultCategory", "NullStringProp");
    nullStrProp->SetDefaultString(nullptr);
    EXPECT_EQ(PropertyType::STRING, nullStrProp->GetType());
    EXPECT_STREQ("", nullStrProp->GetString());

    // Boolean default
    Property *boolProp = config->GetProperty("DefaultCategory", "BoolProp");
    boolProp->SetDefaultBoolean(true);
    EXPECT_EQ(PropertyType::BOOLEAN, boolProp->GetType());
    EXPECT_TRUE(boolProp->GetBoolean());

    // Integer default
    Property *intProp = config->GetProperty("DefaultCategory", "IntProp");
    intProp->SetDefaultInteger(42);
    EXPECT_EQ(PropertyType::INTEGER, intProp->GetType());
    EXPECT_EQ(42, intProp->GetInteger());

    // Float default
    Property *floatProp = config->GetProperty("DefaultCategory", "FloatProp");
    floatProp->SetDefaultFloat(3.14f);
    EXPECT_EQ(PropertyType::FLOAT, floatProp->GetType());
    EXPECT_FLOAT_EQ(3.14f, floatProp->GetFloat());

    // Key default
    Property *keyProp = config->GetProperty("DefaultCategory", "KeyProp");
    keyProp->SetDefaultKey(static_cast<CKKEYBOARD>(123));
    EXPECT_EQ(PropertyType::KEY, keyProp->GetType());
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
        {"Batch", "Second", PropertyType::INTEGER, 2, 20},
        {"Batch", "First", PropertyType::INTEGER, 1, 10},
    };

    const Config::ApplyResult result =
        config->ApplyEdits(&mockMod->Registration, schemaRevision, edits);
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
        {"Batch", "Second", PropertyType::INTEGER, 21, 22},
    };
    const Config::ApplyResult coalescedResult =
        config->ApplyEdits(&mockMod->Registration, schemaRevision, coalesced);
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
        {"Batch", "Value", PropertyType::INTEGER, 1, 2},
    };

    MockMod otherOwner(nullptr);
    Config::ApplyResult result =
        config->ApplyEdits(&otherOwner.Registration, schemaRevision, edits);
    EXPECT_EQ(Config::ApplyError::OwnerChanged, result.Error);
    EXPECT_EQ(1, property->GetInteger());

    config->GetProperty("Batch", "AddedAfterSnapshot")->SetDefaultBoolean(false);
    result = config->ApplyEdits(&mockMod->Registration, schemaRevision, edits);
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

    const Config::ApplyResult result =
        config->ApplyEdits(&mockMod->Registration, schemaRevision,
                           {
                               {"Batch", "Edited", PropertyType::INTEGER, 1, 10},
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

    Config::ApplyResult result = config->ApplyEdits(
        &mockMod->Registration, schemaRevision,
        {
            {"Batch", "Value", PropertyType::STRING, std::string("1"), std::string("2")},
        });
    EXPECT_EQ(Config::ApplyError::TypeChanged, result.Error);

    result = config->ApplyEdits(&mockMod->Registration, schemaRevision,
                                {
                                    {"Batch", "Value", PropertyType::INTEGER, 0, 2},
                                });
    EXPECT_EQ(Config::ApplyError::BaseChanged, result.Error);

    result = config->ApplyEdits(&mockMod->Registration, schemaRevision,
                                {
                                    {"Batch", "Value", PropertyType::INTEGER, 1, 2},
                                    {"Batch", "Value", PropertyType::INTEGER, 1, 3},
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

    const Config::ApplyResult result =
        config->ApplyEdits(&mockMod->Registration, schemaRevision,
                           {
                               {"Batch", "First", PropertyType::INTEGER, 1, 10},
                               {"Batch", "Second", PropertyType::INTEGER, 99, 20},
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
    Property *prop = config->GetProperty("TestCategory", "TestProp");

    // Set initial value
    prop->SetString("Initial");
    EXPECT_EQ(0, mockMod->modifiedCount);
    EXPECT_TRUE(config->IsDirty());

    auto notifications = config->TakePendingNotifications();
    ASSERT_EQ(1u, notifications.size());
    mockMod->Registration.Notify<ModEvent::OnModifyConfig>(notifications[0].Category.c_str(),
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
    mockMod->Registration.Notify<ModEvent::OnModifyConfig>(notifications[0].Category.c_str(),
                                                           notifications[0].Key.c_str(),
                                                           notifications[0].ChangedProperty);
    EXPECT_EQ(2, mockMod->modifiedCount);
    EXPECT_STREQ("Changed Again", prop->GetString());

    // Test with null mod (shouldn't crash)
    Config *nullModConfig = new Config(nullptr);
    Property *nullModProp = nullModConfig->GetProperty("TestCategory", "TestProp");
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
    EXPECT_EQ(PropertyType::STRING, destProp->GetType());
    EXPECT_STREQ("Test String", destProp->GetString());

    // Test boolean copy
    srcProp->SetBoolean(true);
    destProp->CopyValue(srcProp);
    EXPECT_EQ(PropertyType::BOOLEAN, destProp->GetType());
    EXPECT_TRUE(destProp->GetBoolean());

    // Test null copy (should not crash)
    destProp->CopyValue(nullptr);
    EXPECT_EQ(PropertyType::BOOLEAN, destProp->GetType()); // Should remain unchanged
}

// File I/O
TEST_F(ConfigTest, FileIO) {
    // Create test config file
    const wchar_t *filename = configPath.c_str();

    // Set up some properties
    Property *strProp = config->GetProperty("TestCategory", "StringProp");
    strProp->SetString("Test String");
    strProp->SetComment("String Property Comment");

    Property *boolProp = config->GetProperty("TestCategory", "BoolProp");
    boolProp->SetBoolean(true);

    config->SetCategoryComment("TestCategory", "Test Category Comment");

    // Save the config
    EXPECT_TRUE(config->Save(filename));

    // Create a new config and load the file
    MockMod *newMockMod = new MockMod(nullptr);
    Config *newConfig = new Config(&newMockMod->Registration);
    EXPECT_TRUE(newConfig->Load(filename));

    // Check that properties were loaded correctly
    EXPECT_TRUE(newConfig->HasCategory("TestCategory"));
    EXPECT_TRUE(newConfig->HasKey("TestCategory", "StringProp"));
    EXPECT_TRUE(newConfig->HasKey("TestCategory", "BoolProp"));

    Property *loadedStrProp = newConfig->GetProperty("TestCategory", "StringProp");
    EXPECT_EQ(PropertyType::STRING, loadedStrProp->GetType());
    EXPECT_STREQ("Test String", loadedStrProp->GetString());
    EXPECT_STREQ("", static_cast<Property*>(loadedStrProp)->GetComment());

    Property *loadedBoolProp = newConfig->GetProperty("TestCategory", "BoolProp");
    EXPECT_EQ(PropertyType::BOOLEAN, loadedBoolProp->GetType());
    EXPECT_TRUE(loadedBoolProp->GetBoolean());

    EXPECT_STREQ("", newConfig->GetCategoryComment("TestCategory"));

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
    const wchar_t *filename = configPath.c_str();
    {
        std::ofstream output(std::filesystem::path(configPath), std::ios::binary);
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
    ASSERT_EQ(config->GetCategoryCount(), 0U);
    Property *declared = config->GetProperty("General", "Count");
    declared->SetDefaultInteger(0);
    ASSERT_EQ(config->GetCategoryCount(), 1U);
    Category *category = config->GetCategory(static_cast<std::size_t>(0));
    ASSERT_NE(category, nullptr);
    EXPECT_STREQ(category->GetComment(), "");
    ASSERT_EQ(category->GetPropertyCount(), 1U);
    Property *property = category->GetProperty(static_cast<std::size_t>(0));
    ASSERT_NE(property, nullptr);
    EXPECT_EQ(property, category->GetProperty("Count"));
    EXPECT_EQ(property->GetInteger(), 2);
    EXPECT_STREQ(property->GetComment(), "");
    EXPECT_TRUE(config->TakePendingNotifications().empty());
    EXPECT_FALSE(config->IsDirty());

    utils::DeleteFileW(filename);
}

TEST_F(ConfigTest, SaveUsesSnapshottedModMetadata) {
    const wchar_t *filename = configPath.c_str();
    mockMod->nameReadCount = 0;
    mockMod->versionReadCount = 0;
    config->SnapshotModMetadata();
    EXPECT_EQ(0, mockMod->nameReadCount);
    EXPECT_EQ(0, mockMod->versionReadCount);

    mockMod->idReadCount = 0;
    mockMod->nameReadCount = 0;
    mockMod->versionReadCount = 0;

    ASSERT_TRUE(config->Save(filename));
    EXPECT_EQ(0, mockMod->idReadCount);
    EXPECT_EQ(0, mockMod->nameReadCount);
    EXPECT_EQ(0, mockMod->versionReadCount);

    utils::DeleteFileW(filename);
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
            Property *prop = config->GetProperty(category.c_str(), key.c_str());
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
        Property *prop = config->GetProperty(category.c_str(), key.c_str());
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

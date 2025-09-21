#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "IniFile.h"
#include "StringUtils.h"
#include "PathUtils.h"
#include <filesystem>
#include <fstream>

using namespace testing;

class IniFileTest : public Test {
protected:
    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path() / "IniFileTest";
        std::filesystem::create_directories(tempDir);
    }

    void TearDown() override {
        if (std::filesystem::exists(tempDir)) {
            std::filesystem::remove_all(tempDir);
        }
    }

    std::filesystem::path tempDir;

    void CreateTestFile(const std::string& filename, const std::string& content) {
        auto filePath = tempDir / filename;
        std::wstring wFilePath = filePath.wstring();
        std::wstring wContent = utils::Utf8ToUtf16(content);
        utils::WriteTextFileW(wFilePath, wContent);
    }

    std::wstring GetTestFilePath(const std::string& filename) {
        return (tempDir / filename).wstring();
    }
};

// Construction and basic operations
TEST_F(IniFileTest, ConstructorInitializesCorrectly) {
    IniFile ini;
    
    EXPECT_TRUE(ini.IsEmpty());
    EXPECT_EQ(0, ini.GetSectionCount());
    EXPECT_FALSE(ini.IsCaseSensitive());
    EXPECT_TRUE(ini.IsStrictUtf8Validation());
    EXPECT_TRUE(ini.GetLastError().empty());
}

TEST_F(IniFileTest, ClearResetsState) {
    IniFile ini;
    ini.ParseFromString("[section]\nkey=value");
    
    EXPECT_FALSE(ini.IsEmpty());
    
    ini.Clear();
    
    EXPECT_TRUE(ini.IsEmpty());
    EXPECT_EQ(0, ini.GetSectionCount());
}

// UTF-8 validation tests
TEST_F(IniFileTest, UTF8ValidationWorks) {
    IniFile ini;
    
    // Valid UTF-8
    EXPECT_TRUE(ini.IsValidUtf8("Hello World"));
    EXPECT_TRUE(ini.IsValidUtf8("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"));  // "日本語" in UTF-8
    EXPECT_TRUE(ini.IsValidUtf8("\xf0\x9f\x8c\x9f"));  // "🌟" in UTF-8
    EXPECT_TRUE(ini.IsValidUtf8(""));
    
    // Invalid UTF-8 (malformed sequences)
    EXPECT_FALSE(ini.IsValidUtf8("\xFF\xFE"));
    EXPECT_FALSE(ini.IsValidUtf8("\x80"));
}

TEST_F(IniFileTest, UTF8LengthCalculation) {
    IniFile ini;
    
    EXPECT_EQ(0, ini.GetUtf8Length(""));
    EXPECT_EQ(5, ini.GetUtf8Length("Hello"));
    EXPECT_EQ(3, ini.GetUtf8Length("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"));  // "日本語" in UTF-8
    EXPECT_EQ(1, ini.GetUtf8Length("\xf0\x9f\x8c\x9f"));  // "🌟" in UTF-8
    
    // Invalid UTF-8 should return 0
    EXPECT_EQ(0, ini.GetUtf8Length("\xFF\xFE"));
}

// String parsing tests
TEST_F(IniFileTest, ParseEmptyString) {
    IniFile ini;
    
    EXPECT_TRUE(ini.ParseFromString(""));
    EXPECT_TRUE(ini.IsEmpty());
}

TEST_F(IniFileTest, ParseSimpleKeyValue) {
    IniFile ini;
    
    EXPECT_TRUE(ini.ParseFromString("[section]\nkey=value"));
    EXPECT_FALSE(ini.IsEmpty());
    EXPECT_EQ(1, ini.GetSectionCount());
    EXPECT_TRUE(ini.HasSection("section"));
    EXPECT_TRUE(ini.HasKey("section", "key"));
    EXPECT_EQ("value", ini.GetValue("section", "key"));
}

TEST_F(IniFileTest, ParseMultipleSectionsAndKeys) {
    IniFile ini;
    std::string content = 
"# Leading comment\n"
"[section1]\n"
"key1=value1\n"
"key2=value2\n"
"\n"
"[section2]\n"
"key3=value3\n"
"# Comment in section\n"
"key4=value4\n";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    EXPECT_EQ(2, ini.GetSectionCount());
    
    EXPECT_TRUE(ini.HasSection("section1"));
    EXPECT_TRUE(ini.HasSection("section2"));
    
    EXPECT_EQ("value1", ini.GetValue("section1", "key1"));
    EXPECT_EQ("value2", ini.GetValue("section1", "key2"));
    EXPECT_EQ("value3", ini.GetValue("section2", "key3"));
    EXPECT_EQ("value4", ini.GetValue("section2", "key4"));
}

TEST_F(IniFileTest, ParseWithComments) {
    IniFile ini;
    std::string content = 
"# This is a leading comment\n"
"; This is another comment style\n"
"[section]\n"
"key1=value1  # Inline comment (now properly parsed)\n"
"; Comment line in section\n"
"key2=value2\n";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    EXPECT_TRUE(ini.HasSection("section"));
    EXPECT_EQ("value1", ini.GetValue("section", "key1"));  // Value should be separated from comment
    EXPECT_EQ("# Inline comment (now properly parsed)", ini.GetInlineComment("section", "key1"));
    EXPECT_EQ("value2", ini.GetValue("section", "key2"));
}

TEST_F(IniFileTest, ParseWithWhitespace) {
    IniFile ini;
    std::string content = 
"  [  section  ]  \n"
"  key1  =  value1  \n"
"key2=  value with spaces  \n"
"  key3=value3\n";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    EXPECT_TRUE(ini.HasSection("section"));
    EXPECT_EQ("value1", ini.GetValue("section", "key1"));
    EXPECT_EQ("value with spaces", ini.GetValue("section", "key2"));
    EXPECT_EQ("value3", ini.GetValue("section", "key3"));
}

TEST_F(IniFileTest, ParseGlobalSection) {
    IniFile ini;
    std::string content = 
"global_key=global_value\n"
"[section]\n"
"section_key=section_value\n";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    EXPECT_TRUE(ini.HasSection(""));  // Global section
    EXPECT_TRUE(ini.HasSection("section"));
    EXPECT_EQ("global_value", ini.GetValue("", "global_key"));
    EXPECT_EQ("section_value", ini.GetValue("section", "section_key"));
}

TEST_F(IniFileTest, ParseUTF8Content) {
    IniFile ini;
    std::string content = 
"[\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe3\x82\xbb\xe3\x82\xaf\xe3\x82\xb7\xe3\x83\xa7\xe3\x83\xb3]\n"  // "日本語セクション" in UTF-8
"\xe5\x90\x8d\xe5\x89\x8d=\xe5\x80\xa4\n"  // "名前=値" in UTF-8
"emoji=\xf0\x9f\x8c\x9f\xe2\xad\x90\xf0\x9f\x8e\x89\n"  // "🌟⭐🎉" in UTF-8
"chinese=\xe4\xb8\xad\xe6\x96\x87\xe6\xb5\x8b\xe8\xaf\x95\n";  // "中文测试" in UTF-8
    
    EXPECT_TRUE(ini.ParseFromString(content));
    EXPECT_TRUE(ini.HasSection("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe3\x82\xbb\xe3\x82\xaf\xe3\x82\xb7\xe3\x83\xa7\xe3\x83\xb3"));  // "日本語セクション"
    EXPECT_EQ("\xe5\x80\xa4", ini.GetValue("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe3\x82\xbb\xe3\x82\xaf\xe3\x82\xb7\xe3\x83\xa7\xe3\x83\xb3", "\xe5\x90\x8d\xe5\x89\x8d"));  // "値" and "名前"
    EXPECT_EQ("\xf0\x9f\x8c\x9f\xe2\xad\x90\xf0\x9f\x8e\x89", ini.GetValue("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe3\x82\xbb\xe3\x82\xaf\xe3\x82\xb7\xe3\x83\xa7\xe3\x83\xb3", "emoji"));  // "🌟⭐🎉"
    EXPECT_EQ("\xe4\xb8\xad\xe6\x96\x87\xe6\xb5\x8b\xe8\xaf\x95", ini.GetValue("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe3\x82\xbb\xe3\x82\xaf\xe3\x82\xb7\xe3\x83\xa7\xe3\x83\xb3", "chinese"));  // "中文测试"
}

TEST_F(IniFileTest, ParseInvalidUTF8WithStrictMode) {
    IniFile ini;
    ini.SetStrictUtf8Validation(true);
    
    // Invalid UTF-8 sequence
    std::string invalidContent = "key=value\xFF\xFE";
    
    EXPECT_FALSE(ini.ParseFromString(invalidContent));
    EXPECT_FALSE(ini.GetLastError().empty());
}

TEST_F(IniFileTest, ParseInvalidUTF8WithoutStrictMode) {
    IniFile ini;
    ini.SetStrictUtf8Validation(false);
    
    // Invalid UTF-8 sequence - should not fail in non-strict mode
    std::string invalidContent = "key=value";
    
    EXPECT_TRUE(ini.ParseFromString(invalidContent));
}

// Case sensitivity tests
TEST_F(IniFileTest, CaseSensitivityDefault) {
    IniFile ini;
    ini.ParseFromString("[Section]\nKey=Value");
    
    // Default is case-insensitive
    EXPECT_TRUE(ini.HasSection("section"));
    EXPECT_TRUE(ini.HasSection("SECTION"));
    EXPECT_TRUE(ini.HasKey("section", "key"));
    EXPECT_TRUE(ini.HasKey("SECTION", "KEY"));
    EXPECT_EQ("Value", ini.GetValue("section", "key"));
}

TEST_F(IniFileTest, CaseSensitivityEnabled) {
    IniFile ini;
    ini.SetCaseSensitive(true);
    ini.ParseFromString("[Section]\nKey=Value");
    
    EXPECT_TRUE(ini.HasSection("Section"));
    EXPECT_FALSE(ini.HasSection("section"));
    EXPECT_TRUE(ini.HasKey("Section", "Key"));
    EXPECT_FALSE(ini.HasKey("Section", "key"));
    EXPECT_EQ("Value", ini.GetValue("Section", "Key"));
    EXPECT_EQ("", ini.GetValue("Section", "key"));
}

// File I/O tests
TEST_F(IniFileTest, ParseFromFile) {
    std::string content = "[section]\nkey=value";
    CreateTestFile("test.ini", content);
    
    IniFile ini;
    EXPECT_TRUE(ini.ParseFromFile(GetTestFilePath("test.ini")));
    EXPECT_TRUE(ini.HasSection("section"));
    EXPECT_EQ("value", ini.GetValue("section", "key"));
}

TEST_F(IniFileTest, ParseFromNonexistentFile) {
    IniFile ini;
    EXPECT_FALSE(ini.ParseFromFile(GetTestFilePath("nonexistent.ini")));
    EXPECT_FALSE(ini.GetLastError().empty());
}

TEST_F(IniFileTest, WriteToString) {
    IniFile ini;
    ini.AddSection("section1");
    ini.SetValue("section1", "key1", "value1");
    ini.SetValue("section1", "key2", "value2");
    ini.AddSection("section2");
    ini.SetValue("section2", "key3", "value3");
    
    std::string output = ini.WriteToString();
    
    EXPECT_THAT(output, HasSubstr("[section1]"));
    EXPECT_THAT(output, HasSubstr("key1 = value1"));
    EXPECT_THAT(output, HasSubstr("key2 = value2"));
    EXPECT_THAT(output, HasSubstr("[section2]"));
    EXPECT_THAT(output, HasSubstr("key3 = value3"));
}

TEST_F(IniFileTest, WriteToFile) {
    IniFile ini;
    ini.AddSection("section");
    ini.SetValue("section", "key", "value");
    
    std::wstring filePath = GetTestFilePath("output.ini");
    EXPECT_TRUE(ini.WriteToFile(filePath));
    EXPECT_TRUE(utils::FileExistsW(filePath));
    
    // Verify by reading back
    IniFile ini2;
    EXPECT_TRUE(ini2.ParseFromFile(filePath));
    EXPECT_EQ("value", ini2.GetValue("section", "key"));
}

// Section operations
TEST_F(IniFileTest, AddSection) {
    IniFile ini;
    
    auto section = ini.AddSection("newsection");
    EXPECT_NE(nullptr, section);
    EXPECT_TRUE(ini.HasSection("newsection"));
    EXPECT_EQ("newsection", section->name);
}

TEST_F(IniFileTest, AddDuplicateSection) {
    IniFile ini;
    
    auto section1 = ini.AddSection("section");
    auto section2 = ini.AddSection("section");
    
    EXPECT_EQ(section1, section2);  // Should return existing section
    EXPECT_EQ(1, ini.GetSectionCount());
}

TEST_F(IniFileTest, RemoveSection) {
    IniFile ini;
    ini.AddSection("section1");
    ini.AddSection("section2");
    
    EXPECT_TRUE(ini.RemoveSection("section1"));
    EXPECT_FALSE(ini.HasSection("section1"));
    EXPECT_TRUE(ini.HasSection("section2"));
    
    EXPECT_FALSE(ini.RemoveSection("nonexistent"));
}

TEST_F(IniFileTest, GetSectionNames) {
    IniFile ini;
    ini.AddSection("section1");
    ini.AddSection("section2");
    ini.AddSection("section3");
    
    auto names = ini.GetSectionNames();
    EXPECT_EQ(3, names.size());
    EXPECT_THAT(names, UnorderedElementsAre("section1", "section2", "section3"));
}

// Key-value operations
TEST_F(IniFileTest, SetValue) {
    IniFile ini;
    
    EXPECT_TRUE(ini.SetValue("section", "key", "value"));
    EXPECT_TRUE(ini.HasSection("section"));
    EXPECT_TRUE(ini.HasKey("section", "key"));
    EXPECT_EQ("value", ini.GetValue("section", "key"));
}

TEST_F(IniFileTest, SetValueCreatesSection) {
    IniFile ini;
    
    EXPECT_TRUE(ini.SetValue("newsection", "key", "value"));
    EXPECT_TRUE(ini.HasSection("newsection"));
    EXPECT_EQ("value", ini.GetValue("newsection", "key"));
}

TEST_F(IniFileTest, UpdateExistingValue) {
    IniFile ini;
    ini.SetValue("section", "key", "oldvalue");
    
    EXPECT_TRUE(ini.SetValue("section", "key", "newvalue"));
    EXPECT_EQ("newvalue", ini.GetValue("section", "key"));
}

TEST_F(IniFileTest, GetValueWithDefault) {
    IniFile ini;
    
    EXPECT_EQ("default", ini.GetValue("section", "key", "default"));
    
    ini.SetValue("section", "key", "actual");
    EXPECT_EQ("actual", ini.GetValue("section", "key", "default"));
}

TEST_F(IniFileTest, RemoveKey) {
    IniFile ini;
    ini.SetValue("section", "key1", "value1");
    ini.SetValue("section", "key2", "value2");
    
    EXPECT_TRUE(ini.RemoveKey("section", "key1"));
    EXPECT_FALSE(ini.HasKey("section", "key1"));
    EXPECT_TRUE(ini.HasKey("section", "key2"));
    
    EXPECT_FALSE(ini.RemoveKey("section", "nonexistent"));
    EXPECT_FALSE(ini.RemoveKey("nonexistent", "key"));
}

// Bulk operations (mutations)
TEST_F(IniFileTest, ApplyMutationsSet) {
    IniFile ini;
    ini.SetValue("section", "existing", "old");
    
    std::vector<IniFile::Mutation> mutations = {
        {"existing", "new", false},
        {"new_key", "new_value", false}
    };
    
    EXPECT_TRUE(ini.ApplyMutations("section", mutations));
    EXPECT_EQ("new", ini.GetValue("section", "existing"));
    EXPECT_EQ("new_value", ini.GetValue("section", "new_key"));
}

TEST_F(IniFileTest, ApplyMutationsRemove) {
    IniFile ini;
    ini.SetValue("section", "key1", "value1");
    ini.SetValue("section", "key2", "value2");
    
    std::vector<IniFile::Mutation> mutations = {
        {"key1", "", true}
    };
    
    EXPECT_TRUE(ini.ApplyMutations("section", mutations));
    EXPECT_FALSE(ini.HasKey("section", "key1"));
    EXPECT_TRUE(ini.HasKey("section", "key2"));
}

TEST_F(IniFileTest, ApplyMutationsMixed) {
    IniFile ini;
    ini.SetValue("section", "update_me", "old");
    ini.SetValue("section", "remove_me", "gone");
    
    std::vector<IniFile::Mutation> mutations = {
        {"update_me", "updated", false},
        {"remove_me", "", true},
        {"add_me", "added", false}
    };
    
    EXPECT_TRUE(ini.ApplyMutations("section", mutations));
    EXPECT_EQ("updated", ini.GetValue("section", "update_me"));
    EXPECT_FALSE(ini.HasKey("section", "remove_me"));
    EXPECT_EQ("added", ini.GetValue("section", "add_me"));
}

TEST_F(IniFileTest, ApplyMutationsPreservesInlineComment) {
    IniFile ini;
    std::string content = R"([section]
key = value  # keep comment
)";

    ASSERT_TRUE(ini.ParseFromString(content));

    std::vector<IniFile::Mutation> mutations = {
        {"key", "updated", false}
    };

    ASSERT_TRUE(ini.ApplyMutations("section", mutations));
    EXPECT_EQ("# keep comment", ini.GetInlineComment("section", "key"));

    std::string output = ini.WriteToString();
    EXPECT_THAT(output, HasSubstr("key = updated  # keep comment"));
}

// Custom section insertion logic
TEST_F(IniFileTest, DefaultSectionInsertionOrder) {
    IniFile ini;
    
    // Default logic: theme first, overrides last, others in between
    ini.AddSection("normal");
    ini.AddSection("overrides");
    ini.AddSection("theme");
    ini.AddSection("another");
    
    auto names = ini.GetSectionNames();
    EXPECT_EQ("theme", names[0]);      // theme goes first
    EXPECT_EQ("overrides", names.back()); // overrides goes last
}

TEST_F(IniFileTest, CustomSectionInsertionLogic) {
    IniFile ini;
    
    // Custom logic: alphabetical order
    ini.SetSectionInsertionLogic([](const std::vector<IniFile::Section>& sections, const std::string& sectionName) -> size_t {
        for (size_t i = 0; i < sections.size(); ++i) {
            if (sectionName < sections[i].name) {
                return i;
            }
        }
        return sections.size();
    });
    
    ini.AddSection("zebra");
    ini.AddSection("alpha");
    ini.AddSection("beta");
    
    auto names = ini.GetSectionNames();
    EXPECT_EQ("alpha", names[0]);
    EXPECT_EQ("beta", names[1]);
    EXPECT_EQ("zebra", names[2]);
}

// Error handling and validation
TEST_F(IniFileTest, InvalidSectionName) {
    IniFile ini;
    
    EXPECT_EQ(nullptr, ini.AddSection("[invalid]"));
    EXPECT_FALSE(ini.GetLastError().empty());
    
    EXPECT_EQ(nullptr, ini.AddSection("contains\nnewline"));
    EXPECT_FALSE(ini.GetLastError().empty());
}

TEST_F(IniFileTest, InvalidKeyName) {
    IniFile ini;
    
    EXPECT_FALSE(ini.SetValue("section", "", "value"));  // Empty key
    EXPECT_FALSE(ini.SetValue("section", "key=invalid", "value"));  // Contains =
    EXPECT_FALSE(ini.SetValue("section", "key\nwith\nnewlines", "value"));  // Contains newlines
}

TEST_F(IniFileTest, MaxLimitsValidation) {
    IniFile ini;
    
    // Test with very long key (exceeds MAX_KEY_CODEPOINTS)
    std::string longKey(1000, 'a');  // Much longer than MAX_KEY_CODEPOINTS (255)
    EXPECT_FALSE(ini.SetValue("section", longKey, "value"));
    EXPECT_FALSE(ini.GetLastError().empty());
}

TEST_F(IniFileTest, InvalidUTF8InStrictMode) {
    IniFile ini;
    ini.SetStrictUtf8Validation(true);
    
    // Try to set value with invalid UTF-8
    std::string invalidValue = "value\xFF\xFE";
    EXPECT_FALSE(ini.SetValue("section", "key", invalidValue));
    EXPECT_FALSE(ini.GetLastError().empty());
}

// Edge cases and malformed content
TEST_F(IniFileTest, MalformedLines) {
    IniFile ini;
    std::string content = 
"[section]\n"
"key_without_value\n"
"=value_without_key\n"
"key=value\n";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    // Malformed lines should be treated as comments
    EXPECT_EQ("value", ini.GetValue("section", "key"));
    EXPECT_FALSE(ini.HasKey("section", "key_without_value"));
}

TEST_F(IniFileTest, EmptyValues) {
    IniFile ini;
    ini.SetValue("section", "empty_value", "");
    
    EXPECT_EQ("", ini.GetValue("section", "empty_value"));
    EXPECT_TRUE(ini.HasKey("section", "empty_value"));
}

TEST_F(IniFileTest, PreserveFormatting) {
    IniFile ini;
    std::string content = 
"# Leading comment\n"
"[section]\n"
"key1=value1\n"
"# Comment in section\n"
"\n"
"key2=value2\n";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    std::string output = ini.WriteToString();
    
    // Should preserve comments and empty lines
    EXPECT_THAT(output, HasSubstr("# Leading comment"));
    EXPECT_THAT(output, HasSubstr("# Comment in section"));
}

// Round-trip tests (parse -> modify -> write -> parse)
TEST_F(IniFileTest, RoundTripPreservation) {
    std::string originalContent = 
"# Top comment\n"
"[section1]\n"
"key1=value1\n"
"# Mid comment\n"
"key2=value2\n"
"\n"
"[section2]\n"
"key3=value3\n";
    
    IniFile ini1;
    EXPECT_TRUE(ini1.ParseFromString(originalContent));
    
    // Modify slightly
    ini1.SetValue("section1", "key3", "value3");
    
    std::string modified = ini1.WriteToString();
    
    // Parse the modified version
    IniFile ini2;
    EXPECT_TRUE(ini2.ParseFromString(modified));
    
    EXPECT_EQ("value1", ini2.GetValue("section1", "key1"));
    EXPECT_EQ("value2", ini2.GetValue("section1", "key2"));
    EXPECT_EQ("value3", ini2.GetValue("section1", "key3"));
    EXPECT_EQ("value3", ini2.GetValue("section2", "key3"));
}

// Performance and stress tests
TEST_F(IniFileTest, ManyKeys) {
    IniFile ini;
    
    // Add many keys to test performance and limits
    for (int i = 0; i < 100; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        EXPECT_TRUE(ini.SetValue("section", key, value));
    }
    
    // Verify all keys exist
    for (int i = 0; i < 100; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string expectedValue = "value" + std::to_string(i);
        EXPECT_EQ(expectedValue, ini.GetValue("section", key));
    }
}

TEST_F(IniFileTest, ManySections) {
    IniFile ini;
    
    // Add many sections
    for (int i = 0; i < 50; ++i) {
        std::string section = "section" + std::to_string(i);
        EXPECT_NE(nullptr, ini.AddSection(section));
    }
    
    EXPECT_EQ(50, ini.GetSectionCount());
}

// Unicode edge cases
TEST_F(IniFileTest, UnicodeWhitespace) {
    IniFile ini;
    std::string content = "[section]\n\xe3\x80\x80key\xe3\x80\x80=\xe3\x80\x80value\xe3\x80\x80";  // Full-width spaces in UTF-8
    
    EXPECT_TRUE(ini.ParseFromString(content));
    EXPECT_TRUE(ini.HasKey("section", "key"));
    EXPECT_EQ("value", ini.GetValue("section", "key"));
}

TEST_F(IniFileTest, MixedLineEndings) {
    IniFile ini;
    std::string content = "[section]\nkey1=value1\r\nkey2=value2\nkey3=value3";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    EXPECT_EQ("value1", ini.GetValue("section", "key1"));
    EXPECT_EQ("value2", ini.GetValue("section", "key2"));
    EXPECT_EQ("value3", ini.GetValue("section", "key3"));
}

// Comment preservation tests
TEST_F(IniFileTest, InlineCommentParsing) {
    IniFile ini;
    std::string content = 
"[section]\n"
"key1=value1  # This is an inline comment\n"
"key2=value2  ; Another comment style\n"
"key3=value3\n";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    EXPECT_EQ("value1", ini.GetValue("section", "key1"));
    EXPECT_EQ("value2", ini.GetValue("section", "key2"));
    EXPECT_EQ("value3", ini.GetValue("section", "key3"));
    
    EXPECT_EQ("# This is an inline comment", ini.GetInlineComment("section", "key1"));
    EXPECT_EQ("; Another comment style", ini.GetInlineComment("section", "key2"));
    EXPECT_EQ("", ini.GetInlineComment("section", "key3"));
}

TEST_F(IniFileTest, CommentPreservationOnValueUpdate) {
    IniFile ini;
    std::string content = 
"[section]\n"
"key1=value1  # Original comment\n"
"key2=value2\n";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    EXPECT_EQ("# Original comment", ini.GetInlineComment("section", "key1"));
    
    // Update value, comment should be preserved
    EXPECT_TRUE(ini.SetValue("section", "key1", "newvalue"));
    EXPECT_EQ("newvalue", ini.GetValue("section", "key1"));
    EXPECT_EQ("# Original comment", ini.GetInlineComment("section", "key1"));
    
    // Check that the written output preserves the comment
    std::string output = ini.WriteToString();
    EXPECT_NE(output.find("key1 = newvalue  # Original comment"), std::string::npos);
}

TEST_F(IniFileTest, CommentManagement) {
    IniFile ini;
    std::string content = 
"[section]\n"
"key1=value1\n"
"key2=value2\n";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    
    // Add inline comments
    EXPECT_TRUE(ini.SetInlineComment("section", "key1", "Added comment"));
    EXPECT_TRUE(ini.SetInlineComment("section", "key2", "# Another comment"));
    
    EXPECT_EQ("# Added comment", ini.GetInlineComment("section", "key1"));
    EXPECT_EQ("# Another comment", ini.GetInlineComment("section", "key2"));
    
    // Test output
    std::string output = ini.WriteToString();
    EXPECT_NE(output.find("key1 = value1  # Added comment"), std::string::npos);
    EXPECT_NE(output.find("key2 = value2  # Another comment"), std::string::npos);
}

TEST_F(IniFileTest, SetValueWithComment) {
    IniFile ini;
    
    EXPECT_TRUE(ini.SetValueWithComment("section", "key1", "value1", "Inline comment"));
    EXPECT_TRUE(ini.SetValueWithComment("section", "key2", "value2"));
    
    EXPECT_EQ("value1", ini.GetValue("section", "key1"));
    EXPECT_EQ("value2", ini.GetValue("section", "key2"));
    EXPECT_EQ("# Inline comment", ini.GetInlineComment("section", "key1"));
    EXPECT_EQ("", ini.GetInlineComment("section", "key2"));
}

TEST_F(IniFileTest, PrecedingCommentManagement) {
    IniFile ini;
    
    EXPECT_TRUE(ini.SetValue("section", "key1", "value1"));
    EXPECT_TRUE(ini.SetPrecedingComment("section", "key1", "; This is a preceding comment"));
    
    EXPECT_EQ("; This is a preceding comment", ini.GetPrecedingComment("section", "key1"));
    
    std::string output = ini.WriteToString();
    EXPECT_NE(output.find("; This is a preceding comment"), std::string::npos);
}

TEST_F(IniFileTest, QuotedValuesWithComments) {
    IniFile ini;
    std::string content = 
"[section]\n"
"key1=\"quoted # value\"  # This is a comment\n"
"key2=\"another ; value\"  ; Another comment\n";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    EXPECT_EQ("\"quoted # value\"", ini.GetValue("section", "key1"));
    EXPECT_EQ("\"another ; value\"", ini.GetValue("section", "key2"));
    EXPECT_EQ("# This is a comment", ini.GetInlineComment("section", "key1"));
    EXPECT_EQ("; Another comment", ini.GetInlineComment("section", "key2"));
}

TEST_F(IniFileTest, RoundTripCommentPreservation) {
    IniFile ini;
    std::string originalContent = 
"# Leading comment\n"
"[section1]\n"
"key1=value1  # Inline comment 1\n"
"key2=value2  ; Inline comment 2\n"
"\n"
"[section2]\n"
"key3=value3\n"
"; Comment line\n"
"key4=value4  # Another inline\n";
    
    EXPECT_TRUE(ini.ParseFromString(originalContent));
    
    // Make some modifications
    EXPECT_TRUE(ini.SetValue("section1", "key1", "modified"));
    EXPECT_TRUE(ini.SetInlineComment("section2", "key3", "New comment"));
    
    std::string output = ini.WriteToString();
    
    // Check that comments are preserved and modifications are applied
    EXPECT_NE(output.find("# Leading comment"), std::string::npos);
    EXPECT_NE(output.find("key1 = modified  # Inline comment 1"), std::string::npos);
    EXPECT_NE(output.find("key2=value2  ; Inline comment 2"), std::string::npos);
    EXPECT_NE(output.find("key3 = value3  # New comment"), std::string::npos);
    EXPECT_NE(output.find("key4=value4  # Another inline"), std::string::npos);
}

TEST_F(IniFileTest, NoExtraNewlinesBeforeSections) {
    IniFile ini;
    
    // Add values to different sections using SetValue
    EXPECT_TRUE(ini.SetValue("section1", "key1", "value1"));
    EXPECT_TRUE(ini.SetValue("section2", "key2", "value2"));
    EXPECT_TRUE(ini.SetValue("section1", "key3", "value3"));
    
    std::string output = ini.WriteToString();
    
    // Debug: Print the actual output
    std::cout << "Generated output:\n" << output << "\n---\n";
    
    // Should have extra newlines before section headers but not excessive
    EXPECT_EQ(output.find("\n\n\n[section"), std::string::npos);
    EXPECT_NE(output.find("\n\n[section"), std::string::npos);

    // But should have clean section separation
    EXPECT_NE(output.find("[section1]"), std::string::npos);
    EXPECT_NE(output.find("[section2]"), std::string::npos);
}

// Additional comprehensive tests for parsing edge cases and recent fixes
TEST_F(IniFileTest, HexColorValueParsing) {
    IniFile ini;
    std::string content = 
"[colors]\n"
"red = #FF0000\n"
"green = #00FF00\n"
"blue = #0000FF\n"
"rgb = #112233\n"
"rgba = #12345678\n"
"short = #ABC\n"
"short_alpha = #ABCD\n"
"with_comment = #FF00FF  # This is a real comment\n"
"spaced = #123 456  # Comment after non-hex\n";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    
    // Hex colors should be parsed as values, not comments
    EXPECT_EQ("#FF0000", ini.GetValue("colors", "red"));
    EXPECT_EQ("#00FF00", ini.GetValue("colors", "green"));
    EXPECT_EQ("#0000FF", ini.GetValue("colors", "blue"));
    EXPECT_EQ("#112233", ini.GetValue("colors", "rgb"));
    EXPECT_EQ("#12345678", ini.GetValue("colors", "rgba"));
    EXPECT_EQ("#ABC", ini.GetValue("colors", "short"));
    EXPECT_EQ("#ABCD", ini.GetValue("colors", "short_alpha"));
    
    // Comments should be properly separated from hex values
    EXPECT_EQ("#FF00FF", ini.GetValue("colors", "with_comment"));
    EXPECT_EQ("# This is a real comment", ini.GetInlineComment("colors", "with_comment"));
    
    // Non-complete hex should have comment detection (space breaks hex parsing)
    EXPECT_EQ("#123", ini.GetValue("colors", "spaced"));
    EXPECT_EQ("456  # Comment after non-hex", ini.GetInlineComment("colors", "spaced"));
    
    // All inline comments should be empty except where expected
    EXPECT_EQ("", ini.GetInlineComment("colors", "red"));
    EXPECT_EQ("", ini.GetInlineComment("colors", "green"));
    EXPECT_EQ("", ini.GetInlineComment("colors", "blue"));
    EXPECT_EQ("", ini.GetInlineComment("colors", "rgb"));
}

TEST_F(IniFileTest, SemicolonSeparatorParsing) {
    IniFile ini;
    std::string content = 
"[data]\n"
"numbers = 1;2;3;4\n"
"mixed = 5;6;7;8;9\n"
"longer = 10;11;12;13;14;15\n"
"with_spaces = 1; 2; 3; 4\n"
"real_comment = value ; This is a real comment\n"
"edge_case_consecutive = 1;;2;;3\n"
"single_number = 42\n";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    
    // Semicolon-separated numeric values should be parsed as complete values
    EXPECT_EQ("1;2;3;4", ini.GetValue("data", "numbers"));
    EXPECT_EQ("5;6;7;8;9", ini.GetValue("data", "mixed"));
    EXPECT_EQ("10;11;12;13;14;15", ini.GetValue("data", "longer"));
    EXPECT_EQ("1; 2; 3; 4", ini.GetValue("data", "with_spaces"));
    // Edge case: consecutive semicolons - current behavior treats as partial value + comment
    EXPECT_EQ("1;", ini.GetValue("data", "edge_case_consecutive"));
    EXPECT_EQ("42", ini.GetValue("data", "single_number"));
    
    // Real comment after non-numeric should be detected
    EXPECT_EQ("value", ini.GetValue("data", "real_comment"));
    EXPECT_EQ("; This is a real comment", ini.GetInlineComment("data", "real_comment"));
    
    // No false comments
    EXPECT_EQ("", ini.GetInlineComment("data", "numbers"));
    EXPECT_EQ("", ini.GetInlineComment("data", "mixed"));
    EXPECT_EQ("", ini.GetInlineComment("data", "longer"));
    EXPECT_EQ("", ini.GetInlineComment("data", "with_spaces"));
    EXPECT_EQ(";2;;3", ini.GetInlineComment("data", "edge_case_consecutive"));
}

TEST_F(IniFileTest, ComplexCommentEdgeCases) {
    IniFile ini;
    std::string content = 
"[test]\n"
"# Start with # but not hex\n"
"hash_text = #notahexcolor\n"
"hex_then_comment = #FF0000 # Real comment after hex\n"
"semicolon_mixed = abc;123;def ; Comment after mixed content\n"
"quoted_hash = \"#FF0000\" # Comment after quoted hex\n"
"quoted_semicolon = \"1;2;3\" ; Comment after quoted numbers\n"
"multiple_hash = #ABC #DEF # Third is comment\n"
"edge_case = #12G ; Invalid hex so this is comment\n"
"empty_value = \n"
"only_comment = # Just a comment\n";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    
    // Non-hex # is treated as comment (since it doesn't match hex pattern)
    EXPECT_EQ("", ini.GetValue("test", "hash_text"));
    EXPECT_EQ("#notahexcolor", ini.GetInlineComment("test", "hash_text"));
    
    // Hex followed by comment should work
    EXPECT_EQ("#FF0000", ini.GetValue("test", "hex_then_comment"));
    EXPECT_EQ("# Real comment after hex", ini.GetInlineComment("test", "hex_then_comment"));
    
    // Mixed content: semicolon logic only works for pure numeric sequences
    EXPECT_EQ("abc", ini.GetValue("test", "semicolon_mixed"));
    EXPECT_EQ(";123;def ; Comment after mixed content", ini.GetInlineComment("test", "semicolon_mixed"));
    
    // Quoted values should preserve quotes and detect comments
    EXPECT_EQ("\"#FF0000\"", ini.GetValue("test", "quoted_hash"));
    EXPECT_EQ("# Comment after quoted hex", ini.GetInlineComment("test", "quoted_hash"));
    
    EXPECT_EQ("\"1;2;3\"", ini.GetValue("test", "quoted_semicolon"));
    EXPECT_EQ("; Comment after quoted numbers", ini.GetInlineComment("test", "quoted_semicolon"));
    
    // Multiple # - only first hex color recognized, space breaks hex detection
    EXPECT_EQ("#ABC", ini.GetValue("test", "multiple_hash"));
    EXPECT_EQ("#DEF # Third is comment", ini.GetInlineComment("test", "multiple_hash"));
    
    // Invalid hex (contains 'G') is treated as comment
    EXPECT_EQ("", ini.GetValue("test", "edge_case"));
    EXPECT_EQ("#12G ; Invalid hex so this is comment", ini.GetInlineComment("test", "edge_case"));
    
    // Empty value
    EXPECT_EQ("", ini.GetValue("test", "empty_value"));
    EXPECT_EQ("", ini.GetInlineComment("test", "empty_value"));
}

TEST_F(IniFileTest, RoundTripPreservationWithNewParsing) {
    IniFile ini;
    std::string originalContent = 
"[colors]\n"
"red = #FF0000\n"
"data = 1;2;3;4\n"
"normal = value # comment\n"
"\n"
"[mixed]\n"
"hex_comment = #ABCDEF # This is a comment\n"
"semi_comment = 5;6;7 ; This is also a comment\n";
    
    EXPECT_TRUE(ini.ParseFromString(originalContent));
    
    // Verify parsing is correct
    EXPECT_EQ("#FF0000", ini.GetValue("colors", "red"));
    EXPECT_EQ("1;2;3;4", ini.GetValue("colors", "data"));
    EXPECT_EQ("value", ini.GetValue("colors", "normal"));
    EXPECT_EQ("# comment", ini.GetInlineComment("colors", "normal"));
    
    EXPECT_EQ("#ABCDEF", ini.GetValue("mixed", "hex_comment"));
    EXPECT_EQ("# This is a comment", ini.GetInlineComment("mixed", "hex_comment"));
    
    EXPECT_EQ("5;6;7", ini.GetValue("mixed", "semi_comment"));
    EXPECT_EQ("; This is also a comment", ini.GetInlineComment("mixed", "semi_comment"));
    
    // Test round-trip
    std::string output = ini.WriteToString();
    
    // Parse the output again
    IniFile ini2;
    EXPECT_TRUE(ini2.ParseFromString(output));
    
    // Verify values are preserved
    EXPECT_EQ("#FF0000", ini2.GetValue("colors", "red"));
    EXPECT_EQ("1;2;3;4", ini2.GetValue("colors", "data"));
    EXPECT_EQ("value", ini2.GetValue("colors", "normal"));
    EXPECT_EQ("#ABCDEF", ini2.GetValue("mixed", "hex_comment"));
    EXPECT_EQ("5;6;7", ini2.GetValue("mixed", "semi_comment"));
    
    // Verify comments are preserved
    EXPECT_EQ("# comment", ini2.GetInlineComment("colors", "normal"));
    EXPECT_EQ("# This is a comment", ini2.GetInlineComment("mixed", "hex_comment"));
    EXPECT_EQ("; This is also a comment", ini2.GetInlineComment("mixed", "semi_comment"));
}

TEST_F(IniFileTest, NoExcessiveEmptyLinesBeforeSections) {
    IniFile ini;
    
    // Test content that already has proper spacing
    std::string content = R"([section1]
key1 = value1

[theme]
base = original

[section2]  
key2 = value2)";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    
    // Modify the theme base value (simulating what AnsiPalette does)
    EXPECT_TRUE(ini.SetValue("theme", "base", "nord"));
    
    std::string result = ini.WriteToString();
    
    // Count consecutive newlines before [theme]
    size_t themePos = result.find("[theme]");
    ASSERT_NE(themePos, std::string::npos);
    
    // Count preceding newlines
    int consecutiveNewlines = 0;
    for (int i = static_cast<int>(themePos) - 1; i >= 0 && result[i] == '\n'; --i) {
        consecutiveNewlines++;
    }
    
    // Should have exactly 2 newlines before [theme] (one for line ending, one for spacing)
    EXPECT_LE(consecutiveNewlines, 2);
    
    // Verify the base value was updated in place (should be in theme section)
    size_t basePos = result.find("base = nord");
    ASSERT_NE(basePos, std::string::npos);
    EXPECT_GT(basePos, themePos); // base should be after [theme] header
    
    size_t nextSectionPos = result.find("[section2]");
    if (nextSectionPos != std::string::npos) {
        EXPECT_LT(basePos, nextSectionPos); // base should be before next section
    }
}

TEST_F(IniFileTest, InPlaceValueUpdate) {
    IniFile ini;
    
    std::string content = R"([config]
option1 = value1
target = original_value
option2 = value2)";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    
    // Update existing key
    EXPECT_TRUE(ini.SetValue("config", "target", "new_value"));
    
    std::string result = ini.WriteToString();
    
    // Verify the updated line is in the correct position (between option1 and option2)
    size_t option1Pos = result.find("option1 = value1");
    size_t targetPos = result.find("target = new_value");
    size_t option2Pos = result.find("option2 = value2");
    
    ASSERT_NE(option1Pos, std::string::npos);
    ASSERT_NE(targetPos, std::string::npos);
    ASSERT_NE(option2Pos, std::string::npos);
    
    // Verify order is maintained
    EXPECT_LT(option1Pos, targetPos);
    EXPECT_LT(targetPos, option2Pos);
}

TEST_F(IniFileTest, NoTripleNewlinesBeforeSections) {
    IniFile ini;
    
    // Content with existing empty line before theme section  
    std::string content = R"([section1]
key1 = value1


[theme]
base = old_value

[section2]
key2 = value2)";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    
    // Modify value multiple times to ensure no accumulation of empty lines
    EXPECT_TRUE(ini.SetValue("theme", "base", "first_change"));
    std::string result1 = ini.WriteToString();
    
    // Parse and modify again
    EXPECT_TRUE(ini.ParseFromString(result1));
    EXPECT_TRUE(ini.SetValue("theme", "base", "second_change"));
    std::string result2 = ini.WriteToString();
    
    // Check that we don't have triple newlines (excessive empty lines)
    EXPECT_EQ(result2.find("\n\n\n"), std::string::npos);
    
    // Should have at most double newlines for section separation
    size_t doubleNewlineCount = 0;
    size_t pos = 0;
    while ((pos = result2.find("\n\n", pos)) != std::string::npos) {
        doubleNewlineCount++;
        pos += 2;
    }
    
    // Should have reasonable number of double newlines (for section separation)
    EXPECT_LE(doubleNewlineCount, 3); // At most before each of the 3 sections
}

TEST_F(IniFileTest, DeletedKeyReaddedToCorrectSection) {
    IniFile ini;
    
    // Create INI with multiple sections and a key to be deleted/re-added
    std::string content = R"([section1]
key1 = value1

[theme]
base = nord
other = value

[section2]
key2 = value2
)";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    
    // Verify initial state
    EXPECT_EQ("nord", ini.GetValue("theme", "base"));
    EXPECT_EQ("value", ini.GetValue("theme", "other"));
    
    // Delete the base key (simulating "pal theme none")
    EXPECT_TRUE(ini.RemoveKey("theme", "base"));
    
    // Verify base is deleted but other remains
    EXPECT_EQ("", ini.GetValue("theme", "base"));
    EXPECT_EQ("value", ini.GetValue("theme", "other"));
    
    // Re-add the base key (simulating setting base=nord again)
    EXPECT_TRUE(ini.SetValue("theme", "base", "nord"));
    
    // Generate output
    std::string result = ini.WriteToString();
    
    // The key issue: base should be in [theme] section, not at the end
    // Let's find where "base = nord" appears in the output
    size_t themePos = result.find("[theme]");
    size_t section2Pos = result.find("[section2]");
    size_t basePos = result.find("base = nord");
    
    EXPECT_NE(std::string::npos, themePos);
    EXPECT_NE(std::string::npos, section2Pos);  
    EXPECT_NE(std::string::npos, basePos);
    
    // The bug: base = nord should appear between [theme] and [section2]
    // Currently it might appear after [section2] (at end of file)
    EXPECT_LT(themePos, basePos) << "base = nord should appear after [theme] section";
    EXPECT_LT(basePos, section2Pos) << "base = nord should appear before [section2] section, not after it";
    
    // Also verify that "other = value" is still in theme section
    size_t otherPos = result.find("other = value");
    EXPECT_NE(std::string::npos, otherPos);
    EXPECT_LT(themePos, otherPos);
    EXPECT_LT(otherPos, section2Pos);
}

TEST_F(IniFileTest, ExistingKeyModificationInPlace) {
    IniFile ini;
    
    // Create INI with a key that will be modified
    std::string content = R"([section1]
key1 = original_value
key2 = middle_value

[theme]
base = old_theme
other = between_value
last = final_value

[section2]
key3 = end_value
)";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    
    // Verify initial state
    EXPECT_EQ("old_theme", ini.GetValue("theme", "base"));
    EXPECT_EQ("between_value", ini.GetValue("theme", "other"));
    EXPECT_EQ("final_value", ini.GetValue("theme", "last"));
    
    // Modify the existing "base" key (should be in-place modification)
    EXPECT_TRUE(ini.SetValue("theme", "base", "new_theme"));
    
    // Generate output
    std::string result = ini.WriteToString();
    
    // Verify "base = new_theme" is still in its original position (before "other = between_value")
    size_t themePos = result.find("[theme]");
    size_t basePos = result.find("base = new_theme");
    size_t otherPos = result.find("other = between_value");
    size_t lastPos = result.find("last = final_value");
    size_t section2Pos = result.find("[section2]");
    
    EXPECT_NE(std::string::npos, themePos);
    EXPECT_NE(std::string::npos, basePos);  
    EXPECT_NE(std::string::npos, otherPos);
    EXPECT_NE(std::string::npos, lastPos);
    EXPECT_NE(std::string::npos, section2Pos);
    
    // The key requirement: base should stay in its original position
    EXPECT_LT(themePos, basePos) << "base should appear after [theme] section";
    EXPECT_LT(basePos, otherPos) << "base should appear before other (original order)";
    EXPECT_LT(otherPos, lastPos) << "other should appear before last (original order)";
    EXPECT_LT(lastPos, section2Pos) << "last should appear before [section2]";
    
    // Ensure the old value is completely replaced
    EXPECT_EQ(std::string::npos, result.find("old_theme"));
}

TEST_F(IniFileTest, ExistingKeyWithCommentsModificationInPlace) {
    IniFile ini;
    
    // Create INI with comments and formatting that might cause issues
    std::string content = R"([theme]
# Base theme setting
base = old_theme  # inline comment
other = between_value

# Last setting
last = final_value
)";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    
    // Verify initial state
    EXPECT_EQ("old_theme", ini.GetValue("theme", "base"));
    EXPECT_EQ("# inline comment", ini.GetInlineComment("theme", "base"));
    
    // Modify the existing "base" key (should be in-place modification)
    EXPECT_TRUE(ini.SetValue("theme", "base", "new_theme"));
    
    // Generate output
    std::string result = ini.WriteToString();
    
    // Verify "base = new_theme" is still in its original position
    size_t themePos = result.find("[theme]");
    size_t baseCommentPos = result.find("# Base theme setting");
    size_t basePos = result.find("base = new_theme");
    size_t otherPos = result.find("other = between_value");
    
    EXPECT_NE(std::string::npos, themePos);
    EXPECT_NE(std::string::npos, baseCommentPos);
    EXPECT_NE(std::string::npos, basePos);  
    EXPECT_NE(std::string::npos, otherPos);
    
    // The key requirement: base should stay in its original position
    EXPECT_LT(themePos, baseCommentPos) << "[theme] should appear before base comment";
    EXPECT_LT(baseCommentPos, basePos) << "base comment should appear before base";
    EXPECT_LT(basePos, otherPos) << "base should appear before other (original order)";
    
    // Inline comment should be preserved
    EXPECT_EQ("# inline comment", ini.GetInlineComment("theme", "base"));
    
    // Ensure the old value is completely replaced
    EXPECT_EQ(std::string::npos, result.find("old_theme"));
}

TEST_F(IniFileTest, DebugKeyIndexAfterInsertion) {
    IniFile ini;
    
    // Simple test case that matches typical AnsiPalette usage
    std::string content = R"([theme]
base = old_value
other = other_value

)";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    
    // Modify existing key - this should be in-place
    EXPECT_TRUE(ini.SetValue("theme", "base", "new_value"));
    
    std::string result = ini.WriteToString();
    
    // Check order
    size_t basePos = result.find("base = new_value");
    size_t otherPos = result.find("other = other_value");
    
    EXPECT_NE(std::string::npos, basePos);
    EXPECT_NE(std::string::npos, otherPos);
    EXPECT_LT(basePos, otherPos) << "base should appear before other (original order preserved)";
}

TEST_F(IniFileTest, SequentialModificationsAfterInsertion) {
    IniFile ini;
    
    // Create initial content
    std::string content = R"([theme]
first = first_value
second = second_value

)";
    
    EXPECT_TRUE(ini.ParseFromString(content));
    
    // Step 1: Add a new key (this triggers my new insertion logic)
    EXPECT_TRUE(ini.SetValue("theme", "new_key", "new_value"));
    
    // Step 2: Now modify an existing key - this should be in place
    EXPECT_TRUE(ini.SetValue("theme", "first", "modified_first"));
    
    std::string result2 = ini.WriteToString();
    
    // Check that "first" is still in its original position
    size_t themePos = result2.find("[theme]");
    size_t firstPos = result2.find("first = modified_first");
    size_t secondPos = result2.find("second = second_value");
    size_t newKeyPos = result2.find("new_key = new_value");
    
    EXPECT_NE(std::string::npos, themePos);
    EXPECT_NE(std::string::npos, firstPos);
    EXPECT_NE(std::string::npos, secondPos);
    EXPECT_NE(std::string::npos, newKeyPos);
    
    // Verify order: first should come before second, and new_key should be inserted properly  
    EXPECT_LT(firstPos, secondPos) << "first should still appear before second after modification";
    EXPECT_LT(newKeyPos, themePos + result2.find("\n\n", themePos)) << "new_key should be within theme section";
}

TEST_F(IniFileTest, ParseStripsUtf8Bom) {
    IniFile ini;
    std::string content = "\xEF\xBB\xBF[section]\nkey=value\n";

    EXPECT_TRUE(ini.ParseFromString(content));
    EXPECT_TRUE(ini.HasSection("section"));
    EXPECT_FALSE(ini.HasSection("\xEF\xBB\xBFsection"));
    EXPECT_EQ("value", ini.GetValue("section", "key"));
}

TEST_F(IniFileTest, UnicodeWhitespaceKeysNormalizeCorrectly) {
    IniFile ini;
    std::string content = "[sec]\n\xC2\xA0Key\xC2\xA0 = value\n";

    ASSERT_TRUE(ini.ParseFromString(content));
    EXPECT_TRUE(ini.HasKey("sec", "Key"));
    EXPECT_EQ("value", ini.GetValue("sec", "Key"));
    EXPECT_EQ("value", ini.GetValue("sec", "\xC2\xA0Key\xC2\xA0"));

    EXPECT_TRUE(ini.SetValue("sec", "Key", "updated"));
    EXPECT_EQ("updated", ini.GetValue("sec", "Key"));
}

TEST_F(IniFileTest, InlineCommentRequiresWhitespaceAndRespectsHeuristics) {
    IniFile ini;
    std::string content = R"([section]
color = #ffcc00
color_with_comment = #ffcc00  # color comment
path = C:\Program Files;Games
list = 5;6;7;8
list_spaced = 1 ; 2 ; 3 ; 4
quoted = "value # inside"
value_with_comment = foo # trailing comment
no_space_hash = foo#bar
semi_comment = foo ; trailing
)";

    ASSERT_TRUE(ini.ParseFromString(content));


    EXPECT_EQ("#ffcc00", ini.GetValue("section", "color"));
    EXPECT_EQ("", ini.GetInlineComment("section", "color"));

    EXPECT_EQ("#ffcc00", ini.GetValue("section", "color_with_comment"));
    EXPECT_EQ("# color comment", ini.GetInlineComment("section", "color_with_comment"));

    EXPECT_EQ(R"(C:\Program Files;Games)", ini.GetValue("section", "path"));
    EXPECT_EQ("", ini.GetInlineComment("section", "path"));

    EXPECT_EQ("5;6;7;8", ini.GetValue("section", "list"));
    EXPECT_EQ("", ini.GetInlineComment("section", "list"));

    EXPECT_EQ("1 ; 2 ; 3 ; 4", ini.GetValue("section", "list_spaced"));
    EXPECT_EQ("", ini.GetInlineComment("section", "list_spaced"));

    EXPECT_EQ("\"value # inside\"", ini.GetValue("section", "quoted"));
    EXPECT_EQ("", ini.GetInlineComment("section", "quoted"));

    EXPECT_EQ("foo", ini.GetValue("section", "value_with_comment"));
    EXPECT_EQ("# trailing comment", ini.GetInlineComment("section", "value_with_comment"));

    EXPECT_EQ("foo#bar", ini.GetValue("section", "no_space_hash"));
    EXPECT_EQ("", ini.GetInlineComment("section", "no_space_hash"));

    EXPECT_EQ("foo", ini.GetValue("section", "semi_comment"));
    EXPECT_EQ("; trailing", ini.GetInlineComment("section", "semi_comment"));
}

TEST_F(IniFileTest, DuplicateSectionsPreservedLastWinsLookup) {
    IniFile ini;
    std::string content = R"([section]
key = original
[section]
key = override
other = data
)";

    ASSERT_TRUE(ini.ParseFromString(content));

    const auto &sections = ini.GetSections();
    ASSERT_EQ(2u, sections.size());
    EXPECT_EQ("override", ini.GetValue("section", "key"));

    IniFile::Section *lookupSection = ini.GetSection("section");
    ASSERT_NE(nullptr, lookupSection);
    ASSERT_FALSE(lookupSection->entries.empty());
    EXPECT_EQ("override", lookupSection->entries.front().value);

    EXPECT_EQ("original", sections.front().entries.front().value);

    std::string output = ini.WriteToString();
    size_t first = output.find("[section]");
    ASSERT_NE(std::string::npos, first);
    size_t second = output.find("[section]", first + 1);
    EXPECT_NE(std::string::npos, second);
    EXPECT_NE(std::string::npos, output.find("key = original"));
    EXPECT_NE(std::string::npos, output.find("key = override"));
}

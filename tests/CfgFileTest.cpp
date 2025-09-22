#include <gtest/gtest.h>

#include "CfgFile.h"

#include <algorithm>

class CfgFileTest : public ::testing::Test {
protected:
    static CfgFile Parse(const std::string &content) {
        CfgFile cfg;
        EXPECT_TRUE(cfg.ParseFromString(content)) << cfg.GetLastError();
        return cfg;
    }
};

TEST_F(CfgFileTest, ParsesAllPropertyTypesAndComments) {
    const std::string content = R"CFG(# Primary configuration
Graphics {
    # Integer dimensions
    I Width 1920
    I Height 1080

    # Display settings
    B Fullscreen true
    F Gamma 2.2
    K Hotkey 70
    S Title "My Game"
}
)CFG";

    CfgFile cfg;
    ASSERT_TRUE(cfg.ParseFromString(content)) << cfg.GetLastError();

    ASSERT_TRUE(cfg.HasCategory("Graphics"));
    const auto *category = cfg.GetCategory("Graphics");
    ASSERT_NE(category, nullptr);
    EXPECT_EQ(category->properties.size(), 6);

    EXPECT_EQ(cfg.GetIntegerProperty("Graphics", "Width"), 1920);
    EXPECT_EQ(cfg.GetIntegerProperty("Graphics", "Height"), 1080);
    EXPECT_FLOAT_EQ(cfg.GetFloatProperty("Graphics", "Gamma"), 2.2f);
    EXPECT_TRUE(cfg.GetBooleanProperty("Graphics", "Fullscreen"));
    EXPECT_EQ(cfg.GetKeyProperty("Graphics", "Hotkey"), 70);
    EXPECT_EQ(cfg.GetStringProperty("Graphics", "Title"), "My Game");

    EXPECT_EQ(cfg.GetPropertyComment("Graphics", "Width"), "Integer dimensions");
    EXPECT_EQ(cfg.GetPropertyComment("Graphics", "Fullscreen"), "Display settings");
}

TEST_F(CfgFileTest, ParsesBooleanValuesFlexibly) {
    const std::string content = R"CFG(Graphics {
    B FlagTrue true
    B FlagFalse false
    B FlagOne 1
    B FlagZero 0
    B FlagUpper TRUE
    B FlagMixed False
})CFG";

    CfgFile cfg;
    ASSERT_TRUE(cfg.ParseFromString(content)) << cfg.GetLastError();

    EXPECT_TRUE(cfg.GetBooleanProperty("Graphics", "FlagTrue"));
    EXPECT_FALSE(cfg.GetBooleanProperty("Graphics", "FlagFalse"));
    EXPECT_TRUE(cfg.GetBooleanProperty("Graphics", "FlagOne"));
    EXPECT_FALSE(cfg.GetBooleanProperty("Graphics", "FlagZero"));
    EXPECT_TRUE(cfg.GetBooleanProperty("Graphics", "FlagUpper"));
    EXPECT_FALSE(cfg.GetBooleanProperty("Graphics", "FlagMixed"));
}

TEST_F(CfgFileTest, WriteToStringProducesReadableOutput) {
    CfgFile cfg;
    ASSERT_TRUE(cfg.SetIntegerProperty("Gameplay", "Lives", 3));
    ASSERT_TRUE(cfg.SetBooleanProperty("Gameplay", "AllowContinue", true));
    ASSERT_TRUE(cfg.SetFloatProperty("Gameplay", "Speed", 1.25f));
    ASSERT_TRUE(cfg.SetStringProperty("Gameplay", "Nickname", "Player One"));

    const std::string serialized = cfg.WriteToString();

    EXPECT_NE(serialized.find("Gameplay {"), std::string::npos);
    EXPECT_NE(serialized.find("B AllowContinue true"), std::string::npos);
    EXPECT_NE(serialized.find("S Nickname \"Player One\""), std::string::npos);

    CfgFile roundTrip;
    ASSERT_TRUE(roundTrip.ParseFromString(serialized)) << roundTrip.GetLastError();
    EXPECT_EQ(roundTrip.GetIntegerProperty("Gameplay", "Lives"), 3);
    EXPECT_TRUE(roundTrip.GetBooleanProperty("Gameplay", "AllowContinue"));
    EXPECT_FLOAT_EQ(roundTrip.GetFloatProperty("Gameplay", "Speed"), 1.25f);
    EXPECT_EQ(roundTrip.GetStringProperty("Gameplay", "Nickname"), "Player One");
}

TEST_F(CfgFileTest, RespectsCaseSensitivitySetting) {
    CfgFile cfg;
    cfg.SetCaseSensitive(true);

    ASSERT_TRUE(cfg.SetStringProperty("Controls", "Jump", "Space"));
    ASSERT_TRUE(cfg.SetStringProperty("Controls", "jump", "J"));

    EXPECT_EQ(cfg.GetStringProperty("Controls", "Jump"), "Space");
    EXPECT_EQ(cfg.GetStringProperty("Controls", "jump"), "J");

    cfg.SetCaseSensitive(false);
    EXPECT_EQ(cfg.GetStringProperty("Controls", "JUMP"), "J");
}

// Edge case tests for parsing malformed content
TEST_F(CfgFileTest, HandlesEmptyContent) {
    CfgFile cfg;
    EXPECT_TRUE(cfg.ParseFromString(""));
    EXPECT_TRUE(cfg.IsEmpty());
    EXPECT_EQ(cfg.GetCategoryCount(), 0);
}

TEST_F(CfgFileTest, HandlesOnlyComments) {
    const std::string content = R"CFG(# Just comments
# Nothing else here
# Still just comments)CFG";

    CfgFile cfg;
    EXPECT_TRUE(cfg.ParseFromString(content));
    EXPECT_TRUE(cfg.IsEmpty());
}

TEST_F(CfgFileTest, HandlesOnlyWhitespace) {
    const std::string content = "   \n\t\r\n   \n";

    CfgFile cfg;
    EXPECT_TRUE(cfg.ParseFromString(content));
    EXPECT_TRUE(cfg.IsEmpty());
}

TEST_F(CfgFileTest, FailsOnMissingCategoryClosingBrace) {
    const std::string content = R"CFG(Graphics {
    I Width 1920
    I Height 1080)CFG";

    CfgFile cfg;
    EXPECT_FALSE(cfg.ParseFromString(content));
    EXPECT_FALSE(cfg.GetLastError().empty());
}

TEST_F(CfgFileTest, FailsOnUnexpectedClosingBrace) {
    const std::string content = R"CFG(Graphics {
    I Width 1920
}
})CFG";

    CfgFile cfg;
    EXPECT_FALSE(cfg.ParseFromString(content));
    EXPECT_FALSE(cfg.GetLastError().empty());
}

TEST_F(CfgFileTest, FailsOnInvalidPropertyType) {
    const std::string content = R"CFG(Graphics {
    X InvalidType value
})CFG";

    CfgFile cfg;
    EXPECT_FALSE(cfg.ParseFromString(content));
    EXPECT_FALSE(cfg.GetLastError().empty());
}

TEST_F(CfgFileTest, FailsOnMalformedPropertyLine) {
    const std::string content = R"CFG(Graphics {
    I
})CFG";

    CfgFile cfg;
    EXPECT_FALSE(cfg.ParseFromString(content));
    EXPECT_FALSE(cfg.GetLastError().empty());
}

TEST_F(CfgFileTest, FailsOnPropertyOutsideCategory) {
    const std::string content = R"CFG(I Width 1920
Graphics {
    I Height 1080
})CFG";

    CfgFile cfg;
    EXPECT_FALSE(cfg.ParseFromString(content));
    EXPECT_FALSE(cfg.GetLastError().empty());
}

TEST_F(CfgFileTest, HandlesNestedBracesInStrings) {
    const std::string content = R"CFG(Config {
    S Message "Hello {world} with {braces}"
    S Path "C:\\Program Files\\{App}"
})CFG";

    CfgFile cfg;
    EXPECT_TRUE(cfg.ParseFromString(content));
    EXPECT_EQ(cfg.GetStringProperty("Config", "Message"), "Hello {world} with {braces}");
    EXPECT_EQ(cfg.GetStringProperty("Config", "Path"), "C:\\Program Files\\{App}");
}

TEST_F(CfgFileTest, HandlesEscapedQuotesInStrings) {
    const std::string content = R"CFG(Config {
    S Quote "She said \"Hello\" to me"
    S Path "C:\\\"Program Files\\\""
})CFG";

    CfgFile cfg;
    EXPECT_TRUE(cfg.ParseFromString(content));
    EXPECT_EQ(cfg.GetStringProperty("Config", "Quote"), "She said \"Hello\" to me");
    EXPECT_EQ(cfg.GetStringProperty("Config", "Path"), "C:\\\"Program Files\\\"");
}

// File I/O tests
TEST_F(CfgFileTest, HandlesNonExistentFile) {
    CfgFile cfg;
    EXPECT_FALSE(cfg.ParseFromFile(L"nonexistent_file.cfg"));
    EXPECT_FALSE(cfg.GetLastError().empty());
}

TEST_F(CfgFileTest, WriteAndReadFileRoundTrip) {
    CfgFile original;
    ASSERT_TRUE(original.SetStringProperty("Test", "Name", "Value"));
    ASSERT_TRUE(original.SetIntegerProperty("Test", "Number", 42));
    ASSERT_TRUE(original.SetBooleanProperty("Test", "Flag", true));

    const std::wstring testFile = L"test_output.cfg";

    ASSERT_TRUE(original.WriteToFile(testFile));

    CfgFile loaded;
    ASSERT_TRUE(loaded.ParseFromFile(testFile));

    EXPECT_EQ(loaded.GetStringProperty("Test", "Name"), "Value");
    EXPECT_EQ(loaded.GetIntegerProperty("Test", "Number"), 42);
    EXPECT_TRUE(loaded.GetBooleanProperty("Test", "Flag"));
}

// UTF-8 validation and handling tests
TEST_F(CfgFileTest, ValidatesUtf8Input) {
    CfgFile cfg;
    cfg.SetStrictUtf8Validation(true);

    const std::string validUtf8 = "Hello \xe4\xb8\x96\xe7\x95\x8c \xf0\x9f\x8c\x8d";
    EXPECT_TRUE(cfg.IsValidUtf8(validUtf8));

    const std::string invalidUtf8 = "Hello \xFF\xFE World";
    EXPECT_FALSE(cfg.IsValidUtf8(invalidUtf8));
}

TEST_F(CfgFileTest, HandlesNonAsciiCategoryAndPropertyNames) {
    const std::string content = R"CFG(Game_Settings {
    S Player_Name "Zhang_San"
    I Level 42
    B Sound_Enabled true
})CFG";

    CfgFile cfg;
    ASSERT_TRUE(cfg.ParseFromString(content));

    EXPECT_TRUE(cfg.HasCategory("Game_Settings"));
    EXPECT_EQ(cfg.GetStringProperty("Game_Settings", "Player_Name"), "Zhang_San");
    EXPECT_EQ(cfg.GetIntegerProperty("Game_Settings", "Level"), 42);
    EXPECT_TRUE(cfg.GetBooleanProperty("Game_Settings", "Sound_Enabled"));
}

TEST_F(CfgFileTest, HandlesComplexStringValues) {
    const std::string content = R"CFG(Messages {
    S English "Hello World"
    S WithEscapes "Line1\nLine2\tTabbed"
    S WithQuotes "She said \"Hello\" to me"
    S WithBackslash "Path\\to\\file"
    S SpecialChars "!@#$%^&*()_+-=[]{}|;:',.<>?"
})CFG";

    CfgFile cfg;
    ASSERT_TRUE(cfg.ParseFromString(content));

    EXPECT_EQ(cfg.GetStringProperty("Messages", "English"), "Hello World");
    EXPECT_EQ(cfg.GetStringProperty("Messages", "WithEscapes"), "Line1\nLine2\tTabbed");
    EXPECT_EQ(cfg.GetStringProperty("Messages", "WithQuotes"), "She said \"Hello\" to me");
    EXPECT_EQ(cfg.GetStringProperty("Messages", "WithBackslash"), "Path\\to\\file");
    EXPECT_EQ(cfg.GetStringProperty("Messages", "SpecialChars"), "!@#$%^&*()_+-=[]{}|;:',.<>?");
}

TEST_F(CfgFileTest, CalculatesUtf8LengthCorrectly) {
    CfgFile cfg;

    EXPECT_EQ(cfg.GetUtf8Length("Hello"), 5);
    EXPECT_EQ(cfg.GetUtf8Length("\xe4\xbd\xa0\xe5\xa5\xbd"), 2);
    EXPECT_EQ(cfg.GetUtf8Length("\xf0\x9f\x8c\x8d"), 1);
    EXPECT_EQ(cfg.GetUtf8Length("Hello \xf0\x9f\x8c\x8d"), 7);
}

// Category management tests
TEST_F(CfgFileTest, AddsCategoriesCorrectly) {
    CfgFile cfg;

    EXPECT_FALSE(cfg.HasCategory("Graphics"));
    EXPECT_EQ(cfg.GetCategoryCount(), 0);

    auto *graphics = cfg.AddCategory("Graphics");
    ASSERT_NE(graphics, nullptr);
    EXPECT_EQ(graphics->name, "Graphics");
    EXPECT_TRUE(cfg.HasCategory("Graphics"));
    EXPECT_EQ(cfg.GetCategoryCount(), 1);

    auto *audio = cfg.AddCategory("Audio");
    ASSERT_NE(audio, nullptr);
    EXPECT_EQ(cfg.GetCategoryCount(), 2);
}

TEST_F(CfgFileTest, RemovesCategoriesCorrectly) {
    CfgFile cfg;
    ASSERT_TRUE(cfg.SetStringProperty("Graphics", "Resolution", "1920x1080"));
    ASSERT_TRUE(cfg.SetStringProperty("Audio", "Device", "Default"));

    EXPECT_EQ(cfg.GetCategoryCount(), 2);
    EXPECT_TRUE(cfg.HasCategory("Graphics"));
    EXPECT_TRUE(cfg.HasCategory("Audio"));

    EXPECT_TRUE(cfg.RemoveCategory("Graphics"));
    EXPECT_FALSE(cfg.HasCategory("Graphics"));
    EXPECT_TRUE(cfg.HasCategory("Audio"));
    EXPECT_EQ(cfg.GetCategoryCount(), 1);

    EXPECT_FALSE(cfg.RemoveCategory("NonExistent"));
    EXPECT_EQ(cfg.GetCategoryCount(), 1);
}

TEST_F(CfgFileTest, GetsCategoryNamesCorrectly) {
    CfgFile cfg;
    ASSERT_TRUE(cfg.SetStringProperty("Zebra", "Test", "1"));
    ASSERT_TRUE(cfg.SetStringProperty("Alpha", "Test", "2"));
    ASSERT_TRUE(cfg.SetStringProperty("Beta", "Test", "3"));

    auto names = cfg.GetCategoryNames();
    EXPECT_EQ(names.size(), 3);

    std::sort(names.begin(), names.end());
    EXPECT_EQ(names[0], "Alpha");
    EXPECT_EQ(names[1], "Beta");
    EXPECT_EQ(names[2], "Zebra");
}

TEST_F(CfgFileTest, HandlesDuplicateCategoryAddition) {
    CfgFile cfg;

    auto *first = cfg.AddCategory("Test");
    ASSERT_NE(first, nullptr);

    auto *second = cfg.AddCategory("Test");
    EXPECT_EQ(first, second);
    EXPECT_EQ(cfg.GetCategoryCount(), 1);
}

TEST_F(CfgFileTest, ClearRemovesAllContent) {
    CfgFile cfg;
    ASSERT_TRUE(cfg.SetStringProperty("Graphics", "Resolution", "1920x1080"));
    ASSERT_TRUE(cfg.SetStringProperty("Audio", "Device", "Default"));

    EXPECT_FALSE(cfg.IsEmpty());
    EXPECT_EQ(cfg.GetCategoryCount(), 2);

    cfg.Clear();

    EXPECT_TRUE(cfg.IsEmpty());
    EXPECT_EQ(cfg.GetCategoryCount(), 0);
    EXPECT_FALSE(cfg.HasCategory("Graphics"));
    EXPECT_FALSE(cfg.HasCategory("Audio"));
}

// Property management tests
TEST_F(CfgFileTest, HandlesPropertyExistence) {
    CfgFile cfg;
    ASSERT_TRUE(cfg.SetStringProperty("Test", "Prop1", "Value"));

    EXPECT_TRUE(cfg.HasProperty("Test", "Prop1"));
    EXPECT_FALSE(cfg.HasProperty("Test", "Prop2"));
    EXPECT_FALSE(cfg.HasProperty("NonExistent", "Prop1"));
}

TEST_F(CfgFileTest, GetsAndSetsAllPropertyTypes) {
    CfgFile cfg;

    ASSERT_TRUE(cfg.SetStringProperty("Test", "StringProp", "Hello World"));
    ASSERT_TRUE(cfg.SetBooleanProperty("Test", "BoolProp", true));
    ASSERT_TRUE(cfg.SetIntegerProperty("Test", "IntProp", -42));
    ASSERT_TRUE(cfg.SetFloatProperty("Test", "FloatProp", 3.14159f));
    ASSERT_TRUE(cfg.SetKeyProperty("Test", "KeyProp", 65));

    EXPECT_EQ(cfg.GetStringProperty("Test", "StringProp"), "Hello World");
    EXPECT_TRUE(cfg.GetBooleanProperty("Test", "BoolProp"));
    EXPECT_EQ(cfg.GetIntegerProperty("Test", "IntProp"), -42);
    EXPECT_FLOAT_EQ(cfg.GetFloatProperty("Test", "FloatProp"), 3.14159f);
    EXPECT_EQ(cfg.GetKeyProperty("Test", "KeyProp"), 65);
}

TEST_F(CfgFileTest, HandlesPropertyDefaultValues) {
    CfgFile cfg;

    EXPECT_EQ(cfg.GetStringProperty("NonExistent", "Prop", "Default"), "Default");
    EXPECT_TRUE(cfg.GetBooleanProperty("NonExistent", "Prop", true));
    EXPECT_EQ(cfg.GetIntegerProperty("NonExistent", "Prop", 999), 999);
    EXPECT_FLOAT_EQ(cfg.GetFloatProperty("NonExistent", "Prop", 1.5f), 1.5f);
    EXPECT_EQ(cfg.GetKeyProperty("NonExistent", "Prop", 123), 123);
}

TEST_F(CfgFileTest, OverwritesExistingProperties) {
    CfgFile cfg;

    ASSERT_TRUE(cfg.SetStringProperty("Test", "Value", "Original"));
    EXPECT_EQ(cfg.GetStringProperty("Test", "Value"), "Original");

    ASSERT_TRUE(cfg.SetStringProperty("Test", "Value", "Updated"));
    EXPECT_EQ(cfg.GetStringProperty("Test", "Value"), "Updated");

    ASSERT_TRUE(cfg.SetIntegerProperty("Test", "Value", 42));
    EXPECT_EQ(cfg.GetIntegerProperty("Test", "Value"), 42);
}

TEST_F(CfgFileTest, HandlesNumericEdgeCases) {
    CfgFile cfg;

    ASSERT_TRUE(cfg.SetIntegerProperty("Test", "MinInt", INT_MIN));
    ASSERT_TRUE(cfg.SetIntegerProperty("Test", "MaxInt", INT_MAX));
    ASSERT_TRUE(cfg.SetFloatProperty("Test", "Zero", 0.0f));
    ASSERT_TRUE(cfg.SetFloatProperty("Test", "Negative", -123.456f));
    ASSERT_TRUE(cfg.SetFloatProperty("Test", "Small", 1e-6f));
    ASSERT_TRUE(cfg.SetFloatProperty("Test", "Large", 1e6f));

    EXPECT_EQ(cfg.GetIntegerProperty("Test", "MinInt"), INT_MIN);
    EXPECT_EQ(cfg.GetIntegerProperty("Test", "MaxInt"), INT_MAX);
    EXPECT_FLOAT_EQ(cfg.GetFloatProperty("Test", "Zero"), 0.0f);
    EXPECT_FLOAT_EQ(cfg.GetFloatProperty("Test", "Negative"), -123.456f);
    EXPECT_FLOAT_EQ(cfg.GetFloatProperty("Test", "Small"), 1e-6f);
    EXPECT_FLOAT_EQ(cfg.GetFloatProperty("Test", "Large"), 1e6f);
}

TEST_F(CfgFileTest, HandlesSpecialStringValues) {
    CfgFile cfg;

    ASSERT_TRUE(cfg.SetStringProperty("Test", "Empty", ""));
    ASSERT_TRUE(cfg.SetStringProperty("Test", "Spaces", "   "));
    ASSERT_TRUE(cfg.SetStringProperty("Test", "Newlines", "Line1\nLine2\r\nLine3"));
    ASSERT_TRUE(cfg.SetStringProperty("Test", "Tabs", "Before\tAfter"));
    ASSERT_TRUE(cfg.SetStringProperty("Test", "Unicode", "\xf0\x9f\x8e\xae\xf0\x9f\x8c\x8d\xf0\x9f\x8e\xb5"));

    EXPECT_EQ(cfg.GetStringProperty("Test", "Empty"), "");
    EXPECT_EQ(cfg.GetStringProperty("Test", "Spaces"), "   ");
    EXPECT_EQ(cfg.GetStringProperty("Test", "Newlines"), "Line1\nLine2\r\nLine3");
    EXPECT_EQ(cfg.GetStringProperty("Test", "Tabs"), "Before\tAfter");
    EXPECT_EQ(cfg.GetStringProperty("Test", "Unicode"), "\xf0\x9f\x8e\xae\xf0\x9f\x8c\x8d\xf0\x9f\x8e\xb5");
}

TEST_F(CfgFileTest, DirectPropertyAccess) {
    CfgFile cfg;
    ASSERT_TRUE(cfg.SetStringProperty("Test", "Prop", "Value"));

    auto *prop = cfg.GetProperty("Test", "Prop");
    ASSERT_NE(prop, nullptr);
    EXPECT_EQ(prop->name, "Prop");
    EXPECT_EQ(prop->type, CfgPropertyType::STRING);
    EXPECT_EQ(prop->GetString(), "Value");

    const auto *constProp = static_cast<const CfgFile&>(cfg).GetProperty("Test", "Prop");
    ASSERT_NE(constProp, nullptr);
    EXPECT_EQ(constProp->GetString(), "Value");

    EXPECT_EQ(cfg.GetProperty("NonExistent", "Prop"), nullptr);
}

// Comment handling tests
TEST_F(CfgFileTest, SetsAndGetsPropertyComments) {
    CfgFile cfg;
    ASSERT_TRUE(cfg.SetStringProperty("Test", "Prop", "Value"));

    EXPECT_TRUE(cfg.SetPropertyComment("Test", "Prop", "This is a comment"));
    EXPECT_EQ(cfg.GetPropertyComment("Test", "Prop"), "This is a comment");

    EXPECT_TRUE(cfg.SetPropertyComment("Test", "Prop", "Updated comment"));
    EXPECT_EQ(cfg.GetPropertyComment("Test", "Prop"), "Updated comment");

    EXPECT_EQ(cfg.GetPropertyComment("Test", "NonExistent"), "");
    EXPECT_EQ(cfg.GetPropertyComment("NonExistent", "Prop"), "");
}

TEST_F(CfgFileTest, SetsAndGetsCategoryComments) {
    CfgFile cfg;
    ASSERT_TRUE(cfg.SetStringProperty("Graphics", "Width", "1920"));

    EXPECT_TRUE(cfg.SetCategoryComment("Graphics", "Display settings"));
    EXPECT_EQ(cfg.GetCategoryComment("Graphics"), "Display settings");

    EXPECT_TRUE(cfg.SetCategoryComment("Graphics", "Updated display settings"));
    EXPECT_EQ(cfg.GetCategoryComment("Graphics"), "Updated display settings");

    EXPECT_EQ(cfg.GetCategoryComment("NonExistent"), "");
}

TEST_F(CfgFileTest, HandlesEmptyComments) {
    CfgFile cfg;
    ASSERT_TRUE(cfg.SetStringProperty("Test", "Prop", "Value"));

    EXPECT_TRUE(cfg.SetPropertyComment("Test", "Prop", ""));
    EXPECT_EQ(cfg.GetPropertyComment("Test", "Prop"), "");

    EXPECT_TRUE(cfg.SetCategoryComment("Test", ""));
    EXPECT_EQ(cfg.GetCategoryComment("Test"), "");
}

TEST_F(CfgFileTest, HandlesMultilineComments) {
    const std::string content = R"CFG(# This is a
# multiline comment
Graphics {
    # Width setting
    # Controls horizontal resolution
    I Width 1920

    # Simple comment
    I Height 1080
})CFG";

    CfgFile cfg;
    ASSERT_TRUE(cfg.ParseFromString(content));

    EXPECT_EQ(cfg.GetPropertyComment("Graphics", "Width"), "Width setting\nControls horizontal resolution");
    EXPECT_EQ(cfg.GetPropertyComment("Graphics", "Height"), "Simple comment");
}

TEST_F(CfgFileTest, PreservesCommentsInRoundTrip) {
    CfgFile cfg;
    ASSERT_TRUE(cfg.SetStringProperty("Graphics", "Width", "1920"));
    ASSERT_TRUE(cfg.SetPropertyComment("Graphics", "Width", "Screen width"));
    ASSERT_TRUE(cfg.SetCategoryComment("Graphics", "Display configuration"));

    const std::string serialized = cfg.WriteToString();

    CfgFile reloaded;
    ASSERT_TRUE(reloaded.ParseFromString(serialized));

    EXPECT_EQ(reloaded.GetPropertyComment("Graphics", "Width"), "Screen width");
    EXPECT_EQ(reloaded.GetCategoryComment("Graphics"), "Display configuration");
}

// Error handling and validation tests
TEST_F(CfgFileTest, ErrorReportingAndClearing) {
    CfgFile cfg;

    cfg.ClearError();
    EXPECT_TRUE(cfg.GetLastError().empty());

    EXPECT_FALSE(cfg.ParseFromString("Invalid {"));
    EXPECT_FALSE(cfg.GetLastError().empty());

    cfg.ClearError();
    EXPECT_TRUE(cfg.GetLastError().empty());
}

TEST_F(CfgFileTest, ConfigurationSettings) {
    CfgFile cfg;

    EXPECT_FALSE(cfg.IsCaseSensitive());
    cfg.SetCaseSensitive(true);
    EXPECT_TRUE(cfg.IsCaseSensitive());

    EXPECT_TRUE(cfg.IsStrictUtf8Validation());
    cfg.SetStrictUtf8Validation(false);
    EXPECT_FALSE(cfg.IsStrictUtf8Validation());
}

TEST_F(CfgFileTest, HandlesInvalidFloatValues) {
    const std::string content = R"CFG(Test {
    F BadFloat notanumber
})CFG";

    CfgFile cfg;
    EXPECT_FALSE(cfg.ParseFromString(content));
    EXPECT_FALSE(cfg.GetLastError().empty());
}

TEST_F(CfgFileTest, HandlesInvalidIntegerValues) {
    const std::string content = R"CFG(Test {
    I BadInt notanumber
})CFG";

    CfgFile cfg;
    EXPECT_FALSE(cfg.ParseFromString(content));
    EXPECT_FALSE(cfg.GetLastError().empty());
}

TEST_F(CfgFileTest, HandlesInvalidBooleanValues) {
    const std::string content = R"CFG(Test {
    B BadBool maybe
})CFG";

    CfgFile cfg;
    EXPECT_FALSE(cfg.ParseFromString(content));
    EXPECT_FALSE(cfg.GetLastError().empty());
}

TEST_F(CfgFileTest, HandlesMissingStringQuotes) {
    const std::string content = R"CFG(Test {
    S BadString value without quotes
})CFG";

    CfgFile cfg;
    EXPECT_FALSE(cfg.ParseFromString(content));
    EXPECT_FALSE(cfg.GetLastError().empty());
}

TEST_F(CfgFileTest, HandlesUnterminatedStrings) {
    const std::string content = R"CFG(Test {
    S BadString "unterminated string
})CFG";

    CfgFile cfg;
    EXPECT_FALSE(cfg.ParseFromString(content));
    EXPECT_FALSE(cfg.GetLastError().empty());
}

TEST_F(CfgFileTest, FailsOnSettingPropertyInNonExistentCategory) {
    CfgFile cfg;

    EXPECT_FALSE(cfg.SetPropertyComment("NonExistent", "Prop", "Comment"));
}

// Stress tests for limits and boundaries
TEST_F(CfgFileTest, HandlesLargeCategoryNames) {
    CfgFile cfg;

    // Test near the limit
    std::string longName(200, 'A');
    ASSERT_TRUE(cfg.SetStringProperty(longName, "Test", "Value"));
    EXPECT_TRUE(cfg.HasCategory(longName));

    // Test extremely long name that might exceed limits
    std::string veryLongName(1000, 'B');
    ASSERT_TRUE(cfg.SetStringProperty(veryLongName, "Test", "Value"));
    EXPECT_TRUE(cfg.HasCategory(veryLongName));
}

TEST_F(CfgFileTest, HandlesLargePropertyNames) {
    CfgFile cfg;

    std::string longPropName(200, 'P');
    ASSERT_TRUE(cfg.SetStringProperty("Test", longPropName, "Value"));
    EXPECT_TRUE(cfg.HasProperty("Test", longPropName));
    EXPECT_EQ(cfg.GetStringProperty("Test", longPropName), "Value");
}

TEST_F(CfgFileTest, HandlesLargeStringValues) {
    CfgFile cfg;

    // Test large string value
    std::string largeValue(10000, 'X');
    ASSERT_TRUE(cfg.SetStringProperty("Test", "LargeValue", largeValue));
    EXPECT_EQ(cfg.GetStringProperty("Test", "LargeValue"), largeValue);

    // Test round trip with large value
    const std::string serialized = cfg.WriteToString();
    CfgFile reloaded;
    ASSERT_TRUE(reloaded.ParseFromString(serialized));
    EXPECT_EQ(reloaded.GetStringProperty("Test", "LargeValue"), largeValue);
}

TEST_F(CfgFileTest, HandlesManyCategoriesAndProperties) {
    CfgFile cfg;

    // Create many categories with properties
    for (int i = 0; i < 100; ++i) {
        std::string categoryName = "Category" + std::to_string(i);
        for (int j = 0; j < 50; ++j) {
            std::string propName = "Property" + std::to_string(j);
            ASSERT_TRUE(cfg.SetIntegerProperty(categoryName, propName, i * 100 + j));
        }
    }

    EXPECT_EQ(cfg.GetCategoryCount(), 100);

    // Verify data integrity
    for (int i = 0; i < 100; ++i) {
        std::string categoryName = "Category" + std::to_string(i);
        EXPECT_TRUE(cfg.HasCategory(categoryName));

        for (int j = 0; j < 50; ++j) {
            std::string propName = "Property" + std::to_string(j);
            EXPECT_EQ(cfg.GetIntegerProperty(categoryName, propName), i * 100 + j);
        }
    }
}

TEST_F(CfgFileTest, HandlesLargeComments) {
    CfgFile cfg;
    ASSERT_TRUE(cfg.SetStringProperty("Test", "Prop", "Value"));

    std::string largeComment(5000, 'C');
    ASSERT_TRUE(cfg.SetPropertyComment("Test", "Prop", largeComment));
    EXPECT_EQ(cfg.GetPropertyComment("Test", "Prop"), largeComment);

    std::string largeCategoryComment(5000, 'T');
    ASSERT_TRUE(cfg.SetCategoryComment("Test", largeCategoryComment));
    EXPECT_EQ(cfg.GetCategoryComment("Test"), largeCategoryComment);
}

TEST_F(CfgFileTest, HandlesComplexNestedContent) {
    const std::string content = R"CFG(
# Top level comment
Graphics {
    # Display configuration
    I Width 1920
    I Height 1080
    B Fullscreen true
    F Gamma 2.2
    S Driver "DirectX 11"
}

Audio {
    # Sound settings
    F Volume 0.8
    I SampleRate 44100
    B Enabled true
    S Device "Default"
}

Controls {
    # Input mappings
    K MoveForward 87   # W key
    K MoveBackward 83  # S key
    K MoveLeft 65      # A key
    K MoveRight 68     # D key
    K Jump 32          # Space
}

Advanced {
    # Advanced settings
    B DebugMode false
    I LogLevel 2
    S ConfigPath "config/advanced.cfg"
    F NetworkTimeout 30.0
}
)CFG";

    CfgFile cfg;
    ASSERT_TRUE(cfg.ParseFromString(content));

    EXPECT_EQ(cfg.GetCategoryCount(), 4);
    EXPECT_TRUE(cfg.HasCategory("Graphics"));
    EXPECT_TRUE(cfg.HasCategory("Audio"));
    EXPECT_TRUE(cfg.HasCategory("Controls"));
    EXPECT_TRUE(cfg.HasCategory("Advanced"));

    // Verify all properties are accessible
    EXPECT_EQ(cfg.GetIntegerProperty("Graphics", "Width"), 1920);
    EXPECT_FLOAT_EQ(cfg.GetFloatProperty("Audio", "Volume"), 0.8f);
    EXPECT_EQ(cfg.GetKeyProperty("Controls", "Jump"), 32);
    EXPECT_FALSE(cfg.GetBooleanProperty("Advanced", "DebugMode"));
}

TEST_F(CfgFileTest, StressTestCompleteRoundTrip) {
    CfgFile original;

    // Create a complex configuration
    for (int i = 0; i < 20; ++i) {
        std::string cat = "Category" + std::to_string(i);

        ASSERT_TRUE(original.SetStringProperty(cat, "String", "Value" + std::to_string(i)));
        ASSERT_TRUE(original.SetIntegerProperty(cat, "Integer", i * 100));
        ASSERT_TRUE(original.SetBooleanProperty(cat, "Boolean", i % 2 == 0));
        ASSERT_TRUE(original.SetFloatProperty(cat, "Float", i * 3.14f));
        ASSERT_TRUE(original.SetKeyProperty(cat, "Key", 65 + i));

        ASSERT_TRUE(original.SetCategoryComment(cat, "Comment for " + cat));
        ASSERT_TRUE(original.SetPropertyComment(cat, "String", "String property comment"));
    }

    // Serialize and deserialize
    const std::string serialized = original.WriteToString();
    EXPECT_FALSE(serialized.empty());

    CfgFile restored;
    ASSERT_TRUE(restored.ParseFromString(serialized));

    // Verify everything matches
    EXPECT_EQ(restored.GetCategoryCount(), 20);

    for (int i = 0; i < 20; ++i) {
        std::string cat = "Category" + std::to_string(i);

        EXPECT_EQ(restored.GetStringProperty(cat, "String"), "Value" + std::to_string(i));
        EXPECT_EQ(restored.GetIntegerProperty(cat, "Integer"), i * 100);
        EXPECT_EQ(restored.GetBooleanProperty(cat, "Boolean"), i % 2 == 0);
        EXPECT_FLOAT_EQ(restored.GetFloatProperty(cat, "Float"), i * 3.14f);
        EXPECT_EQ(restored.GetKeyProperty(cat, "Key"), 65 + i);

        EXPECT_EQ(restored.GetCategoryComment(cat), "Comment for " + cat);
        EXPECT_EQ(restored.GetPropertyComment(cat, "String"), "String property comment");
    }
}

// Header comment tests
TEST_F(CfgFileTest, HandlesHeaderComments) {
    const std::string content = R"CFG(# BallanceModLoaderPlus Configuration
# This file contains game settings
# Author: Test User

Graphics {
    I Width 1920
    I Height 1080
})CFG";

    CfgFile cfg;
    ASSERT_TRUE(cfg.ParseFromString(content));

    std::string headerComment = cfg.GetHeaderComment();
    EXPECT_FALSE(headerComment.empty());
    EXPECT_NE(headerComment.find("BallanceModLoaderPlus Configuration"), std::string::npos);
    EXPECT_NE(headerComment.find("This file contains game settings"), std::string::npos);
    EXPECT_NE(headerComment.find("Author: Test User"), std::string::npos);
}

TEST_F(CfgFileTest, SetsAndGetsHeaderComments) {
    CfgFile cfg;
    ASSERT_TRUE(cfg.SetStringProperty("Test", "Value", "123"));

    // Test setting header comment
    std::string comment = "Configuration File\nGenerated automatically\nDo not edit manually";
    ASSERT_TRUE(cfg.SetHeaderComment(comment));

    std::string retrieved = cfg.GetHeaderComment();
    EXPECT_NE(retrieved.find("Configuration File"), std::string::npos);
    EXPECT_NE(retrieved.find("Generated automatically"), std::string::npos);
    EXPECT_NE(retrieved.find("Do not edit manually"), std::string::npos);
}

TEST_F(CfgFileTest, HeaderCommentRoundTrip) {
    CfgFile original;
    ASSERT_TRUE(original.SetStringProperty("Graphics", "Width", "1920"));
    ASSERT_TRUE(original.SetHeaderComment("Configuration Header\nLine 2\nLine 3"));

    std::string serialized = original.WriteToString();
    EXPECT_NE(serialized.find("# Configuration Header"), std::string::npos);
    EXPECT_NE(serialized.find("# Line 2"), std::string::npos);
    EXPECT_NE(serialized.find("# Line 3"), std::string::npos);

    CfgFile restored;
    ASSERT_TRUE(restored.ParseFromString(serialized));

    std::string restoredComment = restored.GetHeaderComment();
    EXPECT_NE(restoredComment.find("Configuration Header"), std::string::npos);
    EXPECT_NE(restoredComment.find("Line 2"), std::string::npos);
    EXPECT_NE(restoredComment.find("Line 3"), std::string::npos);
}

TEST_F(CfgFileTest, ClearHeaderComment) {
    CfgFile cfg;
    ASSERT_TRUE(cfg.SetHeaderComment("Initial comment"));
    EXPECT_FALSE(cfg.GetHeaderComment().empty());

    cfg.ClearHeaderComment();
    EXPECT_TRUE(cfg.GetHeaderComment().empty());
}

TEST_F(CfgFileTest, HeaderCommentFormattingWithoutPrefix) {
    CfgFile cfg;
    ASSERT_TRUE(cfg.SetHeaderComment("Line without prefix\nAnother line"));

    std::string comment = cfg.GetHeaderComment();
    EXPECT_NE(comment.find("# Line without prefix"), std::string::npos);
    EXPECT_NE(comment.find("# Another line"), std::string::npos);
}
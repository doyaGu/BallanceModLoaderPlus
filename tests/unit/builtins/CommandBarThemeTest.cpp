#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "BML/IConfig.h"
#include "Console/CommandBarTheme.h"

namespace {
std::unordered_map<const IProperty *, BML_ConfigPropertyEditor> g_PropertyEditors;
}

BML_ConfigPropertyEditor BML_GetConfigPropertyEditor(const IProperty *property) {
    const auto entry = g_PropertyEditors.find(property);
    return entry == g_PropertyEditors.end() ? BML_CONFIG_EDITOR_DEFAULT : entry->second;
}

int BML_SetConfigPropertyEditor(IProperty *property, BML_ConfigPropertyEditor editor) {
    if (!property || (editor != BML_CONFIG_EDITOR_DEFAULT &&
                      editor != BML_CONFIG_EDITOR_COLOR))
        return 0;
    g_PropertyEditors[property] = editor;
    return 1;
}

namespace {

using CommandBarTheme::Color;

class TestProperty final : public IProperty {
public:
    const char *GetString() override { return m_Type == STRING ? m_String.c_str() : ""; }
    bool GetBoolean() override { return false; }
    int GetInteger() override { return 0; }
    float GetFloat() override { return 0.0f; }
    CKKEYBOARD GetKey() override { return static_cast<CKKEYBOARD>(0); }

    void SetString(const char *value) override {
        m_Type = STRING;
        m_String = value ? value : "";
    }
    void SetBoolean(bool) override { m_Type = BOOLEAN; }
    void SetInteger(int) override { m_Type = INTEGER; }
    void SetFloat(float) override { m_Type = FLOAT; }
    void SetKey(CKKEYBOARD) override { m_Type = KEY; }

    void SetComment(const char *comment) override { m_Comment = comment ? comment : ""; }
    void SetDefaultString(const char *value) override {
        if (m_Type != STRING)
            SetString(value);
    }
    void SetDefaultBoolean(bool value) override {
        if (m_Type != BOOLEAN)
            SetBoolean(value);
    }
    void SetDefaultInteger(int value) override {
        if (m_Type != INTEGER)
            SetInteger(value);
    }
    void SetDefaultFloat(float value) override {
        if (m_Type != FLOAT)
            SetFloat(value);
    }
    void SetDefaultKey(CKKEYBOARD value) override {
        if (m_Type != KEY)
            SetKey(value);
    }

    PropertyType GetType() override { return m_Type; }
private:
    PropertyType m_Type = NONE;
    std::string m_String;
    std::string m_Comment;
};

class TestConfig final : public IConfig {
public:
    bool HasCategory(const char *category) override {
        return category && category == m_Category;
    }
    bool HasKey(const char *category, const char *key) override {
        return category && key && category == m_Category && m_Properties.contains(key);
    }
    IProperty *GetProperty(const char *category, const char *key) override {
        if (!category || !key)
            return nullptr;
        m_Category = category;
        auto [entry, inserted] = m_Properties.try_emplace(key);
        if (inserted)
            entry->second = std::make_unique<TestProperty>();
        return entry->second.get();
    }
    void SetCategoryComment(const char *category, const char *comment) override {
        m_Category = category ? category : "";
        m_CategoryComment = comment ? comment : "";
    }

    TestProperty *Property(const char *key) {
        const auto entry = m_Properties.find(key);
        return entry == m_Properties.end() ? nullptr : entry->second.get();
    }
    std::size_t PropertyCount() const { return m_Properties.size(); }

private:
    std::string m_Category;
    std::string m_CategoryComment;
    std::unordered_map<std::string, std::unique_ptr<TestProperty>> m_Properties;
};

TEST(CommandBarTheme, OneDarkUsesExpectedRoleColours) {
    constexpr CommandBarTheme::SyntaxPalette palette = CommandBarTheme::OneDark();

    EXPECT_EQ((Color{0xAB, 0xB2, 0xBF, 0xFF}), palette.plain);
    EXPECT_EQ((Color{0x61, 0xAF, 0xEF, 0xFF}), palette.commandValid);
    EXPECT_EQ((Color{0xD1, 0x9A, 0x66, 0xFF}), palette.commandInvalid);
    EXPECT_EQ((Color{0x98, 0xC3, 0x79, 0xFF}), palette.string);
    EXPECT_EQ((Color{0xE5, 0xC0, 0x7B, 0xFF}), palette.variable);
    EXPECT_EQ((Color{0xC6, 0x78, 0xDD, 0xFF}), palette.op);
    EXPECT_EQ((Color{0x5C, 0x63, 0x70, 0xFF}), palette.comment);
    EXPECT_EQ((Color{0xE0, 0x6C, 0x75, 0xFF}), palette.error);
}

TEST(CommandBarTheme, SettingsOwnRoleSchemaAndReadPalette) {
    g_PropertyEditors.clear();
    TestConfig config;
    CommandBarTheme::Settings settings;
    settings.Define(config);

    EXPECT_EQ(CommandBarTheme::Settings::RoleCount, config.PropertyCount());
    ASSERT_NE(nullptr, config.Property("Plain"));
    EXPECT_EQ(BML_CONFIG_EDITOR_COLOR,
              BML_GetConfigPropertyEditor(config.Property("Plain")));
    EXPECT_EQ(CommandBarTheme::OneDark(), settings.ReadPalette());
    EXPECT_TRUE(settings.Owns("CommandBarTheme", "Plain", config.Property("Plain")));
    EXPECT_FALSE(settings.Owns("CommandBar", "Plain", config.Property("Plain")));

    config.Property("Plain")->SetString("#01020380");
    config.Property("Error")->SetString("invalid");
    const CommandBarTheme::SyntaxPalette palette = settings.ReadPalette();
    EXPECT_EQ((Color{0x01, 0x02, 0x03, 0x80}), palette.plain);
    EXPECT_EQ(CommandBarTheme::OneDark().error, palette.error);
}

} // namespace

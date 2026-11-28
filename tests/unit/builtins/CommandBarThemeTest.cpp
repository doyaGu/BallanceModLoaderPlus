#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "Config/Config.h"
#include "Console/CommandBarTheme.h"

namespace {
using CommandBarTheme::Color;
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
    Config config(nullptr);
    CommandBarTheme::Settings settings;
    settings.Define(config);

    EXPECT_EQ(CommandBarTheme::Settings::RoleCount,
              config.GetCategory("CommandBarTheme")->GetPropertyCount());
    ASSERT_NE(nullptr, config.GetProperty("CommandBarTheme", "Plain"));
    EXPECT_EQ(BML_CONFIG_EDITOR_COLOR,
              BML::ConfigMetadata::GetEditor(config.GetProperty("CommandBarTheme", "Plain")));
    EXPECT_EQ(CommandBarTheme::OneDark(), settings.ReadPalette());
    EXPECT_TRUE(
        settings.Owns("CommandBarTheme", "Plain", config.GetProperty("CommandBarTheme", "Plain")));
    EXPECT_FALSE(
        settings.Owns("CommandBar", "Plain", config.GetProperty("CommandBarTheme", "Plain")));

    config.GetProperty("CommandBarTheme", "Plain")->SetString("#01020380");
    config.GetProperty("CommandBarTheme", "Error")->SetString("invalid");
    const CommandBarTheme::SyntaxPalette palette = settings.ReadPalette();
    EXPECT_EQ((Color{0x01, 0x02, 0x03, 0x80}), palette.plain);
    EXPECT_EQ(CommandBarTheme::OneDark().error, palette.error);
}

} // namespace

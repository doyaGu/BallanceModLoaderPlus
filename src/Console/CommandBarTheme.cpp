#include "Console/CommandBarTheme.h"

#include <array>
#include <cstring>
#include <optional>
#include <string>

#include "BML/IConfig.h"

namespace CommandBarTheme {
namespace {

constexpr char Category[] = "CommandBarTheme";
constexpr SyntaxPalette DefaultPalette = OneDark();

struct RoleDefinition {
    const char *key;
    Color SyntaxPalette::*member;
    Color defaultColor;
    const char *comment;
};

constexpr std::array<RoleDefinition, Settings::RoleCount> Roles{{
    {"Plain", &SyntaxPalette::plain, DefaultPalette.plain,
     "Plain input text colour as #RRGGBB or #RRGGBBAA (One Dark foreground)."},
    {"CommandValid", &SyntaxPalette::commandValid, DefaultPalette.commandValid,
     "Registered command and alias colour (One Dark blue)."},
    {"CommandInvalid", &SyntaxPalette::commandInvalid, DefaultPalette.commandInvalid,
     "Unknown command colour (One Dark orange)."},
    {"String", &SyntaxPalette::string, DefaultPalette.string,
     "Quoted string colour (One Dark green)."},
    {"Variable", &SyntaxPalette::variable, DefaultPalette.variable,
     "Variable expansion and command substitution colour (One Dark yellow)."},
    {"Operator", &SyntaxPalette::op, DefaultPalette.op,
     "Pipe, chain, separator, and newline colour (One Dark purple)."},
    {"Comment", &SyntaxPalette::comment, DefaultPalette.comment,
     "Shell comment colour (One Dark muted foreground)."},
    {"Error", &SyntaxPalette::error, DefaultPalette.error,
     "Malformed syntax colour (One Dark red)."},
}};

} // namespace

void Settings::Define(IConfig &config) {
    for (std::size_t index = 0; index < Roles.size(); ++index) {
        const RoleDefinition &role = Roles[index];
        IProperty *property = config.GetProperty(Category, role.key);
        m_Properties[index] = property;
        if (!property)
            continue;

        const std::string defaultValue = UiColor::FormatHex(role.defaultColor);
        property->SetComment(role.comment);
        property->SetDefaultString(defaultValue.c_str());
        BML_SetConfigPropertyEditor(property, BML_CONFIG_EDITOR_COLOR);
    }
    config.SetCategoryComment(Category, "Command Bar Syntax Theme");
}

bool Settings::Owns(const char *category, const char *key,
                    const IProperty *property) const {
    if (!category || !key || !property || std::strcmp(category, Category) != 0)
        return false;

    for (std::size_t index = 0; index < Roles.size(); ++index) {
        if (m_Properties[index] == property && std::strcmp(key, Roles[index].key) == 0)
            return true;
    }
    return false;
}

SyntaxPalette Settings::ReadPalette() const {
    SyntaxPalette palette = DefaultPalette;
    for (std::size_t index = 0; index < Roles.size(); ++index) {
        IProperty *property = m_Properties[index];
        if (!property)
            continue;
        const std::optional<Color> parsed = UiColor::ParseHex(property->GetString());
        if (parsed)
            palette.*Roles[index].member = *parsed;
    }
    return palette;
}

} // namespace CommandBarTheme

#ifndef BML_COMMANDBAR_THEME_H
#define BML_COMMANDBAR_THEME_H

#include <array>

#include "UI/ColorCodec.h"

class IConfig;
class IProperty;

namespace CommandBarTheme {

using Color = UiColor::Rgba8;

struct SyntaxPalette {
    Color plain;
    Color commandValid;
    Color commandInvalid;
    Color string;
    Color variable;
    Color op;
    Color comment;
    Color error;

    bool operator==(const SyntaxPalette &) const = default;
};

// Atom One Dark's foreground and seven accent colours, mapped to the shell
// roles the command bar can distinguish.
constexpr SyntaxPalette OneDark() {
    return {
        {0xAB, 0xB2, 0xBF, 0xFF}, // foreground
        {0x61, 0xAF, 0xEF, 0xFF}, // blue
        {0xD1, 0x9A, 0x66, 0xFF}, // orange
        {0x98, 0xC3, 0x79, 0xFF}, // green
        {0xE5, 0xC0, 0x7B, 0xFF}, // yellow
        {0xC6, 0x78, 0xDD, 0xFF}, // purple
        {0x5C, 0x63, 0x70, 0xFF}, // muted foreground
        {0xE0, 0x6C, 0x75, 0xFF}, // red
    };
}

// Owns the persisted STRING properties for the syntax roles and adapts them
// into the typed palette consumed by CommandBar. Invalid strings fall back per
// role, so one malformed value never invalidates the rest of the palette.
class Settings {
public:
    static constexpr std::size_t RoleCount = 8;

    void Define(IConfig &config);
    bool Owns(const char *category, const char *key, const IProperty *property) const;
    SyntaxPalette ReadPalette() const;

private:
    std::array<IProperty *, RoleCount> m_Properties{};
};

} // namespace CommandBarTheme

#endif // BML_COMMANDBAR_THEME_H

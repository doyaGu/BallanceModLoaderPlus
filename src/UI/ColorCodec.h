#ifndef BML_UI_COLOR_CODEC_H
#define BML_UI_COLOR_CODEC_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace UiColor {

struct Rgba8 {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    std::uint8_t alpha = 255;

    bool operator==(const Rgba8 &) const = default;
};

// Accepts #RRGGBB and #RRGGBBAA (or the same forms without '#'). Leading
// and trailing ASCII whitespace is ignored; every other form is rejected.
std::optional<Rgba8> ParseHex(std::string_view text);
std::string FormatHex(Rgba8 color, bool includeAlpha = false);

} // namespace UiColor

#endif // BML_UI_COLOR_CODEC_H

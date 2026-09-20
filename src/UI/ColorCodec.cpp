#include "UI/ColorCodec.h"

#include <array>
#include <cctype>

namespace UiColor {
namespace {

int HexDigit(char value) {
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    return -1;
}

std::optional<std::uint8_t> ParseByte(std::string_view text, std::size_t offset) {
    const int high = HexDigit(text[offset]);
    const int low = HexDigit(text[offset + 1]);
    if (high < 0 || low < 0)
        return std::nullopt;
    return static_cast<std::uint8_t>((high << 4) | low);
}

} // namespace

std::optional<Rgba8> ParseHex(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0)
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
        text.remove_suffix(1);
    if (!text.empty() && text.front() == '#')
        text.remove_prefix(1);
    if (text.size() != 6 && text.size() != 8)
        return std::nullopt;

    std::array<std::uint8_t, 4> channels{0, 0, 0, 0xFF};
    const std::size_t channelCount = text.size() / 2;
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        const std::optional<std::uint8_t> value = ParseByte(text, channel * 2);
        if (!value)
            return std::nullopt;
        channels[channel] = *value;
    }
    return Rgba8{channels[0], channels[1], channels[2], channels[3]};
}

std::string FormatHex(Rgba8 color, bool includeAlpha) {
    constexpr char Digits[] = "0123456789ABCDEF";
    const std::array<std::uint8_t, 4> channels{
        color.red, color.green, color.blue, color.alpha,
    };
    const std::size_t channelCount = includeAlpha ? 4 : 3;
    std::string text(1 + channelCount * 2, '#');
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        text[1 + channel * 2] = Digits[channels[channel] >> 4];
        text[2 + channel * 2] = Digits[channels[channel] & 0x0F];
    }
    return text;
}

} // namespace UiColor

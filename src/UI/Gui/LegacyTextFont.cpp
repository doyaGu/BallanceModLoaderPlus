#include "UI/Gui/LegacyTextFont.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <unordered_map>

#include "CKSpriteText.h"

namespace BGui {
namespace {

struct LegacyTextStyle {
    std::string Face;
    int Size = 0;
    int Weight = 400;
    bool Italic = false;
    bool Underline = false;
};

constexpr std::array<std::string_view, 3> PreferredFaces = {
    "Microsoft YaHei UI",
    "Microsoft YaHei",
    "Segoe UI",
};

std::string g_DefaultFace;
std::unordered_map<CKSpriteText *, LegacyTextStyle> g_CustomStyles;

int DefaultSize(int viewportHeight) {
    return (std::max)(viewportHeight / 85, 1);
}

void Apply(CKSpriteText *sprite, const LegacyTextStyle &style) {
    if (!sprite)
        return;
    sprite->SetFont((CKSTRING) style.Face.c_str(), style.Size, style.Weight, style.Italic, style.Underline);
}

void ApplyDefault(CKSpriteText *sprite, int viewportHeight) {
    Apply(sprite, {g_DefaultFace, DefaultSize(viewportHeight), 400, false, false});
}

} // namespace

std::string ChooseLegacyTextDefaultFace(const std::vector<std::string> &availableFaces) {
    for (const std::string_view preferred : PreferredFaces) {
        const auto found = std::find(availableFaces.begin(), availableFaces.end(), preferred);
        if (found != availableFaces.end())
            return *found;
    }
    return {};
}

void SelectLegacyTextDefaultFace(const std::vector<std::string> &availableFaces) {
    g_DefaultFace = ChooseLegacyTextDefaultFace(availableFaces);
}

void InitializeLegacyTextFont(CKSpriteText *sprite, int viewportHeight) {
    if (!sprite)
        return;
    g_CustomStyles.erase(sprite);
    ApplyDefault(sprite, viewportHeight);
}

void SetLegacyTextFont(CKSpriteText *sprite, const char *face, int size, int weight, bool italic, bool underline) {
    if (!sprite)
        return;
    LegacyTextStyle &style = g_CustomStyles[sprite];
    style.Face = face ? face : "";
    style.Size = size;
    style.Weight = weight;
    style.Italic = italic;
    style.Underline = underline;
    Apply(sprite, style);
}

void RefreshLegacyTextFont(CKSpriteText *sprite, int viewportHeight) {
    if (!sprite)
        return;
    const auto found = g_CustomStyles.find(sprite);
    if (found != g_CustomStyles.end())
        Apply(sprite, found->second);
    else
        ApplyDefault(sprite, viewportHeight);
}

void ForgetLegacyTextFont(CKSpriteText *sprite) {
    if (sprite)
        g_CustomStyles.erase(sprite);
}

} // namespace BGui

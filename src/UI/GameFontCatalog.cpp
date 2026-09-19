#include "UI/GameFontCatalog.h"

#include <utility>

namespace BML {

namespace {

constexpr std::array<std::pair<std::string_view, GameFont>, 7> RuntimeFonts = {{
    {"GameFont_01", GameFont::Normal},
    {"GameFont_02", GameFont::Large},
    {"GameFont_03", GameFont::Small},
    {"GameFont_03a", GameFont::SmallGray},
    {"GameFont_04", GameFont::Huge},
    {"GameFont_Credits_Small", GameFont::CreditsSmall},
    {"GameFont_Credits_Big", GameFont::CreditsBig},
}};

static_assert(RuntimeFonts.size() + 1 ==
                  static_cast<std::size_t>(GameFont::Count),
              "Every Game Font role except None needs one runtime name");

} // namespace

GameFontCatalog::GameFontCatalog() {
    Reset();
}

void GameFontCatalog::Reset() {
    for (std::size_t i = 0; i < m_Fonts.size(); ++i) {
        m_Fonts[i] = static_cast<int>(i);
        m_Bound[i] = false;
    }
}

bool GameFontCatalog::Bind(GameFont font, int virtoolsIndex) {
    const std::size_t index = static_cast<std::size_t>(font);
    if (font == GameFont::None || index >= m_Fonts.size() || virtoolsIndex <= 0)
        return false;

    for (std::size_t i = 1; i < m_Fonts.size(); ++i) {
        if (i != index && m_Bound[i] && m_Fonts[i] == virtoolsIndex)
            return false;
    }

    m_Fonts[index] = virtoolsIndex;
    m_Bound[index] = true;
    return true;
}

bool GameFontCatalog::Bind(std::string_view runtimeName, int virtoolsIndex,
                           GameFont *boundRole) {
    for (const auto &[name, font] : RuntimeFonts) {
        if (runtimeName == name) {
            if (!Bind(font, virtoolsIndex))
                return false;
            if (boundRole)
                *boundRole = font;
            return true;
        }
    }
    return false;
}

int GameFontCatalog::Resolve(GameFont font) const {
    const std::size_t index = static_cast<std::size_t>(font);
    return index < m_Fonts.size() ? m_Fonts[index] : m_Fonts[0];
}

GameFont GameFontCatalog::Identify(int virtoolsIndex) const {
    if (virtoolsIndex <= 0)
        return GameFont::None;

    for (std::size_t i = 1; i < m_Fonts.size(); ++i) {
        if (m_Bound[i] && m_Fonts[i] == virtoolsIndex)
            return static_cast<GameFont>(i);
    }
    for (std::size_t i = 1; i < m_Fonts.size(); ++i) {
        if (!m_Bound[i] && m_Fonts[i] == virtoolsIndex)
            return static_cast<GameFont>(i);
    }
    return GameFont::None;
}

} // namespace BML

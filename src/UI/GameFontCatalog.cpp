#include "UI/GameFontCatalog.h"

namespace BML {

GameFontCatalog::GameFontCatalog() {
    Reset();
}

void GameFontCatalog::Reset() {
    for (std::size_t i = 0; i < m_Fonts.size(); ++i)
        m_Fonts[i] = static_cast<int>(i);
}

bool GameFontCatalog::Bind(GameFont font, int virtoolsIndex) {
    const std::size_t index = static_cast<std::size_t>(font);
    if (index >= m_Fonts.size())
        return false;
    m_Fonts[index] = virtoolsIndex;
    return true;
}

int GameFontCatalog::Resolve(GameFont font) const {
    const std::size_t index = static_cast<std::size_t>(font);
    return index < m_Fonts.size() ? m_Fonts[index] : m_Fonts[0];
}

GameFont GameFontCatalog::Identify(int virtoolsIndex) const {
    for (std::size_t i = 0; i < m_Fonts.size(); ++i) {
        if (m_Fonts[i] == virtoolsIndex)
            return static_cast<GameFont>(i);
    }
    return GameFont::None;
}

} // namespace BML

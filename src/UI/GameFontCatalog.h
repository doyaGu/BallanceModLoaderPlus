#ifndef BML_GAMEFONTCATALOG_H
#define BML_GAMEFONTCATALOG_H

#include <array>
#include <cstddef>

namespace BML {

enum class GameFont : std::size_t {
    None,
    Normal,
    Large,
    Small,
    SmallGray,
    Huge,
    CreditsSmall,
    CreditsBig,
    Count,
};

class GameFontCatalog {
public:
    GameFontCatalog();

    void Reset();
    bool Bind(GameFont font, int virtoolsIndex);
    int Resolve(GameFont font) const;
    GameFont Identify(int virtoolsIndex) const;

private:
    static constexpr std::size_t FontCount = static_cast<std::size_t>(GameFont::Count);
    std::array<int, FontCount> m_Fonts{};
};

} // namespace BML

#endif // BML_GAMEFONTCATALOG_H

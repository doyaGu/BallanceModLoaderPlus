#ifndef BML_GAME_SESSION_H
#define BML_GAME_SESSION_H

#include <cstdint>

namespace BML {

enum class GamePhase : std::uint8_t {
    FrontEnd,
    Transitioning,
    LevelActive,
    LevelPaused,
};

struct GameSessionSnapshot {
    GamePhase Phase = GamePhase::FrontEnd;

    [[nodiscard]] constexpr bool IsInGame() const noexcept {
        return Phase != GamePhase::FrontEnd;
    }

    [[nodiscard]] constexpr bool IsInLevel() const noexcept {
        return Phase == GamePhase::LevelActive || Phase == GamePhase::LevelPaused;
    }

    [[nodiscard]] constexpr bool IsPaused() const noexcept {
        return Phase == GamePhase::LevelPaused;
    }

    [[nodiscard]] constexpr bool IsPlaying() const noexcept {
        return Phase == GamePhase::LevelActive;
    }
};

class GameSession final {
public:
    [[nodiscard]] constexpr GameSessionSnapshot Read() const noexcept { return {m_Phase}; }

    void ActivateLevel() noexcept;
    void BeginTransition() noexcept;
    void ReturnToFrontEnd() noexcept;
    void PauseLevel() noexcept;
    void ResumeLevel() noexcept;

private:
    GamePhase m_Phase = GamePhase::FrontEnd;
};

} // namespace BML

#endif // BML_GAME_SESSION_H

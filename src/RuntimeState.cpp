#include "RuntimeState.h"

namespace BML {

RuntimeStateSnapshot RuntimeState::Read() const noexcept {
    const bool inGame = IsSet(InGame);
    const bool paused = IsSet(Paused);
    return {
        inGame,
        IsSet(LevelActive) && !paused,
        paused,
        inGame && !paused,
        IsSet(CheatEnabled),
    };
}

void RuntimeState::Apply(RuntimeStateTransition transition) noexcept {
    switch (transition) {
        case RuntimeStateTransition::EnterLevel:
            Set(InGame, true);
            Set(LevelActive, true);
            Set(Paused, false);
            break;
        case RuntimeStateTransition::LeaveLevel:
            Set(LevelActive, false);
            break;
        case RuntimeStateTransition::LeaveGame:
            Set(InGame, false);
            Set(LevelActive, false);
            break;
        case RuntimeStateTransition::Pause:
            Set(Paused, true);
            break;
        case RuntimeStateTransition::Resume:
            Set(Paused, false);
            break;
    }
}

bool RuntimeState::SetCheatEnabled(bool enabled) noexcept {
    const bool changed = IsSet(CheatEnabled) != enabled;
    Set(CheatEnabled, enabled);
    return changed;
}

bool RuntimeState::IsSet(Bit bit) const noexcept {
    return (m_Bits & bit) != 0;
}

void RuntimeState::Set(Bit bit, bool enabled) noexcept {
    if (enabled)
        m_Bits |= bit;
    else
        m_Bits &= static_cast<std::uint8_t>(~bit);
}

} // namespace BML

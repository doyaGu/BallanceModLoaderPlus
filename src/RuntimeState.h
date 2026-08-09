#ifndef BML_RUNTIME_STATE_H
#define BML_RUNTIME_STATE_H

#include <cstdint>

namespace BML {

struct RuntimeStateSnapshot {
    bool InGame = false;
    bool InLevel = false;
    bool Paused = false;
    bool Playing = false;
    bool CheatEnabled = false;
};

enum class RuntimeStateTransition : std::uint8_t {
    EnterLevel,
    LeaveLevel,
    LeaveGame,
    Pause,
    Resume,
};

class RuntimeState final {
public:
    [[nodiscard]] RuntimeStateSnapshot Read() const noexcept;
    void Apply(RuntimeStateTransition transition) noexcept;
    [[nodiscard]] bool SetCheatEnabled(bool enabled) noexcept;

private:
    enum Bit : std::uint8_t {
        InGame = 1u << 0u,
        LevelActive = 1u << 1u,
        Paused = 1u << 2u,
        CheatEnabled = 1u << 3u,
    };

    bool IsSet(Bit bit) const noexcept;
    void Set(Bit bit, bool enabled) noexcept;

    std::uint8_t m_Bits = 0;
};

} // namespace BML

#endif // BML_RUNTIME_STATE_H

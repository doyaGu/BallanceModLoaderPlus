#pragma once

#include "CKInputManager.h"

#include <cstdint>

namespace BML::PlayerTest {

// The direction keys a test drives. The mask is what the shipped Ball
// Navigation script reads, so scenarios and the pilot both speak in it.
enum GameplayKey : std::uint32_t {
    GameplayKeyNone = 0,
    GameplayKeyLeft = 1u << 0,
    GameplayKeyRight = 1u << 1,
    GameplayKeyUp = 1u << 2,
    GameplayKeyDown = 1u << 3,
};

// Test-only input source. It applies the requested direction keys immediately
// after CKInputManager has sampled the physical keyboard, which is the state
// consumed by Virtools Key Event blocks during the same game frame.
class GameplayInputDriver final {
public:
    GameplayInputDriver() = default;
    ~GameplayInputDriver();

    GameplayInputDriver(const GameplayInputDriver &) = delete;
    GameplayInputDriver &operator=(const GameplayInputDriver &) = delete;

    bool Attach(CKInputManager *manager);
    void Detach();

    void SetEnabled(bool enabled) noexcept;
    void SetMask(std::uint32_t mask) noexcept;

    [[nodiscard]] bool IsAttached() const noexcept { return m_Manager != nullptr; }
    [[nodiscard]] bool IsEnabled() const noexcept { return m_Enabled; }
    [[nodiscard]] std::uint32_t RequestedMask() const noexcept { return m_RequestedMask; }
    [[nodiscard]] std::uint32_t AppliedMask() const noexcept { return m_AppliedMask; }
    [[nodiscard]] std::uint32_t PhysicalMask() const noexcept { return m_PhysicalMask; }
    [[nodiscard]] std::uint64_t Frame() const noexcept { return m_Frame; }

private:
    using PreProcess = CKERROR(__thiscall *)(CKInputManager *);

    static CKERROR __fastcall PreProcessHook(CKInputManager *manager, void *);
    static bool ReplaceSlot(void **slot, void *replacement, void **previous);
    void Apply(CKInputManager *manager);

    static GameplayInputDriver *s_Instance;

    CKInputManager *m_Manager = nullptr;
    void **m_PreProcessSlot = nullptr;
    PreProcess m_PreProcess = nullptr;
    std::uint32_t m_RequestedMask = 0;
    std::uint32_t m_AppliedMask = 0;
    std::uint32_t m_PreviousMask = 0;
    std::uint32_t m_PhysicalMask = 0;
    std::uint64_t m_Frame = 0;
    bool m_Enabled = false;
};

} // namespace BML::PlayerTest

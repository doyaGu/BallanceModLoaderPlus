#include "GameplayInputDriver.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <array>

namespace BML::PlayerTest {
namespace {

// CKBaseManager's first virtual slots are destructor, SaveData, LoadData,
// PreClearAll, PostClearAll, then PreProcess in the CK2.1 ABI.
constexpr std::size_t kPreProcessSlot = 5;

struct DirectionKey {
    std::uint32_t Bit;
    CKKEYBOARD Key;
    int VirtualKey;
};

constexpr std::array<DirectionKey, 4> kDirectionKeys{{
    {GameplayKeyLeft, CKKEY_LEFT, VK_LEFT},
    {GameplayKeyRight, CKKEY_RIGHT, VK_RIGHT},
    {GameplayKeyUp, CKKEY_UP, VK_UP},
    {GameplayKeyDown, CKKEY_DOWN, VK_DOWN},
}};

} // namespace

GameplayInputDriver *GameplayInputDriver::s_Instance = nullptr;

GameplayInputDriver::~GameplayInputDriver() {
    Detach();
}

bool GameplayInputDriver::Attach(CKInputManager *manager) {
    if (manager == m_Manager)
        return true;
    if (!manager || s_Instance)
        return false;

    void **vtable = *reinterpret_cast<void ***>(manager);
    if (!vtable)
        return false;

    void *previous = nullptr;
    void **slot = &vtable[kPreProcessSlot];
    if (!ReplaceSlot(slot, reinterpret_cast<void *>(&PreProcessHook), &previous))
        return false;

    m_Manager = manager;
    m_PreProcessSlot = slot;
    m_PreProcess = reinterpret_cast<PreProcess>(previous);
    s_Instance = this;
    return true;
}

void GameplayInputDriver::Detach() {
    SetEnabled(false);
    if (!m_Manager)
        return;

    if (m_PreProcessSlot &&
        *m_PreProcessSlot == reinterpret_cast<void *>(&PreProcessHook)) {
        void *ignored = nullptr;
        ReplaceSlot(m_PreProcessSlot, reinterpret_cast<void *>(m_PreProcess),
                    &ignored);
    }
    if (s_Instance == this)
        s_Instance = nullptr;
    m_Manager = nullptr;
    m_PreProcessSlot = nullptr;
    m_PreProcess = nullptr;
}

void GameplayInputDriver::SetEnabled(bool enabled) noexcept {
    m_Enabled = enabled;
    if (!enabled)
        m_RequestedMask = 0;
}

void GameplayInputDriver::SetMask(std::uint32_t mask) noexcept {
    m_RequestedMask = mask &
        (GameplayKeyLeft | GameplayKeyRight | GameplayKeyUp | GameplayKeyDown);
}

CKERROR __fastcall GameplayInputDriver::PreProcessHook(
    CKInputManager *manager, void *) {
    GameplayInputDriver *driver = s_Instance;
    if (!driver || !driver->m_PreProcess)
        return CKERR_INVALIDPARAMETER;

    const CKERROR result = driver->m_PreProcess(manager);
    if (result == CK_OK && driver->m_Enabled)
        driver->Apply(manager);
    return result;
}

bool GameplayInputDriver::ReplaceSlot(void **slot, void *replacement,
                                      void **previous) {
    if (!slot || !replacement)
        return false;
    DWORD protection = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_EXECUTE_READWRITE, &protection))
        return false;
    if (previous)
        *previous = *slot;
    *slot = replacement;
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(*slot), protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
    return true;
}

void GameplayInputDriver::Apply(CKInputManager *manager) {
    unsigned char *state = manager ? manager->GetKeyboardState() : nullptr;
    if (!state)
        return;

    std::uint32_t physical = 0;
    for (const DirectionKey &binding : kDirectionKeys) {
        // The CK buffer also contains the state this driver wrote during the
        // previous frame. Query the device independently so that a held
        // synthetic key is not mislabeled as human interference.
        if ((GetAsyncKeyState(binding.VirtualKey) & 0x8000) != 0)
            physical |= binding.Bit;
    }
    m_PhysicalMask = physical;

    for (const DirectionKey &binding : kDirectionKeys) {
        const bool requested = (m_RequestedMask & binding.Bit) != 0;
        const bool previouslyApplied = (m_PreviousMask & binding.Bit) != 0;
        state[binding.Key] = requested ? KS_PRESSED
            : previouslyApplied ? KS_RELEASED : KS_IDLE;
    }
    m_PreviousMask = m_RequestedMask;
    m_AppliedMask = m_RequestedMask;
    ++m_Frame;
}

} // namespace BML::PlayerTest

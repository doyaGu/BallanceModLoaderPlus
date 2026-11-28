#include "Hooks/InputHook.h"

#include <cassert>
#include <cstring>
#include <cstdio>
#include <unordered_map>

#include <SDL3/SDL_mouse.h>

#include "Hooks/HookLifecycle.h"
#include "Hooks/InputCursor.h"
#include "Hooks/InputAccess.h"
#include <climits>
#include "Hooks/InputVTable.h"
#include "Hooks/VTablePatch.h"

// The engine owns acquisition and schedules PostProcess. While BML is running,
// defer input cleanup until its later PostProcess has finished Mod callbacks.
struct InputHook::Impl {
    bool OwnsHook = false;

    struct OriginalMethods {
        decltype(&CKInputManager::PostProcess) PostProcess{};
        decltype(&CKInputManager::IsKeyDown) IsKeyDown{};
        decltype(&CKInputManager::IsKeyUp) IsKeyUp{};
        decltype(&CKInputManager::IsKeyToggled) IsKeyToggled{};
        decltype(&CKInputManager::GetKeyboardState) GetKeyboardState{};
        decltype(&CKInputManager::GetNumberOfKeyInBuffer) GetNumberOfKeyInBuffer{};
        decltype(&CKInputManager::GetKeyFromBuffer) GetKeyFromBuffer{};
        decltype(&CKInputManager::IsMouseButtonDown) IsMouseButtonDown{};
        decltype(&CKInputManager::IsMouseClicked) IsMouseClicked{};
        decltype(&CKInputManager::IsMouseToggled) IsMouseToggled{};
        decltype(&CKInputManager::GetMouseButtonsState) GetMouseButtonsState{};
        decltype(&CKInputManager::GetMousePosition) GetMousePosition{};
        decltype(&CKInputManager::GetMouseRelativePosition) GetMouseRelativePosition{};
        decltype(&CKInputManager::GetJoystickPosition) GetJoystickPosition{};
        decltype(&CKInputManager::GetJoystickRotation) GetJoystickRotation{};
        decltype(&CKInputManager::GetJoystickSliders) GetJoystickSliders{};
        decltype(&CKInputManager::GetJoystickPointOfViewAngle) GetJoystickPointOfViewAngle{};
        decltype(&CKInputManager::GetJoystickButtonsState) GetJoystickButtonsState{};
        decltype(&CKInputManager::IsJoystickButtonDown) IsJoystickButtonDown{};
        decltype(&CKInputManager::ShowCursor) ShowCursor{};
        decltype(&CKInputManager::OnCKEnd) OnCKEnd{};
    };
    static OriginalMethods s_Original;
    static inline VTablePatch s_Patch;


    static inline CKInputManager *s_InputManager = nullptr;
    static inline Impl *s_Owner = nullptr;
    static inline unsigned char s_LastKeyboardState[256] = {};
    static inline unsigned char s_EmptyKeyboardState[256] = {};
    static inline Vx2DVector s_LastMousePosition;
    static inline int s_BlockedDevice[CK_INPUT_DEVICE_COUNT] = {};
    static inline uint64_t s_NextBlockToken = 1;
    static inline std::unordered_map<uint64_t, uint32_t> s_BlockTokens;
    static inline bool s_OverlayVisible = false;
    static inline bool s_DeferPostProcess = false;
    static inline bool s_PostProcessPending = false;
    static inline uint64_t s_Revision = 1;

    static void CompletePostProcess() {
        if (!s_PostProcessPending)
            return;
        s_PostProcessPending = false;
        (s_InputManager->*s_Original.PostProcess)();
        ++s_Revision;
    }

    static int IsBlocked(CK_INPUT_DEVICE device) {
        return device >= 0 && device < CK_INPUT_DEVICE_COUNT ? s_BlockedDevice[device] : 0;
    }

    static void Block(CK_INPUT_DEVICE device) {
        if (!s_InputManager || device < 0 || device >= CK_INPUT_DEVICE_COUNT)
            return;
        if (s_BlockedDevice[device] < INT_MAX) ++s_BlockedDevice[device];
    }

    static void Unblock(CK_INPUT_DEVICE device) {
        if (!s_InputManager || device < 0 || device >= CK_INPUT_DEVICE_COUNT ||
            s_BlockedDevice[device] == 0)
            return;
        --s_BlockedDevice[device];
    }

    static void ApplyCursorVisibility() {
        // Keep the engine's latest game request intact. SDL's platform backend
        // and this overlay request affect only the displayed system cursor.
        if (!s_InputManager)
            return;
        if (s_OverlayVisible || s_InputManager->GetCursorVisibility())
            SDL_ShowCursor();
        else
            SDL_HideCursor();
    }

    static void SetOverlayCursorVisible(bool visible) {
        if (!s_OverlayVisible && !visible)
            return;
        s_OverlayVisible = visible;
        ApplyCursorVisibility();
    }

    static uint32_t NormalizeBlockMask(uint32_t mask) {
        return mask & InputHook::INPUT_BLOCK_ALL;
    }

    static void ApplyBlockMask(uint32_t mask, bool block) {
        if (mask & InputHook::INPUT_BLOCK_KEYBOARD) {
            if (block)
                Block(CK_INPUT_DEVICE_KEYBOARD);
            else
                Unblock(CK_INPUT_DEVICE_KEYBOARD);
        }
        if (mask & InputHook::INPUT_BLOCK_MOUSE) {
            if (block)
                Block(CK_INPUT_DEVICE_MOUSE);
            else
                Unblock(CK_INPUT_DEVICE_MOUSE);
        }
        if (mask & InputHook::INPUT_BLOCK_JOYSTICK) {
            if (block)
                Block(CK_INPUT_DEVICE_JOYSTICK);
            else
                Unblock(CK_INPUT_DEVICE_JOYSTICK);
        }
    }

    static uint64_t AcquireBlock(uint32_t deviceMask) {
        const uint32_t mask = NormalizeBlockMask(deviceMask);
        if (mask == 0) return 0;
        for (uint32_t d = 0; d < CK_INPUT_DEVICE_COUNT; ++d)
            if ((mask & (1u << d)) && s_BlockedDevice[d] == INT_MAX) return 0;

        uint64_t token = 0;
        do {
            token = s_NextBlockToken++;
            if (s_NextBlockToken == 0)
                s_NextBlockToken = 1;
        } while (token == 0 || s_BlockTokens.find(token) != s_BlockTokens.end());

        s_BlockTokens.emplace(token, mask);
        ApplyBlockMask(mask, true);
        return token;
    }

    static void ReleaseBlock(uint64_t token) {
        if (token == 0)
            return;

        auto it = s_BlockTokens.find(token);
        if (it == s_BlockTokens.end())
            return;

        const uint32_t mask = it->second;
        s_BlockTokens.erase(it);
        ApplyBlockMask(mask, false);
    }

    static void ReleaseAllBlocks() {
        for (const auto &entry : s_BlockTokens)
            ApplyBlockMask(entry.second, false);
        s_BlockTokens.clear();
        s_NextBlockToken = 1;
    }

    static void ResetSessionState() {
        s_DeferPostProcess = false;
        s_PostProcessPending = false;
        ReleaseAllBlocks();
        memset(s_BlockedDevice, 0, sizeof(s_BlockedDevice));
        memset(s_LastKeyboardState, 0, sizeof(s_LastKeyboardState));
        s_LastMousePosition = {};
    }

    static CKBOOL IsKeyDownOriginal(CKDWORD iKey, CKDWORD *oStamp) {
        return (s_InputManager->*s_Original.IsKeyDown)(iKey, oStamp);
    }

    static CKBOOL IsKeyUpOriginal(CKDWORD iKey) {
        return (s_InputManager->*s_Original.IsKeyUp)(iKey);
    }

    static CKBOOL IsKeyToggledOriginal(CKDWORD iKey, CKDWORD *oStamp) {
        return (s_InputManager->*s_Original.IsKeyToggled)(iKey, oStamp);
    }

    static unsigned char * GetKeyboardStateOriginal() {
        return (s_InputManager->*s_Original.GetKeyboardState)();
    }

    static int GetNumberOfKeyInBufferOriginal() {
        return (s_InputManager->*s_Original.GetNumberOfKeyInBuffer)();
    }

    static int GetKeyFromBufferOriginal(int i, CKDWORD &oKey, CKDWORD *oTimeStamp) {
        return (s_InputManager->*s_Original.GetKeyFromBuffer)(i, oKey, oTimeStamp);
    }

    static CKBOOL IsMouseButtonDownOriginal(CK_MOUSEBUTTON iButton) {
        return (s_InputManager->*s_Original.IsMouseButtonDown)(iButton);
    }

    static CKBOOL IsMouseClickedOriginal(CK_MOUSEBUTTON iButton) {
        return (s_InputManager->*s_Original.IsMouseClicked)(iButton);
    }

    static CKBOOL IsMouseToggledOriginal(CK_MOUSEBUTTON iButton) {
        return (s_InputManager->*s_Original.IsMouseToggled)(iButton);
    }

    static void GetMouseButtonsStateOriginal(CKBYTE oStates[4]) {
        (s_InputManager->*s_Original.GetMouseButtonsState)(oStates);
    }

    static bool IsOwnedTarget(CKInputManager *manager) {
        return s_Owner && manager == s_InputManager;
    }

    CKERROR PostProcessHook() {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        if (IsOwnedTarget(manager) && s_DeferPostProcess) {
            s_PostProcessPending = true;
            return CK_OK;
        }
        return (manager->*s_Original.PostProcess)();
    }

    CKBOOL IsKeyDownHook(CKDWORD key, CKDWORD *stamp) {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_KEYBOARD)) {
            return FALSE;
        }
        return (manager->*s_Original.IsKeyDown)(key, stamp);
    }

    CKBOOL IsKeyUpHook(CKDWORD key) {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_KEYBOARD)) {
            return FALSE;
        }
        return (manager->*s_Original.IsKeyUp)(key);
    }

    CKBOOL IsKeyToggledHook(CKDWORD key, CKDWORD *stamp) {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_KEYBOARD)) {
            return FALSE;
        }
        return (manager->*s_Original.IsKeyToggled)(key, stamp);
    }

    unsigned char * GetKeyboardStateHook() {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_KEYBOARD)) {
            memset(s_EmptyKeyboardState, KS_IDLE, sizeof(s_EmptyKeyboardState));
            return s_EmptyKeyboardState;
        }
        return (manager->*s_Original.GetKeyboardState)();
    }

    int GetNumberOfKeyInBufferHook() {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_KEYBOARD)) {
            return 0;
        }
        return (manager->*s_Original.GetNumberOfKeyInBuffer)();
    }

    int GetKeyFromBufferHook(int index, CKDWORD &key, CKDWORD *stamp) {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_KEYBOARD)) {
            return NO_KEY;
        }
        return (manager->*s_Original.GetKeyFromBuffer)(index, key, stamp);
    }

    CKBOOL IsMouseButtonDownHook(CK_MOUSEBUTTON button) {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_MOUSE)) {
            return FALSE;
        }
        return (manager->*s_Original.IsMouseButtonDown)(button);
    }

    CKBOOL IsMouseClickedHook(CK_MOUSEBUTTON button) {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_MOUSE)) {
            return FALSE;
        }
        return (manager->*s_Original.IsMouseClicked)(button);
    }

    CKBOOL IsMouseToggledHook(CK_MOUSEBUTTON button) {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_MOUSE)) {
            return FALSE;
        }
        return (manager->*s_Original.IsMouseToggled)(button);
    }

    void GetMouseButtonsStateHook(CKBYTE states[4]) {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_MOUSE)) {
            memset(states, KS_IDLE, 4);
            return;
        }
        return (manager->*s_Original.GetMouseButtonsState)(states);
    }

    void GetMousePositionHook(Vx2DVector &position, CKBOOL absolute) {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_MOUSE)) {
            return;
        }
        return (manager->*s_Original.GetMousePosition)(position, absolute);
    }

    void GetMouseRelativePositionHook(VxVector &position) {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_MOUSE)) {
            return;
        }
        return (manager->*s_Original.GetMouseRelativePosition)(position);
    }

    void GetJoystickPositionHook(int joystick, VxVector *position) {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_JOYSTICK)) {
            return;
        }
        return (manager->*s_Original.GetJoystickPosition)(joystick, position);
    }

    void GetJoystickRotationHook(int joystick, VxVector *rotation) {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_JOYSTICK)) {
            return;
        }
        return (manager->*s_Original.GetJoystickRotation)(joystick, rotation);
    }

    void GetJoystickSlidersHook(int joystick, Vx2DVector *position) {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_JOYSTICK)) {
            return;
        }
        return (manager->*s_Original.GetJoystickSliders)(joystick, position);
    }

    void GetJoystickPointOfViewAngleHook(int joystick, float *angle) {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_JOYSTICK)) {
            return;
        }
        return (manager->*s_Original.GetJoystickPointOfViewAngle)(joystick, angle);
    }

    CKDWORD GetJoystickButtonsStateHook(int joystick) {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_JOYSTICK)) {
            return 0;
        }
        return (manager->*s_Original.GetJoystickButtonsState)(joystick);
    }

    CKBOOL IsJoystickButtonDownHook(int joystick, int button) {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_JOYSTICK)) {
            return FALSE;
        }
        return (manager->*s_Original.IsJoystickButtonDown)(joystick, button);
    }

    void ShowCursorHook(CKBOOL show) {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        (manager->*s_Original.ShowCursor)(show);
        if (IsOwnedTarget(manager) && s_OverlayVisible)
            ApplyCursorVisibility();
    }

    CKERROR OnCKEndHook() {
        auto *manager = reinterpret_cast<CKInputManager *>(this);
        const auto original = s_Original.OnCKEnd;
        const bool detached = !IsOwnedTarget(manager) || Unhook(s_Owner);
        if (!detached) {
            // Context destruction proceeds even if a manager reports an error.
            // Keep callable originals/ownership for a later detach retry, but
            // never retain the soon-to-be-deleted input instance as a target.
            ResetSessionState();
            SetOverlayCursorVisible(false);
            s_InputManager = nullptr;
        }
        const CKERROR result = (manager->*original)();
        return detached ? result : CKERR_INVALIDOPERATION;
    }

    static bool Hook(CKInputManager *input, Impl *owner) {
        if (!input || s_Owner || s_Patch.IsInstalled())
            return false;
#define INPUT_PATCH(Name) {BML_INPUT_SLOT(Name), InputVTable::Address(&Impl::Name##Hook)}
        const VTablePatch::Request requests[] = {
            INPUT_PATCH(PostProcess),
            INPUT_PATCH(IsKeyDown),
            INPUT_PATCH(IsKeyUp),
            INPUT_PATCH(IsKeyToggled),
            INPUT_PATCH(GetKeyboardState),
            INPUT_PATCH(GetNumberOfKeyInBuffer),
            INPUT_PATCH(GetKeyFromBuffer),
            INPUT_PATCH(IsMouseButtonDown),
            INPUT_PATCH(IsMouseClicked),
            INPUT_PATCH(IsMouseToggled),
            INPUT_PATCH(GetMouseButtonsState),
            INPUT_PATCH(GetMousePosition),
            INPUT_PATCH(GetMouseRelativePosition),
            INPUT_PATCH(GetJoystickPosition),
            INPUT_PATCH(GetJoystickRotation),
            INPUT_PATCH(GetJoystickSliders),
            INPUT_PATCH(GetJoystickPointOfViewAngle),
            INPUT_PATCH(GetJoystickButtonsState),
            INPUT_PATCH(IsJoystickButtonDown),
            INPUT_PATCH(ShowCursor),
            INPUT_PATCH(OnCKEnd),
        };
#undef INPUT_PATCH
        const auto result = s_Patch.Install(input, requests, sizeof(requests) / sizeof(requests[0]));
        if (!result) {
            std::fprintf(stderr, "BML SDL InputHook install failed: %s (entry %zu)\n",
                         VTablePatch::GetErrorName(result.Code), result.EntryIndex);
            return false;
        }
#define INPUT_ORIGINAL(Name) \
        s_Original.Name = InputVTable::Direct<decltype(s_Original.Name)>(s_Patch.GetOriginal(BML_INPUT_SLOT(Name)))
        INPUT_ORIGINAL(PostProcess);
        INPUT_ORIGINAL(IsKeyDown);
        INPUT_ORIGINAL(IsKeyUp);
        INPUT_ORIGINAL(IsKeyToggled);
        INPUT_ORIGINAL(GetKeyboardState);
        INPUT_ORIGINAL(GetNumberOfKeyInBuffer);
        INPUT_ORIGINAL(GetKeyFromBuffer);
        INPUT_ORIGINAL(IsMouseButtonDown);
        INPUT_ORIGINAL(IsMouseClicked);
        INPUT_ORIGINAL(IsMouseToggled);
        INPUT_ORIGINAL(GetMouseButtonsState);
        INPUT_ORIGINAL(GetMousePosition);
        INPUT_ORIGINAL(GetMouseRelativePosition);
        INPUT_ORIGINAL(GetJoystickPosition);
        INPUT_ORIGINAL(GetJoystickRotation);
        INPUT_ORIGINAL(GetJoystickSliders);
        INPUT_ORIGINAL(GetJoystickPointOfViewAngle);
        INPUT_ORIGINAL(GetJoystickButtonsState);
        INPUT_ORIGINAL(IsJoystickButtonDown);
        INPUT_ORIGINAL(ShowCursor);
        INPUT_ORIGINAL(OnCKEnd);
#undef INPUT_ORIGINAL
        s_InputManager = input;
        s_Owner = owner;
        return true;
    }

    static bool Unhook(Impl *owner) {
        if (owner != s_Owner)
            return false;
        // Hand back an input manager with all scheduled cleanup completed, even
        // when another hook requires a later retry of the vtable restoration.
        CompletePostProcess();
        const auto result = s_Patch.Remove();
        if (!result) {
            std::fprintf(stderr, "BML SDL InputHook removal deferred: %s (entry %zu)\n",
                         VTablePatch::GetErrorName(result.Code), result.EntryIndex);
            return false;
        }
        ResetSessionState();
        SetOverlayCursorVisible(false);
        owner->OwnsHook = false;
        s_InputManager = nullptr;
        s_Owner = nullptr;
        s_Original = {};
        return true;
    }

};

InputHook::Impl::OriginalMethods InputHook::Impl::s_Original{};

InputHook::InputHook(CKInputManager *input) : m_Impl(new Impl) {
    assert(input != nullptr);
    m_Impl->OwnsHook = Impl::Hook(input, m_Impl);
}

InputHook::~InputHook() {
    if (!DetachInputHook(*this)) {
        // Keep saved functions alive while another owner still chains this hook.
        m_Impl = nullptr;
        return;
    }
    delete m_Impl;
}

bool InputHook::IsValid() {
    return Impl::s_Owner && Impl::s_Owner->OwnsHook && Impl::s_InputManager && Impl::s_Patch.IsInstalled();
}

bool InputHookOwnsActive(const InputHook &hook) {
    return hook.m_Impl && hook.m_Impl == InputHook::Impl::s_Owner && InputHook::IsValid();
}

void DeferInputPostProcess(InputHook &hook, bool defer) {
    if (!InputHookOwnsActive(hook))
        return;
    InputHook::Impl::s_DeferPostProcess = defer;
    if (!defer)
        InputHook::Impl::CompletePostProcess();
}

bool DetachInputHook(InputHook &hook) {
    if (!hook.m_Impl || !hook.m_Impl->OwnsHook)
        return true;
    if (hook.m_Impl != InputHook::Impl::s_Owner)
        return false;
    return InputHook::Impl::Unhook(hook.m_Impl);
}

void InputHook::EnableKeyboardRepetition(CKBOOL iEnable) {
    if (!IsValid()) return;
    Impl::s_InputManager->EnableKeyboardRepetition(iEnable);
}

CKBOOL InputHook::IsKeyboardRepetitionEnabled() {
    if (!IsValid()) return FALSE;
    return Impl::s_InputManager->IsKeyboardRepetitionEnabled();
}

CKBOOL InputHook::IsKeyDown(CKDWORD iKey, CKDWORD *oStamp) {
    if (!IsValid()) return FALSE;
    return Impl::s_InputManager->IsKeyDown(iKey, oStamp);
}

CKBOOL InputHook::IsKeyUp(CKDWORD iKey) {
    if (!IsValid()) return FALSE;
    return Impl::s_InputManager->IsKeyUp(iKey);
}

CKBOOL InputHook::IsKeyToggled(CKDWORD iKey, CKDWORD *oStamp) {
    if (!IsValid()) return FALSE;
    return Impl::s_InputManager->IsKeyToggled(iKey, oStamp);
}

void InputHook::GetKeyName(CKDWORD iKey, CKSTRING oKeyName) {
    if (!IsValid()) return;
    Impl::s_InputManager->GetKeyName(iKey, const_cast<char *>(oKeyName));
}

CKDWORD InputHook::GetKeyFromName(CKSTRING iKeyName) {
    if (!IsValid()) return 0;
    return Impl::s_InputManager->GetKeyFromName(iKeyName);
}

unsigned char *InputHook::GetKeyboardState() {
    if (!IsValid()) return nullptr;
    return Impl::s_InputManager->GetKeyboardState();
}

CKBOOL InputHook::IsKeyboardAttached() {
    if (!IsValid()) return FALSE;
    return Impl::s_InputManager->IsKeyboardAttached();
}

int InputHook::GetNumberOfKeyInBuffer() {
    if (!IsValid()) return 0;
    return Impl::s_InputManager->GetNumberOfKeyInBuffer();
}

int InputHook::GetKeyFromBuffer(int i, CKDWORD &oKey, CKDWORD *oTimeStamp) {
    if (!IsValid()) return 0;
    return Impl::s_InputManager->GetKeyFromBuffer(i, oKey, oTimeStamp);
}

CKBOOL InputHook::IsMouseButtonDown(CK_MOUSEBUTTON iButton) {
    if (!IsValid()) return FALSE;
    return Impl::s_InputManager->IsMouseButtonDown(iButton);
}

CKBOOL InputHook::IsMouseClicked(CK_MOUSEBUTTON iButton) {
    if (!IsValid()) return FALSE;
    return Impl::s_InputManager->IsMouseClicked(iButton);
}

CKBOOL InputHook::IsMouseToggled(CK_MOUSEBUTTON iButton) {
    if (!IsValid()) return FALSE;
    return Impl::s_InputManager->IsMouseToggled(iButton);
}

void InputHook::GetMouseButtonsState(CKBYTE oStates[4]) {
    if (!IsValid()) return;
    Impl::s_InputManager->GetMouseButtonsState(oStates);
}

void InputHook::GetMousePosition(Vx2DVector &oPosition, CKBOOL iAbsolute) {
    if (!IsValid()) return;
    Impl::s_InputManager->GetMousePosition(oPosition, iAbsolute);
}

void InputHook::GetMouseRelativePosition(VxVector &oPosition) {
    if (!IsValid()) return;
    Impl::s_InputManager->GetMouseRelativePosition(oPosition);
}

void InputHook::GetLastMousePosition(Vx2DVector &position) {
    position = Impl::s_LastMousePosition;
}

CKBOOL InputHook::IsMouseAttached() {
    if (!IsValid()) return FALSE;
    return Impl::s_InputManager->IsMouseAttached();
}

CKBOOL InputHook::IsJoystickAttached(int iJoystick) {
    if (!IsValid()) return FALSE;
    return Impl::s_InputManager->IsJoystickAttached(iJoystick);
}

void InputHook::GetJoystickPosition(int iJoystick, VxVector *oPosition) {
    if (!IsValid()) return;
    Impl::s_InputManager->GetJoystickPosition(iJoystick, oPosition);
}

void InputHook::GetJoystickRotation(int iJoystick, VxVector *oRotation) {
    if (!IsValid()) return;
    Impl::s_InputManager->GetJoystickRotation(iJoystick, oRotation);
}

void InputHook::GetJoystickSliders(int iJoystick, Vx2DVector *oPosition) {
    if (!IsValid()) return;
    Impl::s_InputManager->GetJoystickSliders(iJoystick, oPosition);
}

void InputHook::GetJoystickPointOfViewAngle(int iJoystick, float *oAngle) {
    if (!IsValid()) return;
    Impl::s_InputManager->GetJoystickPointOfViewAngle(iJoystick, oAngle);
}

CKDWORD InputHook::GetJoystickButtonsState(int iJoystick) {
    if (!IsValid()) return 0;
    return Impl::s_InputManager->GetJoystickButtonsState(iJoystick);
}

CKBOOL InputHook::IsJoystickButtonDown(int iJoystick, int iButton) {
    if (!IsValid()) return FALSE;
    return Impl::s_InputManager->IsJoystickButtonDown(iJoystick, iButton);
}

void InputHook::Pause(CKBOOL pause) {
    if (!IsValid()) return;
    Impl::s_InputManager->Pause(pause);
    ++Impl::s_Revision;
}

void InputHook::ShowCursor(CKBOOL iShow) {
    if (!IsValid()) return;
    Impl::s_InputManager->ShowCursor(iShow);
}

void SetOverlayCursorVisible(bool visible) {
    if (!InputHook::IsValid()) return;
    InputHook::Impl::SetOverlayCursorVisible(visible);
}

CKBOOL InputHook::GetCursorVisibility() {
    if (!IsValid()) return FALSE;
    return Impl::s_OverlayVisible || Impl::s_InputManager->GetCursorVisibility();
}

VXCURSOR_POINTER InputHook::GetSystemCursor() {
    if (!IsValid()) return static_cast<VXCURSOR_POINTER>(0);
    return Impl::s_InputManager->GetSystemCursor();
}

void InputHook::SetSystemCursor(VXCURSOR_POINTER cursor) {
    if (!IsValid()) return;
    Impl::s_InputManager->SetSystemCursor(cursor);
}

CKBOOL InputHook::IsKeyPressed(CKDWORD iKey) {
    if (IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
        return FALSE;
    return oIsKeyPressed(iKey);
}

CKBOOL InputHook::IsKeyReleased(CKDWORD iKey) {
    if (IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
        return FALSE;
    return oIsKeyReleased(iKey);
}

CKBOOL InputHook::oIsKeyPressed(CKDWORD iKey) {
    if (!IsValid() || iKey >= 256) return FALSE;
    return Impl::IsKeyDownOriginal(iKey, nullptr) && !Impl::s_LastKeyboardState[iKey];
}

CKBOOL InputHook::oIsKeyReleased(CKDWORD iKey) {
    if (!IsValid() || iKey >= 256) return FALSE;
    return Impl::IsKeyToggledOriginal(iKey, nullptr) && Impl::s_LastKeyboardState[iKey];
}

CKBOOL InputHook::oIsKeyDown(CKDWORD iKey, CKDWORD *oStamp) {
    if (!IsValid()) return FALSE;
    return Impl::IsKeyDownOriginal(iKey, oStamp);
}

CKBOOL InputHook::oIsKeyUp(CKDWORD iKey) {
    if (!IsValid()) return FALSE;
    return Impl::IsKeyUpOriginal(iKey);
}

CKBOOL InputHook::oIsKeyToggled(CKDWORD iKey, CKDWORD *oStamp) {
    if (!IsValid()) return FALSE;
    return Impl::IsKeyToggledOriginal(iKey, oStamp);
}

unsigned char *InputHook::oGetKeyboardState() {
    if (!IsValid()) return nullptr;
    return Impl::GetKeyboardStateOriginal();
}

int InputHook::oGetNumberOfKeyInBuffer() {
    if (!IsValid()) return 0;
    return Impl::GetNumberOfKeyInBufferOriginal();
}

int InputHook::oGetKeyFromBuffer(int i, CKDWORD &oKey, CKDWORD *oTimeStamp) {
    if (!IsValid()) return 0;
    return Impl::GetKeyFromBufferOriginal(i, oKey, oTimeStamp);
}

CKBOOL InputHook::oIsMouseButtonDown(CK_MOUSEBUTTON iButton) {
    if (!IsValid()) return FALSE;
    return Impl::IsMouseButtonDownOriginal(iButton);
}

CKBOOL InputHook::oIsMouseClicked(CK_MOUSEBUTTON iButton) {
    if (!IsValid()) return FALSE;
    return Impl::IsMouseClickedOriginal(iButton);
}

CKBOOL InputHook::oIsMouseToggled(CK_MOUSEBUTTON iButton) {
    if (!IsValid()) return FALSE;
    return Impl::IsMouseToggledOriginal(iButton);
}

void InputHook::oGetMouseButtonsState(CKBYTE oStates[4]) {
    if (!IsValid()) return;
    Impl::GetMouseButtonsStateOriginal(oStates);
}

bool InputHook::IsBlock() {
    return Impl::IsBlocked(CK_INPUT_DEVICE_KEYBOARD);
}

void InputHook::SetBlock(bool block) {
    if (block) {
        Impl::Block(CK_INPUT_DEVICE_KEYBOARD);
    } else {
        Impl::Unblock(CK_INPUT_DEVICE_KEYBOARD);
    }
}

int InputHook::IsBlocked(CK_INPUT_DEVICE device) {
    return Impl::IsBlocked(device);
}

void InputHook::Block(CK_INPUT_DEVICE device) {
    Impl::Block(device);
}

void InputHook::Unblock(CK_INPUT_DEVICE device) {
    Impl::Unblock(device);
}

uint64_t InputHook::AcquireBlock(uint32_t deviceMask) {
    return Impl::AcquireBlock(deviceMask);
}

void InputHook::ReleaseBlock(uint64_t token) {
    Impl::ReleaseBlock(token);
}

void InputHook::Process() {
    if (!IsValid()) return;
    Impl::CompletePostProcess();
    const unsigned char *state = Impl::GetKeyboardStateOriginal();
    for (int key = 0; key < 256; ++key)
        Impl::s_LastKeyboardState[key] = (state[key] & KS_RELEASED) ? KS_IDLE : state[key];
    // A snapshot taken without a scheduled cleanup must not retain release bits.
    GetMousePosition(Impl::s_LastMousePosition, false);
    ++Impl::s_Revision;
}

uint64_t InputHookAccess::Revision(InputHook &) { return InputHook::Impl::s_Revision; }
void InputHookAccess::ReadRawMouse(InputHook &hook, BML_InputMouseState &value) {
    if (!InputHook::IsValid() || !value.Attached) return;
    auto *manager = InputHook::Impl::s_InputManager;
    const auto &o = InputHook::Impl::s_Original;
    Vx2DVector screen{}, window{}; VxVector relative{};
    (manager->*o.GetMousePosition)(screen, TRUE); (manager->*o.GetMousePosition)(window, FALSE);
    (manager->*o.GetMouseRelativePosition)(relative);
    value.ScreenPosition = {screen.x, screen.y}; value.WindowPosition = {window.x, window.y}; value.RelativePosition = {relative.x, relative.y, relative.z};
    for (uint32_t i = 0; i < 4; ++i) { const auto b = static_cast<CK_MOUSEBUTTON>(i);
        if (hook.oIsMouseButtonDown(b)) value.DownMask |= 1u << i;
        if (hook.oIsMouseClicked(b)) value.ClickedMask |= 1u << i;
        if (hook.oIsMouseToggled(b)) value.ToggledMask |= 1u << i;
    }
}
void InputHookAccess::ReadRawJoystick(InputHook &, uint32_t index, BML_InputJoystickState &value) {
    if (!InputHook::IsValid() || !value.Attached) return;
    auto *manager = InputHook::Impl::s_InputManager;
    const auto &o = InputHook::Impl::s_Original;
    VxVector position{}, rotation{}; Vx2DVector sliders{};
    (manager->*o.GetJoystickPosition)(index, &position); (manager->*o.GetJoystickRotation)(index, &rotation);
    (manager->*o.GetJoystickSliders)(index, &sliders); (manager->*o.GetJoystickPointOfViewAngle)(index, &value.PointOfViewAngle);
    value.Position = {position.x, position.y, position.z}; value.Rotation = {rotation.x, rotation.y, rotation.z}; value.Sliders = {sliders.x, sliders.y};
    value.Buttons = (manager->*o.GetJoystickButtonsState)(index);
}

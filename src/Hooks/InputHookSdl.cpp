#include "BML/InputHook.h"

#include <cassert>
#include <cstring>
#include <unordered_map>

#include <SDL3/SDL_mouse.h>

#include "Hooks/HookLifecycle.h"
#include "Hooks/InputCursor.h"

// The engine owns acquisition and filtering. BML owns the counted requests and
// the previous-frame snapshots used by its existing public edge queries.
struct InputHook::Impl {
    bool OwnsHook = false;

    static inline CKInputManager *s_InputManager = nullptr;
    static inline Impl *s_Owner = nullptr;
    static inline unsigned char s_LastKeyboardState[256] = {};
    static inline Vx2DVector s_LastMousePosition;
    static inline int s_BlockedDevice[CK_INPUT_DEVICE_COUNT] = {};
    static inline uint64_t s_NextBlockToken = 1;
    static inline std::unordered_map<uint64_t, uint32_t> s_BlockTokens;
    static inline bool s_OverlayVisible = false;

    static int IsBlocked(CK_INPUT_DEVICE device) {
        return device >= 0 && device < CK_INPUT_DEVICE_COUNT ? s_BlockedDevice[device] : 0;
    }

    static void Block(CK_INPUT_DEVICE device) {
        if (!s_InputManager || device < 0 || device >= CK_INPUT_DEVICE_COUNT)
            return;
        if (s_BlockedDevice[device]++ == 0)
            s_InputManager->Block(TRUE, device);
    }

    static void Unblock(CK_INPUT_DEVICE device) {
        if (!s_InputManager || device < 0 || device >= CK_INPUT_DEVICE_COUNT ||
            s_BlockedDevice[device] == 0)
            return;
        if (--s_BlockedDevice[device] == 0)
            s_InputManager->Block(FALSE, device);
    }

    static void ApplyCursorVisibility() {
        // Keep the engine's latest game request intact. SDL's platform backend
        // and this overlay request affect only the displayed system cursor.
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
        if (mask == 0)
            return 0;

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
        ReleaseAllBlocks();
        for (int device = 0; device < CK_INPUT_DEVICE_COUNT; ++device) {
            if (s_BlockedDevice[device] && s_InputManager)
                s_InputManager->Block(FALSE, static_cast<CK_INPUT_DEVICE>(device));
        }
        memset(s_BlockedDevice, 0, sizeof(s_BlockedDevice));
        memset(s_LastKeyboardState, 0, sizeof(s_LastKeyboardState));
        s_LastMousePosition = {};
    }

    static CKBOOL IsKeyDownOriginal(CKDWORD iKey, CKDWORD *oStamp) {
        return s_InputManager->IsKeyDown(iKey, oStamp, TRUE);
    }

    static CKBOOL IsKeyUpOriginal(CKDWORD iKey) {
        return s_InputManager->IsKeyUp(iKey, TRUE);
    }

    static CKBOOL IsKeyToggledOriginal(CKDWORD iKey, CKDWORD *oStamp) {
        return s_InputManager->IsKeyToggled(iKey, oStamp, TRUE);
    }

    static unsigned char * GetKeyboardStateOriginal() {
        return s_InputManager->GetKeyboardState(TRUE);
    }

    static int GetNumberOfKeyInBufferOriginal() {
        return s_InputManager->GetNumberOfKeyInBuffer(TRUE);
    }

    static int GetKeyFromBufferOriginal(int i, CKDWORD &oKey, CKDWORD *oTimeStamp) {
        return s_InputManager->GetKeyFromBuffer(i, oKey, oTimeStamp, TRUE);
    }

    static CKBOOL IsMouseButtonDownOriginal(CK_MOUSEBUTTON iButton) {
        return s_InputManager->IsMouseButtonDown(iButton, TRUE);
    }

    static CKBOOL IsMouseClickedOriginal(CK_MOUSEBUTTON iButton) {
        return s_InputManager->IsMouseClicked(iButton, TRUE);
    }

    static CKBOOL IsMouseToggledOriginal(CK_MOUSEBUTTON iButton) {
        return s_InputManager->IsMouseToggled(iButton, TRUE);
    }

    static void GetMouseButtonsStateOriginal(CKBYTE oStates[4]) {
        s_InputManager->GetMouseButtonsState(oStates, TRUE);
    }

};

InputHook::InputHook(CKInputManager *input) : m_Impl(new Impl) {
    assert(input != nullptr);
    if (input && !Impl::s_Owner) {
        Impl::s_InputManager = input;
        Impl::s_Owner = m_Impl;
        m_Impl->OwnsHook = true;
    }
}

InputHook::~InputHook() {
    DetachInputHook(*this);
    delete m_Impl;
}

bool InputHook::IsValid() {
    return Impl::s_Owner && Impl::s_Owner->OwnsHook && Impl::s_InputManager;
}

bool InputHookOwnsActive(const InputHook &hook) {
    return hook.m_Impl && hook.m_Impl == InputHook::Impl::s_Owner && InputHook::IsValid();
}

bool DetachInputHook(InputHook &hook) {
    if (!hook.m_Impl || !hook.m_Impl->OwnsHook)
        return true;
    if (hook.m_Impl != InputHook::Impl::s_Owner)
        return false;
    InputHook::Impl::ResetSessionState();
    InputHook::Impl::SetOverlayCursorVisible(false);
    InputHook::Impl::s_InputManager = nullptr;
    InputHook::Impl::s_Owner = nullptr;
    hook.m_Impl->OwnsHook = false;
    return true;
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
}

void InputHook::ShowCursor(CKBOOL iShow) {
    if (!IsValid()) return;
    Impl::s_InputManager->ShowCursor(iShow);
    if (Impl::s_OverlayVisible)
        Impl::ApplyCursorVisibility();
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
    const unsigned char *state = Impl::GetKeyboardStateOriginal();
    for (int key = 0; key < 256; ++key)
        Impl::s_LastKeyboardState[key] = (state[key] & KS_RELEASED) ? KS_IDLE : state[key];
    // The engine schedules PostProcess once. Only normalize BML's own snapshot;
    // never clear or advance the manager's input from here.
    Impl::s_InputManager->GetMousePosition(Impl::s_LastMousePosition, false);
}

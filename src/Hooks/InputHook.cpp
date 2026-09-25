#include "BML/InputHook.h"

#include <unordered_map>

#include "Hooks/CursorVisibilityPolicy.h"
#include "Hooks/InputCursor.h"
#include "Hooks/HookLifecycle.h"
#include "Hooks/VTablePatch.h"
#include "Hooks/VTables.h"
#include "HookUtils.h"

struct InputHook::Impl {
    bool OwnsHook = false;

    static unsigned char s_KeyboardState[256];
    static unsigned char s_LastKeyboardState[256];
    static Vx2DVector s_LastMousePosition;
    static int s_BlockedDevice[CK_INPUT_DEVICE_COUNT];
    static uint64_t s_NextBlockToken;
    static std::unordered_map<uint64_t, uint32_t> s_BlockTokens;
    static CKInputManager *s_InputManager;
    static CP_CLASS_VTABLE_NAME(CKInputManager)<CKInputManager> s_VTable;
    static CursorVisibilityPolicy s_CursorVisibility;
    static Impl *s_Owner;
    static VTablePatch s_Patch;

    static CKInputManager *Target(Impl *self) {
        return reinterpret_cast<CKInputManager *>(self);
    }

    static bool IsOwnedTarget(CKInputManager *manager) {
        return manager && manager == s_InputManager && s_Owner;
    }

    CP_DECLARE_METHOD_HOOK(CKERROR, PostProcess, ()) {
        CKInputManager *manager = Target(this);
        return IsOwnedTarget(manager) ? CK_OK : PostProcessOriginal(manager);
    }

    CP_DECLARE_METHOD_HOOK(void, ShowCursor, (CKBOOL show)) {
        CKInputManager *manager = Target(this);
        if (!IsOwnedTarget(manager)) {
            ShowCursorOriginal(manager, show);
            return;
        }
        ShowCursorOriginal(manager, s_CursorVisibility.SetGameVisible(show != FALSE));
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsKeyDown, (CKDWORD iKey, CKDWORD *oStamp)) {
        CKInputManager *manager = Target(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
            return FALSE;
        return IsKeyDownOriginal(manager, iKey, oStamp);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsKeyUp, (CKDWORD iKey)) {
        CKInputManager *manager = Target(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
            return FALSE;
        return IsKeyUpOriginal(manager, iKey);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsKeyToggled, (CKDWORD iKey, CKDWORD *oStamp)) {
        CKInputManager *manager = Target(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
            return FALSE;
        return IsKeyToggledOriginal(manager, iKey, oStamp);
    }

    CP_DECLARE_METHOD_HOOK(unsigned char *, GetKeyboardState, ()) {
        static unsigned char keyboardState[256] = {};
        CKInputManager *manager = Target(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
            return keyboardState;
        return GetKeyboardStateOriginal(manager);
    }

    CP_DECLARE_METHOD_HOOK(int, GetNumberOfKeyInBuffer, ()) {
        CKInputManager *manager = Target(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
            return 0;
        return GetNumberOfKeyInBufferOriginal(manager);
    }

    CP_DECLARE_METHOD_HOOK(int, GetKeyFromBuffer, (int i, CKDWORD &oKey, CKDWORD *oTimeStamp)) {
        CKInputManager *manager = Target(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
            return NO_KEY;
        return GetKeyFromBufferOriginal(manager, i, oKey, oTimeStamp);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsMouseButtonDown, (CK_MOUSEBUTTON iButton)) {
        CKInputManager *manager = Target(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_MOUSE))
            return FALSE;
        return IsMouseButtonDownOriginal(manager, iButton);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsMouseClicked, (CK_MOUSEBUTTON iButton)) {
        CKInputManager *manager = Target(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_MOUSE))
            return FALSE;
        return IsMouseClickedOriginal(manager, iButton);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsMouseToggled, (CK_MOUSEBUTTON iButton)) {
        CKInputManager *manager = Target(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_MOUSE))
            return FALSE;
        return IsMouseToggledOriginal(manager, iButton);
    }

    CP_DECLARE_METHOD_HOOK(void, GetMouseButtonsState, (CKBYTE oStates[4])) {
        CKInputManager *manager = Target(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_MOUSE)) {
            memset(oStates, KS_IDLE, sizeof(CKBYTE) * 4);
            return;
        }
        GetMouseButtonsStateOriginal(manager, oStates);
    }

    CP_DECLARE_METHOD_HOOK(void, GetMousePosition, (Vx2DVector &oPosition, CKBOOL iAbsolute)) {
        CKInputManager *manager = Target(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_MOUSE))
            return;
        GetMousePositionOriginal(manager, oPosition, iAbsolute);
    }

    CP_DECLARE_METHOD_HOOK(void, GetMouseRelativePosition, (VxVector &oPosition)) {
        CKInputManager *manager = Target(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_MOUSE))
            return;
        GetMouseRelativePositionOriginal(manager, oPosition);
    }

    CP_DECLARE_METHOD_HOOK(void, GetJoystickPosition, (int iJoystick, VxVector *oPosition)) {
        CKInputManager *manager = Target(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_JOYSTICK))
            return;
        GetJoystickPositionOriginal(manager, iJoystick, oPosition);
    }

    CP_DECLARE_METHOD_HOOK(void, GetJoystickRotation, (int iJoystick, VxVector *oRotation)) {
        CKInputManager *manager = Target(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_JOYSTICK))
            return;
        GetJoystickRotationOriginal(manager, iJoystick, oRotation);
    }

    CP_DECLARE_METHOD_HOOK(void, GetJoystickSliders, (int iJoystick, Vx2DVector *oPosition)) {
        CKInputManager *manager = Target(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_JOYSTICK))
            return;
        GetJoystickSlidersOriginal(manager, iJoystick, oPosition);
    }

    CP_DECLARE_METHOD_HOOK(void, GetJoystickPointOfViewAngle, (int iJoystick, float *oAngle)) {
        CKInputManager *manager = Target(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_JOYSTICK))
            return;
        GetJoystickPointOfViewAngleOriginal(manager, iJoystick, oAngle);
    }

    CP_DECLARE_METHOD_HOOK(CKDWORD, GetJoystickButtonsState, (int iJoystick)) {
        CKInputManager *manager = Target(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_JOYSTICK))
            return 0;
        return GetJoystickButtonsStateOriginal(manager, iJoystick);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsJoystickButtonDown, (int iJoystick, int iButton)) {
        CKInputManager *manager = Target(this);
        if (IsOwnedTarget(manager) && IsBlocked(CK_INPUT_DEVICE_JOYSTICK))
            return FALSE;
        return IsJoystickButtonDownOriginal(manager, iJoystick, iButton);
    }

    static int IsBlocked(CK_INPUT_DEVICE device) {
        if (device >= 0 && device < CK_INPUT_DEVICE_COUNT)
            return s_BlockedDevice[device];
        return 0;
    }

    static void Block(CK_INPUT_DEVICE device) {
        if (device >= 0 && device < CK_INPUT_DEVICE_COUNT)
            ++s_BlockedDevice[device];
    }

    static void Unblock(CK_INPUT_DEVICE device) {
        if (device >= 0 && device < CK_INPUT_DEVICE_COUNT && s_BlockedDevice[device] > 0)
            --s_BlockedDevice[device];
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
        memset(s_BlockedDevice, 0, sizeof(s_BlockedDevice));
        memset(s_KeyboardState, 0, sizeof(s_KeyboardState));
        memset(s_LastKeyboardState, 0, sizeof(s_LastKeyboardState));
        s_LastMousePosition = {};
    }

    static CKERROR PostProcessOriginal(CKInputManager *manager) {
        return manager && s_VTable.PostProcess ?
            CP_CALL_METHOD_PTR(manager, s_VTable.PostProcess) : CK_OK;
    }

    static CKERROR PostProcessOriginal() {
        return PostProcessOriginal(s_InputManager);
    }

    static void ShowCursorOriginal(CKInputManager *manager, CKBOOL show) {
        if (manager && s_VTable.ShowCursor)
            CP_CALL_METHOD_PTR(manager, s_VTable.ShowCursor, show);
    }

    static void ShowCursorOriginal(CKBOOL show) {
        ShowCursorOriginal(s_InputManager, show);
    }

    static void SetOverlayCursorVisible(bool visible) {
        const bool effective = s_CursorVisibility.SetOverlayVisible(visible);
        if ((s_InputManager->GetCursorVisibility() != FALSE) != effective)
            ShowCursorOriginal(effective);
    }

    static CKBOOL IsKeyDownOriginal(CKInputManager *manager, CKDWORD iKey, CKDWORD *oStamp) {
        return CP_CALL_METHOD_PTR(manager, s_VTable.IsKeyDown, iKey, oStamp);
    }

    static CKBOOL IsKeyDownOriginal(CKDWORD iKey, CKDWORD *oStamp) {
        return IsKeyDownOriginal(s_InputManager, iKey, oStamp);
    }

    static CKBOOL IsKeyUpOriginal(CKInputManager *manager, CKDWORD iKey) {
        return CP_CALL_METHOD_PTR(manager, s_VTable.IsKeyUp, iKey);
    }

    static CKBOOL IsKeyUpOriginal(CKDWORD iKey) {
        return IsKeyUpOriginal(s_InputManager, iKey);
    }

    static CKBOOL IsKeyToggledOriginal(CKInputManager *manager, CKDWORD iKey, CKDWORD *oStamp) {
        return CP_CALL_METHOD_PTR(manager, s_VTable.IsKeyToggled, iKey, oStamp);
    }

    static CKBOOL IsKeyToggledOriginal(CKDWORD iKey, CKDWORD *oStamp) {
        return IsKeyToggledOriginal(s_InputManager, iKey, oStamp);
    }

    static unsigned char *GetKeyboardStateOriginal(CKInputManager *manager) {
        return CP_CALL_METHOD_PTR(manager, s_VTable.GetKeyboardState);
    }

    static unsigned char *GetKeyboardStateOriginal() {
        return GetKeyboardStateOriginal(s_InputManager);
    }

    static int GetNumberOfKeyInBufferOriginal(CKInputManager *manager) {
        return CP_CALL_METHOD_PTR(manager, s_VTable.GetNumberOfKeyInBuffer);
    }

    static int GetNumberOfKeyInBufferOriginal() {
        return GetNumberOfKeyInBufferOriginal(s_InputManager);
    }

    static int GetKeyFromBufferOriginal(CKInputManager *manager, int i, CKDWORD &oKey,
                                        CKDWORD *oTimeStamp) {
        return CP_CALL_METHOD_PTR(manager, s_VTable.GetKeyFromBuffer, i, oKey, oTimeStamp);
    }

    static int GetKeyFromBufferOriginal(int i, CKDWORD &oKey, CKDWORD *oTimeStamp) {
        return GetKeyFromBufferOriginal(s_InputManager, i, oKey, oTimeStamp);
    }

    static CKBOOL IsMouseButtonDownOriginal(CKInputManager *manager, CK_MOUSEBUTTON iButton) {
        return CP_CALL_METHOD_PTR(manager, s_VTable.IsMouseButtonDown, iButton);
    }

    static CKBOOL IsMouseButtonDownOriginal(CK_MOUSEBUTTON iButton) {
        return IsMouseButtonDownOriginal(s_InputManager, iButton);
    }

    static CKBOOL IsMouseClickedOriginal(CKInputManager *manager, CK_MOUSEBUTTON iButton) {
        return CP_CALL_METHOD_PTR(manager, s_VTable.IsMouseClicked, iButton);
    }

    static CKBOOL IsMouseClickedOriginal(CK_MOUSEBUTTON iButton) {
        return IsMouseClickedOriginal(s_InputManager, iButton);
    }

    static CKBOOL IsMouseToggledOriginal(CKInputManager *manager, CK_MOUSEBUTTON iButton) {
        return CP_CALL_METHOD_PTR(manager, s_VTable.IsMouseToggled, iButton);
    }

    static CKBOOL IsMouseToggledOriginal(CK_MOUSEBUTTON iButton) {
        return IsMouseToggledOriginal(s_InputManager, iButton);
    }

    static void GetMouseButtonsStateOriginal(CKInputManager *manager, CKBYTE oStates[4]) {
        CP_CALL_METHOD_PTR(manager, s_VTable.GetMouseButtonsState, oStates);
    }

    static void GetMouseButtonsStateOriginal(CKBYTE oStates[4]) {
        GetMouseButtonsStateOriginal(s_InputManager, oStates);
    }

    static void GetMousePositionOriginal(CKInputManager *manager, Vx2DVector &oPosition,
                                         CKBOOL iAbsolute) {
        CP_CALL_METHOD_PTR(manager, s_VTable.GetMousePosition, oPosition, iAbsolute);
    }

    static void GetMousePositionOriginal(Vx2DVector &oPosition, CKBOOL iAbsolute) {
        GetMousePositionOriginal(s_InputManager, oPosition, iAbsolute);
    }

    static void GetMouseRelativePositionOriginal(CKInputManager *manager, VxVector &oPosition) {
        CP_CALL_METHOD_PTR(manager, s_VTable.GetMouseRelativePosition, oPosition);
    }

    static void GetMouseRelativePositionOriginal(VxVector &oPosition) {
        GetMouseRelativePositionOriginal(s_InputManager, oPosition);
    }

    static void GetJoystickPositionOriginal(CKInputManager *manager, int iJoystick,
                                            VxVector *oPosition) {
        CP_CALL_METHOD_PTR(manager, s_VTable.GetJoystickPosition, iJoystick, oPosition);
    }

    static void GetJoystickPositionOriginal(int iJoystick, VxVector *oPosition) {
        GetJoystickPositionOriginal(s_InputManager, iJoystick, oPosition);
    }

    static void GetJoystickRotationOriginal(CKInputManager *manager, int iJoystick,
                                            VxVector *oRotation) {
        CP_CALL_METHOD_PTR(manager, s_VTable.GetJoystickRotation, iJoystick, oRotation);
    }

    static void GetJoystickRotationOriginal(int iJoystick, VxVector *oRotation) {
        GetJoystickRotationOriginal(s_InputManager, iJoystick, oRotation);
    }

    static void GetJoystickSlidersOriginal(CKInputManager *manager, int iJoystick,
                                           Vx2DVector *oPosition) {
        CP_CALL_METHOD_PTR(manager, s_VTable.GetJoystickSliders, iJoystick, oPosition);
    }

    static void GetJoystickSlidersOriginal(int iJoystick, Vx2DVector *oPosition) {
        GetJoystickSlidersOriginal(s_InputManager, iJoystick, oPosition);
    }

    static void GetJoystickPointOfViewAngleOriginal(CKInputManager *manager, int iJoystick,
                                                    float *oAngle) {
        CP_CALL_METHOD_PTR(manager, s_VTable.GetJoystickPointOfViewAngle, iJoystick, oAngle);
    }

    static void GetJoystickPointOfViewAngleOriginal(int iJoystick, float *oAngle) {
        GetJoystickPointOfViewAngleOriginal(s_InputManager, iJoystick, oAngle);
    }

    static CKDWORD GetJoystickButtonsStateOriginal(CKInputManager *manager, int iJoystick) {
        return CP_CALL_METHOD_PTR(manager, s_VTable.GetJoystickButtonsState, iJoystick);
    }

    static CKDWORD GetJoystickButtonsStateOriginal(int iJoystick) {
        return GetJoystickButtonsStateOriginal(s_InputManager, iJoystick);
    }

    static CKBOOL IsJoystickButtonDownOriginal(CKInputManager *manager, int iJoystick,
                                               int iButton) {
        return CP_CALL_METHOD_PTR(manager, s_VTable.IsJoystickButtonDown, iJoystick, iButton);
    }

    static CKBOOL IsJoystickButtonDownOriginal(int iJoystick, int iButton) {
        return IsJoystickButtonDownOriginal(s_InputManager, iJoystick, iButton);
    }

    static bool Hook(CKInputManager *im, Impl *owner) {
        if (!im)
            return false;
        if (s_Patch.IsInstalled())
            return s_Owner == owner && s_InputManager == im;

#define INPUT_MANAGER_SLOT(Name) \
    (offsetof(CP_CLASS_VTABLE_NAME(CKInputManager)<CKInputManager>, Name) / sizeof(void *))
#define INPUT_MANAGER_PATCH(Name) \
    {INPUT_MANAGER_SLOT(Name), utils::TypeErase(&InputHook::Impl::CP_FUNC_HOOK_NAME(Name))}

        const VTablePatch::Request requests[] = {
            INPUT_MANAGER_PATCH(PostProcess),
            INPUT_MANAGER_PATCH(ShowCursor),
            INPUT_MANAGER_PATCH(IsKeyDown),
            INPUT_MANAGER_PATCH(IsKeyUp),
            INPUT_MANAGER_PATCH(IsKeyToggled),
            INPUT_MANAGER_PATCH(GetKeyboardState),
            INPUT_MANAGER_PATCH(GetNumberOfKeyInBuffer),
            INPUT_MANAGER_PATCH(GetKeyFromBuffer),
            INPUT_MANAGER_PATCH(IsMouseButtonDown),
            INPUT_MANAGER_PATCH(IsMouseClicked),
            INPUT_MANAGER_PATCH(IsMouseToggled),
            INPUT_MANAGER_PATCH(GetMouseButtonsState),
            INPUT_MANAGER_PATCH(GetMousePosition),
            INPUT_MANAGER_PATCH(GetMouseRelativePosition),
            INPUT_MANAGER_PATCH(GetJoystickPosition),
            INPUT_MANAGER_PATCH(GetJoystickRotation),
            INPUT_MANAGER_PATCH(GetJoystickSliders),
            INPUT_MANAGER_PATCH(GetJoystickPointOfViewAngle),
            INPUT_MANAGER_PATCH(GetJoystickButtonsState),
            INPUT_MANAGER_PATCH(IsJoystickButtonDown),
        };

        const VTablePatchResult result = s_Patch.Install(im, requests, sizeof(requests) / sizeof(requests[0]));
        if (!result) {
            utils::OutputDebugA("BML InputHook install failed: %s (entry %zu)\n",
                                VTablePatch::GetErrorName(result.Code), result.EntryIndex);
            return false;
        }

#define LOAD_INPUT_MANAGER_ORIGINAL(Name) \
    s_VTable.Name = utils::ForceReinterpretCast<decltype(s_VTable.Name)>(s_Patch.GetOriginal(INPUT_MANAGER_SLOT(Name)))

        LOAD_INPUT_MANAGER_ORIGINAL(PostProcess);
        LOAD_INPUT_MANAGER_ORIGINAL(ShowCursor);
        LOAD_INPUT_MANAGER_ORIGINAL(IsKeyDown);
        LOAD_INPUT_MANAGER_ORIGINAL(IsKeyUp);
        LOAD_INPUT_MANAGER_ORIGINAL(IsKeyToggled);
        LOAD_INPUT_MANAGER_ORIGINAL(GetKeyboardState);
        LOAD_INPUT_MANAGER_ORIGINAL(GetNumberOfKeyInBuffer);
        LOAD_INPUT_MANAGER_ORIGINAL(GetKeyFromBuffer);
        LOAD_INPUT_MANAGER_ORIGINAL(IsMouseButtonDown);
        LOAD_INPUT_MANAGER_ORIGINAL(IsMouseClicked);
        LOAD_INPUT_MANAGER_ORIGINAL(IsMouseToggled);
        LOAD_INPUT_MANAGER_ORIGINAL(GetMouseButtonsState);
        LOAD_INPUT_MANAGER_ORIGINAL(GetMousePosition);
        LOAD_INPUT_MANAGER_ORIGINAL(GetMouseRelativePosition);
        LOAD_INPUT_MANAGER_ORIGINAL(GetJoystickPosition);
        LOAD_INPUT_MANAGER_ORIGINAL(GetJoystickRotation);
        LOAD_INPUT_MANAGER_ORIGINAL(GetJoystickSliders);
        LOAD_INPUT_MANAGER_ORIGINAL(GetJoystickPointOfViewAngle);
        LOAD_INPUT_MANAGER_ORIGINAL(GetJoystickButtonsState);
        LOAD_INPUT_MANAGER_ORIGINAL(IsJoystickButtonDown);

#undef LOAD_INPUT_MANAGER_ORIGINAL
#undef INPUT_MANAGER_PATCH
#undef INPUT_MANAGER_SLOT

        s_InputManager = im;
        s_Owner = owner;
        s_CursorVisibility.Reset(s_InputManager->GetCursorVisibility() != FALSE);
        return true;
    }

    static bool Unhook(Impl *owner) {
        if (s_Owner && s_Owner != owner)
            return false;
        if (!s_Patch.IsInstalled()) {
            s_InputManager = nullptr;
            s_Owner = nullptr;
            return true;
        }

        SetOverlayCursorVisible(false);
        const VTablePatchResult result = s_Patch.Remove();
        if (!result) {
            utils::OutputDebugA("BML InputHook removal warning: %s (entry %zu)\n",
                                VTablePatch::GetErrorName(result.Code), result.EntryIndex);
        }

        if (!s_Patch.IsInstalled()) {
            s_InputManager = nullptr;
            s_Owner = nullptr;
            s_VTable = {};
            return true;
        }
        return false;
    }
};

unsigned char InputHook::Impl::s_KeyboardState[256] = {};
unsigned char InputHook::Impl::s_LastKeyboardState[256] = {};
Vx2DVector InputHook::Impl::s_LastMousePosition;
int InputHook::Impl::s_BlockedDevice[CK_INPUT_DEVICE_COUNT] = {};
uint64_t InputHook::Impl::s_NextBlockToken = 1;
std::unordered_map<uint64_t, uint32_t> InputHook::Impl::s_BlockTokens;
CKInputManager *InputHook::Impl::s_InputManager = nullptr;
CP_CLASS_VTABLE_NAME(CKInputManager)<CKInputManager> InputHook::Impl::s_VTable = {};
CursorVisibilityPolicy InputHook::Impl::s_CursorVisibility;
InputHook::Impl *InputHook::Impl::s_Owner = nullptr;
VTablePatch InputHook::Impl::s_Patch;

InputHook::InputHook(CKInputManager *input) : m_Impl(new Impl) {
    assert(input != nullptr);
    m_Impl->OwnsHook = Impl::Hook(input, m_Impl);
}

InputHook::~InputHook() {
    if (!DetachInputHook(*this)) {
        utils::OutputDebugA("BML InputHook state retained because its vtable patch is still installed\n");
        m_Impl = nullptr;
        return;
    }
    delete m_Impl;
    m_Impl = nullptr;
}

bool InputHook::IsValid() {
    return Impl::s_Owner && Impl::s_Owner->OwnsHook &&
           Impl::s_InputManager && Impl::s_Patch.IsInstalled();
}

bool InputHookOwnsActive(const InputHook &hook) {
    return hook.m_Impl && hook.m_Impl == InputHook::Impl::s_Owner && InputHook::IsValid();
}

bool DetachInputHook(InputHook &hook) {
    if (!hook.m_Impl || !hook.m_Impl->OwnsHook)
        return true;

    InputHook::Impl::ResetSessionState();
    if (!InputHook::Impl::Unhook(hook.m_Impl))
        return false;

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
    Impl::s_InputManager->GetKeyName(iKey, oKeyName);
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
}

void SetOverlayCursorVisible(bool visible) {
    if (!InputHook::Impl::s_Owner || !InputHook::Impl::s_Owner->OwnsHook ||
        !InputHook::Impl::s_Patch.IsInstalled()) return;
    InputHook::Impl::SetOverlayCursorVisible(visible);
}

CKBOOL InputHook::GetCursorVisibility() {
    if (!IsValid()) return FALSE;
    return Impl::s_InputManager->GetCursorVisibility();
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
    if (!IsValid()) return FALSE;
    return Impl::IsKeyDownOriginal(iKey, nullptr) && !Impl::s_LastKeyboardState[iKey];
}

CKBOOL InputHook::oIsKeyReleased(CKDWORD iKey) {
    if (!IsValid()) return FALSE;
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
    Impl::PostProcessOriginal();
    memcpy(Impl::s_LastKeyboardState, Impl::GetKeyboardStateOriginal(), sizeof(Impl::s_LastKeyboardState));
    Impl::s_InputManager->GetMousePosition(Impl::s_LastMousePosition, false);
}

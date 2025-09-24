#include "BML/InputHook.h"

#include "HookUtils.h"
#include "VTables.h"

struct InputHook::Impl {
    static unsigned char s_KeyboardState[256];
    static unsigned char s_LastKeyboardState[256];
    static Vx2DVector s_LastMousePosition;
    static int s_BlockedDevice[CK_INPUT_DEVICE_COUNT];
    static CKInputManager *s_InputManager;
    static CP_CLASS_VTABLE_NAME(CKInputManager)<CKInputManager> s_VTable;

    CP_DECLARE_METHOD_HOOK(CKERROR, PostProcess, ()) { return CK_OK; }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsKeyDown, (CKDWORD iKey, CKDWORD *oStamp)) {
        if (IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
            return FALSE;
        return IsKeyDownOriginal(iKey, oStamp);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsKeyUp, (CKDWORD iKey)) {
        if (IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
            return FALSE;
        return IsKeyUpOriginal(iKey);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsKeyToggled, (CKDWORD iKey, CKDWORD *oStamp)) {
        if (IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
            return FALSE;
        return IsKeyToggledOriginal(iKey, oStamp);
    }

    CP_DECLARE_METHOD_HOOK(unsigned char *, GetKeyboardState, ()) {
        static unsigned char keyboardState[256] = {};
        if (IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
            return keyboardState;
        return GetKeyboardStateOriginal();
    }

    CP_DECLARE_METHOD_HOOK(int, GetNumberOfKeyInBuffer, ()) {
        if (IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
            return 0;
        return GetNumberOfKeyInBufferOriginal();
    }

    CP_DECLARE_METHOD_HOOK(int, GetKeyFromBuffer, (int i, CKDWORD &oKey, CKDWORD *oTimeStamp)) {
        if (IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
            return NO_KEY;
        return GetKeyFromBufferOriginal(i, oKey, oTimeStamp);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsMouseButtonDown, (CK_MOUSEBUTTON iButton)) {
        if (IsBlocked(CK_INPUT_DEVICE_MOUSE))
            return FALSE;
        return IsMouseButtonDownOriginal(iButton);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsMouseClicked, (CK_MOUSEBUTTON iButton)) {
        if (IsBlocked(CK_INPUT_DEVICE_MOUSE))
            return FALSE;
        return IsMouseClickedOriginal(iButton);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsMouseToggled, (CK_MOUSEBUTTON iButton)) {
        if (IsBlocked(CK_INPUT_DEVICE_MOUSE))
            return FALSE;
        return IsMouseToggledOriginal(iButton);
    }

    CP_DECLARE_METHOD_HOOK(void, GetMouseButtonsState, (CKBYTE oStates[4])) {
        if (IsBlocked(CK_INPUT_DEVICE_MOUSE)) {
            memset(oStates, KS_IDLE, sizeof(CKBYTE) * 4);
            return;
        }
        GetMouseButtonsStateOriginal(oStates);
    }

    CP_DECLARE_METHOD_HOOK(void, GetMousePosition, (Vx2DVector &oPosition, CKBOOL iAbsolute)) {
        if (IsBlocked(CK_INPUT_DEVICE_MOUSE))
            return;
        GetMousePositionOriginal(oPosition, iAbsolute);
    }

    CP_DECLARE_METHOD_HOOK(void, GetMouseRelativePosition, (VxVector &oPosition)) {
        if (IsBlocked(CK_INPUT_DEVICE_MOUSE))
            return;
        GetMouseRelativePositionOriginal(oPosition);
    }

    CP_DECLARE_METHOD_HOOK(void, GetJoystickPosition, (int iJoystick, VxVector *oPosition)) {
        if (IsBlocked(CK_INPUT_DEVICE_JOYSTICK))
            return;
        GetJoystickPositionOriginal(iJoystick, oPosition);
    }

    CP_DECLARE_METHOD_HOOK(void, GetJoystickRotation, (int iJoystick, VxVector *oRotation)) {
        if (IsBlocked(CK_INPUT_DEVICE_JOYSTICK))
            return;
        GetJoystickRotationOriginal(iJoystick, oRotation);
    }

    CP_DECLARE_METHOD_HOOK(void, GetJoystickSliders, (int iJoystick, Vx2DVector *oPosition)) {
        if (IsBlocked(CK_INPUT_DEVICE_JOYSTICK))
            return;
        GetJoystickSlidersOriginal(iJoystick, oPosition);
    }

    CP_DECLARE_METHOD_HOOK(void, GetJoystickPointOfViewAngle, (int iJoystick, float *oAngle)) {
        if (IsBlocked(CK_INPUT_DEVICE_JOYSTICK))
            return;
        GetJoystickPointOfViewAngleOriginal(iJoystick, oAngle);
    }

    CP_DECLARE_METHOD_HOOK(CKDWORD, GetJoystickButtonsState, (int iJoystick)) {
        if (IsBlocked(CK_INPUT_DEVICE_JOYSTICK))
            return 0;
        return GetJoystickButtonsStateOriginal(iJoystick);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsJoystickButtonDown, (int iJoystick, int iButton)) {
        if (IsBlocked(CK_INPUT_DEVICE_JOYSTICK))
            return FALSE;
        return IsJoystickButtonDownOriginal(iJoystick, iButton);
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
        if (IsBlocked(device) > 0)
            --s_BlockedDevice[device];
    }

    static CKERROR PostProcessOriginal() {
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.PostProcess);
    }

    static CKBOOL IsKeyDownOriginal(CKDWORD iKey, CKDWORD *oStamp) {
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.IsKeyDown, iKey, oStamp);
    }

    static CKBOOL IsKeyUpOriginal(CKDWORD iKey) {
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.IsKeyUp, iKey);
    }

    static CKBOOL IsKeyToggledOriginal(CKDWORD iKey, CKDWORD *oStamp) {
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.IsKeyToggled, iKey, oStamp);
    }

    static unsigned char *GetKeyboardStateOriginal() {
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetKeyboardState);
    }

    static int GetNumberOfKeyInBufferOriginal() {
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetNumberOfKeyInBuffer);
    }

    static int GetKeyFromBufferOriginal(int i, CKDWORD &oKey, CKDWORD *oTimeStamp) {
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetKeyFromBuffer, i, oKey, oTimeStamp);
    }

    static CKBOOL IsMouseButtonDownOriginal(CK_MOUSEBUTTON iButton) {
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.IsMouseButtonDown, iButton);
    }

    static CKBOOL IsMouseClickedOriginal(CK_MOUSEBUTTON iButton) {
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.IsMouseClicked, iButton);
    }

    static CKBOOL IsMouseToggledOriginal(CK_MOUSEBUTTON iButton) {
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.IsMouseToggled, iButton);
    }

    static void GetMouseButtonsStateOriginal(CKBYTE oStates[4]) {
        CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetMouseButtonsState, oStates);
    }

    static void GetMousePositionOriginal(Vx2DVector &oPosition, CKBOOL iAbsolute) {
        CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetMousePosition, oPosition, iAbsolute);
    }

    static void GetMouseRelativePositionOriginal(VxVector &oPosition) {
        CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetMouseRelativePosition, oPosition);
    }

    static void GetJoystickPositionOriginal(int iJoystick, VxVector *oPosition) {
        CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetJoystickPosition, iJoystick, oPosition);
    }

    static void GetJoystickRotationOriginal(int iJoystick, VxVector *oRotation) {
        CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetJoystickRotation, iJoystick, oRotation);
    }

    static void GetJoystickSlidersOriginal(int iJoystick, Vx2DVector *oPosition) {
        CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetJoystickSliders, iJoystick, oPosition);
    }

    static void GetJoystickPointOfViewAngleOriginal(int iJoystick, float *oAngle) {
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetJoystickPointOfViewAngle, iJoystick, oAngle);
    }

    static CKDWORD GetJoystickButtonsStateOriginal(int iJoystick) {
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetJoystickButtonsState, iJoystick);
    }

    static CKBOOL IsJoystickButtonDownOriginal(int iJoystick, int iButton) {
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.IsJoystickButtonDown, iJoystick, iButton);
    }

    static void Hook(CKInputManager *im) {
        if (!im) return;
        s_InputManager = im;
        utils::LoadVTable<CP_CLASS_VTABLE_NAME(CKInputManager)<CKInputManager>>(s_InputManager, s_VTable);

#define HOOK_INPUT_MANAGER_VIRTUAL_METHOD(Instance, Name) \
    utils::HookVirtualMethod(Instance, &InputHook::Impl::CP_FUNC_HOOK_NAME(Name), (offsetof(CP_CLASS_VTABLE_NAME(CKInputManager)<CKInputManager>, Name) / sizeof(void*)))

        HOOK_INPUT_MANAGER_VIRTUAL_METHOD(s_InputManager, PostProcess);

        HOOK_INPUT_MANAGER_VIRTUAL_METHOD(s_InputManager, IsKeyDown);
        HOOK_INPUT_MANAGER_VIRTUAL_METHOD(s_InputManager, IsKeyUp);
        HOOK_INPUT_MANAGER_VIRTUAL_METHOD(s_InputManager, IsKeyToggled);
        HOOK_INPUT_MANAGER_VIRTUAL_METHOD(s_InputManager, GetKeyboardState);
        HOOK_INPUT_MANAGER_VIRTUAL_METHOD(s_InputManager, GetNumberOfKeyInBuffer);
        HOOK_INPUT_MANAGER_VIRTUAL_METHOD(s_InputManager, GetKeyFromBuffer);
        HOOK_INPUT_MANAGER_VIRTUAL_METHOD(s_InputManager, IsMouseButtonDown);
        HOOK_INPUT_MANAGER_VIRTUAL_METHOD(s_InputManager, IsMouseClicked);
        HOOK_INPUT_MANAGER_VIRTUAL_METHOD(s_InputManager, IsMouseToggled);
        HOOK_INPUT_MANAGER_VIRTUAL_METHOD(s_InputManager, GetMouseButtonsState);
        HOOK_INPUT_MANAGER_VIRTUAL_METHOD(s_InputManager, GetMousePosition);
        HOOK_INPUT_MANAGER_VIRTUAL_METHOD(s_InputManager, GetMouseRelativePosition);
        HOOK_INPUT_MANAGER_VIRTUAL_METHOD(s_InputManager, GetJoystickPosition);
        HOOK_INPUT_MANAGER_VIRTUAL_METHOD(s_InputManager, GetJoystickRotation);
        HOOK_INPUT_MANAGER_VIRTUAL_METHOD(s_InputManager, GetJoystickSliders);
        HOOK_INPUT_MANAGER_VIRTUAL_METHOD(s_InputManager, GetJoystickPointOfViewAngle);
        HOOK_INPUT_MANAGER_VIRTUAL_METHOD(s_InputManager, GetJoystickButtonsState);
        HOOK_INPUT_MANAGER_VIRTUAL_METHOD(s_InputManager, IsJoystickButtonDown);

#undef HOOK_INPUT_MANAGER_VIRTUAL_METHOD
    }

    static void Unhook() {
        if (s_InputManager)
            utils::SaveVTable<CP_CLASS_VTABLE_NAME(CKInputManager)<CKInputManager>>(s_InputManager, s_VTable);
    }
};

unsigned char InputHook::Impl::s_KeyboardState[256] = {};
unsigned char InputHook::Impl::s_LastKeyboardState[256] = {};
Vx2DVector InputHook::Impl::s_LastMousePosition;
int InputHook::Impl::s_BlockedDevice[CK_INPUT_DEVICE_COUNT] = {};
CKInputManager *InputHook::Impl::s_InputManager = nullptr;
CP_CLASS_VTABLE_NAME(CKInputManager)<CKInputManager> InputHook::Impl::s_VTable = {};

InputHook::InputHook(CKInputManager *input) : m_Impl(new Impl) {
    assert(input != nullptr);
    Impl::Hook(input);
}

InputHook::~InputHook() {
    Impl::Unhook();
    delete m_Impl;
    m_Impl = nullptr;
}

void InputHook::EnableKeyboardRepetition(CKBOOL iEnable) {
    Impl::s_InputManager->EnableKeyboardRepetition(iEnable);
}

CKBOOL InputHook::IsKeyboardRepetitionEnabled() {
    return Impl::s_InputManager->IsKeyboardRepetitionEnabled();
}

CKBOOL InputHook::IsKeyDown(CKDWORD iKey, CKDWORD *oStamp) {
    return Impl::s_InputManager->IsKeyDown(iKey, oStamp);
}

CKBOOL InputHook::IsKeyUp(CKDWORD iKey) {
    return Impl::s_InputManager->IsKeyUp(iKey);
}

CKBOOL InputHook::IsKeyToggled(CKDWORD iKey, CKDWORD *oStamp) {
    return Impl::s_InputManager->IsKeyToggled(iKey, oStamp);
}

void InputHook::GetKeyName(CKDWORD iKey, CKSTRING oKeyName) {
    Impl::s_InputManager->GetKeyName(iKey, oKeyName);
}

CKDWORD InputHook::GetKeyFromName(CKSTRING iKeyName) {
    return Impl::s_InputManager->GetKeyFromName(iKeyName);
}

unsigned char *InputHook::GetKeyboardState() {
    return Impl::s_InputManager->GetKeyboardState();
}

CKBOOL InputHook::IsKeyboardAttached() {
    return Impl::s_InputManager->IsKeyboardAttached();
}

int InputHook::GetNumberOfKeyInBuffer() {
    return Impl::s_InputManager->GetNumberOfKeyInBuffer();
}

int InputHook::GetKeyFromBuffer(int i, CKDWORD &oKey, CKDWORD *oTimeStamp) {
    return Impl::s_InputManager->GetKeyFromBuffer(i, oKey, oTimeStamp);
}

CKBOOL InputHook::IsMouseButtonDown(CK_MOUSEBUTTON iButton) {
    return Impl::s_InputManager->IsMouseButtonDown(iButton);
}

CKBOOL InputHook::IsMouseClicked(CK_MOUSEBUTTON iButton) {
    return Impl::s_InputManager->IsMouseClicked(iButton);
}

CKBOOL InputHook::IsMouseToggled(CK_MOUSEBUTTON iButton) {
    return Impl::s_InputManager->IsMouseToggled(iButton);
}

void InputHook::GetMouseButtonsState(CKBYTE oStates[4]) {
    Impl::s_InputManager->GetMouseButtonsState(oStates);
}

void InputHook::GetMousePosition(Vx2DVector &oPosition, CKBOOL iAbsolute) {
    Impl::s_InputManager->GetMousePosition(oPosition, iAbsolute);
}

void InputHook::GetMouseRelativePosition(VxVector &oPosition) {
    Impl::s_InputManager->GetMouseRelativePosition(oPosition);
}

void InputHook::GetLastMousePosition(Vx2DVector &position) {
    position = Impl::s_LastMousePosition;
}

CKBOOL InputHook::IsMouseAttached() {
    return Impl::s_InputManager->IsMouseAttached();
}

CKBOOL InputHook::IsJoystickAttached(int iJoystick) {
    return Impl::s_InputManager->IsJoystickAttached(iJoystick);
}

void InputHook::GetJoystickPosition(int iJoystick, VxVector *oPosition) {
    Impl::s_InputManager->GetJoystickPosition(iJoystick, oPosition);
}

void InputHook::GetJoystickRotation(int iJoystick, VxVector *oRotation) {
    Impl::s_InputManager->GetJoystickRotation(iJoystick, oRotation);
}

void InputHook::GetJoystickSliders(int iJoystick, Vx2DVector *oPosition) {
    Impl::s_InputManager->GetJoystickSliders(iJoystick, oPosition);
}

void InputHook::GetJoystickPointOfViewAngle(int iJoystick, float *oAngle) {
    Impl::s_InputManager->GetJoystickPointOfViewAngle(iJoystick, oAngle);
}

CKDWORD InputHook::GetJoystickButtonsState(int iJoystick) {
    return Impl::s_InputManager->GetJoystickButtonsState(iJoystick);
}

CKBOOL InputHook::IsJoystickButtonDown(int iJoystick, int iButton) {
    return Impl::s_InputManager->IsJoystickButtonDown(iJoystick, iButton);
}

void InputHook::Pause(CKBOOL pause) {
    Impl::s_InputManager->Pause(pause);
}

void InputHook::ShowCursor(CKBOOL iShow) {
    Impl::s_InputManager->ShowCursor(iShow);
}

CKBOOL InputHook::GetCursorVisibility() {
    return Impl::s_InputManager->GetCursorVisibility();
}

VXCURSOR_POINTER InputHook::GetSystemCursor() {
    return Impl::s_InputManager->GetSystemCursor();
}

void InputHook::SetSystemCursor(VXCURSOR_POINTER cursor) {
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
    return Impl::IsKeyDownOriginal(iKey, nullptr) && !Impl::s_LastKeyboardState[iKey];
}

CKBOOL InputHook::oIsKeyReleased(CKDWORD iKey) {
    return Impl::IsKeyToggledOriginal(iKey, nullptr) && Impl::s_LastKeyboardState[iKey];
}

CKBOOL InputHook::oIsKeyDown(CKDWORD iKey, CKDWORD *oStamp) {
    return Impl::IsKeyDownOriginal(iKey, oStamp);
}

CKBOOL InputHook::oIsKeyUp(CKDWORD iKey) {
    return Impl::IsKeyUpOriginal(iKey);
}

CKBOOL InputHook::oIsKeyToggled(CKDWORD iKey, CKDWORD *oStamp) {
    return Impl::IsKeyToggledOriginal(iKey, oStamp);
}

unsigned char *InputHook::oGetKeyboardState() {
    return Impl::GetKeyboardStateOriginal();
}

int InputHook::oGetNumberOfKeyInBuffer() {
    return Impl::GetNumberOfKeyInBufferOriginal();
}

int InputHook::oGetKeyFromBuffer(int i, CKDWORD &oKey, CKDWORD *oTimeStamp) {
    return Impl::GetKeyFromBufferOriginal(i, oKey, oTimeStamp);
}

CKBOOL InputHook::oIsMouseButtonDown(CK_MOUSEBUTTON iButton) {
    return Impl::IsMouseButtonDownOriginal(iButton);
}

CKBOOL InputHook::oIsMouseClicked(CK_MOUSEBUTTON iButton) {
    return Impl::IsMouseClickedOriginal(iButton);
}

CKBOOL InputHook::oIsMouseToggled(CK_MOUSEBUTTON iButton) {
    return Impl::IsMouseToggledOriginal(iButton);
}

void InputHook::oGetMouseButtonsState(CKBYTE oStates[4]) {
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

void InputHook::Process() {
    Impl::PostProcessOriginal();
    memcpy(Impl::s_LastKeyboardState, Impl::GetKeyboardStateOriginal(), sizeof(Impl::s_LastKeyboardState));
    Impl::s_InputManager->GetMousePosition(Impl::s_LastMousePosition, false);
}

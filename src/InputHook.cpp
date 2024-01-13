#include "BML/InputHook.h"
#include "InputManager.h"

extern InputManager *g_InputManager;

void InputHook::EnableKeyboardRepetition(CKBOOL iEnable) {
    g_InputManager->EnableKeyboardRepetition(iEnable);
}

CKBOOL InputHook::IsKeyboardRepetitionEnabled() {
    return g_InputManager->IsKeyboardRepetitionEnabled();
}

CKBOOL InputHook::IsKeyDown(CKDWORD iKey, CKDWORD *oStamp) {
    return g_InputManager->IsKeyDown(iKey, oStamp);
}

CKBOOL InputHook::IsKeyUp(CKDWORD iKey) {
    return g_InputManager->IsKeyUp(iKey);
}

CKBOOL InputHook::IsKeyToggled(CKDWORD iKey, CKDWORD *oStamp) {
    return g_InputManager->IsKeyToggled(iKey, oStamp);
}

void InputHook::GetKeyName(CKDWORD iKey, CKSTRING oKeyName) {
    g_InputManager->GetKeyName(iKey, oKeyName);
}

CKDWORD InputHook::GetKeyFromName(CKSTRING iKeyName) {
    return g_InputManager->GetKeyFromName(iKeyName);
}

unsigned char *InputHook::GetKeyboardState() {
    return g_InputManager->GetKeyboardState();
}

CKBOOL InputHook::IsKeyboardAttached() {
    return g_InputManager->IsKeyboardAttached();
}

int InputHook::GetNumberOfKeyInBuffer() {
    return g_InputManager->GetNumberOfKeyInBuffer();
}

int InputHook::GetKeyFromBuffer(int i, CKDWORD &oKey, CKDWORD *oTimeStamp) {
    return g_InputManager->GetKeyFromBuffer(i, oKey, oTimeStamp);
}

CKBOOL InputHook::IsMouseButtonDown(CK_MOUSEBUTTON iButton) {
    return g_InputManager->IsMouseButtonDown(iButton);
}

CKBOOL InputHook::IsMouseClicked(CK_MOUSEBUTTON iButton) {
    return g_InputManager->IsMouseClicked(iButton);
}

CKBOOL InputHook::IsMouseToggled(CK_MOUSEBUTTON iButton) {
    return g_InputManager->IsMouseToggled(iButton);
}

void InputHook::GetMouseButtonsState(CKBYTE oStates[4]) {
    g_InputManager->GetMouseButtonsState(oStates);
}

void InputHook::GetMousePosition(Vx2DVector &oPosition, CKBOOL iAbsolute) {
    g_InputManager->GetMousePosition(oPosition, iAbsolute);
}

void InputHook::GetMouseRelativePosition(VxVector &oPosition) {
    g_InputManager->GetMouseRelativePosition(oPosition);
}

void InputHook::GetLastMousePosition(Vx2DVector &position) {
    g_InputManager->GetMousePosition(position, FALSE);
    VxVector delta;
    g_InputManager->GetMouseRelativePosition(delta);
    position.x -= delta.x;
    position.y -= delta.y;
}

CKBOOL InputHook::IsMouseAttached() {
    return g_InputManager->IsMouseAttached();
}

CKBOOL InputHook::IsJoystickAttached(int iJoystick) {
    return g_InputManager->IsJoystickAttached(iJoystick);
}

void InputHook::GetJoystickPosition(int iJoystick, VxVector *oPosition) {
    g_InputManager->GetJoystickPosition(iJoystick, oPosition);
}

void InputHook::GetJoystickRotation(int iJoystick, VxVector *oRotation) {
    g_InputManager->GetJoystickRotation(iJoystick, oRotation);
}

void InputHook::GetJoystickSliders(int iJoystick, Vx2DVector *oPosition) {
    g_InputManager->GetJoystickSliders(iJoystick, oPosition);
}

void InputHook::GetJoystickPointOfViewAngle(int iJoystick, float *oAngle) {
    g_InputManager->GetJoystickPointOfViewAngle(iJoystick, oAngle);
}

CKDWORD InputHook::GetJoystickButtonsState(int iJoystick) {
    return g_InputManager->GetJoystickButtonsState(iJoystick);
}

CKBOOL InputHook::IsJoystickButtonDown(int iJoystick, int iButton) {
    return g_InputManager->IsJoystickButtonDown(iJoystick, iButton);
}

void InputHook::Pause(CKBOOL pause) {
    g_InputManager->Pause(pause);
}

void InputHook::ShowCursor(CKBOOL iShow) {
    g_InputManager->ShowCursor(iShow);
}

CKBOOL InputHook::GetCursorVisibility() {
    return g_InputManager->GetCursorVisibility();
}

VXCURSOR_POINTER InputHook::GetSystemCursor() {
    return g_InputManager->GetSystemCursor();
}

void InputHook::SetSystemCursor(VXCURSOR_POINTER cursor) {
    g_InputManager->SetSystemCursor(cursor);
}

bool InputHook::IsBlock() {
    return g_InputManager->IsBlocked(static_cast<CK_INPUT_DEVICE>(CK_INPUT_DEVICE_KEYBOARD | CK_INPUT_DEVICE_MOUSE));
}

void InputHook::SetBlock(bool block) {
    if (block) {
        g_InputManager->Block(static_cast<CK_INPUT_DEVICE>(CK_INPUT_DEVICE_KEYBOARD | CK_INPUT_DEVICE_MOUSE));
    } else {
        g_InputManager->Unblock(static_cast<CK_INPUT_DEVICE>(CK_INPUT_DEVICE_KEYBOARD | CK_INPUT_DEVICE_MOUSE));
    }
}

CKBOOL InputHook::IsKeyPressed(CKDWORD iKey) {
    return g_InputManager->IsKeyDown(iKey, nullptr);
}

CKBOOL InputHook::IsKeyReleased(CKDWORD iKey) {
    return g_InputManager->IsKeyToggled(iKey, nullptr);
}

CKBOOL InputHook::oIsKeyPressed(CKDWORD iKey) {
    return g_InputManager->IsKeyDownRaw(iKey, nullptr);
}

CKBOOL InputHook::oIsKeyReleased(CKDWORD iKey) {
    return g_InputManager->IsKeyToggledRaw(iKey, nullptr);
}

CKBOOL InputHook::oIsKeyDown(CKDWORD iKey, CKDWORD *oStamp) {
    return g_InputManager->IsKeyDownRaw(iKey, oStamp);
}

CKBOOL InputHook::oIsKeyUp(CKDWORD iKey) {
    return g_InputManager->IsKeyUpRaw(iKey);
}

CKBOOL InputHook::oIsKeyToggled(CKDWORD iKey, CKDWORD *oStamp) {
    return g_InputManager->IsKeyToggledRaw(iKey, oStamp);
}

unsigned char *InputHook::oGetKeyboardState() {
    return g_InputManager->GetKeyboardStateRaw();
}

int InputHook::oGetNumberOfKeyInBuffer() {
    return g_InputManager->GetNumberOfKeyInBufferRaw();
}

int InputHook::oGetKeyFromBuffer(int i, CKDWORD &oKey, CKDWORD *oTimeStamp) {
    return g_InputManager->GetKeyFromBufferRaw(i, oKey, oTimeStamp);
}

CKBOOL InputHook::oIsMouseButtonDown(CK_MOUSEBUTTON iButton) {
    return g_InputManager->IsMouseButtonDownRaw(iButton);
}

CKBOOL InputHook::oIsMouseClicked(CK_MOUSEBUTTON iButton) {
    return g_InputManager->IsMouseClickedRaw(iButton);
}

CKBOOL InputHook::oIsMouseToggled(CK_MOUSEBUTTON iButton) {
    return g_InputManager->IsMouseToggledRaw(iButton);
}

void InputHook::oGetMouseButtonsState(CKBYTE oStates[4]) {
    g_InputManager->GetMouseButtonsStateRaw(oStates);
}

void InputHook::Process() {
    g_InputManager->PostProcess();
}
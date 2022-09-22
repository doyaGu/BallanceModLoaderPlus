#include "InputHook.h"

InputHook::InputHook(CKContext *context) {
    m_InputManager = (CKInputManager *)context->GetManagerByGuid(INPUT_MANAGER_GUID);
}

void InputHook::EnableKeyboardRepetition(CKBOOL iEnable) {
    m_InputManager->EnableKeyboardRepetition(iEnable);
}

CKBOOL InputHook::IsKeyboardRepetitionEnabled() {
    return m_InputManager->IsKeyboardRepetitionEnabled();
}

CKBOOL InputHook::IsKeyDown(CKDWORD iKey, CKDWORD *oStamp) {
    CKBOOL res = InputHook::oIsKeyDown(iKey, oStamp);
    return !m_Block && res;
}

CKBOOL InputHook::IsKeyUp(CKDWORD iKey) {
    return m_Block || InputHook::oIsKeyUp(iKey);
}

CKBOOL InputHook::IsKeyToggled(CKDWORD iKey, CKDWORD *oStamp) {
    CKBOOL res = InputHook::oIsKeyToggled(iKey, oStamp);
    return !m_Block && res;
}

void InputHook::GetKeyName(CKDWORD iKey, CKSTRING oKeyName) {
    m_InputManager->GetKeyName(iKey, oKeyName);
}

CKDWORD InputHook::GetKeyFromName(CKSTRING iKeyName) {
    return m_InputManager->GetKeyFromName(iKeyName);
}

unsigned char *InputHook::GetKeyboardState() {
    return m_Block ? m_KeyboardState : InputHook::oGetKeyboardState();
}

CKBOOL InputHook::IsKeyboardAttached() {
    return m_InputManager->IsKeyboardAttached();
}

int InputHook::GetNumberOfKeyInBuffer() {
    return m_Block ? 0 : InputHook::oGetNumberOfKeyInBuffer();
}

int InputHook::GetKeyFromBuffer(int i, CKDWORD &oKey, CKDWORD *oTimeStamp) {
    return m_Block ? NO_KEY : InputHook::oGetKeyFromBuffer(i, oKey, oTimeStamp);
}

CKBOOL InputHook::IsMouseButtonDown(CK_MOUSEBUTTON iButton) {
    return !m_Block && InputHook::oIsMouseButtonDown(iButton);
}

CKBOOL InputHook::IsMouseClicked(CK_MOUSEBUTTON iButton) {
    return !m_Block && InputHook::oIsMouseClicked(iButton);
}

CKBOOL InputHook::IsMouseToggled(CK_MOUSEBUTTON iButton) {
    return !m_Block && InputHook::oIsMouseToggled(iButton);
}

void InputHook::GetMouseButtonsState(CKBYTE oStates[4]) {
    if (m_Block)
        memset(oStates, KS_IDLE, sizeof(CKBYTE) * 4);
    else
        InputHook::oGetMouseButtonsState(oStates);
}

void InputHook::GetMousePosition(Vx2DVector &oPosition, CKBOOL iAbsolute) {
    m_InputManager->GetMousePosition(oPosition, iAbsolute);
}

void InputHook::GetMouseRelativePosition(VxVector &oPosition) {
    m_InputManager->GetMouseRelativePosition(oPosition);
}

void InputHook::GetLastMousePosition(Vx2DVector &position) {
    position = m_LastMousePos;
}

CKBOOL InputHook::IsMouseAttached() {
    return m_InputManager->IsMouseAttached();
}

CKBOOL InputHook::IsJoystickAttached(int iJoystick) {
    return m_InputManager->IsJoystickAttached(iJoystick);
}

void InputHook::GetJoystickPosition(int iJoystick, VxVector *oPosition) {
    m_InputManager->GetJoystickPosition(iJoystick, oPosition);
}

void InputHook::GetJoystickRotation(int iJoystick, VxVector *oRotation) {
    m_InputManager->GetJoystickRotation(iJoystick, oRotation);
}

void InputHook::GetJoystickSliders(int iJoystick, Vx2DVector *oPosition) {
    m_InputManager->GetJoystickSliders(iJoystick, oPosition);
}

void InputHook::GetJoystickPointOfViewAngle(int iJoystick, float *oAngle) {
    m_InputManager->GetJoystickPointOfViewAngle(iJoystick, oAngle);
}

CKDWORD InputHook::GetJoystickButtonsState(int iJoystick) {
    return m_InputManager->GetJoystickButtonsState(iJoystick);
}

CKBOOL InputHook::IsJoystickButtonDown(int iJoystick, int iButton) {
    return m_InputManager->IsJoystickButtonDown(iJoystick, iButton);
}

void InputHook::Pause(CKBOOL pause) {
    m_InputManager->Pause(pause);
}

void InputHook::ShowCursor(CKBOOL iShow) {
    m_InputManager->ShowCursor(iShow);
}

CKBOOL InputHook::GetCursorVisibility() {
    return m_InputManager->GetCursorVisibility();
}

VXCURSOR_POINTER InputHook::GetSystemCursor() {
    return m_InputManager->GetSystemCursor();
}

void InputHook::SetSystemCursor(VXCURSOR_POINTER cursor) {
    m_InputManager->SetSystemCursor(cursor);
}

bool InputHook::IsBlock() {
    return m_Block;
}

void InputHook::SetBlock(bool block) {
    m_Block = block;
}

void InputHook::Process() {
    memcpy(m_LastKeyboardState, InputHook::oGetKeyboardState(), sizeof(m_LastKeyboardState));
    m_InputManager->GetMousePosition(m_LastMousePos, false);
}

CKBOOL InputHook::IsKeyPressed(CKDWORD iKey) {
    return !m_Block && oIsKeyPressed(iKey);
}

CKBOOL InputHook::IsKeyReleased(CKDWORD iKey) {
    return !m_Block && oIsKeyReleased(iKey);
}

CKBOOL InputHook::oIsKeyPressed(CKDWORD iKey) {
    return oIsKeyDown(iKey) && !m_LastKeyboardState[iKey];
}

CKBOOL InputHook::oIsKeyReleased(CKDWORD iKey) {
    return oIsKeyUp(iKey) && m_LastKeyboardState[iKey];
}

CKBOOL InputHook::oIsKeyDown(CKDWORD iKey, CKDWORD *oStamp) {
    return m_InputManager->IsKeyDown(iKey, oStamp);
}

CKBOOL InputHook::oIsKeyUp(CKDWORD iKey) {
    return m_InputManager->IsKeyUp(iKey);
}

CKBOOL InputHook::oIsKeyToggled(CKDWORD iKey, CKDWORD *oStamp) {
    return (m_InputManager->IsKeyToggled)(iKey, oStamp);
}

unsigned char *InputHook::oGetKeyboardState() {
    return m_InputManager->GetKeyboardState();
}

int InputHook::oGetNumberOfKeyInBuffer() {
    return m_InputManager->GetNumberOfKeyInBuffer();
}

int InputHook::oGetKeyFromBuffer(int i, CKDWORD &oKey, CKDWORD *oTimeStamp) {
    return m_InputManager->GetKeyFromBuffer(i, oKey, oTimeStamp);
}

CKBOOL InputHook::oIsMouseButtonDown(CK_MOUSEBUTTON iButton) {
    return m_InputManager->IsMouseButtonDown(iButton);
}

CKBOOL InputHook::oIsMouseClicked(CK_MOUSEBUTTON iButton) {
    return m_InputManager->IsMouseClicked(iButton);
}

CKBOOL InputHook::oIsMouseToggled(CK_MOUSEBUTTON iButton) {
    return m_InputManager->IsMouseToggled(iButton);
}

void InputHook::oGetMouseButtonsState(CKBYTE oStates[4]) {
    m_InputManager->GetMouseButtonsState(oStates);
}

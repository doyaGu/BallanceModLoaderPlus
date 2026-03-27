#ifndef BML_INPUT_INPUTHOOK_H
#define BML_INPUT_INPUTHOOK_H

#include "CKInputManager.h"

namespace bml { class ModuleServices; }

namespace BML_Input {

enum InputDevice {
    INPUT_DEVICE_KEYBOARD = 0,
    INPUT_DEVICE_MOUSE = 1,
    INPUT_DEVICE_JOYSTICK = 2,
    INPUT_DEVICE_COUNT = 3
};

bool InitInputHook(CKInputManager *inputManager, const bml::ModuleServices &services);
void ShutdownInputHook();

void EnableKeyboardRepetition(CKBOOL enable = TRUE);
CKBOOL IsKeyboardRepetitionEnabled();

CKBOOL IsKeyDown(CKDWORD key, CKDWORD *stamp = nullptr);
CKBOOL IsKeyUp(CKDWORD key);
CKBOOL IsKeyToggled(CKDWORD key, CKDWORD *stamp = nullptr);
CKBOOL IsKeyPressed(CKDWORD key);
CKBOOL IsKeyReleased(CKDWORD key);

void GetKeyName(CKDWORD key, CKSTRING outKeyName);
CKDWORD GetKeyFromName(CKSTRING keyName);
unsigned char *GetKeyboardState();
CKBOOL IsKeyboardAttached();
int GetNumberOfKeyInBuffer();
int GetKeyFromBuffer(int index, CKDWORD &outKey, CKDWORD *outTimestamp = nullptr);

CKBOOL IsMouseButtonDown(CK_MOUSEBUTTON button);
CKBOOL IsMouseClicked(CK_MOUSEBUTTON button);
CKBOOL IsMouseToggled(CK_MOUSEBUTTON button);
void GetMouseButtonsState(CKBYTE outStates[4]);
void GetMousePosition(Vx2DVector &outPosition, CKBOOL absolute = TRUE);
void GetMouseRelativePosition(VxVector &outPosition);
void GetLastMousePosition(Vx2DVector &position);
CKBOOL IsMouseAttached();

CKBOOL IsJoystickAttached(int joystick);
void GetJoystickPosition(int joystick, VxVector *outPosition);
void GetJoystickRotation(int joystick, VxVector *outRotation);
void GetJoystickSliders(int joystick, Vx2DVector *outPosition);
void GetJoystickPointOfViewAngle(int joystick, float *outAngle);
CKDWORD GetJoystickButtonsState(int joystick);
CKBOOL IsJoystickButtonDown(int joystick, int button);

void Pause(CKBOOL pause);
void ShowCursor(CKBOOL show);
CKBOOL GetCursorVisibility();
VXCURSOR_POINTER GetSystemCursor();
void SetSystemCursor(VXCURSOR_POINTER cursor);

void BlockDevice(InputDevice device);
void UnblockDevice(InputDevice device);
int IsDeviceBlocked(InputDevice device);
void ProcessInput();
bool IsInputHookActive();

} // namespace BML_Input

#endif // BML_INPUT_INPUTHOOK_H

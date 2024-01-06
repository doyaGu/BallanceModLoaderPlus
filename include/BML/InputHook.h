#ifndef BML_INPUTHOOK_H
#define BML_INPUTHOOK_H

#include "CKAll.h"

#include "BML/Export.h"

class BML_EXPORT InputHook {
public:
    void EnableKeyboardRepetition(CKBOOL iEnable = TRUE);
    CKBOOL IsKeyboardRepetitionEnabled();

    CKBOOL IsKeyDown(CKDWORD iKey, CKDWORD *oStamp = nullptr);
    CKBOOL IsKeyUp(CKDWORD iKey);
    CKBOOL IsKeyToggled(CKDWORD iKey, CKDWORD *oStamp = nullptr);
    CKBOOL IsKeyPressed(CKDWORD iKey);
    CKBOOL IsKeyReleased(CKDWORD iKey);

    void GetKeyName(CKDWORD iKey, CKSTRING oKeyName);
    CKDWORD GetKeyFromName(CKSTRING iKeyName);
    unsigned char *GetKeyboardState();
    CKBOOL IsKeyboardAttached();
    int GetNumberOfKeyInBuffer();
    int GetKeyFromBuffer(int i, CKDWORD &oKey, CKDWORD *oTimeStamp = nullptr);

    CKBOOL IsMouseButtonDown(CK_MOUSEBUTTON iButton);
    CKBOOL IsMouseClicked(CK_MOUSEBUTTON iButton);
    CKBOOL IsMouseToggled(CK_MOUSEBUTTON iButton);
    void GetMouseButtonsState(CKBYTE oStates[4]);
    void GetMousePosition(Vx2DVector &oPosition, CKBOOL iAbsolute = TRUE);
    void GetMouseRelativePosition(VxVector &oPosition);
    void GetLastMousePosition(Vx2DVector &position);
    CKBOOL IsMouseAttached();

    CKBOOL IsJoystickAttached(int iJoystick);
    void GetJoystickPosition(int iJoystick, VxVector *oPosition);
    void GetJoystickRotation(int iJoystick, VxVector *oRotation);
    void GetJoystickSliders(int iJoystick, Vx2DVector *oPosition);
    void GetJoystickPointOfViewAngle(int iJoystick, float *oAngle);
    CKDWORD GetJoystickButtonsState(int iJoystick);
    CKBOOL IsJoystickButtonDown(int iJoystick, int iButton);

    void Pause(CKBOOL pause);

    void ShowCursor(CKBOOL iShow);
    CKBOOL GetCursorVisibility();
    VXCURSOR_POINTER GetSystemCursor();
    void SetSystemCursor(VXCURSOR_POINTER cursor);

    CKBOOL oIsKeyDown(CKDWORD iKey, CKDWORD *oStamp = nullptr);
    CKBOOL oIsKeyUp(CKDWORD iKey);
    CKBOOL oIsKeyToggled(CKDWORD iKey, CKDWORD *oStamp = nullptr);
    CKBOOL oIsKeyPressed(CKDWORD iKey);
    CKBOOL oIsKeyReleased(CKDWORD iKey);

    unsigned char *oGetKeyboardState();
    int oGetNumberOfKeyInBuffer();
    int oGetKeyFromBuffer(int i, CKDWORD &oKey, CKDWORD *oTimeStamp = nullptr);

    CKBOOL oIsMouseButtonDown(CK_MOUSEBUTTON iButton);
    CKBOOL oIsMouseClicked(CK_MOUSEBUTTON iButton);
    CKBOOL oIsMouseToggled(CK_MOUSEBUTTON iButton);
    void oGetMouseButtonsState(CKBYTE oStates[4]);

    bool IsBlock();
    void SetBlock(bool block);
};

#endif // BML_INPUTHOOK_H

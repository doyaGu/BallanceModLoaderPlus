#ifndef DX8INPUTMANAGER_H
#define DX8INPUTMANAGER_H

#include "CKInputManager.h"

#define DIRECTINPUT_VERSION 0x800
#include <dinput.h>

#define KEYBOARD_BUFFER_SIZE 256
#define MOUSE_BUFFER_SIZE 256

typedef enum CK_INPUT_DEVICE {
    CK_INPUT_DEVICE_NULL     = 0x00000000,
    CK_INPUT_DEVICE_KEYBOARD = 0x00000001,
    CK_INPUT_DEVICE_MOUSE    = 0x00000002,
    CK_INPUT_DEVICE_JOYSTICK = 0x00000004,
} CK_INPUT_DEVICE;

class InputManager : public CKInputManager {
public:
    class CKMouse {
        friend InputManager;
    public:
        CKMouse();

        void Init(HWND hWnd);
        void Release();
        void Clear();
        void Poll(CKBOOL pause);

    private:
        LPDIRECTINPUTDEVICE8 m_Device;
        Vx2DVector m_Position;
        DIMOUSESTATE m_State;
        CKBYTE m_LastButtons[4];
        DIDEVICEOBJECTDATA m_Buffer[MOUSE_BUFFER_SIZE];
        int m_NumberOfBuffer;
    };

    class CKJoystick {
        friend InputManager;
    public:
        CKJoystick();

        void Init(HWND hWnd);
        void Release();

        void Poll();
        void GetInfo();

        CKBOOL IsAttached();

    private:
        LPDIRECTINPUTDEVICE2 m_Device;
        CKDWORD m_JoyID;
        CKDWORD m_Polled;
        VxVector m_Position;
        VxVector m_Rotation;
        Vx2DVector m_Sliders;
        CKDWORD m_PointOfViewAngle;
        CKDWORD m_Buttons;
        CKDWORD m_Xmin;  // Minimum X-coordinate
        CKDWORD m_Xmax;  // Maximum X-coordinate
        CKDWORD m_Ymin;  // Minimum Y-coordinate
        CKDWORD m_Ymax;  // Maximum Y-coordinate
        CKDWORD m_Zmin;  // Minimum Z-coordinate
        CKDWORD m_Zmax;  // Maximum Z-coordinate
        CKDWORD m_XRmin; // Minimum X-rotation
        CKDWORD m_XRmax; // Maximum X-rotation
        CKDWORD m_YRmin; // Minimum Y-rotation
        CKDWORD m_YRmax; // Maximum Y-rotation
        CKDWORD m_ZRmin; // Minimum Z-rotation
        CKDWORD m_ZRmax; // Maximum Z-rotation
        CKDWORD m_Umin;  // Minimum u-coordinate (fifth axis)
        CKDWORD m_Vmin;  // Minimum v-coordinate (sixth axis)
        CKDWORD m_Umax;  // Maximum u-coordinate (fifth axis)
        CKDWORD m_Vmax;  // Maximum v-coordinate (sixth axis)
    };

    void EnableKeyboardRepetition(CKBOOL iEnable) override;
    CKBOOL IsKeyboardRepetitionEnabled() override;

    CKBOOL IsKeyDown(CKDWORD iKey, CKDWORD *oStamp) override;
    CKBOOL IsKeyUp(CKDWORD iKey) override;
    CKBOOL IsKeyToggled(CKDWORD iKey, CKDWORD *oStamp) override;

    int GetKeyName(CKDWORD iKey, CKSTRING oKeyName) override;
    CKDWORD GetKeyFromName(CKSTRING iKeyName) override;

    unsigned char *GetKeyboardState() override;

    CKBOOL IsKeyboardAttached() override;

    int GetNumberOfKeyInBuffer() override;
    int GetKeyFromBuffer(int i, CKDWORD &oKey, CKDWORD *oTimeStamp) override;

    CKBOOL IsMouseButtonDown(CK_MOUSEBUTTON iButton) override;
    CKBOOL IsMouseClicked(CK_MOUSEBUTTON iButton) override;
    CKBOOL IsMouseToggled(CK_MOUSEBUTTON iButton) override;

    void GetMouseButtonsState(CKBYTE oStates[4]) override;
    void GetMousePosition(Vx2DVector &oPosition, CKBOOL iAbsolute) override;
    void GetMouseRelativePosition(VxVector &oPosition) override;

    CKBOOL IsMouseAttached() override;

    CKBOOL IsJoystickAttached(int iJoystick) override;

    void GetJoystickPosition(int iJoystick, VxVector *oPosition) override;
    void GetJoystickRotation(int iJoystick, VxVector *oRotation) override;
    void GetJoystickSliders(int iJoystick, Vx2DVector *oPosition) override;
    void GetJoystickPointOfViewAngle(int iJoystick, float *oAngle) override;
    CKDWORD GetJoystickButtonsState(int iJoystick) override;

    CKBOOL IsJoystickButtonDown(int iJoystick, int iButton) override;

    void Pause(CKBOOL pause) override;

    void ShowCursor(CKBOOL iShow) override;
    CKBOOL GetCursorVisibility() override;

    VXCURSOR_POINTER GetSystemCursor() override;
    void SetSystemCursor(VXCURSOR_POINTER cursor) override;

    virtual int GetJoystickCount();
    virtual IDirectInputDevice2 *GetJoystickDxInterface(int iJoystick);

    virtual CKBOOL IsBlocked(CK_INPUT_DEVICE device);
    virtual void Block(CK_INPUT_DEVICE device);
    virtual void Unblock(CK_INPUT_DEVICE device);

    virtual CKBOOL IsKeyDownRaw(CKDWORD iKey, CKDWORD *oStamp);
    virtual CKBOOL IsKeyUpRaw(CKDWORD iKey);
    virtual CKBOOL IsKeyToggledRaw(CKDWORD iKey, CKDWORD *oStamp);

    virtual unsigned char *GetKeyboardStateRaw();

    virtual int GetNumberOfKeyInBufferRaw();
    virtual int GetKeyFromBufferRaw(int i, CKDWORD &oKey, CKDWORD *oTimeStamp);

    virtual CKBOOL IsMouseButtonDownRaw(CK_MOUSEBUTTON iButton);
    virtual CKBOOL IsMouseClickedRaw(CK_MOUSEBUTTON iButton);
    virtual CKBOOL IsMouseToggledRaw(CK_MOUSEBUTTON iButton);

    virtual void GetMouseButtonsStateRaw(CKBYTE oStates[4]);
    virtual void GetMousePositionRaw(Vx2DVector &oPosition, CKBOOL iAbsolute);
    virtual void GetMouseRelativePositionRaw(VxVector &oPosition);

    virtual void GetJoystickPositionRaw(int iJoystick, VxVector *oPosition);
    virtual void GetJoystickRotationRaw(int iJoystick, VxVector *oRotation);
    virtual void GetJoystickSlidersRaw(int iJoystick, Vx2DVector *oPosition);
    virtual void GetJoystickPointOfViewAngleRaw(int iJoystick, float *oAngle);
    virtual CKDWORD GetJoystickButtonsStateRaw(int iJoystick);

    virtual CKBOOL IsJoystickButtonDownRaw(int iJoystick, int iButton);

    // Internal functions

    CKERROR OnCKInit() override;
    CKERROR OnCKEnd() override;
    CKERROR OnCKReset() override;
    CKERROR OnCKPostReset() override;
    CKERROR OnCKPause() override;
    CKERROR OnCKPlay() override;
    CKERROR PreProcess() override;
    CKERROR PostProcess() override;
    CKERROR OnPostRender(CKRenderContext *dev) override;
    CKERROR OnPostSpriteRender(CKRenderContext *dev) override;

    CKDWORD GetValidFunctionsMask() override {
        return CKMANAGER_FUNC_OnCKInit |
                CKMANAGER_FUNC_OnCKEnd |
                CKMANAGER_FUNC_OnCKReset |
                CKMANAGER_FUNC_OnCKPostReset |
                CKMANAGER_FUNC_OnCKPause |
                CKMANAGER_FUNC_OnCKPlay |
                CKMANAGER_FUNC_PreProcess |
                CKMANAGER_FUNC_PostProcess |
                CKMANAGER_FUNC_OnPostRender |
                CKMANAGER_FUNC_OnPostSpriteRender;
    }

    ~InputManager() override;

    explicit InputManager(CKContext *context);

    void Initialize(HWND hWnd);
    void Uninitialize();

    void ClearBuffers();

    static BOOL CALLBACK JoystickEnum(const DIDEVICEINSTANCE *pdidInstance, void *pContext);

protected:
    LPDIRECTINPUT8 m_DirectInput;
    LPDIRECTINPUTDEVICE8 m_Keyboard;
    VXCURSOR_POINTER m_Cursor;
    CKMouse m_Mouse;
    CKJoystick m_Joysticks[4];
    int m_JoystickCount;
    unsigned char m_KeyboardState[KEYBOARD_BUFFER_SIZE];
    CKDWORD m_KeyboardStamps[KEYBOARD_BUFFER_SIZE];
    DIDEVICEOBJECTDATA m_KeyInBuffer[KEYBOARD_BUFFER_SIZE];
    int m_NumberOfKeyInBuffer;
    int m_BlockedDevice;
    CKBOOL m_Paused;
    CKBOOL m_EnableKeyboardRepetition;
    CKDWORD m_KeyboardRepeatDelay;
    CKDWORD m_KeyboardRepeatInterval;
    CKBOOL m_ShowCursor;

private:
    void EnsureCursorVisible(CKBOOL iShow);
};

void CKInitializeParameterTypes(CKContext *context);
void CKInitializeOperationTypes(CKContext *context);
void CKInitializeOperationFunctions(CKContext *context);
void CKUnInitializeParameterTypes(CKContext *context);
void CKUnInitializeOperationTypes(CKContext *context);

#endif // DX8INPUTMANAGER_H

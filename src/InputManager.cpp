#include "InputManager.h"

#include "CKRenderContext.h"
#include "CKParameterManager.h"

#include "ModLoader.h"

#define CKOGUID_GETMOUSEPOSITION CKGUID(0x6ea0201, 0x680e3a62)
#define CKOGUID_GETMOUSEX CKGUID(0x53c51abe, 0xeba68de)
#define CKOGUID_GETMOUSEY CKGUID(0x27af3c9f, 0xdbc4eb3)

int CKKeyStringFunc(CKParameter *param, CKSTRING ValueString, CKBOOL ReadFromString) {
    if (!param) return 0;

    CKInputManager *im = (CKInputManager *) param->m_Context->GetManagerByGuid(INPUT_MANAGER_GUID);
    if (ReadFromString) {
        if (!ValueString) return 0;
        CKSTRING name = nullptr;
        if (ValueString[0] != '\0')
            name = (CKSTRING) im->GetKeyFromName(ValueString);
        param->SetValue(&name);
    } else {
        CKDWORD key = 0;
        param->GetValue(&key, FALSE);
        int len = im->GetKeyName(key, ValueString);
        if (len > 1) return len;
    }
    return 0;
}

void CK2dVectorGetMousePos(CKContext *context, CKParameterOut *res, CKParameterIn *p1, CKParameterIn *p2) {
    CKInputManager *im = (CKInputManager *) context->GetManagerByGuid(INPUT_MANAGER_GUID);
    if (im) {
        Vx2DVector pos;
        im->GetMousePosition(pos, TRUE);

        CKParameter *param = p1->GetRealSource();
        if (!param) {
            *(Vx2DVector *) res->GetWriteDataPtr() = pos;
            return;
        }

        CKBOOL absolute = FALSE;
        param->GetValue(&absolute);
        if (absolute) {
            CKRenderContext *rc = context->GetPlayerRenderContext();
            if (rc) {
                HWND hWnd = (HWND) rc->GetWindowHandle();
                POINT pt;
                pt.x = (LONG) pos.x;
                pt.y = (LONG) pos.y;
                ::ScreenToClient(hWnd, &pt);
                if (pt.x >= 0) {
                    int width = rc->GetWidth();
                    if (pt.x >= width)
                        pt.x = width - 1;
                } else {
                    pt.x = 0;
                }
                if (pt.y >= 0) {
                    int height = rc->GetHeight();
                    if (pt.y >= height)
                        pt.y = height - 1;
                } else {
                    pt.y = 0;
                }
                pos.x = (float) pt.x;
                pos.y = (float) pt.y;
            }
        }
        *(Vx2DVector *) res->GetWriteDataPtr() = pos;
    }
}

void CKIntGetMouseX(CKContext *context, CKParameterOut *res, CKParameterIn *p1, CKParameterIn *p2) {
    CKInputManager *im = (CKInputManager *) context->GetManagerByGuid(INPUT_MANAGER_GUID);
    if (im) {
        Vx2DVector pos;
        im->GetMousePosition(pos, TRUE);
        *(int *) res->GetWriteDataPtr() = (int) pos.x;
    }
}

void CKIntGetMouseY(CKContext *context, CKParameterOut *res, CKParameterIn *p1, CKParameterIn *p2) {
    CKInputManager *im = (CKInputManager *) context->GetManagerByGuid(INPUT_MANAGER_GUID);
    if (im) {
        Vx2DVector pos;
        im->GetMousePosition(pos, TRUE);
        *(int *) res->GetWriteDataPtr() = (int) pos.y;
    }
}

void CKInitializeParameterTypes(CKContext *context) {
    CKParameterTypeDesc desc;
    desc.TypeName = "Keyboard Key";
    desc.Guid = CKPGUID_KEY;
    desc.DerivedFrom = CKPGUID_INT;
    desc.Valid = TRUE;
    desc.DefaultSize = 4;
    desc.CreateDefaultFunction = nullptr;
    desc.DeleteFunction = nullptr;
    desc.SaveLoadFunction = nullptr;
    desc.StringFunction = CKKeyStringFunc;
    desc.UICreatorFunction = nullptr;
    desc.dwParam = 0;
    desc.dwFlags = 0;
    desc.Cid = 0;

    CKParameterManager *pm = context->GetParameterManager();
    pm->RegisterParameterType(&desc);
}

void CKInitializeOperationTypes(CKContext *context) {
    CKParameterManager *pm = context->GetParameterManager();
    pm->RegisterOperationType(CKOGUID_GETMOUSEPOSITION, "Get Mouse Position");
    pm->RegisterOperationType(CKOGUID_GETMOUSEX, "Get Mouse X");
    pm->RegisterOperationType(CKOGUID_GETMOUSEY, "Get Mouse Y");
}

void CKInitializeOperationFunctions(CKContext *context) {
    CKParameterManager *pm = context->GetParameterManager();

    CKGUID PGuidNone(0x1cb10760, 0x419f50c5);
    CKGUID PGuidBool(0x1ad52a8e, 0x5e741920);
    CKGUID PGuidInt(0x5a5716fd, 0x44e276d7);
    CKGUID PGuid2DVector(0x4efcb34a, 0x6079e42f);

    CKGUID OGuidGetMousePosition(0x6ea0201, 0x680e3a62);
    CKGUID OGuidGetMouseX(0x53c51abe, 0xeba68de);
    CKGUID OGuidGetMouseY(0x27af3c9f, 0xdbc4eb3);

    pm->RegisterOperationFunction(OGuidGetMouseX, PGuidInt, PGuidNone, PGuidNone, CKIntGetMouseX);
    pm->RegisterOperationFunction(OGuidGetMouseY, PGuidInt, PGuidNone, PGuidNone, CKIntGetMouseY);
    pm->RegisterOperationFunction(OGuidGetMousePosition, PGuid2DVector, PGuidNone, PGuidNone, CK2dVectorGetMousePos);
    pm->RegisterOperationFunction(OGuidGetMousePosition, PGuid2DVector, PGuidBool, PGuidNone, CK2dVectorGetMousePos);
}

void CKUnInitializeParameterTypes(CKContext *context) {
    CKParameterManager *pm = context->GetParameterManager();
    pm->UnRegisterParameterType(CKPGUID_KEY);
}

void CKUnInitializeOperationTypes(CKContext *context) {
    CKParameterManager *pm = context->GetParameterManager();
    pm->UnRegisterOperationType(CKOGUID_GETMOUSEPOSITION);
    pm->UnRegisterOperationType(CKOGUID_GETMOUSEX);
    pm->UnRegisterOperationType(CKOGUID_GETMOUSEY);
}

InputManager::CKMouse::CKMouse() : m_Device(nullptr), m_State(), m_LastButtons(), m_Buffer(), m_NumberOfBuffer(0) {}

void InputManager::CKMouse::Init(HWND hWnd) {
    if (!m_Device) return;

    if (FAILED(m_Device->SetDataFormat(&c_dfDIMouse)))
        ::OutputDebugString(TEXT("Input Manager =  Failed : SetDataFormat (Mouse)"));

    if (FAILED(m_Device->SetCooperativeLevel(hWnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND)))
        ::OutputDebugString(TEXT("Input Manager =  Failed : SetCooperativeLevel (Mouse)"));

    DIPROPDWORD dipdw;
    dipdw.diph.dwSize = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj = 0;
    dipdw.diph.dwHow = DIPH_DEVICE;
    dipdw.dwData = DIPROPAXISMODE_REL;
    if (FAILED(m_Device->SetProperty(DIPROP_AXISMODE, &dipdw.diph)))
        ::OutputDebugString(TEXT("Input Manager =  Failed : SetProperty (Mouse) Relative Coord"));

    dipdw.diph.dwSize = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj = 0;
    dipdw.diph.dwHow = DIPH_DEVICE;
    dipdw.dwData = MOUSE_BUFFER_SIZE;
    if (FAILED(m_Device->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph)))
        ::OutputDebugString(TEXT("Input Manager =  Failed : SetProperty (Mouse) Buffered Data"));
    dipdw.dwData = -1;
    if (FAILED(m_Device->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph)))
        ::OutputDebugString(TEXT("Input Manager =  Failed : SetProperty (Mouse) Buffered Data"));
}

void InputManager::CKMouse::Release() {
    if (m_Device) {
        // Unacquire the device.
        m_Device->Unacquire();
        m_Device->Release();
        m_Device = nullptr;
    }
}

void InputManager::CKMouse::Clear() {
    memset(m_LastButtons, 0, sizeof(m_LastButtons));
    memset(m_State.rgbButtons, 0, sizeof(m_State.rgbButtons));
}

void InputManager::CKMouse::Poll(CKBOOL pause) {
    if (!m_Device) return;

    HRESULT hr;

    *(CKDWORD *) m_LastButtons = *(CKDWORD *) m_State.rgbButtons;
    m_NumberOfBuffer = MOUSE_BUFFER_SIZE;
    hr = m_Device->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), m_Buffer, (LPDWORD) &m_NumberOfBuffer, 0);
    if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
        m_Device->Acquire();
        m_NumberOfBuffer = MOUSE_BUFFER_SIZE;
        hr = m_Device->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), m_Buffer, (LPDWORD) &m_NumberOfBuffer, 0);
    }

    if (pause) m_NumberOfBuffer = 0;

    if (SUCCEEDED(hr)) {
        for (int i = 0; i < m_NumberOfBuffer; i++) {
            switch (m_Buffer[i].dwOfs) {
                case DIMOFS_BUTTON0:
                case DIMOFS_BUTTON1:
                case DIMOFS_BUTTON2:
                case DIMOFS_BUTTON3:
                    if ((m_Buffer[i].dwData & 0x80) != 0)
                        m_State.rgbButtons[m_Buffer[i].dwOfs - DIMOFS_BUTTON0] |= KS_PRESSED;
                    else
                        m_State.rgbButtons[m_Buffer[i].dwOfs - DIMOFS_BUTTON0] |= KS_RELEASED;
                    break;
            }
        }
    }

    if (!pause) {
        POINT pt;
        ::GetCursorPos(&pt);
        DIMOUSESTATE state;
        memset(&state, 0, sizeof(DIMOUSESTATE));
        m_Position.x = (float) pt.x;
        m_Position.y = (float) pt.y;
        hr = m_Device->GetDeviceState(sizeof(DIMOUSESTATE), &state);
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
            m_Device->Acquire();
            m_Device->GetDeviceState(sizeof(DIMOUSESTATE), &state);
        }
        m_State.lX = state.lX;
        m_State.lY = state.lY;
        m_State.lZ = state.lZ;
    }
}


InputManager::CKJoystick::CKJoystick() {
    m_Device = nullptr;
    m_JoyID = -1;
    m_Polled = FALSE;
    m_PointOfViewAngle = 0;
    m_Buttons = 0;
    m_Xmin = 0;
    m_Xmax = 0;
    m_Ymin = 0;
    m_Ymax = 0;
    m_Zmin = 0;
    m_Zmax = 0;
    m_XRmin = 0;
    m_XRmax = 0;
    m_YRmin = 0;
    m_YRmax = 0;
    m_ZRmin = 0;
    m_ZRmax = 0;
    m_Umin = 0;
    m_Vmin = 0;
    m_Umax = 0;
    m_Vmax = 0;
}

void InputManager::CKJoystick::Init(HWND hWnd) {
    if (m_Device) {
        m_Device->SetDataFormat(&c_dfDIJoystick);
        m_Device->SetCooperativeLevel(hWnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
        m_Device->Acquire();
    }
    GetInfo();
}

void InputManager::CKJoystick::Release() {
    if (m_Device) {
        // Unacquire the device.
        m_Device->Unacquire();
        m_Device->Release();
        m_Device = nullptr;
    }
}

void InputManager::CKJoystick::Poll() {
    if (m_Polled) return;

    if (m_Device) {
        m_Device->Poll();

        DIJOYSTATE state;
        memset(&state, 0, sizeof(DIJOYSTATE));
        if (m_Device->GetDeviceState(sizeof(DIJOYSTATE), &state) == DIERR_INPUTLOST) {
            m_Device->Acquire();
            m_Device->GetDeviceState(sizeof(DIJOYSTATE), &state);
        }

        double x = (((state.lX - m_Xmin) * 2.0) / (double) (m_Xmax - m_Xmin)) - 1.0;
        double y = (((state.lY - m_Ymin) * 2.0) / (double) (m_Ymax - m_Ymin)) - 1.0;
        double z = (((state.lZ - m_Zmin) * 2.0) / (double) (m_Zmax - m_Zmin)) - 1.0;
        if (fabs(x) < 0.01) x = 0.0;
        if (fabs(y) < 0.01) y = 0.0;
        if (fabs(z) < 0.01) z = 0.0;
        m_Position.Set((float) x, (float) y, (float) z);

        double xr = (((state.lRx - m_XRmin) * 2.0) / (double) (m_XRmax - m_XRmin)) - 1.0;
        double yr = (((state.lRy - m_YRmin) * 2.0) / (double) (m_YRmax - m_YRmin)) - 1.0;
        double zr = (((state.lRz - m_ZRmin) * 2.0) / (double) (m_ZRmax - m_ZRmin)) - 1.0;
        if (fabs(xr) < 0.01) xr = 0.0;
        if (fabs(yr) < 0.01) yr = 0.0;
        if (fabs(zr) < 0.01) zr = 0.0;
        m_Position.Set((float) xr, (float) yr, (float) zr);

        double slider[2] = {
                (((state.rglSlider[0] - m_Umin) * 2.0) / (double) (m_Umax - m_Umin)) - 1.0,
                (((state.rglSlider[1] - m_Vmin) * 2.0) / (double) (m_Vmax - m_Vmin)) - 1.0,
        };
        if (fabs(slider[0]) < 0.01) slider[0] = 0.0;
        if (fabs(slider[1]) < 0.01) slider[1] = 0.0;
        m_Sliders.Set((float) slider[0], (float) slider[1]);

        m_PointOfViewAngle = (state.rgdwPOV[0] != 0xFFFF) ? state.rgdwPOV[0] : -1;

        for (int i = 0; i < 32; i++) {
            if ((state.rgbButtons[i] & 0x80) != 0)
                m_Buttons |= (1 << i);
        }

        m_Polled = TRUE;
    } else if (m_JoyID != -1) {
        JOYINFOEX ji;
        ji.dwSize = sizeof(JOYINFOEX);
        ji.dwFlags = JOY_RETURNALL;
        joyGetPosEx(m_JoyID, &ji);

        m_Position.x = (float) ((((ji.dwXpos - m_Xmin) * 2.0) / (double) (m_Xmax - m_Xmin)) - 1.0);
        m_Position.y = (float) ((((ji.dwYpos - m_Ymin) * 2.0) / (double) (m_Ymax - m_Ymin)) - 1.0);
        m_Position.z = (float) ((((ji.dwZpos - m_Zmin) * 2.0) / (double) (m_Zmax - m_Zmin)) - 1.0);

        m_Rotation.x = (float) ((ji.dwRpos - m_XRmin) / (double) (m_XRmax - m_XRmin));
        m_Rotation.y = 0;
        m_Rotation.z = 0;

        m_Sliders.x = (float) ((ji.dwUpos - m_Umin) / (double) (m_Umax - m_Umin));
        m_Sliders.y = (float) ((ji.dwVpos - m_Vmin) / (double) (m_Vmax - m_Vmin));

        m_PointOfViewAngle = ji.dwPOV;

        m_Buttons = ji.dwButtons;

        m_Polled = TRUE;
    }
}

void InputManager::CKJoystick::GetInfo() {
    if (m_Device) {
        DIPROPRANGE range;
        range.diph.dwSize = sizeof(DIPROPRANGE);
        range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        range.diph.dwHow = DIPH_BYOFFSET;

        range.diph.dwObj = 0;
        m_Device->GetProperty(DIPROP_RANGE, &range.diph);
        m_Xmin = range.lMin;
        m_Xmax = range.lMax;

        range.diph.dwObj = 4;
        m_Device->GetProperty(DIPROP_RANGE, &range.diph);
        m_Ymin = range.lMin;
        m_Ymax = range.lMax;

        range.diph.dwObj = 8;
        m_Device->GetProperty(DIPROP_RANGE, &range.diph);
        m_Zmin = range.lMin;
        m_Zmax = range.lMax;

        range.diph.dwObj = 12;
        m_Device->GetProperty(DIPROP_RANGE, &range.diph);
        m_XRmin = range.lMin;
        m_XRmax = range.lMax;

        range.diph.dwObj = 16;
        m_Device->GetProperty(DIPROP_RANGE, &range.diph);
        m_YRmin = range.lMin;
        m_YRmax = range.lMax;

        range.diph.dwObj = 20;
        m_Device->GetProperty(DIPROP_RANGE, &range.diph);
        m_ZRmin = range.lMin;
        m_ZRmax = range.lMax;

        range.diph.dwObj = 24;
        m_Device->GetProperty(DIPROP_RANGE, &range.diph);
        m_Umin = range.lMin;
        m_Umax = range.lMax;

        range.diph.dwObj = 28;
        m_Device->GetProperty(DIPROP_RANGE, &range.diph);
        m_Vmin = range.lMin;
        m_Vmax = range.lMax;
    } else {
        JOYCAPS jc;
        if (!joyGetDevCaps(m_JoyID, &jc, sizeof(JOYCAPS))) {
            m_Xmin = jc.wXmin;
            m_Xmax = jc.wXmax;
            m_Ymin = jc.wYmin;
            m_Ymax = jc.wYmax;
            m_Zmin = jc.wZmin;
            m_Zmax = jc.wZmax;
            m_XRmin = jc.wRmin;
            m_XRmax = jc.wRmax;
            m_YRmin = 0;
            m_YRmax = 0;
            m_ZRmin = 0;
            m_ZRmax = 0;
            m_Umin = jc.wUmin;
            m_Umax = jc.wUmax;
            m_Vmin = jc.wVmin;
            m_Vmax = jc.wVmax;
        }
    }
}

CKBOOL InputManager::CKJoystick::IsAttached() {
    return m_Device != nullptr || m_JoyID != -1;
}

BOOL InputManager::JoystickEnum(const DIDEVICEINSTANCE *pdidInstance, void *pContext) {
    InputManager *im = (InputManager *) pContext;

    if (im->m_JoystickCount < 4) {
        LPDIRECTINPUTDEVICE8 pJoystick = nullptr;
        im->m_DirectInput->CreateDevice(pdidInstance->guidInstance, &pJoystick, nullptr);
        if (pJoystick) {
            pJoystick->QueryInterface(IID_IDirectInputDevice2,
                                      (void **) &im->m_Joysticks[im->m_JoystickCount].m_Device);
            pJoystick->Release();
            if (im->m_Joysticks[im->m_JoystickCount].m_Device) {
                ++im->m_JoystickCount;
                return TRUE;
            }

            JOYINFO ji;
            if (im->m_JoystickCount == 0 && !joyGetPos(JOYSTICKID1, &ji)) {
                im->m_Joysticks[im->m_JoystickCount].m_JoyID = JOYSTICKID1;
                ++im->m_JoystickCount;
                return TRUE;
            }
            if (im->m_JoystickCount == 1 && !joyGetPos(JOYSTICKID2, &ji)) {
                im->m_Joysticks[im->m_JoystickCount].m_JoyID = JOYSTICKID2;
                ++im->m_JoystickCount;
                return TRUE;
            }
        }
    }

    return FALSE;
}

void InputManager::EnableKeyboardRepetition(CKBOOL iEnable) {
    m_EnableKeyboardRepetition = iEnable;
}

CKBOOL InputManager::IsKeyboardRepetitionEnabled() {
    return m_EnableKeyboardRepetition;
}

CKBOOL InputManager::IsKeyDown(CKDWORD iKey, CKDWORD *oStamp) {
    if (IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
        return FALSE;
    return IsKeyDownRaw(iKey, oStamp);
}

CKBOOL InputManager::IsKeyUp(CKDWORD iKey) {
    if (IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
        return FALSE;
    return IsKeyUpRaw(iKey);
}

CKBOOL InputManager::IsKeyToggled(CKDWORD iKey, CKDWORD *oStamp) {
    if (IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
        return FALSE;
    return IsKeyToggledRaw(iKey, oStamp);
}

int InputManager::GetKeyName(CKDWORD iKey, CKSTRING oKeyName) {
    return VxScanCodeToName(iKey, oKeyName);
}

CKDWORD InputManager::GetKeyFromName(CKSTRING iKeyName) {
    char keyName[32];
    CKDWORD iKey;
    for (iKey = 0; iKey < KEYBOARD_BUFFER_SIZE; iKey++)
        if (GetKeyName(iKey, keyName) != 0 && stricmp(keyName, iKeyName) == 0)
            break;
    return iKey;
}

unsigned char *InputManager::GetKeyboardState() {
    static unsigned char keyboardState[KEYBOARD_BUFFER_SIZE] = {};
    if (IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
        return keyboardState;
    return m_KeyboardState;
}

CKBOOL InputManager::IsKeyboardAttached() {
    return m_Keyboard != nullptr;
}

int InputManager::GetNumberOfKeyInBuffer() {
    if (IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
        return 0;
    return m_NumberOfKeyInBuffer;
}

int InputManager::GetKeyFromBuffer(int i, CKDWORD &oKey, CKDWORD *oTimeStamp) {
    if (IsBlocked(CK_INPUT_DEVICE_KEYBOARD))
        return NO_KEY;
    return GetKeyFromBufferRaw(i, oKey, oTimeStamp);
}

CKBOOL InputManager::IsMouseButtonDown(CK_MOUSEBUTTON iButton) {
    if (IsBlocked(CK_INPUT_DEVICE_MOUSE))
        return FALSE;
    return IsMouseButtonDownRaw(iButton);
}

CKBOOL InputManager::IsMouseClicked(CK_MOUSEBUTTON iButton) {
    if (IsBlocked(CK_INPUT_DEVICE_MOUSE))
        return FALSE;
    return IsMouseClickedRaw(iButton);
}

CKBOOL InputManager::IsMouseToggled(CK_MOUSEBUTTON iButton) {
    if (IsBlocked(CK_INPUT_DEVICE_MOUSE))
        return FALSE;
    return IsMouseToggledRaw(iButton);
}

void InputManager::GetMouseButtonsState(CKBYTE *oStates) {
    if (IsBlocked(CK_INPUT_DEVICE_MOUSE)) {
        memset(oStates, KS_IDLE, sizeof(CKBYTE) * 4);
        return;
    }
    GetMouseButtonsStateRaw(oStates);
}

void InputManager::GetMousePosition(Vx2DVector &oPosition, CKBOOL iAbsolute) {
    if (IsBlocked(CK_INPUT_DEVICE_MOUSE))
        return;
    GetMousePositionRaw(oPosition, iAbsolute);
}

void InputManager::GetMouseRelativePosition(VxVector &oPosition) {
    if (IsBlocked(CK_INPUT_DEVICE_MOUSE))
        return;
    GetMouseRelativePositionRaw(oPosition);
}

CKBOOL InputManager::IsMouseAttached() {
    return m_Mouse.m_Device != nullptr;
}

CKBOOL InputManager::IsJoystickAttached(int iJoystick) {
    return 0 <= iJoystick && iJoystick < m_JoystickCount && m_Joysticks[iJoystick].IsAttached();
}

void InputManager::GetJoystickPosition(int iJoystick, VxVector *oPosition) {
    if (IsBlocked(CK_INPUT_DEVICE_JOYSTICK))
        return;
    GetJoystickPositionRaw(iJoystick, oPosition);
}

void InputManager::GetJoystickRotation(int iJoystick, VxVector *oRotation) {
    if (IsBlocked(CK_INPUT_DEVICE_JOYSTICK))
        return;
    GetJoystickRotationRaw(iJoystick, oRotation);
}

void InputManager::GetJoystickSliders(int iJoystick, Vx2DVector *oPosition) {
    if (IsBlocked(CK_INPUT_DEVICE_JOYSTICK))
        return;
    GetJoystickSlidersRaw(iJoystick, oPosition);
}

void InputManager::GetJoystickPointOfViewAngle(int iJoystick, float *oAngle) {
    if (IsBlocked(CK_INPUT_DEVICE_JOYSTICK))
        return;
    GetJoystickPointOfViewAngleRaw(iJoystick, oAngle);
}

CKDWORD InputManager::GetJoystickButtonsState(int iJoystick) {
    if (IsBlocked(CK_INPUT_DEVICE_JOYSTICK))
        return 0;
    return GetJoystickButtonsStateRaw(iJoystick);
}

CKBOOL InputManager::IsJoystickButtonDown(int iJoystick, int iButton) {
    if (IsBlocked(CK_INPUT_DEVICE_JOYSTICK))
        return FALSE;
    return IsJoystickButtonDownRaw(iJoystick, iButton);
}

void InputManager::Pause(CKBOOL pause) {
    if (pause) {
        if (!m_Paused) {
            ::OutputDebugString(TEXT("InPutManager Paused"));
            ClearBuffers();
            m_Paused = pause;
            return;
        }
    } else {
        if (m_Paused)
            ::OutputDebugString(TEXT("InPutManager Un-Paused"));
    }

    m_Paused = pause;
}

void InputManager::ShowCursor(CKBOOL iShow) {
    m_ShowCursor = iShow;
    EnsureCursorVisible(iShow);
}

void InputManager::EnsureCursorVisible(CKBOOL iShow) {
    if (iShow) {
        int dc = VxShowCursor(TRUE);
        if (dc > 0)
            dc = VxShowCursor(FALSE);
        if (dc < 0)
            while (VxShowCursor(TRUE) < 0)
                continue;
    } else {
        int dc = VxShowCursor(FALSE);
        if (dc < -1)
            dc = VxShowCursor(TRUE);
        if (dc >= 0)
            while (VxShowCursor(FALSE) >= 0)
                continue;
    }
}

CKBOOL InputManager::GetCursorVisibility() {
    return m_ShowCursor;
}

VXCURSOR_POINTER InputManager::GetSystemCursor() {
    return m_Cursor;
}

void InputManager::SetSystemCursor(VXCURSOR_POINTER cursor) {
    m_Cursor = cursor;
    VxSetCursor(cursor);
}

int InputManager::GetJoystickCount() {
    return m_JoystickCount;
}

IDirectInputDevice2 *InputManager::GetJoystickDxInterface(int iJoystick) {
    if (iJoystick < 0 || iJoystick >= m_JoystickCount)
        return nullptr;
    else
        return m_Joysticks[iJoystick].m_Device;
}

CKBOOL InputManager::IsBlocked(CK_INPUT_DEVICE device) {
    return (m_BlockedDevice & device) != 0;
}

void InputManager::Block(CK_INPUT_DEVICE device) {
    m_BlockedDevice |= device;
}

void InputManager::Unblock(CK_INPUT_DEVICE device) {
    m_BlockedDevice &= ~device;
}

CKBOOL InputManager::IsKeyDownRaw(CKDWORD iKey, CKDWORD *oStamp) {
    if (iKey >= KEYBOARD_BUFFER_SIZE)
        return FALSE;
    if ((m_KeyboardState[iKey] & KS_PRESSED) == 0)
        return FALSE;
    if (oStamp) *oStamp = m_KeyboardStamps[iKey];
    return TRUE;
}

CKBOOL InputManager::IsKeyUpRaw(CKDWORD iKey) {
    return iKey < KEYBOARD_BUFFER_SIZE && m_KeyboardState[iKey] == KS_IDLE;
}

CKBOOL InputManager::IsKeyToggledRaw(CKDWORD iKey, CKDWORD *oStamp) {
    if (iKey >= KEYBOARD_BUFFER_SIZE)
        return FALSE;
    if ((m_KeyboardState[iKey] & KS_RELEASED) == 0)
        return FALSE;
    if (oStamp) *oStamp = m_KeyboardStamps[iKey];
    return TRUE;
}

unsigned char *InputManager::GetKeyboardStateRaw() {
    return m_KeyboardState;
}

int InputManager::GetNumberOfKeyInBufferRaw() {
    return m_NumberOfKeyInBuffer;
}

int InputManager::GetKeyFromBufferRaw(int i, CKDWORD &oKey, CKDWORD *oTimeStamp) {
    if (i >= m_NumberOfKeyInBuffer) return 0;
    oKey = m_KeyInBuffer[i].dwOfs;
    if (oTimeStamp) *oTimeStamp = m_KeyInBuffer[i].dwTimeStamp;
    return (m_KeyInBuffer[i].dwData & 0x80) ? KS_PRESSED : KS_RELEASED;
}

CKBOOL InputManager::IsMouseButtonDownRaw(CK_MOUSEBUTTON iButton) {
    return m_Mouse.m_State.rgbButtons[iButton] & KS_PRESSED;
}

CKBOOL InputManager::IsMouseClickedRaw(CK_MOUSEBUTTON iButton) {
    return (m_Mouse.m_State.rgbButtons[iButton] & KS_PRESSED) != 0 &&
           (m_Mouse.m_LastButtons[iButton] & KS_PRESSED) == 0;
}

CKBOOL InputManager::IsMouseToggledRaw(CK_MOUSEBUTTON iButton) {
    return (m_Mouse.m_State.rgbButtons[iButton] >> 1) & KS_PRESSED;
}

void InputManager::GetMouseButtonsStateRaw(CKBYTE *oStates) {
    *(CKDWORD *)oStates = *(CKDWORD *)(m_Mouse.m_State.rgbButtons);
}

void InputManager::GetMousePositionRaw(Vx2DVector &oPosition, CKBOOL iAbsolute) {
    if (iAbsolute) {
        oPosition = m_Mouse.m_Position;
    } else {
        CKRenderContext *rc = m_Context->GetPlayerRenderContext();
        if (rc) {
            VxRect rect;
            rc->GetWindowRect(rect, TRUE);
            oPosition.Set(m_Mouse.m_Position.x - rect.left, m_Mouse.m_Position.y - rect.top);
        } else {
            oPosition = m_Mouse.m_Position;
        }
    }
}

void InputManager::GetMouseRelativePositionRaw(VxVector &oPosition) {
    oPosition.Set((float)m_Mouse.m_State.lX, (float)m_Mouse.m_State.lY, (float)m_Mouse.m_State.lZ);
}

void InputManager::GetJoystickPositionRaw(int iJoystick, VxVector *oPosition) {
    if (iJoystick < 0 || iJoystick >= m_JoystickCount)
        return;

    if (oPosition) {
        CKJoystick *joystick = &m_Joysticks[iJoystick];
        joystick->Poll();
        *oPosition = joystick->m_Position;
    }
}

void InputManager::GetJoystickRotationRaw(int iJoystick, VxVector *oRotation) {
    if (iJoystick < 0 || iJoystick >= m_JoystickCount)
        return;

    if (oRotation) {
        CKJoystick *joystick = &m_Joysticks[iJoystick];
        joystick->Poll();
        *oRotation = joystick->m_Rotation;
    }
}

void InputManager::GetJoystickSlidersRaw(int iJoystick, Vx2DVector *oPosition) {
    if (iJoystick < 0 || iJoystick >= m_JoystickCount)
        return;

    if (oPosition) {
        CKJoystick *joystick = &m_Joysticks[iJoystick];
        joystick->Poll();
        *oPosition = joystick->m_Sliders;
    }
}

void InputManager::GetJoystickPointOfViewAngleRaw(int iJoystick, float *oAngle) {
    if (iJoystick < 0 || iJoystick >= m_JoystickCount)
        return;

    if (oAngle) {
        CKJoystick *joystick = &m_Joysticks[iJoystick];
        joystick->Poll();
        *oAngle = (float)(joystick->m_PointOfViewAngle * 3.1415927 * 0.000055555556);
    }
}

CKDWORD InputManager::GetJoystickButtonsStateRaw(int iJoystick) {
    if (iJoystick < 0 || iJoystick >= m_JoystickCount)
        return 0;

    CKJoystick *joystick = &m_Joysticks[iJoystick];
    joystick->Poll();
    return joystick->m_Buttons;
}

CKBOOL InputManager::IsJoystickButtonDownRaw(int iJoystick, int iButton) {
    if (iJoystick < 0 || iJoystick >= m_JoystickCount)
        return FALSE;

    CKJoystick *joystick = &m_Joysticks[iJoystick];
    joystick->Poll();
    return (joystick->m_Buttons & (1 << iButton)) != 0;
}

CKERROR InputManager::OnCKInit() {
    if (!m_Keyboard)
        Initialize((HWND) m_Context->GetMainWindow());

    ModLoader::GetInstance().Init(m_Context);
    return CK_OK;
}

CKERROR InputManager::OnCKEnd() {
    ModLoader::GetInstance().Shutdown();

    Uninitialize();

    return CK_OK;
}

CKERROR InputManager::OnCKReset() {
    m_ShowCursor = TRUE;
    ClearBuffers();

    ModLoader::GetInstance().OnCKReset();

    return CK_OK;
}

CKERROR InputManager::OnCKPostReset() {
    ModLoader::GetInstance().OnCKPostReset();
    return CK_OK;
}

CKERROR InputManager::OnCKPause() {
    if (!m_ShowCursor)
        EnsureCursorVisible(TRUE);

    return CK_OK;
}

CKERROR InputManager::OnCKPlay() {
    HWND hWnd = (HWND) m_Context->GetMainWindow();
    if (m_Keyboard) {
        m_Keyboard->Unacquire();
        m_Keyboard->SetCooperativeLevel(hWnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
        m_Keyboard->Acquire();
    }
    if (m_Mouse.m_Device) {
        m_Mouse.m_Device->Unacquire();
        m_Mouse.m_Device->SetCooperativeLevel(hWnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
        m_Mouse.m_Device->Acquire();
    }
    for (int i = 0; i < m_JoystickCount; i++) {
        if (m_Joysticks[i].m_Device) {
            m_Joysticks[i].m_Device->Unacquire();
            m_Joysticks[i].m_Device->SetCooperativeLevel(hWnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
            m_Joysticks[i].m_Device->Acquire();
        }
    }

    m_Mouse.Poll(m_Paused);

    memset(m_KeyboardState, 0, sizeof(m_KeyboardState));
    if (m_Keyboard) {
        m_NumberOfKeyInBuffer = KEYBOARD_BUFFER_SIZE;
        HRESULT hr;
        do {
            hr = m_Keyboard->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), m_KeyInBuffer,
                                           (LPDWORD) &m_NumberOfKeyInBuffer, 0);
            if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
                m_Keyboard->Acquire();
                hr = m_Keyboard->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), m_KeyInBuffer,
                                               (LPDWORD) &m_NumberOfKeyInBuffer, 0);
            }
        } while (hr == DI_BUFFEROVERFLOW);
    }

    if (!m_ShowCursor)
        EnsureCursorVisible(FALSE);

    return CK_OK;
}

CKERROR InputManager::PreProcess() {
    if (m_Keyboard) {
        m_NumberOfKeyInBuffer = KEYBOARD_BUFFER_SIZE;
        HRESULT hr = m_Keyboard->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), m_KeyInBuffer,
                                               (LPDWORD) &m_NumberOfKeyInBuffer, 0);
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
            m_Keyboard->Acquire();
            hr = m_Keyboard->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), m_KeyInBuffer,
                                           (LPDWORD) &m_NumberOfKeyInBuffer, 0);
        }

        if (!m_Paused) {
            if (SUCCEEDED(hr)) {
                CKDWORD iKey;
                for (int i = 0; i < m_NumberOfKeyInBuffer; i++) {
                    iKey = m_KeyInBuffer[i].dwOfs;
                    if (iKey < KEYBOARD_BUFFER_SIZE) {
                        if ((m_KeyInBuffer[i].dwData & 0x80) != 0) {
                            m_KeyboardState[iKey] |= KS_PRESSED;
                            m_KeyboardStamps[iKey] = m_KeyInBuffer[i].dwTimeStamp;
                        } else {
                            m_KeyboardState[iKey] |= KS_RELEASED;
                            m_KeyboardStamps[iKey] = m_KeyInBuffer[i].dwTimeStamp - m_KeyboardStamps[iKey];
                        }
                    }
                }
            }

            if (m_EnableKeyboardRepetition) {
                for (int i = 0; i < KEYBOARD_BUFFER_SIZE; i++) {
                    if (m_KeyboardState[i] == KS_PRESSED) {
                        if ((m_KeyboardStamps[i] & 0x80000000) == 0 && ::GetTickCount() - m_KeyboardStamps[i] > m_KeyboardRepeatDelay)
                            m_KeyboardStamps[i] &= 0x80000000;
                        if ((m_KeyboardStamps[i] & 0x80000000) != 0) {
                            for (CKDWORD t = m_KeyboardStamps[i] - m_KeyboardRepeatDelay + ::GetTickCount();
                                 t > m_KeyboardRepeatInterval;) {
                                t -= m_KeyboardRepeatInterval;
                                m_KeyboardStamps[i] -= m_KeyboardRepeatInterval;
                                if (m_NumberOfKeyInBuffer < KEYBOARD_BUFFER_SIZE) {
                                    m_KeyInBuffer[m_NumberOfKeyInBuffer].dwOfs = i;
                                    m_KeyInBuffer[m_NumberOfKeyInBuffer].dwData = 0x80;
                                    m_KeyInBuffer[m_NumberOfKeyInBuffer].dwTimeStamp = m_KeyboardStamps[i] & ~0x80000000;
                                    ++m_NumberOfKeyInBuffer;
                                }
                            }
                        }
                    }
                }
            }
        } else {
            memset(m_KeyboardState, 0, sizeof(m_KeyboardState));
            memset(m_KeyboardStamps, 0, sizeof(m_KeyboardStamps));
            memset(m_KeyInBuffer, 0, sizeof(m_KeyInBuffer));
            m_NumberOfKeyInBuffer = 0;
        }
    }

    m_Mouse.Poll(m_Paused);

    for (int i = 0; i < m_JoystickCount; i++)
        m_Joysticks[i].m_Polled = FALSE;

    return CK_OK;
}

CKERROR InputManager::PostProcess() {
    ModLoader::GetInstance().PostProcess();

    for (int iKey = 0; iKey < KEYBOARD_BUFFER_SIZE; iKey++) {
        if ((m_KeyboardState[iKey] & KS_RELEASED) != 0)
            m_KeyboardState[iKey] = KS_IDLE;
    }

    for (int iButton = 0; iButton < 4; iButton++) {
        if ((m_Mouse.m_State.rgbButtons[iButton] & KS_RELEASED) != 0)
            m_Mouse.m_State.rgbButtons[iButton] = KS_IDLE;
    }

    return CK_OK;
}

CKERROR InputManager::OnPostRender(CKRenderContext *dev) {
    ModLoader::GetInstance().OnPostRender(dev);
    return CK_OK;
}

CKERROR InputManager::OnPostSpriteRender(CKRenderContext *dev) {
    ModLoader::GetInstance().OnPostSpriteRender(dev);
    return CK_OK;
}

InputManager::~InputManager() = default;

InputManager::InputManager(CKContext *context)
    : CKInputManager(context, "DirectX Input Manager"),
      m_KeyboardState(), m_KeyboardStamps(), m_KeyInBuffer() {
    m_DirectInput = nullptr;
    m_Keyboard = nullptr;
    m_JoystickCount = 0;

    int keyboardDelay;
    DWORD keyboardSpeed;
    ::SystemParametersInfo(SPI_GETKEYBOARDDELAY, 0, &keyboardDelay, 0);
    ::SystemParametersInfo(SPI_GETKEYBOARDSPEED, 0, &keyboardSpeed, 0);
    m_KeyboardRepeatDelay = 50 * (5 * keyboardDelay + 5);
    m_KeyboardRepeatInterval = (CKDWORD) (1000.0 / (keyboardSpeed + 2.5));
    m_Paused = FALSE;
    m_EnableKeyboardRepetition = FALSE;
    m_BlockedDevice = CK_INPUT_DEVICE_NULL;

    Initialize((HWND) m_Context->GetMainWindow());

    m_NumberOfKeyInBuffer = KEYBOARD_BUFFER_SIZE;
    m_ShowCursor = TRUE;
    m_Cursor = VXCURSOR_NORMALSELECT;
    InputManager::SetSystemCursor(VXCURSOR_NORMALSELECT);

    m_Context->RegisterNewManager(this);
}

void InputManager::Initialize(HWND hWnd) {
    // Register with the DirectInput subsystem and get a pointer
    // to a IDirectInput interface we can use.
    // Create a DInput object
    DirectInput8Create(::GetModuleHandle(TEXT("CK2.dll")), DIRECTINPUT_VERSION, IID_IDirectInput8,
                       (void **) &m_DirectInput, nullptr);
    if (!m_DirectInput) {
        ::OutputDebugString(TEXT("Cannot create, DirectInput Version 8"));
        ::MessageBox(hWnd, TEXT("Initialization Error"), TEXT("Cannot Initialize Input Manager"), MB_OK);
    }

    // Obtain an interface to the system keyboard device.
    m_DirectInput->CreateDevice(GUID_SysKeyboard, &m_Keyboard, nullptr);

    // Obtain an interface to the system mouse device.
    m_DirectInput->CreateDevice(GUID_SysMouse, &m_Mouse.m_Device, nullptr);

    // Look for some joysticks we can use.
    m_DirectInput->EnumDevices(DI8DEVCLASS_GAMECTRL, JoystickEnum, this, DIEDFL_ATTACHEDONLY);

    if (m_Keyboard) {
        // Set the data format to "keyboard format".
        // This tells DirectInput that we will be passing an array
        // of 256 bytes to IDirectInputDevice::GetDeviceState.
        m_Keyboard->SetDataFormat(&c_dfDIKeyboard);

        // Set the cooperative level to let DirectInput know how
        // this device should interact with the system and with
        // other DirectInput applications.
        m_Keyboard->SetCooperativeLevel(hWnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);

        // Set the buffer size to 256 to let DirectInput uses
        // buffered I/O (buffer size = 256).
        DIPROPDWORD dipdw;
        dipdw.diph.dwSize = sizeof(DIPROPDWORD);
        dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        dipdw.diph.dwObj = 0;
        dipdw.diph.dwHow = DIPH_DEVICE;
        dipdw.dwData = KEYBOARD_BUFFER_SIZE;
        m_Keyboard->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph);

        // Acquire the newly created device.
        m_Keyboard->Acquire();
    }

    m_Mouse.Init(hWnd);

    for (int i = 0; i < m_JoystickCount; i++)
        m_Joysticks[i].Init(hWnd);
}

void InputManager::Uninitialize() {
    if (m_Keyboard) {
        // Unacquire the device.
        m_Keyboard->Unacquire();
        m_Keyboard->Release();
        m_Keyboard = nullptr;
    }

    m_Mouse.Release();

    for (int i = 0; i < sizeof(m_Joysticks) / sizeof(CKJoystick); i++)
        m_Joysticks[i].Release();

    if (m_DirectInput) {
        m_DirectInput->Release();
        m_DirectInput = nullptr;
    }
}

void InputManager::ClearBuffers() {
    if (m_Keyboard) {
        HRESULT hr;
        do {
            m_NumberOfKeyInBuffer = KEYBOARD_BUFFER_SIZE;
            hr = m_Keyboard->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), m_KeyInBuffer,
                                           (LPDWORD) &m_NumberOfKeyInBuffer, 0);
            if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
                m_Keyboard->Acquire();
                hr = m_Keyboard->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), m_KeyInBuffer,
                                               (LPDWORD) &m_NumberOfKeyInBuffer, 0);
            }
        } while (hr == DI_BUFFEROVERFLOW);
    }

    m_Mouse.Clear();
    memset(m_KeyboardState, 0, sizeof(m_KeyboardState));
}
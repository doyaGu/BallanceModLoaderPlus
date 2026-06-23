#ifndef BML_SCRIPTFACADEACCESS_H
#define BML_SCRIPTFACADEACCESS_H

#include <cstdint>
#include <string>

#include "CKAll.h"
#include "BML/InputHook.h"

namespace BML {
namespace ScriptFacadeAccess {

inline Vx2DVector Zero2D() {
    return Vx2DVector(0.0f, 0.0f);
}

inline VxVector Zero3D() {
    return VxVector(0.0f, 0.0f, 0.0f);
}

inline float GetTimeMs(CKTimeManager *time) {
    return time ? time->GetTime() : 0.0f;
}

inline float GetAbsoluteTimeMs(CKTimeManager *time) {
    return time ? time->GetAbsoluteTime() : 0.0f;
}

inline float GetDeltaTimeMs(CKTimeManager *time) {
    return time ? time->GetLastDeltaTime() : 0.0f;
}

inline unsigned int GetFrameCount(CKTimeManager *time) {
    return time ? static_cast<unsigned int>(time->GetMainTickCount()) : 0;
}

inline bool IsInputValid(InputHook *input) {
    return input != nullptr;
}

inline void EnableKeyboardRepetition(InputHook *input, bool enable) {
    if (input)
        input->EnableKeyboardRepetition(enable ? TRUE : FALSE);
}

inline bool IsKeyboardRepetitionEnabled(InputHook *input) {
    return input && input->IsKeyboardRepetitionEnabled();
}

inline bool IsKeyboardAttached(InputHook *input) {
    return input && input->IsKeyboardAttached();
}

inline bool IsMouseAttached(InputHook *input) {
    return input && input->IsMouseAttached();
}

inline bool IsJoystickIndexValid(int joystick) {
    return joystick >= CKJOY_1 && joystick < CKMAX_JOY;
}

inline bool IsJoystickButtonIndexValid(int button) {
    return button >= 0 && button < 32;
}

inline bool IsKeyboardKeyIndexValid(CKKEYBOARD key) {
    const int code = static_cast<int>(key);
    return code >= 0 && code < 256;
}

inline bool IsJoystickAttached(InputHook *input, int joystick) {
    return input && IsJoystickIndexValid(joystick) && input->IsJoystickAttached(joystick);
}

inline bool IsKeyDown(InputHook *input, CKKEYBOARD key) {
    return input && IsKeyboardKeyIndexValid(key) && input->IsKeyDown(static_cast<CKDWORD>(key));
}

inline bool IsKeyDown(InputHook *input, CKKEYBOARD key, unsigned int &stamp) {
    stamp = 0;
    if (!input || !IsKeyboardKeyIndexValid(key))
        return false;
    CKDWORD nativeStamp = 0;
    const bool result = input->IsKeyDown(static_cast<CKDWORD>(key), &nativeStamp) != FALSE;
    stamp = static_cast<unsigned int>(nativeStamp);
    return result;
}

inline bool IsKeyUp(InputHook *input, CKKEYBOARD key) {
    return input && IsKeyboardKeyIndexValid(key) && input->IsKeyUp(static_cast<CKDWORD>(key));
}

inline bool IsKeyPressed(InputHook *input, CKKEYBOARD key) {
    return input && IsKeyboardKeyIndexValid(key) && input->IsKeyPressed(static_cast<CKDWORD>(key));
}

inline bool IsKeyReleased(InputHook *input, CKKEYBOARD key) {
    return input && IsKeyboardKeyIndexValid(key) && input->IsKeyReleased(static_cast<CKDWORD>(key));
}

inline bool IsKeyToggled(InputHook *input, CKKEYBOARD key) {
    return input && IsKeyboardKeyIndexValid(key) && input->IsKeyToggled(static_cast<CKDWORD>(key));
}

inline bool IsKeyToggled(InputHook *input, CKKEYBOARD key, unsigned int &stamp) {
    stamp = 0;
    if (!input || !IsKeyboardKeyIndexValid(key))
        return false;
    CKDWORD nativeStamp = 0;
    const bool result = input->IsKeyToggled(static_cast<CKDWORD>(key), &nativeStamp) != FALSE;
    stamp = static_cast<unsigned int>(nativeStamp);
    return result;
}

inline std::string GetKeyName(InputHook *input, CKKEYBOARD key) {
    if (!input || !IsKeyboardKeyIndexValid(key))
        return {};
    char buffer[128] = {};
    input->GetKeyName(static_cast<CKDWORD>(key), buffer);
    return buffer;
}

inline int GetKeyFromName(InputHook *input, const std::string &name) {
    return input ? static_cast<int>(input->GetKeyFromName(const_cast<char *>(name.c_str()))) : 0;
}

inline int GetKeyboardState(InputHook *input, CKKEYBOARD key) {
    if (!input || !IsKeyboardKeyIndexValid(key))
        return 0;
    const int code = static_cast<int>(key);
    const unsigned char *state = input->GetKeyboardState();
    return state ? static_cast<int>(state[code]) : 0;
}

inline bool IsKeyboardStateDown(InputHook *input, CKKEYBOARD key) {
    return (GetKeyboardState(input, key) & 0x80) != 0;
}

inline int GetNumberOfKeyInBuffer(InputHook *input) {
    return input ? input->GetNumberOfKeyInBuffer() : 0;
}

inline int GetKeyFromBuffer(InputHook *input, int index, CKKEYBOARD &key, unsigned int &timestamp) {
    key = static_cast<CKKEYBOARD>(0);
    timestamp = 0;
    if (!input)
        return 0;
    CKDWORD nativeKey = 0;
    CKDWORD nativeTimestamp = 0;
    const int state = input->GetKeyFromBuffer(index, nativeKey, &nativeTimestamp);
    key = static_cast<CKKEYBOARD>(nativeKey);
    timestamp = static_cast<unsigned int>(nativeTimestamp);
    return state;
}

inline bool IsMouseButtonDown(InputHook *input, CK_MOUSEBUTTON button) {
    return input && input->IsMouseButtonDown(button);
}

inline bool IsMouseClicked(InputHook *input, CK_MOUSEBUTTON button) {
    return input && input->IsMouseClicked(button);
}

inline bool IsMouseToggled(InputHook *input, CK_MOUSEBUTTON button) {
    return input && input->IsMouseToggled(button);
}

inline int GetMouseButtonState(InputHook *input, CK_MOUSEBUTTON button) {
    if (!input)
        return 0;
    const int index = static_cast<int>(button);
    if (index < 0 || index >= 4)
        return 0;
    CKBYTE states[4] = {};
    input->GetMouseButtonsState(states);
    return static_cast<int>(states[index]);
}

inline Vx2DVector GetMousePosition(InputHook *input, bool absolute) {
    Vx2DVector position = Zero2D();
    if (input)
        input->GetMousePosition(position, absolute ? TRUE : FALSE);
    return position;
}

inline Vx2DVector GetLastMousePosition(InputHook *input) {
    Vx2DVector position = Zero2D();
    if (input)
        input->GetLastMousePosition(position);
    return position;
}

inline VxVector GetMouseRelativePosition(InputHook *input) {
    VxVector position = Zero3D();
    if (input)
        input->GetMouseRelativePosition(position);
    return position;
}

inline VxVector GetJoystickPosition(InputHook *input, int joystick) {
    VxVector position = Zero3D();
    if (input && IsJoystickIndexValid(joystick))
        input->GetJoystickPosition(joystick, &position);
    return position;
}

inline VxVector GetJoystickRotation(InputHook *input, int joystick) {
    VxVector rotation = Zero3D();
    if (input && IsJoystickIndexValid(joystick))
        input->GetJoystickRotation(joystick, &rotation);
    return rotation;
}

inline Vx2DVector GetJoystickSliders(InputHook *input, int joystick) {
    Vx2DVector sliders = Zero2D();
    if (input && IsJoystickIndexValid(joystick))
        input->GetJoystickSliders(joystick, &sliders);
    return sliders;
}

inline float GetJoystickPointOfViewAngle(InputHook *input, int joystick) {
    float angle = 0.0f;
    if (input && IsJoystickIndexValid(joystick))
        input->GetJoystickPointOfViewAngle(joystick, &angle);
    return angle;
}

inline unsigned int GetJoystickButtonsState(InputHook *input, int joystick) {
    return input && IsJoystickIndexValid(joystick)
               ? static_cast<unsigned int>(input->GetJoystickButtonsState(joystick))
               : 0;
}

inline bool IsJoystickButtonDown(InputHook *input, int joystick, int button) {
    return input && IsJoystickIndexValid(joystick) && IsJoystickButtonIndexValid(button) &&
           input->IsJoystickButtonDown(joystick, button);
}

inline void PauseInput(InputHook *input, bool pause) {
    if (input)
        input->Pause(pause ? TRUE : FALSE);
}

inline void ShowCursor(InputHook *input, bool show) {
    if (input)
        input->ShowCursor(show ? TRUE : FALSE);
}

inline bool GetCursorVisibility(InputHook *input) {
    return input && input->GetCursorVisibility();
}

inline int GetSystemCursor(InputHook *input) {
    return input ? static_cast<int>(input->GetSystemCursor()) : 0;
}

inline void SetSystemCursor(InputHook *input, int cursor) {
    if (input)
        input->SetSystemCursor(static_cast<VXCURSOR_POINTER>(cursor));
}

inline bool IsBlock(InputHook *input) {
    return input && input->IsBlock();
}

inline void SetBlock(InputHook *input, bool block) {
    if (input)
        input->SetBlock(block);
}

inline int IsBlocked(InputHook *input, int device) {
    return input ? input->IsBlocked(static_cast<CK_INPUT_DEVICE>(device)) : 0;
}

inline void Block(InputHook *input, int device) {
    if (input)
        input->Block(static_cast<CK_INPUT_DEVICE>(device));
}

inline void Unblock(InputHook *input, int device) {
    if (input)
        input->Unblock(static_cast<CK_INPUT_DEVICE>(device));
}

inline uint64_t AcquireBlock(InputHook *input, uint32_t mask) {
    return input ? input->AcquireBlock(mask) : 0;
}

inline void ReleaseBlock(InputHook *input, uint64_t token) {
    if (input)
        input->ReleaseBlock(token);
}

inline bool IsObjectValid(CKObject *object) {
    return object != nullptr;
}

inline int GetObjectId(CKObject *object) {
    return object ? static_cast<int>(object->GetID()) : 0;
}

inline std::string GetObjectName(CKObject *object) {
    CKSTRING name = object ? object->GetName() : nullptr;
    return name ? name : "";
}

inline int GetObjectClassId(CKObject *object) {
    return object ? static_cast<int>(object->GetClassID()) : 0;
}

inline bool IsObjectVisible(CKObject *object) {
    return object && object->IsVisible();
}

inline bool IsObjectDynamic(CKObject *object) {
    return object && object->IsDynamic();
}

inline int GetBeObjectPriority(CKBeObject *object) {
    return object ? object->GetPriority() : 0;
}

inline int GetBeObjectScriptCount(CKBeObject *object) {
    return object ? object->GetScriptCount() : 0;
}

inline int GetBeObjectAttributeCount(CKBeObject *object) {
    return object ? object->GetAttributeCount() : 0;
}

inline VxVector Get3dEntityPosition(CK3dEntity *entity) {
    VxVector position = Zero3D();
    if (entity)
        entity->GetPosition(&position);
    return position;
}

inline VxVector Get3dEntityScale(CK3dEntity *entity, bool local) {
    VxVector scale = Zero3D();
    if (entity)
        entity->GetScale(&scale, local ? TRUE : FALSE);
    return scale;
}

inline int Get3dEntityChildCount(CK3dEntity *entity) {
    return entity ? entity->GetChildrenCount() : 0;
}

inline CK3dEntity *Get3dEntityParent(CK3dEntity *entity) {
    return entity ? entity->GetParent() : nullptr;
}

} // namespace ScriptFacadeAccess
} // namespace BML

#endif

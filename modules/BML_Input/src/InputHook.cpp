#include "InputHook.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "HookUtils.h"
#include "VTables.h"

#include "bml_topics.h"

namespace BML_Input {
namespace {

const BML_ImcBusInterface *s_ImcBus = nullptr;
const BML_CoreLoggingInterface *s_Logging = nullptr;
BML_Mod s_Owner = nullptr;

struct KeyDownEvent {
    uint32_t key_code;
    uint32_t scan_code;
    uint32_t timestamp;
    bool repeat;
};

struct KeyUpEvent {
    uint32_t key_code;
    uint32_t scan_code;
    uint32_t timestamp;
};

struct MouseMoveEvent {
    float x;
    float y;
    float rel_x;
    float rel_y;
    bool absolute;
};

struct MouseButtonEvent {
    uint32_t button;
    bool down;
    uint32_t timestamp;
};

struct InputState {
    bool initialized = false;
    CKInputManager *input_manager = nullptr;
    CP_CLASS_VTABLE_NAME(CKInputManager)<CKInputManager> vtable = {};
    int blocked_devices[INPUT_DEVICE_COUNT] = {};
    unsigned char last_keyboard_state[256] = {};
    Vx2DVector last_mouse_position = {0.0f, 0.0f};
    CKBYTE last_mouse_buttons[4] = {};
    BML_TopicId topic_key_down = 0;
    BML_TopicId topic_key_up = 0;
    BML_TopicId topic_mouse_button = 0;
    BML_TopicId topic_mouse_move = 0;
};

InputState g_State;

bool PublishInputMessage(BML_TopicId topic, const void *data, size_t size) {
    if (!s_ImcBus || !s_ImcBus->Publish || !s_Owner || topic == 0) {
        return false;
    }
    return s_ImcBus->Publish(s_Owner, topic, data, size) == BML_RESULT_OK;
}

struct InputManagerHook {
    CP_DECLARE_METHOD_HOOK(CKERROR, PostProcess, ()) {
        return CK_OK;
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsKeyDown, (CKDWORD key, CKDWORD *stamp)) {
        if (IsDeviceBlocked(INPUT_DEVICE_KEYBOARD))
            return FALSE;
        return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.IsKeyDown, key, stamp);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsKeyUp, (CKDWORD key)) {
        if (IsDeviceBlocked(INPUT_DEVICE_KEYBOARD))
            return FALSE;
        return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.IsKeyUp, key);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsKeyToggled, (CKDWORD key, CKDWORD *stamp)) {
        if (IsDeviceBlocked(INPUT_DEVICE_KEYBOARD))
            return FALSE;
        return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.IsKeyToggled, key, stamp);
    }

    CP_DECLARE_METHOD_HOOK(unsigned char *, GetKeyboardState, ()) {
        static unsigned char keyboard_state[256] = {};
        if (IsDeviceBlocked(INPUT_DEVICE_KEYBOARD))
            return keyboard_state;
        return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.GetKeyboardState);
    }

    CP_DECLARE_METHOD_HOOK(int, GetNumberOfKeyInBuffer, ()) {
        if (IsDeviceBlocked(INPUT_DEVICE_KEYBOARD))
            return 0;
        return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.GetNumberOfKeyInBuffer);
    }

    CP_DECLARE_METHOD_HOOK(int, GetKeyFromBuffer, (int index, CKDWORD &outKey, CKDWORD *outStamp)) {
        if (IsDeviceBlocked(INPUT_DEVICE_KEYBOARD))
            return NO_KEY;
        return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.GetKeyFromBuffer, index, outKey, outStamp);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsMouseButtonDown, (CK_MOUSEBUTTON button)) {
        if (IsDeviceBlocked(INPUT_DEVICE_MOUSE))
            return FALSE;
        return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.IsMouseButtonDown, button);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsMouseClicked, (CK_MOUSEBUTTON button)) {
        if (IsDeviceBlocked(INPUT_DEVICE_MOUSE))
            return FALSE;
        return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.IsMouseClicked, button);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsMouseToggled, (CK_MOUSEBUTTON button)) {
        if (IsDeviceBlocked(INPUT_DEVICE_MOUSE))
            return FALSE;
        return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.IsMouseToggled, button);
    }

    CP_DECLARE_METHOD_HOOK(void, GetMouseButtonsState, (CKBYTE outStates[4])) {
        if (IsDeviceBlocked(INPUT_DEVICE_MOUSE)) {
            std::memset(outStates, KS_IDLE, sizeof(CKBYTE) * 4);
            return;
        }
        CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.GetMouseButtonsState, outStates);
    }

    CP_DECLARE_METHOD_HOOK(void, GetMousePosition, (Vx2DVector &outPosition, CKBOOL absolute)) {
        if (IsDeviceBlocked(INPUT_DEVICE_MOUSE))
            return;
        CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.GetMousePosition, outPosition, absolute);
    }

    CP_DECLARE_METHOD_HOOK(void, GetMouseRelativePosition, (VxVector &outPosition)) {
        if (IsDeviceBlocked(INPUT_DEVICE_MOUSE))
            return;
        CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.GetMouseRelativePosition, outPosition);
    }

    CP_DECLARE_METHOD_HOOK(void, GetJoystickPosition, (int joystick, VxVector *outPosition)) {
        if (IsDeviceBlocked(INPUT_DEVICE_JOYSTICK))
            return;
        CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.GetJoystickPosition, joystick, outPosition);
    }

    CP_DECLARE_METHOD_HOOK(void, GetJoystickRotation, (int joystick, VxVector *outRotation)) {
        if (IsDeviceBlocked(INPUT_DEVICE_JOYSTICK))
            return;
        CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.GetJoystickRotation, joystick, outRotation);
    }

    CP_DECLARE_METHOD_HOOK(void, GetJoystickSliders, (int joystick, Vx2DVector *outPosition)) {
        if (IsDeviceBlocked(INPUT_DEVICE_JOYSTICK))
            return;
        CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.GetJoystickSliders, joystick, outPosition);
    }

    CP_DECLARE_METHOD_HOOK(void, GetJoystickPointOfViewAngle, (int joystick, float *outAngle)) {
        if (IsDeviceBlocked(INPUT_DEVICE_JOYSTICK))
            return;
        CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.GetJoystickPointOfViewAngle, joystick, outAngle);
    }

    CP_DECLARE_METHOD_HOOK(CKDWORD, GetJoystickButtonsState, (int joystick)) {
        if (IsDeviceBlocked(INPUT_DEVICE_JOYSTICK))
            return 0;
        return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.GetJoystickButtonsState, joystick);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsJoystickButtonDown, (int joystick, int button)) {
        if (IsDeviceBlocked(INPUT_DEVICE_JOYSTICK))
            return FALSE;
        return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.IsJoystickButtonDown, joystick, button);
    }
};

void InstallVTableHooks() {
#define HOOK_INPUT_METHOD(Name) \
    utils::HookVirtualMethod(g_State.input_manager, &InputManagerHook::CP_FUNC_HOOK_NAME(Name), \
        (offsetof(CP_CLASS_VTABLE_NAME(CKInputManager)<CKInputManager>, Name) / sizeof(void *)))

    HOOK_INPUT_METHOD(PostProcess);
    HOOK_INPUT_METHOD(IsKeyDown);
    HOOK_INPUT_METHOD(IsKeyUp);
    HOOK_INPUT_METHOD(IsKeyToggled);
    HOOK_INPUT_METHOD(GetKeyboardState);
    HOOK_INPUT_METHOD(GetNumberOfKeyInBuffer);
    HOOK_INPUT_METHOD(GetKeyFromBuffer);
    HOOK_INPUT_METHOD(IsMouseButtonDown);
    HOOK_INPUT_METHOD(IsMouseClicked);
    HOOK_INPUT_METHOD(IsMouseToggled);
    HOOK_INPUT_METHOD(GetMouseButtonsState);
    HOOK_INPUT_METHOD(GetMousePosition);
    HOOK_INPUT_METHOD(GetMouseRelativePosition);
    HOOK_INPUT_METHOD(GetJoystickPosition);
    HOOK_INPUT_METHOD(GetJoystickRotation);
    HOOK_INPUT_METHOD(GetJoystickSliders);
    HOOK_INPUT_METHOD(GetJoystickPointOfViewAngle);
    HOOK_INPUT_METHOD(GetJoystickButtonsState);
    HOOK_INPUT_METHOD(IsJoystickButtonDown);

#undef HOOK_INPUT_METHOD
}

CKERROR PostProcessOriginal() {
    return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.PostProcess);
}

CKBOOL IsKeyDownOriginal(CKDWORD key, CKDWORD *stamp) {
    return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.IsKeyDown, key, stamp);
}

CKBOOL IsKeyUpOriginal(CKDWORD key) {
    return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.IsKeyUp, key);
}

CKBOOL IsKeyToggledOriginal(CKDWORD key, CKDWORD *stamp) {
    return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.IsKeyToggled, key, stamp);
}

unsigned char *GetKeyboardStateOriginal() {
    return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.GetKeyboardState);
}

int GetNumberOfKeyInBufferOriginal() {
    return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.GetNumberOfKeyInBuffer);
}

int GetKeyFromBufferOriginal(int index, CKDWORD &outKey, CKDWORD *outStamp) {
    return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.GetKeyFromBuffer, index, outKey, outStamp);
}

CKBOOL IsMouseButtonDownOriginal(CK_MOUSEBUTTON button) {
    return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.IsMouseButtonDown, button);
}

CKBOOL IsMouseClickedOriginal(CK_MOUSEBUTTON button) {
    return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.IsMouseClicked, button);
}

CKBOOL IsMouseToggledOriginal(CK_MOUSEBUTTON button) {
    return CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.IsMouseToggled, button);
}

void GetMouseButtonsStateOriginal(CKBYTE outStates[4]) {
    CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.GetMouseButtonsState, outStates);
}

void GetMousePositionOriginal(Vx2DVector &outPosition, CKBOOL absolute) {
    CP_CALL_METHOD_PTR(g_State.input_manager, g_State.vtable.GetMousePosition, outPosition, absolute);
}

void PublishKeyboardEvents(unsigned char *currentState) {
    if (!s_ImcBus || !g_State.topic_key_down || !g_State.topic_key_up || !currentState)
        return;

    for (int index = 0; index < 256; ++index) {
        if (currentState[index] && !g_State.last_keyboard_state[index]) {
            KeyDownEvent event{};
            event.key_code = static_cast<uint32_t>(index);
            if (PublishInputMessage(g_State.topic_key_down, &event, sizeof(event))) {
                g_State.last_keyboard_state[index] = currentState[index];
            }
        } else if (!currentState[index] && g_State.last_keyboard_state[index]) {
            KeyUpEvent event{};
            event.key_code = static_cast<uint32_t>(index);
            if (PublishInputMessage(g_State.topic_key_up, &event, sizeof(event))) {
                g_State.last_keyboard_state[index] = currentState[index];
            }
        } else {
            g_State.last_keyboard_state[index] = currentState[index];
        }
    }
}

void PublishMouseEvents() {
    if (!s_ImcBus || !g_State.topic_mouse_button || !g_State.topic_mouse_move)
        return;

    CKBYTE mouseStates[4] = {};
    GetMouseButtonsStateOriginal(mouseStates);

    for (int button = 0; button < 3; ++button) {
        const bool wasDown = (g_State.last_mouse_buttons[button] == KS_PRESSED) ||
            (g_State.last_mouse_buttons[button] != KS_IDLE && g_State.last_mouse_buttons[button] != KS_RELEASED);
        const bool isDown = (mouseStates[button] == KS_PRESSED) ||
            (mouseStates[button] != KS_IDLE && mouseStates[button] != KS_RELEASED);

        if (isDown != wasDown) {
            MouseButtonEvent event{};
            event.button = static_cast<uint32_t>(button);
            event.down = isDown;
            if (PublishInputMessage(g_State.topic_mouse_button, &event, sizeof(event))) {
                g_State.last_mouse_buttons[button] = mouseStates[button];
            }
        } else {
            g_State.last_mouse_buttons[button] = mouseStates[button];
        }
    }

    Vx2DVector currentPos{};
    GetMousePositionOriginal(currentPos, FALSE);
    if (currentPos.x != g_State.last_mouse_position.x || currentPos.y != g_State.last_mouse_position.y) {
        MouseMoveEvent event{};
        event.x = currentPos.x;
        event.y = currentPos.y;
        event.rel_x = currentPos.x - g_State.last_mouse_position.x;
        event.rel_y = currentPos.y - g_State.last_mouse_position.y;
        event.absolute = false;
        if (PublishInputMessage(g_State.topic_mouse_move, &event, sizeof(event))) {
            g_State.last_mouse_position = currentPos;
        }
    } else {
        g_State.last_mouse_position = currentPos;
    }
}

} // namespace

static void HookLog(BML_LogSeverity severity, const char *message) {
    if (!s_Logging || !s_Logging->Log || !message) return;
    s_Logging->Log(s_Owner, severity,
                   "BML_Input",
                   "%s", message);
}

bool InitInputHook(CKInputManager *inputManager,
                   const BML_ImcBusInterface *imc,
                   const BML_CoreLoggingInterface *logging,
                   BML_Mod owner) {
    if (g_State.initialized)
        return true;

    s_ImcBus = imc;
    s_Logging = logging;
    s_Owner = owner;

    if (!inputManager) {
        HookLog(BML_LOG_ERROR, "Cannot initialize input hook: CKInputManager is null");
        return false;
    }

    g_State.input_manager = inputManager;
    utils::LoadVTable<CP_CLASS_VTABLE_NAME(CKInputManager)<CKInputManager>>(g_State.input_manager, g_State.vtable);

    if (s_ImcBus && s_ImcBus->GetTopicId) {
        s_ImcBus->GetTopicId(s_ImcBus->Context, BML_TOPIC_INPUT_KEY_DOWN, &g_State.topic_key_down);
        s_ImcBus->GetTopicId(s_ImcBus->Context, BML_TOPIC_INPUT_KEY_UP, &g_State.topic_key_up);
        s_ImcBus->GetTopicId(
            s_ImcBus->Context, BML_TOPIC_INPUT_MOUSE_BUTTON, &g_State.topic_mouse_button);
        s_ImcBus->GetTopicId(
            s_ImcBus->Context, BML_TOPIC_INPUT_MOUSE_MOVE, &g_State.topic_mouse_move);
    }

    InstallVTableHooks();

    std::memset(g_State.blocked_devices, 0, sizeof(g_State.blocked_devices));
    std::memset(g_State.last_keyboard_state, 0, sizeof(g_State.last_keyboard_state));
    std::memset(g_State.last_mouse_buttons, 0, sizeof(g_State.last_mouse_buttons));
    g_State.last_mouse_position = {0.0f, 0.0f};
    g_State.initialized = true;

    HookLog(BML_LOG_INFO, "Input hooks initialized successfully");
    return true;
}

void ShutdownInputHook() {
    if (!g_State.initialized)
        return;

    if (g_State.input_manager) {
        utils::SaveVTable<CP_CLASS_VTABLE_NAME(CKInputManager)<CKInputManager>>(g_State.input_manager, g_State.vtable);
    }

    HookLog(BML_LOG_INFO, "Input hooks shutdown");
    g_State = {};
    s_ImcBus = nullptr;
    s_Logging = nullptr;
    s_Owner = nullptr;
}

void EnableKeyboardRepetition(CKBOOL enable) {
    if (g_State.input_manager)
        g_State.input_manager->EnableKeyboardRepetition(enable);
}

CKBOOL IsKeyboardRepetitionEnabled() {
    return g_State.input_manager ? g_State.input_manager->IsKeyboardRepetitionEnabled() : FALSE;
}

CKBOOL IsKeyDown(CKDWORD key, CKDWORD *stamp) {
    return g_State.input_manager ? g_State.input_manager->IsKeyDown(key, stamp) : FALSE;
}

CKBOOL IsKeyUp(CKDWORD key) {
    return g_State.input_manager ? g_State.input_manager->IsKeyUp(key) : FALSE;
}

CKBOOL IsKeyToggled(CKDWORD key, CKDWORD *stamp) {
    return g_State.input_manager ? g_State.input_manager->IsKeyToggled(key, stamp) : FALSE;
}

CKBOOL IsKeyPressed(CKDWORD key) {
    if (IsDeviceBlocked(INPUT_DEVICE_KEYBOARD) || !g_State.input_manager)
        return FALSE;
    return IsKeyDownOriginal(key, nullptr) && !g_State.last_keyboard_state[key];
}

CKBOOL IsKeyReleased(CKDWORD key) {
    if (IsDeviceBlocked(INPUT_DEVICE_KEYBOARD) || !g_State.input_manager)
        return FALSE;
    return IsKeyToggledOriginal(key, nullptr) && g_State.last_keyboard_state[key];
}

void GetKeyName(CKDWORD key, CKSTRING outKeyName) {
    if (g_State.input_manager)
        g_State.input_manager->GetKeyName(key, outKeyName);
}

CKDWORD GetKeyFromName(CKSTRING keyName) {
    return g_State.input_manager ? g_State.input_manager->GetKeyFromName(keyName) : 0;
}

unsigned char *GetKeyboardState() {
    return g_State.input_manager ? g_State.input_manager->GetKeyboardState() : nullptr;
}

CKBOOL IsKeyboardAttached() {
    return g_State.input_manager ? g_State.input_manager->IsKeyboardAttached() : FALSE;
}

int GetNumberOfKeyInBuffer() {
    return g_State.input_manager ? g_State.input_manager->GetNumberOfKeyInBuffer() : 0;
}

int GetKeyFromBuffer(int index, CKDWORD &outKey, CKDWORD *outTimestamp) {
    return g_State.input_manager ? g_State.input_manager->GetKeyFromBuffer(index, outKey, outTimestamp) : NO_KEY;
}

CKBOOL IsMouseButtonDown(CK_MOUSEBUTTON button) {
    return g_State.input_manager ? g_State.input_manager->IsMouseButtonDown(button) : FALSE;
}

CKBOOL IsMouseClicked(CK_MOUSEBUTTON button) {
    return g_State.input_manager ? g_State.input_manager->IsMouseClicked(button) : FALSE;
}

CKBOOL IsMouseToggled(CK_MOUSEBUTTON button) {
    return g_State.input_manager ? g_State.input_manager->IsMouseToggled(button) : FALSE;
}

void GetMouseButtonsState(CKBYTE outStates[4]) {
    if (g_State.input_manager)
        g_State.input_manager->GetMouseButtonsState(outStates);
}

void GetMousePosition(Vx2DVector &outPosition, CKBOOL absolute) {
    if (g_State.input_manager)
        g_State.input_manager->GetMousePosition(outPosition, absolute);
}

void GetMouseRelativePosition(VxVector &outPosition) {
    if (g_State.input_manager)
        g_State.input_manager->GetMouseRelativePosition(outPosition);
}

void GetLastMousePosition(Vx2DVector &position) {
    position = g_State.last_mouse_position;
}

CKBOOL IsMouseAttached() {
    return g_State.input_manager ? g_State.input_manager->IsMouseAttached() : FALSE;
}

CKBOOL IsJoystickAttached(int joystick) {
    return g_State.input_manager ? g_State.input_manager->IsJoystickAttached(joystick) : FALSE;
}

void GetJoystickPosition(int joystick, VxVector *outPosition) {
    if (g_State.input_manager)
        g_State.input_manager->GetJoystickPosition(joystick, outPosition);
}

void GetJoystickRotation(int joystick, VxVector *outRotation) {
    if (g_State.input_manager)
        g_State.input_manager->GetJoystickRotation(joystick, outRotation);
}

void GetJoystickSliders(int joystick, Vx2DVector *outPosition) {
    if (g_State.input_manager)
        g_State.input_manager->GetJoystickSliders(joystick, outPosition);
}

void GetJoystickPointOfViewAngle(int joystick, float *outAngle) {
    if (g_State.input_manager)
        g_State.input_manager->GetJoystickPointOfViewAngle(joystick, outAngle);
}

CKDWORD GetJoystickButtonsState(int joystick) {
    return g_State.input_manager ? g_State.input_manager->GetJoystickButtonsState(joystick) : 0;
}

CKBOOL IsJoystickButtonDown(int joystick, int button) {
    return g_State.input_manager ? g_State.input_manager->IsJoystickButtonDown(joystick, button) : FALSE;
}

void Pause(CKBOOL pause) {
    if (g_State.input_manager)
        g_State.input_manager->Pause(pause);
}

void ShowCursor(CKBOOL show) {
    if (g_State.input_manager)
        g_State.input_manager->ShowCursor(show);
}

CKBOOL GetCursorVisibility() {
    return g_State.input_manager ? g_State.input_manager->GetCursorVisibility() : FALSE;
}

VXCURSOR_POINTER GetSystemCursor() {
    return g_State.input_manager ? g_State.input_manager->GetSystemCursor() : static_cast<VXCURSOR_POINTER>(0);
}

void SetSystemCursor(VXCURSOR_POINTER cursor) {
    if (g_State.input_manager)
        g_State.input_manager->SetSystemCursor(cursor);
}

void BlockDevice(InputDevice device) {
    if (device >= 0 && device < INPUT_DEVICE_COUNT)
        ++g_State.blocked_devices[device];
}

void UnblockDevice(InputDevice device) {
    if (device >= 0 && device < INPUT_DEVICE_COUNT && g_State.blocked_devices[device] > 0)
        --g_State.blocked_devices[device];
}

int IsDeviceBlocked(InputDevice device) {
    if (device >= 0 && device < INPUT_DEVICE_COUNT)
        return g_State.blocked_devices[device];
    return 0;
}

void ProcessInput() {
    if (!g_State.initialized || !g_State.input_manager)
        return;

    PostProcessOriginal();

    unsigned char *currentKeyboardState = GetKeyboardStateOriginal();
    if (currentKeyboardState) {
        PublishKeyboardEvents(currentKeyboardState);
    }

    PublishMouseEvents();
}

bool IsInputHookActive() {
    return g_State.initialized;
}

} // namespace BML_Input

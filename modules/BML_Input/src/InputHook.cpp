/**
 * @file InputHook.cpp
 * @brief CKInputManager VTable Hook Implementation for BML_Input Module
 * 
 * Self-contained input hook that intercepts CKInputManager methods
 * and publishes input events via IMC.
 */

#include "InputHook.h"
#include "VTables.h"
#include "HookUtils.h"

#include "bml_loader.h"
#include "bml_topics.h"

#include <cstring>

namespace BML_Input {

//-----------------------------------------------------------------------------
// Input Event Structures (published via IMC)
//-----------------------------------------------------------------------------

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
    uint32_t button;  // 0=left, 1=right, 2=middle
    bool down;
    uint32_t timestamp;
};

//-----------------------------------------------------------------------------
// Static State
//-----------------------------------------------------------------------------

static bool s_Initialized = false;
static CKInputManager *s_InputManager = nullptr;
static CP_CLASS_VTABLE_NAME(CKInputManager)<CKInputManager> s_VTable = {};

// Device blocking
static int s_BlockedDevices[INPUT_DEVICE_COUNT] = {0};

// State tracking for event detection
static unsigned char s_LastKeyboardState[256] = {};
static Vx2DVector s_LastMousePosition = {0, 0};
static CKBYTE s_LastMouseButtons[4] = {0};

// Cached topic IDs
static BML_TopicId s_TopicKeyDown = 0;
static BML_TopicId s_TopicKeyUp = 0;
static BML_TopicId s_TopicMouseButton = 0;
static BML_TopicId s_TopicMouseMove = 0;

//-----------------------------------------------------------------------------
// VTable Method Hooks
//-----------------------------------------------------------------------------

// Hook class to intercept CKInputManager methods
struct InputManagerHook {
    // PostProcess hook - called each frame
    CP_DECLARE_METHOD_HOOK(CKERROR, PostProcess, ()) {
        return CK_OK;
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsKeyDown, (CKDWORD iKey, CKDWORD *oStamp)) {
        if (s_BlockedDevices[INPUT_DEVICE_KEYBOARD] > 0)
            return FALSE;
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.IsKeyDown, iKey, oStamp);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsKeyUp, (CKDWORD iKey)) {
        if (s_BlockedDevices[INPUT_DEVICE_KEYBOARD] > 0)
            return FALSE;
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.IsKeyUp, iKey);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsKeyToggled, (CKDWORD iKey, CKDWORD *oStamp)) {
        if (s_BlockedDevices[INPUT_DEVICE_KEYBOARD] > 0)
            return FALSE;
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.IsKeyToggled, iKey, oStamp);
    }

    CP_DECLARE_METHOD_HOOK(unsigned char *, GetKeyboardState, ()) {
        static unsigned char emptyState[256] = {};
        if (s_BlockedDevices[INPUT_DEVICE_KEYBOARD] > 0)
            return emptyState;
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetKeyboardState);
    }

    CP_DECLARE_METHOD_HOOK(int, GetNumberOfKeyInBuffer, ()) {
        if (s_BlockedDevices[INPUT_DEVICE_KEYBOARD] > 0)
            return 0;
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetNumberOfKeyInBuffer);
    }

    CP_DECLARE_METHOD_HOOK(int, GetKeyFromBuffer, (int i, CKDWORD &oKey, CKDWORD *oTimeStamp)) {
        if (s_BlockedDevices[INPUT_DEVICE_KEYBOARD] > 0)
            return NO_KEY;
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetKeyFromBuffer, i, oKey, oTimeStamp);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsMouseButtonDown, (CK_MOUSEBUTTON iButton)) {
        if (s_BlockedDevices[INPUT_DEVICE_MOUSE] > 0)
            return FALSE;
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.IsMouseButtonDown, iButton);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsMouseClicked, (CK_MOUSEBUTTON iButton)) {
        if (s_BlockedDevices[INPUT_DEVICE_MOUSE] > 0)
            return FALSE;
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.IsMouseClicked, iButton);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsMouseToggled, (CK_MOUSEBUTTON iButton)) {
        if (s_BlockedDevices[INPUT_DEVICE_MOUSE] > 0)
            return FALSE;
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.IsMouseToggled, iButton);
    }

    CP_DECLARE_METHOD_HOOK(void, GetMouseButtonsState, (CKBYTE oStates[4])) {
        if (s_BlockedDevices[INPUT_DEVICE_MOUSE] > 0) {
            memset(oStates, KS_IDLE, sizeof(CKBYTE) * 4);
            return;
        }
        CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetMouseButtonsState, oStates);
    }

    CP_DECLARE_METHOD_HOOK(void, GetMousePosition, (Vx2DVector &oPosition, CKBOOL iAbsolute)) {
        if (s_BlockedDevices[INPUT_DEVICE_MOUSE] > 0)
            return;
        CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetMousePosition, oPosition, iAbsolute);
    }

    CP_DECLARE_METHOD_HOOK(void, GetMouseRelativePosition, (VxVector &oPosition)) {
        if (s_BlockedDevices[INPUT_DEVICE_MOUSE] > 0)
            return;
        CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetMouseRelativePosition, oPosition);
    }

    CP_DECLARE_METHOD_HOOK(void, GetJoystickPosition, (int iJoystick, VxVector *oPosition)) {
        if (s_BlockedDevices[INPUT_DEVICE_JOYSTICK] > 0)
            return;
        CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetJoystickPosition, iJoystick, oPosition);
    }

    CP_DECLARE_METHOD_HOOK(void, GetJoystickRotation, (int iJoystick, VxVector *oRotation)) {
        if (s_BlockedDevices[INPUT_DEVICE_JOYSTICK] > 0)
            return;
        CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetJoystickRotation, iJoystick, oRotation);
    }

    CP_DECLARE_METHOD_HOOK(void, GetJoystickSliders, (int iJoystick, Vx2DVector *oPosition)) {
        if (s_BlockedDevices[INPUT_DEVICE_JOYSTICK] > 0)
            return;
        CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetJoystickSliders, iJoystick, oPosition);
    }

    CP_DECLARE_METHOD_HOOK(void, GetJoystickPointOfViewAngle, (int iJoystick, float *oAngle)) {
        if (s_BlockedDevices[INPUT_DEVICE_JOYSTICK] > 0)
            return;
        CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetJoystickPointOfViewAngle, iJoystick, oAngle);
    }

    CP_DECLARE_METHOD_HOOK(CKDWORD, GetJoystickButtonsState, (int iJoystick)) {
        if (s_BlockedDevices[INPUT_DEVICE_JOYSTICK] > 0)
            return 0;
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetJoystickButtonsState, iJoystick);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsJoystickButtonDown, (int iJoystick, int iButton)) {
        if (s_BlockedDevices[INPUT_DEVICE_JOYSTICK] > 0)
            return FALSE;
        return CP_CALL_METHOD_PTR(s_InputManager, s_VTable.IsJoystickButtonDown, iJoystick, iButton);
    }
};

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

static void InstallVTableHooks() {
#define HOOK_INPUT_METHOD(Name) \
    utils::HookVirtualMethod(s_InputManager, &InputManagerHook::CP_FUNC_HOOK_NAME(Name), \
                             (offsetof(CP_CLASS_VTABLE_NAME(CKInputManager)<CKInputManager>, Name) / sizeof(void*)))

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

static void PublishKeyboardEvents(unsigned char *currentState) {
    if (!s_TopicKeyDown || !s_TopicKeyUp)
        return;

    for (int i = 0; i < 256; ++i) {
        // Key pressed (down transition)
        if (currentState[i] && !s_LastKeyboardState[i]) {
            KeyDownEvent event;
            event.key_code = i;
            event.scan_code = 0;
            event.timestamp = 0;
            event.repeat = false;
            bmlImcPublish(s_TopicKeyDown, &event, sizeof(event));
        }
        // Key released (up transition)
        else if (!currentState[i] && s_LastKeyboardState[i]) {
            KeyUpEvent event;
            event.key_code = i;
            event.scan_code = 0;
            event.timestamp = 0;
            bmlImcPublish(s_TopicKeyUp, &event, sizeof(event));
        }
    }
}

static void PublishMouseEvents() {
    if (!s_TopicMouseButton || !s_TopicMouseMove)
        return;

    // Get current mouse button states
    CKBYTE mouseStates[4];
    CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetMouseButtonsState, mouseStates);

    // Check button state changes
    for (int button = 0; button < 3; ++button) {
        bool wasDown = (s_LastMouseButtons[button] == KS_PRESSED) || 
                       (s_LastMouseButtons[button] != KS_IDLE && s_LastMouseButtons[button] != KS_RELEASED);
        bool isDown = (mouseStates[button] == KS_PRESSED) || 
                      (mouseStates[button] != KS_IDLE && mouseStates[button] != KS_RELEASED);

        if (isDown != wasDown) {
            MouseButtonEvent event;
            event.button = button;
            event.down = isDown;
            event.timestamp = 0;
            bmlImcPublish(s_TopicMouseButton, &event, sizeof(event));
        }
    }

    // Get current mouse position
    Vx2DVector currentPos;
    CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetMousePosition, currentPos, FALSE);

    // Check mouse movement
    if (currentPos.x != s_LastMousePosition.x || currentPos.y != s_LastMousePosition.y) {
        MouseMoveEvent event;
        event.x = currentPos.x;
        event.y = currentPos.y;
        event.rel_x = currentPos.x - s_LastMousePosition.x;
        event.rel_y = currentPos.y - s_LastMousePosition.y;
        event.absolute = false;
        bmlImcPublish(s_TopicMouseMove, &event, sizeof(event));
    }

    // Update last states
    memcpy(s_LastMouseButtons, mouseStates, sizeof(s_LastMouseButtons));
    s_LastMousePosition = currentPos;
}

//-----------------------------------------------------------------------------
// Public API
//-----------------------------------------------------------------------------

bool InitInputHook(CKInputManager *inputManager) {
    if (s_Initialized)
        return true;

    if (!inputManager) {
        bmlLog(bmlGetGlobalContext(), BML_LOG_ERROR, "BML_Input",
               "Cannot initialize input hook: CKInputManager is null");
        return false;
    }

    s_InputManager = inputManager;

    // Save original VTable
    utils::LoadVTable<CP_CLASS_VTABLE_NAME(CKInputManager)<CKInputManager>>(s_InputManager, s_VTable);

    // Register topic IDs
    bmlImcGetTopicId(BML_TOPIC_INPUT_KEY_DOWN, &s_TopicKeyDown);
    bmlImcGetTopicId(BML_TOPIC_INPUT_KEY_UP, &s_TopicKeyUp);
    bmlImcGetTopicId(BML_TOPIC_INPUT_MOUSE_BUTTON, &s_TopicMouseButton);
    bmlImcGetTopicId(BML_TOPIC_INPUT_MOUSE_MOVE, &s_TopicMouseMove);

    // Install hooks
    InstallVTableHooks();

    // Initialize state
    memset(s_BlockedDevices, 0, sizeof(s_BlockedDevices));
    memset(s_LastKeyboardState, 0, sizeof(s_LastKeyboardState));
    memset(s_LastMouseButtons, 0, sizeof(s_LastMouseButtons));
    s_LastMousePosition = {0, 0};

    s_Initialized = true;
    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Input",
           "Input hooks initialized successfully");
    return true;
}

void ShutdownInputHook() {
    if (!s_Initialized)
        return;

    // Restore original VTable
    if (s_InputManager) {
        utils::SaveVTable<CP_CLASS_VTABLE_NAME(CKInputManager)<CKInputManager>>(s_InputManager, s_VTable);
    }

    s_InputManager = nullptr;
    s_Initialized = false;

    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_Input",
           "Input hooks shutdown");
}

void BlockDevice(InputDevice device) {
    if (device < INPUT_DEVICE_COUNT) {
        ++s_BlockedDevices[device];
    }
}

void UnblockDevice(InputDevice device) {
    if (device < INPUT_DEVICE_COUNT && s_BlockedDevices[device] > 0) {
        --s_BlockedDevices[device];
    }
}

int IsDeviceBlocked(InputDevice device) {
    if (device < INPUT_DEVICE_COUNT) {
        return s_BlockedDevices[device];
    }
    return 0;
}

void ProcessInput() {
    if (!s_Initialized || !s_InputManager)
        return;

    // Call original PostProcess
    CP_CALL_METHOD_PTR(s_InputManager, s_VTable.PostProcess);

    // Get current keyboard state and publish events
    unsigned char *currentKeyState = CP_CALL_METHOD_PTR(s_InputManager, s_VTable.GetKeyboardState);
    if (currentKeyState) {
        PublishKeyboardEvents(currentKeyState);
        memcpy(s_LastKeyboardState, currentKeyState, sizeof(s_LastKeyboardState));
    }

    // Publish mouse events
    PublishMouseEvents();
}

bool IsInputHookActive() {
    return s_Initialized;
}

} // namespace BML_Input

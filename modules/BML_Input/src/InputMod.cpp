/**
 * @file InputMod.cpp
 * @brief BML_Input Module -- input hook + capture coordinator + module entry
 *
 * This module:
 * - Hooks CKInputManager to intercept input
 * - Publishes input events via IMC (keyboard, mouse)
 * - Provides token-based input capture service for device blocking
 */

#define BML_LOADER_IMPLEMENTATION
#include "bml_hook_module.hpp"
#include "bml_imc_topic.hpp"
#include "bml_input.h"
#include "bml_input_control.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <new>
#include <thread>
#include <unordered_map>

#include "InputHook.h"

#include "HookUtils.h"
#include "VTables.h"

#include "bml_services.hpp"
#include "bml_topics.h"

#include "CKContext.h"

// ---------------------------------------------------------------------------
// File-scope services pointer -- set by the module before InitInputHook,
// cleared after ShutdownInputHook.  Atomic for portability (prevents
// pointer tearing on weakly-ordered architectures); actual access pattern
// is single-threaded (main thread only).
// ---------------------------------------------------------------------------
static std::atomic<const bml::ModuleServices *> s_Services{nullptr};

// ===================================================================
// Anonymous namespace: hook state, vtable hooks, event publishing
// ===================================================================
namespace {

struct InputState {
    bool initialized = false;
    CKInputManager *input_manager = nullptr;
    CKInputManagerVTable<CKInputManager> vtable = {};
    int blocked_devices[BML_Input::INPUT_DEVICE_COUNT] = {};
    unsigned char last_keyboard_state[256] = {};
    Vx2DVector last_mouse_position = {0.0f, 0.0f};
    CKBYTE last_mouse_buttons[4] = {};
    bml::imc::Topic topic_key_down;
    bml::imc::Topic topic_key_up;
    bml::imc::Topic topic_mouse_button;
    bml::imc::Topic topic_mouse_move;
};

InputState g_State;

uint32_t GetInputTimestampMs() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

bool PublishInputMessage(const bml::imc::Topic &topic, const void *data, size_t size) {
    return topic.Publish(data, size);
}

// ---------- VTable hooks ----------

struct InputManagerHook {
    CKERROR PostProcessHook() {
        return CK_OK;
    }

    CKBOOL IsKeyDownHook(CKDWORD key, CKDWORD *stamp) {
        if (BML_Input::IsDeviceBlocked(BML_Input::INPUT_DEVICE_KEYBOARD))
            return FALSE;
        return (g_State.input_manager->*g_State.vtable.IsKeyDown)(key, stamp);
    }

    CKBOOL IsKeyUpHook(CKDWORD key) {
        if (BML_Input::IsDeviceBlocked(BML_Input::INPUT_DEVICE_KEYBOARD))
            return FALSE;
        return (g_State.input_manager->*g_State.vtable.IsKeyUp)(key);
    }

    CKBOOL IsKeyToggledHook(CKDWORD key, CKDWORD *stamp) {
        if (BML_Input::IsDeviceBlocked(BML_Input::INPUT_DEVICE_KEYBOARD))
            return FALSE;
        return (g_State.input_manager->*g_State.vtable.IsKeyToggled)(key, stamp);
    }

    unsigned char *GetKeyboardStateHook() {
        static unsigned char keyboard_state[256] = {};
        if (BML_Input::IsDeviceBlocked(BML_Input::INPUT_DEVICE_KEYBOARD))
            return keyboard_state;
        return (g_State.input_manager->*g_State.vtable.GetKeyboardState)();
    }

    int GetNumberOfKeyInBufferHook() {
        if (BML_Input::IsDeviceBlocked(BML_Input::INPUT_DEVICE_KEYBOARD))
            return 0;
        return (g_State.input_manager->*g_State.vtable.GetNumberOfKeyInBuffer)();
    }

    int GetKeyFromBufferHook(int index, CKDWORD &outKey, CKDWORD *outStamp) {
        if (BML_Input::IsDeviceBlocked(BML_Input::INPUT_DEVICE_KEYBOARD))
            return NO_KEY;
        return (g_State.input_manager->*g_State.vtable.GetKeyFromBuffer)(index, outKey, outStamp);
    }

    CKBOOL IsMouseButtonDownHook(CK_MOUSEBUTTON button) {
        if (BML_Input::IsDeviceBlocked(BML_Input::INPUT_DEVICE_MOUSE))
            return FALSE;
        return (g_State.input_manager->*g_State.vtable.IsMouseButtonDown)(button);
    }

    CKBOOL IsMouseClickedHook(CK_MOUSEBUTTON button) {
        if (BML_Input::IsDeviceBlocked(BML_Input::INPUT_DEVICE_MOUSE))
            return FALSE;
        return (g_State.input_manager->*g_State.vtable.IsMouseClicked)(button);
    }

    CKBOOL IsMouseToggledHook(CK_MOUSEBUTTON button) {
        if (BML_Input::IsDeviceBlocked(BML_Input::INPUT_DEVICE_MOUSE))
            return FALSE;
        return (g_State.input_manager->*g_State.vtable.IsMouseToggled)(button);
    }

    void GetMouseButtonsStateHook(CKBYTE outStates[4]) {
        if (BML_Input::IsDeviceBlocked(BML_Input::INPUT_DEVICE_MOUSE)) {
            std::memset(outStates, KS_IDLE, sizeof(CKBYTE) * 4);
            return;
        }
        (g_State.input_manager->*g_State.vtable.GetMouseButtonsState)(outStates);
    }

    void GetMousePositionHook(Vx2DVector &outPosition, CKBOOL absolute) {
        if (BML_Input::IsDeviceBlocked(BML_Input::INPUT_DEVICE_MOUSE))
            return;
        (g_State.input_manager->*g_State.vtable.GetMousePosition)(outPosition, absolute);
    }

    void GetMouseRelativePositionHook(VxVector &outPosition) {
        if (BML_Input::IsDeviceBlocked(BML_Input::INPUT_DEVICE_MOUSE))
            return;
        (g_State.input_manager->*g_State.vtable.GetMouseRelativePosition)(outPosition);
    }

    void GetJoystickPositionHook(int joystick, VxVector *outPosition) {
        if (BML_Input::IsDeviceBlocked(BML_Input::INPUT_DEVICE_JOYSTICK))
            return;
        (g_State.input_manager->*g_State.vtable.GetJoystickPosition)(joystick, outPosition);
    }

    void GetJoystickRotationHook(int joystick, VxVector *outRotation) {
        if (BML_Input::IsDeviceBlocked(BML_Input::INPUT_DEVICE_JOYSTICK))
            return;
        (g_State.input_manager->*g_State.vtable.GetJoystickRotation)(joystick, outRotation);
    }

    void GetJoystickSlidersHook(int joystick, Vx2DVector *outPosition) {
        if (BML_Input::IsDeviceBlocked(BML_Input::INPUT_DEVICE_JOYSTICK))
            return;
        (g_State.input_manager->*g_State.vtable.GetJoystickSliders)(joystick, outPosition);
    }

    void GetJoystickPointOfViewAngleHook(int joystick, float *outAngle) {
        if (BML_Input::IsDeviceBlocked(BML_Input::INPUT_DEVICE_JOYSTICK))
            return;
        (g_State.input_manager->*g_State.vtable.GetJoystickPointOfViewAngle)(joystick, outAngle);
    }

    CKDWORD GetJoystickButtonsStateHook(int joystick) {
        if (BML_Input::IsDeviceBlocked(BML_Input::INPUT_DEVICE_JOYSTICK))
            return 0;
        return (g_State.input_manager->*g_State.vtable.GetJoystickButtonsState)(joystick);
    }

    CKBOOL IsJoystickButtonDownHook(int joystick, int button) {
        if (BML_Input::IsDeviceBlocked(BML_Input::INPUT_DEVICE_JOYSTICK))
            return FALSE;
        return (g_State.input_manager->*g_State.vtable.IsJoystickButtonDown)(joystick, button);
    }
};

void InstallVTableHooks() {
#define HOOK_INPUT_METHOD(Name) \
    utils::HookVirtualMethod(g_State.input_manager, &InputManagerHook::Name##Hook, \
        (offsetof(CKInputManagerVTable<CKInputManager>, Name) / sizeof(void *)))

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

// ---------- Original (unhooked) call-throughs ----------

CKERROR PostProcessOriginal() {
    return (g_State.input_manager->*g_State.vtable.PostProcess)();
}

CKBOOL IsKeyDownOriginal(CKDWORD key, CKDWORD *stamp) {
    return (g_State.input_manager->*g_State.vtable.IsKeyDown)(key, stamp);
}

CKBOOL IsKeyUpOriginal(CKDWORD key) {
    return (g_State.input_manager->*g_State.vtable.IsKeyUp)(key);
}

CKBOOL IsKeyToggledOriginal(CKDWORD key, CKDWORD *stamp) {
    return (g_State.input_manager->*g_State.vtable.IsKeyToggled)(key, stamp);
}

unsigned char *GetKeyboardStateOriginal() {
    return (g_State.input_manager->*g_State.vtable.GetKeyboardState)();
}

int GetNumberOfKeyInBufferOriginal() {
    return (g_State.input_manager->*g_State.vtable.GetNumberOfKeyInBuffer)();
}

int GetKeyFromBufferOriginal(int index, CKDWORD &outKey, CKDWORD *outStamp) {
    return (g_State.input_manager->*g_State.vtable.GetKeyFromBuffer)(index, outKey, outStamp);
}

CKBOOL IsMouseButtonDownOriginal(CK_MOUSEBUTTON button) {
    return (g_State.input_manager->*g_State.vtable.IsMouseButtonDown)(button);
}

CKBOOL IsMouseClickedOriginal(CK_MOUSEBUTTON button) {
    return (g_State.input_manager->*g_State.vtable.IsMouseClicked)(button);
}

CKBOOL IsMouseToggledOriginal(CK_MOUSEBUTTON button) {
    return (g_State.input_manager->*g_State.vtable.IsMouseToggled)(button);
}

void GetMouseButtonsStateOriginal(CKBYTE outStates[4]) {
    (g_State.input_manager->*g_State.vtable.GetMouseButtonsState)(outStates);
}

void GetMousePositionOriginal(Vx2DVector &outPosition, CKBOOL absolute) {
    (g_State.input_manager->*g_State.vtable.GetMousePosition)(outPosition, absolute);
}

// ---------- Event publishing ----------

void PublishKeyboardEvents(unsigned char *currentState) {
    if (!g_State.topic_key_down || !g_State.topic_key_up || !currentState)
        return;

    const uint32_t timestamp = GetInputTimestampMs();

    // Edge-detected key transitions.  Track which keys were just pressed
    // this frame so the buffer scan below can distinguish repeats.
    bool pressedThisFrame[256] = {};

    for (int index = 0; index < 256; ++index) {
        const bool isDown = currentState[index] != 0;
        const bool wasDown = g_State.last_keyboard_state[index] != 0;

        if (isDown && !wasDown) {
            pressedThisFrame[index] = true;
            BML_KeyDownEvent event{};
            event.key_code = static_cast<uint32_t>(index);
            event.scan_code = static_cast<uint32_t>(index);  // CK uses DIK scan codes as key IDs
            event.timestamp = timestamp;
            event.repeat = false;
            PublishInputMessage(g_State.topic_key_down, &event, sizeof(event));
        } else if (!isDown && wasDown) {
            BML_KeyUpEvent event{};
            event.key_code = static_cast<uint32_t>(index);
            event.scan_code = static_cast<uint32_t>(index);
            event.timestamp = timestamp;
            PublishInputMessage(g_State.topic_key_up, &event, sizeof(event));
        }
        g_State.last_keyboard_state[index] = currentState[index];
    }

    // Buffered repeat events (requires EnableKeyboardRepetition).
    // The buffer contains both initial presses and repeats as KEY_PRESSED.
    // We skip keys that just transitioned this frame (already emitted above).
    const int bufferCount = GetNumberOfKeyInBufferOriginal();
    for (int i = 0; i < bufferCount; ++i) {
        CKDWORD key = 0;
        CKDWORD stamp = 0;
        int result = GetKeyFromBufferOriginal(i, key, &stamp);
        if (result != KEY_PRESSED || key == 0 || key >= 256)
            continue;
        if (pressedThisFrame[key])
            continue;
        BML_KeyDownEvent event{};
        event.key_code = key;
        event.scan_code = key;
        event.timestamp = timestamp;
        event.repeat = true;
        PublishInputMessage(g_State.topic_key_down, &event, sizeof(event));
    }
}

void PublishMouseEvents() {
    if (!g_State.topic_mouse_button || !g_State.topic_mouse_move)
        return;

    const uint32_t timestamp = GetInputTimestampMs();
    CKBYTE mouseStates[4] = {};
    GetMouseButtonsStateOriginal(mouseStates);

    for (int button = 0; button < 4; ++button) {
        const bool wasDown = (g_State.last_mouse_buttons[button] == KS_PRESSED) ||
            (g_State.last_mouse_buttons[button] != KS_IDLE && g_State.last_mouse_buttons[button] != KS_RELEASED);
        const bool isDown = (mouseStates[button] == KS_PRESSED) ||
            (mouseStates[button] != KS_IDLE && mouseStates[button] != KS_RELEASED);

        if (isDown != wasDown) {
            BML_MouseButtonEvent event{};
            event.button = static_cast<uint32_t>(button);
            event.down = isDown;
            event.timestamp = timestamp;
            PublishInputMessage(g_State.topic_mouse_button, &event, sizeof(event));
        }
        g_State.last_mouse_buttons[button] = mouseStates[button];
    }

    Vx2DVector currentPos{};
    GetMousePositionOriginal(currentPos, FALSE);
    if (currentPos.x != g_State.last_mouse_position.x || currentPos.y != g_State.last_mouse_position.y) {
        BML_MouseMoveEvent event{};
        event.x = currentPos.x;
        event.y = currentPos.y;
        event.rel_x = currentPos.x - g_State.last_mouse_position.x;
        event.rel_y = currentPos.y - g_State.last_mouse_position.y;
        event.absolute = false;
        event.timestamp = timestamp;
        PublishInputMessage(g_State.topic_mouse_move, &event, sizeof(event));
    }
    g_State.last_mouse_position = currentPos;
}

} // anonymous namespace

// ===================================================================
// BML_Input namespace -- public API implementations (declared in InputHook.h)
// ===================================================================
namespace BML_Input {

bool InitInputHook(CKInputManager *inputManager) {
    if (g_State.initialized)
        return true;

    if (!inputManager) {
        auto *svc = s_Services.load(std::memory_order_acquire);
        if (svc)
            svc->Log().Error("Cannot initialize input hook: CKInputManager is null");
        return false;
    }

    g_State.input_manager = inputManager;
    utils::LoadVTable<CKInputManagerVTable<CKInputManager>>(g_State.input_manager, g_State.vtable);

    auto *svc = s_Services.load(std::memory_order_acquire);
    if (svc) {
        auto *imcBus = svc->Interfaces().ImcBus;
        auto owner = svc->Handle();
        g_State.topic_key_down = bml::imc::Topic(BML_TOPIC_INPUT_KEY_DOWN, imcBus, owner);
        g_State.topic_key_up = bml::imc::Topic(BML_TOPIC_INPUT_KEY_UP, imcBus, owner);
        g_State.topic_mouse_button = bml::imc::Topic(BML_TOPIC_INPUT_MOUSE_BUTTON, imcBus, owner);
        g_State.topic_mouse_move = bml::imc::Topic(BML_TOPIC_INPUT_MOUSE_MOVE, imcBus, owner);
    }

    InstallVTableHooks();

    std::memset(g_State.blocked_devices, 0, sizeof(g_State.blocked_devices));
    std::memset(g_State.last_keyboard_state, 0, sizeof(g_State.last_keyboard_state));
    std::memset(g_State.last_mouse_buttons, 0, sizeof(g_State.last_mouse_buttons));
    g_State.last_mouse_position = {0.0f, 0.0f};
    g_State.initialized = true;

    if (svc)
        svc->Log().Info("Input hooks initialized successfully");
    return true;
}

void ShutdownInputHook() {
    if (!g_State.initialized)
        return;

    if (g_State.input_manager) {
        utils::SaveVTable<CKInputManagerVTable<CKInputManager>>(g_State.input_manager, g_State.vtable);
    }

    auto *svc = s_Services.load(std::memory_order_acquire);
    if (svc)
        svc->Log().Info("Input hooks shutdown");
    g_State = {};
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

// ===================================================================
// Input capture coordinator + trampoline functions
// ===================================================================
struct BML_InputCaptureToken_T {
    uint64_t id{0};
};

namespace {
class InputCaptureCoordinator {
    static constexpr size_t kMaxCaptures = 64;

public:
    void SetMainThread(std::thread::id thread_id) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_MainThread = thread_id;
    }

    void Reset() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (auto &[id, record] : m_Captures) {
            delete record.token;
        }
        m_Captures.clear();
        m_NextTokenId = 1;
        ApplyStateLocked(0u, -1);
    }

    BML_Result Acquire(const BML_InputCaptureDesc *desc, BML_InputCaptureToken *out_token) {
        if (!desc || desc->struct_size < sizeof(BML_InputCaptureDesc) || !out_token) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        BML_Result thread_result = CheckThread();
        if (thread_result != BML_RESULT_OK) {
            return thread_result;
        }

        // Lock before allocation so kMaxCaptures check is atomic with insert.
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_Captures.size() >= kMaxCaptures) {
            return BML_RESULT_WOULD_BLOCK;
        }

        auto *token = new (std::nothrow) BML_InputCaptureToken_T{};
        if (!token) {
            return BML_RESULT_OUT_OF_MEMORY;
        }

        token->id = m_NextTokenId++;
        m_Captures.emplace(token->id, CaptureRecord{
            token->id,
            m_NextSequence++,
            desc->flags,
            desc->cursor_visible,
            desc->priority,
            token
        });
        RecomputeLocked();

        *out_token = reinterpret_cast<BML_InputCaptureToken>(token);
        return BML_RESULT_OK;
    }

    BML_Result Update(BML_InputCaptureToken token, const BML_InputCaptureDesc *desc) {
        if (!token || !desc || desc->struct_size < sizeof(BML_InputCaptureDesc)) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        BML_Result thread_result = CheckThread();
        if (thread_result != BML_RESULT_OK) {
            return thread_result;
        }

        std::lock_guard<std::mutex> lock(m_Mutex);
        auto *token_impl = reinterpret_cast<BML_InputCaptureToken_T *>(token);
        auto it = m_Captures.find(token_impl->id);
        if (it == m_Captures.end()) {
            return BML_RESULT_INVALID_HANDLE;
        }

        it->second.flags = desc->flags;
        it->second.cursor_visible = desc->cursor_visible;
        it->second.priority = desc->priority;
        it->second.sequence = m_NextSequence++;
        RecomputeLocked();
        return BML_RESULT_OK;
    }

    BML_Result Release(BML_InputCaptureToken token) {
        if (!token) {
            return BML_RESULT_INVALID_HANDLE;
        }
        BML_Result thread_result = CheckThread();
        if (thread_result != BML_RESULT_OK) {
            return thread_result;
        }

        auto *token_impl = reinterpret_cast<BML_InputCaptureToken_T *>(token);
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_Captures.find(token_impl->id);
        if (it == m_Captures.end()) {
            return BML_RESULT_INVALID_HANDLE;
        }

        m_Captures.erase(it);
        RecomputeLocked();
        delete token_impl;
        return BML_RESULT_OK;
    }

    BML_Result Query(BML_InputCaptureState *out_state) const {
        if (!out_state || out_state->struct_size < sizeof(BML_InputCaptureState)) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        std::lock_guard<std::mutex> lock(m_Mutex);
        out_state->effective_flags = m_AppliedFlags;
        out_state->cursor_visible = m_AppliedCursorVisible;
        return BML_RESULT_OK;
    }

private:
    struct CaptureRecord {
        uint64_t id{0};
        uint64_t sequence{0};
        uint32_t flags{0};
        int cursor_visible{-1};
        int32_t priority{0};
        BML_InputCaptureToken_T *token{nullptr};  // Owning. Deleted by Release() after erase, or by Reset() in bulk.
    };

    BML_Result CheckThread() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_MainThread == std::thread::id{} || std::this_thread::get_id() == m_MainThread) {
            return BML_RESULT_OK;
        }
        return BML_RESULT_WRONG_THREAD;
    }

    void RecomputeLocked() {
        uint32_t effective_flags = 0u;
        int cursor_visible = m_AppliedCursorVisible;
        bool have_cursor_owner = false;
        int32_t best_priority = 0;
        uint64_t best_sequence = 0;

        for (const auto &[id, capture] : m_Captures) {
            (void) id;
            effective_flags |= capture.flags;
            if (capture.cursor_visible < 0) {
                continue;
            }

            if (!have_cursor_owner ||
                capture.priority > best_priority ||
                (capture.priority == best_priority && capture.sequence > best_sequence)) {
                have_cursor_owner = true;
                best_priority = capture.priority;
                best_sequence = capture.sequence;
                cursor_visible = capture.cursor_visible;
            }
        }

        if (!have_cursor_owner) {
            cursor_visible = -1;
        }

        ApplyStateLocked(effective_flags, cursor_visible);
    }

    void ApplyStateLocked(uint32_t effective_flags, int cursor_visible) {
        const bool wants_keyboard = (effective_flags & BML_INPUT_CAPTURE_FLAG_BLOCK_KEYBOARD) != 0;
        const bool wants_mouse = (effective_flags & BML_INPUT_CAPTURE_FLAG_BLOCK_MOUSE) != 0;
        const bool has_cursor_override = cursor_visible >= 0;

        const bool keyboard_applied = (m_AppliedFlags & BML_INPUT_CAPTURE_FLAG_BLOCK_KEYBOARD) != 0;
        const bool mouse_applied = (m_AppliedFlags & BML_INPUT_CAPTURE_FLAG_BLOCK_MOUSE) != 0;

        if (wants_keyboard != keyboard_applied) {
            if (wants_keyboard) {
                BML_Input::BlockDevice(BML_Input::INPUT_DEVICE_KEYBOARD);
            } else {
                BML_Input::UnblockDevice(BML_Input::INPUT_DEVICE_KEYBOARD);
            }
        }

        if (wants_mouse != mouse_applied) {
            if (wants_mouse) {
                BML_Input::BlockDevice(BML_Input::INPUT_DEVICE_MOUSE);
            } else {
                BML_Input::UnblockDevice(BML_Input::INPUT_DEVICE_MOUSE);
            }
        }

        if (has_cursor_override && cursor_visible != m_AppliedCursorVisible) {
            BML_Input::ShowCursor(cursor_visible ? TRUE : FALSE);
        } else if (!has_cursor_override && m_AppliedCursorVisible >= 0) {
            BML_Input::ShowCursor(FALSE);
        }

        m_AppliedFlags = effective_flags;
        m_AppliedCursorVisible = cursor_visible;
    }

    mutable std::mutex m_Mutex;
    std::thread::id m_MainThread;
    uint64_t m_NextTokenId{1};
    uint64_t m_NextSequence{1};
    std::unordered_map<uint64_t, CaptureRecord> m_Captures;
    uint32_t m_AppliedFlags{0u};
    int m_AppliedCursorVisible{-1};
};

InputCaptureCoordinator &GetCaptureCoordinator() {
    static InputCaptureCoordinator coordinator;
    return coordinator;
}

BML_Result Input_AcquireCapture(const BML_InputCaptureDesc *desc, BML_InputCaptureToken *out_token) {
    return GetCaptureCoordinator().Acquire(desc, out_token);
}

BML_Result Input_UpdateCapture(BML_InputCaptureToken token, const BML_InputCaptureDesc *desc) {
    return GetCaptureCoordinator().Update(token, desc);
}

BML_Result Input_ReleaseCapture(BML_InputCaptureToken token) {
    return GetCaptureCoordinator().Release(token);
}

BML_Result Input_QueryEffectiveState(BML_InputCaptureState *out_state) {
    return GetCaptureCoordinator().Query(out_state);
}

const BML_InputCaptureInterface kInputCaptureService = {
    BML_IFACE_HEADER(BML_InputCaptureInterface, BML_INPUT_CAPTURE_INTERFACE_ID, 1, 0),
    Input_AcquireCapture,
    Input_UpdateCapture,
    Input_ReleaseCapture,
    Input_QueryEffectiveState
};
} // anonymous namespace

// ===================================================================
// Module class
// ===================================================================
class InputMod : public bml::HookModule {
    bml::PublishedInterface m_Published;

    const char *HookLogCategory() const override { return "BML_Input"; }

    bool InitHook(CKContext *ctx) override {
        auto *im = static_cast<CKInputManager *>(
            ctx->GetManagerByGuid(INPUT_MANAGER_GUID));
        if (!im) {
            Services().Log().Warn("CKInputManager not available yet - retrying next Engine/Init");
            return false;
        }
        s_Services.store(&Services(), std::memory_order_release);
        return BML_Input::InitInputHook(im);
    }

    void ShutdownHook() override {
        BML_Input::ShutdownInputHook();
        s_Services.store(nullptr, std::memory_order_release);
    }

    BML_Result OnModuleAttach() override {
        m_Subs.Add(BML_TOPIC_ENGINE_POST_PROCESS, [](const bml::imc::Message &) {
            BML_Input::ProcessInput();
        });

        GetCaptureCoordinator().SetMainThread(std::this_thread::get_id());

        m_Published = Publish(BML_INPUT_CAPTURE_INTERFACE_ID,
                              &kInputCaptureService,
                              1,
                              0,
                              0,
                              BML_INTERFACE_FLAG_TOKENIZED | BML_INTERFACE_FLAG_IMMUTABLE);
        if (!m_Published) {
            Services().Log().Error("Failed to register input capture service");
            return BML_RESULT_FAIL;
        }

        return BML_RESULT_OK;
    }

    void OnModuleDetach() override {
        (void)m_Published.Reset();
        GetCaptureCoordinator().Reset();
    }
};

BML_DEFINE_MODULE(InputMod)

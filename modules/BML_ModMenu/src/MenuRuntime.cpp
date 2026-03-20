#include "CKBehavior.h"
#include "CKScene.h"

#include "MenuInputState.h"
#include "MenuRuntime.h"

namespace MenuRuntime {
namespace {
CKContext *g_Context = nullptr;
const BML_InputCaptureInterface *g_InputService = nullptr;
BML_InputCaptureToken g_InputToken = nullptr;
BML::ModMenu::MenuInputState g_InputState;

void ReleaseKeyboard() {
    if (!g_InputService || !g_InputToken) {
        return;
    }

    g_InputService->ReleaseCapture(g_InputToken);
    g_InputToken = nullptr;
}
} // namespace

void Initialize(CKContext *context, const BML_InputCaptureInterface *inputService) {
    g_Context = context;
    g_InputService = inputService;
}

void Reset() {
    ReleaseKeyboard();
    g_Context = nullptr;
    g_InputService = nullptr;
    g_InputState.Reset();
}

void SetInputService(const BML_InputCaptureInterface *inputService) {
    g_InputService = inputService;
}

void BlockInput() {
    if (!g_InputService) {
        return;
    }

    g_InputState.CancelPendingRelease();

    if (!g_InputToken) {
        BML_InputCaptureDesc capture = BML_INPUT_CAPTURE_DESC_INIT;
        capture.flags = BML_INPUT_CAPTURE_FLAG_BLOCK_KEYBOARD;
        capture.cursor_visible = 1;
        capture.priority = 100;
        g_InputService->AcquireCapture(&capture, &g_InputToken);
    }
}

void ActivateScript(const char *scriptName) {
    if (!g_Context || !scriptName || !*scriptName) {
        return;
    }

    auto *behavior = reinterpret_cast<CKBehavior *>(g_Context->GetObjectByNameAndClass((CKSTRING) scriptName, CKCID_BEHAVIOR));
    if (!behavior || !g_Context->GetCurrentScene()) {
        return;
    }

    g_Context->GetCurrentScene()->Activate(behavior, TRUE);
}

void BeginInputRelease() {
    if (g_InputState.BeginRelease()) {
        ReleaseKeyboard();
    }
}

void TransitionToScriptAndUnblock(const char *scriptName) {
    ActivateScript(scriptName);
    BeginInputRelease();
}

void OnKeyDown(uint32_t keyCode) {
    g_InputState.OnKeyDown(keyCode);
}

void OnKeyUp(uint32_t keyCode) {
    if (g_InputState.OnKeyUp(keyCode)) {
        ReleaseKeyboard();
    }
}
} // namespace MenuRuntime

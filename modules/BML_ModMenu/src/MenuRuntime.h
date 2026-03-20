#ifndef BML_MODMENU_RUNTIME_H
#define BML_MODMENU_RUNTIME_H

#include <cstdint>

#include "CKContext.h"

#include "bml_input_control.h"

namespace MenuRuntime {
    void Initialize(CKContext *context, const BML_InputCaptureInterface *inputService);
    void Reset();
    void SetInputService(const BML_InputCaptureInterface *inputService);
    void BlockInput();
    void ActivateScript(const char *scriptName);
    void BeginInputRelease();
    void TransitionToScriptAndUnblock(const char *scriptName);
    void OnKeyDown(uint32_t keyCode);
    void OnKeyUp(uint32_t keyCode);
}

#endif // BML_MODMENU_RUNTIME_H

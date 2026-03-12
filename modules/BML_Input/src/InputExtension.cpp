/**
 * @file InputExtension.cpp
 * @brief BML_EXT_Input Extension Implementation
 * 
 * Provides extension API for other modules to interact with input system.
 */

#include "InputExtension.h"
#include "InputHook.h"

// Device visibility state
static bool g_CursorVisible = true;

// Extension API implementations
static void Input_BlockDevice(BML_InputDevice device) {
    BML_Input::BlockDevice(static_cast<BML_Input::InputDevice>(device));
}

static void Input_UnblockDevice(BML_InputDevice device) {
    BML_Input::UnblockDevice(static_cast<BML_Input::InputDevice>(device));
}

static bool Input_IsDeviceBlocked(BML_InputDevice device) {
    return BML_Input::IsDeviceBlocked(static_cast<BML_Input::InputDevice>(device)) > 0;
}

static void Input_ShowCursor(bool show) {
    g_CursorVisible = show;
    // TODO: Integrate with actual cursor visibility control
}

static bool Input_GetCursorVisibility() {
    return g_CursorVisible;
}

// Global extension instance
static BML_InputExtension g_InputExtension = {
    1,  // major version
    0,  // minor version
    Input_BlockDevice,
    Input_UnblockDevice,
    Input_IsDeviceBlocked,
    Input_ShowCursor,
    Input_GetCursorVisibility
};

BML_InputExtension* GetInputExtension() {
    return &g_InputExtension;
}
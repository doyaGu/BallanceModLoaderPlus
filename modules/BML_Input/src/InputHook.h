/**
 * @file InputHook.h
 * @brief CKInputManager VTable Hook for BML_Input Module
 * 
 * Self-contained input hook implementation that:
 * - Intercepts CKInputManager VTable methods
 * - Provides device blocking functionality
 * - Publishes input events via IMC
 */

#ifndef BML_INPUT_INPUTHOOK_H
#define BML_INPUT_INPUTHOOK_H

#include "CKInputManager.h"

namespace BML_Input {

/**
 * @brief Input device types
 */
enum InputDevice {
    INPUT_DEVICE_KEYBOARD = 0,
    INPUT_DEVICE_MOUSE = 1,
    INPUT_DEVICE_JOYSTICK = 2,
    INPUT_DEVICE_COUNT = 3
};

/**
 * @brief Initialize the input hook
 * 
 * Hooks CKInputManager VTable to intercept input methods and
 * publish events via IMC.
 * 
 * @param inputManager The CKInputManager instance to hook
 * @return true on success, false on failure
 */
bool InitInputHook(CKInputManager *inputManager);

/**
 * @brief Shutdown the input hook
 * 
 * Restores original VTable and cleans up resources.
 */
void ShutdownInputHook();

/**
 * @brief Block input from a device
 * @param device The device to block
 */
void BlockDevice(InputDevice device);

/**
 * @brief Unblock input from a device
 * @param device The device to unblock
 */
void UnblockDevice(InputDevice device);

/**
 * @brief Check if a device is blocked
 * @param device The device to check
 * @return Block count (0 if not blocked)
 */
int IsDeviceBlocked(InputDevice device);

/**
 * @brief Process input and publish events
 * 
 * Called each frame to detect input state changes and
 * publish corresponding IMC events.
 */
void ProcessInput();

/**
 * @brief Check if input hooks are active
 * @return true if hooks are installed
 */
bool IsInputHookActive();

} // namespace BML_Input

#endif // BML_INPUT_INPUTHOOK_H
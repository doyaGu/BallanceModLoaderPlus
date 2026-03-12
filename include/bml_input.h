#ifndef BML_INPUT_H
#define BML_INPUT_H

#include <cstdint>

typedef enum BML_InputDevice {
    BML_INPUT_DEVICE_KEYBOARD = 0,
    BML_INPUT_DEVICE_MOUSE = 1,
    BML_INPUT_DEVICE_JOYSTICK = 2,
    BML_INPUT_DEVICE_COUNT = 3,
} BML_InputDevice;

typedef struct BML_InputExtension {
    uint16_t major;
    uint16_t minor;
    void (*BlockDevice)(BML_InputDevice device);
    void (*UnblockDevice)(BML_InputDevice device);
    bool (*IsDeviceBlocked)(BML_InputDevice device);
    void (*ShowCursor)(bool show);
    bool (*GetCursorVisibility)(void);
} BML_InputExtension;

typedef struct BML_KeyDownEvent {
    uint32_t key_code;
    uint32_t scan_code;
    uint32_t timestamp;
    bool repeat;
} BML_KeyDownEvent;

typedef struct BML_KeyUpEvent {
    uint32_t key_code;
    uint32_t scan_code;
    uint32_t timestamp;
} BML_KeyUpEvent;

typedef struct BML_MouseButtonEvent {
    uint32_t button;
    bool down;
    uint32_t timestamp;
} BML_MouseButtonEvent;

typedef struct BML_MouseMoveEvent {
    float x;
    float y;
    float rel_x;
    float rel_y;
    bool absolute;
} BML_MouseMoveEvent;

#define BML_INPUT_EXTENSION_NAME "BML_EXT_Input"

#endif // BML_INPUT_H
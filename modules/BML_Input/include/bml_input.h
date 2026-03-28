#ifndef BML_INPUT_H
#define BML_INPUT_H

#include <cstdint>

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
    uint32_t timestamp;
} BML_MouseMoveEvent;

#endif // BML_INPUT_H

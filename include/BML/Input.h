#ifndef BML_INPUT_H
#define BML_INPUT_H

#include "BML/Mod.h"

BML_BEGIN_CDECLS
#define BML_INPUT_KEYBOARD_STATE_1_0_SIZE                                                          \
    (offsetof(BML_InputKeyboardState, EventCount) +                                                \
     sizeof(((BML_InputKeyboardState *)0)->EventCount))
#define BML_INPUT_KEYBOARD_STATE_INIT {.StructSize = sizeof(BML_InputKeyboardState)}
#define BML_INPUT_MOUSE_STATE_1_0_SIZE                                                             \
    (offsetof(BML_InputMouseState, ToggledMask) + sizeof(((BML_InputMouseState *)0)->ToggledMask))
#define BML_INPUT_MOUSE_STATE_INIT {.StructSize = sizeof(BML_InputMouseState)}
#define BML_INPUT_JOYSTICK_STATE_1_0_SIZE                                                          \
    (offsetof(BML_InputJoystickState, Buttons) + sizeof(((BML_InputJoystickState *)0)->Buttons))
#define BML_INPUT_JOYSTICK_STATE_INIT {.StructSize = sizeof(BML_InputJoystickState)}
#define BML_INPUT_CURSOR_STATE_1_0_SIZE                                                            \
    (offsetof(BML_InputCursorState, Shape) + sizeof(((BML_InputCursorState *)0)->Shape))
#define BML_INPUT_CURSOR_STATE_INIT {.StructSize = sizeof(BML_InputCursorState)}
#define BML_INPUT_INTERFACE_1_0_SIZE                                                               \
    (offsetof(BML_InputInterface, SetSystemCursor) +                                               \
     sizeof(((BML_InputInterface *)0)->SetSystemCursor))
#pragma pack(push, 8)
#define BML_INPUT_INTERFACE_ID "bml.input"
#define BML_INPUT_INTERFACE_MAJOR 1u
#define BML_KEY_ESCAPE 0x01u
#define BML_KEY_1 0x02u
#define BML_KEY_2 0x03u
#define BML_KEY_3 0x04u
#define BML_KEY_4 0x05u
#define BML_KEY_5 0x06u
#define BML_KEY_6 0x07u
#define BML_KEY_7 0x08u
#define BML_KEY_8 0x09u
#define BML_KEY_9 0x0Au
#define BML_KEY_0 0x0Bu
#define BML_KEY_MINUS 0x0Cu
#define BML_KEY_EQUALS 0x0Du
#define BML_KEY_BACK 0x0Eu
#define BML_KEY_TAB 0x0Fu
#define BML_KEY_Q 0x10u
#define BML_KEY_W 0x11u
#define BML_KEY_E 0x12u
#define BML_KEY_R 0x13u
#define BML_KEY_T 0x14u
#define BML_KEY_Y 0x15u
#define BML_KEY_U 0x16u
#define BML_KEY_I 0x17u
#define BML_KEY_O 0x18u
#define BML_KEY_P 0x19u
#define BML_KEY_LBRACKET 0x1Au
#define BML_KEY_RBRACKET 0x1Bu
#define BML_KEY_RETURN 0x1Cu
#define BML_KEY_LCONTROL 0x1Du
#define BML_KEY_A 0x1Eu
#define BML_KEY_S 0x1Fu
#define BML_KEY_D 0x20u
#define BML_KEY_F 0x21u
#define BML_KEY_G 0x22u
#define BML_KEY_H 0x23u
#define BML_KEY_J 0x24u
#define BML_KEY_K 0x25u
#define BML_KEY_L 0x26u
#define BML_KEY_SEMICOLON 0x27u
#define BML_KEY_APOSTROPHE 0x28u
#define BML_KEY_GRAVE 0x29u
#define BML_KEY_LSHIFT 0x2Au
#define BML_KEY_BACKSLASH 0x2Bu
#define BML_KEY_Z 0x2Cu
#define BML_KEY_X 0x2Du
#define BML_KEY_C 0x2Eu
#define BML_KEY_V 0x2Fu
#define BML_KEY_B 0x30u
#define BML_KEY_N 0x31u
#define BML_KEY_M 0x32u
#define BML_KEY_COMMA 0x33u
#define BML_KEY_PERIOD 0x34u
#define BML_KEY_SLASH 0x35u
#define BML_KEY_RSHIFT 0x36u
#define BML_KEY_MULTIPLY 0x37u
#define BML_KEY_LMENU 0x38u
#define BML_KEY_SPACE 0x39u
#define BML_KEY_CAPITAL 0x3Au
#define BML_KEY_F1 0x3Bu
#define BML_KEY_F2 0x3Cu
#define BML_KEY_F3 0x3Du
#define BML_KEY_F4 0x3Eu
#define BML_KEY_F5 0x3Fu
#define BML_KEY_F6 0x40u
#define BML_KEY_F7 0x41u
#define BML_KEY_F8 0x42u
#define BML_KEY_F9 0x43u
#define BML_KEY_F10 0x44u
#define BML_KEY_NUMLOCK 0x45u
#define BML_KEY_SCROLL 0x46u
#define BML_KEY_NUMPAD7 0x47u
#define BML_KEY_NUMPAD8 0x48u
#define BML_KEY_NUMPAD9 0x49u
#define BML_KEY_SUBTRACT 0x4Au
#define BML_KEY_NUMPAD4 0x4Bu
#define BML_KEY_NUMPAD5 0x4Cu
#define BML_KEY_NUMPAD6 0x4Du
#define BML_KEY_ADD 0x4Eu
#define BML_KEY_NUMPAD1 0x4Fu
#define BML_KEY_NUMPAD2 0x50u
#define BML_KEY_NUMPAD3 0x51u
#define BML_KEY_NUMPAD0 0x52u
#define BML_KEY_DECIMAL 0x53u
#define BML_KEY_F11 0x57u
#define BML_KEY_F12 0x58u
#define BML_KEY_F13 0x64u
#define BML_KEY_F14 0x65u
#define BML_KEY_F15 0x66u
#define BML_KEY_KANA 0x70u
#define BML_KEY_CONVERT 0x79u
#define BML_KEY_NOCONVERT 0x7Bu
#define BML_KEY_YEN 0x7Du
#define BML_KEY_NUMPADEQUALS 0x8Du
#define BML_KEY_CIRCUMFLEX 0x90u
#define BML_KEY_AT 0x91u
#define BML_KEY_COLON 0x92u
#define BML_KEY_UNDERLINE 0x93u
#define BML_KEY_KANJI 0x94u
#define BML_KEY_STOP 0x95u
#define BML_KEY_AX 0x96u
#define BML_KEY_UNLABELED 0x97u
#define BML_KEY_NUMPADENTER 0x9Cu
#define BML_KEY_RCONTROL 0x9Du
#define BML_KEY_NUMPADCOMMA 0xB3u
#define BML_KEY_DIVIDE 0xB5u
#define BML_KEY_SYSRQ 0xB7u
#define BML_KEY_RMENU 0xB8u
#define BML_KEY_HOME 0xC7u
#define BML_KEY_UP 0xC8u
#define BML_KEY_PRIOR 0xC9u
#define BML_KEY_LEFT 0xCBu
#define BML_KEY_RIGHT 0xCDu
#define BML_KEY_END 0xCFu
#define BML_KEY_DOWN 0xD0u
#define BML_KEY_NEXT 0xD1u
#define BML_KEY_INSERT 0xD2u
#define BML_KEY_DELETE 0xD3u
#define BML_KEY_LWIN 0xDBu
#define BML_KEY_RWIN 0xDCu
#define BML_KEY_APPS 0xDDu
#define BML_CURSOR_NORMALSELECT 1u
#define BML_CURSOR_BUSY 2u
#define BML_CURSOR_MOVE 3u
#define BML_CURSOR_LINKSELECT 4u
#define BML_INPUT_KEY_NONE 0u
#define BML_INPUT_KEY_PRESSED 1u
#define BML_INPUT_KEY_RELEASED 2u
#define BML_MOUSE_LEFT 0u
#define BML_MOUSE_RIGHT 1u
#define BML_MOUSE_MIDDLE 2u
#define BML_MOUSE_4 3u
#define BML_INPUT_RAW 1u
#define BML_INPUT_DEVICE_KEYBOARD 0u
#define BML_INPUT_DEVICE_MOUSE 1u
#define BML_INPUT_DEVICE_JOYSTICK 2u
#define BML_INPUT_BLOCK_KEYBOARD 1u
#define BML_INPUT_BLOCK_MOUSE 2u
#define BML_INPUT_BLOCK_JOYSTICK 4u
#define BML_INPUT_BLOCK_ALL 7u
#define BML_INPUTKEYSTATE_1_0_SIZE sizeof(BML_InputKeyState)
#define BML_INPUTKEYEVENT_1_0_SIZE sizeof(BML_InputKeyEvent)
typedef uint64_t BML_InputBlockHandle;

typedef struct BML_InputKeyState {
    uint32_t Key;
    uint32_t Down;
    uint32_t Up;
    uint32_t Toggled;
    uint32_t Pressed;
    uint32_t Released;
    uint32_t DownTimestampMs;
    uint32_t ToggledTimestampMs;
} BML_InputKeyState;

typedef struct BML_InputKeyEvent {
    uint32_t Key;
    uint32_t State;
    uint32_t TimestampMs;
} BML_InputKeyEvent;

typedef struct BML_InputKeyboardState {
    size_t StructSize;
    uint32_t Attached;
    uint32_t RepetitionEnabled;
    const BML_InputKeyState *Keys;
    size_t KeyCount;
    const BML_InputKeyEvent *Events;
    size_t EventCount;
} BML_InputKeyboardState;

typedef struct BML_InputMouseState {
    size_t StructSize;
    uint32_t Attached;
    BML_Vec2 ScreenPosition;
    BML_Vec2 WindowPosition;
    BML_Vec3 RelativePosition;
    BML_Vec2 LastPosition;
    uint32_t DownMask;
    uint32_t ClickedMask;
    uint32_t ToggledMask;
} BML_InputMouseState;

typedef struct BML_InputJoystickState {
    size_t StructSize;
    uint32_t Attached;
    BML_Vec3 Position;
    BML_Vec3 Rotation;
    BML_Vec2 Sliders;
    float PointOfViewAngle;
    uint32_t Buttons;
} BML_InputJoystickState;

typedef struct BML_InputCursorState {
    size_t StructSize;
    uint32_t Visible;
    uint32_t Shape;
} BML_InputCursorState;

typedef struct BML_InputInterface {
    BML_InterfaceHeader Header;
    int(BML_CDECL *ReadKeyboard)(BML_ModContextHandle context, uint32_t flags,
                                 BML_InputKeyboardState *out);
    int(BML_CDECL *ReadMouse)(BML_ModContextHandle context, uint32_t flags,
                              BML_InputMouseState *out);
    int(BML_CDECL *ReadJoystick)(BML_ModContextHandle context, uint32_t index, uint32_t flags,
                                 BML_InputJoystickState *out);
    int(BML_CDECL *GetKeyName)(BML_ModContextHandle context, uint32_t key, char *buffer,
                               size_t capacity, size_t *required);
    int(BML_CDECL *GetKeyFromName)(BML_ModContextHandle context, const char *name,
                                   uint32_t *outKey);
    int(BML_CDECL *EnableKeyboardRepetition)(BML_ModContextHandle context, uint32_t enable);
    int(BML_CDECL *AcquireBlock)(BML_ModContextHandle context, uint32_t deviceMask,
                                 BML_InputBlockHandle *outBlock);
    int(BML_CDECL *ReleaseBlock)(BML_ModContextHandle context, BML_InputBlockHandle block);
    int(BML_CDECL *ReadBlockCount)(BML_ModContextHandle context, uint32_t device,
                                   uint32_t *outCount);
    int(BML_CDECL *Pause)(BML_ModContextHandle context, uint32_t pause);
    int(BML_CDECL *ReadCursor)(BML_ModContextHandle context, BML_InputCursorState *out);
    int(BML_CDECL *ShowCursor)(BML_ModContextHandle context, uint32_t visible);
    int(BML_CDECL *SetSystemCursor)(BML_ModContextHandle context, uint32_t shape);
} BML_InputInterface;

#pragma pack(pop)
BML_END_CDECLS

#endif

#ifndef BML_BUI_H
#define BML_BUI_H
#include "BML/Mod.h"
BML_BEGIN_CDECLS
#pragma pack(push, 8)
#define BML_BUI_INTERFACE_ID "bml.bui"
#define BML_BUI_INTERFACE_MAJOR 1u
#define BML_BUI_INTERFACE_MINOR 0u
#define BML_BUI_WINDOW_NO_TITLE 1u
#define BML_BUI_WINDOW_NO_MOVE 2u
#define BML_BUI_WINDOW_NO_RESIZE 4u
#define BML_BUI_WINDOW_AUTO_RESIZE 8u
#define BML_BUI_WINDOW_NO_BACKGROUND 16u
#define BML_BUI_WINDOW_NO_SCROLLBAR 32u
#define BML_BUI_WINDOW_NO_COLLAPSE 64u
#define BML_BUI_WINDOW_NO_SAVED_SETTINGS 128u
#define BML_BUI_WINDOW_MENU_BOUNDS 256u
#define BML_BUI_BUTTON_PRESS_ON_CLICK 1u
#define BML_BUI_BUTTON_PRESS_ON_RELEASE 2u
#define BML_BUI_BUTTON_REPEAT 4u
#define BML_BUI_INPUT_READ_ONLY 1u
#define BML_BUI_INPUT_PASSWORD 2u
#define BML_BUI_INPUT_ENTER_RETURNS_TRUE 4u
#define BML_BUI_COLOR_NO_ALPHA 1u
#define BML_BUI_MAIN 0u
#define BML_BUI_BACK 1u
#define BML_BUI_OPTION 2u
#define BML_BUI_LEVEL 3u
#define BML_BUI_KEY 4u
#define BML_BUI_SMALL 5u
#define BML_BUI_LEFT 6u
#define BML_BUI_RIGHT 7u
#define BML_BUI_PLUS 8u
#define BML_BUI_MINUS 9u
#define BML_KEY_MOD_SHIFT 1u
#define BML_KEY_MOD_CONTROL 2u
#define BML_KEY_MOD_ALT 4u
#define BML_KEY_MOD_SUPER 8u
#define BML_BUI_INTERFACE_1_0_SIZE (offsetof(BML_BuiInterface, TransitionToScriptAndUnblock) + sizeof(((BML_BuiInterface *)0)->TransitionToScriptAndUnblock))
typedef struct BML_KeyChord {
    uint32_t Key, Modifiers;
} BML_KeyChord;
typedef int (BML_CDECL *BML_BuiDrawCallback)(void *userData);
typedef struct BML_BuiInterface {
    BML_InterfaceHeader Header;
    int (BML_CDECL *ReadMenuStyle)(BML_ModContextHandle, BML_Vec2 *position,
        BML_Vec2 *size, BML_Color *color);
    int (BML_CDECL *ReadButtonStyle)(BML_ModContextHandle, uint32_t kind,
        BML_Vec2 *size, float *indent);
    int (BML_CDECL *BeginWindow)(BML_ModContextHandle, const char *id,
        const char *title, uint32_t *open, uint32_t flags, uint32_t *drawContents);
    int (BML_CDECL *EndWindow)(BML_ModContextHandle);
    int (BML_CDECL *At)(BML_ModContextHandle, BML_Vec2 position, uint32_t pixelCoordinates,
        BML_BuiDrawCallback, void *userData);
    int (BML_CDECL *Button)(BML_ModContextHandle, uint32_t kind, const char *label,
        uint32_t flags, uint32_t *selected, uint32_t *clicked);
    int (BML_CDECL *Navigate)(BML_ModContextHandle, uint32_t kind,
        BML_Vec2 position, uint32_t *clicked);
    int (BML_CDECL *KeyButton)(BML_ModContextHandle, const char *label,
        uint32_t *listening, BML_KeyChord *chord, uint32_t *changed);
    int (BML_CDECL *YesNoButton)(BML_ModContextHandle, const char *label,
        uint32_t *value, uint32_t *changed);
    int (BML_CDECL *RadioButton)(BML_ModContextHandle, const char *label,
        int32_t *current, const char *const *items, size_t count, uint32_t *changed);
    int (BML_CDECL *InputTextButton)(BML_ModContextHandle, const char *label,
        char *buffer, size_t capacity, uint32_t flags, uint32_t *changed);
    int (BML_CDECL *InputFloatButton)(BML_ModContextHandle, const char *label,
        float *value, float step, float fastStep, const char *format,
        uint32_t flags, uint32_t *changed);
    int (BML_CDECL *InputIntButton)(BML_ModContextHandle, const char *label,
        int32_t *value, int32_t step, int32_t fastStep, uint32_t flags, uint32_t *changed);
    int (BML_CDECL *ColorButton)(BML_ModContextHandle, const char *label,
        BML_Color *value, uint32_t flags, uint32_t *changed);
    int (BML_CDECL *WrappedText)(BML_ModContextHandle, const char *text,
        float width, float baseX, float scale);
    int (BML_CDECL *Title)(BML_ModContextHandle, const char *text, float y,
        float scale, uint32_t color);
    int (BML_CDECL *SearchBar)(BML_ModContextHandle, char *buffer, size_t capacity,
        BML_Vec2 position, float width, uint32_t *changed);
    int (BML_CDECL *PlayMenuClickSound)(BML_ModContextHandle);
    int (BML_CDECL *ActivateScript)(BML_ModContextHandle, const char *scriptName);
    int (BML_CDECL *BlockKeyboardInput)(BML_ModContextHandle);
    int (BML_CDECL *UnblockKeyboardAfterRelease)(BML_ModContextHandle);
    int (BML_CDECL *TransitionToScriptAndUnblock)(BML_ModContextHandle, const char *scriptName);
} BML_BuiInterface;

#pragma pack(pop)
BML_END_CDECLS
#endif

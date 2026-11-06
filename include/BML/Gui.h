#ifndef BML_GUI_H
#define BML_GUI_H
#include "BML/Mod.h"
BML_BEGIN_CDECLS
#pragma pack(push, 8)
#define BML_GUI_INTERFACE_ID "bml.gui"
#define BML_GUI_INTERFACE_MAJOR 1u
#define BML_GUI_INTERFACE_MINOR 0u
#define BML_GUI_FONT_ITALIC 1u
#define BML_GUI_FONT_UNDERLINE 2u
#define BML_GUI_PANEL 0u
#define BML_GUI_TEXT 1u
#define BML_GUI_LABEL 2u
#define BML_GUI_BUTTON 3u
#define BML_GUI_INPUT 4u
#define BML_GUI_KEY_INPUT 5u
#define BML_GUI_ELEMENT 6u
#define BML_GUI_BUTTON_NORMAL 0u
#define BML_GUI_BUTTON_BACK 1u
#define BML_GUI_BUTTON_SETTING 2u
#define BML_GUI_BUTTON_LEVEL 3u
#define BML_GUI_BUTTON_KEY 4u
#define BML_GUI_BUTTON_KEYSEL 5u
#define BML_GUI_BUTTON_SMALL 6u
#define BML_GUI_BUTTON_LEFT 7u
#define BML_GUI_BUTTON_RIGHT 8u
#define BML_GUI_BUTTON_PLUS 9u
#define BML_GUI_BUTTON_MINUS 10u
#define BML_GUI_ALIGN_LEFT 1u
#define BML_GUI_ALIGN_CENTER 0u
#define BML_GUI_ALIGN_RIGHT 2u
#define BML_GUI_SPRITE_ALIGN_LEFT 2u
#define BML_GUI_SPRITE_ALIGN_RIGHT 4u
#define BML_GUI_SPRITE_ALIGN_HCENTER 64u
#define BML_GUI_SPRITE_ALIGN_TOP 8u
#define BML_GUI_SPRITE_ALIGN_BOTTOM 16u
#define BML_GUI_SPRITE_ALIGN_VCENTER 32u
#define BML_GUI_ELEMENT_INFO_1_0_SIZE (offsetof(BML_GuiElementInfo, Key) + sizeof(((BML_GuiElementInfo *)0)->Key))
#define BML_GUI_ELEMENT_INFO_INIT { .StructSize = sizeof(BML_GuiElementInfo) }
#define BML_GUI_EVENT_1_0_SIZE (offsetof(BML_GuiEvent, NotifyCallback) + sizeof(((BML_GuiEvent *)0)->NotifyCallback))
#define BML_GUI_EVENT_INIT { .StructSize = sizeof(BML_GuiEvent) }
#define BML_GUI_INTERFACE_1_0_SIZE (offsetof(BML_GuiInterface, ProcessElement) + sizeof(((BML_GuiInterface *)0)->ProcessElement))
#define BML_GUI_TEXT_SCREEN 1u
#define BML_GUI_TEXT_BACKGROUND 2u
#define BML_GUI_TEXT_CLIP 4u
#define BML_GUI_TEXT_RESIZE_VERT 8u
#define BML_GUI_TEXT_RESIZE_HORI 16u
#define BML_GUI_TEXT_WORDWRAP 32u
#define BML_GUI_TEXT_JUSTIFIED 64u
#define BML_GUI_TEXT_COMPILED 128u
#define BML_GUI_TEXT_MULTIPLE 256u
#define BML_GUI_TEXT_SHOWCARET 512u
#define BML_GUI_TEXT_3D 1024u
#define BML_GUI_TEXT_SCREENCLIP 2048u
#define BML_GUI_ALIGN_TOP 4u
#define BML_GUI_ALIGN_TOPLEFT 5u
#define BML_GUI_ALIGN_TOPRIGHT 6u
#define BML_GUI_ALIGN_BOTTOM 8u
#define BML_GUI_ALIGN_BOTTOMLEFT 9u
#define BML_GUI_ALIGN_BOTTOMRIGHT 10u
#define BML_GUI_NOFONT 0u
#define BML_GUI_GAMEFONT_01 1u
#define BML_GUI_GAMEFONT_02 2u
#define BML_GUI_GAMEFONT_03 3u
#define BML_GUI_GAMEFONT_03A 4u
#define BML_GUI_GAMEFONT_04 5u
#define BML_GUI_GAMEFONT_CREDITS_SMALL 6u
#define BML_GUI_GAMEFONT_CREDITS_BIG 7u
typedef struct BML_GuiElementInfo {
    size_t StructSize;
    BML_ObjectRef Object;
    uint32_t Kind, Visible, Active;
    BML_Vec2 Position, Size;
    int32_t ZOrder;
    BML_Color Color;
    uint32_t Alignment, ButtonType, GameFont, TextFlags;
    BML_Vec2 Offset;
    uint32_t Focused, Key;
} BML_GuiElementInfo;
typedef struct BML_GuiEvent {
    size_t StructSize;
    uint32_t Key, Changed, NotifyCallback;
} BML_GuiEvent;
typedef struct BML_GuiInterface {
    BML_InterfaceHeader Header;
    int (BML_CDECL *CreateElement)(BML_ModContextHandle, uint32_t kind,
        const char *name, BML_ObjectRef *outElement);
    int (BML_CDECL *DestroyElement)(BML_ModContextHandle, BML_ObjectRef);
    int (BML_CDECL *ReadElement)(BML_ModContextHandle, BML_ObjectRef, BML_GuiElementInfo *out);
    int (BML_CDECL *SetPosition)(BML_ModContextHandle, BML_ObjectRef, BML_Vec2);
    int (BML_CDECL *SetSize)(BML_ModContextHandle, BML_ObjectRef, BML_Vec2);
    int (BML_CDECL *SetZOrder)(BML_ModContextHandle, BML_ObjectRef, int32_t);
    int (BML_CDECL *SetVisible)(BML_ModContextHandle, BML_ObjectRef, uint32_t);
    int (BML_CDECL *SetActive)(BML_ModContextHandle, BML_ObjectRef, uint32_t);
    int (BML_CDECL *SetColor)(BML_ModContextHandle, BML_ObjectRef, BML_Color);
    int (BML_CDECL *SetText)(BML_ModContextHandle, BML_ObjectRef, const char *);
    int (BML_CDECL *GetText)(BML_ModContextHandle, BML_ObjectRef,
        char *buffer, size_t capacity, size_t *required);
    int (BML_CDECL *SetFont)(BML_ModContextHandle, BML_ObjectRef, const char *face,
        int32_t size, int32_t weight, uint32_t flags);
    int (BML_CDECL *SetAlignment)(BML_ModContextHandle, BML_ObjectRef, uint32_t);
    int (BML_CDECL *SetButtonType)(BML_ModContextHandle, BML_ObjectRef, uint32_t);
    int (BML_CDECL *SetHovered)(BML_ModContextHandle, BML_ObjectRef, uint32_t);
    int (BML_CDECL *SetGameFont)(BML_ModContextHandle, BML_ObjectRef, uint32_t);
    int (BML_CDECL *SetTextFlags)(BML_ModContextHandle, BML_ObjectRef, uint32_t);
    int (BML_CDECL *SetOffset)(BML_ModContextHandle, BML_ObjectRef, BML_Vec2);
    int (BML_CDECL *SetFocused)(BML_ModContextHandle, BML_ObjectRef, uint32_t);
    int (BML_CDECL *SetKey)(BML_ModContextHandle, BML_ObjectRef, uint32_t);
    int (BML_CDECL *OnCharTyped)(BML_ModContextHandle, BML_ObjectRef,
        uint32_t key, BML_GuiEvent *out);
    int (BML_CDECL *ProcessElement)(BML_ModContextHandle, BML_ObjectRef);
} BML_GuiInterface;

#pragma pack(pop)
BML_END_CDECLS
#endif

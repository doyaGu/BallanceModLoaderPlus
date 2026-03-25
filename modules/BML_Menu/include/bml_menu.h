#ifndef BML_MENU_H
#define BML_MENU_H

#include "imgui.h"

#include "CKContext.h"

#include "bml_interface.h"

#define BML_MENU_INTERFACE_ID "bml.menu.runtime"

namespace Menu {
    enum ButtonType : int;
}

struct BML_MenuResourceApi {
    size_t struct_size;

    bool (*InitTextures)(CKContext *context);
    bool (*InitMaterials)(CKContext *context);
    bool (*InitSounds)(CKContext *context);

    CKTexture *(*LoadTexture)(CKContext *context, const char *id, const char *filename, int slot);
    void (*PlayMenuClickSound)();
};

struct BML_MenuLayoutApi {
    size_t struct_size;

    ImVec2 (*GetMenuPos)();
    ImVec2 (*GetMenuSize)();
    ImVec4 (*GetMenuColor)();

    ImVec2 (*GetButtonSize)(Menu::ButtonType type);
    float (*GetButtonIndent)(Menu::ButtonType type);
    ImVec2 (*GetButtonSizeInCoord)(Menu::ButtonType type);
    float (*GetButtonIndentInCoord)(Menu::ButtonType type);
};

struct BML_MenuInputApi {
    size_t struct_size;

    ImGuiKey (*CKKeyToImGuiKey)(CKKEYBOARD key);
    CKKEYBOARD (*ImGuiKeyToCKKey)(ImGuiKey key);
    bool (*KeyChordToString)(ImGuiKeyChord keyChord, char *buf, size_t size);
    bool (*SetKeyChordFromIO)(ImGuiKeyChord *keyChord);
};

struct BML_MenuDrawApi {
    size_t struct_size;

    void (*AddButtonImage)(
        ImDrawList *drawList,
        const ImVec2 &pos,
        Menu::ButtonType type,
        int state,
        const char *text,
        const ImVec2 *textAlign);
};

struct BML_MenuWidgetApi {
    size_t struct_size;

    bool (*MainButton)(const char *label, ImGuiButtonFlags flags);
    bool (*OkButton)(const char *label, ImGuiButtonFlags flags);
    bool (*BackButton)(const char *label, ImGuiButtonFlags flags);
    bool (*OptionButton)(const char *label, ImGuiButtonFlags flags);
    bool (*LevelButton)(const char *label, bool *v, ImGuiButtonFlags flags);
    bool (*SmallButton)(const char *label, bool *v, ImGuiButtonFlags flags);
    bool (*LeftButton)(const char *label, ImGuiButtonFlags flags);
    bool (*RightButton)(const char *label, ImGuiButtonFlags flags);
    bool (*PlusButton)(const char *label, ImGuiButtonFlags flags);
    bool (*MinusButton)(const char *label, ImGuiButtonFlags flags);

    bool (*KeyButton)(const char *label, bool *toggled, ImGuiKeyChord *keyChord);
    bool (*YesNoButton)(const char *label, bool *v);
    bool (*RadioButton)(
        const char *label,
        int *currentItem,
        const char *const items[],
        int itemsCount);

    bool (*InputTextButton)(
        const char *label,
        char *buf,
        size_t bufSize,
        ImGuiInputTextFlags flags,
        ImGuiInputTextCallback callback,
        void *userData);
    bool (*InputFloatButton)(
        const char *label,
        float *v,
        float step,
        float stepFast,
        const char *format,
        ImGuiInputTextFlags flags);
    bool (*InputIntButton)(
        const char *label,
        int *v,
        int step,
        int stepFast,
        ImGuiInputTextFlags flags);

    bool (*SearchBar)(
        char *buffer,
        size_t bufferSize,
        float x,
        float y,
        float width);
};

struct BML_MenuTextApi {
    size_t struct_size;

    void (*WrappedText)(
        const char *text,
        float width,
        float baseX,
        float scale);
    void (*Title)(const char *text, float y, float scale, ImU32 color);
};

struct BML_MenuNavApi {
    size_t struct_size;

    bool (*NavLeft)(float x, float y);
    bool (*NavRight)(float x, float y);
    bool (*NavBack)(float x, float y);
};

struct BML_MenuApi {
    BML_InterfaceHeader header;

    const BML_MenuResourceApi *resources;
    const BML_MenuLayoutApi *layout;
    const BML_MenuInputApi *input;
    const BML_MenuDrawApi *draw;
    const BML_MenuWidgetApi *widgets;
    const BML_MenuTextApi *text;
    const BML_MenuNavApi *nav;
};

#endif /* BML_MENU_H */

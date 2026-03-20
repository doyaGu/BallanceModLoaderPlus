#ifndef BML_MENU_H
#define BML_MENU_H

#include "imgui.h"

#include "CKContext.h"

#include "bml_imgui.h"
#include "bml_interface.h"

#define BML_MENU_INTERFACE_ID "bml.menu.runtime"

namespace Menu {
    enum ButtonType : int;
}

struct BML_MenuApi {
    BML_InterfaceHeader header;

    bool (*InitTextures)(CKContext *context);
    bool (*InitMaterials)(CKContext *context);
    bool (*InitSounds)(CKContext *context);

    CKTexture *(*LoadTexture)(CKContext *context, const char *id, const char *filename, int slot);

    ImVec2 (*GetMenuPos)(const BML_ImGuiApi *imguiApi);
    ImVec2 (*GetMenuSize)(const BML_ImGuiApi *imguiApi);
    ImVec4 (*GetMenuColor)();

    ImVec2 (*GetButtonSize)(Menu::ButtonType type, const BML_ImGuiApi *imguiApi);
    float (*GetButtonIndent)(Menu::ButtonType type, const BML_ImGuiApi *imguiApi);
    ImVec2 (*GetButtonSizeInCoord)(Menu::ButtonType type);
    float (*GetButtonIndentInCoord)(Menu::ButtonType type);

    ImGuiKey (*CKKeyToImGuiKey)(CKKEYBOARD key);
    CKKEYBOARD (*ImGuiKeyToCKKey)(ImGuiKey key);
    bool (*KeyChordToString)(ImGuiKeyChord keyChord, char *buf, size_t size, const BML_ImGuiApi *imguiApi);
    bool (*SetKeyChordFromIO)(ImGuiKeyChord *keyChord, const BML_ImGuiApi *imguiApi);

    void (*PlayMenuClickSound)();

    void (*AddButtonImageState)(
        ImDrawList *drawList,
        const ImVec2 &pos,
        Menu::ButtonType type,
        int state,
        const BML_ImGuiApi *imguiApi);
    void (*AddButtonImageSelected)(
        ImDrawList *drawList,
        const ImVec2 &pos,
        Menu::ButtonType type,
        bool selected,
        const BML_ImGuiApi *imguiApi);
    void (*AddButtonImageStateText)(
        ImDrawList *drawList,
        const ImVec2 &pos,
        Menu::ButtonType type,
        int state,
        const char *text,
        const BML_ImGuiApi *imguiApi);
    void (*AddButtonImageSelectedText)(
        ImDrawList *drawList,
        const ImVec2 &pos,
        Menu::ButtonType type,
        bool selected,
        const char *text,
        const BML_ImGuiApi *imguiApi);
    void (*AddButtonImageStateTextAlign)(
        ImDrawList *drawList,
        const ImVec2 &pos,
        Menu::ButtonType type,
        int state,
        const char *text,
        const ImVec2 &textAlign,
        const BML_ImGuiApi *imguiApi);
    void (*AddButtonImageSelectedTextAlign)(
        ImDrawList *drawList,
        const ImVec2 &pos,
        Menu::ButtonType type,
        bool selected,
        const char *text,
        const ImVec2 &textAlign,
        const BML_ImGuiApi *imguiApi);

    bool (*MainButton)(const char *label, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi);
    bool (*OkButton)(const char *label, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi);
    bool (*BackButton)(const char *label, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi);
    bool (*OptionButton)(const char *label, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi);
    bool (*LevelButton)(const char *label, bool *v, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi);
    bool (*SmallButton)(const char *label, bool *v, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi);
    bool (*LeftButton)(const char *label, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi);
    bool (*RightButton)(const char *label, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi);
    bool (*PlusButton)(const char *label, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi);
    bool (*MinusButton)(const char *label, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi);

    bool (*KeyButton)(const char *label, bool *toggled, ImGuiKeyChord *keyChord, const BML_ImGuiApi *imguiApi);
    bool (*YesNoButton)(const char *label, bool *v, const BML_ImGuiApi *imguiApi);
    bool (*RadioButton)(
        const char *label,
        int *currentItem,
        const char *const items[],
        int itemsCount,
        const BML_ImGuiApi *imguiApi);

    bool (*InputTextButton)(
        const char *label,
        char *buf,
        size_t bufSize,
        ImGuiInputTextFlags flags,
        ImGuiInputTextCallback callback,
        void *userData,
        const BML_ImGuiApi *imguiApi);
    bool (*InputFloatButton)(
        const char *label,
        float *v,
        float step,
        float stepFast,
        const char *format,
        ImGuiInputTextFlags flags,
        const BML_ImGuiApi *imguiApi);
    bool (*InputIntButton)(
        const char *label,
        int *v,
        int step,
        int stepFast,
        ImGuiInputTextFlags flags,
        const BML_ImGuiApi *imguiApi);

    void (*WrappedText)(
        const char *text,
        float width,
        float baseX,
        float scale,
        const BML_ImGuiApi *imguiApi);

    bool (*NavLeft)(float x, float y, const BML_ImGuiApi *imguiApi);
    bool (*NavRight)(float x, float y, const BML_ImGuiApi *imguiApi);
    bool (*NavBack)(float x, float y, const BML_ImGuiApi *imguiApi);

    void (*Title)(const char *text, float y, float scale, ImU32 color, const BML_ImGuiApi *imguiApi);
    bool (*SearchBar)(
        char *buffer,
        size_t bufferSize,
        float x,
        float y,
        float width,
        const BML_ImGuiApi *imguiApi);
};

#endif /* BML_MENU_H */


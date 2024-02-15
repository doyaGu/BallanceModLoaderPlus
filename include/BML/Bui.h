#ifndef BML_BUI_H
#define BML_BUI_H

#include "imgui.h"

#include "BML/Export.h"
#include "CKContext.h"
#include "CKRenderContext.h"

namespace Bui {
    enum ButtonType {
        BUTTON_MAIN,
        BUTTON_BACK,
        BUTTON_OPTION,
        BUTTON_LEVEL,
        BUTTON_KEY,
        BUTTON_SMALL,
        BUTTON_LEFT,
        BUTTON_RIGHT,
        BUTTON_PLUS,
        BUTTON_MINUS,

        BUTTON_COUNT
    };

    inline ImVec2 CoordToScreenPos(const ImVec2 &coord) {
        const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
        return {vpSize.x * coord.x, vpSize.y * coord.y};
    }

    BML_EXPORT bool InitTextures(CKContext *context);
    BML_EXPORT bool InitMaterials(CKContext *context);
    BML_EXPORT bool InitSounds(CKContext *context);

    BML_EXPORT ImGuiContext *GetImGuiContext();

    BML_EXPORT CKTexture *LoadTexture(CKContext *context, const char *id, const char *filename, int slot = 0);

    BML_EXPORT ImVec2 GetMenuPos();
    BML_EXPORT ImVec2 GetMenuSize();
    BML_EXPORT ImVec4 GetMenuColor();

    BML_EXPORT ImVec2 GetButtonSize(ButtonType type);
    BML_EXPORT float GetButtonIndent(ButtonType type);

    BML_EXPORT ImGuiKey CKKeyToImGuiKey(CKKEYBOARD key);
    BML_EXPORT CKKEYBOARD ImGuiKeyToCKKey(ImGuiKey key);
    BML_EXPORT bool KeyChordToString(ImGuiKeyChord key_chord, char *buf, size_t size);

    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state);
    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected);

    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state, const char *text);
    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected, const char *text);

    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, int state, const char *text, const ImVec2 &text_align);
    BML_EXPORT void AddButtonImage(ImDrawList *drawList, const ImVec2 &pos, ButtonType type, bool selected, const char *text, const ImVec2 &text_align);

    BML_EXPORT bool MainButton(const char *label, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool OkButton(const char *label, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool BackButton(const char *label, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool OptionButton(const char *label, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool LevelButton(const char *label, bool *v = nullptr, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool SmallButton(const char *label, bool *v = nullptr, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool LeftButton(const char *label, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool RightButton(const char *label, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool PlusButton(const char *label, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool MinusButton(const char *label, ImGuiButtonFlags flags = 0);

    BML_EXPORT bool YesNoButton(const char *label, bool *v);
    BML_EXPORT bool RadioButton(const char *label, int *current_item, const char *const items[], int items_count);
    BML_EXPORT bool KeyButton(const char *label, bool *toggled, ImGuiKeyChord *key_chord);

    BML_EXPORT bool InputTextButton(const char *label, char *buf, size_t buf_size, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void *user_data = nullptr);
    BML_EXPORT bool InputFloatButton(const char *label, float *v, float step = 0.0f, float step_fast = 0.0f, const char *format = "%.3f", ImGuiInputTextFlags flags = 0);
    BML_EXPORT bool InputIntButton(const char *label, int* v, int step = 1, int step_fast = 100, ImGuiInputTextFlags flags = 0);
}

#endif // BML_BUI_H

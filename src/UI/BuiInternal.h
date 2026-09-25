#ifndef BML_BUI_INTERNAL_H
#define BML_BUI_INTERNAL_H

#include <string>
#include <vector>

#include "BML/Bui.h"

namespace Bui {
    struct ColorPickerArea {
        ImVec2 minimum{};
        ImVec2 maximum{};
    };

    void BlockKeyboardInput(const void *owner);
    bool RenderMenuAfterInputHandoff(Menu &menu, const void *owner);
    void UnblockKeyboardAfterRelease(const void *owner);
    void TransitionToScriptAndUnblock(const char *scriptName, const void *owner);

    bool InputTextButton(const char *label, std::string *value,
                         ImGuiInputTextFlags flags = 0,
                         ImGuiInputTextCallback callback = nullptr,
                         void *userData = nullptr);
    bool ColorStringButton(const char *label, std::string *value);
    bool ColorStringButton(const char *label, std::string *value,
                           const ColorPickerArea &pickerArea);
    bool RadioButton(const char *label, int *currentItem,
                     const std::vector<std::string> &items,
                     const char *emptyItemLabel = nullptr);
}

#endif // BML_BUI_INTERNAL_H

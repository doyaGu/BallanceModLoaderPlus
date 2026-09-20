#ifndef BML_BUI_INTERNAL_H
#define BML_BUI_INTERNAL_H

#include <string>
#include <vector>

#include "BML/Bui.h"

namespace Bui {
    void BlockKeyboardInput(const void *owner);
    void UnblockKeyboardAfterRelease(const void *owner);
    void TransitionToScriptAndUnblock(const char *scriptName, const void *owner);

    bool InputTextButton(const char *label, std::string *value,
                         ImGuiInputTextFlags flags = 0,
                         ImGuiInputTextCallback callback = nullptr,
                         void *userData = nullptr);
    bool ColorStringButton(const char *label, std::string *value);
    bool RadioButton(const char *label, int *currentItem,
                     const std::vector<std::string> &items,
                     const char *emptyItemLabel = nullptr);
}

#endif // BML_BUI_INTERNAL_H

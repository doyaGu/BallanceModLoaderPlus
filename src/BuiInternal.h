#ifndef BML_BUI_INTERNAL_H
#define BML_BUI_INTERNAL_H

#include <string>

#include "BML/Bui.h"

namespace Bui {
    bool InputTextButton(const char *label, std::string *value,
                         ImGuiInputTextFlags flags = 0,
                         ImGuiInputTextCallback callback = nullptr,
                         void *userData = nullptr);
}

#endif // BML_BUI_INTERNAL_H

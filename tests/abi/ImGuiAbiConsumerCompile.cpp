#include "BML/Bui/ImGui.hpp"

static_assert(sizeof(ImWchar) == sizeof(ImWchar32),
              "BML consumers must use the loader's 32-bit ImWchar ABI");

#ifndef IMGUI_USE_BGRA_PACKED_COLOR
#error "BML consumers must use the loader's packed-color ABI"
#endif

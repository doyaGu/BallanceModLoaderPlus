#ifndef BML_BALLANCE_PALETTE_H
#define BML_BALLANCE_PALETTE_H

#include "BML/Bui.h"

namespace BallancePalette {
    inline ImVec4 PanelBackground() {
        return Bui::GetMenuColor();
    }

    inline ImVec4 Selection() {
        return {224.0f / 255.0f, 169.0f / 255.0f, 113.0f / 255.0f, 195.0f / 255.0f};
    }

    inline ImVec4 Hover() {
        return {235.0f / 255.0f, 190.0f / 255.0f, 122.0f / 255.0f, 210.0f / 255.0f};
    }

    inline ImVec4 Active() {
        return {190.0f / 255.0f, 128.0f / 255.0f, 52.0f / 255.0f, 225.0f / 255.0f};
    }
}

#endif // BML_BALLANCE_PALETTE_H

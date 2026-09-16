#ifndef BML_INPUT_SURFACE_STYLE_H
#define BML_INPUT_SURFACE_STYLE_H

#include "UI/BallancePalette.h"

namespace InputSurfaceStyle {
    constexpr float ScreenMargin = 8.0f;
    constexpr float AnchorGap = 3.0f;
    constexpr float PaddingX = 8.0f;
    constexpr float PaddingY = 5.0f;
    constexpr float ItemGap = 6.0f;
    constexpr float ChipPaddingX = 6.0f;
    constexpr float Rounding = 0.0f;
    constexpr float BorderSize = 0.0f;

    constexpr float TransientHeight(float textLineHeight) {
        return textLineHeight + PaddingY * 2.0f;
    }

    inline ImVec4 PanelBackground() {
        return BallancePalette::PanelBackground();
    }

    inline ImVec4 SelectionColor() {
        return BallancePalette::Selection();
    }

    inline ImVec4 HoverColor() {
        return BallancePalette::Hover();
    }

    inline ImVec4 ActiveColor() {
        return BallancePalette::Active();
    }

    inline void DrawPanel(ImDrawList *drawList, const ImVec2 &minimum, const ImVec2 &maximum,
                          const ImVec4 &background, float rounding = Rounding) {
        if (!drawList)
            return;
        drawList->AddRectFilled(minimum, maximum, ImGui::GetColorU32(background), rounding);
        if (BorderSize > 0.0f) {
            drawList->AddRect(minimum, maximum, ImGui::GetColorU32(ImGuiCol_Separator), rounding, 0, BorderSize);
        }
    }
}

#endif // BML_INPUT_SURFACE_STYLE_H

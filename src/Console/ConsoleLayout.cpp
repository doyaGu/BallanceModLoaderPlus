#include "Console/ConsoleLayout.h"

#include <algorithm>

namespace ConsoleLayout {
    namespace {
        constexpr float MarginRatio = 0.02f;
        constexpr float MinimumMargin = 8.0f;
        constexpr float MaximumHorizontalMargin = 32.0f;
        constexpr float MaximumBottomMargin = 24.0f;
        constexpr float RowGapRatio = 0.15f;
        constexpr float MinimumRowGap = 3.0f;
        constexpr float MaximumRowGap = 6.0f;
    }

    Stack Calculate(Viewport viewport, float commandHeight, float transientHeight) noexcept {
        viewport.width = std::max(0.0f, viewport.width);
        viewport.height = std::max(0.0f, viewport.height);
        commandHeight = std::clamp(commandHeight, 0.0f, viewport.height);
        transientHeight = std::clamp(transientHeight, 0.0f, viewport.height);

        const float horizontalMargin = std::min(
            viewport.width * 0.5f,
            std::clamp(viewport.width * MarginRatio, MinimumMargin, MaximumHorizontalMargin));
        const float bottomMargin = std::min(
            viewport.height,
            std::clamp(viewport.height * MarginRatio, MinimumMargin, MaximumBottomMargin));
        const float gap = std::min(
            std::max(0.0f, viewport.height - bottomMargin),
            std::clamp(commandHeight * RowGapRatio, MinimumRowGap, MaximumRowGap));
        const float bottom = viewport.y + viewport.height - bottomMargin;
        const float transientTop = std::max(viewport.y, bottom - transientHeight);
        const float commandTop = std::max(viewport.y, transientTop - gap - commandHeight);

        Stack stack;
        stack.commandBar = {
            viewport.x + horizontalMargin,
            commandTop,
            std::max(0.0f, viewport.width - horizontalMargin * 2.0f),
            std::max(0.0f, std::min(commandHeight, transientTop - gap - commandTop)),
        };
        stack.transientSurface = {
            stack.commandBar.x,
            transientTop,
            stack.commandBar.width,
            std::max(0.0f, bottom - transientTop),
        };
        stack.messageBottom = std::max(viewport.y, commandTop - gap);
        return stack;
    }
}

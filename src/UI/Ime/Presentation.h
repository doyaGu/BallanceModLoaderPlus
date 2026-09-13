#ifndef BML_UI_IME_PRESENTATION_H
#define BML_UI_IME_PRESENTATION_H

#include "imgui.h"

namespace Overlay::Ime::Presentation {
    // A text host can reserve an exact rail placement for the current frame.
    // Without one, presentation follows ImGuiPlatformImeData's active caret.
    void ReservePlacement(const ImVec2 &position, float width);
    void Draw(const ImGuiPlatformImeData &imeData);

    // Active includes empty preedit frames between start and end messages.
    bool IsActive();
}

#endif // BML_UI_IME_PRESENTATION_H

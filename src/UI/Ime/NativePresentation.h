#ifndef BML_UI_IME_NATIVE_PRESENTATION_H
#define BML_UI_IME_NATIVE_PRESENTATION_H

#include <cstdint>

namespace Overlay::Ime::NativePresentation {
    struct MessageDisposition {
        bool suppress = false;
        bool replaceLParam = false;
        std::intptr_t lParam = 0;
    };

    MessageDisposition Decide(std::uint32_t message, std::uintptr_t wParam,
                              std::intptr_t lParam, bool ownsPresentation) noexcept;
}

#endif // BML_UI_IME_NATIVE_PRESENTATION_H

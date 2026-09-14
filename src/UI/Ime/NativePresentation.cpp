#include "UI/Ime/NativePresentation.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <imm.h>

namespace Overlay::Ime::NativePresentation {
    namespace {
        bool ShouldSuppressNativeNotification(std::uintptr_t notification) noexcept {
            return notification == IMN_OPENCANDIDATE ||
                   notification == IMN_CHANGECANDIDATE ||
                   notification == IMN_CLOSECANDIDATE ||
                   notification == IMN_SETCANDIDATEPOS;
        }
    }

    MessageDisposition Decide(std::uint32_t message, std::uintptr_t wParam,
                              std::intptr_t lParam, bool ownsPresentation) noexcept {
        MessageDisposition disposition;
        if (!ownsPresentation)
            return disposition;

        if (message == WM_IME_SETCONTEXT && wParam != 0) {
            disposition.replaceLParam = true;
            disposition.lParam = lParam & ~static_cast<std::intptr_t>(ISC_SHOWUIALL);
            return disposition;
        }

        switch (message) {
        case WM_IME_STARTCOMPOSITION:
        case WM_IME_ENDCOMPOSITION:
            disposition.suppress = true;
            break;
        case WM_IME_COMPOSITION:
            disposition.suppress = (lParam & GCS_RESULTSTR) == 0;
            break;
        case WM_IME_NOTIFY:
            disposition.suppress = ShouldSuppressNativeNotification(wParam);
            break;
        default:
            break;
        }
        return disposition;
    }
}

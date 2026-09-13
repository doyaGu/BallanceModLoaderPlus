#ifndef BML_UI_IME_RUNTIME_H
#define BML_UI_IME_RUNTIME_H

#include "UI/Ime/NativePresentation.h"
#include "UI/Ime/State.h"

#include <cstdint>

namespace Overlay::Ime::Runtime {
    // Overlay Platform Input owns these calls and invokes them only on the
    // Player UI thread which owns the root-window tree.
    void Attach(void *rootWindow);
    void Detach();
    NativePresentation::MessageDisposition HandleNativeMessage(void *messageWindow, std::uint32_t message,
                                                               std::uintptr_t wParam, std::intptr_t lParam);
    bool WantsCandidateNavigation();

    struct PresentationFrame {
        Snapshot snapshot;
        std::uint64_t revision = ~std::uint64_t{0};
        std::uint64_t candidateRevision = ~std::uint64_t{0};
    };

    // Applies the current ImGui text-input visibility, clears stale state on
    // deactivation, and copies a new immutable frame only when IMM/TSF state
    // changed. Returns whether that frame contains anything to present.
    bool PreparePresentationFrame(bool visible, PresentationFrame &frame);

    bool IsPresentationActive();
    bool MoveCandidate(CandidateDirection direction);
}

#endif // BML_UI_IME_RUNTIME_H

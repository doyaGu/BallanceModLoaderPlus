#ifndef BML_UI_IME_TSF_H
#define BML_UI_IME_TSF_H

#include <cstdint>

namespace Overlay::Ime {
    struct Snapshot;
    enum class CandidateDirection;
}

namespace Overlay::Ime::Tsf {
    // TSF is the authoritative candidate UI/control path for modern TIPs.
    // IMM remains responsible for composition mirroring and legacy fallback.
    // Attachment is best-effort; unavailable TSF support leaves IMM active.
    void Attach();
    void Detach();
    // Only the focused ImGui backend window owns in-game candidate
    // presentation. Other controls on the Player UI thread keep native TSF UI.
    void SetPresentationOwned(bool owned) noexcept;
    void ClearCandidates();
    std::uint64_t Revision() noexcept;
    bool HasCandidates() noexcept;
    std::uint64_t ApplyCandidates(Snapshot &snapshot);
    bool MoveSelection(CandidateDirection direction);
}

#endif // BML_UI_IME_TSF_H

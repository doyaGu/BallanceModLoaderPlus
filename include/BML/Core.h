#ifndef BML_CORE_H
#define BML_CORE_H

#include "BML/UI.h"

/* Legacy convenience spelling for BML's own UI API.  The implementation
 * is deliberately shallow and header-only; it no longer binds dotted raw
 * exports or exposes a C++ DLL ABI. */
namespace BML::Core {
namespace UI {

inline int SendMessage(const char *message) { return BML::UI::AddMessage(message ? message : ""); }
inline int ClearMessages() { return BML::UI::ClearMessages(); }

} // namespace UI

namespace Menu {

inline int OpenModsMenu() { return BML::UI::OpenModsMenu(); }
inline int CloseModsMenu() { return BML::UI::CloseModsMenu(); }
inline int OpenMapMenu() { return BML::UI::OpenMapMenu(); }
inline int CloseMapMenu() { return BML::UI::CloseMapMenu(); }

} // namespace Menu

namespace HUD {

inline int GetMode(int fallback = 0) {
    BML::UI::HUDState state{};
    return BML::UI::ReadHUDState(state) == BML_OK ? state.Mode : fallback;
}

inline int SetMode(int mode) { return BML::UI::SetHUDMode(mode); }
inline int ShowTitle(bool show) { return BML::UI::ShowTitle(show); }
inline int ShowFPS(bool show) { return BML::UI::ShowFPS(show); }
inline int ShowSRTimer(bool show) { return BML::UI::ShowSRTimer(show); }
inline int StartSRTimer() { return BML::UI::StartSRTimer(); }
inline int PauseSRTimer() { return BML::UI::PauseSRTimer(); }
inline int ResetSRTimer() { return BML::UI::ResetSRTimer(); }

inline float GetSRTime(float fallback = 0.0f) {
    BML::UI::HUDState state{};
    return BML::UI::ReadHUDState(state) == BML_OK ? state.SRTime : fallback;
}

} // namespace HUD
} // namespace BML::Core

#endif // BML_CORE_H

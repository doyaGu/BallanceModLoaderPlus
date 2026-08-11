// Drives the loader's own UI over IMC: the ingame message list, the mods and map
// menus, and the HUD. This is a thin inline wrapper around the generated bml.ui
// client, so including it costs nothing at link time.
//
// Status handling, first-call client opening, and the thread rules are the same
// as in Runtime.h. The routes here run on the game thread, so a call from the
// main thread executes inline and has taken effect by the time it returns, while
// a call from any other thread is queued and served during the loader's next
// frame, blocking the caller until then or until the 5000 ms default timeout.
//
// None of this is ImGui. Unlike the Bui controls, which only work inside the
// ImGui frame that the loader keeps open across IMod::OnProcess, these functions
// hand work to loader-owned UI and can be called from any callback.
#ifndef BML_UI_H
#define BML_UI_H

#include "BML/Generated/bml_ui_imc.hpp"

#include <string>

namespace BML::UI {

// Mode is a bitmask of the HUDElement values below.
struct HUDState {
    int Mode = 0;
};

// Bits accepted by SetHUDMode and reported by ReadHUDState. These match the
// HUD_TITLE, HUD_FPS, and HUD_SR values the script API exposes.
enum HUDElement {
    HUD_TITLE = 1,
    HUD_FPS = 2,
    HUD_SR = 4,
};

namespace Detail {

namespace Api = Imc::Generated::Bml::Ui;

inline Imc::LazyClient<Api::Client> &ClientState() {
    static Imc::LazyClient<Api::Client> state;
    return state;
}

inline Api::Client &Client() { return ClientState().Get(); }

[[nodiscard]] inline int RequireApi() { return ClientState().EnsureOpen(); }

// Shared bodies for the argument-free and single-bool routes. The 5000u is the
// call timeout in milliseconds, matching the generated client default.
[[nodiscard]] inline int EmptyCommand(int (Api::Client::*command)(std::uint32_t)) {
    int status = RequireApi();
    return status == BML_OK ? (Client().*command)(5000u) : status;
}

[[nodiscard]] inline int VisibleCommand(int (Api::Client::*command)(const Api::VisibleInputValue &, std::uint32_t),
                                        bool visible) {
    Api::VisibleInputValue input{};
    input.Visible = visible;
    int status = RequireApi();
    return status == BML_OK ? (Client().*command)(input, 5000u) : status;
}

} // namespace Detail

// Opens the client if it is not open yet. The functions below already do this,
// so call it directly only to probe whether the routes exist.
[[nodiscard]] inline int RequireApi() { return Detail::RequireApi(); }

[[nodiscard]] inline int ReadHUDState(HUDState &out) {
    Detail::Api::HudStateValue wire{};
    int status = RequireApi();
    if (status == BML_OK)
        status = Detail::Client().CallState(wire);
    if (status == BML_OK)
        out.Mode = wire.Mode;
    return status;
}

// Appends one line to the loader's ingame message list, the same list
// IBML::SendIngameMessage writes to. Older lines scroll off on their own.
[[nodiscard]] inline int AddMessage(const std::string &message) {
    Detail::Api::MessageInputValue input{};
    input.Message = message;
    int status = RequireApi();
    return status == BML_OK ? Detail::Client().CallMessageAdd(input) : status;
}

[[nodiscard]] inline int ClearMessages() { return Detail::EmptyCommand(&Detail::Api::Client::CallMessageClear); }

[[nodiscard]] inline int OpenModsMenu() { return Detail::EmptyCommand(&Detail::Api::Client::CallModsMenuOpen); }
[[nodiscard]] inline int CloseModsMenu() { return Detail::EmptyCommand(&Detail::Api::Client::CallModsMenuClose); }
[[nodiscard]] inline int OpenMapMenu() { return Detail::EmptyCommand(&Detail::Api::Client::CallMapMenuOpen); }
[[nodiscard]] inline int CloseMapMenu() { return Detail::EmptyCommand(&Detail::Api::Client::CallMapMenuClose); }

// Replaces the whole HUD bitmask, so read it first and mask if you only mean to
// change one element. This writes the loader's own ShowTitle, ShowFPS, and ShowSR
// configuration entries, so the change persists across restarts.
[[nodiscard]] inline int SetHUDMode(int mode) {
    Detail::Api::HudModeInputValue input{};
    input.Mode = mode;
    int status = RequireApi();
    return status == BML_OK ? Detail::Client().CallHudSet(input) : status;
}

// These two only toggle the visibility of the HUD element and do not write the
// configuration, so they are not the same as setting one bit through SetHUDMode.
// ReadHUDState keeps reporting the configured bit, and the loader restores that
// configured value whenever it rebuilds the HUD, which includes every level load.
// Use them for a temporary hide and SetHUDMode for a lasting change.
[[nodiscard]] inline int ShowTitle(bool visible) {
    return Detail::VisibleCommand(&Detail::Api::Client::CallHudTitleShow, visible);
}

[[nodiscard]] inline int ShowFPS(bool visible) {
    return Detail::VisibleCommand(&Detail::Api::Client::CallHudFpsShow, visible);
}

} // namespace BML::UI

#endif // BML_UI_H

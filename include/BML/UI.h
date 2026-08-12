// Drives the loader's own UI: the ingame message list, the mods and map menus,
// and the HUD. The interface struct below is what the loader hands out and what a
// Mod written in C uses directly; the inline C++ namespace under it is the same
// thing with the lookup and the checks folded in, so including this header still
// costs nothing at link time.
//
// Interface.h explains the header, the version rules, and BML_IFACE_HAS. Every
// function here runs on the calling thread and touches UI state the loader draws
// from the main thread, so every one of them, the read included, answers
// BML_ERROR_WRONG_THREAD when called from any other thread. Nothing is queued for
// the next frame: hand the work to the main thread yourself, from IMod::OnProcess
// or another loader callback.
//
// Each function answers BML_ERROR_INVALID_PARAMETER for a null argument and
// BML_ERROR_FAIL before the loader has loaded its mods. A read fills its out
// parameter only when it answers BML_OK.
//
// None of this is ImGui. Unlike the Bui controls, which only work inside the
// ImGui frame that the loader keeps open across IMod::OnProcess, these functions
// hand work to loader-owned UI and can be called from any main-thread callback.
#ifndef BML_UI_H
#define BML_UI_H

#include "BML/Interface.h"

BML_BEGIN_CDECLS

#define BML_UI_INTERFACE_ID "bml.ui"
#define BML_UI_INTERFACE_MAJOR 1
#define BML_UI_INTERFACE_MINOR 0

// Bits accepted by SetHUDMode and reported by ReadHUDState. These match the
// HUD_TITLE, HUD_FPS, and HUD_SR values the script API exposes.
typedef enum BML_UIHUDElement {
    BML_UI_HUD_TITLE = 1,
    BML_UI_HUD_FPS = 2,
    BML_UI_HUD_SR = 4,
    _BML_UI_HUD_ELEMENT_FORCE_32BIT = 0x7fffffff
} BML_UIHUDElement;

// Mode is a bitmask of the BML_UIHUDElement values above. This is a C struct and
// has no default member initializers: zero it with BML_UIHUDState state = {0} in
// C, or with HUDState state{} in C++.
typedef struct BML_UIHUDState {
    int Mode;
} BML_UIHUDState;

typedef struct BML_UIInterface {
    BML_InterfaceHeader Header;

    int (*ReadHUDState)(BML_UIHUDState *out);

    // Appends one line to the loader's ingame message list, the same list
    // IBML::SendIngameMessage writes to. Older lines scroll off on their own.
    int (*AddMessage)(const char *message);
    int (*ClearMessages)(void);

    int (*OpenModsMenu)(void);
    int (*CloseModsMenu)(void);
    int (*OpenMapMenu)(void);
    int (*CloseMapMenu)(void);

    // Replaces the whole HUD bitmask, so read it first and mask if you only mean
    // to change one element. This writes the loader's own ShowTitle, ShowFPS, and
    // ShowSR configuration entries, so the change persists across restarts.
    int (*SetHUDMode)(int mode);

    // These two only toggle the visibility of the HUD element and do not write the
    // configuration, so they are not the same as setting one bit through
    // SetHUDMode. ReadHUDState keeps reporting the configured bit, and the loader
    // restores that configured value whenever it rebuilds the HUD, which includes
    // every level load. Use them for a temporary hide and SetHUDMode for a lasting
    // change.
    int (*ShowTitle)(int visible);
    int (*ShowFPS)(int visible);
} BML_UIInterface;

BML_END_CDECLS

#ifdef __cplusplus

#include <string>

namespace BML::UI {

using HUDState = BML_UIHUDState;

enum HUDElement {
    HUD_TITLE = BML_UI_HUD_TITLE,
    HUD_FPS = BML_UI_HUD_FPS,
    HUD_SR = BML_UI_HUD_SR,
};

namespace Detail {

// Looked up once per Mod. The loader's table is static, so a null answer means
// the running loader does not carry this interface at all, which is not something
// that can change later in the process.
inline const BML_UIInterface *Interface() {
    static const BML_UIInterface *found =
        FindInterface<BML_UIInterface>(BML_UI_INTERFACE_ID, BML_UI_INTERFACE_MAJOR);
    return found;
}

} // namespace Detail

// Whether the running loader carries this interface. The functions below check
// for themselves, so call this directly only to probe.
[[nodiscard]] inline int RequireApi() {
    return Detail::Interface() != nullptr ? BML_OK : BML_ERROR_NOT_FOUND;
}

[[nodiscard]] inline int ReadHUDState(HUDState &out) {
    const BML_UIInterface *ui = Detail::Interface();
    if (!BML_IFACE_HAS(ui, BML_UIInterface, ReadHUDState))
        return BML_ERROR_NOT_FOUND;
    return ui->ReadHUDState(&out);
}

[[nodiscard]] inline int AddMessage(const std::string &message) {
    const BML_UIInterface *ui = Detail::Interface();
    if (!BML_IFACE_HAS(ui, BML_UIInterface, AddMessage))
        return BML_ERROR_NOT_FOUND;
    return ui->AddMessage(message.c_str());
}

[[nodiscard]] inline int ClearMessages() {
    const BML_UIInterface *ui = Detail::Interface();
    if (!BML_IFACE_HAS(ui, BML_UIInterface, ClearMessages))
        return BML_ERROR_NOT_FOUND;
    return ui->ClearMessages();
}

[[nodiscard]] inline int OpenModsMenu() {
    const BML_UIInterface *ui = Detail::Interface();
    if (!BML_IFACE_HAS(ui, BML_UIInterface, OpenModsMenu))
        return BML_ERROR_NOT_FOUND;
    return ui->OpenModsMenu();
}

[[nodiscard]] inline int CloseModsMenu() {
    const BML_UIInterface *ui = Detail::Interface();
    if (!BML_IFACE_HAS(ui, BML_UIInterface, CloseModsMenu))
        return BML_ERROR_NOT_FOUND;
    return ui->CloseModsMenu();
}

[[nodiscard]] inline int OpenMapMenu() {
    const BML_UIInterface *ui = Detail::Interface();
    if (!BML_IFACE_HAS(ui, BML_UIInterface, OpenMapMenu))
        return BML_ERROR_NOT_FOUND;
    return ui->OpenMapMenu();
}

[[nodiscard]] inline int CloseMapMenu() {
    const BML_UIInterface *ui = Detail::Interface();
    if (!BML_IFACE_HAS(ui, BML_UIInterface, CloseMapMenu))
        return BML_ERROR_NOT_FOUND;
    return ui->CloseMapMenu();
}

[[nodiscard]] inline int SetHUDMode(int mode) {
    const BML_UIInterface *ui = Detail::Interface();
    if (!BML_IFACE_HAS(ui, BML_UIInterface, SetHUDMode))
        return BML_ERROR_NOT_FOUND;
    return ui->SetHUDMode(mode);
}

[[nodiscard]] inline int ShowTitle(bool visible) {
    const BML_UIInterface *ui = Detail::Interface();
    if (!BML_IFACE_HAS(ui, BML_UIInterface, ShowTitle))
        return BML_ERROR_NOT_FOUND;
    return ui->ShowTitle(visible ? 1 : 0);
}

[[nodiscard]] inline int ShowFPS(bool visible) {
    const BML_UIInterface *ui = Detail::Interface();
    if (!BML_IFACE_HAS(ui, BML_UIInterface, ShowFPS))
        return BML_ERROR_NOT_FOUND;
    return ui->ShowFPS(visible ? 1 : 0);
}

} // namespace BML::UI

#endif // __cplusplus

#endif // BML_UI_H

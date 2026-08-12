#include "BML/BML.h"
#include "BML/Runtime.h"
#include "BML/Speedrun.h"
#include "BML/UI.h"

void BML_TestCAbiMemoryOwnership(char **strings, wchar_t **wideStrings, size_t count) {
    BML_FreeStringArray(strings, count);
    BML_FreeWStringArray(wideStrings, count);
}

// The loader owns what BML_GetLoaderPath returns and the caller owns what
// BML_GetModRoot returns, so this also checks that the two are spelled apart in C:
// dropping the const or freeing the borrowed one would not compile.
const char *BML_TestCAbiLoaderPath(void) {
    return BML_GetLoaderPathUtf8(BML_DIR_LOADER);
}

void BML_TestCAbiModRoot(void) {
    char *root = BML_GetModRootUtf8(NULL);
    BML_FreeString(root);
}

// BML_UnregisterCommand is the one function here that answers with a status code
// instead of 1 or 0, so this also checks that the codes it documents are reachable
// from C.
int BML_TestCAbiUnregisterCommand(const char *name) {
    const int result = BML_UnregisterCommand(name);
    if (result == BML_ERROR_NOT_FOUND || result == BML_ERROR_ACCESS_DENIED ||
        result == BML_ERROR_INVALID_PARAMETER)
        return 0;
    return result == BML_OK;
}

// Interface.h and every interface struct built on it have to compile as C, since
// a Mod that is not written in C++ reaches this capability only through them.
// This walks the whole sequence a C caller goes through: the lookup, the member
// check, and the call.
int BML_TestCAbiSpeedrunInterface(void) {
    const void *found = NULL;
    const BML_SpeedrunInterface *speedrun = NULL;
    BML_SpeedrunTimerState state = {0};

    if (BML_GetInterface(BML_SPEEDRUN_INTERFACE_ID, BML_SPEEDRUN_INTERFACE_MAJOR, &found) != BML_OK)
        return 0;
    speedrun = (const BML_SpeedrunInterface *) found;
    if (!BML_IFACE_HAS(speedrun, BML_SpeedrunInterface, ReadTimerState))
        return 0;
    if (speedrun->ReadTimerState(&state) != BML_OK)
        return 0;
    return state.ElapsedTime >= 0.0f;
}

// The runtime interface fills three out structs rather than one, so this checks
// that each of them is spelled and zeroed the C way.
int BML_TestCAbiRuntimeInterface(void) {
    const void *found = NULL;
    const BML_RuntimeInterface *runtime = NULL;
    BML_RuntimeState state = {0};
    BML_RuntimeClock clock = {0};
    BML_RuntimeScore score = {0};

    if (BML_GetInterface(BML_RUNTIME_INTERFACE_ID, BML_RUNTIME_INTERFACE_MAJOR, &found) != BML_OK)
        return 0;
    runtime = (const BML_RuntimeInterface *) found;
    if (!BML_IFACE_HAS(runtime, BML_RuntimeInterface, ReadScore))
        return 0;
    if (runtime->ReadState(&state) != BML_OK || runtime->ReadClock(&clock) != BML_OK ||
        runtime->ReadScore(&score) != BML_OK)
        return 0;
    return state.Playing && clock.Frame >= 0 && score.HS >= 0;
}

// The UI interface is mostly commands rather than reads, and its HUD bitmask is an
// enum, so this checks that a void-argument member, a string argument, and the
// BML_UI_HUD_* values are all reachable the C way.
int BML_TestCAbiUIInterface(const char *message) {
    const void *found = NULL;
    const BML_UIInterface *ui = NULL;
    BML_UIHUDState hud = {0};

    if (BML_GetInterface(BML_UI_INTERFACE_ID, BML_UI_INTERFACE_MAJOR, &found) != BML_OK)
        return 0;
    ui = (const BML_UIInterface *) found;
    if (!BML_IFACE_HAS(ui, BML_UIInterface, ShowFPS))
        return 0;
    if (ui->ReadHUDState(&hud) != BML_OK)
        return 0;
    if (ui->AddMessage(message) != BML_OK || ui->ClearMessages() != BML_OK)
        return 0;
    if (ui->SetHUDMode(hud.Mode | BML_UI_HUD_TITLE | BML_UI_HUD_FPS | BML_UI_HUD_SR) != BML_OK)
        return 0;
    return ui->ShowTitle(0) == BML_OK && ui->ShowFPS(1) == BML_OK;
}

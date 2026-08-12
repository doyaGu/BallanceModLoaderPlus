// The loader's side of Interface.h: the thunks the interface structs point at,
// the structs themselves, and the table BML_GetInterface looks in.
//
// Each struct is one static const instance shared by every Mod, so publishing an
// interface costs nothing at runtime and there is no registration order to get
// right. Adding an interface means a struct here plus one row in kInterfaces; the
// rules for changing one that already shipped are in Interface.h.
#include "BML/Interface.h"
#include "BML/Runtime.h"
#include "BML/Speedrun.h"
#include "BML/UI.h"

#include <iterator>
#include <limits>

#include "InterfaceRegistry.h"
#include "ModContext.h"

namespace {

// No thunk may unwind into a Mod's own C++ runtime, and none may touch loader
// state before the built-in Mods are there, so both checks live here instead of
// being repeated in every thunk. Before that point the interface is still handed
// out, because the table is static, and answers BML_ERROR_FAIL.
template <typename Body>
int Serve(Body &&body) {
    ModContext *context = BML_GetModContext();
    if (!context || !context->AreModsLoaded())
        return BML_ERROR_FAIL;
    try {
        return body(*context);
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

// The UI thunks read and write state the loader draws from the main thread, so
// they refuse a call from anywhere else rather than racing the frame that draws
// it. The read refuses too, so there is one rule for the whole interface.
template <typename Body>
int ServeOnMainThread(Body &&body) {
    return Serve([&body](ModContext &context) {
        if (!context.IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        return body(context);
    });
}

int RuntimeReadState(BML_RuntimeState *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return Serve([out](ModContext &context) {
        const BML::RuntimeStateSnapshot state = context.ReadRuntimeState();
        out->InGame = state.InGame ? 1 : 0;
        out->InLevel = state.InLevel ? 1 : 0;
        out->Paused = state.Paused ? 1 : 0;
        out->Playing = state.Playing ? 1 : 0;
        out->CheatEnabled = state.CheatEnabled ? 1 : 0;
        return BML_OK;
    });
}

int RuntimeReadClock(BML_RuntimeClock *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return Serve([out](ModContext &context) {
        CKTimeManager *time = context.GetTimeManager();
        if (!time)
            return BML_ERROR_UNAVAILABLE;
        out->TimeMs = time->GetTime();
        out->AbsoluteMs = time->GetAbsoluteTime();
        out->DeltaMs = time->GetLastDeltaTime();
        const CKDWORD tick = time->GetMainTickCount();
        out->Frame = tick > static_cast<CKDWORD>((std::numeric_limits<int>::max)())
                         ? (std::numeric_limits<int>::max)()
                         : static_cast<int>(tick);
        return BML_OK;
    });
}

int RuntimeReadScore(BML_RuntimeScore *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return Serve([out](ModContext &context) {
        out->SR = context.GetSRScore();
        out->HS = context.GetHSScore();
        return BML_OK;
    });
}

int SpeedrunReadTimerState(BML_SpeedrunTimerState *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return Serve([out](ModContext &context) {
        out->ElapsedTime = context.GetSRTime();
        return BML_OK;
    });
}

int SpeedrunSetTimerVisible(int visible) {
    return Serve([visible](ModContext &context) {
        context.ShowSRTimer(visible != 0);
        return BML_OK;
    });
}

int SpeedrunStartTimer() {
    return Serve([](ModContext &context) {
        context.StartSRTimer();
        return BML_OK;
    });
}

int SpeedrunPauseTimer() {
    return Serve([](ModContext &context) {
        context.PauseSRTimer();
        return BML_OK;
    });
}

int SpeedrunResetTimer() {
    return Serve([](ModContext &context) {
        context.ResetSRTimer();
        return BML_OK;
    });
}

int UIReadHUDState(BML_UIHUDState *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([out](ModContext &context) {
        out->Mode = context.GetHUD();
        return BML_OK;
    });
}

int UIAddMessage(const char *message) {
    if (!message)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([message](ModContext &context) {
        context.SendIngameMessage(message);
        return BML_OK;
    });
}

int UIClearMessages() {
    return ServeOnMainThread([](ModContext &context) {
        context.ClearIngameMessages();
        return BML_OK;
    });
}

int UIOpenModsMenu() {
    return ServeOnMainThread([](ModContext &context) {
        context.OpenModsMenu();
        return BML_OK;
    });
}

int UICloseModsMenu() {
    return ServeOnMainThread([](ModContext &context) {
        context.CloseModsMenu();
        return BML_OK;
    });
}

int UIOpenMapMenu() {
    return ServeOnMainThread([](ModContext &context) {
        context.OpenMapMenu();
        return BML_OK;
    });
}

int UICloseMapMenu() {
    return ServeOnMainThread([](ModContext &context) {
        context.CloseMapMenu();
        return BML_OK;
    });
}

int UISetHUDMode(int mode) {
    return ServeOnMainThread([mode](ModContext &context) {
        context.SetHUD(mode);
        return BML_OK;
    });
}

int UIShowTitle(int visible) {
    return ServeOnMainThread([visible](ModContext &context) {
        context.ShowTitle(visible != 0);
        return BML_OK;
    });
}

int UIShowFPS(int visible) {
    return ServeOnMainThread([visible](ModContext &context) {
        context.ShowFPS(visible != 0);
        return BML_OK;
    });
}

const BML_RuntimeInterface kRuntimeInterface = {
    BML_IFACE_HEADER(BML_RuntimeInterface, BML_RUNTIME_INTERFACE_ID, BML_RUNTIME_INTERFACE_MAJOR,
                     BML_RUNTIME_INTERFACE_MINOR),
    &RuntimeReadState,
    &RuntimeReadClock,
    &RuntimeReadScore,
};

const BML_SpeedrunInterface kSpeedrunInterface = {
    BML_IFACE_HEADER(BML_SpeedrunInterface, BML_SPEEDRUN_INTERFACE_ID, BML_SPEEDRUN_INTERFACE_MAJOR,
                     BML_SPEEDRUN_INTERFACE_MINOR),
    &SpeedrunReadTimerState,
    &SpeedrunSetTimerVisible,
    &SpeedrunStartTimer,
    &SpeedrunPauseTimer,
    &SpeedrunResetTimer,
};

const BML_UIInterface kUIInterface = {
    BML_IFACE_HEADER(BML_UIInterface, BML_UI_INTERFACE_ID, BML_UI_INTERFACE_MAJOR,
                     BML_UI_INTERFACE_MINOR),
    &UIReadHUDState,
    &UIAddMessage,
    &UIClearMessages,
    &UIOpenModsMenu,
    &UICloseModsMenu,
    &UIOpenMapMenu,
    &UICloseMapMenu,
    &UISetHUDMode,
    &UIShowTitle,
    &UIShowFPS,
};

const BML::InterfaceEntry kInterfaces[] = {
    {BML_RUNTIME_INTERFACE_ID, BML_RUNTIME_INTERFACE_MAJOR, &kRuntimeInterface},
    {BML_SPEEDRUN_INTERFACE_ID, BML_SPEEDRUN_INTERFACE_MAJOR, &kSpeedrunInterface},
    {BML_UI_INTERFACE_ID, BML_UI_INTERFACE_MAJOR, &kUIInterface},
};

} // namespace

int BML_GetInterface(const char *interfaceId, uint16_t majorVersion, const void **out) {
    return BML::FindInterface(kInterfaces, std::size(kInterfaces), interfaceId, majorVersion, out);
}

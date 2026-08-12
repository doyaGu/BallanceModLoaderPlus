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

const BML::InterfaceEntry kInterfaces[] = {
    {BML_RUNTIME_INTERFACE_ID, BML_RUNTIME_INTERFACE_MAJOR, &kRuntimeInterface},
    {BML_SPEEDRUN_INTERFACE_ID, BML_SPEEDRUN_INTERFACE_MAJOR, &kSpeedrunInterface},
};

} // namespace

int BML_GetInterface(const char *interfaceId, uint16_t majorVersion, const void **out) {
    return BML::FindInterface(kInterfaces, std::size(kInterfaces), interfaceId, majorVersion, out);
}

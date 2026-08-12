// The loader's side of Interface.h: the thunks the interface structs point at,
// the structs themselves, and the table BML_GetInterface looks in.
//
// Each struct is one static const instance shared by every Mod, so publishing an
// interface costs nothing at runtime and there is no registration order to get
// right. Adding an interface means a struct here plus one row in kInterfaces; the
// rules for changing one that already shipped are in Interface.h.
#include "BML/Events.h"
#include "BML/Gameplay.h"
#include "BML/Interface.h"
#include "BML/Runtime.h"
#include "BML/Scene.h"
#include "BML/Speedrun.h"
#include "BML/UI.h"

#include <iterator>
#include <limits>

#include "BuiltinCapabilities.h"
#include "EventStreams.h"
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

// The gameplay, scene, and UI thunks touch the game's data arrays, its objects,
// and the UI the loader draws from the main thread, so they refuse a call from anywhere else rather than racing the
// frame that draws it. The reads refuse too, so there is one rule per interface
// rather than one per member.
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

int GameplayReadLevel(BML_GameplayLevelState *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([out](ModContext &context) {
        return ReadBuiltinGameplayLevel(context, *out);
    });
}

int GameplayReadEnergy(BML_GameplayEnergyState *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([out](ModContext &context) {
        return ReadBuiltinGameplayEnergy(context, *out);
    });
}

int GameplayReadCatalogCount(size_t *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([out](ModContext &context) {
        return ReadBuiltinGameplayCatalogCount(context, *out);
    });
}

int GameplayReadCatalogEntry(size_t index, BML_GameplayCatalogEntry *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([index, out](ModContext &context) {
        return ReadBuiltinGameplayCatalogEntry(context, index, *out);
    });
}

int GameplayReadCheckpointCount(size_t *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([out](ModContext &context) {
        return ReadBuiltinGameplayCheckpointCount(context, *out);
    });
}

int GameplayReadCheckpoint(size_t index, BML_GameplayCheckpoint *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([index, out](ModContext &context) {
        return ReadBuiltinGameplayCheckpoint(context, index, *out);
    });
}

int GameplayReadResetpointCount(size_t *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([out](ModContext &context) {
        return ReadBuiltinGameplayResetpointCount(context, *out);
    });
}

int GameplayReadResetpoint(size_t index, BML_GameplayResetpoint *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([index, out](ModContext &context) {
        return ReadBuiltinGameplayResetpoint(context, index, *out);
    });
}

int SceneReadObject(BML_ObjectRef object, BML_SceneObjectInfo *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([object, out](ModContext &context) {
        return ReadBuiltinSceneObject(context, object, *out);
    });
}

int SceneReadEntityTransform(BML_ObjectRef object, BML_SceneEntityTransform *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([object, out](ModContext &context) {
        return ReadBuiltinSceneEntityTransform(context, object, *out);
    });
}

int SceneFindObject(const char *name, BML_ObjectRef *out) {
    if (!name || !out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([name, out](ModContext &context) {
        return FindBuiltinSceneObject(context, name, *out);
    });
}

int SceneFindObjectOfClass(const char *name, int classId, BML_ObjectRef *out) {
    if (!name || !out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([name, classId, out](ModContext &context) {
        return FindBuiltinSceneObjectOfClass(context, name, classId, *out);
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

// The event queues carry no locks, and every one of these takes a handle a Mod is
// holding, so the game thread owns them: opening, polling, reading, and closing a
// stream all refuse a call from anywhere else. EventStreams.h keeps the queues
// themselves, so nothing here needs the ModContext beyond that rule.
int EventsOpenStream(int capacity, BML_EventStream *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([capacity, out](ModContext &) {
        return BML::OpenEventStream(capacity, *out);
    });
}

int EventsCloseStream(BML_EventStream stream) {
    return ServeOnMainThread([stream](ModContext &) {
        return BML::CloseEventStream(stream);
    });
}

int EventsReadDroppedCount(BML_EventStream stream, int *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([stream, out](ModContext &) {
        return BML::ReadEventStreamDroppedCount(stream, *out);
    });
}

int EventsPoll(BML_EventStream stream, BML_EventInfo *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([stream, out](ModContext &) {
        return BML::PollEventStream(stream, *out);
    });
}

int EventsReadLoad(BML_EventStream stream, BML_EventLoad *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([stream, out](ModContext &) {
        return BML::ReadEventLoad(stream, *out);
    });
}

int EventsReadLoadObject(BML_EventStream stream, size_t index, BML_ObjectRef *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([stream, index, out](ModContext &) {
        return BML::ReadEventLoadObject(stream, index, *out);
    });
}

int EventsReadPhysics(BML_EventStream stream, BML_EventPhysics *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([stream, out](ModContext &) {
        return BML::ReadEventPhysics(stream, *out);
    });
}

int EventsReadPhysicsConvexMesh(BML_EventStream stream, size_t index, BML_ObjectRef *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([stream, index, out](ModContext &) {
        return BML::ReadEventPhysicsConvexMesh(stream, index, *out);
    });
}

int EventsReadPhysicsBall(BML_EventStream stream, size_t index, BML_Vec3 *outCenter,
                          float *outRadius) {
    if (!outCenter || !outRadius)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([stream, index, outCenter, outRadius](ModContext &) {
        return BML::ReadEventPhysicsBall(stream, index, *outCenter, *outRadius);
    });
}

int EventsReadPhysicsConcaveMesh(BML_EventStream stream, size_t index, BML_ObjectRef *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([stream, index, out](ModContext &) {
        return BML::ReadEventPhysicsConcaveMesh(stream, index, *out);
    });
}

int EventsReadCommand(BML_EventStream stream, BML_EventCommand *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([stream, out](ModContext &) {
        return BML::ReadEventCommand(stream, *out);
    });
}

int EventsReadCommandArgument(BML_EventStream stream, size_t index, BML_EventText *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([stream, index, out](ModContext &) {
        return BML::ReadEventCommandArgument(stream, index, *out);
    });
}

int EventsReadConfig(BML_EventStream stream, BML_EventConfig *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([stream, out](ModContext &) {
        return BML::ReadEventConfig(stream, *out);
    });
}

int EventsReadCheat(BML_EventStream stream, BML_EventCheat *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    return ServeOnMainThread([stream, out](ModContext &) {
        return BML::ReadEventCheat(stream, *out);
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

const BML_GameplayInterface kGameplayInterface = {
    BML_IFACE_HEADER(BML_GameplayInterface, BML_GAMEPLAY_INTERFACE_ID, BML_GAMEPLAY_INTERFACE_MAJOR,
                     BML_GAMEPLAY_INTERFACE_MINOR),
    &GameplayReadLevel,
    &GameplayReadEnergy,
    &GameplayReadCatalogCount,
    &GameplayReadCatalogEntry,
    &GameplayReadCheckpointCount,
    &GameplayReadCheckpoint,
    &GameplayReadResetpointCount,
    &GameplayReadResetpoint,
};

const BML_SceneInterface kSceneInterface = {
    BML_IFACE_HEADER(BML_SceneInterface, BML_SCENE_INTERFACE_ID, BML_SCENE_INTERFACE_MAJOR,
                     BML_SCENE_INTERFACE_MINOR),
    &SceneReadObject,
    &SceneReadEntityTransform,
    &SceneFindObject,
    &SceneFindObjectOfClass,
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

const BML_EventsInterface kEventsInterface = {
    BML_IFACE_HEADER(BML_EventsInterface, BML_EVENTS_INTERFACE_ID, BML_EVENTS_INTERFACE_MAJOR,
                     BML_EVENTS_INTERFACE_MINOR),
    &EventsOpenStream,
    &EventsCloseStream,
    &EventsReadDroppedCount,
    &EventsPoll,
    &EventsReadLoad,
    &EventsReadLoadObject,
    &EventsReadPhysics,
    &EventsReadPhysicsConvexMesh,
    &EventsReadPhysicsBall,
    &EventsReadPhysicsConcaveMesh,
    &EventsReadCommand,
    &EventsReadCommandArgument,
    &EventsReadConfig,
    &EventsReadCheat,
};

const BML::InterfaceEntry kInterfaces[] = {
    {BML_EVENTS_INTERFACE_ID, BML_EVENTS_INTERFACE_MAJOR, &kEventsInterface},
    {BML_GAMEPLAY_INTERFACE_ID, BML_GAMEPLAY_INTERFACE_MAJOR, &kGameplayInterface},
    {BML_RUNTIME_INTERFACE_ID, BML_RUNTIME_INTERFACE_MAJOR, &kRuntimeInterface},
    {BML_SCENE_INTERFACE_ID, BML_SCENE_INTERFACE_MAJOR, &kSceneInterface},
    {BML_SPEEDRUN_INTERFACE_ID, BML_SPEEDRUN_INTERFACE_MAJOR, &kSpeedrunInterface},
    {BML_UI_INTERFACE_ID, BML_UI_INTERFACE_MAJOR, &kUIInterface},
};

} // namespace

int BML_GetInterface(const char *interfaceId, uint16_t majorVersion, const void **out) {
    return BML::FindInterface(kInterfaces, std::size(kInterfaces), interfaceId, majorVersion, out);
}

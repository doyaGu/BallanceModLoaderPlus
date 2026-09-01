// The loader's side of Interface.h: the thunks the interface structs point at,
// the structs themselves, and the table BML_GetInterface looks in.
//
// Each struct is one static const instance shared by every Mod, so publishing an
// interface costs nothing at runtime and there is no registration order to get
// right. Adding an interface means a struct here plus one row in kInterfaces; the
// rules for changing one that already shipped are in Interface.h.
#include "BML/Gameplay.h"
#include "BML/Behavior.h"
#include "BML/Interface.h"
#include "BML/Runtime.h"
#include "BML/Scene.h"
#include "BML/Speedrun.h"
#include "BML/UI.h"

#include <cstring>
#include <iterator>
#include <limits>

#include "Api/BuiltinCapabilities.h"
#include "Api/BehaviorApi.h"
#if BML_ENABLE_PLAYER_TESTS
#include "Api/BehaviorTestApi.h"
#endif
#include "Loader/ModContext.h"

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
        const BML::GameSessionSnapshot session = context.ReadGameSession();
        out->InGame = session.IsInGame() ? 1 : 0;
        out->InLevel = session.IsInLevel() ? 1 : 0;
        out->Paused = session.IsPaused() ? 1 : 0;
        out->Playing = session.IsPlaying() ? 1 : 0;
        out->CheatEnabled = context.IsCheatEnabled() ? 1 : 0;
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

struct InterfaceEntry {
    const char *Id;
    uint16_t MajorVersion;
    const void *Interface;
};

const InterfaceEntry kInterfaces[] = {
#if BML_ENABLE_PLAYER_TESTS
    {BML_BEHAVIOR_TEST_INTERFACE_ID, BML_BEHAVIOR_TEST_INTERFACE_MAJOR,
     &BML::Api::BehaviorTestInterface()},
#endif
    {BML_BEHAVIOR_INTERFACE_ID, BML_BEHAVIOR_INTERFACE_MAJOR,
     &BML::Api::BehaviorInterface()},
    {BML_GAMEPLAY_INTERFACE_ID, BML_GAMEPLAY_INTERFACE_MAJOR, &kGameplayInterface},
    {BML_RUNTIME_INTERFACE_ID, BML_RUNTIME_INTERFACE_MAJOR, &kRuntimeInterface},
    {BML_SCENE_INTERFACE_ID, BML_SCENE_INTERFACE_MAJOR, &kSceneInterface},
    {BML_SPEEDRUN_INTERFACE_ID, BML_SPEEDRUN_INTERFACE_MAJOR, &kSpeedrunInterface},
    {BML_UI_INTERFACE_ID, BML_UI_INTERFACE_MAJOR, &kUIInterface},
};

} // namespace

int BML_GetInterface(const char *interfaceId, uint16_t majorVersion, const void **out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = nullptr;
    if (!interfaceId || interfaceId[0] == '\0')
        return BML_ERROR_INVALID_PARAMETER;

    bool idExists = false;
    for (const InterfaceEntry &entry : kInterfaces) {
        if (std::strcmp(entry.Id, interfaceId) != 0)
            continue;
        idExists = true;
        if (entry.MajorVersion != majorVersion)
            continue;
        *out = entry.Interface;
        return BML_OK;
    }
    return idExists ? BML_ERROR_VERSION_MISMATCH : BML_ERROR_NOT_FOUND;
}

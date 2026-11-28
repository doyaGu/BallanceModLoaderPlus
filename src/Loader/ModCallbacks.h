#ifndef BML_MODCALLBACKS_H
#define BML_MODCALLBACKS_H

#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include "BML/Mod.h"
#include "CKAll.h"

class Property;

namespace BML {
struct CommandRecord;
}

// Internal callback ids reuse the public game-event values. Native payloads and
// local concrete callbacks therefore share names, ordering, and event identity.
enum class ModEvent {
    OnPreStartMenu = BML_GAME_EVENT_PRE_START_MENU,
    OnPostStartMenu = BML_GAME_EVENT_POST_START_MENU,
    OnExitGame = BML_GAME_EVENT_EXIT_GAME,
    OnPreLoadLevel = BML_GAME_EVENT_PRE_LOAD_LEVEL,
    OnPostLoadLevel = BML_GAME_EVENT_POST_LOAD_LEVEL,
    OnStartLevel = BML_GAME_EVENT_START_LEVEL,
    OnPreResetLevel = BML_GAME_EVENT_PRE_RESET_LEVEL,
    OnPostResetLevel = BML_GAME_EVENT_POST_RESET_LEVEL,
    OnPauseLevel = BML_GAME_EVENT_PAUSE_LEVEL,
    OnUnpauseLevel = BML_GAME_EVENT_UNPAUSE_LEVEL,
    OnPreExitLevel = BML_GAME_EVENT_PRE_EXIT_LEVEL,
    OnPostExitLevel = BML_GAME_EVENT_POST_EXIT_LEVEL,
    OnPreNextLevel = BML_GAME_EVENT_PRE_NEXT_LEVEL,
    OnPostNextLevel = BML_GAME_EVENT_POST_NEXT_LEVEL,
    OnDead = BML_GAME_EVENT_DEAD,
    OnPreEndLevel = BML_GAME_EVENT_PRE_END_LEVEL,
    OnPostEndLevel = BML_GAME_EVENT_POST_END_LEVEL,
    OnCounterActive = BML_GAME_EVENT_COUNTER_ACTIVE,
    OnCounterInactive = BML_GAME_EVENT_COUNTER_INACTIVE,
    OnBallNavActive = BML_GAME_EVENT_BALL_NAV_ACTIVE,
    OnBallNavInactive = BML_GAME_EVENT_BALL_NAV_INACTIVE,
    OnCamNavActive = BML_GAME_EVENT_CAM_NAV_ACTIVE,
    OnCamNavInactive = BML_GAME_EVENT_CAM_NAV_INACTIVE,
    OnBallOff = BML_GAME_EVENT_BALL_OFF,
    OnPreCheckpointReached = BML_GAME_EVENT_PRE_CHECKPOINT_REACHED,
    OnPostCheckpointReached = BML_GAME_EVENT_POST_CHECKPOINT_REACHED,
    OnLevelFinish = BML_GAME_EVENT_LEVEL_FINISH,
    OnGameOver = BML_GAME_EVENT_GAME_OVER,
    OnExtraPoint = BML_GAME_EVENT_EXTRA_POINT,
    OnPreSubLife = BML_GAME_EVENT_PRE_SUB_LIFE,
    OnPostSubLife = BML_GAME_EVENT_POST_SUB_LIFE,
    OnPreLifeUp = BML_GAME_EVENT_PRE_LIFE_UP,
    OnPostLifeUp = BML_GAME_EVENT_POST_LIFE_UP,
    OnLoad,
    OnUnload,
    OnModifyConfig,
    OnLoadObject,
    OnLoadScript,
    OnProcess,
    OnRender,
    OnCheatEnabled,
    OnPhysicalize,
    OnUnphysicalize,
    OnPreCommandExecute,
    OnPostCommandExecute,
    Count
};

template <ModEvent Event> struct ModEventTraits;

// Bind only callbacks actually declared by a concrete implementation. The
// stored function pointers never allocate and do not depend on a C++ vtable.
#define BML_LOCAL_MOD_CALLBACK(Name, Arguments, ...)                                               \
    template <> struct ModEventTraits<ModEvent::Name> {                                            \
        using Callback = void (*)(void *__VA_OPT__(, ) __VA_ARGS__);                               \
        template <class T> static Callback Bind() {                                                \
            if constexpr (requires(T &value __VA_OPT__(, ) __VA_ARGS__) {                          \
                              value.Name Arguments;                                                \
                          }) {                                                                     \
                return [](void *object __VA_OPT__(, ) __VA_ARGS__) {                               \
                    static_cast<T *>(object)->Name Arguments;                                      \
                };                                                                                 \
            } else {                                                                               \
                return nullptr;                                                                    \
            }                                                                                      \
        }                                                                                          \
    };

BML_LOCAL_MOD_CALLBACK(OnPreStartMenu, ())
BML_LOCAL_MOD_CALLBACK(OnPostStartMenu, ())
BML_LOCAL_MOD_CALLBACK(OnExitGame, ())
BML_LOCAL_MOD_CALLBACK(OnPreLoadLevel, ())
BML_LOCAL_MOD_CALLBACK(OnPostLoadLevel, ())
BML_LOCAL_MOD_CALLBACK(OnStartLevel, ())
BML_LOCAL_MOD_CALLBACK(OnPreResetLevel, ())
BML_LOCAL_MOD_CALLBACK(OnPostResetLevel, ())
BML_LOCAL_MOD_CALLBACK(OnPauseLevel, ())
BML_LOCAL_MOD_CALLBACK(OnUnpauseLevel, ())
BML_LOCAL_MOD_CALLBACK(OnPreExitLevel, ())
BML_LOCAL_MOD_CALLBACK(OnPostExitLevel, ())
BML_LOCAL_MOD_CALLBACK(OnPreNextLevel, ())
BML_LOCAL_MOD_CALLBACK(OnPostNextLevel, ())
BML_LOCAL_MOD_CALLBACK(OnDead, ())
BML_LOCAL_MOD_CALLBACK(OnPreEndLevel, ())
BML_LOCAL_MOD_CALLBACK(OnPostEndLevel, ())
BML_LOCAL_MOD_CALLBACK(OnCounterActive, ())
BML_LOCAL_MOD_CALLBACK(OnCounterInactive, ())
BML_LOCAL_MOD_CALLBACK(OnBallNavActive, ())
BML_LOCAL_MOD_CALLBACK(OnBallNavInactive, ())
BML_LOCAL_MOD_CALLBACK(OnCamNavActive, ())
BML_LOCAL_MOD_CALLBACK(OnCamNavInactive, ())
BML_LOCAL_MOD_CALLBACK(OnBallOff, ())
BML_LOCAL_MOD_CALLBACK(OnPreCheckpointReached, ())
BML_LOCAL_MOD_CALLBACK(OnPostCheckpointReached, ())
BML_LOCAL_MOD_CALLBACK(OnLevelFinish, ())
BML_LOCAL_MOD_CALLBACK(OnGameOver, ())
BML_LOCAL_MOD_CALLBACK(OnExtraPoint, ())
BML_LOCAL_MOD_CALLBACK(OnPreSubLife, ())
BML_LOCAL_MOD_CALLBACK(OnPostSubLife, ())
BML_LOCAL_MOD_CALLBACK(OnPreLifeUp, ())
BML_LOCAL_MOD_CALLBACK(OnPostLifeUp, ())
BML_LOCAL_MOD_CALLBACK(OnLoad, ())
BML_LOCAL_MOD_CALLBACK(OnUnload, ())
BML_LOCAL_MOD_CALLBACK(OnModifyConfig, (category, key, prop), const char *category, const char *key,
                       Property *prop)
BML_LOCAL_MOD_CALLBACK(OnLoadObject,
                       (filename, isMap, masterName, filterClass, addToScene, reuseMeshes,
                        reuseMaterials, dynamic, objArray, masterObj),
                       const char *filename, CKBOOL isMap, const char *masterName,
                       CK_CLASSID filterClass, CKBOOL addToScene, CKBOOL reuseMeshes,
                       CKBOOL reuseMaterials, CKBOOL dynamic, XObjectArray *objArray,
                       CKObject *masterObj)
BML_LOCAL_MOD_CALLBACK(OnLoadScript, (filename, script), const char *filename, CKBehavior *script)
BML_LOCAL_MOD_CALLBACK(OnProcess, ())
BML_LOCAL_MOD_CALLBACK(OnRender, (flags), CK_RENDER_FLAGS flags)
BML_LOCAL_MOD_CALLBACK(OnCheatEnabled, (enable), bool enable)
BML_LOCAL_MOD_CALLBACK(OnPhysicalize,
                       (target, fixed, friction, elasticity, mass, collGroup, startFrozen,
                        enableColl, calcMassCenter, linearDamp, rotDamp, collSurface, massCenter,
                        convexCnt, convexMesh, ballCnt, ballCenter, ballRadius, concaveCnt,
                        concaveMesh),
                       CK3dEntity *target, CKBOOL fixed, float friction, float elasticity,
                       float mass, const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl,
                       CKBOOL calcMassCenter, float linearDamp, float rotDamp,
                       const char *collSurface, VxVector massCenter, int convexCnt,
                       CKMesh **convexMesh, int ballCnt, VxVector *ballCenter, float *ballRadius,
                       int concaveCnt, CKMesh **concaveMesh)
BML_LOCAL_MOD_CALLBACK(OnUnphysicalize, (target), CK3dEntity *target)
BML_LOCAL_MOD_CALLBACK(OnPreCommandExecute, (command, args), BML::CommandRecord *command,
                       const std::vector<std::string> &args)
BML_LOCAL_MOD_CALLBACK(OnPostCommandExecute, (command, args), BML::CommandRecord *command,
                       const std::vector<std::string> &args)

#undef BML_LOCAL_MOD_CALLBACK

using ModCallbackTable = std::tuple<
    ModEventTraits<ModEvent::OnPreStartMenu>::Callback,
    ModEventTraits<ModEvent::OnPostStartMenu>::Callback,
    ModEventTraits<ModEvent::OnExitGame>::Callback,
    ModEventTraits<ModEvent::OnPreLoadLevel>::Callback,
    ModEventTraits<ModEvent::OnPostLoadLevel>::Callback,
    ModEventTraits<ModEvent::OnStartLevel>::Callback,
    ModEventTraits<ModEvent::OnPreResetLevel>::Callback,
    ModEventTraits<ModEvent::OnPostResetLevel>::Callback,
    ModEventTraits<ModEvent::OnPauseLevel>::Callback,
    ModEventTraits<ModEvent::OnUnpauseLevel>::Callback,
    ModEventTraits<ModEvent::OnPreExitLevel>::Callback,
    ModEventTraits<ModEvent::OnPostExitLevel>::Callback,
    ModEventTraits<ModEvent::OnPreNextLevel>::Callback,
    ModEventTraits<ModEvent::OnPostNextLevel>::Callback, ModEventTraits<ModEvent::OnDead>::Callback,
    ModEventTraits<ModEvent::OnPreEndLevel>::Callback,
    ModEventTraits<ModEvent::OnPostEndLevel>::Callback,
    ModEventTraits<ModEvent::OnCounterActive>::Callback,
    ModEventTraits<ModEvent::OnCounterInactive>::Callback,
    ModEventTraits<ModEvent::OnBallNavActive>::Callback,
    ModEventTraits<ModEvent::OnBallNavInactive>::Callback,
    ModEventTraits<ModEvent::OnCamNavActive>::Callback,
    ModEventTraits<ModEvent::OnCamNavInactive>::Callback,
    ModEventTraits<ModEvent::OnBallOff>::Callback,
    ModEventTraits<ModEvent::OnPreCheckpointReached>::Callback,
    ModEventTraits<ModEvent::OnPostCheckpointReached>::Callback,
    ModEventTraits<ModEvent::OnLevelFinish>::Callback,
    ModEventTraits<ModEvent::OnGameOver>::Callback,
    ModEventTraits<ModEvent::OnExtraPoint>::Callback,
    ModEventTraits<ModEvent::OnPreSubLife>::Callback,
    ModEventTraits<ModEvent::OnPostSubLife>::Callback,
    ModEventTraits<ModEvent::OnPreLifeUp>::Callback,
    ModEventTraits<ModEvent::OnPostLifeUp>::Callback, ModEventTraits<ModEvent::OnLoad>::Callback,
    ModEventTraits<ModEvent::OnUnload>::Callback,
    ModEventTraits<ModEvent::OnModifyConfig>::Callback,
    ModEventTraits<ModEvent::OnLoadObject>::Callback,
    ModEventTraits<ModEvent::OnLoadScript>::Callback, ModEventTraits<ModEvent::OnProcess>::Callback,
    ModEventTraits<ModEvent::OnRender>::Callback,
    ModEventTraits<ModEvent::OnCheatEnabled>::Callback,
    ModEventTraits<ModEvent::OnPhysicalize>::Callback,
    ModEventTraits<ModEvent::OnUnphysicalize>::Callback,
    ModEventTraits<ModEvent::OnPreCommandExecute>::Callback,
    ModEventTraits<ModEvent::OnPostCommandExecute>::Callback>;

#endif

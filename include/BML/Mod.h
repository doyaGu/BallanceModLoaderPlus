/* Native Mod protocol. All borrowed records are valid only for the current
 * call. Tables and metadata returned by the Mod remain immutable while loaded.
 * See docs/design/native-mod-api.md for ownership and evolution rules. */
#ifndef BML_MOD_H
#define BML_MOD_H

#include "BML/Interface.h"
#include "BML/Types.h"

BML_BEGIN_CDECLS
#define BML_MOD_DEPENDENCY_1_0_SIZE                                                                \
    (offsetof(BML_ModDependency, Optional) + sizeof(((BML_ModDependency *)0)->Optional))
#define BML_MOD_DEPENDENCY_INIT {.StructSize = sizeof(BML_ModDependency)}
#define BML_MOD_DEFINITION_1_0_SIZE                                                                \
    (offsetof(BML_ModDefinition, DependencyStride) +                                               \
     sizeof(((BML_ModDefinition *)0)->DependencyStride))
#define BML_MOD_DEFINITION_INIT {.StructSize = sizeof(BML_ModDefinition)}
#define BML_MOD_INTERFACE_1_0_SIZE                                                                 \
    (offsetof(BML_ModInterface, Destroy) + sizeof(((BML_ModInterface *)0)->Destroy))
#define BML_RENDER_EVENT_1_0_SIZE                                                                  \
    (offsetof(BML_RenderEvent, Flags) + sizeof(((BML_RenderEvent *)0)->Flags))
#define BML_RENDER_EVENT_INIT {.StructSize = sizeof(BML_RenderEvent)}
#define BML_CHEAT_EVENT_1_0_SIZE                                                                   \
    (offsetof(BML_CheatEvent, Enabled) + sizeof(((BML_CheatEvent *)0)->Enabled))
#define BML_CHEAT_EVENT_INIT {.StructSize = sizeof(BML_CheatEvent)}
#define BML_LOAD_OBJECT_EVENT_1_0_SIZE                                                             \
    (offsetof(BML_LoadObjectEvent, MasterObject) + sizeof(((BML_LoadObjectEvent *)0)->MasterObject))
#define BML_LOAD_OBJECT_EVENT_INIT {.StructSize = sizeof(BML_LoadObjectEvent)}
#define BML_LOAD_SCRIPT_EVENT_1_0_SIZE                                                             \
    (offsetof(BML_LoadScriptEvent, Script) + sizeof(((BML_LoadScriptEvent *)0)->Script))
#define BML_LOAD_SCRIPT_EVENT_INIT {.StructSize = sizeof(BML_LoadScriptEvent)}
#define BML_COMMAND_EVENT_1_0_SIZE                                                                 \
    (offsetof(BML_CommandEvent, Cheat) + sizeof(((BML_CommandEvent *)0)->Cheat))
#define BML_COMMAND_EVENT_INIT {.StructSize = sizeof(BML_CommandEvent)}
#define BML_CONFIG_EVENT_1_0_SIZE                                                                  \
    (offsetof(BML_ConfigEvent, Property) + sizeof(((BML_ConfigEvent *)0)->Property))
#define BML_CONFIG_EVENT_INIT {.StructSize = sizeof(BML_ConfigEvent)}
#define BML_PHYSICALIZE_EVENT_1_0_SIZE                                                             \
    (offsetof(BML_PhysicalizeEvent, ConcaveCount) +                                                \
     sizeof(((BML_PhysicalizeEvent *)0)->ConcaveCount))
#define BML_PHYSICALIZE_EVENT_INIT {.StructSize = sizeof(BML_PhysicalizeEvent)}
#define BML_OBJECT_EVENT_1_0_SIZE                                                                  \
    (offsetof(BML_ObjectEvent, Object) + sizeof(((BML_ObjectEvent *)0)->Object))
#define BML_OBJECT_EVENT_INIT {.StructSize = sizeof(BML_ObjectEvent)}
#define BML_MOD_CALLBACKS_1_0_SIZE                                                                 \
    (offsetof(BML_ModCallbacks, OnUnphysicalize) + sizeof(((BML_ModCallbacks *)0)->OnUnphysicalize))
#define BML_MOD_CALLBACKS_INIT {.StructSize = sizeof(BML_ModCallbacks)}
#define BML_MOD_INFO_1_0_SIZE                                                                      \
    (offsetof(BML_ModInfo, Diagnostic) + sizeof(((BML_ModInfo *)0)->Diagnostic))
#define BML_MOD_INFO_INIT {.StructSize = sizeof(BML_ModInfo)}
#define BML_MOD_CONTEXT_INTERFACE_1_0_SIZE                                                         \
    (offsetof(BML_ModContextInterface, ResourceExists) +                                           \
     sizeof(((BML_ModContextInterface *)0)->ResourceExists))
#pragma pack(push, 8)
#define BML_MOD_INTERFACE_ID "bml.mod"
#define BML_MOD_CONTEXT_INTERFACE_ID "bml.mod.context"
#define BML_MOD_INTERFACE_MAJOR 1u
#define BML_MOD_CONTEXT_INTERFACE_MAJOR 1u
#define BML_MOD_DIRECTORY_ROOT 5u
#define BML_MOD_KIND_UNKNOWN 0u
#define BML_MOD_KIND_NATIVE 1u
#define BML_MOD_KIND_SCRIPT 2u
#define BML_MOD_STATE_NOT_FOUND 0u
#define BML_MOD_STATE_REGISTERED 1u
#define BML_MOD_STATE_LOADED 2u
#define BML_MOD_STATE_FAILED 3u
#define BML_LOG_INFO 0u
#define BML_LOG_WARN 1u
#define BML_LOG_ERROR 2u
#define BML_COMMAND_EVENT_PRE 0u
#define BML_COMMAND_EVENT_POST 1u
#define BML_COMMAND_EVENT_EXECUTE 2u
#define BML_COMMAND_EVENT_COMPLETE 3u
#define BML_GAME_EVENT_PRE_START_MENU 0u
#define BML_GAME_EVENT_POST_START_MENU 1u
#define BML_GAME_EVENT_EXIT_GAME 2u
#define BML_GAME_EVENT_PRE_LOAD_LEVEL 3u
#define BML_GAME_EVENT_POST_LOAD_LEVEL 4u
#define BML_GAME_EVENT_START_LEVEL 5u
#define BML_GAME_EVENT_PRE_RESET_LEVEL 6u
#define BML_GAME_EVENT_POST_RESET_LEVEL 7u
#define BML_GAME_EVENT_PAUSE_LEVEL 8u
#define BML_GAME_EVENT_UNPAUSE_LEVEL 9u
#define BML_GAME_EVENT_PRE_EXIT_LEVEL 10u
#define BML_GAME_EVENT_POST_EXIT_LEVEL 11u
#define BML_GAME_EVENT_PRE_NEXT_LEVEL 12u
#define BML_GAME_EVENT_POST_NEXT_LEVEL 13u
#define BML_GAME_EVENT_DEAD 14u
#define BML_GAME_EVENT_PRE_END_LEVEL 15u
#define BML_GAME_EVENT_POST_END_LEVEL 16u
#define BML_GAME_EVENT_COUNTER_ACTIVE 17u
#define BML_GAME_EVENT_COUNTER_INACTIVE 18u
#define BML_GAME_EVENT_BALL_NAV_ACTIVE 19u
#define BML_GAME_EVENT_BALL_NAV_INACTIVE 20u
#define BML_GAME_EVENT_CAM_NAV_ACTIVE 21u
#define BML_GAME_EVENT_CAM_NAV_INACTIVE 22u
#define BML_GAME_EVENT_BALL_OFF 23u
#define BML_GAME_EVENT_PRE_CHECKPOINT_REACHED 24u
#define BML_GAME_EVENT_POST_CHECKPOINT_REACHED 25u
#define BML_GAME_EVENT_LEVEL_FINISH 26u
#define BML_GAME_EVENT_GAME_OVER 27u
#define BML_GAME_EVENT_EXTRA_POINT 28u
#define BML_GAME_EVENT_PRE_SUB_LIFE 29u
#define BML_GAME_EVENT_POST_SUB_LIFE 30u
#define BML_GAME_EVENT_PRE_LIFE_UP 31u
#define BML_GAME_EVENT_POST_LIFE_UP 32u

typedef uint64_t BML_ModHandle;
typedef uint64_t BML_ModContextHandle;
typedef uint64_t BML_ConfigPropertyHandle;

typedef struct BML_ModDependency {
    size_t StructSize;
    const char *Id;
    const char *MinimumVersion;
    uint32_t Optional;
} BML_ModDependency;

typedef struct BML_ModDefinition {
    size_t StructSize;
    const char *Id;
    const char *Name;
    const char *Version;
    const char *Author;
    const char *Description;
    const char *MinimumLoaderVersion;
    const BML_ModDependency *Dependencies;
    size_t DependencyCount;
    size_t DependencyStride;
} BML_ModDefinition;

typedef struct BML_ModCallbacks BML_ModCallbacks;

typedef struct BML_ModInterface {
    BML_InterfaceHeader Header;
    const BML_ModDefinition *Definition;
    const BML_ModCallbacks *Callbacks;
    int(BML_CDECL *Create)(BML_ModContextHandle context, void **outInstance);
    void(BML_CDECL *Destroy)(void *instance);
} BML_ModInterface;

#if !defined(BML_EXPORTS)
BML_MOD_ENTRY(int)
BML_GetModInterface(uint16_t majorVersion, const BML_ModInterface **outInterface);
#endif

typedef uint32_t BML_GameEvent;
typedef uint32_t BML_CommandEventPhase;

typedef struct BML_RenderEvent {
    size_t StructSize;
    uint32_t Flags;
} BML_RenderEvent;

typedef struct BML_CheatEvent {
    size_t StructSize;
    uint32_t Enabled;
} BML_CheatEvent;

typedef struct BML_LoadObjectEvent {
    size_t StructSize;
    const char *Filename;
    uint32_t IsMap;
    const char *MasterName;
    uint32_t FilterClass;
    uint32_t AddToScene;
    uint32_t ReuseMeshes;
    uint32_t ReuseMaterials;
    uint32_t Dynamic;
    const BML_ObjectRef *Objects;
    size_t ObjectCount;
    BML_ObjectRef MasterObject;
} BML_LoadObjectEvent;

typedef struct BML_LoadScriptEvent {
    size_t StructSize;
    const char *Filename;
    BML_ObjectRef Script;
} BML_LoadScriptEvent;

typedef struct BML_CommandEvent {
    size_t StructSize;
    BML_CommandEventPhase Phase;
    const char *Name;
    const char *InvokedAs;
    const char *const *Arguments;
    size_t ArgumentCount;
    uint32_t Cheat;
} BML_CommandEvent;

typedef struct BML_ConfigEvent {
    size_t StructSize;
    BML_ModHandle Mod;
    const char *ModId;
    const char *Category;
    const char *Key;
    uint32_t Type;
    BML_ConfigPropertyHandle Property;
} BML_ConfigEvent;

typedef struct BML_PhysicalizeEvent {
    size_t StructSize;
    BML_ObjectRef Target;
    uint32_t Fixed;
    float Friction;
    float Elasticity;
    float Mass;
    const char *CollisionGroup;
    uint32_t StartFrozen;
    uint32_t EnableCollision;
    uint32_t CalcMassCenter;
    float LinearDamp;
    float RotDamp;
    const char *CollisionSurface;
    BML_Vec3 MassCenter;
    const BML_ObjectRef *ConvexMeshes;
    size_t ConvexCount;
    const BML_Vec3 *SphereCenters;
    const float *SphereRadii;
    size_t SphereCount;
    const BML_ObjectRef *ConcaveMeshes;
    size_t ConcaveCount;
} BML_PhysicalizeEvent;

typedef struct BML_ObjectEvent {
    size_t StructSize;
    BML_ObjectRef Object;
} BML_ObjectEvent;

struct BML_ModCallbacks {
    size_t StructSize;
    int(BML_CDECL *OnLoad)(void *instance, BML_ModContextHandle context);
    int(BML_CDECL *OnUnload)(void *instance, BML_ModContextHandle context);
    int(BML_CDECL *OnProcess)(void *instance, BML_ModContextHandle context);
    int(BML_CDECL *OnRender)(void *instance, BML_ModContextHandle context,
                             const BML_RenderEvent *event);
    int(BML_CDECL *OnGameEvent)(void *instance, BML_ModContextHandle context, BML_GameEvent event);
    int(BML_CDECL *OnCheatEnabled)(void *instance, BML_ModContextHandle context,
                                   const BML_CheatEvent *event);
    int(BML_CDECL *OnLoadObject)(void *instance, BML_ModContextHandle context,
                                 const BML_LoadObjectEvent *event);
    int(BML_CDECL *OnLoadScript)(void *instance, BML_ModContextHandle context,
                                 const BML_LoadScriptEvent *event);
    int(BML_CDECL *OnCommandEvent)(void *instance, BML_ModContextHandle context,
                                   const BML_CommandEvent *event);
    int(BML_CDECL *OnModifyConfig)(void *instance, BML_ModContextHandle context,
                                   const BML_ConfigEvent *event);
    int(BML_CDECL *OnPhysicalize)(void *instance, BML_ModContextHandle context,
                                  const BML_PhysicalizeEvent *event);
    int(BML_CDECL *OnUnphysicalize)(void *instance, BML_ModContextHandle context,
                                    const BML_ObjectEvent *event);
};

typedef struct BML_ModInfo {
    size_t StructSize;
    BML_ModHandle Handle;
    uint32_t Kind;
    uint32_t State;
    const BML_ModDefinition *Definition;
    const char *Diagnostic;
} BML_ModInfo;

typedef int(BML_CDECL *BML_ModVisitor)(void *userData, const BML_ModInfo *info);

typedef struct BML_ModContextInterface {
    BML_InterfaceHeader Header;
    int(BML_CDECL *GetOwner)(BML_ModContextHandle context, BML_ModHandle *outMod);
    int(BML_CDECL *ReadMod)(BML_ModContextHandle context, BML_ModHandle mod, BML_ModVisitor visitor,
                            void *userData);
    int(BML_CDECL *FindMod)(BML_ModContextHandle context, const char *id, BML_ModHandle *outMod);
    int(BML_CDECL *VisitMods)(BML_ModContextHandle context, BML_ModVisitor visitor, void *userData);
    int(BML_CDECL *Log)(BML_ModContextHandle context, uint32_t level, const char *message,
                        size_t length);
    int(BML_CDECL *GetDirectory)(BML_ModContextHandle context, uint32_t directory, char *buffer,
                                 size_t capacity, size_t *required);
    int(BML_CDECL *GetResourcePath)(BML_ModContextHandle context, const char *relativePath,
                                    char *buffer, size_t capacity, size_t *required);
    int(BML_CDECL *ReadResource)(BML_ModContextHandle context, const char *relativePath,
                                 void *buffer, size_t capacity, size_t *required);
    int(BML_CDECL *ResourceExists)(BML_ModContextHandle context, const char *relativePath,
                                   uint32_t *outExists);
} BML_ModContextInterface;

#pragma pack(pop)
BML_END_CDECLS

#endif

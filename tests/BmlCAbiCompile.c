#include "BML/BML.h"
#include "BML/Behavior.h"
#include "BML/Gameplay.h"
#include "BML/Runtime.h"
#include "BML/Scene.h"
#include "BML/Speedrun.h"
#include "BML/UI.h"

#include <string.h>

#define BML_C_ABI_ASSERT(name, expression) typedef char name[(expression) ? 1 : -1]

BML_C_ABI_ASSERT(BmlBehaviorGuidSize, sizeof(BML_BehaviorGuid) == 8u);
BML_C_ABI_ASSERT(BmlBehaviorStatusSize, sizeof(BML_BehaviorStatus) == 296u);
BML_C_ABI_ASSERT(BmlBehaviorFrameSize, sizeof(BML_BehaviorRunFrame) == 64u);
BML_C_ABI_ASSERT(BmlBehaviorOutSize, sizeof(BML_BehaviorOutRecord) == 20u);
BML_C_ABI_ASSERT(BmlBehaviorPoutSize, sizeof(BML_BehaviorPoutRecord) == 40u);
BML_C_ABI_ASSERT(BmlBehaviorDiagnosticSize,
                 sizeof(BML_BehaviorDiagnosticRecord) == 44u);
BML_C_ABI_ASSERT(BmlBehaviorPrototypeRefSize,
                 sizeof(BML_BehaviorPrototypeRef) == 24u);
BML_C_ABI_ASSERT(BmlBehaviorManagerInfoSize,
                 sizeof(BML_BehaviorManagerInfo) == 16u);
BML_C_ABI_ASSERT(BmlBehaviorPrototypeInfoSize,
                 sizeof(BML_BehaviorPrototypeInfo) == 96u);
BML_C_ABI_ASSERT(BmlBehaviorSlotRecordSize,
                 sizeof(BML_BehaviorSlotRecord) == 48u);
BML_C_ABI_ASSERT(BmlBehaviorLayoutSize,
                 sizeof(BML_BehaviorLayout) == 128u);
BML_C_ABI_ASSERT(BmlBehaviorGraphPortSize,
                 sizeof(BML_BehaviorGraphPort) == 40u);
BML_C_ABI_ASSERT(BmlBehaviorGraphNodeSize,
                 sizeof(BML_BehaviorGraphNode) == 72u);
BML_C_ABI_ASSERT(BmlBehaviorGraphLinkSize,
                 sizeof(BML_BehaviorGraphLink) == 80u);
BML_C_ABI_ASSERT(BmlBehaviorGraphSize,
                 sizeof(BML_BehaviorGraph) == 56u);
BML_C_ABI_ASSERT(BmlBehaviorGraphValueSize,
                 sizeof(BML_BehaviorGraphValue) == 32u);

#if UINTPTR_MAX == UINT32_MAX
BML_C_ABI_ASSERT(BmlBehaviorSelectorSize, sizeof(BML_BehaviorSelector) == 24u);
BML_C_ABI_ASSERT(BmlBehaviorBindingSize, sizeof(BML_BehaviorBinding) == 108u);
BML_C_ABI_ASSERT(BmlBehaviorGenerationOffset,
                 offsetof(BML_BehaviorBlock, PrototypeGeneration) == 80u);
BML_C_ABI_ASSERT(BmlBehaviorBlockSize, sizeof(BML_BehaviorBlock) == 88u);
BML_C_ABI_ASSERT(BmlBehaviorPrototypeQuerySize,
                 sizeof(BML_BehaviorPrototypeQuery) == 64u);
BML_C_ABI_ASSERT(BmlBehaviorInterfaceSize, sizeof(BML_BehaviorInterface) == 84u);
BML_C_ABI_ASSERT(BmlBehaviorWatchValueSize,
                 sizeof(BML_BehaviorWatchValue) == 92u);
BML_C_ABI_ASSERT(BmlBehaviorWatchEventSize,
                 sizeof(BML_BehaviorWatchEvent) == 224u);
BML_C_ABI_ASSERT(BmlBehaviorWatchFunctionSize,
                 sizeof(BML_BehaviorWatchFunction) == 20u);
BML_C_ABI_ASSERT(BmlBehaviorWatchSpecSize,
                 sizeof(BML_BehaviorWatchSpec) == 68u);
#else
BML_C_ABI_ASSERT(BmlBehaviorSelectorSize, sizeof(BML_BehaviorSelector) == 32u);
BML_C_ABI_ASSERT(BmlBehaviorBindingSize, sizeof(BML_BehaviorBinding) == 120u);
BML_C_ABI_ASSERT(BmlBehaviorGenerationOffset,
                 offsetof(BML_BehaviorBlock, PrototypeGeneration) == 96u);
BML_C_ABI_ASSERT(BmlBehaviorBlockSize, sizeof(BML_BehaviorBlock) == 104u);
BML_C_ABI_ASSERT(BmlBehaviorPrototypeQuerySize,
                 sizeof(BML_BehaviorPrototypeQuery) == 96u);
BML_C_ABI_ASSERT(BmlBehaviorInterfaceSize, sizeof(BML_BehaviorInterface) == 168u);
BML_C_ABI_ASSERT(BmlBehaviorWatchValueSize,
                 sizeof(BML_BehaviorWatchValue) == 96u);
BML_C_ABI_ASSERT(BmlBehaviorWatchEventSize,
                 sizeof(BML_BehaviorWatchEvent) == 232u);
BML_C_ABI_ASSERT(BmlBehaviorWatchFunctionSize,
                 sizeof(BML_BehaviorWatchFunction) == 40u);
BML_C_ABI_ASSERT(BmlBehaviorWatchSpecSize,
                 sizeof(BML_BehaviorWatchSpec) == 80u);
#endif

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
        result == BML_ERROR_INVALID_PARAMETER || result == BML_ERROR_WRONG_THREAD ||
        result == BML_ERROR_BUSY)
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
// The gameplay collections are read as a count and then one row at a time, which
// is the shape a C caller has to walk without a std::vector to hand back.
int BML_TestCAbiGameplayInterface(void) {
    const void *found = NULL;
    const BML_GameplayInterface *gameplay = NULL;
    BML_GameplayLevelState level;
    BML_GameplayEnergyState energy;
    BML_GameplayCatalogEntry entry;
    size_t count = 0;
    size_t index = 0;

    if (BML_GetInterface(BML_GAMEPLAY_INTERFACE_ID, BML_GAMEPLAY_INTERFACE_MAJOR, &found) != BML_OK)
        return 0;
    gameplay = (const BML_GameplayInterface *) found;
    if (!BML_IFACE_HAS(gameplay, BML_GameplayInterface, ReadResetpoint))
        return 0;

    if (gameplay->ReadLevel(&level) != BML_OK || gameplay->ReadEnergy(&energy) != BML_OK)
        return 0;
    if (gameplay->ReadCatalogCount(&count) != BML_OK)
        return 0;
    for (index = 0; index < count; ++index) {
        if (gameplay->ReadCatalogEntry(index, &entry) != BML_OK)
            return 0;
        if (entry.FileLength < (int) strlen(entry.File))
            return 0;
    }
    return gameplay->ReadCatalogEntry(count, &entry) == BML_ERROR_NOT_FOUND;
}

// The scene interface is the one that writes text back, so this also checks the
// fixed-capacity Name buffer next to its NameLength from C: a name longer than the
// buffer is still terminated, and NameLength says how long it really was.
int BML_TestCAbiSceneInterface(const char *name) {
    const void *found = NULL;
    const BML_SceneInterface *scene = NULL;
    BML_ObjectRef reference;
    BML_SceneObjectInfo info;
    BML_SceneEntityTransform transform;

    if (BML_GetInterface(BML_SCENE_INTERFACE_ID, BML_SCENE_INTERFACE_MAJOR, &found) != BML_OK)
        return 0;
    scene = (const BML_SceneInterface *) found;
    if (!BML_IFACE_HAS(scene, BML_SceneInterface, FindObjectOfClass))
        return 0;

    reference.Domain = 0u;
    reference.Slot = 0u;
    reference.Generation = 0u;
    if (scene->FindObject(name, &reference) != BML_OK)
        return 0;
    if (reference.Domain == 0u)
        return 1;

    if (scene->ReadObject(reference, &info) != BML_OK)
        return 0;
    if (info.NameLength >= (int) BML_SCENE_NAME_CAPACITY)
        return strlen(info.Name) == BML_SCENE_NAME_CAPACITY - 1u;

    return scene->ReadEntityTransform(reference, &transform) != BML_ERROR_INVALID_PARAMETER &&
           scene->FindObjectOfClass(info.Name, info.ClassId, &reference) == BML_OK;
}

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

int BML_TestCAbiBehaviorInterface(BML_BehaviorRun run) {
    const void *found = NULL;
    const BML_BehaviorInterface *behavior = NULL;
    BML_BehaviorStatus status = {sizeof(status)};
    uint32_t frameCount = 0;
    uint32_t payloadSize = 0;

    if (BML_GetInterface(BML_BEHAVIOR_INTERFACE_ID,
                         BML_BEHAVIOR_INTERFACE_MAJOR, &found) != BML_OK)
        return 0;
    behavior = (const BML_BehaviorInterface *) found;
    if (!BML_IFACE_HAS(behavior, BML_BehaviorInterface, CloseRun))
        return 0;
    if (behavior->Header.MinorVersion >= 1 &&
        !BML_IFACE_HAS(behavior, BML_BehaviorInterface, ReadLiveLayout))
        return 0;
    return behavior->TakeFrames(run, NULL, 0, sizeof(BML_BehaviorRunFrame),
                                NULL, 0, &frameCount, &payloadSize,
                                &status) == BML_ERROR_BUFFER_TOO_SMALL;
}

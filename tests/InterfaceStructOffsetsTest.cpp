// Golden offsets for the current pre-release interface structs. Once an
// interface ships these become immutable; before then they keep accidental
// reordering visible while the contract is still being completed.
//
// Offsets are the x86 MSVC layout, the only platform the loader ships on.
#include "BML/Gameplay.h"
#include "BML/Behavior.h"
#include "BML/Interface.h"
#include "BML/IVP.h"
#include "BML/Runtime.h"
#include "BML/Scene.h"
#include "BML/Speedrun.h"
#include "BML/UI.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <gtest/gtest.h>

namespace {

#define EXPECT_GOLDEN_OFFSET(Interface, member, golden)                        \
    EXPECT_EQ(offsetof(Interface, member), static_cast<std::size_t>(golden))   \
        << #Interface "::" #member " moved"

// The header every interface starts with. Its layout is as frozen as the
// members that follow it.
TEST(InterfaceStructOffsets, InterfaceHeaderLayout) {
    EXPECT_GOLDEN_OFFSET(BML_InterfaceHeader, StructSize, 0);
    EXPECT_GOLDEN_OFFSET(BML_InterfaceHeader, MajorVersion, 4);
    EXPECT_GOLDEN_OFFSET(BML_InterfaceHeader, MinorVersion, 6);
    EXPECT_GOLDEN_OFFSET(BML_InterfaceHeader, InterfaceId, 8);
    EXPECT_EQ(sizeof(BML_InterfaceHeader), static_cast<std::size_t>(12));
}

// Growth rule: members may be appended (size grows, minor bumps), nothing
// earlier may move or shrink.
template <typename Interface>
void ExpectGrowthRules(const char *id, std::size_t shippedSize,
                       std::uint16_t shippedMinor, std::uint16_t currentMinor) {
    EXPECT_TRUE(std::is_standard_layout<Interface>::value) << id;
    EXPECT_GE(sizeof(Interface), shippedSize) << id << " shrank below its shipped size";
    if (sizeof(Interface) > shippedSize)
        EXPECT_GT(currentMinor, shippedMinor) << id << " grew without a minor bump";
}

TEST(InterfaceStructOffsets, RuntimeInterface) {
    EXPECT_GOLDEN_OFFSET(BML_RuntimeInterface, ReadState, 12);
    EXPECT_GOLDEN_OFFSET(BML_RuntimeInterface, ReadClock, 16);
    EXPECT_GOLDEN_OFFSET(BML_RuntimeInterface, ReadScore, 20);
    ExpectGrowthRules<BML_RuntimeInterface>("bml.runtime", 24, 0, BML_RUNTIME_INTERFACE_MINOR);
}

TEST(InterfaceStructOffsets, IvpInterface) {
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, ReadApiInfo, 12);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, GetManager, 16);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, GetEnvironment, 20);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, GetPhysicsObject, 24);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, GetRealObject, 28);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, GetCore, 32);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, GetMaterial, 36);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, ResolveSymbol, 40);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, ResolveRva, 44);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, GetSymbolCount, 48);
    EXPECT_GOLDEN_OFFSET(BML_IvpInterface, GetSymbol, 52);
    ExpectGrowthRules<BML_IvpInterface>("bml.ivp", 56, 0,
                                        BML_IVP_INTERFACE_MINOR);

    EXPECT_EQ(sizeof(BML_IvpApiInfo), static_cast<std::size_t>(92));
    EXPECT_EQ(sizeof(BML_IvpSymbol), static_cast<std::size_t>(12));
}

TEST(InterfaceStructOffsets, BehaviorInterface) {
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, OpenSession, 12);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, CloseSession, 16);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Call, 20);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Start, 24);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Spawn, 28);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Continue, 32);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Pulse, 36);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadRun, 40);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, TakeFrames, 44);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, CloseRun, 48);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, FindPrototypes, 52);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadDeclaredLayout, 56);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadLiveLayout, 60);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Inspect, 64);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadNodeLayout, 68);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadValue, 72);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Watch, 76);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, CloseWatch, 80);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, SubmitPlan, 84);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadPlan, 88);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ClosePlan, 92);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, InspectRun, 96);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Set, 100);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Bind, 104);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Configure, 108);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ApplyPatch, 112);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadPatch, 116);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ClosePatch, 120);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadWatch, 124);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Reference, 128);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ResolvePatchNode, 132);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, AttachBlock, 136);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, CreateScript, 140);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadScript, 144);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, SetScriptActive, 148);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, CloseScript, 152);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, SetPatchActive, 156);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReplacePatch, 160);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, SetPlanActive, 164);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReplacePlan, 168);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadPatchFailures, 172);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadPlanFailures, 176);
    ExpectGrowthRules<BML_BehaviorInterface>("bml.behavior", 180, 0,
                                             BML_BEHAVIOR_INTERFACE_MINOR);
    EXPECT_EQ(BML_BEHAVIOR_INTERFACE_MAJOR, 1u);
    EXPECT_EQ(BML_BEHAVIOR_INTERFACE_MINOR, 0u);
    EXPECT_EQ(BML_BEHAVIOR_INTERFACE_1_0_SIZE,
              sizeof(BML_BehaviorInterface));
}

TEST(InterfaceStructOffsets, BehaviorWireRecords) {
    EXPECT_GOLDEN_OFFSET(BML_BehaviorStatus, StructSize, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorStatus, Error, 4);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorStatus, Phase, 8);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorStatus, CkError, 12);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorStatus, NativeResult, 16);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorStatus, Prototype, 20);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorStatus, Type, 28);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorStatus, MessageLength, 36);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorStatus, Message, 40);
    EXPECT_EQ(sizeof(BML_BehaviorStatus), static_cast<std::size_t>(296));

    EXPECT_GOLDEN_OFFSET(BML_BehaviorFailures, StructSize, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorFailures, Apply, 4);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorFailures, Restore, 300);
    EXPECT_EQ(sizeof(BML_BehaviorFailures), static_cast<std::size_t>(596));

    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, StructSize, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, Sequence, 8);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, Frame, 16);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, NativeResult, 24);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, Continuation, 28);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, Error, 32);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, OutOffset, 36);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, OutCount, 40);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, PoutOffset, 44);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, PoutCount, 48);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, DiagnosticOffset, 52);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, DiagnosticCount, 56);
    EXPECT_EQ(sizeof(BML_BehaviorRunFrame), static_cast<std::size_t>(64));

    EXPECT_GOLDEN_OFFSET(BML_BehaviorPoutRecord, StructSize, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPoutRecord, Index, 4);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPoutRecord, Occurrence, 8);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPoutRecord, Type, 12);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPoutRecord, Kind, 20);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPoutRecord, NameOffset, 24);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPoutRecord, NameLength, 28);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPoutRecord, ValueOffset, 32);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPoutRecord, ValueSize, 36);
    EXPECT_EQ(sizeof(BML_BehaviorPoutRecord), static_cast<std::size_t>(40));

    EXPECT_GOLDEN_OFFSET(BML_BehaviorGraphValue, StructSize, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorGraphValue, State, 4);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorGraphValue, Relation, 8);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorGraphValue, Type, 12);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorGraphValue, Kind, 20);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorGraphValue, ValueOffset, 24);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorGraphValue, ValueSize, 28);
    EXPECT_EQ(sizeof(BML_BehaviorGraphValue), static_cast<std::size_t>(32));
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptSpec, StructSize, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptSpec, Owner, 4);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptSpec, Name, 16);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptSpec, Priority, 24);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptSpec, StepCount, 28);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptSpec, Steps, 32);
    EXPECT_EQ(sizeof(BML_BehaviorScriptSpec), static_cast<std::size_t>(36));
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptInfo, StructSize, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptInfo, State, 4);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptInfo, Active, 8);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptInfo, RequestedActive, 12);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptInfo, Root, 16);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptInfo, Owner, 28);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptInfo, Scene, 40);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptInfo, Priority, 52);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptInfo, Status, 56);
    EXPECT_EQ(sizeof(BML_BehaviorScriptInfo), static_cast<std::size_t>(352));
    EXPECT_GOLDEN_OFFSET(BML_BehaviorWatchInfo, StructSize, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorWatchInfo, State, 4);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorWatchInfo, Diagnostic, 8);
    EXPECT_EQ(sizeof(BML_BehaviorWatchInfo), static_cast<std::size_t>(304));
    EXPECT_EQ(BML_BEHAVIOR_VALUE_ALIGNMENT, 4u);
}

TEST(InterfaceStructOffsets, SceneInterface) {
    EXPECT_GOLDEN_OFFSET(BML_SceneInterface, ReadObject, 12);
    EXPECT_GOLDEN_OFFSET(BML_SceneInterface, ReadEntityTransform, 16);
    EXPECT_GOLDEN_OFFSET(BML_SceneInterface, FindObject, 20);
    EXPECT_GOLDEN_OFFSET(BML_SceneInterface, FindObjectOfClass, 24);
    ExpectGrowthRules<BML_SceneInterface>("bml.scene", 28, 0, BML_SCENE_INTERFACE_MINOR);
}

TEST(InterfaceStructOffsets, GameplayInterface) {
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadLevel, 12);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadEnergy, 16);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadCatalogCount, 20);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadCatalogEntry, 24);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadCheckpointCount, 28);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadCheckpoint, 32);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadResetpointCount, 36);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadResetpoint, 40);
    ExpectGrowthRules<BML_GameplayInterface>("bml.gameplay", 44, 0, BML_GAMEPLAY_INTERFACE_MINOR);
}

TEST(InterfaceStructOffsets, UIInterface) {
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, ReadHUDState, 12);
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, AddMessage, 16);
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, ClearMessages, 20);
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, OpenModsMenu, 24);
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, CloseModsMenu, 28);
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, OpenMapMenu, 32);
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, CloseMapMenu, 36);
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, SetHUDMode, 40);
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, ShowTitle, 44);
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, ShowFPS, 48);
    ExpectGrowthRules<BML_UIInterface>("bml.ui", 52, 0, BML_UI_INTERFACE_MINOR);
}

TEST(InterfaceStructOffsets, SpeedrunInterface) {
    EXPECT_GOLDEN_OFFSET(BML_SpeedrunInterface, ReadTimerState, 12);
    EXPECT_GOLDEN_OFFSET(BML_SpeedrunInterface, SetTimerVisible, 16);
    EXPECT_GOLDEN_OFFSET(BML_SpeedrunInterface, StartTimer, 20);
    EXPECT_GOLDEN_OFFSET(BML_SpeedrunInterface, PauseTimer, 24);
    EXPECT_GOLDEN_OFFSET(BML_SpeedrunInterface, ResetTimer, 28);
    ExpectGrowthRules<BML_SpeedrunInterface>("bml.speedrun", 32, 0, BML_SPEEDRUN_INTERFACE_MINOR);
}

} // namespace

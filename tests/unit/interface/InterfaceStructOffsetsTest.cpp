// Golden offsets for the current pre-release interface structs. Once an
// interface ships these become immutable; before then they keep accidental
// reordering visible while the interface is still being completed.
//
// Offsets are the x86 MSVC layout, the only platform the loader ships on.
#include "BML/Gameplay.h"
#include "BML/Behavior.h"
#include "BML/Command.h"
#include "BML/Interface.h"
#include "BML/ModMenu.h"
#include "BML/Time.h"
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

TEST(InterfaceStructOffsets, TimeInterface) {
    EXPECT_GOLDEN_OFFSET(BML_TimeInterface, ReadClock, 12);
    EXPECT_GOLDEN_OFFSET(BML_TimeClock, TimeMs, 0);
    EXPECT_GOLDEN_OFFSET(BML_TimeClock, AbsoluteMs, 4);
    EXPECT_GOLDEN_OFFSET(BML_TimeClock, DeltaMs, 8);
    EXPECT_GOLDEN_OFFSET(BML_TimeClock, MainTickCount, 12);
    EXPECT_EQ(sizeof(BML_TimeClock), static_cast<std::size_t>(16));
    EXPECT_EQ(sizeof(BML_TimeInterface), static_cast<std::size_t>(16));
    ExpectGrowthRules<BML_TimeInterface>("bml.time", 16, 0, BML_TIME_INTERFACE_MINOR);
}

TEST(InterfaceStructOffsets, CommandRecordsAndInterface) {
    EXPECT_GOLDEN_OFFSET(BML_CommandInvocation, StructSize, 0);
    EXPECT_GOLDEN_OFFSET(BML_CommandInvocation, Name, 4);
    EXPECT_GOLDEN_OFFSET(BML_CommandInvocation, InvokedAs, 8);
    EXPECT_GOLDEN_OFFSET(BML_CommandInvocation, ArgumentCount, 12);
    EXPECT_GOLDEN_OFFSET(BML_CommandInvocation, Arguments, 16);
    EXPECT_GOLDEN_OFFSET(BML_CommandInvocation, Input, 20);
    EXPECT_GOLDEN_OFFSET(BML_CommandInvocation, InputLength, 24);
    EXPECT_GOLDEN_OFFSET(BML_CommandInvocation, OutputContext, 28);
    EXPECT_GOLDEN_OFFSET(BML_CommandInvocation, Write, 32);
    EXPECT_EQ(sizeof(BML_CommandInvocation), static_cast<std::size_t>(36));

    EXPECT_GOLDEN_OFFSET(BML_CommandCompletionRequest, StructSize, 0);
    EXPECT_GOLDEN_OFFSET(BML_CommandCompletionRequest, Name, 4);
    EXPECT_GOLDEN_OFFSET(BML_CommandCompletionRequest, InvokedAs, 8);
    EXPECT_GOLDEN_OFFSET(BML_CommandCompletionRequest, ArgumentCount, 12);
    EXPECT_GOLDEN_OFFSET(BML_CommandCompletionRequest, Arguments, 16);
    EXPECT_GOLDEN_OFFSET(BML_CommandCompletionRequest, ActiveArgument, 20);
    EXPECT_GOLDEN_OFFSET(BML_CommandCompletionRequest, Prefix, 24);
    EXPECT_EQ(sizeof(BML_CommandCompletionRequest), static_cast<std::size_t>(28));

    EXPECT_GOLDEN_OFFSET(BML_CommandCompletion, StructSize, 0);
    EXPECT_GOLDEN_OFFSET(BML_CommandCompletion, Context, 4);
    EXPECT_GOLDEN_OFFSET(BML_CommandCompletion, Add, 8);
    EXPECT_EQ(sizeof(BML_CommandCompletion), static_cast<std::size_t>(12));

    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, StructSize, 0);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, Name, 4);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, Alias, 8);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, Description, 12);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, Usage, 16);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, Category, 20);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, Flags, 24);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, UserData, 28);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, Execute, 32);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, Complete, 36);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, Release, 40);
    EXPECT_EQ(sizeof(BML_CommandDefinition), static_cast<std::size_t>(44));

    EXPECT_GOLDEN_OFFSET(BML_CommandInfo, StructSize, 0);
    EXPECT_GOLDEN_OFFSET(BML_CommandInfo, Handle, 8);
    EXPECT_GOLDEN_OFFSET(BML_CommandInfo, Name, 16);
    EXPECT_GOLDEN_OFFSET(BML_CommandInfo, Alias, 20);
    EXPECT_GOLDEN_OFFSET(BML_CommandInfo, Description, 24);
    EXPECT_GOLDEN_OFFSET(BML_CommandInfo, Usage, 28);
    EXPECT_GOLDEN_OFFSET(BML_CommandInfo, Category, 32);
    EXPECT_GOLDEN_OFFSET(BML_CommandInfo, Flags, 36);
    EXPECT_EQ(sizeof(BML_CommandInfo), static_cast<std::size_t>(40));

    EXPECT_GOLDEN_OFFSET(BML_CommandInterface, Register, 12);
    EXPECT_GOLDEN_OFFSET(BML_CommandInterface, Unregister, 16);
    EXPECT_GOLDEN_OFFSET(BML_CommandInterface, SetEnabled, 20);
    EXPECT_GOLDEN_OFFSET(BML_CommandInterface, Visit, 24);
    EXPECT_GOLDEN_OFFSET(BML_CommandInterface, Find, 28);
    EXPECT_GOLDEN_OFFSET(BML_CommandInterface, ExecuteLine, 32);
    EXPECT_EQ(sizeof(BML_CommandInterface), static_cast<std::size_t>(36));
    ExpectGrowthRules<BML_CommandInterface>("bml.command", 36, 0,
                                            BML_COMMAND_INTERFACE_MINOR);
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
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadPlanInstances, 180);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface,
                         ResolvePlanInstanceNode, 184);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadPatchValue, 188);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, WritePatchValue, 192);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface,
                         ReadPlanInstanceValue, 196);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface,
                         WritePlanInstanceValue, 200);
    ExpectGrowthRules<BML_BehaviorInterface>("bml.behavior", 204, 0,
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
    EXPECT_GOLDEN_OFFSET(BML_BehaviorNodeRef, StructSize, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorNodeRef, Graph, 4);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorNodeRef, Handle, 8);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorNodeRef, Binding, 16);
    EXPECT_EQ(sizeof(BML_BehaviorNodeRef), static_cast<std::size_t>(24));
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPlanInstance, StructSize, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPlanInstance, Rule, 4);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPlanInstance, PlanRevision, 8);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPlanInstance, Binding, 16);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPlanInstance, World, 24);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPlanInstance, Revision, 32);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPlanInstance, Identity, 40);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPlanInstance, Script, 48);
    EXPECT_EQ(sizeof(BML_BehaviorPlanInstance),
              static_cast<std::size_t>(64));
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
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadHighScore, 44);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadCheatEnabled, 48);
    EXPECT_EQ(sizeof(BML_GameplayInterface), static_cast<std::size_t>(52));
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

TEST(InterfaceStructOffsets, ModMenuPageAndInterface) {
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPageFrame, StructSize, 0);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPageFrame, Action, 4);
    EXPECT_EQ(sizeof(BML_ModMenuPageFrame), static_cast<std::size_t>(8));

    EXPECT_GOLDEN_OFFSET(BML_ModMenuPage, StructSize, 0);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPage, Id, 4);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPage, Label, 8);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPage, Description, 12);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPage, UserData, 16);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPage, Draw, 20);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPage, Enter, 24);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPage, Leave, 28);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPage, Release, 32);
    EXPECT_EQ(sizeof(BML_ModMenuPage), static_cast<std::size_t>(36));

    EXPECT_GOLDEN_OFFSET(BML_ModMenuInterface, RegisterPage, 12);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuInterface, UnregisterPage, 16);
    ExpectGrowthRules<BML_ModMenuInterface>("bml.mod-menu", 20, 0,
                                            BML_MOD_MENU_INTERFACE_MINOR);
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

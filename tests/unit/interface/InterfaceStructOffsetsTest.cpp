// Golden offsets for the current pre-release interface structs. Once an
// interface ships these become immutable; before then they keep accidental
// reordering visible while the interface is still being completed.
//
// Keep independent goldens for the Windows x86 ABI and the 64-bit SDL ABIs.
// Fixed-width payload records retain their layout on both word sizes.
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

constexpr std::size_t AbiSize(std::size_t x86, std::size_t wide) {
    static_assert(sizeof(void *) == 4 || sizeof(void *) == 8);
    return sizeof(void *) == 8 ? wide : x86;
}

#define EXPECT_GOLDEN_OFFSET(Interface, member, x86, wide)                     \
    EXPECT_EQ(offsetof(Interface, member), AbiSize(x86, wide))                 \
        << #Interface "::" #member " moved"

// The header every interface starts with. Its layout is as frozen as the
// members that follow it.
TEST(InterfaceStructOffsets, InterfaceHeaderLayout) {
    EXPECT_GOLDEN_OFFSET(BML_InterfaceHeader, StructSize, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_InterfaceHeader, MajorVersion, 4, 8);
    EXPECT_GOLDEN_OFFSET(BML_InterfaceHeader, MinorVersion, 6, 10);
    EXPECT_GOLDEN_OFFSET(BML_InterfaceHeader, InterfaceId, 8, 16);
    EXPECT_EQ(sizeof(BML_InterfaceHeader), AbiSize(12, 24));
}

// Growth rule: members may be appended (size grows, minor bumps), nothing
// earlier may move or shrink.
template <typename Interface>
void ExpectGrowthRules(const char *id, std::size_t shippedSize32, std::size_t shippedSize64,
                       std::uint16_t shippedMinor, std::uint16_t currentMinor) {
    const std::size_t shippedSize = AbiSize(shippedSize32, shippedSize64);
    EXPECT_TRUE(std::is_standard_layout<Interface>::value) << id;
    EXPECT_GE(sizeof(Interface), shippedSize) << id << " shrank below its shipped size";
    if (sizeof(Interface) > shippedSize)
        EXPECT_GT(currentMinor, shippedMinor) << id << " grew without a minor bump";
}

TEST(InterfaceStructOffsets, TimeInterface) {
    EXPECT_GOLDEN_OFFSET(BML_TimeInterface, ReadClock, 12, 24);
    EXPECT_GOLDEN_OFFSET(BML_TimeClock, TimeMs, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_TimeClock, AbsoluteMs, 4, 4);
    EXPECT_GOLDEN_OFFSET(BML_TimeClock, DeltaMs, 8, 8);
    EXPECT_GOLDEN_OFFSET(BML_TimeClock, MainTickCount, 12, 12);
    EXPECT_EQ(sizeof(BML_TimeClock), AbiSize(16, 16));
    EXPECT_EQ(sizeof(BML_TimeInterface), AbiSize(16, 32));
    ExpectGrowthRules<BML_TimeInterface>("bml.time", 16, 32, 0, BML_TIME_INTERFACE_MINOR);
}

TEST(InterfaceStructOffsets, CommandRecordsAndInterface) {
    EXPECT_GOLDEN_OFFSET(BML_CommandInvocation, StructSize, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_CommandInvocation, Name, 4, 8);
    EXPECT_GOLDEN_OFFSET(BML_CommandInvocation, InvokedAs, 8, 16);
    EXPECT_GOLDEN_OFFSET(BML_CommandInvocation, ArgumentCount, 12, 24);
    EXPECT_GOLDEN_OFFSET(BML_CommandInvocation, Arguments, 16, 32);
    EXPECT_GOLDEN_OFFSET(BML_CommandInvocation, Input, 20, 40);
    EXPECT_GOLDEN_OFFSET(BML_CommandInvocation, InputLength, 24, 48);
    EXPECT_GOLDEN_OFFSET(BML_CommandInvocation, OutputContext, 28, 56);
    EXPECT_GOLDEN_OFFSET(BML_CommandInvocation, Write, 32, 64);
    EXPECT_EQ(sizeof(BML_CommandInvocation), AbiSize(36, 72));

    EXPECT_GOLDEN_OFFSET(BML_CommandCompletionRequest, StructSize, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_CommandCompletionRequest, Name, 4, 8);
    EXPECT_GOLDEN_OFFSET(BML_CommandCompletionRequest, InvokedAs, 8, 16);
    EXPECT_GOLDEN_OFFSET(BML_CommandCompletionRequest, ArgumentCount, 12, 24);
    EXPECT_GOLDEN_OFFSET(BML_CommandCompletionRequest, Arguments, 16, 32);
    EXPECT_GOLDEN_OFFSET(BML_CommandCompletionRequest, ActiveArgument, 20, 40);
    EXPECT_GOLDEN_OFFSET(BML_CommandCompletionRequest, Prefix, 24, 48);
    EXPECT_EQ(sizeof(BML_CommandCompletionRequest), AbiSize(28, 56));

    EXPECT_GOLDEN_OFFSET(BML_CommandCompletion, StructSize, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_CommandCompletion, Context, 4, 8);
    EXPECT_GOLDEN_OFFSET(BML_CommandCompletion, Add, 8, 16);
    EXPECT_EQ(sizeof(BML_CommandCompletion), AbiSize(12, 24));

    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, StructSize, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, Name, 4, 8);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, Alias, 8, 16);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, Description, 12, 24);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, Usage, 16, 32);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, Category, 20, 40);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, Flags, 24, 48);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, UserData, 28, 56);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, Execute, 32, 64);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, Complete, 36, 72);
    EXPECT_GOLDEN_OFFSET(BML_CommandDefinition, Release, 40, 80);
    EXPECT_EQ(sizeof(BML_CommandDefinition), AbiSize(44, 88));

    EXPECT_GOLDEN_OFFSET(BML_CommandInfo, StructSize, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_CommandInfo, Handle, 8, 8);
    EXPECT_GOLDEN_OFFSET(BML_CommandInfo, Name, 16, 16);
    EXPECT_GOLDEN_OFFSET(BML_CommandInfo, Alias, 20, 24);
    EXPECT_GOLDEN_OFFSET(BML_CommandInfo, Description, 24, 32);
    EXPECT_GOLDEN_OFFSET(BML_CommandInfo, Usage, 28, 40);
    EXPECT_GOLDEN_OFFSET(BML_CommandInfo, Category, 32, 48);
    EXPECT_GOLDEN_OFFSET(BML_CommandInfo, Flags, 36, 56);
    EXPECT_EQ(sizeof(BML_CommandInfo), AbiSize(40, 64));

    EXPECT_GOLDEN_OFFSET(BML_CommandInterface, Register, 12, 24);
    EXPECT_GOLDEN_OFFSET(BML_CommandInterface, Unregister, 16, 32);
    EXPECT_GOLDEN_OFFSET(BML_CommandInterface, SetEnabled, 20, 40);
    EXPECT_GOLDEN_OFFSET(BML_CommandInterface, Visit, 24, 48);
    EXPECT_GOLDEN_OFFSET(BML_CommandInterface, Find, 28, 56);
    EXPECT_GOLDEN_OFFSET(BML_CommandInterface, ExecuteLine, 32, 64);
    EXPECT_EQ(sizeof(BML_CommandInterface), AbiSize(36, 72));
    ExpectGrowthRules<BML_CommandInterface>("bml.command", 36, 72, 0,
                                            BML_COMMAND_INTERFACE_MINOR);
}

TEST(InterfaceStructOffsets, BehaviorInterface) {
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, OpenSession, 12, 24);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, CloseSession, 16, 32);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Call, 20, 40);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Start, 24, 48);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Spawn, 28, 56);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Continue, 32, 64);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Pulse, 36, 72);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadRun, 40, 80);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, TakeFrames, 44, 88);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, CloseRun, 48, 96);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, FindPrototypes, 52, 104);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadDeclaredLayout, 56, 112);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadLiveLayout, 60, 120);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Inspect, 64, 128);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadNodeLayout, 68, 136);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadValue, 72, 144);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Watch, 76, 152);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, CloseWatch, 80, 160);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, SubmitPlan, 84, 168);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadPlan, 88, 176);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ClosePlan, 92, 184);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, InspectRun, 96, 192);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Set, 100, 200);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Bind, 104, 208);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Configure, 108, 216);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ApplyPatch, 112, 224);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadPatch, 116, 232);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ClosePatch, 120, 240);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadWatch, 124, 248);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, Reference, 128, 256);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ResolvePatchNode, 132, 264);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, AttachBlock, 136, 272);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, CreateScript, 140, 280);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadScript, 144, 288);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, SetScriptActive, 148, 296);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, CloseScript, 152, 304);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, SetPatchActive, 156, 312);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReplacePatch, 160, 320);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, SetPlanActive, 164, 328);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReplacePlan, 168, 336);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadPatchFailures, 172, 344);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadPlanFailures, 176, 352);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadPlanInstances, 180, 360);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ResolvePlanInstanceNode, 184, 368);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadPatchValue, 188, 376);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, WritePatchValue, 192, 384);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ReadPlanInstanceValue, 196, 392);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, WritePlanInstanceValue, 200, 400);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, ValidateBlock, 204, 408);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorInterface, AttachHook, 208, 416);
    ExpectGrowthRules<BML_BehaviorInterface>("bml.behavior", 212, 424, 0,
                                             BML_BEHAVIOR_INTERFACE_MINOR);
    EXPECT_EQ(BML_BEHAVIOR_INTERFACE_MAJOR, 1u);
    EXPECT_EQ(BML_BEHAVIOR_INTERFACE_MINOR, 0u);
    EXPECT_EQ(BML_BEHAVIOR_INTERFACE_1_0_SIZE,
              sizeof(BML_BehaviorInterface));
}

TEST(InterfaceStructOffsets, BehaviorWireRecords) {
    EXPECT_GOLDEN_OFFSET(BML_BehaviorStatus, StructSize, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorStatus, Error, 4, 4);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorStatus, Phase, 8, 8);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorStatus, CkError, 12, 12);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorStatus, NativeResult, 16, 16);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorStatus, Prototype, 20, 20);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorStatus, Type, 28, 28);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorStatus, MessageLength, 36, 36);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorStatus, Message, 40, 40);
    EXPECT_EQ(sizeof(BML_BehaviorStatus), AbiSize(296, 296));

    EXPECT_GOLDEN_OFFSET(BML_BehaviorFailures, StructSize, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorFailures, Apply, 4, 4);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorFailures, Restore, 300, 300);
    EXPECT_EQ(sizeof(BML_BehaviorFailures), AbiSize(596, 596));

    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, StructSize, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, Sequence, 8, 8);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, Frame, 16, 16);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, NativeResult, 24, 24);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, Continuation, 28, 28);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, Error, 32, 32);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, OutOffset, 36, 36);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, OutCount, 40, 40);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, PoutOffset, 44, 44);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, PoutCount, 48, 48);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, DiagnosticOffset, 52, 52);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorRunFrame, DiagnosticCount, 56, 56);
    EXPECT_EQ(sizeof(BML_BehaviorRunFrame), AbiSize(64, 64));

    EXPECT_GOLDEN_OFFSET(BML_BehaviorPoutRecord, StructSize, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPoutRecord, Index, 4, 4);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPoutRecord, Occurrence, 8, 8);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPoutRecord, Type, 12, 12);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPoutRecord, Kind, 20, 20);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPoutRecord, NameOffset, 24, 24);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPoutRecord, NameLength, 28, 28);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPoutRecord, ValueOffset, 32, 32);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPoutRecord, ValueSize, 36, 36);
    EXPECT_EQ(sizeof(BML_BehaviorPoutRecord), AbiSize(40, 40));

    EXPECT_GOLDEN_OFFSET(BML_BehaviorGraphValue, StructSize, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorGraphValue, State, 4, 4);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorGraphValue, Relation, 8, 8);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorGraphValue, Type, 12, 12);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorGraphValue, Kind, 20, 20);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorGraphValue, ValueOffset, 24, 24);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorGraphValue, ValueSize, 28, 28);
    EXPECT_EQ(sizeof(BML_BehaviorGraphValue), AbiSize(32, 32));
    EXPECT_GOLDEN_OFFSET(BML_BehaviorNodeRef, StructSize, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorNodeRef, Graph, 4, 4);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorNodeRef, Handle, 8, 8);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorNodeRef, Binding, 16, 16);
    EXPECT_EQ(sizeof(BML_BehaviorNodeRef), AbiSize(24, 24));
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPlanInstance, StructSize, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPlanInstance, Rule, 4, 4);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPlanInstance, PlanRevision, 8, 8);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPlanInstance, Binding, 16, 16);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPlanInstance, World, 24, 24);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPlanInstance, Revision, 32, 32);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPlanInstance, Identity, 40, 40);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorPlanInstance, Script, 48, 48);
    EXPECT_EQ(sizeof(BML_BehaviorPlanInstance), AbiSize(64, 64));
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptSpec, StructSize, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptSpec, Owner, 4, 4);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptSpec, Name, 16, 16);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptSpec, Priority, 24, 32);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptSpec, Program, 28, 40);
    EXPECT_EQ(sizeof(BML_BehaviorScriptSpec), AbiSize(104, 192));
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptInfo, StructSize, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptInfo, State, 4, 4);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptInfo, Active, 8, 8);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptInfo, RequestedActive, 12, 12);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptInfo, Root, 16, 16);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptInfo, Owner, 28, 28);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptInfo, Scene, 40, 40);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptInfo, Priority, 52, 52);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorScriptInfo, Status, 56, 56);
    EXPECT_EQ(sizeof(BML_BehaviorScriptInfo), AbiSize(352, 352));
    EXPECT_GOLDEN_OFFSET(BML_BehaviorWatchInfo, StructSize, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorWatchInfo, State, 4, 4);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorWatchInfo, Diagnostic, 8, 8);
    EXPECT_EQ(sizeof(BML_BehaviorWatchInfo), AbiSize(304, 304));
    EXPECT_GOLDEN_OFFSET(BML_BehaviorHookBlock, StructSize, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorHookBlock, Inputs, 4, 4);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorHookBlock, Outputs, 8, 8);
    EXPECT_GOLDEN_OFFSET(BML_BehaviorHookBlock, Flags, 12, 12);
    EXPECT_EQ(sizeof(BML_BehaviorHookBlock), AbiSize(16, 16));
    EXPECT_EQ(BML_BEHAVIOR_VALUE_ALIGNMENT, 4u);
}

TEST(InterfaceStructOffsets, SceneInterface) {
    EXPECT_GOLDEN_OFFSET(BML_SceneInterface, ReadObject, 12, 24);
    EXPECT_GOLDEN_OFFSET(BML_SceneInterface, ReadEntityTransform, 16, 32);
    EXPECT_GOLDEN_OFFSET(BML_SceneInterface, FindObject, 20, 40);
    EXPECT_GOLDEN_OFFSET(BML_SceneInterface, FindObjectOfClass, 24, 48);
    ExpectGrowthRules<BML_SceneInterface>("bml.scene", 28, 56, 0, BML_SCENE_INTERFACE_MINOR);
}

TEST(InterfaceStructOffsets, GameplayInterface) {
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadLevel, 12, 24);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadEnergy, 16, 32);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadCatalogCount, 20, 40);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadCatalogEntry, 24, 48);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadCheckpointCount, 28, 56);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadCheckpoint, 32, 64);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadResetpointCount, 36, 72);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadResetpoint, 40, 80);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadHighScore, 44, 88);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadCheatEnabled, 48, 96);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, RegisterBallType, 52, 104);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, RegisterFloorType, 56, 112);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, RegisterModuleBall, 60, 120);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, RegisterModuleConvex, 64, 128);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, RegisterTrafo, 68, 136);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, RegisterModule, 72, 144);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ReadState, 76, 152);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, EnableCheat, 80, 160);
    EXPECT_GOLDEN_OFFSET(BML_GameplayInterface, ExitGame, 84, 168);
    EXPECT_EQ(sizeof(BML_GameplayInterface), AbiSize(88, 176));
    ExpectGrowthRules<BML_GameplayInterface>("bml.gameplay", 44, 88, 0, BML_GAMEPLAY_INTERFACE_MINOR);
}

TEST(InterfaceStructOffsets, UIInterface) {
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, ReadHUDState, 12, 24);
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, AddMessage, 16, 32);
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, ClearMessages, 20, 40);
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, OpenModsMenu, 24, 48);
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, CloseModsMenu, 28, 56);
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, OpenMapMenu, 32, 64);
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, CloseMapMenu, 36, 72);
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, SetHUDMode, 40, 80);
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, ShowTitle, 44, 88);
    EXPECT_GOLDEN_OFFSET(BML_UIInterface, ShowFPS, 48, 96);
    ExpectGrowthRules<BML_UIInterface>("bml.ui", 52, 104, 0, BML_UI_INTERFACE_MINOR);
}

TEST(InterfaceStructOffsets, ModMenuPageAndInterface) {
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPageFrame, StructSize, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPageFrame, Action, 4, 8);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPageFrame, TargetPageId, 8, 12);
    EXPECT_EQ(sizeof(BML_ModMenuPageFrame), AbiSize(264, 272));

    EXPECT_GOLDEN_OFFSET(BML_ModMenuPage, StructSize, 0, 0);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPage, Id, 4, 8);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPage, Label, 8, 16);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPage, Description, 12, 24);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPage, UserData, 16, 32);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPage, Draw, 20, 40);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPage, Enter, 24, 48);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPage, Leave, 28, 56);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPage, Release, 32, 64);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuPage, Flags, 36, 72);
    EXPECT_EQ(sizeof(BML_ModMenuPage), AbiSize(40, 80));

    EXPECT_GOLDEN_OFFSET(BML_ModMenuInterface, RegisterPage, 12, 24);
    EXPECT_GOLDEN_OFFSET(BML_ModMenuInterface, UnregisterPage, 16, 32);
    ExpectGrowthRules<BML_ModMenuInterface>("bml.mod-menu", 20, 40, 0,
                                            BML_MOD_MENU_INTERFACE_MINOR);
}

TEST(InterfaceStructOffsets, SpeedrunInterface) {
    EXPECT_GOLDEN_OFFSET(BML_SpeedrunInterface, ReadTimerState, 12, 24);
    EXPECT_GOLDEN_OFFSET(BML_SpeedrunInterface, SetTimerVisible, 16, 32);
    EXPECT_GOLDEN_OFFSET(BML_SpeedrunInterface, StartTimer, 20, 40);
    EXPECT_GOLDEN_OFFSET(BML_SpeedrunInterface, PauseTimer, 24, 48);
    EXPECT_GOLDEN_OFFSET(BML_SpeedrunInterface, ResetTimer, 28, 56);
    ExpectGrowthRules<BML_SpeedrunInterface>("bml.speedrun", 32, 64, 0, BML_SPEEDRUN_INTERFACE_MINOR);
}

} // namespace

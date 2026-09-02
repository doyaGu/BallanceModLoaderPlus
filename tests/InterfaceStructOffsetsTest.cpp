// Golden offsets for every published interface-struct member, recorded when the
// member first shipped (every interface is at minor 0 today, so the values below
// are the shipping layout). The constants never change: a failing line means a
// member moved or was inserted before an existing one, which the rules in
// BML/Interface.h forbid -- restore the layout, or ship a new major and a new id.
// Appending a member needs no edit here: earlier offsets do not move, and the
// size checks below verify growth is paired with a minor bump.
//
// Offsets are the x86 MSVC layout, the only platform the loader ships on.
#include "BML/Gameplay.h"
#include "BML/Behavior.h"
#include "BML/Interface.h"
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
    ExpectGrowthRules<BML_BehaviorInterface>("bml.behavior", 52, 0,
                                             BML_BEHAVIOR_INTERFACE_MINOR);
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

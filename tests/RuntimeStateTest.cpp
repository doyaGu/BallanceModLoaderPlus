#include <gtest/gtest.h>

#include "RuntimeState.h"

namespace {

void ExpectState(const BML::RuntimeState &state,
                 bool inGame,
                 bool inLevel,
                 bool paused,
                 bool playing,
                 bool cheatEnabled = false) {
    const BML::RuntimeStateSnapshot snapshot = state.Read();
    EXPECT_EQ(inGame, snapshot.InGame);
    EXPECT_EQ(inLevel, snapshot.InLevel);
    EXPECT_EQ(paused, snapshot.Paused);
    EXPECT_EQ(playing, snapshot.Playing);
    EXPECT_EQ(cheatEnabled, snapshot.CheatEnabled);
}

TEST(RuntimeStateTest, InitialStateIsInactive) {
    const BML::RuntimeState state;

    ExpectState(state, false, false, false, false);
}

TEST(RuntimeStateTest, EnterLevelAndPausePreserveDerivedSemantics) {
    BML::RuntimeState state;

    state.Apply(BML::RuntimeStateTransition::EnterLevel);
    ExpectState(state, true, true, false, true);

    state.Apply(BML::RuntimeStateTransition::Pause);
    ExpectState(state, true, false, true, false);

    state.Apply(BML::RuntimeStateTransition::Resume);
    ExpectState(state, true, true, false, true);
}

TEST(RuntimeStateTest, LeaveLevelKeepsGamePlaying) {
    BML::RuntimeState state;
    state.Apply(BML::RuntimeStateTransition::EnterLevel);

    state.Apply(BML::RuntimeStateTransition::LeaveLevel);

    ExpectState(state, true, false, false, true);
}

TEST(RuntimeStateTest, LeaveGamePreservesPausedState) {
    BML::RuntimeState state;
    state.Apply(BML::RuntimeStateTransition::EnterLevel);
    state.Apply(BML::RuntimeStateTransition::Pause);

    state.Apply(BML::RuntimeStateTransition::LeaveGame);

    ExpectState(state, false, false, true, false);
}

TEST(RuntimeStateTest, EnterLevelClearsPausedStateFromPreviousGame) {
    BML::RuntimeState state;
    state.Apply(BML::RuntimeStateTransition::Pause);
    state.Apply(BML::RuntimeStateTransition::LeaveGame);

    state.Apply(BML::RuntimeStateTransition::EnterLevel);

    ExpectState(state, true, true, false, true);
}

TEST(RuntimeStateTest, CheatUpdatesAreIdempotentAndIndependent) {
    BML::RuntimeState state;
    state.Apply(BML::RuntimeStateTransition::EnterLevel);

    EXPECT_TRUE(state.SetCheatEnabled(true));
    EXPECT_FALSE(state.SetCheatEnabled(true));
    ExpectState(state, true, true, false, true, true);

    EXPECT_TRUE(state.SetCheatEnabled(false));
    EXPECT_FALSE(state.SetCheatEnabled(false));
    ExpectState(state, true, true, false, true, false);
}

TEST(RuntimeStateTest, RepeatedTransitionsAreStable) {
    BML::RuntimeState state;

    state.Apply(BML::RuntimeStateTransition::EnterLevel);
    state.Apply(BML::RuntimeStateTransition::EnterLevel);
    ExpectState(state, true, true, false, true);

    state.Apply(BML::RuntimeStateTransition::LeaveLevel);
    state.Apply(BML::RuntimeStateTransition::LeaveLevel);
    ExpectState(state, true, false, false, true);

    state.Apply(BML::RuntimeStateTransition::LeaveGame);
    state.Apply(BML::RuntimeStateTransition::LeaveGame);
    ExpectState(state, false, false, false, false);
}

} // namespace

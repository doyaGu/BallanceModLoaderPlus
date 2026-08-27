#include <gtest/gtest.h>

#include "GameSession.h"

namespace {

void ExpectSession(const BML::GameSession &session,
                   BML::GamePhase phase,
                   bool inGame,
                   bool inLevel,
                   bool paused,
                   bool playing) {
    const BML::GameSessionSnapshot snapshot = session.Read();
    EXPECT_EQ(phase, snapshot.Phase);
    EXPECT_EQ(inGame, snapshot.IsInGame());
    EXPECT_EQ(inLevel, snapshot.IsInLevel());
    EXPECT_EQ(paused, snapshot.IsPaused());
    EXPECT_EQ(playing, snapshot.IsPlaying());
}

TEST(GameSessionTest, InitialPhaseIsFrontEnd) {
    const BML::GameSession session;

    ExpectSession(session, BML::GamePhase::FrontEnd, false, false, false, false);
}

TEST(GameSessionTest, ActiveAndPausedPhasesHaveConsistentProjections) {
    BML::GameSession session;

    session.ActivateLevel();
    ExpectSession(session, BML::GamePhase::LevelActive, true, true, false, true);

    session.PauseLevel();
    ExpectSession(session, BML::GamePhase::LevelPaused, true, true, true, false);

    session.ResumeLevel();
    ExpectSession(session, BML::GamePhase::LevelActive, true, true, false, true);
}

TEST(GameSessionTest, TransitionIsInGameButNotInLevelOrPlaying) {
    BML::GameSession session;
    session.ActivateLevel();

    session.BeginTransition();

    ExpectSession(session, BML::GamePhase::Transitioning, true, false, false, false);
}

TEST(GameSessionTest, ReturningToFrontEndClearsLevelAndPauseState) {
    BML::GameSession session;
    session.ActivateLevel();
    session.PauseLevel();

    session.ReturnToFrontEnd();

    ExpectSession(session, BML::GamePhase::FrontEnd, false, false, false, false);
}

TEST(GameSessionTest, PauseAndResumeDoNotFabricateAnActiveLevel) {
    BML::GameSession session;

    session.PauseLevel();
    ExpectSession(session, BML::GamePhase::FrontEnd, false, false, false, false);

    session.BeginTransition();
    session.ResumeLevel();
    ExpectSession(session, BML::GamePhase::Transitioning, true, false, false, false);
}

TEST(GameSessionTest, RepeatedChangesAreStable) {
    BML::GameSession session;

    session.ActivateLevel();
    session.ActivateLevel();
    ExpectSession(session, BML::GamePhase::LevelActive, true, true, false, true);

    session.BeginTransition();
    session.BeginTransition();
    ExpectSession(session, BML::GamePhase::Transitioning, true, false, false, false);

    session.ReturnToFrontEnd();
    session.ReturnToFrontEnd();
    ExpectSession(session, BML::GamePhase::FrontEnd, false, false, false, false);
}

} // namespace

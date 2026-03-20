#include <gtest/gtest.h>

#include "../modules/BML_Console/src/ConsoleCompletionState.h"

namespace {

TEST(ConsoleCompletionStateTests, CompletionTriggerStartsByRebuildingCandidates) {
    EXPECT_EQ(BML::Console::CompletionTriggerAction::RebuildCandidates,
        BML::Console::GetCompletionTriggerAction(false, false));
    EXPECT_EQ(BML::Console::CompletionTriggerAction::RebuildCandidates,
        BML::Console::GetCompletionTriggerAction(false, true));
}

TEST(ConsoleCompletionStateTests, CompletionTriggerCyclesCandidatesOncePerTab) {
    EXPECT_EQ(BML::Console::CompletionTriggerAction::SelectNextCandidate,
        BML::Console::GetCompletionTriggerAction(true, false));
    EXPECT_EQ(BML::Console::CompletionTriggerAction::SelectPreviousCandidate,
        BML::Console::GetCompletionTriggerAction(true, true));
}

TEST(ConsoleCompletionStateTests, NextAndPreviousCandidateDoNotSkipEntries) {
    const std::vector<int> pageStarts{0, 2};
    int candidateIndex = 0;
    int candidatePage = 0;

    BML::Console::NextCandidate(4, pageStarts, candidateIndex, candidatePage);
    EXPECT_EQ(1, candidateIndex);
    EXPECT_EQ(0, candidatePage);

    BML::Console::NextCandidate(4, pageStarts, candidateIndex, candidatePage);
    EXPECT_EQ(2, candidateIndex);
    EXPECT_EQ(1, candidatePage);

    BML::Console::PrevCandidate(4, pageStarts, candidateIndex, candidatePage);
    EXPECT_EQ(1, candidateIndex);
    EXPECT_EQ(0, candidatePage);
}

TEST(ConsoleCompletionStateTests, PageNavigationWrapsAcrossCandidatePages) {
    const std::vector<int> pageStarts{0, 2, 4};
    int candidateIndex = 0;
    int candidatePage = 0;

    BML::Console::NextCandidatePage(6, pageStarts, candidateIndex, candidatePage);
    EXPECT_EQ(1, candidateIndex);
    EXPECT_EQ(0, candidatePage);

    BML::Console::NextCandidatePage(6, pageStarts, candidateIndex, candidatePage);
    EXPECT_EQ(2, candidateIndex);
    EXPECT_EQ(1, candidatePage);

    BML::Console::PrevCandidatePage(6, pageStarts, candidateIndex, candidatePage);
    EXPECT_EQ(1, candidateIndex);
    EXPECT_EQ(0, candidatePage);

    BML::Console::PrevCandidatePage(6, pageStarts, candidateIndex, candidatePage);
    EXPECT_EQ(0, candidateIndex);
    EXPECT_EQ(0, candidatePage);
}

} // namespace

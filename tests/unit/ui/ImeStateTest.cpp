#include "UI/Ime/NativePresentation.h"
#include "UI/Ime/State.h"

#include <gtest/gtest.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <imm.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {
using Overlay::Ime::CandidateListSnapshot;
using Overlay::Ime::CandidateDirection;
using Overlay::Ime::CompositionUpdate;
using Overlay::Ime::PlanCandidateSelection;
using Overlay::Ime::ParseCandidateList;
using Overlay::Ime::ParseClauses;
using Overlay::Ime::ParseUtf16;
using Overlay::Ime::State;

void AppendU32(std::vector<std::byte> &bytes, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::byte>((value >> shift) & 0xff));
}

void StoreU32(std::vector<std::byte> &bytes, std::size_t offset,
              std::uint32_t value) {
    ASSERT_LE(offset + sizeof(value), bytes.size());
    for (unsigned int shift = 0; shift < 32; shift += 8)
        bytes[offset++] = static_cast<std::byte>((value >> shift) & 0xff);
}

void AppendUtf16(std::vector<std::byte> &bytes, std::u16string_view value) {
    for (const char16_t character : value) {
        bytes.push_back(static_cast<std::byte>(character & 0xff));
        bytes.push_back(static_cast<std::byte>((character >> 8) & 0xff));
    }
    bytes.push_back(std::byte{0});
    bytes.push_back(std::byte{0});
}

std::vector<std::byte> MakeCandidateList(
    const std::vector<std::u16string> &items, std::uint32_t selection,
    std::uint32_t pageStart, std::uint32_t pageSize,
    std::uint32_t style = 0) {
    constexpr std::size_t fixedHeaderSize = offsetof(CANDIDATELIST, dwOffset);
    std::vector<std::byte> bytes(fixedHeaderSize +
                                 items.size() * sizeof(std::uint32_t));

    StoreU32(bytes, offsetof(CANDIDATELIST, dwStyle), style);
    StoreU32(bytes, offsetof(CANDIDATELIST, dwCount), static_cast<std::uint32_t>(items.size()));
    StoreU32(bytes, offsetof(CANDIDATELIST, dwSelection), selection);
    StoreU32(bytes, offsetof(CANDIDATELIST, dwPageStart), pageStart);
    StoreU32(bytes, offsetof(CANDIDATELIST, dwPageSize), pageSize);

    for (std::size_t index = 0; index < items.size(); ++index) {
        StoreU32(bytes, fixedHeaderSize + index * sizeof(std::uint32_t),
                 static_cast<std::uint32_t>(bytes.size()));
        AppendUtf16(bytes, items[index]);
    }
    StoreU32(bytes, offsetof(CANDIDATELIST, dwSize), static_cast<std::uint32_t>(bytes.size()));
    return bytes;
}
} // namespace

TEST(ImePolicyTest, ContextActivationKeepsSystemUiWhenPresentationIsUnowned) {
    const std::intptr_t original =
        ISC_SHOWUICOMPOSITIONWINDOW | ISC_SHOWUICANDIDATEWINDOW;
    const auto disposition =
        Overlay::Ime::NativePresentation::Decide(
            WM_IME_SETCONTEXT, TRUE, original, false);

    EXPECT_FALSE(disposition.replaceLParam);
    EXPECT_FALSE(disposition.suppress);
}

TEST(ImePolicyTest, ContextDeactivationIsNotRewritten) {
    const auto disposition =
        Overlay::Ime::NativePresentation::Decide(
            WM_IME_SETCONTEXT, FALSE, ISC_SHOWUIALL, false);

    EXPECT_FALSE(disposition.replaceLParam);
    EXPECT_FALSE(disposition.suppress);
}

TEST(ImePolicyTest, ContextActivationPreservesNonUiFlags) {
    constexpr std::intptr_t privateFlag = 0x10000000;
    static_assert((privateFlag & ISC_SHOWUIALL) == 0);
    const auto disposition =
        Overlay::Ime::NativePresentation::Decide(
            WM_IME_SETCONTEXT, TRUE, ISC_SHOWUIALL | privateFlag, true);

    ASSERT_TRUE(disposition.replaceLParam);
    EXPECT_EQ(disposition.lParam & ISC_SHOWUIALL, 0);
    EXPECT_EQ(disposition.lParam & privateFlag, privateFlag);
}

TEST(ImePolicyTest, SuppressesOnlyNativePreeditPresentationWhenOwned) {
    const auto preedit = Overlay::Ime::NativePresentation::Decide(
        WM_IME_COMPOSITION, 0, GCS_COMPSTR, true);
    EXPECT_TRUE(preedit.suppress);

    const auto result = Overlay::Ime::NativePresentation::Decide(
        WM_IME_COMPOSITION, 0, GCS_RESULTSTR, true);
    EXPECT_FALSE(result.suppress);

    const auto unowned = Overlay::Ime::NativePresentation::Decide(
        WM_IME_COMPOSITION, 0, GCS_COMPSTR, false);
    EXPECT_FALSE(unowned.suppress);
}

TEST(ImePolicyTest, SuppressesCandidateNotificationsButNotImeStateChanges) {
    const auto candidate = Overlay::Ime::NativePresentation::Decide(
        WM_IME_NOTIFY, IMN_CHANGECANDIDATE, 0, true);
    EXPECT_TRUE(candidate.suppress);

    const auto openStatus = Overlay::Ime::NativePresentation::Decide(
        WM_IME_NOTIFY, IMN_SETOPENSTATUS, 0, true);
    EXPECT_FALSE(openStatus.suppress);
}

TEST(ImeStateTest, ParsesUtf16AndRejectsMalformedLengths) {
    const std::vector<std::byte> text{
        std::byte{0x60}, std::byte{0x4f},
        std::byte{0x7d}, std::byte{0x59},
    };
    std::u16string decoded;
    ASSERT_TRUE(ParseUtf16(text, decoded));
    EXPECT_EQ(decoded, u"\u4f60\u597d");

    const std::vector<std::byte> odd{text.begin(), text.end() - 1};
    EXPECT_FALSE(ParseUtf16(odd, decoded));

    const std::vector<std::byte> oversized(
        Overlay::Ime::MaxImmPayloadBytes + 2);
    EXPECT_FALSE(ParseUtf16(oversized, decoded));
}

TEST(ImeStateTest, ValidatesUnicodeCompositionClauses) {
    std::vector<std::byte> bytes;
    AppendU32(bytes, 0);
    AppendU32(bytes, 2);
    AppendU32(bytes, 4);

    std::vector<std::uint32_t> clauses;
    ASSERT_TRUE(ParseClauses(bytes, 4, clauses));
    EXPECT_EQ(clauses, (std::vector<std::uint32_t>{0, 2, 4}));

    StoreU32(bytes, sizeof(std::uint32_t), 5);
    EXPECT_FALSE(ParseClauses(bytes, 4, clauses));

    bytes.pop_back();
    EXPECT_FALSE(ParseClauses(bytes, 4, clauses));
}

TEST(ImeStateTest, CompositionResultOnlyClearsPresentationState) {
    State state;
    state.StartComposition();

    CompositionUpdate update;
    update.hasText = true;
    update.text = u"nih";
    update.hasTargetRanges = true;
    update.targetRanges = {{2, 3}};
    update.hasClauseBoundaries = true;
    update.clauseBoundaries = {0, 3};
    update.hasCursor = true;
    update.cursor = 2;
    state.ApplyComposition(std::move(update));

    ASSERT_EQ(state.GetSnapshot().composition, u"nih");
    EXPECT_EQ(state.GetSnapshot().cursor, 2u);

    CandidateListSnapshot candidates;
    candidates.items = {u"\u4f60\u597d"};
    candidates.pageSize = 1;
    ASSERT_TRUE(state.SetCandidateList(0, std::move(candidates)));

    CompositionUpdate result;
    result.hasResult = true;
    state.ApplyComposition(std::move(result));

    EXPECT_FALSE(state.GetSnapshot().composing);
    EXPECT_TRUE(state.GetSnapshot().composition.empty());
    EXPECT_TRUE(state.GetSnapshot().targetRanges.empty());
    EXPECT_TRUE(state.GetSnapshot().clauseBoundaries.empty());
    EXPECT_FALSE(state.GetSnapshot().IsActive());
    EXPECT_FALSE(state.GetSnapshot().HasContent());
}

TEST(ImeStateTest, EmptyPreeditRemainsActiveUntilCompositionEnds) {
    State state;
    state.StartComposition();
    EXPECT_TRUE(state.GetSnapshot().IsActive());
    EXPECT_FALSE(state.GetSnapshot().HasContent());

    CompositionUpdate emptyPreedit;
    emptyPreedit.hasText = true;
    state.ApplyComposition(std::move(emptyPreedit));
    EXPECT_TRUE(state.GetSnapshot().composing);
    EXPECT_TRUE(state.GetSnapshot().IsActive());
    EXPECT_FALSE(state.GetSnapshot().HasContent());

    state.Reset();
    EXPECT_FALSE(state.GetSnapshot().IsActive());
}

TEST(ImeStateTest, RevisionChangesOnlyWhenPresentationStateMayChange) {
    constexpr std::size_t CandidateListIndex = 0;
    constexpr std::uint32_t CandidateListMask = std::uint32_t{1} << CandidateListIndex;
    State state;
    const std::uint64_t initial = state.GetRevision();
    const std::uint64_t initialCandidates = state.GetCandidateRevision();

    state.CloseCandidateLists(0);
    EXPECT_EQ(state.GetRevision(), initial);
    EXPECT_TRUE(state.SetCandidateList(CandidateListIndex, {}));
    EXPECT_EQ(state.GetRevision(), initial);

    state.StartComposition();
    const std::uint64_t composing = state.GetRevision();
    EXPECT_GT(composing, initial);

    CompositionUpdate update;
    update.hasText = true;
    update.text = u"cache";
    state.ApplyComposition(std::move(update));
    const std::uint64_t composition = state.GetRevision();
    EXPECT_GT(composition, composing);
    EXPECT_EQ(state.GetCandidateRevision(), initialCandidates);

    CompositionUpdate unchangedComposition;
    unchangedComposition.hasText = true;
    unchangedComposition.text = u"cache";
    state.ApplyComposition(std::move(unchangedComposition));
    EXPECT_EQ(state.GetRevision(), composition);
    EXPECT_EQ(state.GetCandidateRevision(), initialCandidates);

    CandidateListSnapshot candidates;
    candidates.items = {u"candidate"};
    ASSERT_TRUE(state.SetCandidateList(CandidateListIndex, std::move(candidates)));
    const std::uint64_t candidate = state.GetRevision();
    const std::uint64_t candidateData = state.GetCandidateRevision();
    EXPECT_GT(candidate, composition);
    EXPECT_GT(candidateData, initialCandidates);

    CandidateListSnapshot unchanged;
    unchanged.items = {u"candidate"};
    ASSERT_TRUE(state.SetCandidateList(CandidateListIndex, std::move(unchanged)));
    EXPECT_EQ(state.GetRevision(), candidate);
    EXPECT_EQ(state.GetCandidateRevision(), candidateData);

    state.CloseCandidateLists(CandidateListMask);
    const std::uint64_t closed = state.GetRevision();
    EXPECT_GT(closed, candidate);
    EXPECT_GT(state.GetCandidateRevision(), candidateData);
    state.CloseCandidateLists(CandidateListMask);
    EXPECT_EQ(state.GetRevision(), closed);
}

TEST(ImeStateTest, ResultOnlyMetadataCannotRestartComposition) {
    State state;
    state.StartComposition();

    CompositionUpdate result;
    result.hasResult = true;
    result.hasCursor = true;
    result.cursor = 3;
    state.ApplyComposition(std::move(result));
    EXPECT_FALSE(state.GetSnapshot().IsActive());

    CompositionUpdate resultAndNewPreedit;
    resultAndNewPreedit.hasResult = true;
    resultAndNewPreedit.hasText = true;
    resultAndNewPreedit.text = u"next";
    state.ApplyComposition(std::move(resultAndNewPreedit));
    EXPECT_TRUE(state.GetSnapshot().IsActive());
    EXPECT_EQ(state.GetSnapshot().composition, u"next");
}

TEST(ImeStateTest, CompositionCursorCannotSplitSurrogatePair) {
    State state;
    CompositionUpdate update;
    update.hasText = true;
    update.text = u"a\U0001F600b";
    update.hasCursor = true;
    update.cursor = 2;
    state.ApplyComposition(std::move(update));

    EXPECT_EQ(state.GetSnapshot().cursor, 1u);
}

TEST(ImeStateTest, ParsesCandidatePageAndNormalizesBounds) {
    const std::vector<std::byte> bytes =
        MakeCandidateList({u"\u4f60\u597d", u"\u62df\u597d",
                           u"\u4f60\u53f7"}, 1, 1, 9);

    CandidateListSnapshot list;
    ASSERT_TRUE(ParseCandidateList(bytes, list));
    ASSERT_EQ(list.items.size(), 3u);
    EXPECT_EQ(list.items[0], u"\u4f60\u597d");
    EXPECT_EQ(list.items[1], u"\u62df\u597d");
    EXPECT_EQ(list.selection, 1u);
    EXPECT_TRUE(list.hasSelection);
    EXPECT_EQ(list.pageStart, 1u);
    EXPECT_EQ(list.pageSize, 3u);
}

TEST(ImeStateTest, PreservesCandidatePageCapacityOnPartialFinalPage) {
    const std::vector<std::byte> bytes =
        MakeCandidateList({u"one", u"two", u"three", u"four", u"five"},
                          4, 4, 2);

    CandidateListSnapshot list;
    ASSERT_TRUE(ParseCandidateList(bytes, list));
    EXPECT_EQ(list.pageStart, 4u);
    EXPECT_EQ(list.pageSize, 2u);
}

TEST(ImeStateTest, PreservesIndexedCandidatePageMetadata) {
    CandidateListSnapshot list;
    const std::vector<std::uint32_t> pageStarts{0, 5, 10};
    const std::uint32_t pageIndex = static_cast<std::uint32_t>(pageStarts.size() - 1);
    list.items.resize(pageStarts.back() + 2, u"candidate");

    ASSERT_TRUE(list.SetIndexedPage(pageStarts, pageIndex));
    EXPECT_EQ(list.pageStart, pageStarts[pageIndex]);
    EXPECT_EQ(list.pageSize, list.items.size() - pageStarts[pageIndex]);
    ASSERT_TRUE(list.pagePosition.has_value());
    EXPECT_EQ(list.pagePosition->index, pageIndex);
    EXPECT_EQ(list.pagePosition->count, pageStarts.size());
}

TEST(ImeStateTest, RejectsInvalidIndexedCandidatePagesWithoutChangingState) {
    CandidateListSnapshot list;
    list.items = {u"one", u"two", u"three"};
    list.pageStart = 1;
    list.pageSize = 2;
    const CandidateListSnapshot original = list;
    const std::uint32_t duplicateStarts[] = {0, 2, 2};

    EXPECT_FALSE(list.SetIndexedPage(duplicateStarts, 1));
    EXPECT_EQ(list, original);
}

TEST(ImeStateTest, PreservesCandidateStyleAndRejectsLegacyCodeEntry) {
    CandidateListSnapshot list;
    const std::vector<std::byte> reading =
        MakeCandidateList({u"reading"}, 0, 0, 1, 1);
    ASSERT_TRUE(ParseCandidateList(reading, list));
    EXPECT_EQ(list.style, 1u);

    const std::vector<std::byte> legacyCode =
        MakeCandidateList({u"ignored"}, 0, 0, 1, 2);
    EXPECT_FALSE(ParseCandidateList(legacyCode, list));
}

TEST(ImeStateTest, RejectsTruncatedAndOutOfRangeCandidateOffsets) {
    std::vector<std::byte> bytes =
        MakeCandidateList({u"\u5019\u88dc"}, 0, 0, 1);
    CandidateListSnapshot list;

    std::vector<std::byte> truncated(bytes.begin(), bytes.end() - 2);
    EXPECT_FALSE(ParseCandidateList(truncated, list));

    StoreU32(bytes, offsetof(CANDIDATELIST, dwOffset),
             static_cast<std::uint32_t>(bytes.size() + 2));
    EXPECT_FALSE(ParseCandidateList(bytes, list));

    StoreU32(bytes, offsetof(CANDIDATELIST, dwOffset), 1);
    EXPECT_FALSE(ParseCandidateList(bytes, list));

    std::vector<std::byte> excessiveCount(offsetof(CANDIDATELIST, dwOffset));
    StoreU32(excessiveCount, offsetof(CANDIDATELIST, dwSize),
             static_cast<std::uint32_t>(excessiveCount.size()));
    StoreU32(excessiveCount, offsetof(CANDIDATELIST, dwCount),
             static_cast<std::uint32_t>(
                 Overlay::Ime::MaxCandidateCount + 1));
    EXPECT_FALSE(ParseCandidateList(excessiveCount, list));

    const std::vector<std::u16string> overlappingItems{u"a", u"b"};
    std::vector<std::byte> overlapping = MakeCandidateList(overlappingItems, 0, 0, 2);
    const std::size_t firstStringOffset = offsetof(CANDIDATELIST, dwOffset) +
                                          overlappingItems.size() * sizeof(std::uint32_t);
    const std::size_t firstTerminatorOffset = firstStringOffset +
                                              overlappingItems.front().size() * sizeof(char16_t);
    overlapping[firstTerminatorOffset] = static_cast<std::byte>('x');
    overlapping[firstTerminatorOffset + 1] = std::byte{0};
    EXPECT_FALSE(ParseCandidateList(overlapping, list));
}

TEST(ImeStateTest, TracksAndClosesMultipleCandidateLists) {
    State state;
    CandidateListSnapshot chinese;
    chinese.items = {u"\u4f60\u597d"};
    chinese.pageSize = 1;
    CandidateListSnapshot korean;
    korean.items = {u"\ud55c\uae00"};
    korean.pageSize = 1;

    ASSERT_TRUE(state.SetCandidateList(0, std::move(chinese)));
    ASSERT_TRUE(state.SetCandidateList(3, std::move(korean)));
    EXPECT_TRUE(state.GetSnapshot().candidateLists[0].has_value());
    EXPECT_TRUE(state.GetSnapshot().candidateLists[3].has_value());

    state.CloseCandidateLists(1u << 3);
    EXPECT_TRUE(state.GetSnapshot().candidateLists[0].has_value());
    EXPECT_FALSE(state.GetSnapshot().candidateLists[3].has_value());

    state.CloseCandidateLists(0);
    EXPECT_FALSE(state.GetSnapshot().HasContent());
}

TEST(ImeStateTest, CandidateNavigationWrapsAcrossTheWholeList) {
    Overlay::Ime::Snapshot snapshot;
    CandidateListSnapshot candidates;
    candidates.items = {u"one", u"two", u"three"};
    candidates.selection = static_cast<std::uint32_t>(
        candidates.items.size() - 1);
    candidates.hasSelection = true;
    snapshot.candidateLists.front() = candidates;

    const auto next = PlanCandidateSelection(
        snapshot, CandidateDirection::Next);
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(next->listIndex, 0u);
    EXPECT_EQ(next->itemIndex, 0u);

    snapshot.candidateLists.front()->selection = 0;
    const auto previous = PlanCandidateSelection(
        snapshot, CandidateDirection::Previous);
    ASSERT_TRUE(previous.has_value());
    EXPECT_EQ(previous->itemIndex,
              snapshot.candidateLists.front()->items.size() - 1);
}

TEST(ImeStateTest, CandidateStepHandlesMissingAndInvalidSelection) {
    const std::uint32_t count = 3;
    EXPECT_EQ(StepCandidateIndex(
                  count, std::nullopt, CandidateDirection::Next),
              0u);
    EXPECT_EQ(StepCandidateIndex(
                  count, count, CandidateDirection::Previous),
              count - 1);
    EXPECT_FALSE(StepCandidateIndex(
        0, std::nullopt, CandidateDirection::Next));
}

TEST(ImeStateTest, CandidateNavigationUsesTheSelectedList) {
    Overlay::Ime::Snapshot snapshot;
    CandidateListSnapshot first;
    first.items = {u"first"};
    snapshot.candidateLists.front() = std::move(first);

    CandidateListSnapshot selected;
    selected.items = {u"alpha", u"beta"};
    selected.selection = 0;
    selected.hasSelection = true;
    const std::size_t selectedListIndex = snapshot.candidateLists.size() - 1;
    snapshot.candidateLists[selectedListIndex] = std::move(selected);

    const auto request = PlanCandidateSelection(
        snapshot, CandidateDirection::Next);
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->listIndex, selectedListIndex);
    EXPECT_EQ(request->itemIndex, 1u);
}

TEST(ImeStateTest, CandidateNavigationRequiresCandidates) {
    const Overlay::Ime::Snapshot snapshot;
    EXPECT_FALSE(PlanCandidateSelection(
        snapshot, CandidateDirection::Next).has_value());
}

TEST(ImeStateTest, ResetDropsCompositionAndCandidates) {
    State state;
    state.StartComposition();
    CompositionUpdate update;
    update.hasText = true;
    update.text = u"\u304b\u306a";
    state.ApplyComposition(std::move(update));

    CandidateListSnapshot candidates;
    candidates.items = {u"\u4eee\u540d"};
    candidates.pageSize = 1;
    ASSERT_TRUE(state.SetCandidateList(0, std::move(candidates)));

    state.Reset();
    EXPECT_FALSE(state.GetSnapshot().composing);
    EXPECT_FALSE(state.GetSnapshot().IsActive());
    EXPECT_FALSE(state.GetSnapshot().HasContent());
}

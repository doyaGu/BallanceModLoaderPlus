#include "UI/Ime/RailLayout.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace {
    float MeasureCodepoints(std::u16string_view text, const void *) {
        float count = 0.0f;
        for (std::size_t index = 0; index < text.size(); ++index) {
            if (text[index] >= 0xd800 && text[index] <= 0xdbff && index + 1 < text.size() &&
                text[index + 1] >= 0xdc00 && text[index + 1] <= 0xdfff) {
                ++index;
            }
            count += 1.0f;
        }
        return count;
    }
}

TEST(ImePresentationTest, CompositionFitKeepsTargetAndCursorVisible) {
    Overlay::Ime::Snapshot snapshot;
    snapshot.composition = u"0123456789";
    snapshot.cursor = 7;
    snapshot.targetRanges = {{6, 8}};

    const auto fit = Overlay::Ime::Presentation::Layout::FitComposition(
        snapshot, 5.0f, 1.0f, &MeasureCodepoints);
    EXPECT_LE(fit.begin, 6u);
    EXPECT_GE(fit.end, 8u);
    EXPECT_LE(fit.begin, snapshot.cursor);
    EXPECT_GE(fit.end, snapshot.cursor);
    EXPECT_TRUE(fit.clippedBefore);
    EXPECT_TRUE(fit.clippedAfter);
    EXPECT_LE(fit.width, 5.0f);
}

TEST(ImePresentationTest, CompositionFitDoesNotSplitSurrogatePair) {
    Overlay::Ime::Snapshot snapshot;
    snapshot.composition = u"a\U0001F600bc";
    snapshot.cursor = 2;
    snapshot.targetRanges = {{1, 3}};

    const auto fit = Overlay::Ime::Presentation::Layout::FitComposition(
        snapshot, 3.0f, 1.0f, &MeasureCodepoints);
    EXPECT_LE(fit.begin, 1u);
    EXPECT_GE(fit.end, 3u);
    EXPECT_NE(fit.begin, 2u);
    EXPECT_NE(fit.end, 2u);
    EXPECT_LE(fit.width, 3.0f);

    snapshot.targetRanges.clear();
    const auto caretFit = Overlay::Ime::Presentation::Layout::FitComposition(
        snapshot, 2.0f, 0.5f, &MeasureCodepoints);
    EXPECT_NE(caretFit.begin, 2u);
    EXPECT_NE(caretFit.end, 2u);
}

TEST(ImePresentationTest, VerticalTextFitPreservesNegativeGlyphOverhang) {
    const auto fit = Overlay::Ime::Presentation::Layout::FitTextVertically(
        19.0f, 5.0f, {-2.0f, 17.0f, true});

    EXPECT_FLOAT_EQ(fit.textOrigin, 7.0f);
    EXPECT_FLOAT_EQ(fit.surfaceHeight, 31.0f);
    EXPECT_GE(fit.textOrigin - 2.0f, fit.contentMin);
    EXPECT_LE(fit.textOrigin + 17.0f, fit.contentMax);
    EXPECT_LE(fit.textOrigin + 19.0f, fit.contentMax);
}

TEST(ImePresentationTest, VerticalTextFitKeepsTheNormalRowHeight) {
    const auto fit = Overlay::Ime::Presentation::Layout::FitTextVertically(
        19.0f, 5.0f, {5.0f, 15.0f, true});

    EXPECT_FLOAT_EQ(fit.textOrigin, 5.0f);
    EXPECT_FLOAT_EQ(fit.contentMin, 5.0f);
    EXPECT_FLOAT_EQ(fit.contentMax, 24.0f);
    EXPECT_FLOAT_EQ(fit.surfaceHeight, 29.0f);
}

TEST(ImePresentationTest, CandidatePagesUseImePageBoundsAndSelectedList) {
    Overlay::Ime::Snapshot snapshot;
    Overlay::Ime::CandidateListSnapshot first;
    first.items = {u"one", u"two", u"three", u"four", u"five"};
    first.selection = 3;
    first.hasSelection = true;
    first.pageStart = 2;
    first.pageSize = 2;
    snapshot.candidateLists[0] = std::move(first);

    Overlay::Ime::CandidateListSnapshot second;
    second.items = {u"alpha", u"beta"};
    snapshot.candidateLists[4] = std::move(second);

    const auto pages = Overlay::Ime::Presentation::Layout::BuildCandidatePages(snapshot);
    ASSERT_EQ(pages.size(), 2u);
    EXPECT_EQ(pages[0].listIndex, 0u);
    EXPECT_EQ(pages[0].begin, 2u);
    EXPECT_EQ(pages[0].end, 4u);
    EXPECT_EQ(pages[0].selection, 3u);
    EXPECT_TRUE(pages[0].hasSelection);
    EXPECT_EQ(pages[0].pageNumber, 2u);
    EXPECT_EQ(pages[0].pageCount, 3u);

    EXPECT_EQ(pages[1].listIndex, 4u);
    EXPECT_EQ(pages[1].begin, 0u);
    EXPECT_EQ(pages[1].end, 2u);
    EXPECT_EQ(pages[1].pageNumber, 1u);
    EXPECT_EQ(pages[1].pageCount, 1u);
}

TEST(ImePresentationTest, CandidatePageDoesNotSelectAnItemOutsideTheVisiblePage) {
    Overlay::Ime::Snapshot snapshot;
    Overlay::Ime::CandidateListSnapshot candidates;
    candidates.items = {u"one", u"two", u"three"};
    candidates.selection = 0;
    candidates.hasSelection = true;
    candidates.pageStart = 1;
    candidates.pageSize = 2;
    snapshot.candidateLists[0] = std::move(candidates);

    const auto pages = Overlay::Ime::Presentation::Layout::BuildCandidatePages(snapshot);
    ASSERT_EQ(pages.size(), 1u);
    EXPECT_FALSE(pages[0].hasSelection);
}

TEST(ImePresentationTest, CandidatePageUsesAuthoritativePositionOnShortFinalPage) {
    Overlay::Ime::Snapshot snapshot;
    Overlay::Ime::CandidateListSnapshot candidates;
    const std::vector<std::uint32_t> pageStarts{0, 5, 10};
    const std::uint32_t pageIndex = static_cast<std::uint32_t>(pageStarts.size() - 1);
    candidates.items.resize(pageStarts.back() + 2, u"candidate");
    candidates.pageStart = pageStarts[pageIndex];
    candidates.pageSize = static_cast<std::uint32_t>(candidates.items.size()) - candidates.pageStart;
    candidates.pagePosition = Overlay::Ime::CandidatePagePosition{
        pageIndex,
        static_cast<std::uint32_t>(pageStarts.size()),
    };
    snapshot.candidateLists[0] = std::move(candidates);

    const auto pages = Overlay::Ime::Presentation::Layout::BuildCandidatePages(snapshot);
    ASSERT_EQ(pages.size(), 1u);
    EXPECT_EQ(pages[0].pageNumber, pageIndex + 1);
    EXPECT_EQ(pages[0].pageCount, pageStarts.size());
}

TEST(ImePresentationTest, CandidatePageKeepsImeCapacityOnShortFinalPage) {
    Overlay::Ime::Snapshot snapshot;
    Overlay::Ime::CandidateListSnapshot candidates;
    candidates.items = {u"one", u"two", u"three", u"four", u"five"};
    candidates.pageStart = 4;
    candidates.pageSize = 2;
    snapshot.candidateLists[0] = std::move(candidates);

    const auto pages = Overlay::Ime::Presentation::Layout::BuildCandidatePages(snapshot);
    ASSERT_EQ(pages.size(), 1u);
    EXPECT_EQ(pages[0].begin, 4u);
    EXPECT_EQ(pages[0].end, 5u);
    EXPECT_EQ(pages[0].pageNumber, 3u);
    EXPECT_EQ(pages[0].pageCount, 3u);
}

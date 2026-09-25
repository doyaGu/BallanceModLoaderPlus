#include "UI/Ime/RailLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Overlay::Ime::Presentation::Layout {
    namespace {
        constexpr float PixelRoundingTolerance = 0.5f;

        std::size_t PreviousCodepoint(std::u16string_view text, std::size_t offset) {
            if (offset == 0)
                return 0;
            --offset;
            if (offset != 0 && text[offset] >= 0xdc00 &&
                text[offset] <= 0xdfff && text[offset - 1] >= 0xd800 &&
                text[offset - 1] <= 0xdbff) {
                --offset;
            }
            return offset;
        }

        std::size_t NextCodepoint(std::u16string_view text, std::size_t offset) {
            if (offset >= text.size())
                return text.size();
            if (text[offset] >= 0xd800 && text[offset] <= 0xdbff &&
                offset + 1 < text.size() && text[offset + 1] >= 0xdc00 &&
                text[offset + 1] <= 0xdfff) {
                return offset + 2;
            }
            return offset + 1;
        }

        std::size_t NormalizeBoundary(std::u16string_view text, std::size_t offset) {
            offset = std::min(offset, text.size());
            if (offset != 0 && offset < text.size() &&
                text[offset] >= 0xdc00 && text[offset] <= 0xdfff &&
                text[offset - 1] >= 0xd800 &&
                text[offset - 1] <= 0xdbff) {
                --offset;
            }
            return offset;
        }

        float Width(std::u16string_view text, std::size_t begin,
                    std::size_t end, bool clippedBefore,
                    bool clippedAfter, float ellipsisWidth,
                    MeasureText measure, const void *measureContext) {
            float width = measure(text.substr(begin, end - begin),
                                  measureContext);
            if (clippedBefore)
                width += ellipsisWidth;
            if (clippedAfter)
                width += ellipsisWidth;
            return width;
        }

        float NonnegativeFinite(float value) noexcept {
            return std::isfinite(value) && value > 0.0f ? value : 0.0f;
        }
    }

    CompositionFit FitComposition(const Snapshot &snapshot,
                                  float maxWidth, float ellipsisWidth,
                                  MeasureText measure,
                                  const void *measureContext) {
        const std::u16string_view text(snapshot.composition);
        CompositionFit fit{0, text.size(), false, false, 0.0f};
        if (!measure || text.empty())
            return fit;

        maxWidth = std::max(0.0f, maxWidth);
        ellipsisWidth = std::max(0.0f, ellipsisWidth);
        fit.width = Width(text, 0, text.size(), false, false,
                          ellipsisWidth, measure, measureContext);
        if (fit.width <= maxWidth)
            return fit;

        const std::size_t cursor = NormalizeBoundary(
            text, snapshot.cursor);
        std::size_t focusBegin = cursor;
        std::size_t focusEnd = cursor;
        bool foundCursorRange = false;
        for (const TextRange &range : snapshot.targetRanges) {
            const std::size_t begin = std::min<std::size_t>(
                range.begin, text.size());
            const std::size_t end = std::min<std::size_t>(
                std::max(range.begin, range.end), text.size());
            if (begin <= cursor && cursor <= end) {
                focusBegin = begin;
                focusEnd = end;
                foundCursorRange = true;
                break;
            }
            if (focusBegin == cursor && focusEnd == cursor) {
                focusBegin = begin;
                focusEnd = end;
            }
        }

        const float focusWidth = Width(
            text, focusBegin, focusEnd, focusBegin != 0,
            focusEnd != text.size(), ellipsisWidth,
            measure, measureContext);
        if (focusBegin < focusEnd && focusWidth <= maxWidth) {
            fit.begin = focusBegin;
            fit.end = focusEnd;
        } else if (cursor < text.size()) {
            fit.begin = cursor;
            fit.end = NextCodepoint(text, cursor);
        } else {
            fit.begin = PreviousCodepoint(text, cursor);
            fit.end = cursor;
        }

        while (true) {
            const std::size_t left = PreviousCodepoint(text, fit.begin);
            const std::size_t right = NextCodepoint(text, fit.end);
            const bool canLeft = left != fit.begin &&
                Width(text, left, fit.end, left != 0,
                      fit.end != text.size(), ellipsisWidth,
                      measure, measureContext) <= maxWidth;
            const bool canRight = right != fit.end &&
                Width(text, fit.begin, right, fit.begin != 0,
                      right != text.size(), ellipsisWidth,
                      measure, measureContext) <= maxWidth;
            if (!canLeft && !canRight)
                break;

            const bool needsTargetLeft = fit.begin > focusBegin;
            const bool needsTargetRight = fit.end < focusEnd;
            if (canLeft && (needsTargetLeft || !canRight ||
                            (!needsTargetRight && foundCursorRange))) {
                fit.begin = left;
            } else {
                fit.end = right;
            }
        }

        fit.clippedBefore = fit.begin != 0;
        fit.clippedAfter = fit.end != text.size();
        fit.width = Width(
            text, fit.begin, fit.end, fit.clippedBefore,
            fit.clippedAfter, ellipsisWidth, measure, measureContext);
        return fit;
    }

    VerticalTextFit FitTextVertically(
        float lineHeight, float padding,
        GlyphVerticalBounds glyphBounds) noexcept {
        if (!std::isfinite(lineHeight) || lineHeight < 0.0f)
            lineHeight = 0.0f;
        if (!std::isfinite(padding) || padding < 0.0f)
            padding = 0.0f;

        float minimum = 0.0f;
        float maximum = lineHeight;
        if (glyphBounds.valid && std::isfinite(glyphBounds.minimum) &&
            std::isfinite(glyphBounds.maximum) &&
            glyphBounds.minimum <= glyphBounds.maximum) {
            minimum = std::min(0.0f, glyphBounds.minimum);
            maximum = std::max(lineHeight, glyphBounds.maximum);
        }

        VerticalTextFit fit;
        fit.textOrigin = padding - minimum;
        fit.contentMin = padding;
        fit.contentMax = padding + maximum - minimum;
        fit.surfaceHeight = fit.contentMax + padding;
        return fit;
    }

    HorizontalRail FitHorizontalRail(float anchorX, float desiredWidth, float minimumWidth,
                                    float maximumWidth, float workMinX, float workMaxX) noexcept {
        workMaxX = std::max(workMinX, workMaxX);
        const float width = std::clamp(desiredWidth, minimumWidth, maximumWidth);
        const float caretX = std::clamp(anchorX, workMinX, workMaxX);
        const float roomRight = workMaxX - caretX;
        if (roomRight >= minimumWidth)
            return {caretX, std::min(width, roomRight)};
        return {std::max(workMinX, workMaxX - minimumWidth), minimumWidth};
    }

    CandidateRowFit FitCandidateRow(float availableWidth, float compositionWidth,
                                    float priorityCandidateWidth, float statusWidth,
                                    float itemGap) noexcept {
        availableWidth = NonnegativeFinite(availableWidth);
        compositionWidth = NonnegativeFinite(compositionWidth);
        priorityCandidateWidth = NonnegativeFinite(priorityCandidateWidth);
        statusWidth = NonnegativeFinite(statusWidth);
        itemGap = NonnegativeFinite(itemGap);

        CandidateRowFit fit;
        const float statusSpace = statusWidth + itemGap;
        fit.showStatus = statusWidth > 0.0f &&
            FitsTextHorizontally(priorityCandidateWidth + statusSpace, availableWidth);

        float contentWidth = availableWidth;
        if (fit.showStatus)
            contentWidth = std::max(0.0f, contentWidth - statusSpace);

        const float priorityWidth = std::min(priorityCandidateWidth, contentWidth);
        const float separatorWidth = priorityWidth > 0.0f && compositionWidth > 0.0f
            ? itemGap * 2.0f
            : 0.0f;
        const float maximumCompositionWidth = std::max(
            0.0f, contentWidth - priorityWidth - separatorWidth);
        fit.compositionWidth = std::min(compositionWidth, maximumCompositionWidth);
        fit.showSeparator = fit.compositionWidth > 0.0f && priorityWidth > 0.0f;
        fit.candidateWidth = std::max(
            0.0f, contentWidth - fit.compositionWidth -
                      (fit.showSeparator ? itemGap * 2.0f : 0.0f));
        return fit;
    }

    bool FitsTextHorizontally(float textWidth, float availableWidth) noexcept {
        if (!std::isfinite(textWidth) || !std::isfinite(availableWidth) ||
            textWidth < 0.0f || availableWidth < 0.0f) {
            return false;
        }
        return textWidth <= availableWidth + PixelRoundingTolerance;
    }

    std::vector<CandidatePage> BuildCandidatePages(const Snapshot &snapshot) {
        std::vector<CandidatePage> pages;
        pages.reserve(snapshot.candidateLists.size());
        for (std::size_t listIndex = 0; listIndex < snapshot.candidateLists.size(); ++listIndex) {
            const auto &optionalList = snapshot.candidateLists[listIndex];
            if (!optionalList || optionalList->items.empty())
                continue;

            const CandidateListSnapshot &list = *optionalList;
            const std::uint32_t count = static_cast<std::uint32_t>(
                std::min<std::size_t>(list.items.size(), std::numeric_limits<std::uint32_t>::max()));
            const std::uint32_t begin = std::min(list.pageStart, count - 1);
            const std::uint32_t pageSize = list.pageSize == 0
                ? count - begin
                : std::min(list.pageSize, count - begin);

            CandidatePage page;
            page.listIndex = listIndex;
            page.begin = begin;
            page.end = begin + pageSize;
            page.selection = list.selection;
            page.hasSelection = list.hasSelection && list.selection >= page.begin && list.selection < page.end;
            if (list.pagePosition &&
                list.pagePosition->index < list.pagePosition->count) {
                page.pageNumber = list.pagePosition->index + 1;
                page.pageCount = list.pagePosition->count;
            } else {
                const std::uint32_t numberingSize = list.pageSize == 0
                    ? std::max(count, std::uint32_t{1})
                    : list.pageSize;
                page.pageNumber = begin / numberingSize + 1;
                page.pageCount = (count + numberingSize - 1) / numberingSize;
            }
            pages.push_back(page);
        }
        return pages;
    }
}

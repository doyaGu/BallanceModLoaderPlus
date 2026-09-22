#ifndef BML_UI_IME_RAIL_LAYOUT_H
#define BML_UI_IME_RAIL_LAYOUT_H

#include "UI/Ime/State.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace Overlay::Ime::Presentation::Layout {
    using MeasureText = float (*)(std::u16string_view text, const void *context);

    struct CompositionFit {
        std::size_t begin = 0;
        std::size_t end = 0;
        bool clippedBefore = false;
        bool clippedAfter = false;
        float width = 0.0f;
    };

    struct GlyphVerticalBounds {
        float minimum = 0.0f;
        float maximum = 0.0f;
        bool valid = false;
    };

    struct VerticalTextFit {
        // All values are offsets from the top of the presentation surface.
        float textOrigin = 0.0f;
        float contentMin = 0.0f;
        float contentMax = 0.0f;
        float surfaceHeight = 0.0f;
    };

    struct HorizontalRail {
        float x = 0.0f;
        float width = 0.0f;
    };

    struct CandidatePage {
        std::size_t listIndex = 0;
        std::uint32_t begin = 0;
        std::uint32_t end = 0;
        std::uint32_t selection = 0;
        std::uint32_t pageNumber = 1;
        std::uint32_t pageCount = 1;
        bool hasSelection = false;
    };

    CompositionFit FitComposition(const Snapshot &snapshot, float maxWidth, float ellipsisWidth,
                                  MeasureText measure, const void *measureContext = nullptr);
    VerticalTextFit FitTextVertically(float lineHeight, float padding, GlyphVerticalBounds glyphBounds) noexcept;
    HorizontalRail FitHorizontalRail(float anchorX, float desiredWidth, float minimumWidth,
                                    float maximumWidth, float workMinX, float workMaxX) noexcept;
    std::vector<CandidatePage> BuildCandidatePages(const Snapshot &snapshot);
}

#endif // BML_UI_IME_RAIL_LAYOUT_H

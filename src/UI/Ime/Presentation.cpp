#include "UI/Ime/Presentation.h"

#include "UI/Ime/RailLayout.h"
#include "UI/Ime/Runtime.h"
#include "UI/InputSurfaceStyle.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Overlay::Ime::Presentation {
    namespace {
        constexpr float ScreenMargin = InputSurfaceStyle::ScreenMargin;
        constexpr float AnchorGap = InputSurfaceStyle::AnchorGap;
        constexpr float RailPaddingX = InputSurfaceStyle::PaddingX;
        constexpr float RailPaddingY = InputSurfaceStyle::PaddingY;
        constexpr float RailItemGap = InputSurfaceStyle::ItemGap;
        constexpr float CandidatePaddingX = InputSurfaceStyle::ChipPaddingX;
        constexpr float MinimumRailWidth = 280.0f;
        constexpr float MaximumRailWidthRatio = 0.72f;
        constexpr float CandidateWidthReserveRatio = 0.62f;
        constexpr float MinimumCompositionWidth = 72.0f;
        constexpr const char *WindowName = "##BML IME";
        constexpr const char *Ellipsis = "\xe2\x80\xa6";
        constexpr const char *CandidateNavigationHint = "Tab";

        struct Placement {
            ImVec2 position;
            float width = 0.0f;
        };

        struct PlacementReservation {
            Placement value;
            ImGuiContext *context = nullptr;
            int frame = -1;
        };

        PlacementReservation g_Placement;
        Runtime::PresentationFrame g_Frame;

        bool HandleCandidateShortcuts(const Snapshot &snapshot) {
            if (!ImGui::GetCurrentContext() || !snapshot.HasCandidates())
                return false;

            constexpr ImGuiInputFlags route = ImGuiInputFlags_RouteGlobal | ImGuiInputFlags_RouteOverActive;
            ImGui::Shortcut(ImGuiMod_Shift | ImGuiKey_Tab, route);
            ImGui::Shortcut(ImGuiKey_Tab, route);
            const ImGuiIO &io = ImGui::GetIO();
            if (!ImGui::IsKeyPressed(ImGuiKey_Tab, false) || io.KeyCtrl || io.KeyAlt || io.KeySuper)
                return false;

            return Runtime::MoveCandidate(io.KeyShift ? CandidateDirection::Previous : CandidateDirection::Next);
        }

        void AppendUtf8(std::string &output, std::uint32_t codepoint) {
            if (codepoint <= 0x7f) {
                output.push_back(static_cast<char>(codepoint));
            } else if (codepoint <= 0x7ff) {
                output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
                output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
            } else if (codepoint <= 0xffff) {
                output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
                output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
                output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
            } else {
                output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
                output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
                output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
                output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
            }
        }

        float TextWidth(ImFont *font, float fontSize, std::string_view text) {
            if (!font || text.empty())
                return 0.0f;
            return font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.data(), text.data() + text.size()).x;
        }

        struct PreparedComposition {
            const char16_t *source = nullptr;
            std::size_t sourceLength = 0;
            std::u16string value;
            std::string utf8;
            std::vector<std::size_t> byteOffsets;
            std::vector<float> widths;

            float Width(std::size_t begin, std::size_t end) const {
                begin = std::min(begin, sourceLength);
                end = std::clamp(end, begin, sourceLength);
                return widths.empty() ? 0.0f : widths[end] - widths[begin];
            }
        };

        bool PrepareComposition(std::u16string_view text, ImFontBaked &baked, float fontSize,
                                bool fontChanged, PreparedComposition &prepared) {
            prepared.source = text.data();
            prepared.sourceLength = text.size();
            if (!fontChanged && prepared.value == text)
                return false;

            prepared.value.assign(text);
            prepared.utf8.clear();
            prepared.utf8.reserve(text.size() * 3);
            prepared.byteOffsets.assign(text.size() + 1, 0);
            prepared.widths.assign(text.size() + 1, 0.0f);

            const float scale = baked.Size > 0.0f ? fontSize / baked.Size : 1.0f;
            std::size_t index = 0;
            float width = 0.0f;
            while (index < text.size()) {
                const std::size_t begin = index;
                prepared.byteOffsets[begin] = prepared.utf8.size();
                prepared.widths[begin] = width;

                std::uint32_t codepoint = text[index++];
                if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                    if (index < text.size()) {
                        const std::uint32_t low = text[index];
                        if (low >= 0xdc00 && low <= 0xdfff) {
                            codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                            prepared.byteOffsets[index] = prepared.utf8.size();
                            prepared.widths[index] = width;
                            ++index;
                        } else {
                            codepoint = 0xfffd;
                        }
                    } else {
                        codepoint = 0xfffd;
                    }
                } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                    codepoint = 0xfffd;
                }
                AppendUtf8(prepared.utf8, codepoint);
                width += baked.GetCharAdvance(static_cast<ImWchar>(codepoint)) * scale;
                prepared.byteOffsets[index] = prepared.utf8.size();
                prepared.widths[index] = width;
            }
            return true;
        }

        struct CandidateChip {
            std::u16string source;
            std::string key;
            std::string value;
            float keyWidth = 0.0f;
            float valueWidth = 0.0f;
            float width = 0.0f;
            bool selected = false;
        };

        struct CandidateRail {
            std::vector<CandidateChip> chips;
            std::string status;
            std::uint32_t begin = 0;
            std::uint32_t selection = 0;
            std::uint32_t pageNumber = 0;
            std::uint32_t pageCount = 0;
            std::size_t additionalLists = 0;
            bool hasSelection = false;
            float statusWidth = 0.0f;
            float totalWidth = 0.0f;
            float priorityWidth = 0.0f;
            float widthThroughSelection = 0.0f;
        };

        bool BuildCandidateRail(const Snapshot &snapshot, ImFont *font, float fontSize,
                                bool fontChanged, CandidateRail &rail) {
            bool contentChanged = fontChanged;
            rail.begin = 0;
            rail.selection = 0;
            rail.hasSelection = false;
            rail.totalWidth = 0.0f;
            rail.priorityWidth = 0.0f;
            rail.widthThroughSelection = 0.0f;
            const std::vector<Layout::CandidatePage> pages = Layout::BuildCandidatePages(snapshot);
            if (pages.empty()) {
                contentChanged |= !rail.chips.empty() || !rail.status.empty();
                rail.chips.clear();
                rail.status.clear();
                rail.statusWidth = 0.0f;
                rail.pageNumber = 0;
                rail.pageCount = 0;
                rail.additionalLists = 0;
                return contentChanged;
            }

            const Layout::CandidatePage *page = &pages.front();
            for (const Layout::CandidatePage &candidatePage : pages) {
                if (candidatePage.hasSelection) {
                    page = &candidatePage;
                    break;
                }
            }
            const CandidateListSnapshot &list = *snapshot.candidateLists[page->listIndex];

            rail.begin = page->begin;
            rail.selection = page->selection;
            rail.hasSelection = page->hasSelection;
            const std::size_t chipCount = page->end - page->begin;
            contentChanged |= rail.chips.size() != chipCount;
            rail.chips.resize(chipCount);
            for (std::uint32_t index = page->begin; index < page->end; ++index) {
                const std::uint32_t ordinal = index - page->begin + 1;
                CandidateChip &chip = rail.chips[index - page->begin];
                const std::string key = ordinal == 10 ? "0" : std::to_string(ordinal);
                const bool keyChanged = chip.key != key;
                const bool valueChanged = chip.source != list.items[index];
                contentChanged |= keyChanged || valueChanged;
                if (keyChanged)
                    chip.key = key;
                if (valueChanged) {
                    chip.source = list.items[index];
                    chip.value.clear();
                    chip.value.reserve(chip.source.size() * 3);
                    for (std::size_t character = 0; character < chip.source.size(); ++character) {
                        std::uint32_t codepoint = chip.source[character];
                        if (codepoint >= 0xd800 && codepoint <= 0xdbff && character + 1 < chip.source.size()) {
                            const std::uint32_t low = chip.source[character + 1];
                            if (low >= 0xdc00 && low <= 0xdfff) {
                                codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                                ++character;
                            } else {
                                codepoint = 0xfffd;
                            }
                        } else if (codepoint >= 0xd800 && codepoint <= 0xdfff) {
                            codepoint = 0xfffd;
                        }
                        AppendUtf8(chip.value, codepoint);
                    }
                }
                if (fontChanged || keyChanged)
                    chip.keyWidth = TextWidth(font, fontSize, chip.key);
                if (fontChanged || valueChanged)
                    chip.valueWidth = TextWidth(font, fontSize, chip.value);
                chip.width = chip.keyWidth + chip.valueWidth + 4.0f + CandidatePaddingX * 2.0f;
                chip.selected = page->hasSelection && page->selection == index;
                rail.totalWidth += chip.width + RailItemGap;
                if (chip.selected) {
                    rail.priorityWidth = chip.width;
                    rail.widthThroughSelection = rail.totalWidth;
                }
            }
            if (rail.priorityWidth == 0.0f && !rail.chips.empty())
                rail.priorityWidth = rail.chips.front().width;

            const std::size_t additionalLists = pages.size() - 1;
            const bool statusChanged = rail.pageNumber != page->pageNumber ||
                                       rail.pageCount != page->pageCount || rail.additionalLists != additionalLists;
            contentChanged |= statusChanged;
            if (statusChanged) {
                std::string status = CandidateNavigationHint;
                status += "  ";
                status += std::to_string(page->pageNumber) + "/" + std::to_string(page->pageCount);
                if (additionalLists != 0)
                    status += "  +" + std::to_string(additionalLists);
                rail.status = std::move(status);
                rail.pageNumber = page->pageNumber;
                rail.pageCount = page->pageCount;
                rail.additionalLists = additionalLists;
            }
            if (fontChanged || statusChanged)
                rail.statusWidth = TextWidth(font, fontSize, rail.status);
            return contentChanged;
        }

        float MeasureCompositionText(std::u16string_view text, const void *context) {
            const auto &composition = *static_cast<const PreparedComposition *>(context);
            const std::size_t begin = static_cast<std::size_t>(text.data() - composition.source);
            return composition.Width(begin, begin + text.size());
        }

        void IncludeGlyphVerticalBounds(ImFontBaked &baked, float scale, std::string_view text,
                                        Layout::GlyphVerticalBounds &bounds) {
            if (text.empty())
                return;
            const char *current = text.data();
            const char *end = current + text.size();
            while (current < end) {
                unsigned int codepoint = static_cast<unsigned char>(*current);
                int consumed = 1;
                if (codepoint >= 0x80)
                    consumed = ImTextCharFromUtf8(&codepoint, current, end);
                if (consumed <= 0) {
                    ++current;
                    continue;
                }
                current += consumed;

                const ImFontGlyph *glyph = baked.FindGlyph(static_cast<ImWchar>(codepoint));
                if (!glyph || !glyph->Visible)
                    continue;

                const float minimum = glyph->Y0 * scale;
                const float maximum = glyph->Y1 * scale;
                if (!bounds.valid) {
                    bounds.minimum = minimum;
                    bounds.maximum = maximum;
                    bounds.valid = true;
                } else {
                    bounds.minimum = std::min(bounds.minimum, minimum);
                    bounds.maximum = std::max(bounds.maximum, maximum);
                }
            }
        }

        Layout::GlyphVerticalBounds MeasureRailGlyphBounds(ImFontBaked &baked, float fontSize,
                                                           const PreparedComposition &composition,
                                                           const CandidateRail &candidates) {
            Layout::GlyphVerticalBounds bounds;
            const float scale = baked.Size > 0.0f ? fontSize / baked.Size : 1.0f;
            IncludeGlyphVerticalBounds(baked, scale, composition.utf8, bounds);
            IncludeGlyphVerticalBounds(baked, scale, candidates.status, bounds);
            IncludeGlyphVerticalBounds(baked, scale, Ellipsis, bounds);
            for (const CandidateChip &chip : candidates.chips) {
                IncludeGlyphVerticalBounds(baked, scale, chip.key, bounds);
                IncludeGlyphVerticalBounds(baked, scale, chip.value, bounds);
            }
            return bounds;
        }

        struct PreparedRail {
            std::uint64_t revision = ~std::uint64_t{0};
            std::uint64_t candidateRevision = ~std::uint64_t{0};
            ImGuiContext *context = nullptr;
            ImFont *font = nullptr;
            ImGuiID bakedId = 0;
            float fontSize = 0.0f;
            float lineHeight = 0.0f;
            PreparedComposition composition;
            CandidateRail candidates;
            Layout::GlyphVerticalBounds glyphBounds;
            float compositionWidth = 0.0f;
            float desiredWidth = 0.0f;
            float ellipsisWidth = 0.0f;
            float compositionFitWidth = -1.0f;
            Layout::CompositionFit compositionFit;
            Layout::VerticalTextFit verticalFit;
            std::vector<TextRange> fittedTargetRanges;
            std::uint32_t fittedCursor = 0;
        };

        PreparedRail g_PreparedRail;

        PreparedRail &PrepareRail(const Snapshot &snapshot, std::uint64_t revision,
                                  std::uint64_t candidateRevision) {
            ImFont *font = ImGui::GetFont();
            ImGuiContext *context = ImGui::GetCurrentContext();
            const float fontSize = ImGui::GetFontSize();
            ImFontBaked *baked = font ? ImGui::GetFontBaked() : nullptr;
            if (!font || !baked) {
                g_PreparedRail.font = nullptr;
                g_PreparedRail.context = context;
                g_PreparedRail.bakedId = 0;
                return g_PreparedRail;
            }
            const bool fontChanged = g_PreparedRail.context != context || g_PreparedRail.font != font ||
                                     g_PreparedRail.bakedId != baked->BakedId || g_PreparedRail.fontSize != fontSize;
            if (g_PreparedRail.revision == revision && !fontChanged) {
                return g_PreparedRail;
            }

            g_PreparedRail.revision = revision;
            g_PreparedRail.context = context;
            g_PreparedRail.font = font;
            g_PreparedRail.bakedId = baked->BakedId;
            g_PreparedRail.fontSize = fontSize;
            g_PreparedRail.lineHeight = ImGui::GetTextLineHeight();
            const bool compositionChanged = PrepareComposition(
                snapshot.composition, *baked, fontSize, fontChanged, g_PreparedRail.composition);
            const bool candidateStateChanged = g_PreparedRail.candidateRevision != candidateRevision;
            const bool candidatesChanged = (fontChanged || candidateStateChanged)
                ? BuildCandidateRail(snapshot, font, fontSize, fontChanged, g_PreparedRail.candidates)
                : false;
            g_PreparedRail.candidateRevision = candidateRevision;
            const bool compositionLayoutChanged = compositionChanged ||
                g_PreparedRail.fittedCursor != snapshot.cursor ||
                g_PreparedRail.fittedTargetRanges != snapshot.targetRanges;
            if (compositionChanged || candidatesChanged) {
                g_PreparedRail.glyphBounds = MeasureRailGlyphBounds(
                    *baked, fontSize, g_PreparedRail.composition, g_PreparedRail.candidates);
                g_PreparedRail.verticalFit = Layout::FitTextVertically(
                    g_PreparedRail.lineHeight, RailPaddingY, g_PreparedRail.glyphBounds);
            }
            g_PreparedRail.compositionWidth = g_PreparedRail.composition.Width(0, snapshot.composition.size());
            if (fontChanged)
                g_PreparedRail.ellipsisWidth = TextWidth(font, fontSize, Ellipsis);
            g_PreparedRail.desiredWidth = RailPaddingX * 2.0f;
            if (!snapshot.composition.empty())
                g_PreparedRail.desiredWidth += g_PreparedRail.compositionWidth + RailItemGap;
            g_PreparedRail.desiredWidth += g_PreparedRail.candidates.totalWidth;
            if (!g_PreparedRail.candidates.status.empty())
                g_PreparedRail.desiredWidth += g_PreparedRail.candidates.statusWidth + RailItemGap;
            if (compositionLayoutChanged) {
                g_PreparedRail.compositionFitWidth = -1.0f;
                g_PreparedRail.fittedCursor = snapshot.cursor;
                g_PreparedRail.fittedTargetRanges = snapshot.targetRanges;
            }
            return g_PreparedRail;
        }

        void DrawTextEllipsizedHorizontally(ImDrawList *drawList, ImFont *font, float fontSize,
                                            const ImVec2 &position, float clipMinX, float clipMaxX,
                                            float clipMinY, float clipMaxY, ImU32 color,
                                            std::string_view text, float textWidth, float ellipsisWidth) {
            if (!drawList || text.empty() || clipMaxX <= clipMinX ||
                clipMaxY <= clipMinY)
                return;
            if (!font)
                return;
            const char *begin = text.data();
            const char *end = begin + text.size();
            const float availableWidth = clipMaxX - position.x;
            if (availableWidth <= 0.0f)
                return;

            const ImVec4 clipRect(clipMinX, clipMinY,
                                  clipMaxX, clipMaxY);
            if (textWidth <= availableWidth) {
                drawList->AddText(font, fontSize, position, color,
                                  begin, end, 0.0f, &clipRect);
                return;
            }

            // RenderTextEllipsis uses its text origin as the vertical fine
            // clip minimum. That cuts merged fallback glyphs whose Y0 is
            // negative, so elide horizontally while retaining the rail's
            // complete vertical clip range.
            const char *prefixEnd = begin;
            ImVec2 prefixSize(0.0f, 0.0f);
            if (ellipsisWidth < availableWidth) {
                prefixSize = font->CalcTextSizeA(
                    fontSize, availableWidth - ellipsisWidth,
                    0.0f, begin, end, &prefixEnd);
                if (prefixEnd > begin) {
                    drawList->AddText(font, fontSize, position, color,
                                      begin, prefixEnd, 0.0f, &clipRect);
                }
            }
            drawList->AddText(
                font, fontSize,
                ImVec2(position.x + prefixSize.x, position.y), color,
                Ellipsis, nullptr, 0.0f, &clipRect);
        }

        void DrawCompositionDecoration(ImDrawList *drawList,
                                       const Snapshot &snapshot,
                                       const PreparedComposition &composition,
                                       const Layout::CompositionFit &slice,
                                       const ImVec2 &textMin,
                                       const ImVec2 &textMax,
                                       ImU32 normalColor, ImU32 targetColor,
                                       ImU32 cursorColor) {
            const float underlineY = textMax.y - 1.0f;
            drawList->AddLine(ImVec2(textMin.x, underlineY),
                              ImVec2(textMax.x, underlineY), normalColor, 1.0f);

            for (const TextRange &range : snapshot.targetRanges) {
                const std::size_t begin = std::max<std::size_t>(
                    slice.begin, range.begin);
                const std::size_t end = std::min<std::size_t>(
                    slice.end, range.end);
                if (begin >= end)
                    continue;
                const float x1 = textMin.x + composition.Width(slice.begin, begin);
                const float x2 = textMin.x + composition.Width(slice.begin, end);
                drawList->AddLine(ImVec2(x1, underlineY),
                                  ImVec2(x2, underlineY),
                                  targetColor, 2.0f);
            }

            for (std::size_t index = 1;
                 index + 1 < snapshot.clauseBoundaries.size(); ++index) {
                const std::size_t boundary =
                    snapshot.clauseBoundaries[index];
                if (boundary <= slice.begin || boundary >= slice.end)
                    continue;
                const float clauseX = textMin.x + composition.Width(slice.begin, boundary);
                drawList->AddLine(ImVec2(clauseX, underlineY - 3.0f),
                                  ImVec2(clauseX, underlineY + 1.0f),
                                  normalColor, 1.0f);
            }

            const std::size_t cursor = std::min<std::size_t>(
                snapshot.cursor, snapshot.composition.size());
            if (cursor >= slice.begin && cursor <= slice.end) {
                const float cursorX = textMin.x + composition.Width(slice.begin, cursor);
                drawList->AddLine(
                    ImVec2(cursorX, textMin.y),
                    ImVec2(cursorX, textMax.y),
                    cursorColor, 1.0f);
            }
        }

        std::size_t ChooseFirstChip(const CandidateRail &candidates,
                                    float availableWidth) {
            if (!candidates.hasSelection)
                return 0;
            return candidates.widthThroughSelection <= availableWidth
                ? 0
                : candidates.selection - candidates.begin;
        }

        void DrawCandidateChips(ImDrawList *drawList, const PreparedRail &prepared,
                                float &contentX,
                                float contentY,
                                float contentMaxX,
                                float contentMinY,
                                float contentMaxY,
                                float clipMinY, float clipMaxY,
                                ImU32 textColor, ImU32 mutedColor,
                                ImU32 selectionColor) {
            const CandidateRail &candidates = prepared.candidates;
            std::size_t firstChip = ChooseFirstChip(
                candidates, std::max(0.0f, contentMaxX - contentX));
            if (firstChip != 0 && contentX < contentMaxX) {
                drawList->AddText(ImVec2(contentX, contentY), mutedColor, Ellipsis);
                contentX += prepared.ellipsisWidth + RailItemGap;
            }

            std::size_t nextChip = firstChip;
            for (; nextChip < candidates.chips.size(); ++nextChip) {
                const CandidateChip &chip = candidates.chips[nextChip];
                const float chipWidth = chip.width;
                const float remaining = contentMaxX - contentX;
                if (remaining <= RailItemGap)
                    break;

                const float visibleWidth = std::min(chipWidth, remaining);
                const ImVec2 chipMin(contentX, contentMinY - 1.0f);
                const ImVec2 chipMax(contentX + visibleWidth,
                                     contentMaxY + 1.0f);
                if (chip.selected) {
                    drawList->AddRectFilled(
                        chipMin, chipMax, selectionColor,
                        InputSurfaceStyle::Rounding);
                }

                drawList->PushClipRect(
                    ImVec2(chipMin.x, clipMinY),
                    ImVec2(chipMax.x, clipMaxY), true);
                const ImVec2 keyPos(
                    contentX + CandidatePaddingX, contentY);
                drawList->AddText(
                    keyPos, chip.selected ? textColor : mutedColor,
                    chip.key.c_str());
                const float valueX = keyPos.x + chip.keyWidth + 4.0f;
                const float valueMaxX = std::max(
                    valueX, chipMax.x - CandidatePaddingX);
                DrawTextEllipsizedHorizontally(
                    drawList, prepared.font, prepared.fontSize, ImVec2(valueX, contentY),
                    valueX, valueMaxX, clipMinY, clipMaxY, textColor,
                    chip.value, chip.valueWidth, prepared.ellipsisWidth);
                drawList->PopClipRect();
                contentX += visibleWidth + RailItemGap;
                if (visibleWidth < chipWidth) {
                    ++nextChip;
                    break;
                }
            }

            if (nextChip < candidates.chips.size()) {
                if (contentX + prepared.ellipsisWidth <= contentMaxX)
                    drawList->AddText(ImVec2(contentX, contentY), mutedColor, Ellipsis);
            }
        }
    }

    static void DrawRail(const Snapshot &snapshot, std::uint64_t revision, std::uint64_t candidateRevision,
                         const ImGuiPlatformImeData &imeData, const Placement *preferredPlacement) {
        if (!snapshot.HasContent())
            return;

        PreparedRail &prepared = PrepareRail(snapshot, revision, candidateRevision);
        if (!prepared.font || prepared.bakedId == 0)
            return;
        const PreparedComposition &composition = prepared.composition;
        const CandidateRail &candidates = prepared.candidates;
        const ImGuiStyle &style = ImGui::GetStyle();
        const float lineHeight = prepared.lineHeight;
        // A merged CJK fallback may extend above the primary font's logical
        // line origin. Fit the row to actual glyph bounds instead of assuming
        // that fixed padding can absorb that scale-dependent overhang.
        const Layout::VerticalTextFit &verticalFit = prepared.verticalFit;
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        const ImVec2 workMin(viewport->WorkPos.x + ScreenMargin,
                             viewport->WorkPos.y + ScreenMargin);
        const ImVec2 workMax(viewport->WorkPos.x + viewport->WorkSize.x -
                                 ScreenMargin,
                             viewport->WorkPos.y + viewport->WorkSize.y -
                                 ScreenMargin);
        const float workWidth = std::max(1.0f, workMax.x - workMin.x);

        const float minimumWidth = std::min(MinimumRailWidth, workWidth);
        const float fallbackLimit = std::max(
            minimumWidth,
            std::min(workWidth,
                     viewport->WorkSize.x * MaximumRailWidthRatio));
        const float preferredWidth = preferredPlacement
            ? preferredPlacement->width
            : 0.0f;
        const float windowWidth = preferredPlacement
            ? std::clamp(preferredWidth, 1.0f, workWidth)
            : std::clamp(prepared.desiredWidth, minimumWidth, fallbackLimit);
        const ImVec2 windowSize(windowWidth, verticalFit.surfaceHeight);

        const float anchorX = preferredPlacement
            ? preferredPlacement->position.x
            : imeData.InputPos.x;
        const float anchorTop = imeData.InputPos.y;
        const float anchorBottom = preferredPlacement
            ? preferredPlacement->position.y
            : imeData.InputPos.y + imeData.InputLineHeight + AnchorGap;
        const float x = std::clamp(
            anchorX, workMin.x,
            std::max(workMin.x, workMax.x - windowSize.x));
        float y = anchorBottom;
        if (!preferredPlacement && y + windowSize.y > workMax.y)
            y = anchorTop - AnchorGap - windowSize.y;
        y = std::clamp(y, workMin.y,
                       std::max(workMin.y, workMax.y - windowSize.y));

        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
        const ImVec4 railBackground =
            InputSurfaceStyle::PanelBackground();
        ImGui::PushStyleColor(ImGuiCol_WindowBg, railBackground);
        ImGui::PushStyleColor(ImGuiCol_Border,
                              style.Colors[ImGuiCol_Separator]);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,
                            InputSurfaceStyle::BorderSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                            InputSurfaceStyle::Rounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                            ImVec2(RailPaddingX, RailPaddingY));
        constexpr ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;
        if (ImGui::Begin(WindowName, nullptr, flags)) {
            ImDrawList *drawList = ImGui::GetWindowDrawList();
            const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
            const ImU32 mutedColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
            const ImU32 selectionColor = ImGui::GetColorU32(InputSurfaceStyle::SelectionColor());
            const ImU32 cursorColor = ImGui::GetColorU32(ImGuiCol_InputTextCursor);
            const ImU32 separatorColor = ImGui::GetColorU32(ImGuiCol_Separator);
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const float windowMaxY = windowPos.y + windowSize.y;
            const float contentY = windowPos.y + verticalFit.textOrigin;
            const float contentMinY = windowPos.y + verticalFit.contentMin;
            const float contentMaxY = windowPos.y + verticalFit.contentMax;
            float contentX = windowPos.x + RailPaddingX;
            const float contentMaxX = windowPos.x + windowSize.x -
                                      RailPaddingX;

            float candidatesMaxX = contentMaxX;
            if (!candidates.status.empty()) {
                const float statusWidth = candidates.statusWidth;
                if (statusWidth + RailItemGap <
                    contentMaxX - contentX) {
                    candidatesMaxX = contentMaxX - statusWidth;
                    drawList->AddText(
                        ImVec2(candidatesMaxX, contentY),
                        mutedColor,
                        candidates.status.c_str());
                    candidatesMaxX -= RailItemGap;
                }
            }

            if (!snapshot.composition.empty()) {
                const float availableWidth = std::max(
                    1.0f, candidatesMaxX - contentX);
                float maxCompositionWidth = availableWidth;
                if (!candidates.chips.empty()) {
                    const float candidateReserve = std::min(
                        candidates.priorityWidth + RailItemGap * 2.0f,
                        availableWidth * CandidateWidthReserveRatio);
                    maxCompositionWidth = std::max(
                        std::min(MinimumCompositionWidth, availableWidth),
                        availableWidth - candidateReserve);
                }
                if (std::abs(prepared.compositionFitWidth - maxCompositionWidth) >= 0.5f) {
                    prepared.compositionFit = Layout::FitComposition(
                        snapshot, maxCompositionWidth, prepared.ellipsisWidth,
                        &MeasureCompositionText, &composition);
                    prepared.compositionFitWidth = maxCompositionWidth;
                }
                const Layout::CompositionFit &slice = prepared.compositionFit;
                const float compositionWidth = std::min(
                    maxCompositionWidth, slice.width);
                const ImVec2 compositionMin(contentX, contentY);
                const ImVec2 compositionMax(
                    std::min(candidatesMaxX,
                             contentX + std::max(1.0f, compositionWidth)),
                    contentY + lineHeight);
                drawList->PushClipRect(
                    ImVec2(compositionMin.x, windowPos.y),
                    ImVec2(compositionMax.x, windowMaxY), true);
                float textX = compositionMin.x;
                if (slice.clippedBefore) {
                    drawList->AddText(ImVec2(textX, contentY), mutedColor, Ellipsis);
                    textX += prepared.ellipsisWidth;
                }
                const char *visibleBegin = composition.utf8.data() + composition.byteOffsets[slice.begin];
                const char *visibleEnd = composition.utf8.data() + composition.byteOffsets[slice.end];
                const float visibleTextWidth = composition.Width(slice.begin, slice.end);
                const ImVec2 visibleMin(textX, contentY);
                const ImVec2 visibleMax(
                    std::min(compositionMax.x, textX + visibleTextWidth),
                    contentY + lineHeight);
                drawList->AddText(prepared.font, prepared.fontSize, visibleMin, textColor,
                                  visibleBegin, visibleEnd);
                DrawCompositionDecoration(drawList, snapshot, composition, slice, visibleMin, visibleMax,
                                          mutedColor, selectionColor, cursorColor);
                if (slice.clippedAfter) {
                    drawList->AddText(
                        ImVec2(textX + visibleTextWidth, contentY), mutedColor,
                        Ellipsis);
                }
                drawList->PopClipRect();
                contentX = compositionMax.x;

                if (!candidates.chips.empty() &&
                    contentX + RailItemGap < candidatesMaxX) {
                    contentX += RailItemGap;
                    drawList->AddLine(
                        ImVec2(contentX, contentY + 2.0f),
                        ImVec2(contentX, contentY + lineHeight - 2.0f),
                        separatorColor, 1.0f);
                    contentX += RailItemGap;
                }
            }

            DrawCandidateChips(drawList, prepared, contentX, contentY, candidatesMaxX,
                               contentMinY, contentMaxY, windowPos.y, windowMaxY,
                               textColor, mutedColor, selectionColor);
        }
        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }

    void ReservePlacement(const ImVec2 &position, float width) {
        ImGuiContext *context = ImGui::GetCurrentContext();
        if (!context || width <= 0.0f)
            return;
        g_Placement.value.position = position;
        g_Placement.value.width = width;
        g_Placement.context = context;
        g_Placement.frame = ImGui::GetFrameCount();
    }

    void Draw(const ImGuiPlatformImeData &imeData) {
        if (!Runtime::PreparePresentationFrame(imeData.WantVisible, g_Frame)) {
            if (imeData.WantVisible)
                return;
            g_Placement.context = nullptr;
            g_Placement.frame = -1;
            return;
        }

        if (HandleCandidateShortcuts(g_Frame.snapshot) &&
            !Runtime::PreparePresentationFrame(true, g_Frame))
            return;

        const Placement *placement = g_Placement.context == ImGui::GetCurrentContext() &&
                                     g_Placement.frame == ImGui::GetFrameCount()
            ? &g_Placement.value
            : nullptr;
        DrawRail(g_Frame.snapshot, g_Frame.revision, g_Frame.candidateRevision, imeData, placement);
    }

    bool IsActive() {
        return Runtime::IsPresentationActive();
    }
}

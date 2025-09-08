#include "AnsiText.h"

#include <cmath>
#include <algorithm>
#include <utf8.h>

#include "AnsiPalette.h"

namespace AnsiText {
    // =============================================================================
    // ConsoleColor Implementation
    // =============================================================================

    ConsoleColor ConsoleColor::GetRendered() const {
        ConsoleColor r = *this;
        if (reverse) {
            // Swap FG/BG; if BG had zero alpha, synthesize a fallback from style so FG won't vanish.
            ImU32 bg = r.background;
            const int a = (bg >> IM_COL32_A_SHIFT) & 0xFF;
            if (a == 0) {
                const ImVec4 c = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
                bg = ImGui::GetColorU32(c);
            }
            ImU32 fg = r.foreground;
            r.foreground = bg;
            r.background = fg;
            // Keep palette indices consistent with swapped channels
            std::swap(r.fgIsAnsi256, r.bgIsAnsi256);
            std::swap(r.fgAnsiIndex, r.bgAnsiIndex);
        }
        if (hidden) {
            // Render text as background color (maintain alpha), but don't force full invisibility.
            r.foreground = r.background;
        }
        return r;
    }

    // =============================================================================
    // AnsiString Implementation
    // =============================================================================

    AnsiString::AnsiString(const char *text) {
        SetText(text);
    }

    void AnsiString::SetText(const char *text) {
        if (!text) return;

        m_OriginalText = text;
        m_Segments.clear();

        ParseAnsiEscapeCodes();
    }

    void AnsiString::Clear() {
        m_OriginalText.clear();
        m_Segments.clear();
    }

    void AnsiString::ParseAnsiEscapeCodes() {
        m_Segments.clear();

        if (m_OriginalText.empty()) {
            return;
        }

        const char* p = m_OriginalText.c_str();
        ConsoleColor currentColor;
        std::string currentSegment;

        while (*p) {
            if (*p == '\033' && *(p + 1) == '[') {
                // Save current text segment
                if (!currentSegment.empty()) {
                    m_Segments.emplace_back(std::move(currentSegment), currentColor);
                    currentSegment.clear();
                }

                // Parse escape sequence
                p += 2; // Skip \033[
                std::string sequence;

                // Only parse color/style sequences (ending with 'm')
                while (*p && *p != 'm' && sequence.length() < 50) {
                    sequence += *p++;
                }

                if (*p == 'm') {
                    p++; // Skip the 'm'
                    currentColor = ParseAnsiColorSequence(sequence, currentColor);
                } else {
                    // Invalid sequence - treat as literal text
                    currentSegment += '\033';
                    currentSegment += '[';
                    currentSegment += sequence;
                }
            } else {
                currentSegment += *p++;
            }
        }

        // Add final segment
        if (!currentSegment.empty()) {
            m_Segments.emplace_back(std::move(currentSegment), currentColor);
        }

        // Ensure at least one segment exists
        if (m_Segments.empty()) {
            m_Segments.emplace_back(m_OriginalText, ConsoleColor());
        }
    }

    ConsoleColor AnsiString::ParseAnsiColorSequence(const std::string &sequence, const ConsoleColor &currentColor) {
        ConsoleColor color = currentColor;
        if (sequence.empty()) return color;

        // Parse semicolon-separated codes
        std::vector<int> codes;
        size_t start = 0;
        for (size_t pos = 0; pos <= sequence.length(); ++pos) {
            if (pos == sequence.length() || sequence[pos] == ';') {
                if (pos > start) {
                    std::string codeStr = sequence.substr(start, pos - start);
                    if (!codeStr.empty() && std::all_of(codeStr.begin(), codeStr.end(), ::isdigit)) {
                        int code = std::stoi(codeStr);
                        if (code >= 0) {
                            codes.push_back(code);
                        }
                    }
                }
                start = pos + 1;
            }
        }

        // Process each color code
        for (size_t i = 0; i < codes.size(); ++i) {
            int code = codes[i];

            if (code == 0) {
                // Reset all
                color = ConsoleColor();
            } else if (code >= 30 && code <= 37) {
                // Standard foreground colors -> mark palette index 0..7
                color.fgIsAnsi256 = true; color.fgAnsiIndex = code - 30;
            } else if (code >= 40 && code <= 47) {
                // Standard background colors -> mark palette index 0..7
                color.bgIsAnsi256 = true; color.bgAnsiIndex = code - 40;
            } else if (code >= 90 && code <= 97) {
                // Bright foreground colors -> mark palette index 8..15
                color.fgIsAnsi256 = true; color.fgAnsiIndex = (code - 90) + 8;
            } else if (code >= 100 && code <= 107) {
                // Bright background colors -> mark palette index 8..15
                color.bgIsAnsi256 = true; color.bgAnsiIndex = (code - 100) + 8;
            } else if ((code == 38 || code == 48) && i + 1 < codes.size()) {
                // Extended color codes
                bool isBackground = (code == 48);
                if (codes[i + 1] == 5 && i + 2 < codes.size()) {
                    // 256-color mode
                    if (isBackground) {
                        color.bgIsAnsi256 = true;
                        color.bgAnsiIndex = codes[i + 2];
                    } else {
                        color.fgIsAnsi256 = true;
                        color.fgAnsiIndex = codes[i + 2];
                    }
                    i += 2;
                } else if (codes[i + 1] == 2 && i + 4 < codes.size()) {
                    // RGB mode
                    ImU32 colorValue = GetRgbColor(codes[i + 2], codes[i + 3], codes[i + 4]);
                    if (isBackground) {
                        color.background = colorValue;
                        color.bgIsAnsi256 = false; color.bgAnsiIndex = -1;
                    } else {
                        color.foreground = colorValue;
                        color.fgIsAnsi256 = false; color.fgAnsiIndex = -1;
                    }
                    i += 4;
                }
            } else if (code == 39) {
                // Default foreground color
                color.foreground = IM_COL32_WHITE;
                color.fgIsAnsi256 = false; color.fgAnsiIndex = -1;
            } else if (code == 49) {
                // Default background color
                color.background = IM_COL32(0, 0, 0, 0);
                color.bgIsAnsi256 = false; color.bgAnsiIndex = -1;
            } else {
                // Style codes
                switch (code) {
                case 1: color.bold = true; break;
                case 2: color.dim = true; break;
                case 3: color.italic = true; break;
                case 4: color.underline = true; break;
                // 5/6: blink (unsupported/no-op)
                case 5: case 6: break;
                case 7: color.reverse = true; break;
                case 8: color.hidden = true; break;
                case 9: color.strikethrough = true; break;
                case 21: case 22: color.bold = false; color.dim = false; break;
                case 23: color.italic = false; break;
                case 24: color.underline = false; break;
                // 25: blink off (unsupported/no-op)
                case 25: break;
                case 27: color.reverse = false; break;
                case 28: color.hidden = false; break;
                case 29: color.strikethrough = false; break;
                default: break; // Ignore unknown codes
                }
            }
        }

        return color;
    }

    ImU32 AnsiString::GetRgbColor(int r, int g, int b) {
        return IM_COL32(
            std::clamp(r, 0, 255),
            std::clamp(g, 0, 255),
            std::clamp(b, 0, 255),
            255
        );
    }

    // =============================================================================
    // Layout Implementation
    // =============================================================================

    const char *Layout::Utf8Next(const char *s, const char *end) {
        if (!s || s >= end) return s;
        utf8_int32_t cp = 0;
        const char *next = (const char *) utf8codepoint((const utf8_int8_t *) s, &cp);
        if (!next) return s + 1;
        return next > end ? end : next;
    }

    float Layout::Measure(ImFont *font, float fontSize, const char *b, const char *e) {
        if (b == e) return 0.0f;
        return font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, b, e, nullptr).x;
    }

    void Layout::BuildLines(const std::vector<TextSegment> &segments, float wrapWidth, int tabColumns, float fontSize, std::vector<Line> &outLines) {
        outLines.clear();
        if (wrapWidth <= 0.0f) wrapWidth = FLT_MAX;

        ImFont* font = ImGui::GetFont();
        const char* kSpace = " ";
        const float spaceW = Measure(font, fontSize, kSpace, kSpace + 1);

        Line line;
        float x = 0.0f;
        bool trimLeadingSpace = false;

        auto NewLine = [&]() {
            outLines.push_back(std::move(line));
            line = Line();
            x = 0.0f;
        };
        auto EmitSpan = [&](const TextSegment *seg, const char *b, const char *e, float w, bool isTab) {
            line.spans.push_back(Span{seg, b, e, w, isTab});
            x += w;
            if (x > line.width) line.width = x;
        };

        for (const TextSegment &seg : segments) {
            const char* p = seg.text.c_str();
            const char* end = p + seg.text.size();
            while (p < end) {
                if (*p == '\n') { NewLine(); trimLeadingSpace = false; ++p; continue; }
                if (*p == '\r') { x = 0.0f; ++p; continue; }
                if (*p == '\t') {
                    const float cols = (tabColumns > 0 ? (float) tabColumns : (float) kDefaultTabColumns);
                    const float tabW = spaceW * cols;
                    const float nextTab = (static_cast<int>(x / tabW) + 1) * tabW;
                    const float w = nextTab - x;
                    if (x > 0.0f && (x + w) > wrapWidth + 0.0001f) { NewLine(); trimLeadingSpace = true; }
                    EmitSpan(&seg, p, p /*empty*/, w, true);
                    ++p; continue;
                }

                // Tokenize: spaces vs non-spaces
                const char *t = p;
                utf8_int32_t c = 0;
                const char *tNext = (const char *) utf8codepoint((const utf8_int8_t *) t, &c);
                if (!tNext || tNext <= t) tNext = t + 1;
                bool tokenIsSpace = (c == ' ' || c == 0x3000);
                const char* q = p;
                if (tokenIsSpace) {
                    while (q < end) {
                        utf8_int32_t sc = 0;
                        const char *nn = (const char*) utf8codepoint((const utf8_int8_t*) q, &sc);
                        if (!nn || nn <= q) { q++; break; }
                        if (!(sc == ' ' || sc == 0x3000)) break; q = nn;
                    }
                } else {
                    while (q < end) {
                        const char *next = Utf8Next(q, end);
                        if (next <= q) { q++; break; }
                        utf8_int32_t cc = 0; (void)utf8codepoint((const utf8_int8_t*)q, &cc);
                        if (cc == ' ' || cc == 0x3000 || cc == '\t' || cc == '\n' || cc == '\r') break;
                        q = next;
                    }
                }

                if (x == 0.0f && tokenIsSpace && trimLeadingSpace) { p = q; continue; }

                float w = Measure(font, fontSize, p, q);
                auto WrapLine = [&]() { if (!line.spans.empty()) NewLine(); };
                float avail = wrapWidth - x;
                if (w <= avail + 0.0001f) { EmitSpan(&seg, p, q, w, false); p = q; continue; }

                if (x > 0.0f) {
                    WrapLine(); trimLeadingSpace = true; if (tokenIsSpace) { p = q; continue; } avail = wrapWidth;
                }

                // Split long token
                const char* start = p;
                while (start < q) {
                    const char* cur = start; const char* lastFit = start;
                    while (cur < q) {
                        const char* next = Utf8Next(cur, q); if (next <= cur) { next = cur + 1; }
                        float ww = Measure(font, fontSize, start, next);
                        if (ww <= avail + 0.0001f) { lastFit = next; cur = next; } else { break; }
                    }
                    if (lastFit == start) {
                        const char* one = Utf8Next(start, q); if (one <= start) one = start + 1;
                        float ww = Measure(font, fontSize, start, one);
                        EmitSpan(&seg, start, one, ww, false); start = one;
                    } else {
                        float ww = Measure(font, fontSize, start, lastFit);
                        EmitSpan(&seg, start, lastFit, ww, false); start = lastFit;
                    }
                    if (start < q) { NewLine(); trimLeadingSpace = true; avail = wrapWidth; }
                }
                p = q;
            }
        }

        if (!line.spans.empty() || outLines.empty()) outLines.push_back(std::move(line));
    }

    // =============================================================================
    // Color Implementation
    // =============================================================================

    ImU32 Color::ApplyDim(ImU32 color) {
        const ImU32 r = ((color >> IM_COL32_R_SHIFT) & 0xFF) / 2;
        const ImU32 g = ((color >> IM_COL32_G_SHIFT) & 0xFF) / 2;
        const ImU32 b = ((color >> IM_COL32_B_SHIFT) & 0xFF) / 2;
        const ImU32 a = (color >> IM_COL32_A_SHIFT) & 0xFF;
        return IM_COL32(r, g, b, a);
    }

    ImU32 Color::ApplyAlpha(ImU32 color, float alpha) {
        const ImU32 originalAlpha = (color >> IM_COL32_A_SHIFT) & 0xFF;
        if (originalAlpha == 0) return ImU32(0);
        const ImU32 newAlpha = (ImU32) (std::clamp(alpha, 0.0f, 1.0f) * (float) originalAlpha);
        return (color & 0x00FFFFFF) | (newAlpha << IM_COL32_A_SHIFT);
    }

    // =============================================================================
    // Metrics Implementation
    // =============================================================================

    float Metrics::UnderlineY(float lineTop, float fontSize) {
        ImFont *font = ImGui::GetFont();
        const float usedSize = (fontSize > 0.0f) ? fontSize : ImGui::GetFontSize();
        ImFontBaked *baked = font ? font->GetFontBaked(usedSize) : nullptr;

        const float ascent = baked ? std::max(0.0f, baked->Ascent) : usedSize * 0.8f;
        const float descent_mag = baked ? std::max(0.0f, -baked->Descent) : usedSize * 0.2f;
        const float baseline = lineTop + ascent;
        const float offset = std::clamp(descent_mag * 0.5f, 1.0f, std::max(1.0f, descent_mag - 1.0f));
        return baseline + offset;
    }

    float Metrics::StrikeY(float lineTop, float fontSize) {
        ImFont *font = ImGui::GetFont();
        const float usedSize = (fontSize > 0.0f) ? fontSize : ImGui::GetFontSize();
        ImFontBaked *baked = font ? font->GetFontBaked(usedSize) : nullptr;
        const float ascent = baked ? std::max(0.0f, baked->Ascent) : usedSize * 0.8f;
        return lineTop + ascent * 0.6f;
    }

    float Metrics::Thickness(float fontSize) {
        float t = std::round(fontSize / 18.0f);
        return std::clamp(t, 1.0f, 4.0f);
    }

    float CalculateHeight(const AnsiString &text, float wrapWidth, float fontSize, int tabColumns) {
        if (text.IsEmpty()) {
            return ImGui::GetTextLineHeightWithSpacing();
        }

        const float usedFontSize = (fontSize > 0.0f) ? fontSize : ImGui::GetFontSize();
        const float lineH = ImGui::GetTextLineHeightWithSpacing();

        std::vector<Layout::Line> lines;
        Layout::BuildLines(text.GetSegments(), wrapWidth, tabColumns, usedFontSize, lines);

        const float lineCount = static_cast<float>(lines.size());
        return std::max(lineH, lineCount * lineH);
    }

    ImVec2 CalculateSize(const AnsiString &text, float wrapWidth, float fontSize, int tabColumns) {
        if (text.IsEmpty()) {
            return ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing());
        }

        const float usedFontSize = (fontSize > 0.0f) ? fontSize : ImGui::GetFontSize();
        const float lineH = ImGui::GetTextLineHeightWithSpacing();

        std::vector<Layout::Line> lines;
        Layout::BuildLines(text.GetSegments(), wrapWidth, tabColumns, usedFontSize, lines);

        float maxWidth = 0.0f;
        for (const auto &line : lines) {
            maxWidth = std::max(maxWidth, line.width);
        }

        const float height = std::max(lineH, (float) lines.size() * lineH);
        return ImVec2(maxWidth, height);
    }

    // =============================================================================
    // Renderer Implementation
    // =============================================================================

    constexpr float kItalicShearBase = 0.16f;
    constexpr float kItalicShearSizeMin = 12.0f;
    constexpr float kItalicShearSizeMax = 36.0f;
    constexpr float kItalicShearFactorMin = 0.85f;
    constexpr float kItalicShearFactorMax = 1.20f;

    Renderer::BoldParams &Renderer::DefaultBold() {
        static BoldParams k;
        return k;
    }

    AnsiPalette &Renderer::DefaultPalette() {
        static AnsiPalette palette;
        return palette;
    }

    float Renderer::ComputeItalicShear(float fontSize) {
        const float fs = std::clamp(fontSize, kItalicShearSizeMin, kItalicShearSizeMax);
        const float t = (fs - kItalicShearSizeMin) / (kItalicShearSizeMax - kItalicShearSizeMin);
        const float factor = kItalicShearFactorMin + (kItalicShearFactorMax - kItalicShearFactorMin) * t;
        return kItalicShearBase * factor;
    }

    float Renderer::ComputeBoldOffsetScale(float fontSize, const BoldParams &bp) {
        const float fs = std::clamp(fontSize, bp.sizeMinPx, bp.sizeMaxPx);
        const float t = (fs - bp.sizeMinPx) / (bp.sizeMaxPx - bp.sizeMinPx + 1e-5f);
        return bp.offsetScaleMin + (bp.offsetScaleMax - bp.offsetScaleMin) * t;
    }

    static void BuildBoldOffsets(float pixelOffset, int rings, bool includeDiag, std::vector<ImVec2> &out) {
        out.clear();
        auto push4 = [&](float d) {
            out.emplace_back(+d, 0.0f);
            out.emplace_back(-d, 0.0f);
            out.emplace_back(0.0f, +d);
            out.emplace_back(0.0f, -d);
        };
        auto pushDiag = [&](float d) {
            const float dd = d * 0.70710678f;
            out.emplace_back(+dd, +dd);
            out.emplace_back(-dd, +dd);
            out.emplace_back(+dd, -dd);
            out.emplace_back(-dd, -dd);
        };
        for (int r = 1; r <= std::max(1, rings); ++r) {
            float d = pixelOffset * r;
            push4(d);
            if (includeDiag) pushDiag(d);
        }
    }

    void Renderer::AddTextStyled(ImDrawList *drawList, ImFont *font, float fontSize, const ImVec2 &pos, ImU32 col,
                                 const char *begin, const char *end, bool italic, bool fauxBold) {
        AddTextStyledEx(drawList, font, fontSize, pos, col, begin, end, italic, fauxBold, DefaultBold());
    }

    void Renderer::AddTextStyledEx(ImDrawList *drawList, ImFont *font, float fontSize, const ImVec2 &pos, ImU32 col,
                                   const char *begin, const char *end, bool italic, bool fauxBold, const BoldParams &bp) {
        const float usedSize = (fontSize > 0.0f) ? fontSize : ImGui::GetFontSize();

        auto drawOncePlain = [&](const ImVec2 &p, ImU32 c) {
            drawList->AddText(font, usedSize, p, c, begin, end);
        };

        auto drawOnceItalic = [&](const ImVec2 &p, ImU32 c) {
            if ((c & IM_COL32_A_MASK) == 0) return;
            ImFontBaked *baked = font ? font->GetFontBaked(usedSize) : nullptr;
            if (!baked) {
                drawOncePlain(p, c);
                return;
            }

            const float scale = (usedSize >= 0.0f) ? (usedSize / baked->Size) : 1.0f;
            const float x0 = p.x, y0 = p.y;
            const float shear = ComputeItalicShear(usedSize);
            const float ascent = std::max(0.0f, baked->Ascent);
            const float anchorY = y0 + ascent;

            drawList->PushTextureID(font->ContainerAtlas->TexRef);

            const char *s = begin;
            float x = x0;
            while (s < end) {
                utf8_int32_t code = 0;
                const char *next = (const char *) utf8codepoint((const utf8_int8_t *) s, &code);
                if (!next || next <= s) next = s + 1;
                const ImFontGlyph *g = baked->FindGlyph((ImWchar) code);
                if (g && g->Visible) {
                    float x1 = x + g->X0 * scale, x2 = x + g->X1 * scale;
                    float y1 = y0 + g->Y0 * scale, y2 = y0 + g->Y1 * scale;
                    float u1 = g->U0, v1 = g->V0, u2 = g->U1, v2 = g->V1;
                    const float dx1 = shear * (anchorY - y1);
                    const float dx2 = shear * (anchorY - y2);
                    drawList->PrimReserve(6, 4);
                    drawList->PrimQuadUV(
                        ImVec2(x1 + dx1, y1), ImVec2(x2 + dx1, y1),
                        ImVec2(x2 + dx2, y2), ImVec2(x1 + dx2, y2),
                        ImVec2(u1, v1), ImVec2(u2, v1), ImVec2(u2, v2), ImVec2(u1, v2), c);
                    x += g->AdvanceX * scale;
                } else {
                    x += baked->GetCharAdvance((ImWchar) code) * scale;
                }
                s = next;
            }

            drawList->PopTextureID();
        };

        auto draw_once = [&](const ImVec2 &p, ImU32 c) {
            if (!italic) drawOncePlain(p, c);
            else drawOnceItalic(p, c);
        };

        draw_once(pos, col);

        if (!fauxBold) return;

        const float ofsScale = ComputeBoldOffsetScale(usedSize, bp);
        float pxOffset = bp.baseOffsetPx * ofsScale;
        pxOffset = std::clamp(pxOffset, 0.30f, 0.60f);
        if (usedSize <= 14.0f) // Small font, reduce bold offset
            pxOffset = std::max(0.35f, pxOffset * 0.85f);
        std::vector<ImVec2> offsets;
        BuildBoldOffsets(pxOffset, bp.rings, bp.includeDiagonals, offsets);

        const float baseAlpha = ((col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
        float ringAlpha = std::clamp(baseAlpha * bp.alphaScale, 0.0f, 1.0f);
        if (usedSize <= 14.0f) // Small font, reduce bold alpha
            ringAlpha *= 0.85f;
        int perRing = bp.includeDiagonals ? 8 : 4;
        if (perRing <= 0) perRing = 4;

        for (int i = 0; i < (int) offsets.size(); ++i) {
            const int ringIndex = i / perRing;
            float alphaForThis = ringAlpha * std::pow(std::max(0.0f, bp.alphaDecay), (float) ringIndex);
            ImU32 a = (ImU32) std::round(std::clamp(alphaForThis, 0.0f, 1.0f) * 255.0f);
            ImU32 colHalf = (col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
            draw_once(ImVec2(pos.x + offsets[i].x, pos.y + offsets[i].y), colHalf);
        }
    }

    void Renderer::DrawText(ImDrawList *drawList, const AnsiString &text, const ImVec2 &startPos, float wrapWidth,
                      float alpha, float fontSize, int tabColumns, const AnsiPalette *palette) {
        if (!palette)
            palette = &DefaultPalette();

        ImVec2 cursor = startPos;

        // Unify font size and baked data
        ImFont *font = ImGui::GetFont();
        const float usedFontSize = (fontSize > 0.0f) ? fontSize : ImGui::GetFontSize();
        ImFontBaked *baked = font ? font->GetFontBaked(usedFontSize) : nullptr;

        // Pre-fetch line metrics
        const float ascent = baked ? std::max(0.0f, baked->Ascent) : usedFontSize * 0.8f;
        const float descentMag = baked ? std::max(0.0f, -baked->Descent) : usedFontSize * 0.2f;
        const float lineStep = ImGui::GetTextLineHeightWithSpacing();
        const float italicShear = Renderer::ComputeItalicShear(usedFontSize);

        // Layout
        std::vector<Layout::Line> lines;
        Layout::BuildLines(text.GetSegments(), wrapWidth, tabColumns, usedFontSize, lines);

        for (const auto &line : lines) {
            const float lineTop = cursor.y;
            const float lineBottom = cursor.y + ascent + descentMag;

            // Pass 1: Merge and draw background runs
            struct BgRun {
                float x0, x1;
                ImU32 col;
            };
            std::vector<BgRun> runs;
            float xForBg = startPos.x;
            bool hasOpenRun = false;
            BgRun cur{};

            for (const auto &sp : line.spans) {
                const TextSegment *seg = sp.seg;

                if (!seg) {
                    xForBg += sp.width;
                    continue;
                }

                ConsoleColor rc = seg->color.GetRendered();

                ImU32 bgBase = rc.background;
                if (rc.bgIsAnsi256 && rc.bgAnsiIndex >= 0) {
                    const_cast<AnsiPalette *>(palette)->EnsureInitialized();
                    if (palette->IsActive()) palette->GetColor(rc.bgAnsiIndex, bgBase);
                }

                ImU32 bg = Color::ApplyAlpha(bgBase, alpha);
                const bool drawBg = (!sp.isTab) && (((bg >> IM_COL32_A_SHIFT) & 0xFF) != 0) && (sp.b < sp.e);

                if (drawBg) {
                    const float x0 = xForBg;
                    float italicPad = 0.0f;
                    if (rc.italic) {
                        italicPad = std::max(0.0f, italicShear) * (lineBottom - lineTop);
                    }
                    const float x1 = xForBg + sp.width + italicPad;

                    if (hasOpenRun && cur.col == bg && std::abs(cur.x1 - x0) <= 0.25f) {
                        cur.x1 = x1; // Merge with current run
                    } else {
                        if (hasOpenRun) runs.push_back(cur);
                        cur = BgRun{x0, x1, bg};
                        hasOpenRun = true;
                    }
                } else {
                    if (hasOpenRun) {
                        runs.push_back(cur);
                        hasOpenRun = false;
                    }
                }
                xForBg += sp.width;
            }
            if (hasOpenRun) runs.push_back(cur);

            // Draw merged background runs
            for (const BgRun &r : runs) {
                drawList->AddRectFilled(ImVec2(r.x0, lineTop), ImVec2(r.x1, lineBottom), r.col);
            }

            // Pass 2: Text and decoration lines
            float x = startPos.x;
            for (const auto &sp : line.spans) {
                const TextSegment *seg = sp.seg;
                if (!seg) {
                    x += sp.width;
                    continue;
                }

                ConsoleColor rc = seg->color.GetRendered();

                ImU32 fg = rc.foreground;
                if (rc.fgIsAnsi256 && rc.fgAnsiIndex >= 0) {
                    const_cast<AnsiPalette *>(palette)->EnsureInitialized();
                    if (palette->IsActive()) palette->GetColor(rc.fgAnsiIndex, fg);
                }
                if (rc.dim) fg = Color::ApplyDim(fg);
                if (rc.hidden) {
                    ImU32 bgBase2 = rc.background;
                    if (rc.bgIsAnsi256 && rc.bgAnsiIndex >= 0) {
                        const_cast<AnsiPalette *>(palette)->EnsureInitialized();
                        if (palette->IsActive()) palette->GetColor(
                            rc.bgAnsiIndex, bgBase2);
                    }
                    fg = Color::ApplyAlpha(bgBase2, alpha);
                }

                fg = Color::ApplyAlpha(fg, alpha);
                const bool fauxBold = rc.bold;
                const bool italic = rc.italic;

                if (!sp.isTab && sp.b < sp.e) {
                    Renderer::AddTextStyled(drawList, font, usedFontSize, ImVec2(x, lineTop), fg, sp.b, sp.e, italic, fauxBold);

                    if (rc.underline) {
                        float y = Metrics::UnderlineY(lineTop, usedFontSize);
                        float th = Metrics::Thickness(usedFontSize);
                        const float italicPad = (rc.italic) ? std::max(0.0f, italicShear) * (lineBottom - lineTop) : 0.0f;
                        if (fmodf(th, 2.0f) != 0.0f) y = floorf(y) + 0.5f;
                        else y = roundf(y);
                        drawList->AddLine(ImVec2(x, y), ImVec2(x + sp.width + italicPad, y), fg, th);
                    }
                    if (rc.strikethrough) {
                        float y = Metrics::StrikeY(lineTop, usedFontSize);
                        float th = Metrics::Thickness(usedFontSize);
                        const float italicPad = (rc.italic) ? std::max(0.0f, italicShear) * (lineBottom - lineTop) : 0.0f;
                        if (fmodf(th, 2.0f) != 0.0f) y = floorf(y) + 0.5f;
                        else y = roundf(y);
                        drawList->AddLine(ImVec2(x, y), ImVec2(x + sp.width + italicPad, y), fg, th);
                    }
                }

                x += sp.width;
            }

            cursor.y += lineStep;
        }
    }
} // namespace AnsiText

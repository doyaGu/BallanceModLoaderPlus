#include "MessageBoard.h"

#include <cmath>
#include <cstdarg>
#include <algorithm>

#include <utf8.h>

#include "imgui_internal.h"
#include "ModContext.h"

namespace {
    // Default constants (can be overridden by member settings)
    constexpr int kDefaultTabColumns = 4;  // default 4-space tab stops
    constexpr float kScrollEpsilon = 0.5f; // tolerance for bottom checks

    // Grouped helpers for layout, colors, metrics and rendering.
    namespace MsgText {
        using Seg = MessageBoard::TextSegment;

        struct Layout {
            struct Span { const Seg *seg; const char *b; const char *e; float width; bool isTab; };
            struct Line { std::vector<Span> spans; float width = 0.0f; };

            static const char *Utf8Next(const char *s, const char *end) {
                if (!s || s >= end) return s;
                utf8_int32_t cp = 0;
                const char *next = (const char *) utf8codepoint((const utf8_int8_t *) s, &cp);
                if (!next) return s + 1; // fail-safe advance
                return next > end ? end : next;
            }
            static float Measure(ImFont *font, float fontSize, const char *b, const char *e) {
                if (b == e) return 0.0f;
                return font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, b, e, nullptr).x;
            }
            // Greedy wrap into lines of spans.
            static void BuildLines(const std::vector<Seg> &segments, float wrapWidth, int tabColumns, std::vector<Line> &outLines) {
                outLines.clear();
                if (wrapWidth <= 0.0f) wrapWidth = FLT_MAX;

                ImFont *font = ImGui::GetFont();
                const float fontSize = ImGui::GetFontSize();
                const float spaceW = ImGui::CalcTextSize(" ").x;

                Line line;
                float x = 0.0f;
                bool trimLeadingSpace = false; // only after a soft-wrap

                auto NewLine = [&]() {
                    outLines.push_back(std::move(line));
                    line = Line();
                    x = 0.0f;
                };
                auto EmitSpan = [&](const Seg *seg, const char *b, const char *e, float w, bool isTab) {
                    line.spans.push_back(Span{seg, b, e, w, isTab});
                    x += w;
                    if (x > line.width) line.width = x;
                };

                for (const Seg &seg : segments) {
                    const char *p = seg.text.c_str();
                    const char *end = p + seg.text.size();
                    while (p < end) {
                        if (*p == '\n') { NewLine(); trimLeadingSpace = false; ++p; continue; }
                        if (*p == '\r') { x = 0.0f; ++p; continue; }
                        if (*p == '\t') {
                            const float cols = (tabColumns > 0 ? (float)tabColumns : (float)kDefaultTabColumns);
                            const float tabW = spaceW * cols;
                            const float nextTab = (static_cast<int>(x / tabW) + 1) * tabW;
                            const float w = nextTab - x;
                            if (x > 0.0f && (x + w) > wrapWidth + 0.0001f) { NewLine(); trimLeadingSpace = true; }
                            EmitSpan(&seg, p, p /*empty*/, w, true);
                            ++p; continue;
                        }

                        // Build next token: run of spaces or non-spaces
                        const char *t = p; utf8_int32_t c = 0; const char *tNext = (const char*)utf8codepoint((const utf8_int8_t*)t, &c);
                        if (!tNext || tNext <= t) tNext = t + 1;
                        bool tokenIsSpace = (c == ' ' || c == 0x3000);
                        const char *q = p;
                        if (tokenIsSpace) {
                            while (q < end) {
                                utf8_int32_t sc = 0; const char *nn = (const char*)utf8codepoint((const utf8_int8_t*)q, &sc);
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

                        // Split inside long token
                        const char *start = p;
                        while (start < q) {
                            const char *cur = start; const char *lastFit = start;
                            while (cur < q) {
                                const char *next = Utf8Next(cur, q); if (next <= cur) { next = cur + 1; }
                                float ww = Measure(font, fontSize, start, next);
                                if (ww <= avail + 0.0001f) { lastFit = next; cur = next; } else { break; }
                            }
                            if (lastFit == start) {
                                const char *one = Utf8Next(start, q); if (one <= start) one = start + 1;
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
        };

        struct Color {
            static ImU32 ApplyDim(ImU32 color) {
                const ImU32 r = ((color >> IM_COL32_R_SHIFT) & 0xFF) / 2;
                const ImU32 g = ((color >> IM_COL32_G_SHIFT) & 0xFF) / 2;
                const ImU32 b = ((color >> IM_COL32_B_SHIFT) & 0xFF) / 2;
                const ImU32 a = (color >> IM_COL32_A_SHIFT) & 0xFF;
                return IM_COL32(r, g, b, a);
            }
            static ImU32 ApplyAlpha(ImU32 color, float alpha) {
                const ImU32 originalAlpha = (color >> IM_COL32_A_SHIFT) & 0xFF;
                if (originalAlpha == 0) return ImU32(0);
                const ImU32 newAlpha = (ImU32)(std::clamp(alpha, 0.0f, 1.0f) * (float)originalAlpha);
                return (color & 0x00FFFFFF) | (newAlpha << IM_COL32_A_SHIFT);
            }
        };

        struct Metrics {
            static float UnderlineY(float lineTop, float fontSize) {
                ImFont *font = ImGui::GetFont();
                ImFontBaked *baked = font ? font->GetFontBaked(fontSize) : nullptr;
                const float ascent = baked ? ImMax(0.0f, baked->Ascent) : fontSize * 0.8f;
                const float descent_mag = baked ? ImMax(0.0f, -baked->Descent) : fontSize * 0.2f;
                const float baseline = lineTop + ascent;
                const float offset = std::clamp(descent_mag * 0.5f, 1.0f, ImMax(1.0f, descent_mag - 1.0f));
                return baseline + offset;
            }
            static float StrikeY(float lineTop, float fontSize) {
                ImFont *font = ImGui::GetFont();
                ImFontBaked *baked = font ? font->GetFontBaked(fontSize) : nullptr;
                const float ascent = baked ? ImMax(0.0f, baked->Ascent) : fontSize * 0.8f;
                return lineTop + ascent * 0.55f;
            }
            static float Thickness(float fontSize) {
                float t = std::round(fontSize / 18.0f);
                return std::clamp(t, 1.0f, 4.0f);
            }
        };

        struct Renderer {
            // Base shear for faux-italic (clockwise). Adjusted slightly by font size.
            static constexpr float kItalicShearBase = 0.16f; // ~9.1° at reference size
            // Easy-to-tune internal parameters for size-adaptive shear
            static constexpr float kItalicShearSizeMin = 12.0f;  // px
            static constexpr float kItalicShearSizeMax = 36.0f;  // px
            static constexpr float kItalicShearFactorMin = 0.85f; // at min size
            static constexpr float kItalicShearFactorMax = 1.20f; // at max size

            // Size-adaptive shear: dampen at small sizes to reduce aliasing, gently boost at large sizes.
            // Reference span: 12..36 px -> factor 0.85..1.20
            static float ComputeItalicShear(float fontSize) {
                const float fs = std::clamp(fontSize, kItalicShearSizeMin, kItalicShearSizeMax);
                const float t = (fs - kItalicShearSizeMin) / (kItalicShearSizeMax - kItalicShearSizeMin); // 0..1
                const float factor = kItalicShearFactorMin + (kItalicShearFactorMax - kItalicShearFactorMin) * t;
                return kItalicShearBase * factor;
            }

            static void AddTextStyled(ImDrawList *drawList, ImFont *font, float fontSize, const ImVec2 &pos, ImU32 col,
                                      const char *begin, const char *end, bool italic, bool fauxBold) {
                if (!italic) {
                    drawList->AddText(pos, col, begin, end);
                    if (!fauxBold) return;
                    const float a = ((col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
                    const float ah = std::clamp(a * kAuxStrokeAlphaScale, 0.0f, 1.0f);
                    const ImU32 aHalf = (ImU32)(ah * 255.0f + 0.5f);
                    const ImU32 colHalf = (col & ~IM_COL32_A_MASK) | (aHalf << IM_COL32_A_SHIFT);
                    drawList->AddText(ImVec2(pos.x + 1.0f, pos.y), colHalf, begin, end);
                    drawList->AddText(ImVec2(pos.x, pos.y + 1.0f), colHalf, begin, end);
                    return;
                }

                // Italic path: shear glyph quads
                auto draw_once = [&](const ImVec2 &p, ImU32 c) {
                    if ((c & IM_COL32_A_MASK) == 0) return;
                    ImFontBaked *baked = font ? font->GetFontBaked(fontSize) : nullptr;
                    if (!baked) { drawList->AddText(p, c, begin, end); return; }
                    const float scale = (fontSize >= 0.0f) ? (fontSize / baked->Size) : 1.0f;
                    // Use subpixel origin for smoother sheared edges
                    const float x0 = p.x;
                    const float y0 = p.y;
                    // Shear factor and anchor: baseline-anchored slight clockwise slant
                    const float shear = ComputeItalicShear(fontSize);
                    const float ascent = ImMax(0.0f, baked->Ascent);
                    const float anchor_y = y0 + ascent;

                    const char *s = begin; float x = x0;
                    while (s < end) {
                        utf8_int32_t code = 0; const char *next = (const char*)utf8codepoint((const utf8_int8_t*)s, &code);
                        if (!next || next <= s) next = s + 1;
                        const ImFontGlyph *g = baked->FindGlyph((ImWchar)code);
                        if (g && g->Visible) {
                            float x1 = x + g->X0 * scale, x2 = x + g->X1 * scale;
                            float y1 = y0 + g->Y0 * scale, y2 = y0 + g->Y1 * scale;
                            float u1 = g->U0, v1 = g->V0, u2 = g->U1, v2 = g->V1;
                            // Shear relative to anchor: dx(y) = shear * (anchor_y - y)
                            const float dx1 = shear * (anchor_y - y1);
                            const float dx2 = shear * (anchor_y - y2);
                            drawList->PrimReserve(6, 4);
                            drawList->PrimQuadUV(ImVec2(x1 + dx1, y1), ImVec2(x2 + dx1, y1),
                                                 ImVec2(x2 + dx2, y2), ImVec2(x1 + dx2, y2),
                                                 ImVec2(u1, v1), ImVec2(u2, v1), ImVec2(u2, v2), ImVec2(u1, v2), c);
                            x += g->AdvanceX * scale;
                        } else {
                            x += baked->FallbackAdvanceX * scale;
                        }
                        s = next;
                    }
                };

                draw_once(pos, col);
                if (!fauxBold) return;
                const float a = ((col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
                const float ah = std::clamp(a * kAuxStrokeAlphaScale, 0.0f, 1.0f);
                const ImU32 aHalf = (ImU32)(ah * 255.0f + 0.5f);
                const ImU32 colHalf = (col & ~IM_COL32_A_MASK) | (aHalf << IM_COL32_A_SHIFT);
                draw_once(ImVec2(pos.x + 1.0f, pos.y), colHalf);
                draw_once(ImVec2(pos.x, pos.y + 1.0f), colHalf);
            }
        };
    } // namespace MsgText
}

// =============================================================================
// MessageUnit Implementation
// =============================================================================

MessageBoard::MessageUnit::MessageUnit(const char *msg, float timer)
    : originalText(msg ? msg : ""), timer(timer) {
    if (msg && strlen(msg) > 0) {
        ParseAnsiEscapeCodes();
    }
}

void MessageBoard::MessageUnit::SetMessage(const char *msg) {
    if (!msg) return;

    originalText = msg;
    segments.clear();
    cachedHeight = -1.0f;
    cachedWrapWidth = -1.0f;

    ParseAnsiEscapeCodes();
}

float MessageBoard::MessageUnit::GetTextHeight(float wrapWidth, int tabColumns) const {
    if (cachedHeight >= 0.0f && std::abs(cachedWrapWidth - wrapWidth) < 0.5f)
        return cachedHeight;

    const float lineH = ImGui::GetTextLineHeightWithSpacing();

    std::vector<MsgText::Layout::Line> lines;
    MsgText::Layout::BuildLines(segments, wrapWidth, tabColumns, lines);
    const auto lineCount = static_cast<float>(lines.size());

    cachedHeight = ImMax(lineH, lineCount * lineH);
    cachedWrapWidth = wrapWidth;
    return cachedHeight;
}

void MessageBoard::MessageUnit::Reset() {
    originalText.clear();
    segments.clear();
    timer = 0.0f;
    cachedHeight = -1.0f;
    cachedWrapWidth = -1.0f;
}

void MessageBoard::MessageUnit::ParseAnsiEscapeCodes() {
    segments.clear();

    if (originalText.empty()) {
        return;
    }

    const char *p = originalText.c_str();
    ConsoleColor currentColor;
    std::string currentSegment;

    while (*p) {
        if (*p == '\033' && *(p + 1) == '[') {
            // Save current text segment
            if (!currentSegment.empty()) {
                segments.emplace_back(std::move(currentSegment), currentColor);
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
        segments.emplace_back(std::move(currentSegment), currentColor);
    }

    // Ensure at least one segment exists
    if (segments.empty()) {
        segments.emplace_back(originalText, ConsoleColor());
    }
}

MessageBoard::ConsoleColor MessageBoard::MessageUnit::ParseAnsiColorSequence(const std::string &sequence, const ConsoleColor &currentColor) {
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


ImU32 MessageBoard::MessageUnit::GetRgbColor(int r, int g, int b) {
    return IM_COL32(
        std::clamp(r, 0, 255),
        std::clamp(g, 0, 255),
        std::clamp(b, 0, 255),
        255
    );
}

// =============================================================================
// ConsoleColor Implementation
// =============================================================================

MessageBoard::ConsoleColor MessageBoard::ConsoleColor::GetRendered() const {
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
// MessageBoard Implementation
// =============================================================================

MessageBoard::MessageBoard(int size) : Bui::Window("MessageBoard") {
    if (size < 1) size = 500;
    m_Messages.resize(size);
}

MessageBoard::~MessageBoard() = default;

// =============================================================================
// Configuration and State Management
// =============================================================================

ImGuiWindowFlags MessageBoard::GetFlags() {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (!m_IsCommandBarVisible) {
        flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav;
    }

    return flags;
}

void MessageBoard::SetCommandBarVisible(bool visible) {
    if (m_IsCommandBarVisible != visible) {
        m_IsCommandBarVisible = visible;

        if (visible) {
            m_ScrollToBottom = true;
            Show();
        } else {
            // Reset scroll state when hiding command bar
            m_ScrollY = 0.0f;
            m_MaxScrollY = 0.0f;
            m_ScrollToBottom = true;
        }
    }
}

void MessageBoard::SetScrollPosition(float scrollY) {
    if (m_IsCommandBarVisible && m_MaxScrollY > 0.0f) {
        SetScrollYClamped(scrollY);
    }
}

void MessageBoard::ScrollToTop() {
    if (m_IsCommandBarVisible) {
        SetScrollYClamped(0.0f);
    }
}

void MessageBoard::ScrollToBottom() {
    if (m_IsCommandBarVisible) {
        SetScrollYClamped(m_MaxScrollY);
    }
}

// =============================================================================
// Height Calculation System
// =============================================================================

bool MessageBoard::ShouldShowMessage(const MessageUnit &msg) const {
    return m_IsCommandBarVisible || msg.GetTimer() > 0;
}

float MessageBoard::GetMessageAlpha(const MessageUnit &msg) const {
    const float maxAlpha = std::clamp(m_FadeMaxAlpha, 0.0f, 1.0f);
    if (m_IsCommandBarVisible) {
        return maxAlpha;
    }

    if (msg.GetTimer() <= 0) {
        return 0.0f;
    }

    const float maxAlpha255 = maxAlpha * 255.0f;
    return std::min(maxAlpha255, msg.GetTimer() / 20.0f) / 255.0f;
}

int MessageBoard::CountVisibleMessages() const {
    int count = 0;
    for (int i = 0; i < m_MessageCount; i++) {
        if (ShouldShowMessage(m_Messages[i])) {
            count++;
        }
    }
    return count;
}

bool MessageBoard::HasVisibleContent() const {
    if (m_IsCommandBarVisible) {
        return m_MessageCount > 0;
    }
    return m_DisplayMessageCount > 0;
}

float MessageBoard::CalculateContentHeight(float wrapWidth) const {
    float contentHeight = 0.0f;
    int visibleCount = 0;

    for (int i = 0; i < m_MessageCount; i++) {
        const MessageUnit &msg = m_Messages[i];
        if (ShouldShowMessage(msg)) {
            contentHeight += msg.GetTextHeight(wrapWidth, m_TabColumns);
            if (visibleCount > 0) {
                contentHeight += m_MessageGap;
            }
            visibleCount++;
        }
    }

    // Return pure content height (no padding)
    return std::max(contentHeight, ImGui::GetTextLineHeightWithSpacing());
}

float MessageBoard::CalculateDisplayHeight(float contentHeight) const {
    // Add padding to content height to get total display height needed
    return contentHeight + m_PadY * 2.0f;
}

// =============================================================================
// Window Rendering
// =============================================================================

void MessageBoard::OnPreBegin() {
    // Pre-read style values BEFORE pushing style overrides
    const ImGuiStyle &style = ImGui::GetStyle();

    // Scale layout parameters with current font size (fallback to style when larger)
    const float fs = ImGui::GetFontSize();
    const float pad_x_scaled = fs * 0.5f;    // ~8px @16pt
    const float pad_y_scaled = fs * 0.5f;    // ~8px @16pt
    const float gap_scaled = fs * 0.25f;     // ~4px @16pt
    const float sb_w_scaled = fs * 0.5f;     // ~8px @16pt
    const float sb_pad_scaled = fs * 0.125f; // ~2px @16pt
    m_PadX = ImMax((style.WindowPadding.x > 0.0f) ? style.WindowPadding.x : 0.0f, pad_x_scaled);
    m_PadY = ImMax((style.WindowPadding.y > 0.0f) ? style.WindowPadding.y : 0.0f, pad_y_scaled);
    m_MessageGap = ImMax((style.ItemSpacing.y > 0.0f) ? style.ItemSpacing.y : 0.0f, gap_scaled);
    m_ScrollbarW = ImMax((style.ScrollbarSize > 0.0f) ? style.ScrollbarSize : 0.0f, sb_w_scaled);
    m_ScrollbarPad = ImMax((style.ItemInnerSpacing.x > 0.0f) ? (style.ItemInnerSpacing.x * 0.5f) : 0.0f, sb_pad_scaled);

    // Push style overrides
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImVec4 winBg = m_HasCustomWindowBg ? m_WindowBgColor : Bui::GetMenuColor();
    winBg.w = std::clamp(winBg.w * std::clamp(m_WindowBgAlphaScale, 0.0f, 1.0f), 0.0f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, winBg);

    const ImVec2 vpSize = ImGui::GetMainViewport()->Size;
    const float windowWidth = vpSize.x * 0.96f;
    const float wrapWidth = windowWidth - m_PadX * 2.0f;
    const float maxDisplayHeight = vpSize.y * 0.8f;

    const float contentHeight = CalculateContentHeight(wrapWidth);
    const float displayHeight = CalculateDisplayHeight(contentHeight);
    float windowHeight = std::min(displayHeight, maxDisplayHeight);

    const float bottomAnchor = vpSize.y * 0.9f;
    float posY = bottomAnchor - windowHeight;

    const float posX = vpSize.x * 0.02f;

    ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
}

void MessageBoard::OnDraw() {
    if (!HasVisibleContent()) {
        return;
    }

    // Keep messages on top in normal mode; when command bar is visible,
    // allow the command bar to overlay the message board if they overlap.
    if (!m_IsCommandBarVisible)
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    const ImVec2 contentPos = ImGui::GetCursorScreenPos();
    const ImVec2 contentSize = ImGui::GetContentRegionAvail();
    const float baseWrapWidth = contentSize.x - m_PadX * 2.0f;

    // Calculate content dimensions for scrolling
    float contentHeightNoSB = CalculateContentHeight(baseWrapWidth);
    const float availableContentHeight = contentSize.y - m_PadY * 2.0f;

    // Determine if a scrollbar is needed when command bar is visible.
    bool needsScrollbar = m_IsCommandBarVisible && (contentHeightNoSB > availableContentHeight);
    float wrapWidth = baseWrapWidth;
    float contentHeight = contentHeightNoSB;
    if (needsScrollbar) {
        wrapWidth = std::max(0.0f, baseWrapWidth - (m_ScrollbarW + m_ScrollbarPad * 2.0f));
        contentHeight = CalculateContentHeight(wrapWidth); // recompute since wrap width shrinks
    }

    // Handle scrolling when command bar is visible
    if (m_IsCommandBarVisible) {
        if (needsScrollbar) {
            UpdateScrollBounds(contentHeight, availableContentHeight);
            HandleScrolling(availableContentHeight);
        } else {
            m_ScrollY = 0.0f;
            m_MaxScrollY = 0.0f;
            m_ScrollToBottom = true;
        }
    }

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    const ImVec2 contentStart = ImVec2(contentPos.x + m_PadX, contentPos.y + m_PadY);
    ImVec2 startPos = ImVec2(contentStart.x, contentStart.y - m_ScrollY);

    // Set up clipping for scrollable content
    if (m_IsCommandBarVisible) {
        const ImVec2 clipMin = ImVec2(contentPos.x + m_PadX, contentPos.y + m_PadY);
        const ImVec2 clipMax = ImVec2(contentPos.x + contentSize.x - m_PadX - (needsScrollbar ? (m_ScrollbarW + m_ScrollbarPad * 2.0f) : 0.0f), contentPos.y + contentSize.y - m_PadY);
        drawList->PushClipRect(clipMin, clipMax, true);
    }

    RenderMessages(drawList, startPos, wrapWidth);

    if (m_IsCommandBarVisible) {
        drawList->PopClipRect();
        if (needsScrollbar && m_MaxScrollY > 0.0f) {
            DrawScrollIndicators(drawList, contentPos, contentSize, contentHeight, availableContentHeight);
        }
    }
}

void MessageBoard::RenderMessages(ImDrawList *drawList, ImVec2 startPos, float wrapWidth) {
    ImVec4 bgColorBase = m_HasCustomMessageBg ? m_MessageBgColor : Bui::GetMenuColor();
    ImVec2 currentPos = startPos;

    for (int i = m_MessageCount - 1; i >= 0; i--) {
        const MessageUnit &msg = m_Messages[i];
        bool shouldShow = m_IsCommandBarVisible || ShouldShowMessage(msg);

        if (shouldShow) {
            const float alpha = GetMessageAlpha(msg);
            const float msgHeight = msg.GetTextHeight(wrapWidth, m_TabColumns);

            // Draw background with style-derived padding
            const float finalAlpha = std::clamp(bgColorBase.w * std::clamp(m_MessageBgAlphaScale, 0.0f, 1.0f) * alpha, 0.0f, 1.0f);
            ImVec4 bg = ImVec4(bgColorBase.x, bgColorBase.y, bgColorBase.z, finalAlpha);
            drawList->AddRectFilled(
                ImVec2(currentPos.x - m_PadX * 0.5f, currentPos.y - m_PadY * 0.25f),
                ImVec2(currentPos.x + wrapWidth + m_PadX * 0.5f, currentPos.y + msgHeight + m_PadY * 0.25f),
                ImGui::GetColorU32(bg)
            );

            DrawMessageText(drawList, msg, currentPos, wrapWidth, alpha);
            currentPos.y += msgHeight + m_MessageGap;
        }
    }
}

void MessageBoard::DrawMessageText(ImDrawList *drawList, const MessageUnit &message, const ImVec2 &startPos, float wrapWidth, float alpha) {
    ImVec2 cursor = startPos;

    // Prepare layout
    std::vector<MsgText::Layout::Line> lines;
    MsgText::Layout::BuildLines(message.segments, wrapWidth, m_TabColumns, lines);

    const float fontSize = ImGui::GetFontSize();

        for (const auto &line : lines) {
            // Fetch font metrics for this line (cursor.y is line top)
            ImFont *font = ImGui::GetFont();
            ImFontBaked *baked = font ? font->GetFontBaked(fontSize) : nullptr;
            const float ascent = baked ? ImMax(0.0f, baked->Ascent) : fontSize * 0.8f;
            const float descentMag = baked ? ImMax(0.0f, -baked->Descent) : fontSize * 0.2f;
            const float lineTop = cursor.y;
            const float lineBottom = cursor.y + ascent + descentMag;
            const float italicShearForEffects = MsgText::Renderer::ComputeItalicShear(fontSize);

        // Pass 1: merge adjacent background spans into runs and draw them once.
        struct BgRun { float x0, x1; ImU32 col; };
        std::vector<BgRun> runs;

        float xForBg = startPos.x;
        bool hasOpenRun = false;
        BgRun cur{};

        for (const auto &sp : line.spans) {
            const MessageBoard::TextSegment *seg = sp.seg;

            if (!seg) {
                // advance, nothing to draw
                xForBg += sp.width;
                continue;
            }

            // Resolve final bg color for this span
            ConsoleColor rc = m_AnsiEnabled
                                  ? seg->color.GetRendered()
                                  : ConsoleColor(seg->color.foreground, IM_COL32(0, 0, 0, 0));
            ImU32 bgBase = rc.background;
            if (m_AnsiEnabled) {
                if (rc.bgIsAnsi256 && rc.bgAnsiIndex >= 0) {
                    m_Palette.EnsureInitialized();
                    if (m_Palette.IsActive()) {
                        m_Palette.GetColor(rc.bgAnsiIndex, bgBase);
                    }
                }
            }

            ImU32 bg = MsgText::Color::ApplyAlpha(bgBase, alpha);
            const bool drawBg = (!sp.isTab) && (((bg >> IM_COL32_A_SHIFT) & 0xFF) != 0) && (sp.b < sp.e);

            if (drawBg) {
                const float x0 = xForBg;
                // Extend background to cover slanted top-right edge when italic is active
                float italicPad = 0.0f;
                if (m_AnsiEnabled && rc.italic) {
                    italicPad = ImMax(0.0f, italicShearForEffects) * (lineBottom - lineTop);
                }
                const float x1 = xForBg + sp.width + italicPad;
                if (hasOpenRun && cur.col == bg && std::abs(cur.x1 - x0) <= 0.25f) {
                    // extend current run contiguously
                    cur.x1 = x1;
                } else {
                    // flush previous run
                    if (hasOpenRun) runs.push_back(cur);
                    cur = BgRun{x0, x1, bg};
                    hasOpenRun = true;
                }
            } else {
                // gap breaks run
                if (hasOpenRun) { runs.push_back(cur); hasOpenRun = false; }
            }

            xForBg += sp.width;
        }
        if (hasOpenRun) runs.push_back(cur);

        // Draw merged background runs without inflation to avoid overlaps.
        for (const BgRun &r : runs) {
            drawList->AddRectFilled(
                ImVec2(r.x0, lineTop),
                ImVec2(r.x1, lineBottom),
                r.col
            );
        }

        // Pass 2: draw text and decorations (no background here)
        float x = startPos.x;
        for (const auto &sp : line.spans) {
            const MessageBoard::TextSegment *seg = sp.seg;
            if (!seg) { x += sp.width; continue; }

            ConsoleColor rc = m_AnsiEnabled
                                  ? seg->color.GetRendered()
                                  : ConsoleColor(seg->color.foreground, IM_COL32(0, 0, 0, 0));

            ImU32 fg = rc.foreground;
            if (m_AnsiEnabled) {
                if (rc.fgIsAnsi256 && rc.fgAnsiIndex >= 0) {
                    m_Palette.EnsureInitialized();
                    if (m_Palette.IsActive()) {
                        m_Palette.GetColor(rc.fgAnsiIndex, fg);
                    }
                }
                // No color toning
            }
            if (m_AnsiEnabled) {
                if (rc.dim) fg = MsgText::Color::ApplyDim(fg);
                if (rc.hidden) {
                    // Render hidden text as its background color, with message alpha already applied in BG pass.
                    ImU32 bgBase2 = rc.background;
                    if (rc.bgIsAnsi256 && rc.bgAnsiIndex >= 0) {
                        m_Palette.EnsureInitialized();
                        if (m_Palette.IsActive()) {
                            m_Palette.GetColor(rc.bgAnsiIndex, bgBase2);
                        }
                        // preserve palette
                        fg = MsgText::Color::ApplyAlpha(bgBase2, alpha);
                    } else {
                        // no toning for non-palette colors either
                        fg = MsgText::Color::ApplyAlpha(bgBase2, alpha);
                    }
                }
            }
            const bool fauxBold = rc.bold;
            const bool italic = rc.italic;

            if (!sp.isTab && sp.b < sp.e) {
                // Use line top for AddText position to align with ImGui expectations
                MsgText::Renderer::AddTextStyled(drawList, ImGui::GetFont(), fontSize, ImVec2(x, lineTop), fg,
                                                sp.b, sp.e, italic, fauxBold);

                if (rc.underline) {
                    float y = MsgText::Metrics::UnderlineY(lineTop, fontSize);
                    float th = MsgText::Metrics::Thickness(fontSize);
                    float italicPad = (m_AnsiEnabled && rc.italic) ? ImMax(0.0f, italicShearForEffects) * (lineBottom - lineTop) : 0.0f;
                    // Align odd thickness to pixel centers for crisper AA lines
                    if (fmodf(th, 2.0f) != 0.0f) y = floorf(y) + 0.5f; else y = roundf(y);
                    drawList->AddLine(ImVec2(x, y), ImVec2(x + sp.width + italicPad, y), fg, th);
                }
                if (rc.strikethrough) {
                    float y = MsgText::Metrics::StrikeY(lineTop, fontSize);
                    float th = MsgText::Metrics::Thickness(fontSize);
                    float italicPad = (m_AnsiEnabled && rc.italic) ? ImMax(0.0f, italicShearForEffects) * (lineBottom - lineTop) : 0.0f;
                    if (fmodf(th, 2.0f) != 0.0f) y = floorf(y) + 0.5f; else y = roundf(y);
                    drawList->AddLine(ImVec2(x, y), ImVec2(x + sp.width + italicPad, y), fg, th);
                }
            }

            x += sp.width;
        }

        cursor.y += ImGui::GetTextLineHeightWithSpacing();
    }
}

// =============================================================================
// Scrolling System
// =============================================================================

void MessageBoard::HandleScrolling(float availableHeight) {
    if (!m_IsCommandBarVisible || m_MaxScrollY <= 0.0f) return;

    const ImGuiIO &io = ImGui::GetIO();

    // Mouse wheel scrolling
    if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f) {
        const float scrollSpeed = ImGui::GetTextLineHeightWithSpacing() * 3.0f;
        SetScrollYClamped(m_ScrollY - io.MouseWheel * scrollSpeed);
    }

    // Final clamp and bottom sync (in case no inputs were pressed this frame but bounds changed)
    SetScrollYClamped(m_ScrollY);
}

void MessageBoard::UpdateScrollBounds(float contentHeight, float availableHeight) {
    if (contentHeight > availableHeight) {
        m_MaxScrollY = contentHeight - availableHeight;

        if (m_ScrollToBottom) {
            m_ScrollY = m_MaxScrollY;
        }

        // Ensure scroll and bottom flag are consistent with new bounds
        SetScrollYClamped(m_ScrollY);
    } else {
        m_MaxScrollY = 0.0f;
        m_ScrollY = 0.0f;
        m_ScrollToBottom = true;
    }
}

void MessageBoard::InvalidateLayoutCache() {
    for (auto &m : m_Messages) {
        m.cachedHeight = -1.0f;
        m.cachedWrapWidth = -1.0f;
    }
}

void MessageBoard::DrawScrollIndicators(ImDrawList *drawList, const ImVec2 &contentPos, const ImVec2 &contentSize, float contentHeight, float visibleHeight) {
    if (m_MaxScrollY <= 0.0f) return;

    // Scrollbar background
    const ImVec2 scrollbarStart = ImVec2(
        contentPos.x + contentSize.x - m_ScrollbarW - m_ScrollbarPad,
        contentPos.y + m_PadY + m_ScrollbarPad
    );
    const ImVec2 scrollbarEnd = ImVec2(
        contentPos.x + contentSize.x - m_ScrollbarPad,
        contentPos.y + contentSize.y - m_PadY - m_ScrollbarPad
    );

    drawList->AddRectFilled(scrollbarStart, scrollbarEnd, IM_COL32(60, 60, 60, 100));

    // Scrollbar handle
    const float scrollbarHeight = scrollbarEnd.y - scrollbarStart.y;
    const ScrollMetrics m = GetScrollMetrics(contentHeight, visibleHeight);
    const float handleHeight = std::max(ImGui::GetStyle().GrabMinSize, scrollbarHeight * m.visibleRatio);
    const float handlePos = (m.maxScroll > 0.0f ? (m.scrollY / m.maxScroll) : 0.0f) * (scrollbarHeight - handleHeight);

    const ImVec2 handleStart = ImVec2(scrollbarStart.x + 1.0f, scrollbarStart.y + handlePos);
    const ImVec2 handleEnd = ImVec2(scrollbarEnd.x - 1.0f, handleStart.y + handleHeight);

    drawList->AddRectFilled(handleStart, handleEnd, IM_COL32(150, 150, 150, 200));

    // Scroll position indicator
    if (m_ScrollY > 0.0f || !m_ScrollToBottom) {
        const std::string scrollText = FormatScrollPercent(contentHeight, visibleHeight);
        const ImVec2 textSize = ImGui::CalcTextSize(scrollText.c_str());
        const ImVec2 textPos = ImVec2(
            contentPos.x + contentSize.x - textSize.x - m_ScrollbarW - m_ScrollbarPad - m_PadX,
            contentPos.y + m_PadY * 0.5f
        );

        // Text background
        drawList->AddRectFilled(
            ImVec2(textPos.x - m_PadX * 0.25f, textPos.y - m_PadY * 0.25f),
            ImVec2(textPos.x + textSize.x + m_PadX * 0.25f, textPos.y + textSize.y + m_PadY * 0.25f),
            IM_COL32(0, 0, 0, 150)
        );

        // Text
        drawList->AddText(textPos, IM_COL32(255, 255, 255, 200), scrollText.c_str());
    }
}

MessageBoard::ScrollMetrics MessageBoard::GetScrollMetrics(float contentHeight, float visibleHeight) const {
    ScrollMetrics m{};
    m.contentHeight = std::max(0.0f, contentHeight);
    m.visibleHeight = std::clamp(visibleHeight, 1.0f, std::max(1.0f, m.contentHeight));
    m.maxScroll = std::max(0.0f, m.contentHeight - m.visibleHeight);
    m.scrollY = std::clamp(m_ScrollY, 0.0f, m.maxScroll);
    m.scrollRatio = (m.maxScroll > 0.0f) ? (m.scrollY / m.maxScroll) : 0.0f;
    m.visibleRatio = (m.contentHeight > 0.0f) ? (m.visibleHeight / m.contentHeight) : 1.0f;
    return m;
}

void MessageBoard::SetScrollYClamped(float y) {
    m_ScrollY = std::clamp(y, 0.0f, m_MaxScrollY);
    m_ScrollToBottom = (m_ScrollY >= m_MaxScrollY - kScrollEpsilon);
}

void MessageBoard::SyncScrollBottomFlag() {
    m_ScrollToBottom = (m_ScrollY >= m_MaxScrollY - kScrollEpsilon);
}

std::string MessageBoard::FormatScrollPercent(float contentHeight, float visibleHeight) const {
    const ScrollMetrics m = GetScrollMetrics(contentHeight, visibleHeight);
    const float pct = (m.maxScroll > 0.0f) ? (m.scrollRatio * 100.0f) : 0.0f;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f%%", pct);
    return std::string(buf);
}

// =============================================================================
// Message Management
// =============================================================================

void MessageBoard::UpdateTimers(float deltaTime) {
    for (int i = 0; i < m_MessageCount; i++) {
        if (m_Messages[i].timer > 0.0f) {
            m_Messages[i].timer -= deltaTime;
            if (m_Messages[i].timer <= 0.0f) {
                m_Messages[i].timer = 0.0f;
                --m_DisplayMessageCount;
            }
        }
    }
}

void MessageBoard::AddMessageInternal(const char *msg) {
    if (!msg) return;

    if (msg[0] == '\0')
        msg = "\n"; // treat empty messages as newlines

    // Update display count
    if (m_MessageCount == static_cast<int>(m_Messages.size()) && m_Messages[m_MessageCount - 1].GetTimer() > 0) {
        --m_DisplayMessageCount;
    }

    // Shift messages
    const int shiftCount = std::min(m_MessageCount, static_cast<int>(m_Messages.size()) - 1);
    for (int i = shiftCount - 1; i >= 0; i--) {
        m_Messages[i + 1] = std::move(m_Messages[i]);
    }

    // Add new message
    m_Messages[0] = MessageUnit(msg, m_MaxTimer);

    if (m_MessageCount < static_cast<int>(m_Messages.size())) {
        ++m_MessageCount;
    }
    ++m_DisplayMessageCount;

    // Auto-scroll to bottom for new messages
    if (m_IsCommandBarVisible && (m_ScrollToBottom || m_MaxScrollY <= 0.0f)) {
        m_ScrollToBottom = true;
    }
}

void MessageBoard::OnPostEnd() {
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    // Update timers
    CKStats stats;
    BML_GetCKContext()->GetProfileStats(&stats);
    UpdateTimers(stats.TotalFrameTime);

    // Hide if no visible content
    if (!m_IsCommandBarVisible && m_DisplayMessageCount == 0) {
        Hide();
    }
}

// =============================================================================
// Public Interface
// =============================================================================

void MessageBoard::AddMessage(const char *msg) {
    AddMessageInternal(msg);
}

void MessageBoard::Printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int needed = vsnprintf(nullptr, 0, format, args);
    va_end(args);
    if (needed < 0) return;
    std::string buf((size_t) needed + 1, '\0');
    va_start(args, format);
    vsnprintf(buf.data(), buf.size(), format, args);
    va_end(args);
    AddMessage(buf.c_str());
}

void MessageBoard::PrintfColored(ImU32 color, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int needed = vsnprintf(nullptr, 0, format, args);
    va_end(args);
    if (needed < 0) return;
    std::string buf((size_t) needed + 1, '\0');
    va_start(args, format);
    vsnprintf(buf.data(), buf.size(), format, args);
    va_end(args);

    const ImU32 r = (color >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (color >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (color >> IM_COL32_B_SHIFT) & 0xFF;

    std::string coloredBuffer = "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m" + buf + "\033[0m";
    AddMessage(coloredBuffer.c_str());
}

void MessageBoard::ClearMessages() {
    m_MessageCount = 0;
    m_DisplayMessageCount = 0;
    for (auto &message : m_Messages) {
        message.Reset();
    }
}

void MessageBoard::ResizeMessages(int size) {
    if (size < 1) return;

    m_Messages.resize(size);
    m_MessageCount = std::min(m_MessageCount, size);
    m_DisplayMessageCount = std::min(m_DisplayMessageCount, size);
}

// =============================================================================
// Configuration API
// =============================================================================

void MessageBoard::SetTabColumns(int columns) {
    int c = std::clamp(columns, 1, 64);
    if (m_TabColumns != c) {
        m_TabColumns = c;
        InvalidateLayoutCache();
    }
}

void MessageBoard::SetWindowBackgroundColor(ImVec4 color) {
    m_HasCustomWindowBg = true;
    m_WindowBgColor = color;
}

void MessageBoard::SetWindowBackgroundColorU32(ImU32 color) {
    SetWindowBackgroundColor(ImGui::ColorConvertU32ToFloat4(color));
}

void MessageBoard::ClearWindowBackgroundColor() {
    m_HasCustomWindowBg = false;
}

void MessageBoard::SetMessageBackgroundColor(ImVec4 color) {
    m_HasCustomMessageBg = true;
    m_MessageBgColor = color;
}

void MessageBoard::SetMessageBackgroundColorU32(ImU32 color) {
    SetMessageBackgroundColor(ImGui::ColorConvertU32ToFloat4(color));
}

void MessageBoard::ClearMessageBackgroundColor() {
    m_HasCustomMessageBg = false;
}

void MessageBoard::SetWindowBackgroundAlpha(float alpha) {
    m_WindowBgAlphaScale = std::clamp(alpha, 0.0f, 1.0f);
}

void MessageBoard::SetMessageBackgroundAlpha(float alpha) {
    m_MessageBgAlphaScale = std::clamp(alpha, 0.0f, 1.0f);
}

void MessageBoard::SetFadeMaxAlpha(float alpha) {
    m_FadeMaxAlpha = std::clamp(alpha, 0.0f, 1.0f);
}

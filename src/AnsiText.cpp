#include "AnsiText.h"

#include <cmath>
#include <algorithm>
#include <cstring>
#include <utf8.h>

#include "AnsiPalette.h"
#include "StringUtils.h"

namespace AnsiText {
    // =============================================================================
    // Global configuration
    // =============================================================================
    static Sgr21Policy g_Sgr21Policy = Sgr21Policy::DoubleUnderline;
    static const AnsiPalette *g_PreResolvePalette = nullptr;
    static bool g_PreResolveEnabled = false;

    void SetSgr21Policy(Sgr21Policy policy) { g_Sgr21Policy = policy; }
    Sgr21Policy GetSgr21Policy() { return g_Sgr21Policy; }
    void SetPreResolvePalette(const AnsiPalette *palette) { g_PreResolvePalette = palette; }
    const AnsiPalette *GetPreResolvePalette() { return g_PreResolvePalette; }
    void SetPreResolveEnabled(bool enabled) { g_PreResolveEnabled = enabled; }
    bool GetPreResolveEnabled() { return g_PreResolveEnabled; }

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

    AnsiString::AnsiString(const char *text) { SetText(text); }
    AnsiString::AnsiString(const std::string &text) { SetText(text); }
    AnsiString::AnsiString(std::string &&text) { SetText(std::move(text)); }

    void AnsiString::RebindSegmentsPointers(const char *oldBase, const char *newBase) {
        if (!oldBase || !newBase) return;
        if (oldBase == newBase) return;
        for (auto &seg : m_Segments) {
            if (seg.begin && seg.end) {
                ptrdiff_t offB = seg.begin - oldBase;
                ptrdiff_t offE = seg.end   - oldBase;
                if (offB >= 0 && offE >= offB) {
                    seg.begin = newBase + offB;
                    seg.end   = newBase + offE;
                } else {
                    // Fallback safety: if offsets look invalid, clear pointers to avoid UB
                    seg.begin = seg.end = newBase;
                }
            }
        }
    }

    AnsiString::AnsiString(const AnsiString &other) {
        // Deep copy text then segments, adjust pointers to this->text buffer
        const char *srcBase = other.m_OriginalText.c_str();
        m_OriginalText = other.m_OriginalText;
        m_Segments = other.m_Segments;
        m_HasAnsi256BG = other.m_HasAnsi256BG;
        m_HasTrueColorBG = other.m_HasTrueColorBG;
        m_HasReverse = other.m_HasReverse;
        const char *dstBase = m_OriginalText.c_str();
        RebindSegmentsPointers(srcBase, dstBase);
    }

    AnsiString &AnsiString::operator=(const AnsiString &other) {
        if (this == &other) return *this;
        const char *srcBase = other.m_OriginalText.c_str();
        m_OriginalText = other.m_OriginalText;
        m_Segments = other.m_Segments;
        m_HasAnsi256BG = other.m_HasAnsi256BG;
        m_HasTrueColorBG = other.m_HasTrueColorBG;
        m_HasReverse = other.m_HasReverse;
        const char *dstBase = m_OriginalText.c_str();
        RebindSegmentsPointers(srcBase, dstBase);
        return *this;
    }

    AnsiString::AnsiString(AnsiString &&other) noexcept {
        const char *srcBase = other.m_OriginalText.c_str();
        m_OriginalText = std::move(other.m_OriginalText);
        m_Segments = std::move(other.m_Segments);
        m_HasAnsi256BG = other.m_HasAnsi256BG;
        m_HasTrueColorBG = other.m_HasTrueColorBG;
        m_HasReverse = other.m_HasReverse;
        const char *dstBase = m_OriginalText.c_str();
        RebindSegmentsPointers(srcBase, dstBase);
    }

    AnsiString &AnsiString::operator=(AnsiString &&other) noexcept {
        if (this == &other) return *this;
        const char *srcBase = other.m_OriginalText.c_str();
        m_OriginalText = std::move(other.m_OriginalText);
        m_Segments = std::move(other.m_Segments);
        m_HasAnsi256BG = other.m_HasAnsi256BG;
        m_HasTrueColorBG = other.m_HasTrueColorBG;
        m_HasReverse = other.m_HasReverse;
        const char *dstBase = m_OriginalText.c_str();
        RebindSegmentsPointers(srcBase, dstBase);
        return *this;
    }

    static bool IsValidUtf8Z(const char *s) {
        if (!s) return true;
        return utf8valid(reinterpret_cast<const utf8_int8_t *>(s)) == nullptr;
    }

    static std::string NormalizeToUtf8(const char *s) {
        if (!s) return std::string();
        // Fast path: already valid UTF-8
        if (IsValidUtf8Z(s)) return std::string(s);
        // Fallback: treat input as ANSI/ACP and convert to UTF-8
        // This covers cases where callers pass local-encoded strings.
        std::wstring w = utils::AnsiToUtf16(s);
        return utils::Utf16ToUtf8(w);
    }

    static std::string NormalizeToUtf8(const std::string &s) {
        if (s.empty()) return s;
        if (utf8valid(reinterpret_cast<const utf8_int8_t*>(s.c_str())) == nullptr) return s;
        std::wstring w = utils::AnsiToUtf16(s.c_str());
        return utils::Utf16ToUtf8(w);
    }

    void AnsiString::SetText(const char *text) {
        if (!text) { Clear(); return; }
        AssignAndParse(NormalizeToUtf8(text));
    }

    void AnsiString::SetText(const std::string &text) {
        AssignAndParse(NormalizeToUtf8(text));
    }

    void AnsiString::SetText(std::string &&text) {
        if (text.empty()) { Clear(); return; }
        // Validate and normalize without extra copies when possible
        if (utf8valid(reinterpret_cast<const utf8_int8_t*>(text.c_str())) == nullptr) {
            AssignAndParse(std::move(text));
        } else {
            std::wstring w = utils::AnsiToUtf16(text.c_str());
            AssignAndParse(utils::Utf16ToUtf8(w));
        }
    }

    void AnsiString::AssignAndParse(std::string &&text) {
        m_OriginalText = std::move(text);
        m_Segments.clear();
        ParseAnsiEscapeCodes();
    }

    void AnsiString::Clear() {
        m_OriginalText.clear();
        m_Segments.clear();
        m_HasAnsi256BG = false;
        m_HasTrueColorBG = false;
        m_HasReverse = false;
    }

    void AnsiString::ParseAnsiEscapeCodes() {
        m_Segments.clear();

        if (m_OriginalText.empty())
            return;

        const char *const base = m_OriginalText.c_str();
        const char *const end = base + m_OriginalText.size();
        // Skip UTF-8 BOM if present to avoid rendering garbage at start of first line
        const char *const start = ((end - base) >= 3 &&
                                   (unsigned char)base[0] == 0xEF &&
                                   (unsigned char)base[1] == 0xBB &&
                                   (unsigned char)base[2] == 0xBF)
                                  ? (base + 3) : base;

        // Reset aggregate flags
        m_HasAnsi256BG = false;
        m_HasTrueColorBG = false;
        m_HasReverse = false;

        // Fast path: no ESC present -> single zero-copy segment
        if (std::memchr(start, 0x1B, (size_t)(end - start)) == nullptr &&
            std::memchr(start, 0x9B, (size_t)(end - start)) == nullptr) {
            m_Segments.emplace_back(start, end, ConsoleColor());
            return;
        }

        // General parser with zero-copy segments
        m_Segments.reserve(8);
        ConsoleColor currentColor;
        const char *p = start;
        const char *segStart = start;

        while (p < end) {
            if ((end - p) >= 2 && p[0] == '\033' && p[1] == '[') {
                const char *seqStart = p + 2;

                // Finite-state scan per ECMA-48: parameters (0x30-0x3F), intermediates (0x20-0x2F), final (0x40-0x7E)
                const char *q = seqStart;
                bool sawFinal = false;
                bool isSGR = false;

                // Consume parameter bytes
                while (q < end) {
                    unsigned char ch = (unsigned char)*q;
                    if (ch >= 0x30 && ch <= 0x3F) { ++q; continue; } // params: 0-9:;<=>?
                    if (ch >= 0x20 && ch <= 0x2F) { ++q; continue; } // intermediates (rare for SGR)
                    if (ch >= 0x40 && ch <= 0x7E) {                    // final byte
                        sawFinal = true;
                        isSGR = (ch == 'm');
                        break;
                    }
                    // Any other byte (including ESC, C0 controls) -> abort CSI parsing
                    break;
                }

                if (sawFinal && isSGR) {
                    // Flush preceding text [segStart, p)
                    if (segStart < p) {
                        if (!m_Segments.empty() && m_Segments.back().color == currentColor && m_Segments.back().end == segStart) {
                            m_Segments.back().end = p;
                        } else {
                            m_Segments.emplace_back(segStart, p, currentColor);
                        }
                    }
                    currentColor = ParseAnsiColorSequence(seqStart, (size_t)(q - seqStart), currentColor, &m_HasAnsi256BG, &m_HasTrueColorBG, &m_HasReverse);
                    p = q + 1;      // skip final 'm'
                    segStart = p;
                    continue;
                }
                // Not an SGR (or incomplete): fall through to treat bytes literally
            }
            else if ((end - p) >= 1 && (unsigned char)p[0] == 0x9B) { // 8-bit C1 CSI variant
                const char *seqStart = p + 1;
                const char *q = seqStart;
                bool sawFinal = false;
                bool isSGR = false;
                while (q < end) {
                    unsigned char ch = (unsigned char)*q;
                    if (ch >= 0x30 && ch <= 0x3F) { ++q; continue; }
                    if (ch >= 0x20 && ch <= 0x2F) { ++q; continue; }
                    if (ch >= 0x40 && ch <= 0x7E) { sawFinal = true; isSGR = (ch == 'm'); break; }
                    break;
                }
                if (sawFinal && isSGR) {
                    if (segStart < p) {
                        if (!m_Segments.empty() && m_Segments.back().color == currentColor && m_Segments.back().end == segStart) {
                            m_Segments.back().end = p;
                        } else {
                            m_Segments.emplace_back(segStart, p, currentColor);
                        }
                    }
                    currentColor = ParseAnsiColorSequence(seqStart, (size_t)(q - seqStart), currentColor, &m_HasAnsi256BG, &m_HasTrueColorBG, &m_HasReverse);
                    p = q + 1; // skip final 'm'
                    segStart = p;
                    continue;
                }
                // else treat as literal
            }
            ++p;
        }

        // Flush tail
        if (segStart < end) {
            if (!m_Segments.empty() && m_Segments.back().color == currentColor && m_Segments.back().end == segStart) {
                m_Segments.back().end = end;
            } else {
                m_Segments.emplace_back(segStart, end, currentColor);
            }
        }

        if (m_Segments.empty())
            m_Segments.emplace_back(end, end, ConsoleColor());

        // Optional pre-resolve of ANSI 256-color indices to RGBA using a fixed palette
        if (g_PreResolveEnabled && g_PreResolvePalette) {
            const_cast<AnsiPalette *>(g_PreResolvePalette)->EnsureInitialized();
            const bool active = g_PreResolvePalette->IsActive();
            if (active) {
                for (auto &seg : m_Segments) {
                    ConsoleColor &cc = seg.color;
                    if (cc.fgIsAnsi256 && cc.fgAnsiIndex >= 0) {
                        ImU32 col = cc.foreground;
                        if (g_PreResolvePalette->GetColor(cc.fgAnsiIndex, col)) {
                            cc.foreground = col;
                            cc.fgIsAnsi256 = false;
                            cc.fgAnsiIndex = -1;
                        }
                    }
                    if (cc.bgIsAnsi256 && cc.bgAnsiIndex >= 0) {
                        ImU32 col = cc.background;
                        if (g_PreResolvePalette->GetColor(cc.bgAnsiIndex, col)) {
                            cc.background = col;
                            cc.bgIsAnsi256 = false;
                            cc.bgAnsiIndex = -1;
                        }
                    }
                }
            }
        }
    }

    ConsoleColor AnsiString::ParseAnsiColorSequence(const char *sequence, size_t length, const ConsoleColor &currentColor,
                                                    bool *out_hasAnsi256Bg, bool *out_hasTrueColorBg, bool *out_hasReverse) {
        ConsoleColor color = currentColor;
        if (length == 0) return color;

        const char *p = sequence;
        const char *const e = sequence + length;

        auto skipSep = [&]() { while (p < e && (*p == ';' || *p == ' ')) ++p; };
        auto readInt = [&](int &out) -> bool {
            skipSep();
            if (p >= e || *p < '0' || *p > '9') return false;
            int v = 0;
            while (p < e && *p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); ++p; }
            out = v; return true;
        };

        int code = 0;
        while (readInt(code)) {
            if (code == 0) { color = ConsoleColor(); continue; }

            if (code >= 30 && code <= 37) { color.fgIsAnsi256 = true; color.fgAnsiIndex = code - 30; continue; }
            if (code >= 40 && code <= 47) { color.bgIsAnsi256 = true; color.bgAnsiIndex = code - 40; if (out_hasAnsi256Bg) *out_hasAnsi256Bg = true; continue; }
            if (code >= 90 && code <= 97) { color.fgIsAnsi256 = true; color.fgAnsiIndex = (code - 90) + 8; continue; }
            if (code >= 100 && code <= 107){ color.bgIsAnsi256 = true; color.bgAnsiIndex = (code - 100) + 8; if (out_hasAnsi256Bg) *out_hasAnsi256Bg = true; continue; }

            if (code == 38 || code == 48) {
                const bool isBg = (code == 48);
                int mode = -1;
                if (!readInt(mode)) break;
                if (mode == 5) {
                    int idx = 0; if (!readInt(idx)) break;
                    idx = std::clamp(idx, 0, 255);
                    if (isBg) { color.bgIsAnsi256 = true; color.bgAnsiIndex = idx; if (out_hasAnsi256Bg) *out_hasAnsi256Bg = true; }
                    else      { color.fgIsAnsi256 = true; color.fgAnsiIndex = idx; }
                    continue;
                }
                if (mode == 2) {
                    int r=0,g=0,b=0; if (!readInt(r) || !readInt(g) || !readInt(b)) break;
                    ImU32 v = GetRgbColor(r, g, b);
                    if (isBg) { color.background = v; color.bgIsAnsi256 = false; color.bgAnsiIndex = -1; if (out_hasTrueColorBg) *out_hasTrueColorBg = true; }
                    else      { color.foreground = v; color.fgIsAnsi256 = false; color.fgAnsiIndex = -1; }
                    continue;
                }
                // Unknown sub-mode, ignore
                continue;
            }

            if (code == 39) { color.foreground = ImGui::GetColorU32(ImGuiCol_Text); color.fgIsAnsi256 = false; color.fgAnsiIndex = -1; continue; }
            if (code == 49) { color.background = IM_COL32(0, 0, 0, 0); color.bgIsAnsi256 = false; color.bgAnsiIndex = -1; continue; }

            switch (code) {
                case 1: color.bold = true; break;
                case 2: color.dim = true; break;
                case 3: color.italic = true; break;
                case 4: color.underline = true; color.doubleUnderline = false; break;
                case 5: case 6: /* blink no-op */ break;
                case 7: color.reverse = true; if (out_hasReverse) *out_hasReverse = true; break;
                case 8: color.hidden = true; break;
                case 9: color.strikethrough = true; break;
                case 21:
                    if (GetSgr21Policy() == Sgr21Policy::DoubleUnderline) { color.underline = true; color.doubleUnderline = true; }
                    else { color.bold = false; color.dim = false; }
                    break;
                case 22: color.bold = false; color.dim = false; break;
                case 23: color.italic = false; break;
                case 24: color.underline = false; color.doubleUnderline = false; break;
                case 25: /* blink off no-op */ break;
                case 27: color.reverse = false; break;
                case 28: color.hidden = false; break;
                case 29: color.strikethrough = false; break;
                default: break; // ignore unknown
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

    static bool IsCombiningMark(uint32_t c) {
        return (c >= 0x0300 && c <= 0x036F) ||
               (c >= 0x1AB0 && c <= 0x1AFF) ||
               (c >= 0x1DC0 && c <= 0x1DFF) ||
               (c >= 0x20D0 && c <= 0x20FF) ||
               (c >= 0xFE20 && c <= 0xFE2F);
    }
    static bool IsVariationSelector(uint32_t c) {
        return (c >= 0xFE00 && c <= 0xFE0F) || (c >= 0xE0100 && c <= 0xE01EF);
    }
    static bool IsZWJ(uint32_t c) { return c == 0x200D; }

    const char *Layout::NextGrapheme(const char *s, const char *end) {
        if (!s || s >= end) return s;
        const char *next = Utf8Next(s, end);
        if (!next || next <= s) return (s + 1 < end) ? s + 1 : end;

        for (;;) {
            if (next >= end) break;
            utf8_int32_t cp2 = 0; const char *p2 = (const char*) utf8codepoint((const utf8_int8_t*) next, &cp2);
            if (!p2 || p2 <= next) { next = next + 1; break; }
            if (IsVariationSelector((uint32_t)cp2) || IsCombiningMark((uint32_t)cp2)) { next = p2; continue; }
            if (IsZWJ((uint32_t)cp2)) {
                // Include ZWJ + following character and its combining marks
                utf8_int32_t cp3 = 0; const char *p3 = (const char*) utf8codepoint((const utf8_int8_t*) p2, &cp3);
                if (!p3 || p3 <= p2) { next = p2; break; }
                next = p3;
                // absorb trailing marks after the joined char
                for (;;) {
                    if (next >= end) break;
                    utf8_int32_t cp4 = 0; const char *p4 = (const char*) utf8codepoint((const utf8_int8_t*) next, &cp4);
                    if (!p4 || p4 <= next) { next = next + 1; break; }
                    if (IsVariationSelector((uint32_t)cp4) || IsCombiningMark((uint32_t)cp4)) { next = p4; continue; }
                    break;
                }
                continue;
            }
            break;
        }
        return next > end ? end : next;
    }

    float Layout::Measure(ImFont *font, float fontSize, const char *b, const char *e) {
        if (b == e) return 0.0f;
        return font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, b, e, nullptr).x;
    }

    void Layout::BuildLines(const std::vector<TextSegment> &segments, float wrapWidth, int tabColumns, float fontSize, std::vector<Line> &outLines) {
        outLines.clear();
        if (wrapWidth <= 0.0f) wrapWidth = FLT_MAX;

        // Reserve space to reduce reallocations
        outLines.reserve(std::max(1, (int)(segments.size() / 4))); // Estimate lines based on segments

        ImFont *font = ImGui::GetFont();
        ImFontBaked *baked = font ? font->GetFontBaked(fontSize) : nullptr;
        const float scale = (fontSize > 0.0f && baked) ? (fontSize / baked->Size) : 1.0f;
        static const char *kSpace = " ";
        const float spaceW = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, kSpace, kSpace + 1, nullptr).x;

        // Fast path: no wrapping requested and text contains no tabs/newlines.
        if (wrapWidth == FLT_MAX) {
            bool hasSpecial = false;
            for (const TextSegment &seg : segments) {
                const char *p = seg.begin, *e = seg.end;
                while (p < e) { char ch = *p++; if (ch == '\n' || ch == '\r' || ch == '\t') { hasSpecial = true; break; } }
                if (hasSpecial) break;
            }
            if (!hasSpecial) {
                Line lineFast; lineFast.spans.reserve(segments.size());
                float xsum = 0.0f;
                for (const TextSegment &seg : segments) {
                    float w = Measure(font, fontSize, seg.begin, seg.end);
                    lineFast.spans.push_back(Span{&seg, seg.begin, seg.end, w, false});
                    xsum += w;
                }
                lineFast.width = xsum;
                outLines.push_back(std::move(lineFast));
                return;
            }
        }

        Line line;
        line.spans.reserve(8); // Reserve space for spans to reduce reallocations
        float x = 0.0f;
        bool trimLeadingSpace = false;

        auto NewLine = [&]() {
            outLines.push_back(std::move(line));
            line = Line();
            line.spans.reserve(8); // Reserve space for new line
            x = 0.0f;
        };
        auto EmitSpan = [&](const TextSegment *seg, const char *b, const char *e, float w, bool isTab) {
            line.spans.push_back(Span{seg, b, e, w, isTab});
            x += w;
            if (x > line.width) line.width = x;
        };

        for (const TextSegment &seg : segments) {
            const char *p = seg.begin;
            const char *end = seg.end;
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
                        utf8_int32_t cc = 0;
                        const char *nn = (const char*) utf8codepoint((const utf8_int8_t*) q, &cc);
                        if (!nn || nn <= q) { q++; break; }
                        if (cc == ' ' || cc == 0x3000 || cc == '\t' || cc == '\n' || cc == '\r') break;
                        q = nn;
                    }
                }

                if (x == 0.0f && tokenIsSpace && trimLeadingSpace) { p = q; continue; }

                // Fast measurement of a short UTF-8 range using baked advances
                auto MeasureRangeFast = [&](const char* b, const char* e) -> float {
                    if (!baked) return Measure(font, fontSize, b, e);
                    float sum = 0.0f;
                    const char* s = b;
                    while (s < e) {
                        utf8_int32_t cp = 0;
                        const char* next = (const char*)utf8codepoint((const utf8_int8_t*)s, &cp);
                        if (!next || next <= s) next = s + 1;
                        float adv;
                        if ((unsigned)cp < (unsigned)baked->IndexAdvanceX.Size) {
                            adv = baked->IndexAdvanceX.Data[cp];
                            if (adv < 0.0f) adv = baked->GetCharAdvance((ImWchar)cp);
                        } else {
                            adv = baked->GetCharAdvance((ImWchar)cp);
                        }
                        sum += adv * scale;
                        s = next;
                    }
                    return sum;
                };

                auto WrapLine = [&]() { if (!line.spans.empty()) NewLine(); };
                float avail = wrapWidth - x;

                if (tokenIsSpace) {
                    float w = MeasureRangeFast(p, q);
                    if (w <= avail + 0.0001f) { EmitSpan(&seg, p, q, w, false); p = q; continue; }
                    if (x > 0.0f) { WrapLine(); trimLeadingSpace = true; p = q; continue; }
                    // If avail is too small to fit spaces at line start, just consume them without emitting.
                    p = q; continue;
                }
                // Quick width check for whole token to avoid unnecessary grapheme scanning
                float w = Measure(font, fontSize, p, q);
                if (w <= avail + 0.0001f) { EmitSpan(&seg, p, q, w, false); p = q; continue; }

                if (x > 0.0f) { WrapLine(); trimLeadingSpace = true; avail = wrapWidth; }

                // Split long token incrementally by grapheme, accumulate widths without extra allocations
                const char* slice_b = p;
                const char* cur = p;
                float acc = 0.0f;
                while (cur < q) {
                    const char* next = NextGrapheme(cur, q);
                    if (next <= cur) next = cur + 1;
                    float dw = MeasureRangeFast(cur, next);
                    if (acc + dw <= avail + 0.0001f) {
                        acc += dw;
                        cur = next;
                    } else {
                        if (cur == slice_b) {
                            // Force one grapheme to progress
                            EmitSpan(&seg, cur, next, dw, false);
                            cur = next;
                        } else {
                            EmitSpan(&seg, slice_b, cur, acc, false);
                        }
                        NewLine(); trimLeadingSpace = true; avail = wrapWidth;
                        slice_b = cur; acc = 0.0f;
                    }
                }
                if (cur > slice_b) {
                    EmitSpan(&seg, slice_b, cur, acc, false);
                }
                p = q; continue;
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
        const float usedSize = (fontSize > 0.0f) ? fontSize : ImGui::GetStyle().FontSizeBase;
        ImFontBaked *baked = font ? font->GetFontBaked(usedSize) : nullptr;

        const float ascent = baked ? std::max(0.0f, baked->Ascent) : usedSize * 0.8f;
        const float descent_mag = baked ? std::max(0.0f, -baked->Descent) : usedSize * 0.2f;
        const float baseline = lineTop + ascent;
        const float offset = std::clamp(descent_mag * 0.5f, 1.0f, std::max(1.0f, descent_mag - 1.0f));
        return baseline + offset;
    }

    float Metrics::StrikeY(float lineTop, float fontSize) {
        ImFont *font = ImGui::GetFont();
        const float usedSize = (fontSize > 0.0f) ? fontSize : ImGui::GetStyle().FontSizeBase;
        ImFontBaked *baked = font ? font->GetFontBaked(usedSize) : nullptr;
        const float ascent = baked ? std::max(0.0f, baked->Ascent) : usedSize * 0.8f;
        return lineTop + ascent * 0.6f;
    }

    float Metrics::Thickness(float fontSize) {
        const float t = std::round(fontSize * 0.0555555556f); // 1/18 = 0.0555555556
        return t < 1.0f ? 1.0f : (t > 4.0f ? 4.0f : t);
    }

    float CalculateHeight(const AnsiString &text, float wrapWidth, float fontSize, float lineSpacing, int tabColumns) {
        if (text.IsEmpty()) {
            return ImGui::GetTextLineHeightWithSpacing();
        }

        const float usedFontSize = (fontSize > 0.0f) ? fontSize : ImGui::GetStyle().FontSizeBase;
        ImFont *font = ImGui::GetFont();
        ImFontBaked *baked = font ? font->GetFontBaked(usedFontSize) : nullptr;
        const float ascent = baked ? std::max(0.0f, baked->Ascent) : usedFontSize * 0.8f;
        const float descentMag = baked ? std::max(0.0f, -baked->Descent) : usedFontSize * 0.2f;
        const float lineHeight = std::max(usedFontSize, ascent + descentMag);
        const float spacing = (lineSpacing >= 0.0f) ? lineSpacing : ImGui::GetStyle().ItemSpacing.y;

        std::vector<Layout::Line> lines;
        Layout::BuildLines(text.GetSegments(), wrapWidth, tabColumns, usedFontSize, lines);

        const float lineCount = lines.empty() ? 1.0f : static_cast<float>(lines.size());
        const float totalSpacing = spacing * std::max(0.0f, lineCount - 1.0f);
        return lineHeight * lineCount + totalSpacing;
    }
    ImVec2 CalculateSize(const AnsiString &text, float wrapWidth, float fontSize, float lineSpacing, int tabColumns) {
        if (text.IsEmpty()) {
            return {0.0f, ImGui::GetTextLineHeightWithSpacing()};
        }

        const float usedFontSize = (fontSize > 0.0f) ? fontSize : ImGui::GetStyle().FontSizeBase;
        ImFont *font = ImGui::GetFont();
        ImFontBaked *baked = font ? font->GetFontBaked(usedFontSize) : nullptr;
        const float ascent = baked ? std::max(0.0f, baked->Ascent) : usedFontSize * 0.8f;
        const float descentMag = baked ? std::max(0.0f, -baked->Descent) : usedFontSize * 0.2f;
        const float lineHeight = std::max(usedFontSize, ascent + descentMag);
        const float spacing = (lineSpacing >= 0.0f) ? lineSpacing : ImGui::GetStyle().ItemSpacing.y;

        std::vector<Layout::Line> lines;
        Layout::BuildLines(text.GetSegments(), wrapWidth, tabColumns, usedFontSize, lines);

        float maxWidth = 0.0f;
        for (const auto &line : lines) {
            maxWidth = std::max(maxWidth, line.width);
        }

        const float lineCount = lines.empty() ? 1.0f : static_cast<float>(lines.size());
        const float totalSpacing = spacing * std::max(0.0f, lineCount - 1.0f);
        const float height = lineHeight * lineCount + totalSpacing;
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
        const float usedSize = (fontSize > 0.0f) ? fontSize : ImGui::GetStyle().FontSizeBase;

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
                      float alpha, float fontSize, float lineSpacing, int tabColumns, const AnsiPalette *palette) {
        if (!palette)
            palette = &DefaultPalette();

        // Unify font size and baked data
        ImFont *font = ImGui::GetFont();
        const float usedFontSize = (fontSize > 0.0f) ? fontSize : ImGui::GetStyle().FontSizeBase;
        ImFontBaked *baked = font ? font->GetFontBaked(usedFontSize) : nullptr;

        // Ensure font atlas texture is bound for the duration of this text draw to avoid accidental use
        // of a previously-bound texture (which may cause random-looking glyphs on first line).
        bool pushedFontTex = false;
        if (font && font->ContainerAtlas) {
            drawList->PushTextureID(font->ContainerAtlas->TexRef);
            pushedFontTex = true;
        }

        // Pre-fetch line metrics
        const float ascent = baked ? std::max(0.0f, baked->Ascent) : usedFontSize * 0.8f;
        const float descentMag = baked ? std::max(0.0f, -baked->Descent) : usedFontSize * 0.2f;
        const float spacing = (lineSpacing >= 0.0f) ? lineSpacing : ImGui::GetStyle().ItemSpacing.y;
        const float lineHeight = std::max(usedFontSize, ascent + descentMag);
        const float lineStep = lineHeight + spacing;
        const float italicShear = ComputeItalicShear(usedFontSize);

        // Layout
        std::vector<Layout::Line> lines;
        Layout::BuildLines(text.GetSegments(), wrapWidth, tabColumns, usedFontSize, lines);

        ImGuiListClipper clipper;
        clipper.Begin((int)lines.size(), lineStep);
        while (clipper.Step()) {
            for (int lineIndex = clipper.DisplayStart; lineIndex < clipper.DisplayEnd; ++lineIndex) {
                const auto &line = lines[(size_t)lineIndex];
                const float lineTop = startPos.y + lineIndex * lineStep;
                const float lineBottom = lineTop + lineHeight;

                // Background pass (optional pre-scan fast path only when no wrap requested)
                auto draw_background_runs = [&]() {
                    struct BgRun { float x0, x1; ImU32 col; };
                    std::vector<BgRun> runs; runs.reserve(line.spans.size() / 2);
                    float xForBg = startPos.x; bool hasOpenRun = false; BgRun cur{};
                    for (const auto &sp : line.spans) {
                        const TextSegment *seg = sp.seg;
                        if (!seg) { xForBg += sp.width; continue; }
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
                            if (rc.italic) { const float pad = italicShear * (lineBottom - lineTop); italicPad = pad > 0.0f ? pad : 0.0f; }
                            const float x1 = xForBg + sp.width + italicPad;
                            if (hasOpenRun && cur.col == bg && std::abs(cur.x1 - x0) <= 0.25f) cur.x1 = x1;
                            else { if (hasOpenRun) runs.push_back(cur); cur = BgRun{x0, x1, bg}; hasOpenRun = true; }
                        } else { if (hasOpenRun) { runs.push_back(cur); hasOpenRun = false; } }
                        xForBg += sp.width;
                    }
                    if (hasOpenRun) runs.push_back(cur);
                    for (const BgRun &r : runs) drawList->AddRectFilled(ImVec2(r.x0, lineTop), ImVec2(r.x1, lineBottom), r.col);
                };

                if (wrapWidth == FLT_MAX) {
                    // If palette inactive and text has no true-color backgrounds, all backgrounds render as transparent -> skip entirely
                    const bool paletteInactive = !palette->IsActive();
                    if (paletteInactive && !text.HasTrueColorBackground() && !text.HasReverse()) {
                        // nothing to draw for backgrounds on this line
                    } else {
                        // Per-line fast-path: if all backgrounds are fully transparent, skip building runs
                        bool any_bg = false;
                        for (const auto &sp_check : line.spans) {
                            const TextSegment *seg_check = sp_check.seg;
                            if (!seg_check) continue;
                            if (sp_check.isTab || !(sp_check.b < sp_check.e)) continue;
                            ConsoleColor rc_check = seg_check->color.GetRendered();
                            ImU32 bgBaseCheck = rc_check.background;
                            if (rc_check.bgIsAnsi256 && rc_check.bgAnsiIndex >= 0) {
                                const_cast<AnsiPalette *>(palette)->EnsureInitialized();
                                if (palette->IsActive()) palette->GetColor(rc_check.bgAnsiIndex, bgBaseCheck);
                            }
                            ImU32 bgCheck = Color::ApplyAlpha(bgBaseCheck, alpha);
                            if (((bgCheck >> IM_COL32_A_SHIFT) & 0xFF) != 0) { any_bg = true; break; }
                        }
                        if (any_bg) draw_background_runs();
                    }
                } else {
                    // With wrapping, always build runs (cost amortized by fewer long lines)
                    draw_background_runs();
                }

                // Pass 2: Text and decoration lines
                bool any_decor = true;
                if (wrapWidth != FLT_MAX) {
                    any_decor = false;
                    for (const auto &sp_check : line.spans) {
                        const TextSegment *seg_check = sp_check.seg;
                        if (!seg_check) continue;
                        if (sp_check.isTab || !(sp_check.b < sp_check.e)) continue;
                        ConsoleColor rc_check = seg_check->color.GetRendered();
                        if (rc_check.underline || rc_check.doubleUnderline || rc_check.strikethrough) { any_decor = true; break; }
                    }
                }
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
                        // Use background color as text color; alpha will be applied once below.
                        fg = bgBase2;
                    }

                    fg = Color::ApplyAlpha(fg, alpha);
                    const bool fauxBold = rc.bold;
                    const bool italic = rc.italic;

                    if (!sp.isTab && sp.b < sp.e) {
                        AddTextStyled(drawList, font, usedFontSize, ImVec2(x, lineTop), fg, sp.b, sp.e, italic, fauxBold);

                        if (any_decor) {
                            // Calculate italic padding once if needed for decorations
                            float italicPadForDecorations = 0.0f;
                            if (rc.italic && (rc.underline || rc.strikethrough)) {
                                const float pad = italicShear * (lineBottom - lineTop);
                                italicPadForDecorations = pad > 0.0f ? pad : 0.0f;
                            }

                            if (rc.underline) {
                                float y = Metrics::UnderlineY(lineTop, usedFontSize);
                                float th = Metrics::Thickness(usedFontSize);
                                if ((static_cast<int>(th) & 1) != 0) y = floorf(y) + 0.5f;
                                else y = roundf(y);
                                drawList->AddLine(ImVec2(x, y), ImVec2(x + sp.width + italicPadForDecorations, y), fg, th);
                                if (rc.doubleUnderline) {
                                    float y2 = y + th + 1.0f;
                                    if ((static_cast<int>(th) & 1) != 0) y2 = floorf(y2) + 0.5f;
                                    else y2 = roundf(y2);
                                    drawList->AddLine(ImVec2(x, y2), ImVec2(x + sp.width + italicPadForDecorations, y2), fg, th);
                                }
                            }
                            if (rc.strikethrough) {
                                float y = Metrics::StrikeY(lineTop, usedFontSize);
                                float th = Metrics::Thickness(usedFontSize);
                                if ((static_cast<int>(th) & 1) != 0) y = floorf(y) + 0.5f;
                                else y = roundf(y);
                                drawList->AddLine(ImVec2(x, y), ImVec2(x + sp.width + italicPadForDecorations, y), fg, th);
                            }
                        }
                    }

                    x += sp.width;
                }
            }
        }

        if (pushedFontTex)
            drawList->PopTextureID();
    }
} // namespace AnsiText

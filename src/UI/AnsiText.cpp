#include "UI/AnsiText.h"

#include <algorithm>
#include <climits>
#include <cmath>

#include <utf8.h>

#include "imgui_internal.h"

#include "UI/AnsiPalette.h"
#include "StringUtils.h"

namespace AnsiText {
    namespace {
        struct ResolvedTextOptions {
            ImFont *font = nullptr;
            ImFontBaked *baked = nullptr;
            float fontSize = 0.0f;
            float wrapWidth = FLT_MAX;
            float lineSpacing = 0.0f;
            int tabColumns = DefaultTabColumns;
            float lineHeight = 0.0f;
        };

        class SgrParameterReader {
        public:
            SgrParameterReader(const char *sequence, std::size_t length)
                : m_Current(sequence), m_End(sequence + length) {}

            bool Read(int &value) {
                SkipSeparators();
                if (m_Current >= m_End || *m_Current < '0' || *m_Current > '9')
                    return false;

                int result = 0;
                while (m_Current < m_End && *m_Current >= '0' && *m_Current <= '9') {
                    const int digit = *m_Current - '0';
                    if (result > (INT_MAX - digit) / 10) {
                        SkipDigits();
                        return false;
                    }
                    result = result * 10 + digit;
                    ++m_Current;
                }
                value = result;
                return true;
            }

        private:
            void SkipSeparators() {
                while (m_Current < m_End && (*m_Current == ';' || *m_Current == ' '))
                    ++m_Current;
            }

            void SkipDigits() {
                while (m_Current < m_End && *m_Current >= '0' && *m_Current <= '9')
                    ++m_Current;
            }

            const char *m_Current;
            const char *m_End;
        };

        ResolvedTextOptions ResolveTextOptions(const TextOptions &options) {
            ResolvedTextOptions resolved;
            resolved.font = options.font ? options.font : ImGui::GetFont();
            if (!resolved.font)
                return resolved;

            resolved.fontSize = options.fontSize > 0.0f ? options.fontSize : ImGui::GetFontSize();
            resolved.baked = resolved.font->GetFontBaked(resolved.fontSize);
            resolved.wrapWidth = options.wrapWidth > 0.0f && options.wrapWidth < FLT_MAX ? options.wrapWidth : FLT_MAX;
            resolved.tabColumns = options.tabColumns > 0 ? options.tabColumns : DefaultTabColumns;

            const float ascent = resolved.baked ? std::max(0.0f, resolved.baked->Ascent) : resolved.fontSize * 0.8f;
            const float descent = resolved.baked ? std::max(0.0f, -resolved.baked->Descent) : resolved.fontSize * 0.2f;
            resolved.lineHeight = std::max(resolved.fontSize, ascent + descent);

            const float styleFontSize = ImGui::GetFontSize();
            const float scale = styleFontSize > 0.0f ? resolved.fontSize / styleFontSize : 1.0f;
            resolved.lineSpacing = options.lineSpacing >= 0.0f
                ? options.lineSpacing
                : ImGui::GetStyle().ItemSpacing.y * scale;
            return resolved;
        }

        ImU32 ApplyDim(ImU32 color);
        ImU32 ApplyAlpha(ImU32 color, float alpha);
        float DecorationThickness(float fontSize);
        float ComputeItalicShear(float fontSize);
        void AddTextStyled(ImDrawList *drawList, ImFont *font, ImFontBaked *baked, float fontSize,
                           float fontScale, float fontAscent, float italicShear,
                           const ImVec2 &position, ImU32 color,
                           const char *begin, const char *end, bool italic, bool fauxBold);
    }

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
    AnsiString::AnsiString(std::string &&text, const ConsoleColor &initialColor) {
        SetText(std::move(text), initialColor);
    }

    void AnsiString::RebindSegmentsPointers(const char *oldBase, const char *newBase) {
        if (!oldBase || !newBase) return;
        if (oldBase == newBase) return;
        for (auto &seg : m_Segments) {
            if (seg.begin && seg.end) {
                const std::ptrdiff_t beginOffset = seg.begin - oldBase;
                const std::ptrdiff_t endOffset = seg.end - oldBase;
                if (beginOffset >= 0 && endOffset >= beginOffset) {
                    seg.begin = newBase + beginOffset;
                    seg.end = newBase + endOffset;
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
        m_Revision = 1;
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
        ++m_Revision;
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
        m_Revision = 1;

        RebindSegmentsPointers(srcBase, m_OriginalText.c_str());
    }

    AnsiString &AnsiString::operator=(AnsiString &&other) noexcept {
        if (this == &other) return *this;

        const char *srcBase = other.m_OriginalText.c_str();
        m_OriginalText = std::move(other.m_OriginalText);
        m_Segments = std::move(other.m_Segments);
        m_HasAnsi256BG = other.m_HasAnsi256BG;
        m_HasTrueColorBG = other.m_HasTrueColorBG;
        m_HasReverse = other.m_HasReverse;
        ++m_Revision;

        RebindSegmentsPointers(srcBase, m_OriginalText.c_str());
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

    void AnsiString::SetText(std::string &&text, const ConsoleColor &initialColor) {
        if (text.empty()) { Clear(); return; }
        if (utf8valid(reinterpret_cast<const utf8_int8_t *>(text.c_str())) == nullptr) {
            AssignAndParse(std::move(text), initialColor);
        } else {
            std::wstring w = utils::AnsiToUtf16(text.c_str());
            AssignAndParse(utils::Utf16ToUtf8(w), initialColor);
        }
    }

    std::string AnsiString::GetPlainText(std::size_t begin, std::size_t end) const {
        begin = std::min(begin, m_OriginalText.size());
        end = std::min(end, m_OriginalText.size());
        if (end <= begin)
            return {};

        std::string plain;
        plain.reserve(end - begin);
        const char *base = m_OriginalText.data();
        for (const TextSegment &segment : m_Segments) {
            const std::size_t segmentBegin = static_cast<std::size_t>(segment.begin - base);
            const std::size_t segmentEnd = static_cast<std::size_t>(segment.end - base);
            if (segmentEnd <= begin)
                continue;
            if (segmentBegin >= end)
                break;
            const std::size_t copyBegin = std::max(begin, segmentBegin);
            const std::size_t copyEnd = std::min(end, segmentEnd);
            if (copyEnd > copyBegin)
                plain.append(base + copyBegin, copyEnd - copyBegin);
        }
        return plain;
    }

    void AnsiString::AssignAndParse(std::string &&text, const ConsoleColor &initialColor) {
        m_OriginalText = std::move(text);
        m_Segments.clear();
        ParseAnsiEscapeCodes(initialColor);
        ++m_Revision;
    }

    void AnsiString::Clear() {
        m_OriginalText.clear();
        m_Segments.clear();
        m_HasAnsi256BG = false;
        m_HasTrueColorBG = false;
        m_HasReverse = false;
        ++m_Revision;
    }

    void AnsiString::ParseAnsiEscapeCodes(const ConsoleColor &initialColor) {
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

        // The input has already been normalized to UTF-8. Treating raw 0x9B as
        // 8-bit CSI here would collide with valid UTF-8 continuation bytes.
        if (std::memchr(start, 0x1B, static_cast<std::size_t>(end - start)) == nullptr) {
            m_Segments.emplace_back(start, end, initialColor);
            return;
        }

        // General parser with zero-copy segments
        m_Segments.reserve(8);
        ConsoleColor currentColor = initialColor;
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
                    currentColor = ParseAnsiColorSequence(seqStart, static_cast<std::size_t>(q - seqStart), currentColor, &m_HasAnsi256BG, &m_HasTrueColorBG, &m_HasReverse);
                    p = q + 1;      // skip final 'm'
                    segStart = p;
                    continue;
                }
                // Not an SGR (or incomplete): fall through to treat bytes literally
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

    }

    ConsoleColor AnsiString::ParseAnsiColorSequence(const char *sequence, std::size_t length, const ConsoleColor &currentColor,
                                                    bool *out_hasAnsi256Bg, bool *out_hasTrueColorBg, bool *out_hasReverse) {
        if (length == 0) return ConsoleColor();
        ConsoleColor color = currentColor;

        SgrParameterReader parameters(sequence, length);
        int code = 0;
        while (parameters.Read(code)) {
            if (code == 0) { color = ConsoleColor(); continue; }

            if (code >= 30 && code <= 37) { color.fgIsAnsi256 = true; color.fgAnsiIndex = code - 30; continue; }
            if (code >= 40 && code <= 47) { color.bgIsAnsi256 = true; color.bgAnsiIndex = code - 40; if (out_hasAnsi256Bg) *out_hasAnsi256Bg = true; continue; }
            if (code >= 90 && code <= 97) { color.fgIsAnsi256 = true; color.fgAnsiIndex = (code - 90) + 8; continue; }
            if (code >= 100 && code <= 107){ color.bgIsAnsi256 = true; color.bgAnsiIndex = (code - 100) + 8; if (out_hasAnsi256Bg) *out_hasAnsi256Bg = true; continue; }

            if (code == 38 || code == 48) {
                const bool isBg = (code == 48);
                int mode = -1;
                if (!parameters.Read(mode)) break;
                if (mode == 5) {
                    int idx = 0; if (!parameters.Read(idx)) break;
                    idx = std::clamp(idx, 0, 255);
                    if (isBg) { color.bgIsAnsi256 = true; color.bgAnsiIndex = idx; if (out_hasAnsi256Bg) *out_hasAnsi256Bg = true; }
                    else      { color.fgIsAnsi256 = true; color.fgAnsiIndex = idx; }
                    continue;
                }
                if (mode == 2) {
                    int r=0,g=0,b=0; if (!parameters.Read(r) || !parameters.Read(g) || !parameters.Read(b)) break;
                    ImU32 v = GetRgbColor(r, g, b);
                    if (isBg) { color.background = v; color.bgIsAnsi256 = false; color.bgAnsiIndex = -1; if (out_hasTrueColorBg) *out_hasTrueColorBg = true; }
                    else      { color.foreground = v; color.fgIsAnsi256 = false; color.fgAnsiIndex = -1; }
                    continue;
                }
                // Unknown sub-mode: stop processing this sequence to avoid
                // misinterpreting its parameters as subsequent SGR codes.
                break;
            }

            if (code == 39) { color.foreground = ConsoleColor().foreground; color.fgIsAnsi256 = false; color.fgAnsiIndex = -1; continue; }
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
                    color.underline = true;
                    color.doubleUnderline = true;
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

    static const char *NextCodepoint(const char *current, const char *end,
                                     std::uint32_t &codepoint) {
        codepoint = 0;
        if (!current || current >= end)
            return current;

        utf8_int32_t decoded = 0;
        const char *next = reinterpret_cast<const char *>(
            utf8codepoint(reinterpret_cast<const utf8_int8_t *>(current), &decoded));
        codepoint = static_cast<std::uint32_t>(decoded);
        if (!next || next <= current)
            return current + 1;
        return std::min(next, end);
    }

    static std::size_t ClampTextOffset(const char *base, std::size_t size,
                                       const char *pointer) {
        if (!pointer || pointer <= base)
            return 0;
        return std::min(static_cast<std::size_t>(pointer - base), size);
    }

    static bool IsCombiningMark(std::uint32_t codepoint) {
        return (codepoint >= 0x0300 && codepoint <= 0x036F) ||
               (codepoint >= 0x1AB0 && codepoint <= 0x1AFF) ||
               (codepoint >= 0x1DC0 && codepoint <= 0x1DFF) ||
               (codepoint >= 0x20D0 && codepoint <= 0x20FF) ||
               (codepoint >= 0xFE20 && codepoint <= 0xFE2F);
    }
    static bool IsVariationSelector(std::uint32_t codepoint) {
        return (codepoint >= 0xFE00 && codepoint <= 0xFE0F) ||
               (codepoint >= 0xE0100 && codepoint <= 0xE01EF);
    }
    static bool IsEmojiModifier(std::uint32_t codepoint) {
        return codepoint >= 0x1F3FB && codepoint <= 0x1F3FF;
    }
    static bool IsRegionalIndicator(std::uint32_t codepoint) {
        return codepoint >= 0x1F1E6 && codepoint <= 0x1F1FF;
    }
    static bool IsEmojiTag(std::uint32_t codepoint) {
        return codepoint >= 0xE0020 && codepoint <= 0xE007F;
    }
    static bool IsClusterExtension(std::uint32_t codepoint) {
        return IsVariationSelector(codepoint) || IsCombiningMark(codepoint) ||
               IsEmojiModifier(codepoint) || IsEmojiTag(codepoint);
    }
    static bool IsZeroWidthJoiner(std::uint32_t codepoint) { return codepoint == 0x200D; }

    const char *PreparedText::NextGrapheme(const char *s, const char *end) {
        if (!s || s >= end)
            return s;

        std::uint32_t firstCodepoint = 0;
        const char *next = NextCodepoint(s, end, firstCodepoint);
        if (!next || next <= s)
            return std::min(s + 1, end);

        if (firstCodepoint == '\r' && next < end) {
            std::uint32_t following = 0;
            const char *afterFollowing = NextCodepoint(next, end, following);
            if (following == '\n' && afterFollowing > next)
                return afterFollowing;
        }

        if (IsRegionalIndicator(firstCodepoint) && next < end) {
            std::uint32_t following = 0;
            const char *afterFollowing = NextCodepoint(next, end, following);
            if (IsRegionalIndicator(following) && afterFollowing > next)
                next = afterFollowing;
        }

        while (next < end) {
            std::uint32_t codepoint = 0;
            const char *afterCodepoint = NextCodepoint(next, end, codepoint);
            if (!afterCodepoint || afterCodepoint <= next) {
                next = std::min(next + 1, end);
                break;
            }
            if (IsClusterExtension(codepoint)) {
                next = afterCodepoint;
                continue;
            }
            if (IsZeroWidthJoiner(codepoint)) {
                next = afterCodepoint;
                if (next >= end)
                    break;

                std::uint32_t joinedCodepoint = 0;
                const char *afterJoined = NextCodepoint(next, end, joinedCodepoint);
                if (!afterJoined || afterJoined <= next)
                    break;
                next = afterJoined;

                while (next < end) {
                    std::uint32_t extension = 0;
                    const char *afterExtension = NextCodepoint(next, end, extension);
                    if (!afterExtension || afterExtension <= next) {
                        next = std::min(next + 1, end);
                        break;
                    }
                    if (IsClusterExtension(extension)) {
                        next = afterExtension;
                        continue;
                    }
                    break;
                }
                continue;
            }
            break;
        }
        return next > end ? end : next;
    }

    float PreparedText::Measure(ImFont *font, float fontSize, const char *b, const char *e) {
        if (!font || b == e) return 0.0f;
        return font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, b, e, nullptr).x;
    }

    float PreparedText::MeasureFast(ImFont *font, ImFontBaked *baked, float fontSize, float scale,
                                    const char *begin, const char *end) {
        if (!baked)
            return Measure(font, fontSize, begin, end);

        float width = 0.0f;
        const char *current = begin;
        while (current < end) {
            utf8_int32_t codepoint = 0;
            const char *next = reinterpret_cast<const char *>(
                utf8codepoint(reinterpret_cast<const utf8_int8_t *>(current), &codepoint));
            if (!next || next <= current)
                next = current + 1;

            float advance;
            if (static_cast<unsigned>(codepoint) < static_cast<unsigned>(baked->IndexAdvanceX.Size)) {
                advance = baked->IndexAdvanceX.Data[codepoint];
                if (advance < 0.0f)
                    advance = baked->GetCharAdvance(static_cast<ImWchar>(codepoint));
            } else {
                advance = baked->GetCharAdvance(static_cast<ImWchar>(codepoint));
            }
            width += advance * scale;
            current = next;
        }
        return width;
    }

    void PreparedText::FinishLine(Line &line, std::vector<Line> &lines,
                                  float &lineWidth, const char *end) {
        if (!line.begin)
            line.begin = end;
        line.end = end;
        lines.push_back(std::move(line));
        line = Line{};
        line.spans.reserve(8);
        lineWidth = 0.0f;
    }

    void PreparedText::AppendSpan(Line &line, float &lineWidth, const TextSegment &segment,
                                  const char *begin, const char *end, float width, bool tab) {
        const ConsoleColor color = segment.color.GetRendered();
        if (!line.begin)
            line.begin = begin;
        line.end = end;
        line.spans.push_back(Span{begin, end, color, width, tab});
        line.hasDecorations |= color.underline || color.doubleUnderline || color.strikethrough;
        lineWidth += width;
        line.width = std::max(line.width, lineWidth);
    }

    void PreparedText::BuildLines(ImFont *font, const std::vector<TextSegment> &segments, float wrapWidth,
                                  int tabColumns, float fontSize, std::vector<Line> &outLines) {
        outLines.clear();
        if (wrapWidth <= 0.0f) wrapWidth = FLT_MAX;
        if (!font) return;

        // Reserve space to reduce reallocations
        outLines.reserve(std::max(1, (int)(segments.size() / 4))); // Estimate lines based on segments

        ImFontBaked *baked = font ? font->GetFontBaked(fontSize) : nullptr;
        const float scale = (fontSize > 0.0f && baked) ? (fontSize / baked->Size) : 1.0f;
        static constexpr char SpaceText[] = " ";
        const float spaceW = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, SpaceText, SpaceText + 1, nullptr).x;

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
                    const ConsoleColor color = seg.color.GetRendered();
                    if (!lineFast.begin)
                        lineFast.begin = seg.begin;
                    lineFast.end = seg.end;
                    lineFast.spans.push_back(Span{seg.begin, seg.end, color, w, false});
                    lineFast.hasDecorations |= color.underline || color.doubleUnderline || color.strikethrough;
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

        for (const TextSegment &seg : segments) {
            const char *p = seg.begin;
            const char *end = seg.end;
            while (p < end) {
                if (*p == '\n') { FinishLine(line, outLines, x, p); trimLeadingSpace = false; ++p; continue; }
                if (*p == '\r') { x = 0.0f; ++p; continue; }
                if (*p == '\t') {
                    const float cols = static_cast<float>(tabColumns > 0 ? tabColumns : DefaultTabColumns);
                    const float tabW = spaceW * cols;
                    const float nextTab = (static_cast<int>(x / tabW) + 1) * tabW;
                    const float w = nextTab - x;
                    if (x > 0.0f && (x + w) > wrapWidth + 0.0001f) {
                        FinishLine(line, outLines, x, p);
                        trimLeadingSpace = true;
                    }
                    AppendSpan(line, x, seg, p, p + 1, w, true);
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

                float avail = wrapWidth - x;

                if (tokenIsSpace) {
                    float w = MeasureFast(font, baked, fontSize, scale, p, q);
                    if (w <= avail + 0.0001f) { AppendSpan(line, x, seg, p, q, w, false); p = q; continue; }
                    if (x > 0.0f) {
                        if (!line.spans.empty())
                            FinishLine(line, outLines, x, p);
                        trimLeadingSpace = true;
                        p = q;
                        continue;
                    }
                    // If avail is too small to fit spaces at line start, just consume them without emitting.
                    p = q; continue;
                }
                // Quick width check for whole token to avoid unnecessary grapheme scanning
                float w = Measure(font, fontSize, p, q);
                if (w <= avail + 0.0001f) { AppendSpan(line, x, seg, p, q, w, false); p = q; continue; }

                if (x > 0.0f) {
                    if (!line.spans.empty())
                        FinishLine(line, outLines, x, p);
                    trimLeadingSpace = true;
                    avail = wrapWidth;
                }

                // Split long token incrementally by grapheme, accumulate widths without extra allocations
                const char* slice_b = p;
                const char* cur = p;
                float acc = 0.0f;
                while (cur < q) {
                    const char* next = NextGrapheme(cur, q);
                    if (next <= cur) next = cur + 1;
                    float dw = MeasureFast(font, baked, fontSize, scale, cur, next);
                    if (acc + dw <= avail + 0.0001f) {
                        acc += dw;
                        cur = next;
                    } else {
                        if (cur == slice_b) {
                            // Force one grapheme to progress
                            AppendSpan(line, x, seg, cur, next, dw, false);
                            cur = next;
                        } else {
                            AppendSpan(line, x, seg, slice_b, cur, acc, false);
                        }
                        FinishLine(line, outLines, x, cur); trimLeadingSpace = true; avail = wrapWidth;
                        slice_b = cur; acc = 0.0f;
                    }
                }
                if (cur > slice_b) {
                    AppendSpan(line, x, seg, slice_b, cur, acc, false);
                }
                p = q; continue;
            }
        }

        if (!line.spans.empty() || outLines.empty()) {
            const char *end = line.end;
            if (!end && !segments.empty())
                end = segments.back().end;
            if (!line.begin)
                line.begin = end;
            line.end = end;
            outLines.push_back(std::move(line));
        }
    }

    void PreparedText::Clear() {
        m_Source = nullptr;
        m_SourceRevision = 0;
        m_Context = nullptr;
        m_Font = nullptr;
        m_Baked = nullptr;
        m_BakedId = 0;
        m_FontSize = 0.0f;
        m_FontScale = 1.0f;
        m_FontAscent = 0.0f;
        m_WrapWidth = 0.0f;
        m_LineSpacing = 0.0f;
        m_LineHeight = 0.0f;
        m_LineStep = 0.0f;
        m_ItalicShear = 0.0f;
        m_DecorationThickness = 0.0f;
        m_UnderlineOffset = 0.0f;
        m_StrikeOffset = 0.0f;
        m_TabColumns = 0;
        m_UsesAnsiPalette = false;
        m_MayHaveBackground = false;
        m_Size = {};
        m_Lines.clear();
    }

    bool PreparedText::Matches(const AnsiString &text, const TextOptions &options) const {
        if (m_Source != &text || m_SourceRevision != text.m_Revision)
            return false;

        const ResolvedTextOptions resolved = ResolveTextOptions(options);
        const ImGuiID bakedId = resolved.baked ? resolved.baked->BakedId : 0;
        return m_Context == ImGui::GetCurrentContext() && m_Font == resolved.font && m_BakedId == bakedId &&
               std::abs(m_FontSize - resolved.fontSize) < 1e-3f &&
               std::abs(m_WrapWidth - resolved.wrapWidth) < 0.5f &&
               std::abs(m_LineSpacing - resolved.lineSpacing) < 0.5f && m_TabColumns == resolved.tabColumns;
    }

    bool PreparedText::Prepare(const AnsiString &text, const TextOptions &options) {
        if (Matches(text, options))
            return true;

        Clear();
        const ResolvedTextOptions resolved = ResolveTextOptions(options);
        if (!resolved.font)
            return false;

        m_Source = &text;
        m_SourceRevision = text.m_Revision;
        m_Context = ImGui::GetCurrentContext();
        m_Font = resolved.font;
        m_Baked = resolved.baked;
        m_BakedId = resolved.baked ? resolved.baked->BakedId : 0;
        m_FontSize = resolved.fontSize;
        m_FontScale = resolved.baked && resolved.baked->Size > 0.0f ? m_FontSize / resolved.baked->Size : 1.0f;
        m_WrapWidth = resolved.wrapWidth;
        m_LineSpacing = resolved.lineSpacing;
        m_LineHeight = resolved.lineHeight;
        m_LineStep = m_LineHeight + m_LineSpacing;
        m_ItalicShear = ComputeItalicShear(m_FontSize);
        const float ascent = resolved.baked ? std::max(0.0f, resolved.baked->Ascent) : m_FontSize * 0.8f;
        const float descent = resolved.baked ? std::max(0.0f, -resolved.baked->Descent) : m_FontSize * 0.2f;
        m_FontAscent = ascent;
        m_DecorationThickness = DecorationThickness(m_FontSize);
        m_UnderlineOffset = ascent + std::clamp(descent * 0.5f, 1.0f, std::max(1.0f, descent - 1.0f));
        m_StrikeOffset = ascent * 0.6f;
        m_TabColumns = resolved.tabColumns;
        m_MayHaveBackground = text.m_HasAnsi256BG || text.m_HasTrueColorBG || text.m_HasReverse;

        if (text.IsEmpty()) {
            m_Size = ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing());
            return true;
        }

        BuildLines(m_Font, text.GetSegments(), m_WrapWidth, m_TabColumns, m_FontSize, m_Lines);
        float width = 0.0f;
        for (const Line &line : m_Lines) {
            width = std::max(width, line.width);
            if (!m_UsesAnsiPalette) {
                for (const Span &span : line.spans) {
                    if (span.color.fgIsAnsi256 || span.color.bgIsAnsi256) {
                        m_UsesAnsiPalette = true;
                        break;
                    }
                }
            }
        }
        const float lineCount = m_Lines.empty() ? 1.0f : static_cast<float>(m_Lines.size());
        m_Size = ImVec2(width, m_LineHeight * lineCount + m_LineSpacing * std::max(0.0f, lineCount - 1.0f));
        return true;
    }

    std::size_t PreparedText::HitTest(const ImVec2 &position) const {
        if (!m_Source || m_Source->m_Revision != m_SourceRevision || m_Lines.empty())
            return 0;

        const char *base = m_Source->m_OriginalText.data();
        const std::size_t sourceSize = m_Source->m_OriginalText.size();
        const int lastLine = static_cast<int>(m_Lines.size()) - 1;
        const int lineIndex = m_LineStep > 0.0f
            ? std::clamp(static_cast<int>(std::floor(position.y / m_LineStep)), 0, lastLine)
            : 0;
        const Line &line = m_Lines[static_cast<std::size_t>(lineIndex)];

        if (position.x <= 0.0f || line.spans.empty())
            return ClampTextOffset(base, sourceSize, line.begin);

        float x = 0.0f;
        for (const Span &span : line.spans) {
            const float spanEndX = x + span.width;
            if (position.x <= spanEndX) {
                if (span.tab)
                    return ClampTextOffset(
                        base, sourceSize,
                        position.x < x + span.width * 0.5f ? span.begin : span.end);

                float cursorX = x;
                const char *current = span.begin;
                while (current < span.end) {
                    const char *next = NextGrapheme(current, span.end);
                    if (next <= current)
                        next = current + 1;
                    const float width = MeasureFast(
                        m_Font, m_Baked, m_FontSize, m_FontScale, current, next);
                    if (position.x < cursorX + width * 0.5f)
                        return ClampTextOffset(base, sourceSize, current);
                    cursorX += width;
                    current = next;
                }
                return ClampTextOffset(base, sourceSize, span.end);
            }
            x = spanEndX;
        }
        return ClampTextOffset(base, sourceSize, line.end);
    }

    PreparedText::VisibleLineRange PreparedText::GetVisibleLineRange(
        const ImDrawList *drawList, const ImVec2 &position) const {
        VisibleLineRange range{0, static_cast<int>(m_Lines.size())};
        if (!drawList || m_LineStep <= 0.0f || range.end == 0)
            return range;

        const ImVec2 clipMin = drawList->GetClipRectMin();
        const ImVec2 clipMax = drawList->GetClipRectMax();
        range.begin = static_cast<int>(
            std::floor((clipMin.y - position.y - m_LineHeight) / m_LineStep)) + 1;
        const float exclusiveClipMaxY = std::nextafter(clipMax.y, -FLT_MAX);
        range.end = static_cast<int>(
            std::floor((exclusiveClipMaxY - position.y) / m_LineStep)) + 1;
        range.begin = std::clamp(range.begin, 0, static_cast<int>(m_Lines.size()));
        range.end = std::clamp(range.end, range.begin, static_cast<int>(m_Lines.size()));
        return range;
    }

    void PreparedText::DrawSelection(ImDrawList *drawList, const ImVec2 &position,
                                     std::size_t begin, std::size_t end, ImU32 color) const {
        if (!drawList || !m_Source || m_Source->m_Revision != m_SourceRevision || begin == end)
            return;

        const char *base = m_Source->m_OriginalText.data();
        const std::size_t sourceSize = m_Source->m_OriginalText.size();
        begin = std::min(begin, sourceSize);
        end = std::min(end, sourceSize);
        if (end < begin)
            std::swap(begin, end);

        const VisibleLineRange visible = GetVisibleLineRange(drawList, position);
        for (int lineIndex = visible.begin; lineIndex < visible.end; ++lineIndex) {
            const Line &line = m_Lines[static_cast<std::size_t>(lineIndex)];
            const std::size_t lineBegin = ClampTextOffset(base, sourceSize, line.begin);
            const std::size_t lineEnd = ClampTextOffset(base, sourceSize, line.end);
            if (end <= lineBegin || begin >= lineEnd)
                continue;

            float x = position.x;
            float selectionStart = FLT_MAX;
            float selectionEnd = -FLT_MAX;
            for (const Span &span : line.spans) {
                const std::size_t spanBegin = static_cast<std::size_t>(span.begin - base);
                const std::size_t spanEnd = static_cast<std::size_t>(span.end - base);
                if (spanEnd <= begin) {
                    x += span.width;
                    continue;
                }
                if (spanBegin >= end)
                    break;

                const std::size_t selectedBegin = std::max(begin, spanBegin);
                const std::size_t selectedEnd = std::min(end, spanEnd);
                if (selectedEnd > selectedBegin) {
                    float selectedStartX = x;
                    float selectedEndX = x + span.width;
                    if (!span.tab) {
                        if (selectedBegin > spanBegin) {
                            selectedStartX += MeasureFast(
                                m_Font, m_Baked, m_FontSize, m_FontScale,
                                span.begin, base + selectedBegin);
                        }
                        if (selectedEnd < spanEnd) {
                            selectedEndX = x + MeasureFast(
                                m_Font, m_Baked, m_FontSize, m_FontScale,
                                span.begin, base + selectedEnd);
                        }
                    }
                    selectionStart = std::min(selectionStart, selectedStartX);
                    selectionEnd = std::max(selectionEnd, selectedEndX);
                }
                x += span.width;
            }

            if (selectionEnd > selectionStart) {
                const float top = position.y + static_cast<float>(lineIndex) * m_LineStep;
                drawList->AddRectFilled(
                    ImVec2(selectionStart, top),
                    ImVec2(selectionEnd, top + m_LineHeight), color);
            }
        }
    }

    namespace {
        ImU32 ApplyDim(ImU32 color) {
            const ImU32 red = ((color >> IM_COL32_R_SHIFT) & 0xFF) / 2;
            const ImU32 green = ((color >> IM_COL32_G_SHIFT) & 0xFF) / 2;
            const ImU32 blue = ((color >> IM_COL32_B_SHIFT) & 0xFF) / 2;
            const ImU32 alpha = (color >> IM_COL32_A_SHIFT) & 0xFF;
            return IM_COL32(red, green, blue, alpha);
        }

        ImU32 ApplyAlpha(ImU32 color, float alpha) {
            const ImU32 originalAlpha = (color >> IM_COL32_A_SHIFT) & 0xFF;
            if (originalAlpha == 0)
                return 0;
            const ImU32 resolvedAlpha = static_cast<ImU32>(
                std::clamp(alpha, 0.0f, 1.0f) * static_cast<float>(originalAlpha));
            constexpr ImU32 AlphaMask = static_cast<ImU32>(0xFF) << IM_COL32_A_SHIFT;
            return (color & ~AlphaMask) | (resolvedAlpha << IM_COL32_A_SHIFT);
        }

        float DecorationThickness(float fontSize) {
            return std::clamp(std::round(fontSize / 18.0f), 1.0f, 4.0f);
        }
    }

    ImVec2 CalcTextSize(const AnsiString &text, const TextOptions &options) {
        PreparedText prepared;
        return prepared.Prepare(text, options) ? prepared.GetSize() : ImVec2();
    }

    // =============================================================================
    // Renderer Implementation
    // =============================================================================

    namespace {
        constexpr float ItalicShearBase = 0.16f;
        constexpr float ItalicShearSizeMin = 12.0f;
        constexpr float ItalicShearSizeMax = 36.0f;
        constexpr float ItalicShearFactorMin = 0.85f;
        constexpr float ItalicShearFactorMax = 1.20f;
        constexpr float BoldBaseOffset = 0.35f;
        constexpr float BoldAlphaScale = 0.30f;
        constexpr float BoldSizeMin = 12.0f;
        constexpr float BoldSizeMax = 36.0f;
        constexpr float BoldOffsetScaleMin = 0.6f;
        constexpr float BoldOffsetScaleMax = 1.0f;
        constexpr float SmallFontThreshold = 14.0f;
        constexpr float SmallFontScale = 0.85f;
    } // namespace

    AnsiPalette &Renderer::DefaultPalette() {
        static AnsiPalette palette;
        return palette;
    }

    namespace {
        float ComputeItalicShear(float fontSize) {
            const float clampedSize = std::clamp(fontSize, ItalicShearSizeMin, ItalicShearSizeMax);
            const float position = (clampedSize - ItalicShearSizeMin) / (ItalicShearSizeMax - ItalicShearSizeMin);
            const float factor = ItalicShearFactorMin + (ItalicShearFactorMax - ItalicShearFactorMin) * position;
            return ItalicShearBase * factor;
        }

        static float ComputeBoldOffsetScale(float fontSize) {
            const float clampedSize = std::clamp(fontSize, BoldSizeMin, BoldSizeMax);
            const float position = (clampedSize - BoldSizeMin) / (BoldSizeMax - BoldSizeMin);
            return BoldOffsetScaleMin + (BoldOffsetScaleMax - BoldOffsetScaleMin) * position;
        }

        struct ItalicTextMetrics {
            ImFontBaked *baked = nullptr;
            float scale = 1.0f;
            float shear = 0.0f;
            float ascent = 0.0f;
        };

        static void DrawItalicText(ImDrawList *drawList, const ItalicTextMetrics &metrics, const ImVec2 &position,
                                   ImU32 color, const char *begin, const char *end) {
            if ((color & IM_COL32_A_MASK) == 0) return;

            ImFontBaked &baked = *metrics.baked;
            const float anchorY = position.y + metrics.ascent;
            const char *current = begin;
            float x = position.x;
            while (current < end) {
                utf8_int32_t codepoint = 0;
                const char *next = reinterpret_cast<const char *>(
                    utf8codepoint(reinterpret_cast<const utf8_int8_t *>(current), &codepoint));
                if (!next || next <= current) next = current + 1;

                const ImFontGlyph *glyph = baked.FindGlyph(static_cast<ImWchar>(codepoint));
                if (glyph && glyph->Visible) {
                    const float x1 = x + glyph->X0 * metrics.scale;
                    const float x2 = x + glyph->X1 * metrics.scale;
                    const float y1 = position.y + glyph->Y0 * metrics.scale;
                    const float y2 = position.y + glyph->Y1 * metrics.scale;
                    const float topOffset = metrics.shear * (anchorY - y1);
                    const float bottomOffset = metrics.shear * (anchorY - y2);
                    drawList->PrimReserve(6, 4);
                    drawList->PrimQuadUV(ImVec2(x1 + topOffset, y1), ImVec2(x2 + topOffset, y1),
                                         ImVec2(x2 + bottomOffset, y2), ImVec2(x1 + bottomOffset, y2),
                                         ImVec2(glyph->U0, glyph->V0), ImVec2(glyph->U1, glyph->V0),
                                         ImVec2(glyph->U1, glyph->V1), ImVec2(glyph->U0, glyph->V1), color);
                    x += glyph->AdvanceX * metrics.scale;
                } else {
                    x += baked.GetCharAdvance(static_cast<ImWchar>(codepoint)) * metrics.scale;
                }
                current = next;
            }
        }

        static void DrawStyledTextPass(ImDrawList *drawList, ImFont *font, float fontSize,
                                       const ItalicTextMetrics &italicMetrics, const ImVec2 &position, ImU32 color,
                                       const char *begin, const char *end) {
            if (italicMetrics.baked)
                DrawItalicText(drawList, italicMetrics, position, color, begin, end);
            else
                drawList->AddText(font, fontSize, position, color, begin, end);
        }

        void AddTextStyled(ImDrawList *drawList, ImFont *font, ImFontBaked *baked, float fontSize, float fontScale,
                           float fontAscent, float italicShear, const ImVec2 &position, ImU32 color, const char *begin,
                           const char *end, bool italic, bool fauxBold) {
            if (!drawList || !font || !begin || begin >= end || (color & IM_COL32_A_MASK) == 0) return;

            ItalicTextMetrics italicMetrics;
            if (italic && baked) {
                italicMetrics.baked = baked;
                italicMetrics.scale = fontScale;
                italicMetrics.shear = italicShear;
                italicMetrics.ascent = fontAscent;
            }

            DrawStyledTextPass(drawList, font, fontSize, italicMetrics, position, color, begin, end);
            if (!fauxBold) return;

            float pixelOffset = std::clamp(BoldBaseOffset * ComputeBoldOffsetScale(fontSize), 0.30f, 0.60f);
            if (fontSize <= SmallFontThreshold) pixelOffset = std::max(BoldBaseOffset, pixelOffset * SmallFontScale);
            const float baseAlpha = ((color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
            float boldAlpha = std::clamp(baseAlpha * BoldAlphaScale, 0.0f, 1.0f);
            if (fontSize <= SmallFontThreshold) boldAlpha *= SmallFontScale;

            const ImU32 alpha = static_cast<ImU32>(std::round(boldAlpha * 255.0f));
            if (alpha == 0) return;
            const ImU32 boldColor = (color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
            DrawStyledTextPass(drawList, font, fontSize, italicMetrics, ImVec2(position.x + pixelOffset, position.y),
                               boldColor, begin, end);
            DrawStyledTextPass(drawList, font, fontSize, italicMetrics, ImVec2(position.x - pixelOffset, position.y),
                               boldColor, begin, end);
            DrawStyledTextPass(drawList, font, fontSize, italicMetrics, ImVec2(position.x, position.y + pixelOffset),
                               boldColor, begin, end);
            DrawStyledTextPass(drawList, font, fontSize, italicMetrics, ImVec2(position.x, position.y - pixelOffset),
                               boldColor, begin, end);
        }

        static ImU32 ResolveBackgroundColor(const ConsoleColor &color, const AnsiPalette *palette, float alpha) {
            ImU32 background = color.background;
            if (color.bgIsAnsi256 && color.bgAnsiIndex >= 0 && palette->IsActive())
                palette->GetColor(color.bgAnsiIndex, background);
            return ApplyAlpha(background, alpha);
        }
    } // namespace

    void PreparedText::DrawBackgroundRuns(ImDrawList *drawList, const Line &line, float startX,
                                          float lineTop, float lineBottom, float italicShear,
                                          const AnsiPalette *palette, float alpha) {
        bool hasRun = false;
        float runStart = 0.0f;
        float runEnd = 0.0f;
        ImU32 runColor = 0;
        float x = startX;
        for (const Span &span : line.spans) {
            const ImU32 background = ResolveBackgroundColor(span.color, palette, alpha);
            const bool visible = !span.tab && span.begin < span.end && (background & IM_COL32_A_MASK) != 0;
            if (visible) {
                const float italicPadding = span.color.italic
                    ? std::max(0.0f, italicShear * (lineBottom - lineTop))
                    : 0.0f;
                const float spanEnd = x + span.width + italicPadding;
                if (hasRun && runColor == background && std::abs(runEnd - x) <= 0.25f) {
                    runEnd = spanEnd;
                } else {
                    if (hasRun)
                        drawList->AddRectFilled(ImVec2(runStart, lineTop), ImVec2(runEnd, lineBottom), runColor);
                    runStart = x;
                    runEnd = spanEnd;
                    runColor = background;
                    hasRun = true;
                }
            } else if (hasRun) {
                drawList->AddRectFilled(ImVec2(runStart, lineTop), ImVec2(runEnd, lineBottom), runColor);
                hasRun = false;
            }
            x += span.width;
        }
        if (hasRun)
            drawList->AddRectFilled(ImVec2(runStart, lineTop), ImVec2(runEnd, lineBottom), runColor);
    }

    void PreparedText::Draw(ImDrawList *drawList, const ImVec2 &startPos, float alpha,
                            const AnsiPalette *palette) const {
        if (!drawList || !m_Source || m_Source->m_Revision != m_SourceRevision || m_Source->IsEmpty() || !m_Font)
            return;

        alpha = std::clamp(alpha, 0.0f, 1.0f);
        palette = palette ? palette : &Renderer::DefaultPalette();

        if (alpha <= 0.0f)
            return;

        bool pushedFontTex = false;
        if (m_Font->OwnerAtlas) {
            drawList->PushTextureID(m_Font->OwnerAtlas->TexRef);
            pushedFontTex = true;
        }

        const VisibleLineRange visible = GetVisibleLineRange(drawList, startPos);

        if (m_UsesAnsiPalette)
            const_cast<AnsiPalette *>(palette)->EnsureInitialized();

        for (int lineIndex = visible.begin; lineIndex < visible.end; ++lineIndex) {
            const Line &line = m_Lines[static_cast<std::size_t>(lineIndex)];
            const float lineTop = startPos.y + lineIndex * m_LineStep;
            const float lineBottom = lineTop + m_LineHeight;

            if (m_MayHaveBackground)
                DrawBackgroundRuns(drawList, line, startPos.x, lineTop, lineBottom, m_ItalicShear, palette, alpha);

            // Pass 2: Text and decoration lines
            float underlineY = lineTop + m_UnderlineOffset;
            float strikeY = lineTop + m_StrikeOffset;
            if (line.hasDecorations) {
                if ((static_cast<int>(m_DecorationThickness) & 1) != 0) {
                    underlineY = std::floor(underlineY) + 0.5f;
                    strikeY = std::floor(strikeY) + 0.5f;
                } else {
                    underlineY = std::round(underlineY);
                    strikeY = std::round(strikeY);
                }
            }
            float x = startPos.x;
            for (const Span &span : line.spans) {
                const ConsoleColor &color = span.color;

                ImU32 foreground = color.foreground;
                if (color.fgIsAnsi256 && color.fgAnsiIndex >= 0) {
                    if (palette->IsActive())
                        palette->GetColor(color.fgAnsiIndex, foreground);
                }
                if (color.dim)
                    foreground = ApplyDim(foreground);
                if (color.hidden) {
                    ImU32 background = color.background;
                    if (color.bgIsAnsi256 && color.bgAnsiIndex >= 0) {
                        if (palette->IsActive())
                            palette->GetColor(color.bgAnsiIndex, background);
                    }
                    foreground = background;
                }

                foreground = ApplyAlpha(foreground, alpha);

                if (!span.tab && span.begin < span.end) {
                    AddTextStyled(drawList, m_Font, m_Baked, m_FontSize, m_FontScale, m_FontAscent,
                                  m_ItalicShear, ImVec2(x, lineTop), foreground,
                                  span.begin, span.end, color.italic, color.bold);

                    if (line.hasDecorations) {
                        float italicPadForDecorations = 0.0f;
                        if (color.italic && (color.underline || color.strikethrough)) {
                            const float pad = m_ItalicShear * (lineBottom - lineTop);
                            italicPadForDecorations = pad > 0.0f ? pad : 0.0f;
                        }

                        if (color.underline) {
                            drawList->AddLine(ImVec2(x, underlineY),
                                              ImVec2(x + span.width + italicPadForDecorations, underlineY),
                                              foreground, m_DecorationThickness);
                            if (color.doubleUnderline) {
                                float secondUnderlineY = underlineY + m_DecorationThickness + 1.0f;
                                if ((static_cast<int>(m_DecorationThickness) & 1) != 0)
                                    secondUnderlineY = std::floor(secondUnderlineY) + 0.5f;
                                else
                                    secondUnderlineY = std::round(secondUnderlineY);
                                drawList->AddLine(ImVec2(x, secondUnderlineY),
                                                  ImVec2(x + span.width + italicPadForDecorations, secondUnderlineY),
                                                  foreground, m_DecorationThickness);
                            }
                        }
                        if (color.strikethrough) {
                            drawList->AddLine(ImVec2(x, strikeY),
                                              ImVec2(x + span.width + italicPadForDecorations, strikeY),
                                              foreground, m_DecorationThickness);
                        }
                    }
                }

                x += span.width;
            }
        }

        if (pushedFontTex)
            drawList->PopTextureID();
    }

    void Renderer::DrawText(ImDrawList *drawList, const AnsiString &text, const ImVec2 &startPos, const TextOptions &options) {
        PreparedText prepared;
        if (prepared.Prepare(text, options))
            prepared.Draw(drawList, startPos, options.alpha, options.palette);
    }
} // namespace AnsiText

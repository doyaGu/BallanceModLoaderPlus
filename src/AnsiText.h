#ifndef BML_ANSITEXT_H
#define BML_ANSITEXT_H

#include <vector>
#include <string>

#include "imgui.h"

// Forward declaration
class AnsiPalette;

/**
 * AnsiText - ANSI escape code parsing and rendering module
 * 
 * Provides terminal-like text rendering with ANSI color and formatting support.
 */
namespace AnsiText {
    // Default tab columns (historical terminal default).
    static constexpr int kDefaultTabColumns = 8;

    // SGR 21 policy (ECMA-48: commonly double-underline; some terminals use it as bold-off)
    enum class Sgr21Policy {
        DoubleUnderline,
        ResetBoldDim,
    };

    // ANSI color and formatting state
    struct ConsoleColor {
        // Packed RGBA colors used when true-color is specified or after resolution.
        // For 256-color indices, *_IsAnsi256 is true and *_AnsiIndex stores [0..255].
        ImU32 foreground = IM_COL32_WHITE;
        ImU32 background = IM_COL32_BLACK_TRANS;

        // Origin markers for 256-color palette application
        bool fgIsAnsi256 = false;
        bool bgIsAnsi256 = false;
        int fgAnsiIndex = -1; // valid iff fgIsAnsi256
        int bgAnsiIndex = -1; // valid iff bgIsAnsi256

        // Styling flags (subset of ECMA-48 SGR commonly implemented by terminals)
        bool bold = false;
        bool italic = false;
        bool underline = false;
        bool doubleUnderline = false; // SGR 21 policy-dependent
        bool strikethrough = false;
        bool dim = false;
        bool hidden = false;
        bool reverse = false;

        ConsoleColor() = default;
        explicit ConsoleColor(ImU32 fg) : foreground(fg) {}
        ConsoleColor(ImU32 fg, ImU32 bg) : foreground(fg), background(bg) {}

        // Returns the final colors after applying reverse video and "hidden".
        ConsoleColor GetRendered() const;

        // Comparisons for segment fusion
        friend bool operator==(const ConsoleColor& a, const ConsoleColor& b) {
            return a.foreground==b.foreground && a.background==b.background &&
                   a.fgIsAnsi256==b.fgIsAnsi256 && a.bgIsAnsi256==b.bgIsAnsi256 &&
                   a.fgAnsiIndex==b.fgAnsiIndex && a.bgAnsiIndex==b.bgAnsiIndex &&
                   a.bold==b.bold && a.dim==b.dim && a.italic==b.italic &&
                   a.underline==b.underline && a.doubleUnderline==b.doubleUnderline && a.strikethrough==b.strikethrough &&
                   a.hidden==b.hidden && a.reverse==b.reverse;
        }
        friend bool operator!=(const ConsoleColor& a, const ConsoleColor& b) { return !(a==b); }
    };

    // Text segment with associated ANSI formatting
    // Zero-copy: hold begin/end pointers into AnsiString::m_OriginalText
    struct TextSegment {
        const char *begin = nullptr;
        const char *end = nullptr;
        ConsoleColor color;

        TextSegment() = default;
        TextSegment(const char *b, const char *e, ConsoleColor c) : begin(b), end(e), color(c) {}
    };

    // Parsed ANSI text ready for rendering
    class AnsiString {
    public:
        AnsiString() = default;
        explicit AnsiString(const char *text);

        // Parses text, producing zero-copy segments and fusing adjacent-equal styles.
        void SetText(const char *text);

        const std::string &GetOriginalText() const { return m_OriginalText; }
        const std::vector<TextSegment> &GetSegments() const { return m_Segments; }
        bool HasAnsi256Background() const { return m_HasAnsi256BG; }
        bool HasTrueColorBackground() const { return m_HasTrueColorBG; }
        bool HasReverse() const { return m_HasReverse; }

        void Clear();
        bool IsEmpty() const { return m_Segments.empty(); }

    private:
        std::string m_OriginalText;
        std::vector<TextSegment> m_Segments;
        bool m_HasAnsi256BG = false;   // Any 40-47/100-107 or 48;5 background used
        bool m_HasTrueColorBG = false; // Any 48;2;r;g;b background used
        bool m_HasReverse = false;     // Any SGR 7 encountered (conservative)

        void ParseAnsiEscapeCodes();
        static ConsoleColor ParseAnsiColorSequence(const char *sequence, size_t length, const ConsoleColor &currentColor,
                                                  bool *out_hasAnsi256Bg = nullptr, bool *out_hasTrueColorBg = nullptr,
                                                  bool *out_hasReverse = nullptr);
        static ImU32 GetRgbColor(int r, int g, int b);
    };

    // Layout and text measurement utilities
    struct Layout {
        // A materialized piece of a TextSegment on a line (may be a sub-slice).
        struct Span {
            const TextSegment *seg = nullptr;
            const char *b = nullptr;
            const char *e = nullptr;
            float width = 0.0f; // precomputed width in pixels
            bool isTab = false;
        };

        struct Line {
            std::vector<Span> spans;
            float width = 0.0f;
        };

        static const char *Utf8Next(const char *s, const char *end);
        static const char *NextGrapheme(const char *s, const char *end);
        static float Measure(ImFont *font, float fontSize, const char *b, const char *e);
        static void BuildLines(const std::vector<TextSegment> &segments, float wrapWidth, int tabColumns, float fontSize, std::vector<Line> &outLines);
    };

    // Color manipulation utilities
    namespace Color {
        ImU32 ApplyDim(ImU32 color);
        ImU32 ApplyAlpha(ImU32 color, float alpha);
    };

    // Text decoration metrics
    namespace Metrics {
        float UnderlineY(float lineTop, float fontSize);
        float StrikeY(float lineTop, float fontSize);
        float Thickness(float fontSize);
    };

    float CalculateHeight(const AnsiString &text, float wrapWidth, float fontSize = 0.0f, int tabColumns = kDefaultTabColumns);
    ImVec2 CalculateSize(const AnsiString &text, float wrapWidth, float fontSize = 0.0f, int tabColumns = kDefaultTabColumns);

    // Global configurable behavior
    void SetSgr21Policy(Sgr21Policy policy);
    Sgr21Policy GetSgr21Policy();

    // Text rendering with style support
    namespace Renderer {
        struct BoldParams {
            int rings = 1;
            bool includeDiagonals = false;
            float baseOffsetPx = 0.35f;
            float alphaScale = 0.30f;
            float alphaDecay = 0.80f;
            float sizeMinPx = 12.0f;
            float sizeMaxPx = 36.0f;
            float offsetScaleMin = 0.6f;
            float offsetScaleMax = 1.0f;
        };

        BoldParams &DefaultBold();
        AnsiPalette &DefaultPalette();

        float ComputeItalicShear(float fontSize);
        float ComputeBoldOffsetScale(float fontSize, const BoldParams &bp);
        void AddTextStyled(ImDrawList *drawList, ImFont *font, float fontSize, const ImVec2 &pos, ImU32 col,
                           const char *begin, const char *end, bool italic, bool fauxBold);
        void AddTextStyledEx(ImDrawList *drawList, ImFont *font, float fontSize, const ImVec2 &pos, ImU32 col,
                             const char *begin, const char *end, bool italic, bool fauxBold, const BoldParams &bp);

        void DrawText(ImDrawList *drawList, const AnsiString &text, const ImVec2 &startPos, float wrapWidth,
                      float alpha = 1.0f, float fontSize = 0.0f, int tabColumns = kDefaultTabColumns, const AnsiPalette *palette = nullptr);
    };
} // namespace AnsiText

#endif // BML_ANSITEXT_H

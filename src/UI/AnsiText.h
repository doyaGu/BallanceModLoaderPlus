#ifndef BML_ANSITEXT_H
#define BML_ANSITEXT_H

#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "imgui.h"

#ifndef IM_COL32_BLACK_TRANS
#define IM_COL32_BLACK_TRANS IM_COL32(0, 0, 0, 0)
#endif

class AnsiPalette;

/**
 * AnsiText - ANSI escape code parsing and rendering module
 * 
 * Provides terminal-like text rendering with ANSI color and formatting support.
 */
namespace AnsiText {
    // Default tab columns (historical terminal default).
    inline constexpr int DefaultTabColumns = 8;

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
        bool doubleUnderline = false;
        bool strikethrough = false;
        bool dim = false;
        bool hidden = false;
        bool reverse = false;

        ConsoleColor() = default;
        explicit ConsoleColor(ImU32 fg) : foreground(fg) {}

        // Returns the final colors after applying reverse video and "hidden".
        ConsoleColor GetRendered() const;

        // Comparisons for segment fusion (ignore RGBA when palette indices are used)
        friend bool operator==(const ConsoleColor &a, const ConsoleColor &b) {
            if (a.fgIsAnsi256 != b.fgIsAnsi256) return false;
            if (a.bgIsAnsi256 != b.bgIsAnsi256) return false;
            if (a.fgIsAnsi256) {
                if (a.fgAnsiIndex != b.fgAnsiIndex) return false;
            } else {
                if (a.foreground != b.foreground) return false;
            }
            if (a.bgIsAnsi256) {
                if (a.bgAnsiIndex != b.bgAnsiIndex) return false;
            } else {
                if (a.background != b.background) return false;
            }
            return a.bold == b.bold && a.dim == b.dim && a.italic == b.italic &&
                   a.underline == b.underline && a.doubleUnderline == b.doubleUnderline &&
                   a.strikethrough == b.strikethrough && a.hidden == b.hidden && a.reverse == b.reverse;
        }

        friend bool operator!=(const ConsoleColor &a, const ConsoleColor &b) { return !(a == b); }
    };

    struct TextSegment {
        const char *begin = nullptr;
        const char *end = nullptr;
        ConsoleColor color;

        TextSegment() = default;
        TextSegment(const char *b, const char *e, ConsoleColor c) : begin(b), end(e), color(c) {}
    };

    class AnsiString {
    public:
        AnsiString() = default;
        explicit AnsiString(const char *text);
        explicit AnsiString(const std::string &text);
        explicit AnsiString(std::string &&text);
        AnsiString(std::string &&text, const ConsoleColor &initialColor);

        AnsiString(const AnsiString &other);
        AnsiString &operator=(const AnsiString &other);
        AnsiString(AnsiString &&other) noexcept;
        AnsiString &operator=(AnsiString &&other) noexcept;

        void SetText(const char *text);
        void SetText(const std::string &text);
        void SetText(std::string &&text);
        void SetText(std::string &&text, const ConsoleColor &initialColor);

        const std::string &GetOriginalText() const { return m_OriginalText; }
        const std::vector<TextSegment> &GetSegments() const { return m_Segments; }
        void Clear();
        bool IsEmpty() const { return m_Segments.empty(); }

    private:
        friend class PreparedText;

        std::string m_OriginalText;
        std::vector<TextSegment> m_Segments;
        bool m_HasAnsi256BG = false;
        bool m_HasTrueColorBG = false;
        bool m_HasReverse = false;
        std::uint64_t m_Revision = 0;

        void ParseAnsiEscapeCodes(const ConsoleColor &initialColor = {});
        void AssignAndParse(std::string &&text, const ConsoleColor &initialColor = {});
        void RebindSegmentsPointers(const char *oldBase, const char *newBase);
        static ConsoleColor ParseAnsiColorSequence(const char *sequence, std::size_t length, const ConsoleColor &currentColor,
                                                  bool *out_hasAnsi256Bg = nullptr, bool *out_hasTrueColorBg = nullptr, bool *out_hasReverse = nullptr);
        static ImU32 GetRgbColor(int r, int g, int b);
    };

    struct TextOptions {
        ImFont *font = nullptr;
        float fontSize = 0.0f;
        float wrapWidth = FLT_MAX;
        float alpha = 1.0f;
        float lineSpacing = -1.0f;
        int tabColumns = DefaultTabColumns;
        const AnsiPalette *palette = nullptr;
    };

    // Reusable font-aware layout for one AnsiString. Preparation owns wrapping,
    // span styling and metrics; drawing may vary only alpha and palette.
    class PreparedText {
    public:
        bool Matches(const AnsiString &text, const TextOptions &options) const;
        bool Prepare(const AnsiString &text, const TextOptions &options);
        void Clear();

        bool IsValid() const { return m_Source != nullptr; }
        ImVec2 GetSize() const { return m_Size; }
        void Draw(ImDrawList *drawList, const ImVec2 &position, float alpha = 1.0f,
                  const AnsiPalette *palette = nullptr) const;

    private:
        struct Span {
            const char *begin = nullptr;
            const char *end = nullptr;
            ConsoleColor color;
            float width = 0.0f;
            bool tab = false;
        };

        struct Line {
            std::vector<Span> spans;
            float width = 0.0f;
            bool hasDecorations = false;
        };

        static const char *NextGrapheme(const char *current, const char *end);
        static float Measure(ImFont *font, float fontSize, const char *begin, const char *end);
        static float MeasureFast(ImFont *font, ImFontBaked *baked, float fontSize, float scale,
                                 const char *begin, const char *end);
        static void FinishLine(Line &line, std::vector<Line> &lines, float &lineWidth);
        static void AppendSpan(Line &line, float &lineWidth, const TextSegment &segment,
                               const char *begin, const char *end, float width, bool tab);
        static void BuildLines(ImFont *font, const std::vector<TextSegment> &segments, float wrapWidth,
                               int tabColumns, float fontSize, std::vector<Line> &lines);
        static void DrawBackgroundRuns(ImDrawList *drawList, const Line &line, float startX,
                                       float lineTop, float lineBottom, float italicShear,
                                       const AnsiPalette *palette, float alpha);

        const AnsiString *m_Source = nullptr;
        std::uint64_t m_SourceRevision = 0;
        ImGuiContext *m_Context = nullptr;
        ImFont *m_Font = nullptr;
        ImFontBaked *m_Baked = nullptr;
        ImGuiID m_BakedId = 0;
        float m_FontSize = 0.0f;
        float m_FontScale = 1.0f;
        float m_FontAscent = 0.0f;
        float m_WrapWidth = 0.0f;
        float m_LineSpacing = 0.0f;
        float m_LineHeight = 0.0f;
        float m_LineStep = 0.0f;
        float m_ItalicShear = 0.0f;
        float m_DecorationThickness = 0.0f;
        float m_UnderlineOffset = 0.0f;
        float m_StrikeOffset = 0.0f;
        int m_TabColumns = 0;
        bool m_UsesAnsiPalette = false;
        bool m_MayHaveBackground = false;
        ImVec2 m_Size;
        std::vector<Line> m_Lines;
    };

    ImVec2 CalcTextSize(const AnsiString &text, const TextOptions &options = {});

    namespace Renderer {
        AnsiPalette &DefaultPalette();
        void DrawText(ImDrawList *drawList, const AnsiString &text, const ImVec2 &startPos, const TextOptions &options = {});
    }
}

#endif // BML_ANSITEXT_H

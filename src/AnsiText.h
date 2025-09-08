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
    // Default tab size in columns
    constexpr int kDefaultTabColumns = 8;

    // ANSI color and formatting state
    struct ConsoleColor {
        ImU32 foreground = IM_COL32_WHITE;
        ImU32 background = IM_COL32_BLACK_TRANS;

        bool bold = false;
        bool italic = false;
        bool underline = false;
        bool strikethrough = false;
        bool dim = false;
        bool hidden = false;
        bool reverse = false;

        // Origin markers for 256-color palette application
        bool fgIsAnsi256 = false;
        bool bgIsAnsi256 = false;
        int fgAnsiIndex = -1;
        int bgAnsiIndex = -1;

        ConsoleColor() = default;
        explicit ConsoleColor(ImU32 fg) : foreground(fg) {}
        ConsoleColor(ImU32 fg, ImU32 bg) : foreground(fg), background(bg) {}

        // Returns the final colors after applying reverse video and "hidden".
        ConsoleColor GetRendered() const;
    };

    // Text segment with associated ANSI formatting
    struct TextSegment {
        std::string text;
        ConsoleColor color;

        TextSegment(std::string t, ConsoleColor c) : text(std::move(t)), color(c) {}
    };

    // Parsed ANSI text ready for rendering
    class AnsiString {
    public:
        AnsiString() = default;
        explicit AnsiString(const char *text);

        void SetText(const char *text);
        const std::string &GetOriginalText() const { return m_OriginalText; }
        const std::vector<TextSegment> &GetSegments() const { return m_Segments; }

        void Clear();
        bool IsEmpty() const { return m_Segments.empty(); }

    private:
        std::string m_OriginalText;
        std::vector<TextSegment> m_Segments;

        void ParseAnsiEscapeCodes();
        static ConsoleColor ParseAnsiColorSequence(const char *sequence, size_t length, const ConsoleColor &currentColor);
        static ImU32 GetRgbColor(int r, int g, int b);
    };

    // Layout and text measurement utilities
    namespace Layout {
        struct Span {
            const TextSegment *seg;
            const char *b;
            const char *e;
            float width;
            bool isTab;
        };

        struct Line {
            std::vector<Span> spans;
            float width = 0.0f;
        };

        const char *Utf8Next(const char *s, const char *end);
        float Measure(ImFont *font, float fontSize, const char *b, const char *e);
        void BuildLines(const std::vector<TextSegment> &segments, float wrapWidth, int tabColumns, float fontSize, std::vector<Line> &outLines);
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

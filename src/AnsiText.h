#ifndef BML_ANSITEXT_H
#define BML_ANSITEXT_H

#include <cfloat>
#include <vector>
#include <string>

#include "imgui.h"

// Fallback for portability across Dear ImGui versions
#ifndef IM_COL32_BLACK_TRANS
#define IM_COL32_BLACK_TRANS IM_COL32(0, 0, 0, 0)
#endif

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

        AnsiString(const AnsiString &other);
        AnsiString &operator=(const AnsiString &other);
        AnsiString(AnsiString &&other) noexcept;
        AnsiString &operator=(AnsiString &&other) noexcept;

        void SetText(const char *text);
        void SetText(const std::string &text);
        void SetText(std::string &&text);

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
        void AssignAndParse(std::string &&text);
        void RebindSegmentsPointers(const char *oldBase, const char *newBase);
        static ConsoleColor ParseAnsiColorSequence(const char *sequence, size_t length, const ConsoleColor &currentColor,
                                                  bool *out_hasAnsi256Bg = nullptr, bool *out_hasTrueColorBg = nullptr, bool *out_hasReverse = nullptr);
        static ImU32 GetRgbColor(int r, int g, int b);
    };

    struct TextOptions {
        ImFont *font = nullptr;
        float fontSize = 0.0f;
        float wrapWidth = FLT_MAX;
        float alpha = 1.0f;
        float lineSpacing = -1.0f;
        int tabColumns = kDefaultTabColumns;
        const AnsiPalette *palette = nullptr;
    };

    ImVec2 CalcTextSize(const AnsiString &text, const TextOptions &options = {});
    float CalcTextHeight(const AnsiString &text, const TextOptions &options = {});
    ImVec2 CalcTextSize(const char *text, const TextOptions &options = {});
    float CalcTextHeight(const char *text, const TextOptions &options = {});
    ImVec2 CalcTextSize(const std::string &text, const TextOptions &options = {});
    float CalcTextHeight(const std::string &text, const TextOptions &options = {});

    void RenderText(ImDrawList *drawList, const AnsiString &text, const ImVec2 &pos, const TextOptions &options = {});
    void RenderText(ImDrawList *drawList, const char *text, const ImVec2 &pos, const TextOptions &options = {});
    void RenderText(ImDrawList *drawList, const std::string &text, const ImVec2 &pos, const TextOptions &options = {});

    void TextAnsi(const AnsiString &text, const TextOptions &options = {});
    void TextUnformatted(const char *text, const TextOptions &options = {});
    void TextUnformatted(const std::string &text, const TextOptions &options = {});
    void TextV(const char *fmt, va_list args, const TextOptions &options = {});
    void Text(const char *fmt, ...);

    // Global configurable behavior
    void SetSgr21Policy(Sgr21Policy policy);
    Sgr21Policy GetSgr21Policy();

    // Optional: pre-resolve ANSI 256-color indices to RGBA at parse-time when using a fixed palette.
    // This avoids per-span palette lookups during rendering. Disabled by default.
    void SetPreResolvePalette(const AnsiPalette *palette);
    const AnsiPalette *GetPreResolvePalette();
    void SetPreResolveEnabled(bool enabled);
    bool GetPreResolveEnabled();

    namespace Layout {
        struct Span {
            const TextSegment *seg = nullptr;
            const char *b = nullptr;
            const char *e = nullptr;
            float width = 0.0f;
            bool isTab = false;
        };

        struct Line {
            std::vector<Span> spans;
            float width = 0.0f;
        };

        const char *Utf8Next(const char *s, const char *end);
        const char *NextGrapheme(const char *s, const char *end);
        float Measure(ImFont *font, float fontSize, const char *b, const char *e);
        void BuildLines(ImFont *font, const std::vector<TextSegment> &segments, float wrapWidth, int tabColumns, float fontSize, std::vector<Line> &outLines);
    }

    namespace Color {
        ImU32 ApplyDim(ImU32 color);
        ImU32 ApplyAlpha(ImU32 color, float alpha);
    }

    namespace Metrics {
        float UnderlineY(float lineTop, float fontSize);
        float StrikeY(float lineTop, float fontSize);
        float Thickness(float fontSize);
    }

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

        void DrawText(ImDrawList *drawList, const AnsiString &text, const ImVec2 &startPos, const TextOptions &options = {});
    }
}

#endif // BML_ANSITEXT_H

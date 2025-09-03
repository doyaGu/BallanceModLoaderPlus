#ifndef BML_MESSAGEBOARD_H
#define BML_MESSAGEBOARD_H

#include <vector>
#include <string>
#include <algorithm>

#include "BML/Bui.h"

// Minimum alpha used when drawing auxiliary strokes (e.g., faux-bold offsets)
static constexpr float kAuxStrokeAlphaScale = 0.5f;

/**
 * MessageBoard - ANSI color-capable scrollable message display system
 *
 * Features:
 * - ANSI color and text formatting support
 * - Automatic message expiration with timers
 * - Scrollable view when command bar is visible
 * - Unicode support
 * - Style system integration for DPI scaling and theme support
 *
 * Two display modes:
 * 1. Normal mode: Messages fade out based on timer, no input allowed
 * 2. Command bar mode: All messages visible, scrolling enabled when content overflows
 *
 * Scrolling controls (when command bar visible AND content overflows):
 * - Mouse wheel: Scroll up/down
 * - Page Up/Down: Large scroll steps
 * - Home/End: Go to top/bottom
 * - Up/Down arrows: Single line steps
 */
class MessageBoard : public Bui::Window {
public:
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

        ConsoleColor() = default;
        explicit ConsoleColor(ImU32 fg) : foreground(fg) {}
        ConsoleColor(ImU32 fg, ImU32 bg) : foreground(fg), background(bg) {}

        // Returns the final colors after applying reverse video and "hidden".
        // Guarantees foreground alpha > 0 when reverse is set by synthesizing a fallback BG.
        ConsoleColor GetRendered() const;
    };

    struct TextSegment {
        std::string text;
        ConsoleColor color;

        TextSegment(std::string t, ConsoleColor c) : text(std::move(t)), color(c) {}
    };

    struct MessageUnit {
        std::string originalText;
        std::vector<TextSegment> segments;
        float timer = 0.0f;
        mutable float cachedHeight = -1.0f;
        mutable float cachedWrapWidth = -1.0f;

        MessageUnit() = default;
        MessageUnit(const char *msg, float timer);

        MessageUnit(MessageUnit &&other) noexcept = default;
        MessageUnit &operator=(MessageUnit &&other) noexcept = default;
        MessageUnit(const MessageUnit &other) = default;
        MessageUnit &operator=(const MessageUnit &other) = default;

        const char *GetMessage() const { return originalText.c_str(); }
        void SetMessage(const char *msg);
        float GetTimer() const { return timer; }
        void SetTimer(float t) { timer = t; }
        float GetTextHeight(float wrapWidth) const;
        void Reset();

    private:
        void ParseAnsiEscapeCodes();

        static ConsoleColor ParseAnsiColorSequence(const std::string &sequence, const ConsoleColor &currentColor);
        static ImU32 GetStandardColor(int colorCode, bool bright = false);
        static ImU32 Get256Color(int colorIndex);
        static ImU32 GetRgbColor(int r, int g, int b);
    };

    explicit MessageBoard(int size = 500);
    ~MessageBoard() override;

    // Message management
    void AddMessage(const char *msg);
    void Printf(const char *format, ...);
    void PrintfColored(ImU32 color, const char *format, ...);
    void ClearMessages();
    void ResizeMessages(int size);

    // Settings
    void SetMaxTimer(float maxTimer) { m_MaxTimer = std::max(100.0f, maxTimer); }
    void SetCommandBarVisible(bool visible);
    void SetAnsiEnabled(bool enabled) { m_AnsiEnabled = enabled; }

    // Modern color tuning
    void SetColorSaturation(float s) { m_ColorSaturation = std::clamp(s, 0.0f, 1.0f); }
    void SetColorValue(float v) { m_ColorValue = std::clamp(v, 0.0f, 1.5f); }

    // Scrolling control (only active when command bar is visible)
    void SetScrollPosition(float scrollY);
    void ScrollToTop();
    void ScrollToBottom();

    // Getters
    float GetMaxTimer() const { return m_MaxTimer; }
    bool IsCommandBarVisible() const { return m_IsCommandBarVisible; }
    bool IsAnsiEnabled() const { return m_AnsiEnabled; }
    float GetScrollY() const { return m_IsCommandBarVisible ? m_ScrollY : 0.0f; }
    float GetMaxScrollY() const { return m_IsCommandBarVisible ? m_MaxScrollY : 0.0f; }
    bool IsScrolledToBottom() const { return m_ScrollToBottom; }
    bool HasScrollableContent() const { return m_IsCommandBarVisible && m_MaxScrollY > 0.0f; }

protected:
    ImGuiWindowFlags GetFlags() override;
    void OnPreBegin() override;
    void OnDraw() override;
    void OnPostEnd() override;

private:
    struct ScrollMetrics {
        float contentHeight;
        float visibleHeight;
        float maxScroll;
        float scrollY;
        float scrollRatio;   // 0..1, position along scroll range
        float visibleRatio;  // 0..1, visibleHeight/contentHeight
    };

    // Visibility and state
    bool ShouldShowMessage(const MessageUnit &msg) const;
    float GetMessageAlpha(const MessageUnit &msg) const;
    int CountVisibleMessages() const;
    bool HasVisibleContent() const;

    // Layout calculation
    float CalculateContentHeight(float wrapWidth) const;
    float CalculateDisplayHeight(float contentHeight) const;

    // Rendering
    void RenderMessages(ImDrawList *drawList, ImVec2 startPos, float wrapWidth);
    void DrawMessageText(ImDrawList *drawList, const MessageUnit &message, const ImVec2 &pos, float wrapWidth, float alpha);
    void DrawScrollIndicators(ImDrawList *drawList, const ImVec2 &contentPos, const ImVec2 &contentSize, float contentHeight, float visibleHeight);

    // Core operations
    void UpdateTimers(float deltaTime);
    void AddMessageInternal(const char *msg);
    void HandleScrolling(float visibleHeight);
    void UpdateScrollBounds(float contentHeight, float windowHeight);

    // Utilities
    ImU32 ToneColor(ImU32 c) const;
    ScrollMetrics GetScrollMetrics(float contentHeight, float visibleHeight) const;
    void SetScrollYClamped(float y);
    void SyncScrollBottomFlag();

    // Message storage
    std::vector<MessageUnit> m_Messages;
    int m_MessageCount = 0;
    int m_DisplayMessageCount = 0;

    // Configuration
    bool m_IsCommandBarVisible = false;
    bool m_AnsiEnabled = true;
    float m_MaxTimer = 6000.0f;

    // Color tuning
    float m_ColorSaturation = 0.65f;
    float m_ColorValue = 0.95f;

    // Scrolling state
    float m_ScrollY = 0.0f;
    float m_MaxScrollY = 0.0f;
    bool m_ScrollToBottom = true;

    // Style-derived layout cache
    float m_PadX = 8.0f;         // Content area horizontal padding
    float m_PadY = 8.0f;         // Content area vertical padding
    float m_MessageGap = 4.0f;   // Spacing between message blocks
    float m_ScrollbarW = 8.0f;   // Scrollbar width
    float m_ScrollbarPad = 2.0f; // Scrollbar edge padding
};

#endif // BML_MESSAGEBOARD_H

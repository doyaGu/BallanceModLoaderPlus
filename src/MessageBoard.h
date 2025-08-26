#ifndef BML_MESSAGEBOARD_H
#define BML_MESSAGEBOARD_H

#include <vector>
#include <string>
#include <algorithm>

#include "BML/Bui.h"

/**
 * MessageBoard - ANSI-capable scrollable message display system
 *
 * Features:
 * - Full ANSI escape sequence support (colors, formatting, cursor control)
 * - Automatic message expiration with timers
 * - Scrollable view when command bar is visible
 * - Unicode support including surrogate pairs
 * - Consistent text measurement and rendering
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
 *
 * The scrollbar only appears when there's more content than fits in the window.
 */
class MessageBoard : public Bui::Window {
public:
    struct ConsoleColor {
        ImU32 foreground = IM_COL32_WHITE;
        ImU32 background = IM_COL32(0, 0, 0, 0);
        bool bold = false;
        bool dim = false;
        bool italic = false;
        bool underline = false;
        bool blink = false;
        bool reverse = false;
        bool strikethrough = false;
        bool hidden = false;

        ConsoleColor() = default;
        explicit ConsoleColor(ImU32 fg) : foreground(fg) {}
        ConsoleColor(ImU32 fg, ImU32 bg) : foreground(fg), background(bg) {}

        // Apply reverse video effect
        ConsoleColor GetRendered() const {
            ConsoleColor result = *this;
            if (reverse) {
                std::swap(result.foreground, result.background);
                // Ensure background has alpha if swapped
                if ((result.background & 0xFF000000) == 0) {
                    result.background |= 0xFF000000;
                }
            }
            return result;
        }
    };

    struct TextSegment {
        std::string text;
        ConsoleColor color;
        int cursorX = -1;
        int cursorY = -1;
        bool isClearScreen = false;
        bool isClearLine = false;

        TextSegment(std::string t, ConsoleColor c) : text(std::move(t)), color(c) {}
        TextSegment(std::string t, ConsoleColor c, int x, int y) : text(std::move(t)), color(c), cursorX(x), cursorY(y) {}

        // Special control segments
        static TextSegment ClearScreen(ConsoleColor c) {
            TextSegment seg("", c);
            seg.isClearScreen = true;
            return seg;
        }

        static TextSegment ClearLine(ConsoleColor c) {
            TextSegment seg("", c);
            seg.isClearLine = true;
            return seg;
        }
    };

    struct MessageUnit {
        std::string originalText;
        std::vector<TextSegment> segments;
        float timer = 0.0f;
        mutable float cachedHeight = -1.0f;
        mutable float cachedWrapWidth = -1.0f;
        bool hasControlSequences = false;

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
        static ConsoleColor ParseAnsiColor(const std::string &sequence, ConsoleColor currentColor);
        static bool ParseCursorMovement(const std::string &sequence, int &deltaX, int &deltaY, bool &absolute);
        static bool ParseScreenControl(const std::string &sequence, bool &clearScreen, bool &clearLine);
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
    void SetCommandBarVisible(bool visible); // When true, shows all messages and enables scrolling when needed
    void SetAnsiEnabled(bool enabled) { m_AnsiEnabled = enabled; }

    // Scrolling control (only active when command bar is visible)
    void SetScrollPosition(float scrollY) {
        if (m_IsCommandBarVisible && m_MaxScrollY > 0.0f) {
            m_ScrollY = std::clamp(scrollY, 0.0f, m_MaxScrollY);
            m_ScrollToBottom = (m_ScrollY >= m_MaxScrollY - 1.0f);
        }
    }
    void ScrollToTop() {
        if (m_IsCommandBarVisible) {
            m_ScrollY = 0.0f;
            m_ScrollToBottom = false;
        }
    }
    void ScrollToBottom() {
        if (m_IsCommandBarVisible) {
            m_ScrollY = m_MaxScrollY;
            m_ScrollToBottom = true;
        }
    }

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
    // Cursor save/restore stack
    struct CursorPosition {
        int x = 0, y = 0;
        CursorPosition() = default;
        CursorPosition(int x, int y) : x(x), y(y) {}
    };

    // Visibility and state helpers
    bool ShouldShowMessage(const MessageUnit &msg) const;
    float GetMessageAlpha(const MessageUnit &msg) const;
    int CountVisibleMessages() const;
    bool HasVisibleContent() const;

    // Layout calculation
    float CalculateContentHeight(float wrapWidth) const;

    // Rendering system
    void RenderMessages(ImDrawList *drawList, ImVec2 startPos, float wrapWidth);
    void DrawMessageText(ImDrawList *drawList, const MessageUnit &message, const ImVec2 &pos, float wrapWidth, float alpha);
    void DrawTextSegment(ImDrawList *drawList, const TextSegment &segment, ImVec2 &currentPos, float wrapWidth, float alpha, const ImVec2 &contentStart, const ImVec2 &contentSize);
    void RenderText(ImDrawList *drawList, const std::string &text, const ImVec2 &pos, ImU32 color, const ConsoleColor &effects) const;
    void DrawScrollIndicators(ImDrawList *drawList, const ImVec2 &contentPos, const ImVec2 &contentSize);
    static void HandleSpecialCharacter(char ch, ImVec2 &currentPos, const ImVec2 &startPos, float wrapWidth);

    // Core operations
    void UpdateTimers(float deltaTime);
    void AddMessageInternal(const char *msg);
    void HandleScrolling(float contentHeight, float windowHeight);
    void UpdateScrollBounds(float contentHeight, float windowHeight);
    float CalculateTotalContentHeight(float wrapWidth) const;

    // Message storage
    std::vector<MessageUnit> m_Messages;
    int m_MessageCount = 0;
    int m_DisplayMessageCount = 0; // Count of messages with active timers

    // Configuration
    bool m_IsCommandBarVisible = false;
    bool m_AnsiEnabled = true;
    float m_MaxTimer = 6000.0f; // 6 seconds default (assuming milliseconds)
    float m_BlinkTime = 0.0f; // Global blink time for consistent blinking

    // Scrolling state
    float m_ScrollY = 0.0f;
    float m_MaxScrollY = 0.0f;
    bool m_ScrollToBottom = true; // Auto-scroll to bottom for new messages
};

#endif // BML_MESSAGEBOARD_H

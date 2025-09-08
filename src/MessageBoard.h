#ifndef BML_MESSAGEBOARD_H
#define BML_MESSAGEBOARD_H

#include <vector>
#include <string>
#include <algorithm>

#include "BML/Bui.h"

#include "AnsiPalette.h"
#include "AnsiText.h"

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
 */
class MessageBoard : public Bui::Window {
public:
    using ConsoleColor = AnsiText::ConsoleColor;
    using TextSegment = AnsiText::TextSegment;

    struct MessageUnit {
        AnsiText::AnsiString ansiText;
        float timer = 0.0f;
        mutable float cachedHeight = -1.0f;
        mutable float cachedWrapWidth = -1.0f;

        MessageUnit() = default;
        MessageUnit(const char *msg, float timer);

        MessageUnit(MessageUnit &&other) noexcept = default;
        MessageUnit &operator=(MessageUnit &&other) noexcept = default;
        MessageUnit(const MessageUnit &other) = default;
        MessageUnit &operator=(const MessageUnit &other) = default;

        const char *GetMessage() const { return ansiText.GetOriginalText().c_str(); }
        void SetMessage(const char *msg);
        float GetTimer() const { return timer; }
        void SetTimer(float t) { timer = t; }
        float GetTextHeight(float wrapWidth, int tabColumns) const;
        const std::vector<TextSegment> &GetSegments() const { return ansiText.GetSegments(); }
        void Reset();
    };

    struct ScrollMetrics {
        float contentHeight;
        float visibleHeight;
        float maxScroll;
        float scrollY;
        float scrollRatio;   // 0..1, position along scroll range
        float visibleRatio;  // 0..1, visibleHeight/contentHeight
    };

    explicit MessageBoard(int size = 500);
    ~MessageBoard() override;

    // Message management
    void AddMessage(const char *msg);
    void Printf(const char *format, ...);
    void PrintfColored(ImU32 color, const char *format, ...);
    void ClearMessages();
    void ResizeMessages(int size);

    // Appearance and layout configuration
    void SetTabColumns(int columns);
    int GetTabColumns() const;

    void SetWindowBackgroundColor(ImVec4 color);
    void SetWindowBackgroundColorU32(ImU32 color);
    bool HasCustomWindowBackground() const { return m_HasCustomWindowBg; }
    ImVec4 GetWindowBackgroundColor() const { return m_WindowBgColor; }
    void ClearWindowBackgroundColor();

    void SetMessageBackgroundColor(ImVec4 color);
    void SetMessageBackgroundColorU32(ImU32 color);
    bool HasCustomMessageBackground() const { return m_HasCustomMessageBg; }
    ImVec4 GetMessageBackgroundColor() const { return m_MessageBgColor; }
    void ClearMessageBackgroundColor();

    // Alpha controls (0..1)
    void SetWindowBackgroundAlpha(float alpha);
    float GetWindowBackgroundAlpha() const { return m_WindowBgAlphaScale; }
    void SetMessageBackgroundAlpha(float alpha);
    float GetMessageBackgroundAlpha() const { return m_MessageBgAlphaScale; }

    // Fade maximum alpha for messages (0..1)
    void SetFadeMaxAlpha(float alpha);
    float GetFadeMaxAlpha() const { return m_FadeMaxAlpha; }

    // Settings
    void SetMaxTimer(float maxTimer) { m_MaxTimer = std::max(100.0f, maxTimer); }
    void SetCommandBarVisible(bool visible);

    // Scrolling control (only active when command bar is visible)
    void SetScrollPosition(float scrollY);
    void ScrollToTop();
    void ScrollToBottom();

    // Scrolling metrics and display helpers
    ScrollMetrics GetScrollMetrics(float contentHeight, float visibleHeight) const;
    std::string FormatScrollPercent(float contentHeight, float visibleHeight) const;

    // Getters
    float GetMaxTimer() const { return m_MaxTimer; }
    bool IsCommandBarVisible() const { return m_IsCommandBarVisible; }
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
    void InvalidateLayoutCache();

    // Utilities
    void SetScrollYClamped(float y);
    void SyncScrollBottomFlag();

    // Message storage
    std::vector<MessageUnit> m_Messages;
    int m_MessageCount = 0;
    int m_DisplayMessageCount = 0;

    // Configuration
    bool m_IsCommandBarVisible = false;
    float m_MaxTimer = 6000.0f;

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
    float m_ScrollEpsilon = 0.5f; // Scrollbar tolerance for bottom checks

    // Configurable behavior
    int m_TabColumns = AnsiText::kDefaultTabColumns; // Tab size in columns
    bool m_HasCustomWindowBg = false;
    bool m_HasCustomMessageBg = false;
    ImVec4 m_WindowBgColor = {};        // If !m_HasCustomWindowBg, use Bui::GetMenuColor()
    ImVec4 m_MessageBgColor = {};       // If !m_HasCustomMessageBg, use Bui::GetMenuColor()
    float m_WindowBgAlphaScale = 1.0f;
    float m_MessageBgAlphaScale = 1.0f;
    float m_FadeMaxAlpha = 1.0f;
};

#endif // BML_MESSAGEBOARD_H

#ifndef BML_MESSAGEBOARD_H
#define BML_MESSAGEBOARD_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "BML/Bui.h"

#include "Console/ConsoleLayout.h"
#include "UI/AnsiPalette.h"
#include "UI/AnsiText.h"

/**
 * MessageBoard - ANSI color-capable scrollable message display system
 *
 * Features:
 * - ANSI color and text formatting support
 * - Automatic message expiration with timers
 * - Scrollable view when command bar is visible
 * - Read-only mouse selection and clipboard copy in scrollback
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

    struct DisplayPolicy {
        // Timed messages shown while the command bar is closed.
        bool notifications = true;
        // Stored messages shown while the command bar is open.
        bool scrollback = true;

        bool operator==(const DisplayPolicy &) const = default;
    };

    struct MessageUnit {
        AnsiText::AnsiString ansiText;
        std::uint64_t sequence = 0;
        float timer = 0.0f;

        MessageUnit() = default;
        MessageUnit(const char *msg, float timer);
        MessageUnit(std::string msg, float timer, ImU32 color);

        MessageUnit(MessageUnit &&other) noexcept = default;
        MessageUnit &operator=(MessageUnit &&other) noexcept = default;
        MessageUnit(const MessageUnit &other) = default;
        MessageUnit &operator=(const MessageUnit &other) = default;

        const char *GetMessage() const { return ansiText.GetOriginalText().c_str(); }
        void SetMessage(const char *msg);
        float GetTimer() const { return timer; }
        void SetTimer(float t) { timer = t; }
        void Reset();
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
    void SetLineSpacing(float spacing);
    float GetLineSpacing() const;

    void SetMessageBackgroundColor(ImVec4 color);
    void SetMessageBackgroundColorU32(ImU32 color);
    bool HasCustomMessageBackground() const { return m_HasCustomMessageBg; }
    ImVec4 GetMessageBackgroundColor() const { return m_MessageBgColor; }
    void ClearMessageBackgroundColor();

    // Alpha controls (0..1)
    void SetMessageBackgroundAlpha(float alpha);
    float GetMessageBackgroundAlpha() const { return m_MessageBgAlphaScale; }

    // Fade maximum alpha for messages (0..1)
    void SetFadeMaxAlpha(float alpha);
    float GetFadeMaxAlpha() const { return m_FadeMaxAlpha; }

    // Settings
    void SetMaxTimer(float maxTimer) { m_MaxTimer = std::max(100.0f, maxTimer); }
    void SetDisplayPolicy(DisplayPolicy policy);
    const DisplayPolicy &GetDisplayPolicy() const { return m_DisplayPolicy; }
    void SetCommandBarVisible(bool visible);
    void AdvanceNotificationTimers();
    void SetFrameLayout(const ConsoleLayout::Stack &layout) { m_ConsoleLayout = layout; }

    // Scrolling control (only active when command bar is visible)
    void SetScrollPosition(float scrollY);
    void ScrollToTop();
    void ScrollToBottom();

    // Getters
    float GetMaxTimer() const { return m_MaxTimer; }
    bool IsCommandBarVisible() const { return m_IsCommandBarVisible; }
    float GetScrollY() const { return m_IsCommandBarVisible ? m_Scroll.position : 0.0f; }
    float GetMaxScrollY() const { return m_IsCommandBarVisible ? m_Scroll.maximum : 0.0f; }
    bool IsScrolledToBottom() const { return m_Scroll.followEnd; }
    bool HasScrollableContent() const { return m_IsCommandBarVisible && m_Scroll.HasOverflow(); }
    bool IsMouseInteractionActive() const { return m_MouseInteractionActive; }

protected:
    ImGuiWindowFlags GetFlags() override;
    void OnPreBegin() override;
    void OnDraw() override;
    void OnPostEnd() override;

private:
    struct ScrollMetrics {
        float contentHeight = 0.0f;
        float visibleHeight = 0.0f;
        float maxScroll = 0.0f;
        float scrollY = 0.0f;
        float scrollRatio = 0.0f;
        float visibleRatio = 1.0f;
    };

    struct ScrollState {
        float position = 0.0f;
        float maximum = 0.0f;
        float selectionRemainder = 0.0f;
        bool followEnd = true;

        bool HasOverflow() const { return maximum > 0.0f; }
        void Reset();
        void UpdateBounds(float contentHeight, float visibleHeight);
        void SetPosition(float value);
        void ScrollBy(float delta);
        void ScrollToStart();
        void ScrollToEnd();
        void StopSelectionScroll();
        void AutoScrollSelection(float mouseY, float regionTop, float regionBottom,
                                 float lineHeight, float deltaTime);
        ScrollMetrics GetMetrics(float contentHeight, float visibleHeight) const;
    };

    struct MessageRow {
        int messageIndex;
        std::uint64_t messageSequence;
        float top;
        float height;
        AnsiText::PreparedText textLayout;
    };

    struct MessageRows {
        std::uint64_t revision = ~std::uint64_t{0};
        bool commandBarVisible = false;
        ImGuiContext *context = nullptr;
        ImFont *font = nullptr;
        ImGuiID bakedId = 0;
        float fontSize = 0.0f;
        float wrapWidth = 0.0f;
        float messageGap = 0.0f;
        float contentHeight = 0.0f;
        int tabColumns = 0;
        std::vector<MessageRow> rows;

        bool Matches(std::uint64_t currentRevision, bool currentCommandBarVisible,
                     ImGuiContext *currentContext, ImFont *currentFont,
                     ImGuiID currentBakedId, float currentFontSize, float currentWrapWidth,
                     float currentMessageGap, int currentTabColumns) const;
        void Invalidate();
    };

    struct ScrollLabel {
        ImGuiContext *context = nullptr;
        ImGuiID bakedId = 0;
        float fontSize = 0.0f;
        int percent = -1;
        std::array<char, 8> text{};
        ImVec2 size;
    };

    struct FrameLayout {
        float padX;
        float padY;
        float messageGap;
        float scrollbarWidth;
        float scrollbarPadding;
        ImGuiContext *context;
        ImFont *font;
        ImGuiID bakedId;
        float fontSize;
        float wrapWidth;
        float contentHeight;
        float availableContentHeight;
        float scrollbarReserve;
        bool needsScrollbar;
        const MessageRows *messageRows;
    };

    struct SelectionPoint {
        std::uint64_t messageSequence = 0;
        std::size_t offset = 0;

        bool operator==(const SelectionPoint &) const = default;
    };

    struct TextRange {
        std::size_t begin = 0;
        std::size_t end = 0;
    };

    // Visibility and state
    bool ShouldShowMessage(const MessageUnit &msg) const;
    float GetMessageAlpha(const MessageUnit &msg, float maximumAlpha) const;
    bool HasVisibleContent() const;

    // Layout calculation
    FrameLayout CaptureFrameLayout() const;
    const MessageRows &PrepareMessageRows(float wrapWidth, const FrameLayout &layout);
    static float CalculateDisplayHeight(float contentHeight, const FrameLayout &layout);

    // Rendering
    void RenderMessages(ImDrawList *drawList, ImVec2 startPos, float wrapWidth, const FrameLayout &layout);
    static void DrawMessageText(ImDrawList *drawList, const AnsiText::PreparedText &textLayout,
                                const ImVec2 &position, float alpha);
    void HandleTextSelection(const ImVec2 &regionMin, const ImVec2 &regionMax,
                             const MessageRows &rows, float lineHeight);
    void DrawTextSelection(ImDrawList *drawList, const MessageRow &row,
                           const ImVec2 &position, const SelectionPoint &begin,
                           const SelectionPoint &end) const;
    SelectionPoint HitTestText(const ImVec2 &position, const ImVec2 &textPosition,
                               const MessageRows &rows) const;
    std::string GetSelectedText(const MessageRows &rows, const SelectionPoint &begin,
                                const SelectionPoint &end) const;
    std::optional<TextRange> ProjectSelection(const MessageRow &row,
                                              const SelectionPoint &begin,
                                              const SelectionPoint &end) const;
    static bool SelectionPointBefore(const SelectionPoint &left,
                                     const SelectionPoint &right);
    void GetSelectionRange(SelectionPoint &begin, SelectionPoint &end) const;
    bool HasTextSelection() const;
    void ClearTextSelection();
    void DrawScrollIndicators(ImDrawList *drawList, const ImVec2 &contentPos, const ImVec2 &contentSize, float contentHeight, float visibleHeight, const FrameLayout &layout);

    // Core operations
    void UpdateTimers(float deltaTime);
    void AddMessageInternal(const char *msg);
    void AddMessageInternal(MessageUnit message);
    void HandleMouseWheel(const FrameLayout &layout);
    void InvalidateLayoutCache();
    void InvalidateMessageRows();
    MessageUnit &MessageAt(int logicalIndex);
    const MessageUnit &MessageAt(int logicalIndex) const;

    // Message storage
    std::vector<MessageUnit> m_Messages;
    std::array<MessageRows, 2> m_MessageRowLayouts;
    std::size_t m_NextMessageRowLayout = 0;
    std::uint64_t m_MessageRevision = 0;
    std::uint64_t m_NextMessageSequence = 1;
    int m_MessageCount = 0;
    int m_MessageHead = 0;
    int m_DisplayMessageCount = 0;

    // Configuration
    bool m_IsCommandBarVisible = false;
    float m_MaxTimer = 6000.0f;
    DisplayPolicy m_DisplayPolicy;

    // Scrolling state
    ScrollState m_Scroll;
    ScrollLabel m_ScrollLabel;
    SelectionPoint m_SelectionAnchor;
    SelectionPoint m_SelectionCaret;
    bool m_SelectionFocused = false;
    bool m_MouseInteractionActive = false;

    // Style-derived values captured before this window overrides the ImGui style.
    std::optional<FrameLayout> m_FrameLayout;
    ConsoleLayout::Stack m_ConsoleLayout;
    bool m_LineSpacingOverride = false;
    float m_CustomLineSpacing = 0.0f;
    // Configurable behavior
    int m_TabColumns = AnsiText::DefaultTabColumns; // Tab size in columns
    bool m_HasCustomMessageBg = false;
    ImVec4 m_MessageBgColor = {};       // If !m_HasCustomMessageBg, use the Console background.
    float m_MessageBgAlphaScale = 1.0f;
    float m_FadeMaxAlpha = 1.0f;
};

#endif // BML_MESSAGEBOARD_H

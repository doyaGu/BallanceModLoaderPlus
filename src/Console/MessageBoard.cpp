#include "Console/MessageBoard.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

#include "imgui_internal.h"

#include "Loader/ModContext.h"
#include "StringUtils.h"

static constexpr float MaximumDisplayHeightRatio = 0.8f;
static constexpr ImVec4 DefaultBackgroundColor = {0.0f, 0.0f, 0.0f, 155.0f / 255.0f};

// =============================================================================
// MessageUnit Implementation
// =============================================================================

MessageBoard::MessageUnit::MessageUnit(const char *msg, float timer) : timer(timer) {
    if (msg) {
        ansiText.SetText(msg);
    }
}

MessageBoard::MessageUnit::MessageUnit(std::string msg, float timer, ImU32 color)
    : ansiText(std::move(msg), ConsoleColor(color)), timer(timer) {}

void MessageBoard::MessageUnit::SetMessage(const char *msg) {
    if (!msg) return;

    ansiText.SetText(msg);
}

void MessageBoard::MessageUnit::Reset() {
    ansiText.Clear();
    timer = 0.0f;
}

// =============================================================================
// MessageBoard Implementation
// =============================================================================

MessageBoard::MessageBoard(int size) : Bui::Window("MessageBoard") {
    if (size < 1) size = 500;
    m_Messages.resize(size);
    Hide();
}

MessageBoard::~MessageBoard() = default;

// =============================================================================
// Configuration and State Management
// =============================================================================

ImGuiWindowFlags MessageBoard::GetFlags() {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoScrollWithMouse;

    if (!m_IsCommandBarVisible || !HasVisibleContent()) {
        flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav;
    }

    return flags;
}

void MessageBoard::SetCommandBarVisible(bool visible) {
    if (m_IsCommandBarVisible != visible) {
        m_IsCommandBarVisible = visible;

        if (visible) {
            m_ScrollToBottom = true;
            if (m_MessageCount > 0)
                Show();
            else
                Hide();
        } else {
            // Reset scroll state when hiding command bar
            m_ScrollY = 0.0f;
            m_MaxScrollY = 0.0f;
            m_ScrollToBottom = true;
            if (m_DisplayMessageCount == 0)
                Hide();
        }
    }
}

void MessageBoard::SetScrollPosition(float scrollY) {
    if (m_IsCommandBarVisible && m_MaxScrollY > 0.0f) {
        SetScrollYClamped(scrollY);
    }
}

void MessageBoard::ScrollToTop() {
    if (m_IsCommandBarVisible) {
        SetScrollYClamped(0.0f);
    }
}

void MessageBoard::ScrollToBottom() {
    if (m_IsCommandBarVisible) {
        SetScrollYClamped(m_MaxScrollY);
    }
}

// =============================================================================
// Height Calculation System
// =============================================================================

bool MessageBoard::ShouldShowMessage(const MessageUnit &msg) const {
    return m_IsCommandBarVisible || msg.GetTimer() > 0;
}

float MessageBoard::GetMessageAlpha(const MessageUnit &msg, float maximumAlpha) const {
    if (m_IsCommandBarVisible) {
        return maximumAlpha;
    }

    if (msg.GetTimer() <= 0) {
        return 0.0f;
    }

    const float maxAlpha255 = maximumAlpha * 255.0f;
    return std::min(maxAlpha255, msg.GetTimer() / 20.0f) / 255.0f;
}

bool MessageBoard::HasVisibleContent() const {
    if (m_IsCommandBarVisible) {
        return m_MessageCount > 0;
    }
    return m_DisplayMessageCount > 0;
}

MessageBoard::FrameLayout MessageBoard::CaptureFrameLayout() const {
    const ImGuiStyle &style = ImGui::GetStyle();
    ImFont *font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize();
    ImFontBaked *baked = font ? ImGui::GetFontBaked() : nullptr;
    FrameLayout layout{};
    layout.padX = std::max(0.0f, style.WindowPadding.x);
    layout.padY = std::max(0.0f, style.WindowPadding.y);
    layout.messageGap = m_LineSpacingOverride ? m_CustomLineSpacing : std::max(0.0f, style.ItemSpacing.y);
    layout.scrollbarWidth = std::max(0.0f, style.ScrollbarSize);
    layout.scrollbarPadding = std::max(0.0f, style.ItemInnerSpacing.x * 0.5f);
    layout.context = ImGui::GetCurrentContext();
    layout.font = font;
    layout.bakedId = baked ? baked->BakedId : 0;
    layout.fontSize = fontSize;
    return layout;
}

bool MessageBoard::MessageRows::Matches(std::uint64_t currentRevision, bool currentCommandBarVisible,
                                        ImGuiContext *currentContext, ImFont *currentFont,
                                        ImGuiID currentBakedId, float currentFontSize,
                                        float currentWrapWidth, float currentMessageGap, int currentTabColumns) const {
    return revision == currentRevision && commandBarVisible == currentCommandBarVisible && context == currentContext &&
           font == currentFont &&
           bakedId == currentBakedId && fontSize == currentFontSize &&
           std::abs(wrapWidth - currentWrapWidth) < 0.5f &&
           std::abs(messageGap - currentMessageGap) < 0.5f && tabColumns == currentTabColumns;
}

void MessageBoard::MessageRows::Invalidate() {
    revision = ~std::uint64_t{0};
    context = nullptr;
    rows.clear();
}

const MessageBoard::MessageRows &MessageBoard::PrepareMessageRows(float wrapWidth, const FrameLayout &layout) {
    for (const MessageRows &cached : m_MessageRowLayouts) {
        if (cached.Matches(m_MessageRevision, m_IsCommandBarVisible, layout.context, layout.font, layout.bakedId,
                           layout.fontSize, wrapWidth, layout.messageGap, m_TabColumns)) {
            return cached;
        }
    }

    MessageRows *prepared = nullptr;
    for (MessageRows &candidate : m_MessageRowLayouts) {
        if (candidate.revision == ~std::uint64_t{0}) {
            prepared = &candidate;
            break;
        }
    }
    if (!prepared) {
        prepared = &m_MessageRowLayouts[m_NextMessageRowLayout];
        m_NextMessageRowLayout = (m_NextMessageRowLayout + 1) % m_MessageRowLayouts.size();
    }

    prepared->Invalidate();
    prepared->revision = m_MessageRevision;
    prepared->commandBarVisible = m_IsCommandBarVisible;
    prepared->context = layout.context;
    prepared->font = layout.font;
    prepared->bakedId = layout.bakedId;
    prepared->fontSize = layout.fontSize;
    prepared->wrapWidth = wrapWidth;
    prepared->messageGap = layout.messageGap;
    prepared->tabColumns = m_TabColumns;
    prepared->rows.reserve(m_MessageCount);
    AnsiText::TextOptions textOptions;
    textOptions.font = layout.font;
    textOptions.fontSize = layout.fontSize;
    textOptions.wrapWidth = wrapWidth;
    textOptions.lineSpacing = layout.messageGap;
    textOptions.tabColumns = m_TabColumns;
    float contentHeight = 0.0f;
    for (int i = m_MessageCount - 1; i >= 0; --i) {
        const MessageUnit &msg = MessageAt(i);
        if (!ShouldShowMessage(msg))
            continue;

        prepared->rows.emplace_back();
        MessageRow &row = prepared->rows.back();
        row.messageIndex = i;
        row.top = contentHeight;
        row.textLayout.Prepare(msg.ansiText, textOptions);
        row.height = row.textLayout.GetSize().y;
        contentHeight += row.height + layout.messageGap;
    }

    if (!prepared->rows.empty())
        contentHeight -= layout.messageGap;
    prepared->contentHeight = contentHeight;
    return *prepared;
}

float MessageBoard::CalculateDisplayHeight(float contentHeight, const FrameLayout &layout) {
    // Add padding to content height to get total display height needed
    return contentHeight + layout.padY * 2.0f;
}

// =============================================================================
// Window Rendering
// =============================================================================

void MessageBoard::OnPreBegin() {
    m_FrameLayout = CaptureFrameLayout();
    FrameLayout &layout = *m_FrameLayout;

    // Push style overrides
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const ConsoleLayout::Stack &stack = m_ConsoleLayout;
    const float windowWidth = stack.commandBar.width;
    const float baseWrapWidth = std::max(1.0f, windowWidth - layout.padX * 2.0f);
    const float bottomAnchor = stack.messageBottom;
    const float availableHeight = std::max(0.0f, bottomAnchor - viewport->WorkPos.y);
    const float maxDisplayHeight = std::min(viewport->WorkSize.y * MaximumDisplayHeightRatio, availableHeight);

    layout.wrapWidth = baseWrapWidth;
    layout.messageRows = &PrepareMessageRows(layout.wrapWidth, layout);
    layout.contentHeight = layout.messageRows->contentHeight;
    float windowHeight = std::min(CalculateDisplayHeight(layout.contentHeight, layout), maxDisplayHeight);
    layout.availableContentHeight = std::max(0.0f, windowHeight - layout.padY * 2.0f);
    layout.scrollbarReserve = layout.scrollbarWidth + layout.scrollbarPadding * 2.0f;
    layout.needsScrollbar = m_IsCommandBarVisible && layout.contentHeight > layout.availableContentHeight;
    if (layout.needsScrollbar) {
        layout.wrapWidth = std::max(1.0f, baseWrapWidth - layout.scrollbarReserve);
        layout.messageRows = &PrepareMessageRows(layout.wrapWidth, layout);
        layout.contentHeight = layout.messageRows->contentHeight;
        windowHeight = std::min(CalculateDisplayHeight(layout.contentHeight, layout), maxDisplayHeight);
        layout.availableContentHeight = std::max(0.0f, windowHeight - layout.padY * 2.0f);
    }

    const float posY = std::max(viewport->WorkPos.y, bottomAnchor - windowHeight);
    const float posX = stack.commandBar.x;

    ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
}

void MessageBoard::OnDraw() {
    if (!HasVisibleContent() || !m_FrameLayout) {
        return;
    }
    const FrameLayout &layout = *m_FrameLayout;

    // Keep messages on top in normal mode; when command bar is visible,
    // allow the command bar to overlay the message board if they overlap.
    if (!m_IsCommandBarVisible)
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    const ImVec2 contentPos = ImGui::GetCursorScreenPos();
    const ImVec2 contentSize = ImGui::GetContentRegionAvail();

    // Handle scrolling when command bar is visible
    if (m_IsCommandBarVisible) {
        if (layout.needsScrollbar) {
            UpdateScrollBounds(layout.contentHeight, layout.availableContentHeight);
            HandleScrolling(layout.availableContentHeight, layout);
        } else {
            m_ScrollY = 0.0f;
            m_MaxScrollY = 0.0f;
            m_ScrollToBottom = true;
        }
    }

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    const ImVec2 contentStart(contentPos.x + layout.padX, contentPos.y + layout.padY);

    const ImVec2 startPos(contentStart.x, contentStart.y - m_ScrollY);
    // Set up clipping for content area
    const ImVec2 clipMin(contentPos.x + layout.padX, contentPos.y + layout.padY);
    const ImVec2 clipMax(
        contentPos.x + contentSize.x - layout.padX - (layout.needsScrollbar ? layout.scrollbarReserve : 0.0f),
        contentPos.y + contentSize.y - layout.padY
    );
    drawList->PushClipRect(clipMin, clipMax, true);

    RenderMessages(drawList, startPos, layout.wrapWidth, layout);

    drawList->PopClipRect();
    if (layout.needsScrollbar && m_MaxScrollY > 0.0f) {
        DrawScrollIndicators(drawList, contentPos, contentSize, layout.contentHeight,
                             layout.availableContentHeight, layout);
    }
}

void MessageBoard::RenderMessages(ImDrawList *drawList, ImVec2 startPos, float wrapWidth, const FrameLayout &layout) {
    const ImVec4 backgroundBase = m_HasCustomMessageBg ? m_MessageBgColor : DefaultBackgroundColor;
    const float backgroundAlpha = std::clamp(m_MessageBgAlphaScale, 0.0f, 1.0f);
    const float maximumAlpha = std::clamp(m_FadeMaxAlpha, 0.0f, 1.0f);
    const ImU32 backgroundRgb = ImGui::ColorConvertFloat4ToU32(
        ImVec4(backgroundBase.x, backgroundBase.y, backgroundBase.z, 1.0f)) & ~IM_COL32_A_MASK;
    if (!layout.messageRows || layout.messageRows->rows.empty())
        return;

    // Register the logical content extent once. Visibility is already determined by
    // the variable-height rows below, so an ImGuiListClipper would only repeat it.
    ImGui::SetCursorScreenPos(startPos);
    ImGui::Dummy(ImVec2(wrapWidth, layout.contentHeight));

    const float clipMinY = drawList->GetClipRectMin().y;
    const float clipMaxY = drawList->GetClipRectMax().y;
    const float clipMinRelative = clipMinY - startPos.y;
    std::size_t firstVisible = 0;
    std::size_t searchEnd = layout.messageRows->rows.size();
    while (firstVisible < searchEnd) {
        const std::size_t middle = firstVisible + (searchEnd - firstVisible) / 2;
        const MessageRow &row = layout.messageRows->rows[middle];
        if (row.top + row.height < clipMinRelative)
            firstVisible = middle + 1;
        else
            searchEnd = middle;
    }

    for (std::size_t index = firstVisible; index < layout.messageRows->rows.size(); ++index) {
        const MessageRow &row = layout.messageRows->rows[index];
        const float rowTop = startPos.y + row.top;
        if (rowTop > clipMaxY)
            break;

        const MessageUnit &msg = MessageAt(row.messageIndex);
        const ImVec2 pos(startPos.x, rowTop);

#ifdef IMGUI_ENABLE_TEST_ENGINE
        // Messages are custom draw-list text rather than ImGui widgets. Give
        // each visible production message a non-interactive test landmark so
        // acceptance can prove command output reached the board.
        const char *label = msg.GetMessage();
        if (label && *label) {
            ImGuiWindow *window = ImGui::GetCurrentWindow();
            const ImGuiID id = window->GetID(label);
            const ImRect bounds(pos, ImVec2(pos.x + wrapWidth,
                                            pos.y + row.height));
            if (ImGui::ItemAdd(bounds, id)) {
                ImGuiContext &g = *GImGui;
                IMGUI_TEST_ENGINE_ITEM_INFO(
                    id, label, g.LastItemData.StatusFlags);
            }
        }
#endif

        const float alpha = GetMessageAlpha(msg, maximumAlpha);
        if (alpha > 0.0f) {
            const float finalAlpha = std::clamp(backgroundBase.w * backgroundAlpha * alpha, 0.0f, 1.0f);
            if (finalAlpha > 0.0f) {
                const ImU32 background = backgroundRgb |
                    (static_cast<ImU32>(std::round(finalAlpha * 255.0f)) << IM_COL32_A_SHIFT);
                // Each background owns its complete row, including the inter-row gap,
                // so adjacent message bands remain continuous when text wraps.
                drawList->AddRectFilled(
                    ImVec2(pos.x - layout.padX, pos.y),
                    ImVec2(pos.x + wrapWidth + layout.padX, pos.y + row.height + layout.messageGap),
                    background);
            }

            DrawMessageText(drawList, row.textLayout, pos, alpha);
        }
    }
}

void MessageBoard::DrawMessageText(ImDrawList *drawList, const AnsiText::PreparedText &textLayout,
                                   const ImVec2 &position, float alpha) {
    textLayout.Draw(drawList, position, alpha);
}

// =============================================================================
// Scrolling System
// =============================================================================

void MessageBoard::HandleScrolling(float visibleHeight, const FrameLayout &layout) {
    if (!m_IsCommandBarVisible || m_MaxScrollY <= 0.0f) return;

    const ImGuiIO &io = ImGui::GetIO();

    // Mouse wheel scrolling
    if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f) {
        const float fontPixelSize = ImGui::GetFontSize();
        const float scrollSpeed = (fontPixelSize + layout.messageGap) * 3.0f;
        SetScrollYClamped(m_ScrollY - io.MouseWheel * scrollSpeed);
    }

    // Final clamp and bottom sync (in case no inputs were pressed this frame but bounds changed)
    SetScrollYClamped(m_ScrollY);
}

void MessageBoard::UpdateScrollBounds(float contentHeight, float availableHeight) {
    if (contentHeight > availableHeight) {
        m_MaxScrollY = contentHeight - availableHeight;

        if (m_ScrollToBottom) {
            m_ScrollY = m_MaxScrollY;
        }

        // Ensure scroll and bottom flag are consistent with new bounds
        SetScrollYClamped(m_ScrollY);
    } else {
        m_MaxScrollY = 0.0f;
        m_ScrollY = 0.0f;
        m_ScrollToBottom = true;
    }
}

void MessageBoard::InvalidateLayoutCache() {
    InvalidateMessageRows();
}

void MessageBoard::InvalidateMessageRows() {
    ++m_MessageRevision;
    for (MessageRows &layout : m_MessageRowLayouts)
        layout.Invalidate();
    m_NextMessageRowLayout = 0;
}

void MessageBoard::DrawScrollIndicators(ImDrawList *drawList, const ImVec2 &contentPos, const ImVec2 &contentSize, float contentHeight, float visibleHeight, const FrameLayout &layout) {
    if (m_MaxScrollY <= 0.0f) return;

    // Scrollbar background
    const ImVec2 scrollbarStart = ImVec2(
        contentPos.x + contentSize.x - layout.scrollbarWidth - layout.scrollbarPadding,
        contentPos.y + layout.padY + layout.scrollbarPadding
    );
    const ImVec2 scrollbarEnd = ImVec2(
        contentPos.x + contentSize.x - layout.scrollbarPadding,
        contentPos.y + contentSize.y - layout.padY - layout.scrollbarPadding
    );

    drawList->AddRectFilled(scrollbarStart, scrollbarEnd, IM_COL32(60, 60, 60, 100));

    // Scrollbar handle
    const float scrollbarHeight = scrollbarEnd.y - scrollbarStart.y;
    const ScrollMetrics m = GetScrollMetrics(contentHeight, visibleHeight);
    const float handleHeight = std::max(ImGui::GetStyle().GrabMinSize, scrollbarHeight * m.visibleRatio);
    const float handlePos = (m.maxScroll > 0.0f ? (m.scrollY / m.maxScroll) : 0.0f) * (scrollbarHeight - handleHeight);

    const ImVec2 handleStart = ImVec2(scrollbarStart.x + 1.0f, scrollbarStart.y + handlePos);
    const ImVec2 handleEnd = ImVec2(scrollbarEnd.x - 1.0f, handleStart.y + handleHeight);

    drawList->AddRectFilled(handleStart, handleEnd, IM_COL32(150, 150, 150, 200));

    // Scroll position indicator
    if (m_ScrollY > 0.0f || !m_ScrollToBottom) {
        const int percent = static_cast<int>(std::round(m.scrollRatio * 100.0f));
        if (m_ScrollLabel.context != layout.context || m_ScrollLabel.bakedId != layout.bakedId ||
            m_ScrollLabel.fontSize != layout.fontSize || m_ScrollLabel.percent != percent) {
            m_ScrollLabel.context = layout.context;
            m_ScrollLabel.bakedId = layout.bakedId;
            m_ScrollLabel.fontSize = layout.fontSize;
            m_ScrollLabel.percent = percent;
            std::snprintf(m_ScrollLabel.text.data(), m_ScrollLabel.text.size(), "%d%%", percent);
            m_ScrollLabel.size = ImGui::CalcTextSize(m_ScrollLabel.text.data());
        }
        const ImVec2 textPos = ImVec2(
            contentPos.x + contentSize.x - m_ScrollLabel.size.x - layout.scrollbarWidth - layout.scrollbarPadding - layout.padX,
            contentPos.y + layout.padY * 0.5f
        );

        // Text background
        drawList->AddRectFilled(
            ImVec2(textPos.x - layout.padX * 0.25f, textPos.y - layout.padY * 0.25f),
            ImVec2(textPos.x + m_ScrollLabel.size.x + layout.padX * 0.25f,
                   textPos.y + m_ScrollLabel.size.y + layout.padY * 0.25f),
            IM_COL32(0, 0, 0, 150)
        );

        // Text
        drawList->AddText(textPos, IM_COL32(255, 255, 255, 200), m_ScrollLabel.text.data());
    }
}

MessageBoard::ScrollMetrics MessageBoard::GetScrollMetrics(float contentHeight, float visibleHeight) const {
    ScrollMetrics m{};
    m.contentHeight = std::max(0.0f, contentHeight);
    m.visibleHeight = std::clamp(visibleHeight, 1.0f, std::max(1.0f, m.contentHeight));
    m.maxScroll = std::max(0.0f, m.contentHeight - m.visibleHeight);
    m.scrollY = std::clamp(m_ScrollY, 0.0f, m.maxScroll);
    m.scrollRatio = (m.maxScroll > 0.0f) ? (m.scrollY / m.maxScroll) : 0.0f;
    m.visibleRatio = (m.contentHeight > 0.0f) ? (m.visibleHeight / m.contentHeight) : 1.0f;
    return m;
}

void MessageBoard::SetScrollYClamped(float y) {
    m_ScrollY = std::clamp(y, 0.0f, m_MaxScrollY);
    m_ScrollToBottom = (m_ScrollY >= m_MaxScrollY - m_ScrollEpsilon);
}

// =============================================================================
// Message Management
// =============================================================================

MessageBoard::MessageUnit &MessageBoard::MessageAt(int logicalIndex) {
    const int capacity = static_cast<int>(m_Messages.size());
    return m_Messages[(m_MessageHead + logicalIndex) % capacity];
}

const MessageBoard::MessageUnit &MessageBoard::MessageAt(int logicalIndex) const {
    const int capacity = static_cast<int>(m_Messages.size());
    return m_Messages[(m_MessageHead + logicalIndex) % capacity];
}

void MessageBoard::UpdateTimers(float deltaTime) {
    bool visibilityChanged = false;
    for (int i = 0; i < m_MessageCount; i++) {
        MessageUnit &message = MessageAt(i);
        if (message.timer > 0.0f) {
            message.timer -= deltaTime;
            if (message.timer <= 0.0f) {
                message.timer = 0.0f;
                --m_DisplayMessageCount;
                visibilityChanged = true;
            }
        }
    }
    if (visibilityChanged)
        InvalidateMessageRows();
}

void MessageBoard::AddMessageInternal(const char *msg) {
    if (!msg) return;

    if (msg[0] == '\0')
        msg = "\n"; // treat empty messages as newlines

    AddMessageInternal(MessageUnit(msg, m_MaxTimer));
}

void MessageBoard::AddMessageInternal(MessageUnit message) {
    const int capacity = static_cast<int>(m_Messages.size());

    // Update display count
    if (m_MessageCount == capacity && MessageAt(m_MessageCount - 1).GetTimer() > 0) {
        --m_DisplayMessageCount;
    }

    if (m_MessageCount > 0)
        m_MessageHead = (m_MessageHead + capacity - 1) % capacity;

    // Add new message
    m_Messages[m_MessageHead] = std::move(message);

    if (m_MessageCount < capacity) {
        ++m_MessageCount;
    }
    ++m_DisplayMessageCount;

    // Auto-scroll to bottom for new messages
    if (m_IsCommandBarVisible && (m_ScrollToBottom || m_MaxScrollY <= 0.0f)) {
        m_ScrollToBottom = true;
    }
    Show();
    InvalidateMessageRows();
}

void MessageBoard::OnPostEnd() {
    ImGui::PopStyleVar(3);
    m_FrameLayout.reset();

    // Update timers
    CKStats stats;
    BML_GetCKContext()->GetProfileStats(&stats);
    UpdateTimers(stats.TotalFrameTime);

    // Hide if no visible content
    if (!HasVisibleContent()) {
        Hide();
    }
}

// =============================================================================
// Public Interface
// =============================================================================

void MessageBoard::AddMessage(const char *msg) {
    AddMessageInternal(msg);
}

void MessageBoard::Printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    std::string message;
    const bool formatted = utils::FormatStringV(format, args, message);
    va_end(args);
    if (formatted)
        AddMessage(message.c_str());
}

void MessageBoard::PrintfColored(ImU32 color, const char *format, ...) {
    va_list args;
    va_start(args, format);
    std::string message;
    const bool formatted = utils::FormatStringV(format, args, message);
    va_end(args);
    if (!formatted)
        return;

    if (message.empty())
        message = "\n";
    AddMessageInternal(MessageUnit(std::move(message), m_MaxTimer, color));
}

void MessageBoard::ClearMessages() {
    m_MessageCount = 0;
    m_MessageHead = 0;
    m_DisplayMessageCount = 0;
    for (auto &message : m_Messages) {
        message.Reset();
    }
    Hide();
    InvalidateMessageRows();
}

void MessageBoard::ResizeMessages(int size) {
    if (size < 1) return;

    const int retainedCount = std::min(m_MessageCount, size);
    std::vector<MessageUnit> resized(static_cast<std::size_t>(size));
    for (int i = 0; i < retainedCount; ++i) {
        resized[i] = std::move(MessageAt(i));
    }
    m_Messages.swap(resized);
    m_MessageCount = retainedCount;
    m_MessageHead = 0;

    // Recount displayed messages since truncation may have removed active-timer entries
    int displayed = 0;
    for (int i = 0; i < m_MessageCount; ++i) {
        if (MessageAt(i).timer > 0.0f)
            ++displayed;
    }
    m_DisplayMessageCount = displayed;
    if (m_MessageCount == 0)
        Hide();
    InvalidateMessageRows();
}

// =============================================================================
// Configuration API
// =============================================================================

void MessageBoard::SetTabColumns(int columns) {
    int c = std::clamp(columns, 1, 64);
    if (m_TabColumns != c) {
        m_TabColumns = c;
        InvalidateLayoutCache();
    }
}

int MessageBoard::GetTabColumns() const {
    return m_TabColumns;
}

void MessageBoard::SetLineSpacing(float spacing) {
    if (spacing >= 0.0f) {
        const float clamped = std::max(0.0f, spacing);
        if (!m_LineSpacingOverride || std::abs(m_CustomLineSpacing - clamped) > 0.01f) {
            m_LineSpacingOverride = true;
            m_CustomLineSpacing = clamped;
            InvalidateLayoutCache();
        }
    } else if (m_LineSpacingOverride) {
        m_LineSpacingOverride = false;
        InvalidateLayoutCache();
    }
}

float MessageBoard::GetLineSpacing() const {
    return m_LineSpacingOverride ? m_CustomLineSpacing : -1.0f;
}

void MessageBoard::SetMessageBackgroundColor(ImVec4 color) {
    m_HasCustomMessageBg = true;
    m_MessageBgColor = color;
}

void MessageBoard::SetMessageBackgroundColorU32(ImU32 color) {
    SetMessageBackgroundColor(ImGui::ColorConvertU32ToFloat4(color));
}

void MessageBoard::ClearMessageBackgroundColor() {
    m_HasCustomMessageBg = false;
}

void MessageBoard::SetMessageBackgroundAlpha(float alpha) {
    m_MessageBgAlphaScale = std::clamp(alpha, 0.0f, 1.0f);
}

void MessageBoard::SetFadeMaxAlpha(float alpha) {
    m_FadeMaxAlpha = std::clamp(alpha, 0.0f, 1.0f);
}

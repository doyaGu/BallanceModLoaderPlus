#include "MessageBoard.h"

#include <cstdarg>
#include <algorithm>

#include "imgui_internal.h"

#include "ModContext.h"

// =============================================================================
// MessageUnit Implementation
// =============================================================================

MessageBoard::MessageUnit::MessageUnit(const char *msg, float timer) : timer(timer) {
    if (msg) {
        ansiText.SetText(msg);
    }
}

void MessageBoard::MessageUnit::SetMessage(const char *msg) {
    if (!msg) return;

    ansiText.SetText(msg);
    cachedHeight = -1.0f;
    cachedWrapWidth = -1.0f;
    cachedLineSpacing = -1.0f;
}

float MessageBoard::MessageUnit::GetTextHeight(float wrapWidth, float lineSpacing, int tabColumns) const {
    if (cachedHeight >= 0.0f && std::abs(cachedWrapWidth - wrapWidth) < 0.5f && std::abs(cachedLineSpacing - lineSpacing) < 0.5f)
        return cachedHeight;

    cachedHeight = AnsiText::CalculateHeight(ansiText, wrapWidth, 0, lineSpacing, tabColumns);
    cachedWrapWidth = wrapWidth;
    cachedLineSpacing = lineSpacing;
    return cachedHeight;
}

void MessageBoard::MessageUnit::Reset() {
    ansiText.Clear();
    timer = 0.0f;
    cachedHeight = -1.0f;
    cachedWrapWidth = -1.0f;
    cachedLineSpacing = -1.0f;
}


// =============================================================================
// MessageBoard Implementation
// =============================================================================

MessageBoard::MessageBoard(int size) : Bui::Window("MessageBoard") {
    if (size < 1) size = 500;
    m_Messages.resize(size);
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

    if (!m_IsCommandBarVisible) {
        flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav;
    }

    return flags;
}

void MessageBoard::SetCommandBarVisible(bool visible) {
    if (m_IsCommandBarVisible != visible) {
        m_IsCommandBarVisible = visible;

        if (visible) {
            m_ScrollToBottom = true;
            Show();
        } else {
            // Reset scroll state when hiding command bar
            m_ScrollY = 0.0f;
            m_MaxScrollY = 0.0f;
            m_ScrollToBottom = true;
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

float MessageBoard::GetMessageAlpha(const MessageUnit &msg) const {
    const float maxAlpha = std::clamp(m_FadeMaxAlpha, 0.0f, 1.0f);
    if (m_IsCommandBarVisible) {
        return maxAlpha;
    }

    if (msg.GetTimer() <= 0) {
        return 0.0f;
    }

    const float maxAlpha255 = maxAlpha * 255.0f;
    return std::min(maxAlpha255, msg.GetTimer() / 20.0f) / 255.0f;
}

int MessageBoard::CountVisibleMessages() const {
    int count = 0;
    for (int i = 0; i < m_MessageCount; i++) {
        if (ShouldShowMessage(m_Messages[i])) {
            count++;
        }
    }
    return count;
}

bool MessageBoard::HasVisibleContent() const {
    if (m_IsCommandBarVisible) {
        return m_MessageCount > 0;
    }
    return m_DisplayMessageCount > 0;
}

float MessageBoard::CalculateContentHeight(float wrapWidth) const {
    float contentHeight = 0.0f;
    int visibleCount = 0;

    for (int i = 0; i < m_MessageCount; i++) {
        const MessageUnit &msg = m_Messages[i];
        if (ShouldShowMessage(msg)) {
            contentHeight += msg.GetTextHeight(wrapWidth, m_MessageGap, m_TabColumns);
            visibleCount++;
        }
    }

    if (visibleCount > 1)
        contentHeight += m_MessageGap * (visibleCount - 1);

    // Return pure content height (no padding)
    return std::max(contentHeight, ImGui::GetTextLineHeightWithSpacing());
}

float MessageBoard::CalculateDisplayHeight(float contentHeight) const {
    // Add padding to content height to get total display height needed
    return contentHeight + m_PadY * 2.0f;
}

// =============================================================================
// Window Rendering
// =============================================================================

void MessageBoard::OnPreBegin() {
    // Pre-read style values BEFORE pushing style overrides
    const ImGuiStyle &style = ImGui::GetStyle();

    // Scale layout parameters with current font size (fallback to style when larger)
    const float fs = ImGui::GetStyle().FontSizeBase;
    const float pad_x_scaled = fs * 0.5f;    // ~8px @16pt
    const float pad_y_scaled = fs * 0.5f;    // ~8px @16pt
    const float gap_scaled = fs * 0.25f;     // ~4px @16pt
    const float sb_w_scaled = fs * 0.5f;     // ~8px @16pt
    const float sb_pad_scaled = fs * 0.125f; // ~2px @16pt
    m_PadX = ImMax((style.WindowPadding.x > 0.0f) ? style.WindowPadding.x : 0.0f, pad_x_scaled);
    m_PadY = ImMax((style.WindowPadding.y > 0.0f) ? style.WindowPadding.y : 0.0f, pad_y_scaled);
    m_MessageGap = ImMax((style.ItemSpacing.y > 0.0f) ? style.ItemSpacing.y : 0.0f, gap_scaled);
    m_ScrollbarW = ImMax((style.ScrollbarSize > 0.0f) ? style.ScrollbarSize : 0.0f, sb_w_scaled);
    m_ScrollbarPad = ImMax((style.ItemInnerSpacing.x > 0.0f) ? (style.ItemInnerSpacing.x * 0.5f) : 0.0f, sb_pad_scaled);

    // Push style overrides
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImVec4 winBg = m_HasCustomWindowBg ? m_WindowBgColor : Bui::GetMenuColor();
    winBg.w = std::clamp(winBg.w * std::clamp(m_WindowBgAlphaScale, 0.0f, 1.0f), 0.0f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, winBg);

    const ImVec2 vpSize = ImGui::GetMainViewport()->Size;
    const float windowWidth = vpSize.x * 0.96f;
    const float wrapWidth = windowWidth - m_PadX * 2.0f;
    const float maxDisplayHeight = vpSize.y * 0.8f;

    const float contentHeight = CalculateContentHeight(wrapWidth);
    const float displayHeight = CalculateDisplayHeight(contentHeight);
    float windowHeight = std::min(displayHeight, maxDisplayHeight);

    const float bottomAnchor = vpSize.y * 0.9f;
    const float posY = bottomAnchor - windowHeight;
    const float posX = vpSize.x * 0.02f;

    ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
}

void MessageBoard::OnDraw() {
    if (!HasVisibleContent()) {
        return;
    }

    // Keep messages on top in normal mode; when command bar is visible,
    // allow the command bar to overlay the message board if they overlap.
    if (!m_IsCommandBarVisible)
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    const ImVec2 contentPos = ImGui::GetCursorScreenPos();
    const ImVec2 contentSize = ImGui::GetContentRegionAvail();
    const float baseWrapWidth = contentSize.x - m_PadX * 2.0f;

    // Calculate content dimensions for scrolling
    float contentHeightNoSB = CalculateContentHeight(baseWrapWidth);
    const float availableContentHeight = contentSize.y - m_PadY * 2.0f;

    // Determine if a scrollbar is needed when command bar is visible.
    bool needsScrollbar = m_IsCommandBarVisible && (contentHeightNoSB > availableContentHeight);
    float wrapWidth = baseWrapWidth;
    float contentHeight = contentHeightNoSB;
    if (needsScrollbar) {
        wrapWidth = std::max(0.0f, baseWrapWidth - (m_ScrollbarW + m_ScrollbarPad * 2.0f));
        contentHeight = CalculateContentHeight(wrapWidth); // recompute since wrap width shrinks
    }

    // Handle scrolling when command bar is visible
    if (m_IsCommandBarVisible) {
        if (needsScrollbar) {
            UpdateScrollBounds(contentHeight, availableContentHeight);
            HandleScrolling(availableContentHeight);
        } else {
            m_ScrollY = 0.0f;
            m_MaxScrollY = 0.0f;
            m_ScrollToBottom = true;
        }
    }

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    const ImVec2 contentStart = ImVec2(contentPos.x + m_PadX, contentPos.y + m_PadY);

    ImVec2 startPos = ImVec2(contentStart.x, contentStart.y - m_ScrollY);
    // Set up clipping for content area
    const ImVec2 clipMin = ImVec2(contentPos.x + m_PadX, contentPos.y + m_PadY);
    const ImVec2 clipMax = ImVec2(
        contentPos.x + contentSize.x - m_PadX - (m_IsCommandBarVisible && needsScrollbar ? (m_ScrollbarW + m_ScrollbarPad * 2.0f) : 0.0f),
        contentPos.y + contentSize.y - m_PadY
    );
    drawList->PushClipRect(clipMin, clipMax, true);

    RenderMessages(drawList, startPos, wrapWidth);

    drawList->PopClipRect();
    if (m_IsCommandBarVisible && needsScrollbar && m_MaxScrollY > 0.0f) {
        DrawScrollIndicators(drawList, contentPos, contentSize, contentHeight, availableContentHeight);
    }
}

void MessageBoard::RenderMessages(ImDrawList *drawList, ImVec2 startPos, float wrapWidth) {
    ImVec4 bgColorBase = m_HasCustomMessageBg ? m_MessageBgColor : Bui::GetMenuColor();

    // Prepare visible indices (display order: newest first) and cached heights
    std::vector<int> indices;
    indices.reserve(m_MessageCount);
    std::vector<float> heights;
    heights.reserve(m_MessageCount);

    for (int i = m_MessageCount - 1; i >= 0; --i) {
        const MessageUnit &msg = m_Messages[i];
        const bool shouldShow = m_IsCommandBarVisible || ShouldShowMessage(msg);
        if (!shouldShow)
            continue;
        indices.push_back(i);
        heights.push_back(msg.GetTextHeight(wrapWidth, m_MessageGap, m_TabColumns));
    }

    const int n = (int)indices.size();
    if (n == 0)
        return;

    // Precompute offsets (top Y of each message relative to startPos.y) and bottoms
    std::vector<float> offsets;
    offsets.resize(n);
    std::vector<float> bottoms;
    bottoms.resize(n);
    float acc = 0.0f;
    for (int j = 0; j < n; ++j) {
        offsets[j] = acc;
        const float h = heights[j];
        bottoms[j] = acc + h;                 // bottom Y (relative)
        acc += h + m_MessageGap;              // advance incl. gap (gap after last is harmless)
    }

    // Determine visible index range against current clip rect
    const ImVec2 clip_min = drawList->GetClipRectMin();
    const ImVec2 clip_max = drawList->GetClipRectMax();
    const float clipMinRel = clip_min.y - startPos.y;
    const float clipMaxRel = clip_max.y - startPos.y;

    auto lb = std::lower_bound(bottoms.begin(), bottoms.end(), clipMinRel);
    int begin = (int)std::distance(bottoms.begin(), lb);
    auto ub = std::upper_bound(offsets.begin(), offsets.end(), clipMaxRel);
    int end = (int)std::distance(offsets.begin(), ub);
    begin = std::clamp(begin, 0, n);
    end = std::clamp(end, begin, n);

    // Use ImGuiListClipper to iterate the visible range (for correctness and consistency with other code)
    ImGuiListClipper clipper;
    // Anchor clipper cursor to our content start so its internal math stays consistent
    ImGui::SetCursorScreenPos(startPos);
    clipper.Begin(n, 1.0f); // items_height non-zero to skip measurement path
    clipper.IncludeItemsByIndex(begin, end);
    while (clipper.Step()) {
        // Draw only our intended [begin, end) range to preserve behavior
        const int ds = ImMax(clipper.DisplayStart, begin);
        const int de = ImMin(clipper.DisplayEnd, end);
        for (int j = ds; j < de; ++j) {
            const int i = indices[j];
            const MessageUnit &msg = m_Messages[i];
            const float msgHeight = heights[j];
            ImVec2 pos = ImVec2(startPos.x, startPos.y + offsets[j]);

            // Keep ImGui cursor in sync with the draw position so nested clippers work while scrolling.
            ImGui::SetCursorScreenPos(pos);

            const float alpha = GetMessageAlpha(msg);
            if (alpha > 0.0f) {
                const float finalAlpha = std::clamp(bgColorBase.w * std::clamp(m_MessageBgAlphaScale, 0.0f, 1.0f) * alpha, 0.0f, 1.0f);
                if (finalAlpha > 0.0f) {
                    const ImVec4 bg = ImVec4(bgColorBase.x, bgColorBase.y, bgColorBase.z, finalAlpha);
                    drawList->AddRectFilled(
                        ImVec2(pos.x - m_PadX * 0.5f, pos.y - m_PadY * 0.25f),
                        ImVec2(pos.x + wrapWidth + m_PadX * 0.5f, pos.y + msgHeight + m_PadY * 0.25f),
                        ImGui::GetColorU32(bg)
                    );
                }

                // Text
                DrawMessageText(drawList, msg, pos, wrapWidth, alpha);
            }

            // Feed clipper a logical item advance (height + gap). We don't rely on its cursor for drawing.
            ImGui::ItemSize(ImVec2(0.0f, msgHeight + m_MessageGap));
        }
    }
}

void MessageBoard::DrawMessageText(ImDrawList *drawList, const MessageUnit &message, const ImVec2 &startPos, float wrapWidth, float alpha) {
    AnsiText::Renderer::DrawText(drawList, message.ansiText, startPos, wrapWidth, alpha, 0.0f, m_MessageGap, m_TabColumns, nullptr);
}

// =============================================================================
// Scrolling System
// =============================================================================

void MessageBoard::HandleScrolling(float availableHeight) {
    if (!m_IsCommandBarVisible || m_MaxScrollY <= 0.0f) return;

    const ImGuiIO &io = ImGui::GetIO();

    // Mouse wheel scrolling
    if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f) {
        const ImGuiStyle &st = ImGui::GetStyle();
        const float scrollSpeed = (st.FontSizeBase + st.ItemSpacing.y) * 3.0f;
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
    for (auto &m : m_Messages) {
        m.cachedHeight = -1.0f;
        m.cachedWrapWidth = -1.0f;
    }
}

void MessageBoard::DrawScrollIndicators(ImDrawList *drawList, const ImVec2 &contentPos, const ImVec2 &contentSize, float contentHeight, float visibleHeight) {
    if (m_MaxScrollY <= 0.0f) return;

    // Scrollbar background
    const ImVec2 scrollbarStart = ImVec2(
        contentPos.x + contentSize.x - m_ScrollbarW - m_ScrollbarPad,
        contentPos.y + m_PadY + m_ScrollbarPad
    );
    const ImVec2 scrollbarEnd = ImVec2(
        contentPos.x + contentSize.x - m_ScrollbarPad,
        contentPos.y + contentSize.y - m_PadY - m_ScrollbarPad
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
        const std::string scrollText = FormatScrollPercent(contentHeight, visibleHeight);
        const ImVec2 textSize = ImGui::CalcTextSize(scrollText.c_str());
        const ImVec2 textPos = ImVec2(
            contentPos.x + contentSize.x - textSize.x - m_ScrollbarW - m_ScrollbarPad - m_PadX,
            contentPos.y + m_PadY * 0.5f
        );

        // Text background
        drawList->AddRectFilled(
            ImVec2(textPos.x - m_PadX * 0.25f, textPos.y - m_PadY * 0.25f),
            ImVec2(textPos.x + textSize.x + m_PadX * 0.25f, textPos.y + textSize.y + m_PadY * 0.25f),
            IM_COL32(0, 0, 0, 150)
        );

        // Text
        drawList->AddText(textPos, IM_COL32(255, 255, 255, 200), scrollText.c_str());
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

void MessageBoard::SyncScrollBottomFlag() {
    m_ScrollToBottom = (m_ScrollY >= m_MaxScrollY - m_ScrollEpsilon);
}

std::string MessageBoard::FormatScrollPercent(float contentHeight, float visibleHeight) const {
    const ScrollMetrics m = GetScrollMetrics(contentHeight, visibleHeight);
    const float pct = (m.maxScroll > 0.0f) ? (m.scrollRatio * 100.0f) : 0.0f;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f%%", pct);
    return std::string(buf);
}

// =============================================================================
// Message Management
// =============================================================================

void MessageBoard::UpdateTimers(float deltaTime) {
    for (int i = 0; i < m_MessageCount; i++) {
        if (m_Messages[i].timer > 0.0f) {
            m_Messages[i].timer -= deltaTime;
            if (m_Messages[i].timer <= 0.0f) {
                m_Messages[i].timer = 0.0f;
                --m_DisplayMessageCount;
            }
        }
    }
}

void MessageBoard::AddMessageInternal(const char *msg) {
    if (!msg) return;

    if (msg[0] == '\0')
        msg = "\n"; // treat empty messages as newlines

    // Update display count
    if (m_MessageCount == static_cast<int>(m_Messages.size()) && m_Messages[m_MessageCount - 1].GetTimer() > 0) {
        --m_DisplayMessageCount;
    }

    // Shift messages
    const int shiftCount = std::min(m_MessageCount, static_cast<int>(m_Messages.size()) - 1);
    for (int i = shiftCount - 1; i >= 0; i--) {
        m_Messages[i + 1] = std::move(m_Messages[i]);
    }

    // Add new message
    m_Messages[0] = MessageUnit(msg, m_MaxTimer);

    if (m_MessageCount < static_cast<int>(m_Messages.size())) {
        ++m_MessageCount;
    }
    ++m_DisplayMessageCount;

    // Auto-scroll to bottom for new messages
    if (m_IsCommandBarVisible && (m_ScrollToBottom || m_MaxScrollY <= 0.0f)) {
        m_ScrollToBottom = true;
    }
}

void MessageBoard::OnPostEnd() {
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    // Update timers
    CKStats stats;
    BML_GetCKContext()->GetProfileStats(&stats);
    UpdateTimers(stats.TotalFrameTime);

    // Hide if no visible content
    if (!m_IsCommandBarVisible && m_DisplayMessageCount == 0) {
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
    int needed = vsnprintf(nullptr, 0, format, args);
    va_end(args);
    if (needed < 0) return;
    std::string buf((size_t) needed + 1, '\0');
    va_start(args, format);
    vsnprintf(buf.data(), buf.size(), format, args);
    va_end(args);
    // Remove the embedded NUL we reserved space for
    buf.resize((size_t)needed);
    AddMessage(buf.c_str());
}

void MessageBoard::PrintfColored(ImU32 color, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int needed = vsnprintf(nullptr, 0, format, args);
    va_end(args);
    if (needed < 0) return;
    std::string buf((size_t) needed + 1, '\0');
    va_start(args, format);
    vsnprintf(buf.data(), buf.size(), format, args);
    va_end(args);
    buf.resize((size_t)needed);

    const ImU32 r = (color >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (color >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (color >> IM_COL32_B_SHIFT) & 0xFF;

    std::string coloredBuffer = "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m" + buf + "\033[0m";
    AddMessage(coloredBuffer.c_str());
}

void MessageBoard::ClearMessages() {
    m_MessageCount = 0;
    m_DisplayMessageCount = 0;
    for (auto &message : m_Messages) {
        message.Reset();
    }
}

void MessageBoard::ResizeMessages(int size) {
    if (size < 1) return;

    m_Messages.resize(size);
    m_MessageCount = std::min(m_MessageCount, size);
    m_DisplayMessageCount = std::min(m_DisplayMessageCount, size);
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

void MessageBoard::SetWindowBackgroundColor(ImVec4 color) {
    m_HasCustomWindowBg = true;
    m_WindowBgColor = color;
}

void MessageBoard::SetWindowBackgroundColorU32(ImU32 color) {
    SetWindowBackgroundColor(ImGui::ColorConvertU32ToFloat4(color));
}

void MessageBoard::ClearWindowBackgroundColor() {
    m_HasCustomWindowBg = false;
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

void MessageBoard::SetWindowBackgroundAlpha(float alpha) {
    m_WindowBgAlphaScale = std::clamp(alpha, 0.0f, 1.0f);
}

void MessageBoard::SetMessageBackgroundAlpha(float alpha) {
    m_MessageBgAlphaScale = std::clamp(alpha, 0.0f, 1.0f);
}

void MessageBoard::SetFadeMaxAlpha(float alpha) {
    m_FadeMaxAlpha = std::clamp(alpha, 0.0f, 1.0f);
}

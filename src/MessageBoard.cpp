#include "MessageBoard.h"

#include <cmath>
#include <sstream>
#include <cstdarg>
#include <algorithm>
#include <regex>

#include "imgui_internal.h"
#include "ModContext.h"

namespace {
    // Apply dim effect to color
    ImU32 ApplyDimEffect(ImU32 color) {
        const ImU32 r = ((color >> IM_COL32_R_SHIFT) & 0xFF) / 2;
        const ImU32 g = ((color >> IM_COL32_G_SHIFT) & 0xFF) / 2;
        const ImU32 b = ((color >> IM_COL32_B_SHIFT) & 0xFF) / 2;
        const ImU32 a = (color >> IM_COL32_A_SHIFT) & 0xFF;
        return IM_COL32(r, g, b, a);
    }

    // Apply blink effect (alpha modulation)
    ImU32 ApplyBlinkEffect(ImU32 color, float time) {
        const float alpha = (std::sin(time * 4.0f) + 1.0f) * 0.5f;
        const ImU32 originalAlpha = (color >> IM_COL32_A_SHIFT) & 0xFF;
        const ImU32 newAlpha = static_cast<ImU32>(originalAlpha * alpha);
        return (color & 0x00FFFFFF) | (newAlpha << IM_COL32_A_SHIFT);
    }

    // Calculate wrapped text size consistently
    ImVec2 CalcWrappedTextSize(const std::string &text, float wrapWidth) {
        if (text.empty()) return ImVec2(0, 0);
        if (wrapWidth <= 0) return ImGui::CalcTextSize(text.c_str());
        return ImGui::CalcTextSize(text.c_str(), nullptr, false, wrapWidth);
    }
}

// =============================================================================
// MessageUnit Implementation
// =============================================================================

MessageBoard::MessageUnit::MessageUnit(const char *msg, float timer)
    : originalText(msg ? msg : ""), timer(timer) {
    if (msg && strlen(msg) > 0) {
        ParseAnsiEscapeCodes();
    }
}

void MessageBoard::MessageUnit::SetMessage(const char *msg) {
    if (!msg) return;

    originalText = msg;
    segments.clear();
    cachedHeight = -1.0f;
    cachedWrapWidth = -1.0f;

    ParseAnsiEscapeCodes();
}

float MessageBoard::MessageUnit::GetTextHeight(float wrapWidth) const {
    if (cachedHeight < 0.0f || std::abs(cachedWrapWidth - wrapWidth) >= 0.5f) {
        if (segments.empty()) {
            cachedHeight = ImGui::GetTextLineHeightWithSpacing();
        } else {
            float totalHeight = 0.0f;
            float currentLineWidth = 0.0f;
            const float lineHeight = ImGui::GetTextLineHeightWithSpacing();

            for (const auto &segment : segments) {
                if (segment.text.empty()) continue;

                size_t pos = 0;
                while (pos < segment.text.length()) {
                    size_t newlinePos = segment.text.find('\n', pos);
                    size_t endPos = (newlinePos != std::string::npos) ? newlinePos : segment.text.length();

                    if (endPos > pos) {
                        std::string line = segment.text.substr(pos, endPos - pos);
                        float remainingWidth = std::max(0.0f, wrapWidth - currentLineWidth);
                        ImVec2 textSize = CalcWrappedTextSize(line, remainingWidth);

                        if (currentLineWidth + textSize.x > wrapWidth && wrapWidth > 0 && currentLineWidth > 0) {
                            totalHeight += lineHeight;
                            currentLineWidth = textSize.x;
                            if (textSize.y > lineHeight) {
                                totalHeight += textSize.y - lineHeight;
                            }
                        } else {
                            currentLineWidth += textSize.x;
                            if (textSize.y > lineHeight) {
                                totalHeight += textSize.y - lineHeight;
                            }
                        }
                    }

                    if (newlinePos != std::string::npos) {
                        totalHeight += lineHeight;
                        currentLineWidth = 0.0f;
                        pos = newlinePos + 1;
                    } else {
                        break;
                    }
                }
            }

            if (currentLineWidth > 0 || totalHeight == 0.0f) {
                totalHeight += lineHeight;
            }

            cachedHeight = totalHeight;
        }
        cachedWrapWidth = wrapWidth;
    }
    return cachedHeight;
}

void MessageBoard::MessageUnit::Reset() {
    originalText.clear();
    segments.clear();
    timer = 0.0f;
    cachedHeight = -1.0f;
    cachedWrapWidth = -1.0f;
}

void MessageBoard::MessageUnit::ParseAnsiEscapeCodes() {
    segments.clear();

    if (originalText.empty()) {
        return;
    }

    const char *p = originalText.c_str();
    ConsoleColor currentColor;
    std::string currentSegment;

    while (*p) {
        if (*p == '\033' && *(p + 1) == '[') {
            // Save current text segment
            if (!currentSegment.empty()) {
                segments.emplace_back(std::move(currentSegment), currentColor);
                currentSegment.clear();
            }

            // Parse escape sequence
            p += 2; // Skip \033[
            std::string sequence;

            // Only parse color/style sequences (ending with 'm')
            while (*p && *p != 'm' && sequence.length() < 50) {
                sequence += *p++;
            }

            if (*p == 'm') {
                p++; // Skip the 'm'
                currentColor = ParseAnsiColorSequence(sequence, currentColor);
            } else {
                // Invalid sequence - treat as literal text
                currentSegment += '\033';
                currentSegment += '[';
                currentSegment += sequence;
            }
        } else {
            currentSegment += *p++;
        }
    }

    // Add final segment
    if (!currentSegment.empty()) {
        segments.emplace_back(std::move(currentSegment), currentColor);
    }

    // Ensure at least one segment exists
    if (segments.empty()) {
        segments.emplace_back(originalText, ConsoleColor());
    }
}

MessageBoard::ConsoleColor MessageBoard::MessageUnit::ParseAnsiColorSequence(const std::string &sequence, const ConsoleColor &currentColor) {
    ConsoleColor color = currentColor;
    if (sequence.empty()) return color;

    // Parse semicolon-separated codes
    std::vector<int> codes;
    size_t start = 0;
    for (size_t pos = 0; pos <= sequence.length(); ++pos) {
        if (pos == sequence.length() || sequence[pos] == ';') {
            if (pos > start) {
                std::string codeStr = sequence.substr(start, pos - start);
                if (!codeStr.empty() && std::all_of(codeStr.begin(), codeStr.end(), ::isdigit)) {
                    int code = std::stoi(codeStr);
                    if (code >= 0) {
                        codes.push_back(code);
                    }
                }
            }
            start = pos + 1;
        }
    }

    // Process each color code
    for (size_t i = 0; i < codes.size(); ++i) {
        int code = codes[i];

        if (code == 0) {
            // Reset all
            color = ConsoleColor();
        } else if (code >= 30 && code <= 37) {
            // Standard foreground colors
            color.foreground = GetStandardColor(code - 30);
        } else if (code >= 40 && code <= 47) {
            // Standard background colors
            color.background = GetStandardColor(code - 40);
            color.background = (color.background & 0x00FFFFFF) | 0xFF000000;
        } else if (code >= 90 && code <= 97) {
            // Bright foreground colors
            color.foreground = GetStandardColor(code - 90, true);
        } else if (code >= 100 && code <= 107) {
            // Bright background colors
            color.background = GetStandardColor(code - 100, true);
            color.background = (color.background & 0x00FFFFFF) | 0xFF000000;
        } else if ((code == 38 || code == 48) && i + 1 < codes.size()) {
            // Extended color codes
            bool isBackground = (code == 48);
            if (codes[i + 1] == 5 && i + 2 < codes.size()) {
                // 256-color mode
                ImU32 colorValue = Get256Color(codes[i + 2]);
                if (isBackground) {
                    color.background = (colorValue & 0x00FFFFFF) | 0xFF000000;
                } else {
                    color.foreground = colorValue;
                }
                i += 2;
            } else if (codes[i + 1] == 2 && i + 4 < codes.size()) {
                // RGB mode
                ImU32 colorValue = GetRgbColor(codes[i + 2], codes[i + 3], codes[i + 4]);
                if (isBackground) {
                    color.background = (colorValue & 0x00FFFFFF) | 0xFF000000;
                } else {
                    color.foreground = colorValue;
                }
                i += 4;
            }
        } else if (code == 39) {
            // Default foreground color
            color.foreground = IM_COL32_WHITE;
        } else if (code == 49) {
            // Default background color
            color.background = IM_COL32(0, 0, 0, 0);
        } else {
            // Style codes
            switch (code) {
            case 1: color.bold = true; break;
            case 2: color.dim = true; break;
            case 3: color.italic = true; break;
            case 4: color.underline = true; break;
            case 5: case 6: color.blink = true; break;
            case 7: color.reverse = true; break;
            case 8: color.hidden = true; break;
            case 9: color.strikethrough = true; break;
            case 21: case 22: color.bold = false; color.dim = false; break;
            case 23: color.italic = false; break;
            case 24: color.underline = false; break;
            case 25: color.blink = false; break;
            case 27: color.reverse = false; break;
            case 28: color.hidden = false; break;
            case 29: color.strikethrough = false; break;
            default: break; // Ignore unknown codes
            }
        }
    }

    return color;
}

ImU32 MessageBoard::MessageUnit::GetStandardColor(int colorCode, bool bright) {
    static constexpr ImU32 standardColors[] = {
        IM_COL32(0, 0, 0, 255),      // Black
        IM_COL32(128, 0, 0, 255),    // Red
        IM_COL32(0, 128, 0, 255),    // Green
        IM_COL32(128, 128, 0, 255),  // Yellow
        IM_COL32(0, 0, 128, 255),    // Blue
        IM_COL32(128, 0, 128, 255),  // Magenta
        IM_COL32(0, 128, 128, 255),  // Cyan
        IM_COL32(192, 192, 192, 255) // White
    };

    static constexpr ImU32 brightColors[] = {
        IM_COL32(128, 128, 128, 255), // Bright Black
        IM_COL32(255, 0, 0, 255),     // Bright Red
        IM_COL32(0, 255, 0, 255),     // Bright Green
        IM_COL32(255, 255, 0, 255),   // Bright Yellow
        IM_COL32(0, 0, 255, 255),     // Bright Blue
        IM_COL32(255, 0, 255, 255),   // Bright Magenta
        IM_COL32(0, 255, 255, 255),   // Bright Cyan
        IM_COL32(255, 255, 255, 255)  // Bright White
    };

    if (colorCode >= 0 && colorCode < 8) {
        return bright ? brightColors[colorCode] : standardColors[colorCode];
    }
    return IM_COL32_WHITE;
}

ImU32 MessageBoard::MessageUnit::Get256Color(int colorIndex) {
    if (colorIndex < 0 || colorIndex > 255) return IM_COL32_WHITE;

    if (colorIndex < 16) {
        return GetStandardColor(colorIndex % 8, colorIndex >= 8);
    } else if (colorIndex < 232) {
        // 6x6x6 color cube
        int idx = colorIndex - 16;
        int r6 = (idx / 36) % 6;
        int g6 = ((idx % 36) / 6) % 6;
        int b6 = idx % 6;

        static const int values[6] = {0, 95, 135, 175, 215, 255};
        int r = values[r6];
        int g = values[g6];
        int b = values[b6];

        return IM_COL32(r, g, b, 255);
    } else {
        // Grayscale ramp
        int gray = std::min(255, 8 + (colorIndex - 232) * 10);
        return IM_COL32(gray, gray, gray, 255);
    }
}

ImU32 MessageBoard::MessageUnit::GetRgbColor(int r, int g, int b) {
    return IM_COL32(
        std::clamp(r, 0, 255),
        std::clamp(g, 0, 255),
        std::clamp(b, 0, 255),
        255
    );
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
                             ImGuiWindowFlags_NoBringToFrontOnFocus;

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
        m_ScrollY = std::clamp(scrollY, 0.0f, m_MaxScrollY);
        m_ScrollToBottom = (m_ScrollY >= m_MaxScrollY - 0.5f);
    }
}

void MessageBoard::ScrollToTop() {
    if (m_IsCommandBarVisible) {
        m_ScrollY = 0.0f;
        m_ScrollToBottom = false;
    }
}

void MessageBoard::ScrollToBottom() {
    if (m_IsCommandBarVisible) {
        m_ScrollY = m_MaxScrollY;
        m_ScrollToBottom = true;
    }
}

// =============================================================================
// Height Calculation System (Redesigned)
// =============================================================================

bool MessageBoard::ShouldShowMessage(const MessageUnit &msg) const {
    return m_IsCommandBarVisible || msg.GetTimer() > 0;
}

float MessageBoard::GetMessageAlpha(const MessageUnit &msg) const {
    if (m_IsCommandBarVisible) {
        return 180.0f / 255.0f;
    }

    if (msg.GetTimer() <= 0) {
        return 0.0f;
    }

    return std::min(180.0f, msg.GetTimer() / 20.0f) / 255.0f;
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
            contentHeight += msg.GetTextHeight(wrapWidth);
            if (visibleCount > 0) {
                contentHeight += m_MessageGap;
            }
            visibleCount++;
        }
    }

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

    // Cache style-derived layout parameters with fallbacks
    m_PadX = (style.WindowPadding.x > 0.0f) ? style.WindowPadding.x : (style.FramePadding.x * 2.0f);
    m_PadY = (style.WindowPadding.y > 0.0f) ? style.WindowPadding.y : (style.FramePadding.y * 2.0f);
    m_MessageGap = (style.ItemSpacing.y > 0.0f) ? style.ItemSpacing.y : 4.0f;
    m_ScrollbarW = (style.ScrollbarSize > 0.0f) ? style.ScrollbarSize : 8.0f;
    m_ScrollbarPad = (style.ItemInnerSpacing.x > 0.0f) ? (style.ItemInnerSpacing.x * 0.5f) : 2.0f;

    // Push style overrides
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Bui::GetMenuColor());

    const ImVec2 vpSize = ImGui::GetMainViewport()->Size;
    const float windowWidth = vpSize.x * 0.96f;
    const float wrapWidth = windowWidth - m_PadX * 2.0f;
    const float maxDisplayHeight = vpSize.y * 0.8f;

    const float contentHeight = CalculateContentHeight(wrapWidth);
    const float displayHeight = CalculateDisplayHeight(contentHeight);
    float windowHeight = std::min(displayHeight, maxDisplayHeight);

    const float bottomAnchor = vpSize.y * 0.9f;
    float posY = bottomAnchor - windowHeight;

    const float posX = vpSize.x * 0.02f;

    ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
}

void MessageBoard::OnDraw() {
    if (!HasVisibleContent()) {
        return;
    }

    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    const ImVec2 contentPos = ImGui::GetCursorScreenPos();
    const ImVec2 contentSize = ImGui::GetContentRegionAvail();
    const float wrapWidth = contentSize.x - m_PadX * 2.0f;

    // Calculate content dimensions for scrolling
    const float contentHeight = CalculateContentHeight(wrapWidth);
    const float availableContentHeight = contentSize.y - m_PadY * 2.0f;

    bool needsScrollbar = false;

    // Handle scrolling when command bar is visible
    if (m_IsCommandBarVisible) {
        needsScrollbar = contentHeight > availableContentHeight;

        if (needsScrollbar) {
            UpdateScrollBounds(contentHeight, availableContentHeight);
            HandleScrolling(contentHeight, availableContentHeight);
        } else {
            m_ScrollY = 0.0f;
            m_MaxScrollY = 0.0f;
            m_ScrollToBottom = true;
        }
    }

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImVec2 startPos = ImVec2(contentPos.x + m_PadX, contentPos.y + m_PadY - m_ScrollY);

    // Set up clipping for scrollable content
    if (m_IsCommandBarVisible) {
        const ImVec2 clipMin = ImVec2(contentPos.x + m_PadX, contentPos.y + m_PadY);
        const ImVec2 clipMax = ImVec2(contentPos.x + contentSize.x - m_PadX, contentPos.y + contentSize.y - m_PadY);
        drawList->PushClipRect(clipMin, clipMax, true);
    }

    RenderMessages(drawList, startPos, wrapWidth);

    if (m_IsCommandBarVisible) {
        drawList->PopClipRect();
        if (needsScrollbar && m_MaxScrollY > 0.0f) {
            DrawScrollIndicators(drawList, contentPos, contentSize);
        }
    }
}

void MessageBoard::RenderMessages(ImDrawList *drawList, ImVec2 startPos, float wrapWidth) {
    ImVec4 bgColor = Bui::GetMenuColor();
    ImVec2 currentPos = startPos;

    for (int i = m_MessageCount - 1; i >= 0; i--) {
        const MessageUnit &msg = m_Messages[i];
        bool shouldShow = m_IsCommandBarVisible || ShouldShowMessage(msg);

        if (shouldShow) {
            const float alpha = GetMessageAlpha(msg);
            const float msgHeight = msg.GetTextHeight(wrapWidth);

            // Draw background with style-derived padding
            drawList->AddRectFilled(
                ImVec2(currentPos.x - m_PadX * 0.5f, currentPos.y - m_PadY * 0.25f),
                ImVec2(currentPos.x + wrapWidth + m_PadX * 0.5f, currentPos.y + msgHeight + m_PadY * 0.25f),
                ImGui::GetColorU32(ImVec4(bgColor.x, bgColor.y, bgColor.z, bgColor.w * alpha))
            );

            DrawMessageText(drawList, msg, currentPos, wrapWidth, alpha);
            currentPos.y += msgHeight + m_MessageGap;
        }
    }
}

void MessageBoard::DrawMessageText(ImDrawList *drawList, const MessageUnit &message, const ImVec2 &startPos, float wrapWidth, float alpha) {
    ImVec2 currentPos = startPos;

    for (const auto &segment : message.segments) {
        DrawTextSegment(drawList, segment, currentPos, wrapWidth, alpha);
    }
}

void MessageBoard::DrawTextSegment(ImDrawList *drawList, const TextSegment &segment, ImVec2 &currentPos, float wrapWidth, float alpha) {
    if (segment.text.empty()) return;

    const ImVec2 startPos = ImVec2(currentPos.x, currentPos.y);
    const float lineHeight = ImGui::GetTextLineHeightWithSpacing();

    // Get rendered color with reverse effect applied
    ConsoleColor renderColor = m_AnsiEnabled
                                   ? segment.color.GetRendered()
                                   : ConsoleColor(segment.color.foreground, IM_COL32(0, 0, 0, 0));

    // Apply alpha to colors
    const auto applyAlpha = [alpha](ImU32 color) {
        const ImU32 originalAlpha = (color >> IM_COL32_A_SHIFT) & 0xFF;
        if (originalAlpha == 0) return ImU32(0);
        const ImU32 newAlpha = static_cast<ImU32>(static_cast<float>(originalAlpha) * alpha);
        return (color & 0x00FFFFFF) | (newAlpha << IM_COL32_A_SHIFT);
    };

    auto tone = [this](ImU32 c)-> ImU32 {
        if (((c >> IM_COL32_A_SHIFT) & 0xFF) == 0) return c;
        float r = ((c >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
        float g = ((c >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
        float b = ((c >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
        float h, s, v;
        ImGui::ColorConvertRGBtoHSV(r, g, b, h, s, v);
        s *= m_ColorSaturation;
        v = ImClamp(v * m_ColorValue, 0.0f, 1.0f);
        ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
        return IM_COL32((int)(r * 255.0f), (int)(g * 255.0f), (int)(b * 255.0f), (c >> IM_COL32_A_SHIFT) & 0xFF);
    };

    ImU32 fgColor = m_AnsiEnabled ? tone(renderColor.foreground) : renderColor.foreground;
    ImU32 bgColor = m_AnsiEnabled ? applyAlpha(tone(renderColor.background)) : applyAlpha(renderColor.background);

    // Apply effects to foreground color if ANSI is enabled
    if (m_AnsiEnabled) {
        if (renderColor.dim) {
            fgColor = ApplyDimEffect(fgColor);
        }
        if (renderColor.blink) {
            fgColor = ApplyBlinkEffect(fgColor, m_BlinkTime);
        }
        if (renderColor.hidden) {
            fgColor = bgColor;
        }
    }

    // Process text with special character handling
    size_t pos = 0;
    while (pos < segment.text.length()) {
        size_t nextSpecial = pos;
        while (nextSpecial < segment.text.length() &&
            segment.text[nextSpecial] != '\n' &&
            segment.text[nextSpecial] != '\r' &&
            segment.text[nextSpecial] != '\t') {
            nextSpecial++;
        }

        // Render text chunk
        if (nextSpecial > pos) {
            std::string textChunk = segment.text.substr(pos, nextSpecial - pos);
            float remainingWidth = std::max(0.0f, wrapWidth - (currentPos.x - startPos.x));

            ImVec2 textSize = CalcWrappedTextSize(textChunk, remainingWidth);

            // Word wrap if needed
            if (currentPos.x + textSize.x > startPos.x + wrapWidth && currentPos.x > startPos.x) {
                currentPos.y += lineHeight;
                currentPos.x = startPos.x;
                remainingWidth = wrapWidth;
                textSize = CalcWrappedTextSize(textChunk, remainingWidth);
            }

            // Draw background if visible
            if ((bgColor >> IM_COL32_A_SHIFT) > 0) {
                const float bgPadding = m_PadY * 0.125f;
                drawList->AddRectFilled(
                    ImVec2(currentPos.x - bgPadding, currentPos.y - bgPadding),
                    ImVec2(currentPos.x + textSize.x + bgPadding, currentPos.y + textSize.y + bgPadding),
                    bgColor
                );
            }

            // Render text with effects
            RenderText(drawList, textChunk, currentPos, fgColor, renderColor);
            currentPos.x += textSize.x;
        }

        // Handle special characters
        if (nextSpecial < segment.text.length()) {
            HandleSpecialCharacter(segment.text[nextSpecial], currentPos, startPos, wrapWidth);
            pos = nextSpecial + 1;
        } else {
            pos = nextSpecial;
        }
    }
}

void MessageBoard::RenderText(ImDrawList *drawList, const std::string &text, const ImVec2 &pos, ImU32 color, const ConsoleColor &effects) const {
    if (!m_AnsiEnabled) {
        drawList->AddText(pos, color, text.c_str());
        return;
    }

    // Bold effect (render with slight offset)
    if (effects.bold) {
        drawList->AddText(ImVec2(pos.x + 0.5f, pos.y), color, text.c_str());
        drawList->AddText(ImVec2(pos.x, pos.y + 0.5f), color, text.c_str());
    }

    // Main text
    drawList->AddText(pos, color, text.c_str());

    // Text decorations
    if (effects.underline || effects.strikethrough) {
        ImVec2 textSize = ImGui::CalcTextSize(text.c_str());

        if (effects.underline) {
            drawList->AddLine(
                ImVec2(pos.x, pos.y + textSize.y - 1.0f),
                ImVec2(pos.x + textSize.x, pos.y + textSize.y - 1.0f),
                color, 1.0f
            );
        }

        if (effects.strikethrough) {
            drawList->AddLine(
                ImVec2(pos.x, pos.y + textSize.y * 0.5f),
                ImVec2(pos.x + textSize.x, pos.y + textSize.y * 0.5f),
                color, 1.0f
            );
        }
    }
}

void MessageBoard::HandleSpecialCharacter(char ch, ImVec2 &currentPos, const ImVec2 &startPos, float wrapWidth) {
    const float lineHeight = ImGui::GetTextLineHeightWithSpacing();

    switch (ch) {
    case '\n':
        currentPos.y += lineHeight;
        currentPos.x = startPos.x;
        break;
    case '\r':
        currentPos.x = startPos.x;
        break;
    case '\t': {
        const float charWidth = ImGui::CalcTextSize(" ").x;
        const float tabWidth = charWidth * 8.0f;
        const float currentOffset = currentPos.x - startPos.x;
        const float nextTab = ((int) (currentOffset / tabWidth) + 1) * tabWidth;
        currentPos.x = startPos.x + nextTab;

        if (currentPos.x > startPos.x + wrapWidth) {
            currentPos.y += lineHeight;
            currentPos.x = startPos.x;
        }
        break;
    }
    default:
        break;
    }
}

// =============================================================================
// Scrolling System
// =============================================================================

void MessageBoard::HandleScrolling(float contentHeight, float availableHeight) {
    if (!m_IsCommandBarVisible || m_MaxScrollY <= 0.0f) return;

    const ImGuiIO &io = ImGui::GetIO();

    // Mouse wheel scrolling
    if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f) {
        const float scrollSpeed = ImGui::GetTextLineHeightWithSpacing() * 3.0f;
        m_ScrollY -= io.MouseWheel * scrollSpeed;
        m_ScrollToBottom = false;
    }

    // Keyboard scrolling
    if (ImGui::IsWindowFocused()) {
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
            m_ScrollY -= availableHeight * 0.8f;
            m_ScrollToBottom = false;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
            m_ScrollY += availableHeight * 0.8f;
            m_ScrollToBottom = false;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
            m_ScrollY = 0.0f;
            m_ScrollToBottom = false;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_End)) {
            m_ScrollY = m_MaxScrollY;
            m_ScrollToBottom = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            m_ScrollY -= ImGui::GetTextLineHeightWithSpacing();
            m_ScrollToBottom = false;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            m_ScrollY += ImGui::GetTextLineHeightWithSpacing();
            if (m_ScrollY >= m_MaxScrollY - 0.5f) {
                m_ScrollToBottom = true;
            }
        }
    }

    // Clamp scroll position
    m_ScrollY = std::clamp(m_ScrollY, 0.0f, m_MaxScrollY);

    // Check if at bottom for auto-scroll
    if (m_ScrollY >= m_MaxScrollY - 0.5f) {
        m_ScrollToBottom = true;
    }
}

void MessageBoard::UpdateScrollBounds(float contentHeight, float availableHeight) {
    if (contentHeight > availableHeight) {
        m_MaxScrollY = contentHeight - availableHeight;

        if (m_ScrollToBottom) {
            m_ScrollY = m_MaxScrollY;
        }

        m_ScrollY = std::clamp(m_ScrollY, 0.0f, m_MaxScrollY);
    } else {
        m_MaxScrollY = 0.0f;
        m_ScrollY = 0.0f;
        m_ScrollToBottom = true;
    }
}

void MessageBoard::DrawScrollIndicators(ImDrawList *drawList, const ImVec2 &contentPos, const ImVec2 &contentSize) {
    if (m_MaxScrollY <= 0.0f) return;

    // Scrollbar background
    const ImVec2 scrollbarStart = ImVec2(
        contentPos.x + contentSize.x - m_ScrollbarW - m_ScrollbarPad,
        contentPos.y + m_ScrollbarPad
    );
    const ImVec2 scrollbarEnd = ImVec2(
        contentPos.x + contentSize.x - m_ScrollbarPad,
        contentPos.y + contentSize.y - m_ScrollbarPad
    );

    drawList->AddRectFilled(scrollbarStart, scrollbarEnd, IM_COL32(60, 60, 60, 100));

    // Scrollbar handle
    const float scrollbarHeight = scrollbarEnd.y - scrollbarStart.y;
    const float availableContentHeight = contentSize.y - m_PadY * 2.0f;
    const float contentHeight = m_MaxScrollY + availableContentHeight;
    const float visibleRatio = availableContentHeight / contentHeight;
    const float handleHeight = std::max(ImGui::GetStyle().GrabMinSize, scrollbarHeight * visibleRatio);
    const float handlePos = (m_MaxScrollY > 0.0f ? m_ScrollY / m_MaxScrollY : 0.0f) * (scrollbarHeight - handleHeight);

    const ImVec2 handleStart = ImVec2(scrollbarStart.x + 1.0f, scrollbarStart.y + handlePos);
    const ImVec2 handleEnd = ImVec2(scrollbarEnd.x - 1.0f, handleStart.y + handleHeight);

    drawList->AddRectFilled(handleStart, handleEnd, IM_COL32(150, 150, 150, 200));

    // Scroll position indicator
    if (m_ScrollY > 0.0f || !m_ScrollToBottom) {
        const float scrollPercent = (m_MaxScrollY > 0) ? (m_ScrollY / m_MaxScrollY) * 100.0f : 0.0f;
        char scrollText[32];
        snprintf(scrollText, sizeof(scrollText), "%.0f%%", scrollPercent);

        const ImVec2 textSize = ImGui::CalcTextSize(scrollText);
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
        drawList->AddText(textPos, IM_COL32(255, 255, 255, 200), scrollText);
    }
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
    if (!msg || strlen(msg) == 0) return;

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

    // Update blink time
    m_BlinkTime = static_cast<float>(ImGui::GetTime());

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

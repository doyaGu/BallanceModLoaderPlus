#include "MessageBoard.h"

#include <cmath>
#include <sstream>
#include <cstdarg>
#include <algorithm>
#include <regex>
#include <stack>

#include "imgui_internal.h"

#include "ModContext.h"

namespace {
    int HexVal(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    }

    bool IsValidCodePoint(unsigned int cp) {
        // Valid range per Unicode/UTF-8: <= 0x10FFFF and not a surrogate.
        if (cp > 0x10FFFF) return false;                // RFC 3629 limit
        if (cp >= 0xD800 && cp <= 0xDFFF) return false; // surrogate range
        return true;
    }

    void AppendUTF8(std::string &out, unsigned int cp) {
        if (cp <= 0x7F) {
            out += static_cast<char>(cp);
        } else if (cp <= 0x7FF) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            // cp <= 0x10FFFF
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    // Apply dim effect to color
    ImU32 ApplyDimEffect(ImU32 color) {
        const ImU32 r = ((color >> IM_COL32_R_SHIFT) & 0xFF) / 2;
        const ImU32 g = ((color >> IM_COL32_G_SHIFT) & 0xFF) / 2;
        const ImU32 b = ((color >> IM_COL32_B_SHIFT) & 0xFF) / 2;
        const ImU32 a = (color >> IM_COL32_A_SHIFT) & 0xFF;
        return IM_COL32(r, g, b, a);
    }

    // Apply blink effect (simple alpha modulation)
    ImU32 ApplyBlinkEffect(ImU32 color, float time) {
        const float alpha = (std::sin(time * 4.0f) + 1.0f) * 0.5f; // 0-1 range
        const ImU32 originalAlpha = (color >> IM_COL32_A_SHIFT) & 0xFF;
        const ImU32 newAlpha = static_cast<ImU32>(originalAlpha * alpha);
        return (color & 0x00FFFFFF) | (newAlpha << IM_COL32_A_SHIFT);
    }

    // Helper to calculate wrapped text size consistently
    ImVec2 CalcWrappedTextSize(const std::string &text, float wrapWidth) {
        if (text.empty()) return ImVec2(0, 0);

        if (wrapWidth <= 0) {
            return ImGui::CalcTextSize(text.c_str());
        }

        return ImGui::CalcTextSize(text.c_str(), nullptr, false, wrapWidth);
    }
}

// MessageUnit Implementation
MessageBoard::MessageUnit::MessageUnit(const char *msg, float timer, bool processEscapes)
    : originalText(msg ? msg : ""), timer(timer), escapeProcessed(processEscapes) {
    if (msg && strlen(msg) > 0) {
        ParseAnsiEscapeCodes();
    }
}

void MessageBoard::MessageUnit::SetMessage(const char *msg, bool processEscapes) {
    if (!msg) return;
    originalText = msg;
    escapeProcessed = processEscapes;

    segments.clear();
    hasControlSequences = false;

    ParseAnsiEscapeCodes();

    cachedHeight = -1.0f;
    cachedWrapWidth = -1.0f;
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
                // Skip control segments for height calculation
                if (segment.isClearScreen || segment.isClearLine || segment.text.empty()) {
                    continue;
                }

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
    hasControlSequences = false;
    escapeProcessed = false;
}

std::string MessageBoard::MessageUnit::ProcessEscapeSequences(const char *text) {
    if (!text) return "";

    std::string result;
    result.reserve(std::strlen(text));

    const char *p = text;
    while (*p) {
        if (*p == '\\' && *(p + 1)) {
            ++p; // Skip backslash
            switch (*p) {
                case 'a': result += '\a'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'v': result += '\v'; break;
                case '\\': result += '\\'; break;
                case '\'': result += '\''; break;
                case '"': result += '"'; break;
                case '?': result += '?'; break;
                case 'e': result += '\033'; break; // ESC character

                // Octal escape: up to 3 octal digits (includes \0)
                case '0': case '1': case '2': case '3':
                case '4': case '5': case '6': case '7': {
                    unsigned int value = 0;
                    int digits = 0;
                    while (digits < 3 && *p >= '0' && *p <= '7') {
                        value = (value * 8) + static_cast<unsigned int>(*p - '0');
                        ++p; ++digits;
                    }
                    result += static_cast<char>(value & 0xFF);
                    --p; // will be incremented below
                    break;
                }

                // Hex escape: \xHH (unlimited length as per C standard)
                case 'x': {
                    ++p;
                    unsigned int value = 0;
                    int digits = 0, hv;
                    while ((hv = HexVal(*p)) != -1) {
                        value = (value << 4) + static_cast<unsigned int>(hv);
                        ++p; ++digits;
                    }
                    if (digits > 0) {
                        result += static_cast<char>(value & 0xFF);
                        --p;
                    } else {
                        result += '\\'; result += 'x'; --p;
                    }
                    break;
                }

                // Unicode escapes: \uXXXX (4 hex) and \UXXXXXXXX (8 hex)
                case 'u':
                case 'U': {
                    const char kind = *p;
                    const int need = (kind == 'u') ? 4 : 8;
                    ++p;
                    unsigned int cp = 0;
                    int digits = 0, hv;
                    const char *digits_begin = p;

                    while (digits < need && (hv = HexVal(*p)) != -1) {
                        cp = (cp << 4) + static_cast<unsigned int>(hv);
                        ++p; ++digits;
                    }

                    if (digits == need) {
                        // Handle surrogate pairs for \u
                        if (kind == 'u' && cp >= 0xD800 && cp <= 0xDBFF) {
                            // High surrogate, check for low surrogate
                            const char* save_pos = p;
                            if (*p == '\\' && *(p + 1) == 'u') {
                                p += 2;
                                unsigned int lo = 0;
                                int lo_digits = 0;
                                while (lo_digits < 4 && (hv = HexVal(*p)) != -1) {
                                    lo = (lo << 4) + static_cast<unsigned int>(hv);
                                    ++p; ++lo_digits;
                                }
                                if (lo_digits == 4 && lo >= 0xDC00 && lo <= 0xDFFF) {
                                    // Valid surrogate pair
                                    unsigned int code = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                                    AppendUTF8(result, code);
                                    --p;
                                    break;
                                }
                            }
                            // Not a valid surrogate pair, restore position
                            p = save_pos;
                        }

                        if (IsValidCodePoint(cp)) {
                            AppendUTF8(result, cp);
                            --p;
                        } else {
                            // Invalid code point, emit literally
                            result += '\\'; result += kind;
                            while (digits_begin < p) { result += *digits_begin++; }
                            --p;
                        }
                    } else {
                        // Incomplete sequence, emit literally
                        result += '\\'; result += kind;
                        while (digits_begin < p) { result += *digits_begin++; }
                        --p;
                    }
                    break;
                }

                default:
                    // Unknown escape: keep backslash and char
                    result += '\\';
                    result += *p;
                    break;
            }
            ++p;
        } else {
            result += *p++;
        }
    }

    return result;
}

void MessageBoard::MessageUnit::ParseAnsiEscapeCodes() {
    segments.clear();
    hasControlSequences = false;

    if (originalText.empty()) {
        return;
    }

    std::string processedText;
    if (escapeProcessed) {
        processedText = ProcessEscapeSequences(originalText.c_str());
    } else {
        processedText = originalText;
    }

    const char *p = processedText.c_str();

    ConsoleColor currentColor;
    std::string currentSegment;
    int virtualCursorX = 0;
    int virtualCursorY = 0;
    std::stack<CursorPosition> cursorStack;

    while (*p) {
        if (*p == '\033' && *(p + 1) == '[') {
            // Save current segment
            if (!currentSegment.empty()) {
                segments.emplace_back(std::move(currentSegment), currentColor);
                currentSegment.clear();
            }

            // Parse escape sequence
            p += 2; // Skip \033[
            std::string sequence;

            while (*p && *p != 'm' && *p != 'A' && *p != 'B' && *p != 'C' && *p != 'D' &&
                   *p != 'H' && *p != 'f' && *p != 'J' && *p != 'K' && *p != 's' && *p != 'u' &&
                   sequence.length() < 50) {
                sequence += *p++;
            }

            if (*p) {
                char command = *p++;
                hasControlSequences = true;

                switch (command) {
                    case 'm': // Color and style
                        if (sequence == "0" || sequence.empty()) {
                            currentColor = ConsoleColor(); // Reset to defaults
                        } else {
                            currentColor = ParseAnsiColor(sequence, currentColor);
                        }
                        break;

                    case 'A': case 'B': case 'C': case 'D': // Cursor movement
                    case 'H': case 'f': { // Cursor position
                        int deltaX = 0, deltaY = 0;
                        bool absolute = false;
                        if (ParseCursorMovement(sequence + command, deltaX, deltaY, absolute)) {
                            if (absolute) {
                                virtualCursorX = deltaX;
                                virtualCursorY = deltaY;
                            } else {
                                virtualCursorX += deltaX;
                                virtualCursorY += deltaY;
                            }
                            // Create segment with cursor position info
                            segments.emplace_back("", currentColor, virtualCursorX, virtualCursorY);
                        }
                        break;
                    }

                    case 'J': { // Screen clearing
                        int mode = sequence.empty() ? 0 : std::stoi(sequence);
                        if (mode == 2) { // Clear entire screen
                            segments.push_back(TextSegment::ClearScreen(currentColor));
                            virtualCursorX = 0;
                            virtualCursorY = 0;
                        }
                        break;
                    }

                    case 'K': { // Line clearing
                        int mode = sequence.empty() ? 0 : std::stoi(sequence);
                        segments.push_back(TextSegment::ClearLine(currentColor));
                        if (mode == 0 || mode == 2) { // Clear to end or entire line
                            virtualCursorX = 0;
                        }
                        break;
                    }

                    case 's': // Save cursor position
                        cursorStack.emplace(virtualCursorX, virtualCursorY);
                        break;

                    case 'u': // Restore cursor position
                        if (!cursorStack.empty()) {
                            CursorPosition pos = cursorStack.top();
                            cursorStack.pop();
                            virtualCursorX = pos.x;
                            virtualCursorY = pos.y;
                            segments.emplace_back("", currentColor, virtualCursorX, virtualCursorY);
                        }
                        break;

                    default:
                        // Unknown sequence - treat as literal
                        currentSegment += '\033';
                        currentSegment += '[';
                        currentSegment += sequence;
                        currentSegment += command;
                        break;
                }
            } else {
                // Invalid sequence - treat as literal
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
        segments.emplace_back(std::move(processedText), ConsoleColor());
    }
}

MessageBoard::ConsoleColor MessageBoard::MessageUnit::ParseAnsiColor(const std::string &sequence, ConsoleColor currentColor) {
    ConsoleColor color = currentColor; // Start with current state
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
                        // Remove arbitrary upper limit
                        codes.push_back(code);
                    }
                }
            }
            start = pos + 1;
        }
    }

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
            // Default background color (transparent)
            color.background = IM_COL32(0, 0, 0, 0);
        } else if (code == 1) {
            color.bold = true;
        } else if (code == 2) {
            color.dim = true;
        } else if (code == 3) {
            color.italic = true;
        } else if (code == 4) {
            color.underline = true;
        } else if (code == 5 || code == 6) {
            color.blink = true;
        } else if (code == 7) {
            color.reverse = true;
        } else if (code == 8) {
            color.hidden = true;
        } else if (code == 9) {
            color.strikethrough = true;
        } else if (code == 21 || code == 22) {
            color.bold = false;
            color.dim = false;
        } else if (code == 23) {
            color.italic = false;
        } else if (code == 24) {
            color.underline = false;
        } else if (code == 25) {
            color.blink = false;
        } else if (code == 27) {
            color.reverse = false;
        } else if (code == 28) {
            color.hidden = false;
        } else if (code == 29) {
            color.strikethrough = false;
        }
    }

    return color;
}

bool MessageBoard::MessageUnit::ParseCursorMovement(const std::string &sequence, int &deltaX, int &deltaY, bool &absolute) {
    if (sequence.empty()) return false;

    char command = sequence.back();
    std::string params = sequence.substr(0, sequence.length() - 1);

    deltaX = 0;
    deltaY = 0;
    absolute = false;

    switch (command) {
        case 'A': // Cursor up
            deltaY = params.empty() ? -1 : -std::stoi(params);
            break;
        case 'B': // Cursor down
            deltaY = params.empty() ? 1 : std::stoi(params);
            break;
        case 'C': // Cursor right
            deltaX = params.empty() ? 1 : std::stoi(params);
            break;
        case 'D': // Cursor left
            deltaX = params.empty() ? -1 : -std::stoi(params);
            break;
        case 'H': case 'f': { // Cursor position
            absolute = true;
            size_t semicolon = params.find(';');
            if (semicolon != std::string::npos) {
                deltaY = params.empty() || params.substr(0, semicolon).empty() ? 0 : std::stoi(params.substr(0, semicolon)) - 1;
                deltaX = params.substr(semicolon + 1).empty() ? 0 : std::stoi(params.substr(semicolon + 1)) - 1;
            } else {
                deltaY = params.empty() ? 0 : std::stoi(params) - 1;
                deltaX = 0;
            }
            break;
        }
        default:
            return false;
    }
    return true;
}

bool MessageBoard::MessageUnit::ParseScreenControl(const std::string &sequence, bool &clearScreen, bool &clearLine) {
    if (sequence.empty()) return false;

    char command = sequence.back();
    std::string params = sequence.substr(0, sequence.length() - 1);
    int param = params.empty() ? 0 : std::stoi(params);

    clearScreen = false;
    clearLine = false;

    switch (command) {
        case 'J': // Clear screen
            clearScreen = (param == 2); // Only support full screen clear
            break;
        case 'K': // Clear line
            clearLine = true; // Support line clear
            break;
        default:
            return false;
    }
    return true;
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
        // 6x6x6 color cube with xterm values
        int idx = colorIndex - 16;
        int r6 = (idx / 36) % 6;
        int g6 = ((idx % 36) / 6) % 6;
        int b6 = idx % 6;

        // xterm 256-color cube values
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

// MessageBoard Implementation
MessageBoard::MessageBoard(int size) : Bui::Window("MessageBoard") {
    if (size < 1) size = 500;
    m_Messages.resize(size);
}

MessageBoard::~MessageBoard() = default;

ImGuiWindowFlags MessageBoard::GetFlags() {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;

    // Allow input when command bar is visible for scrolling
    if (!m_IsCommandBarVisible) {
        flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav;
    }

    return flags;
}

void MessageBoard::SetCommandBarVisible(bool visible) {
    if (m_IsCommandBarVisible != visible) {
        m_IsCommandBarVisible = visible;

        if (visible) {
            // When showing command bar, start scrolled to bottom
            m_ScrollToBottom = true;
            Show();
        } else {
            // When hiding command bar, reset all scroll state
            m_ScrollY = 0.0f;
            m_MaxScrollY = 0.0f;
            m_ScrollToBottom = true;
        }
    }
}

bool MessageBoard::ShouldShowMessage(const MessageUnit &msg) const {
    return m_IsCommandBarVisible || msg.GetTimer() > 0;
}

float MessageBoard::GetMessageAlpha(const MessageUnit &msg) const {
    if (m_IsCommandBarVisible) {
        return 0.7f; // Dimmed when command bar is visible
    }

    if (msg.GetTimer() <= 0) {
        return 0.0f; // Hidden if timer expired
    }

    return std::min(128.0f, msg.GetTimer() / 20.0f) / 255.0f;
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

float MessageBoard::CalculateContentHeight(float wrapWidth) const {
    const float messageGap = 4.0f;
    const float verticalPadding = 16.0f;

    float totalHeight = verticalPadding;
    int visibleCount = 0;

    for (int i = 0; i < m_MessageCount; i++) {
        const MessageUnit &msg = m_Messages[i];
        if (ShouldShowMessage(msg)) {
            totalHeight += msg.GetTextHeight(wrapWidth);
            if (visibleCount > 0) {
                totalHeight += messageGap;
            }
            visibleCount++;
        }
    }

    const float minHeight = ImGui::GetTextLineHeightWithSpacing() + verticalPadding;
    if (visibleCount > 0 && totalHeight < minHeight) {
        totalHeight = minHeight;
    }

    return totalHeight;
}

void MessageBoard::OnBegin() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Bui::GetMenuColor());

    const ImVec2 vpSize = ImGui::GetMainViewport()->Size;
    const float windowWidth = vpSize.x * 0.96f;
    const float wrapWidth = windowWidth - 16.0f;

    const float maxHeight = vpSize.y * 0.7f;

    float actualContentHeight = CalculateContentHeight(wrapWidth);
    float contentHeight = std::min(actualContentHeight, maxHeight);

    const float posX = vpSize.x * 0.02f;
    const float posY = vpSize.y * 0.9f - contentHeight;

    ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(windowWidth, contentHeight), ImGuiCond_Always);
}

void MessageBoard::OnDraw() {
    if (!HasVisibleContent()) {
        return;
    }

    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    const ImVec2 contentPos = ImGui::GetCursorScreenPos();
    const ImVec2 contentSize = ImGui::GetContentRegionAvail();
    const float wrapWidth = contentSize.x - 16.0f;
    bool needsScrollbar = false;

    // Handle scrolling when command bar is visible
    if (m_IsCommandBarVisible) {
        const float totalContentHeight = CalculateTotalContentHeight(wrapWidth);
        const float windowHeight = contentSize.y - 16.0f; // Account for padding

        needsScrollbar = totalContentHeight > windowHeight;

        if (needsScrollbar) {
            UpdateScrollBounds(totalContentHeight, windowHeight);
            HandleScrolling(totalContentHeight, windowHeight);
        } else {
            // No scrolling needed, reset scroll state
            m_ScrollY = 0.0f;
            m_MaxScrollY = 0.0f;
            m_ScrollToBottom = true;
        }
    } else {
        // Reset scroll when command bar is hidden
        m_ScrollY = 0.0f;
        m_MaxScrollY = 0.0f;
        m_ScrollToBottom = true;
    }

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImVec2 startPos = ImVec2(contentPos.x + 8.0f, contentPos.y + 8.0f - m_ScrollY);

    // Set up clipping for scrollable content
    if (m_IsCommandBarVisible) {
        drawList->PushClipRect(
            contentPos,
            ImVec2(contentPos.x + contentSize.x, contentPos.y + contentSize.y),
            true
        );
    }

    RenderMessages(drawList, startPos, wrapWidth);

    if (m_IsCommandBarVisible) {
        drawList->PopClipRect();

        // Only draw scroll indicators when scrolling is actually needed
        if (needsScrollbar && m_MaxScrollY > 0.0f) {
            DrawScrollIndicators(drawList, contentPos, contentSize);
        }
    }
}

bool MessageBoard::HasVisibleContent() const {
    if (m_IsCommandBarVisible) {
        return m_MessageCount > 0;
    }
    return m_DisplayMessageCount > 0;
}

void MessageBoard::RenderMessages(ImDrawList *drawList, ImVec2 startPos, float wrapWidth) {
    const float messageGap = 4.0f;
    ImVec4 bgColor = Bui::GetMenuColor();
    ImVec2 currentPos = startPos;

    for (int i = m_MessageCount - 1; i >= 0; i--) {
        const MessageUnit &msg = m_Messages[i];

        // Show all messages when command bar is visible, otherwise only visible ones
        bool shouldShow = m_IsCommandBarVisible || ShouldShowMessage(msg);

        if (shouldShow) {
            float alpha = GetMessageAlpha(msg);

            const float msgHeight = msg.GetTextHeight(wrapWidth);

            drawList->AddRectFilled(
                ImVec2(currentPos.x - 4.0f, currentPos.y - 2.0f),
                ImVec2(currentPos.x + wrapWidth + 4.0f, currentPos.y + msgHeight + 2.0f),
                ImGui::GetColorU32(ImVec4(bgColor.x, bgColor.y, bgColor.z, bgColor.w * alpha))
            );

            DrawMessageText(drawList, msg, currentPos, wrapWidth, alpha);

            currentPos.y += msgHeight + messageGap;
        }
    }
}

void MessageBoard::DrawScrollIndicators(ImDrawList *drawList, const ImVec2 &contentPos, const ImVec2 &contentSize) {
    if (m_MaxScrollY <= 0.0f) return; // Don't draw if no scrolling is needed

    const float scrollbarWidth = 8.0f;
    const float scrollbarPadding = 2.0f;

    // Scrollbar background
    const ImVec2 scrollbarStart = ImVec2(
        contentPos.x + contentSize.x - scrollbarWidth - scrollbarPadding,
        contentPos.y + scrollbarPadding
    );
    const ImVec2 scrollbarEnd = ImVec2(
        contentPos.x + contentSize.x - scrollbarPadding,
        contentPos.y + contentSize.y - scrollbarPadding
    );

    drawList->AddRectFilled(
        scrollbarStart,
        scrollbarEnd,
        IM_COL32(60, 60, 60, 100) // Semi-transparent dark background
    );

    // Scrollbar handle
    const float scrollbarHeight = scrollbarEnd.y - scrollbarStart.y;
    const float totalContentHeight = m_MaxScrollY + (contentSize.y - 16.0f); // Total height including visible area
    const float visibleRatio = (contentSize.y - 16.0f) / totalContentHeight;
    const float handleHeight = std::max(20.0f, scrollbarHeight * visibleRatio);
    const float handlePos = (m_ScrollY / m_MaxScrollY) * (scrollbarHeight - handleHeight);

    const ImVec2 handleStart = ImVec2(
        scrollbarStart.x + 1.0f,
        scrollbarStart.y + handlePos
    );
    const ImVec2 handleEnd = ImVec2(
        scrollbarEnd.x - 1.0f,
        handleStart.y + handleHeight
    );

    drawList->AddRectFilled(
        handleStart,
        handleEnd,
        IM_COL32(150, 150, 150, 200) // Semi-transparent light handle
    );

    // Show scroll position indicator only if we're not at the top
    if (m_ScrollY > 0.0f || !m_ScrollToBottom) {
        const float scrollPercent = (m_MaxScrollY > 0) ? (m_ScrollY / m_MaxScrollY) * 100.0f : 0.0f;
        char scrollText[32];
        snprintf(scrollText, sizeof(scrollText), "%.0f%%", scrollPercent);

        const ImVec2 textSize = ImGui::CalcTextSize(scrollText);
        const ImVec2 textPos = ImVec2(
            contentPos.x + contentSize.x - textSize.x - scrollbarWidth - scrollbarPadding - 8.0f,
            contentPos.y + 4.0f
        );

        // Text background
        drawList->AddRectFilled(
            ImVec2(textPos.x - 2.0f, textPos.y - 2.0f),
            ImVec2(textPos.x + textSize.x + 2.0f, textPos.y + textSize.y + 2.0f),
            IM_COL32(0, 0, 0, 150)
        );

        // Text
        drawList->AddText(textPos, IM_COL32(255, 255, 255, 200), scrollText);
    }
}

void MessageBoard::DrawMessageText(ImDrawList *drawList, const MessageUnit &message, const ImVec2 &startPos, float wrapWidth, float alpha) {
    ImVec2 currentPos = startPos;
    const ImVec2 contentSize = ImGui::GetContentRegionAvail();

    for (const auto &segment : message.segments) {
        // Handle cursor positioning if specified
        if (segment.cursorX >= 0 || segment.cursorY >= 0) {
            if (segment.cursorX >= 0) {
                currentPos.x = startPos.x + segment.cursorX * ImGui::CalcTextSize(" ").x;
            }
            if (segment.cursorY >= 0) {
                currentPos.y = startPos.y + segment.cursorY * ImGui::GetTextLineHeightWithSpacing();
            }
        }

        DrawTextSegment(drawList, segment, currentPos, wrapWidth, alpha, startPos, contentSize);
    }
}

void MessageBoard::DrawTextSegment(ImDrawList *drawList, const TextSegment &segment, ImVec2 &currentPos, float wrapWidth, float alpha, const ImVec2 &contentStart, const ImVec2 &contentSize) {
    // Handle control segments
    if (segment.isClearScreen) {
        // Clear the entire content area
        drawList->AddRectFilled(
            contentStart,
            ImVec2(contentStart.x + contentSize.x, contentStart.y + contentSize.y),
            IM_COL32(0, 0, 0, static_cast<int>(alpha * 255))
        );
        currentPos = contentStart;
        return;
    }

    if (segment.isClearLine) {
        // Clear from current position to end of line
        const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
        drawList->AddRectFilled(
            ImVec2(currentPos.x, currentPos.y),
            ImVec2(contentStart.x + wrapWidth, currentPos.y + lineHeight),
            IM_COL32(0, 0, 0, static_cast<int>(alpha * 255))
        );
        return;
    }

    if (segment.text.empty()) return;

    const ImVec2 startPos = contentStart;
    const float lineHeight = ImGui::GetTextLineHeightWithSpacing();

    // Get rendered color (with reverse effect applied)
    ConsoleColor renderColor = m_AnsiEnabled
                                   ? segment.color.GetRendered()
                                   : ConsoleColor(segment.color.foreground, IM_COL32(0, 0, 0, 0));

    // Apply alpha while preserving original alpha information
    const auto applyAlpha = [alpha](ImU32 color) {
        const ImU32 originalAlpha = (color >> IM_COL32_A_SHIFT) & 0xFF;
        if (originalAlpha == 0) {
            return ImU32(0);
        }
        const ImU32 newAlpha = static_cast<ImU32>(static_cast<float>(originalAlpha) * alpha);
        return (color & 0x00FFFFFF) | (newAlpha << IM_COL32_A_SHIFT);
    };

    ImU32 fgColor = renderColor.foreground;
    ImU32 bgColor = applyAlpha(renderColor.background);

    // Apply effects to foreground color only if ANSI is enabled
    if (m_AnsiEnabled) {
        if (renderColor.dim) {
            fgColor = ApplyDimEffect(fgColor);
        }
        if (renderColor.blink) {
            fgColor = ApplyBlinkEffect(fgColor, m_BlinkTime);
        }
        if (renderColor.hidden) {
            fgColor = bgColor; // Hidden text same color as background
        }
    }

    // Process text with proper handling of special characters
    size_t pos = 0;
    while (pos < segment.text.length()) {
        size_t nextSpecial = pos;
        while (nextSpecial < segment.text.length() &&
            segment.text[nextSpecial] != '\n' &&
            segment.text[nextSpecial] != '\r' &&
            segment.text[nextSpecial] != '\t') {
            nextSpecial++;
        }

        // Render regular text
        if (nextSpecial > pos) {
            std::string textChunk = segment.text.substr(pos, nextSpecial - pos);
            float remainingWidth = std::max(0.0f, wrapWidth - (currentPos.x - startPos.x));

            // Use consistent text size calculation
            ImVec2 textSize = CalcWrappedTextSize(textChunk, remainingWidth);

            if (currentPos.x + textSize.x > startPos.x + wrapWidth && currentPos.x > startPos.x) {
                currentPos.y += lineHeight;
                currentPos.x = startPos.x;
                remainingWidth = wrapWidth;
                textSize = CalcWrappedTextSize(textChunk, remainingWidth);
            }

            // Draw background if it has alpha > 0
            if ((bgColor >> IM_COL32_A_SHIFT) > 0) {
                drawList->AddRectFilled(
                    ImVec2(currentPos.x - 1.0f, currentPos.y - 1.0f),
                    ImVec2(currentPos.x + textSize.x + 1.0f, currentPos.y + textSize.y + 1.0f),
                    bgColor
                );
            }

            // Draw text with effects
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

    // Bold effect (render slightly offset)
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
            const float nextTab = ((int)(currentOffset / tabWidth) + 1) * tabWidth;
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

void MessageBoard::AddMessageInternal(const char *msg, bool processEscapes) {
    if (!msg || strlen(msg) == 0) return;

    if (m_MessageCount == static_cast<int>(m_Messages.size()) &&
        m_Messages[m_MessageCount - 1].GetTimer() > 0) {
        --m_DisplayMessageCount;
    }

    const int shiftCount = std::min(m_MessageCount, static_cast<int>(m_Messages.size()) - 1);
    for (int i = shiftCount - 1; i >= 0; i--) {
        m_Messages[i + 1] = std::move(m_Messages[i]);
    }

    // Only process escapes if both requested and ANSI is enabled
    bool shouldProcessEscapes = processEscapes && m_AnsiEnabled;
    m_Messages[0] = MessageUnit(msg, m_MaxTimer, shouldProcessEscapes);

    if (m_MessageCount < static_cast<int>(m_Messages.size())) {
        ++m_MessageCount;
    }
    ++m_DisplayMessageCount;

    // Auto-scroll to bottom for new messages only if we were already at bottom
    // or if scrolling isn't active
    if (m_IsCommandBarVisible && (m_ScrollToBottom || m_MaxScrollY <= 0.0f)) {
        m_ScrollToBottom = true;
    }
}

void MessageBoard::HandleScrolling(float contentHeight, float windowHeight) {
    if (!m_IsCommandBarVisible || m_MaxScrollY <= 0.0f) return;

    const ImGuiIO &io = ImGui::GetIO();

    // Handle mouse wheel scrolling
    if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f) {
        const float scrollSpeed = ImGui::GetTextLineHeightWithSpacing() * 3.0f;
        m_ScrollY -= io.MouseWheel * scrollSpeed;
        m_ScrollToBottom = false; // Disable auto-scroll when user manually scrolls
    }

    // Handle keyboard scrolling
    if (ImGui::IsWindowFocused()) {
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
            m_ScrollY -= windowHeight * 0.8f;
            m_ScrollToBottom = false;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
            m_ScrollY += windowHeight * 0.8f;
            m_ScrollToBottom = false;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) {
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
            if (m_ScrollY >= m_MaxScrollY) {
                m_ScrollToBottom = true;
            }
        }
    }

    // Clamp scroll position
    m_ScrollY = std::clamp(m_ScrollY, 0.0f, m_MaxScrollY);

    // Check if we're at the bottom for auto-scroll
    if (m_ScrollY >= m_MaxScrollY - 1.0f) {
        m_ScrollToBottom = true;
    }
}

void MessageBoard::UpdateScrollBounds(float contentHeight, float windowHeight) {
    // Only set scroll bounds if content actually overflows
    if (contentHeight > windowHeight) {
        m_MaxScrollY = contentHeight - windowHeight;

        // Auto-scroll to bottom if enabled
        if (m_ScrollToBottom) {
            m_ScrollY = m_MaxScrollY;
        }

        // Ensure scroll position is valid
        m_ScrollY = std::clamp(m_ScrollY, 0.0f, m_MaxScrollY);
    } else {
        // Content fits in window, no scrolling needed
        m_MaxScrollY = 0.0f;
        m_ScrollY = 0.0f;
        m_ScrollToBottom = true;
    }
}

float MessageBoard::CalculateTotalContentHeight(float wrapWidth) const {
    const float messageGap = 4.0f;
    const float verticalPadding = 16.0f;

    float totalHeight = verticalPadding;
    int processedCount = 0;

    // Calculate height for all messages
    for (int i = 0; i < m_MessageCount; i++) {
        const MessageUnit &msg = m_Messages[i];
        if (m_IsCommandBarVisible || ShouldShowMessage(msg)) {
            totalHeight += msg.GetTextHeight(wrapWidth);
            if (processedCount > 0) {
                totalHeight += messageGap;
            }
            processedCount++;
        }
    }

    return totalHeight;
}

void MessageBoard::OnAfterEnd() {
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    CKStats stats;
    BML_GetCKContext()->GetProfileStats(&stats);
    UpdateTimers(stats.TotalFrameTime);

    // Update global blink time for consistent blinking
    m_BlinkTime = static_cast<float>(ImGui::GetTime());

    if (!m_IsCommandBarVisible && m_DisplayMessageCount == 0) {
        Hide();
    }
}

// Public interface
void MessageBoard::AddMessage(const char *msg, bool processEscapes) {
    AddMessageInternal(msg, processEscapes);
}

void MessageBoard::Printf(const char *format, ...) {
    if (!format) return;

    char buffer[4096];
    va_list args;
    va_start(args, format);
    const int result = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (result > 0) {
        AddMessage(buffer); // Plain text
    }
}

void MessageBoard::PrintfColored(ImU32 color, const char *format, ...) {
    if (!format) return;

    char buffer[4096];
    va_list args;
    va_start(args, format);
    const int result = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (result > 0) {
        const ImU32 r = (color >> IM_COL32_R_SHIFT) & 0xFF;
        const ImU32 g = (color >> IM_COL32_G_SHIFT) & 0xFF;
        const ImU32 b = (color >> IM_COL32_B_SHIFT) & 0xFF;

        char coloredBuffer[4200];
        snprintf(coloredBuffer, sizeof(coloredBuffer),
                 "\033[38;2;%u;%u;%um%s\033[0m", r, g, b, buffer);
        AddMessage(coloredBuffer); // Use ANSI message for colored output
    }
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

// Static utilities
std::string MessageBoard::ProcessEscapeSequences(const char *text) {
    return MessageUnit::ProcessEscapeSequences(text);
}

std::string MessageBoard::StripAnsiCodes(const char *text) {
    if (!text) return "";

    std::string result;
    result.reserve(strlen(text));
    const char *p = text;

    while (*p) {
        if (*p == '\033' && *(p + 1) == '[') {
            p += 2;
            while (*p && *p != 'm' && *p != 'A' && *p != 'B' && *p != 'C' && *p != 'D' &&
                   *p != 'H' && *p != 'f' && *p != 'J' && *p != 'K' && *p != 's' && *p != 'u') {
                ++p;
            }
            if (*p) ++p; // Skip the final command character
        } else {
            result += *p++;
        }
    }

    return result;
}
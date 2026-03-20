#ifndef BML_CONSOLE_MESSAGE_BOARD_H
#define BML_CONSOLE_MESSAGE_BOARD_H

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "bml_imgui.hpp"
#include "bml_console.h"
#include "AnsiPalette.h"
#include "AnsiText.h"

namespace BML::Console {

constexpr size_t kMaxMessages = 200;
constexpr float kDefaultMessageLifetime = 6.0f;
constexpr float kPinnedMessageLifetime = 3600.0f;

struct ConsoleSettings {
    float messageDuration = kDefaultMessageLifetime;
    int tabColumns = 4;
    float lineSpacing = -1.0f;
    float messageBackgroundAlpha = 0.80f;
    float windowBackgroundAlpha = 1.0f;
    float fadeMaxAlpha = 1.0f;
};

struct ConsoleMessage {
    AnsiText::AnsiString text;
    float ttl = kDefaultMessageLifetime;
    uint32_t flags = BML_CONSOLE_OUTPUT_FLAG_NONE;
    mutable float cachedHeight = -1.0f;
    mutable float cachedWrapWidth = -1.0f;
    mutable float cachedLineSpacing = -1.0f;
    mutable float cachedFontPixels = -1.0f;

    float GetTextHeight(float wrapWidth, float lineSpacing, int tabColumns, const AnsiPalette *palette) const {
        const float fontPixelSize = ImGui::GetFontSize();
        if (cachedHeight >= 0.0f &&
            std::abs(cachedWrapWidth - wrapWidth) < 0.5f &&
            std::abs(cachedLineSpacing - lineSpacing) < 0.5f &&
            std::fabs(cachedFontPixels - fontPixelSize) < 1e-3f) {
            return cachedHeight;
        }

        AnsiText::TextOptions options;
        options.font = ImGui::GetFont();
        options.wrapWidth = wrapWidth;
        options.lineSpacing = lineSpacing;
        options.tabColumns = tabColumns;
        options.palette = palette;
        cachedHeight = AnsiText::CalcTextHeight(text, options);
        cachedWrapWidth = wrapWidth;
        cachedLineSpacing = lineSpacing;
        cachedFontPixels = fontPixelSize;
        return cachedHeight;
    }
};

class MessageBoard {
public:
    void Add(std::string text, uint32_t flags, float ttl, bool visible, float defaultDuration) {
        if (text.empty()) text = "\n";
        if (ttl < 0.0f) ttl = visible ? kPinnedMessageLifetime : defaultDuration;
        if (m_Messages.size() >= kMaxMessages) {
            m_Messages.erase(m_Messages.begin());
        }
        ConsoleMessage message;
        message.text.SetText(std::move(text));
        message.ttl = ttl;
        message.flags = flags;
        m_Messages.push_back(std::move(message));
    }

    void Clear() {
        m_Messages.clear();
    }

    ~MessageBoard() { Clear(); }

    void Tick(float dt, bool visible) {
        if (visible) return;
        for (auto &message : m_Messages) {
            if (message.ttl > 0.0f) message.ttl = (std::max)(0.0f, message.ttl - dt);
        }
        PruneExpired();
    }

    void PruneExpired() {
        for (auto it = m_Messages.begin(); it != m_Messages.end();) {
            if (it->ttl <= 0.0f) {
                it = m_Messages.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool HasVisible(bool consoleVisible) const {
        if (consoleVisible) return !m_Messages.empty();
        return std::any_of(m_Messages.begin(), m_Messages.end(), [](const ConsoleMessage &m) {
            return m.ttl > 0.0f;
        });
    }

    float CalculateContentHeight(float wrapWidth, float lineSpacing, int tabColumns,
        const AnsiPalette *palette, bool consoleVisible) const {
        float contentHeight = 0.0f;
        int visibleCount = 0;
        for (const ConsoleMessage &message : m_Messages) {
            if (!consoleVisible && message.ttl <= 0.0f) continue;
            contentHeight += message.GetTextHeight(wrapWidth, lineSpacing, tabColumns, palette);
            ++visibleCount;
        }
        if (visibleCount > 1) {
            contentHeight += lineSpacing * static_cast<float>(visibleCount - 1);
        }
        return (std::max)(contentHeight, ImGui::GetTextLineHeightWithSpacing());
    }

    const std::vector<ConsoleMessage> &Messages() const { return m_Messages; }
    std::vector<ConsoleMessage> &Messages() { return m_Messages; }
    bool Empty() const { return m_Messages.empty(); }

private:
    std::vector<ConsoleMessage> m_Messages;
};

} // namespace BML::Console

#endif // BML_CONSOLE_MESSAGE_BOARD_H

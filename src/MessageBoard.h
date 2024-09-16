#ifndef BML_MESSAGEBOARD_H
#define BML_MESSAGEBOARD_H

#include <vector>

#include "BML/Bui.h"

class MessageBoard : public Bui::Window {
public:
    struct MessageUnit {
        std::string text;
        float timer = 0.0f;

        MessageUnit() = default;
        MessageUnit(const char *msg, const float timer) : text(msg), timer(timer) {}
        MessageUnit(const MessageUnit &other) = default;
        MessageUnit(MessageUnit &&other) noexcept : text(std::move(other.text)), timer(other.timer) {}

        MessageUnit &operator=(const MessageUnit &other) {
            if (this == &other)
                return *this;
            text = other.text;
            timer = other.timer;
            return *this;
        }

        MessageUnit &operator=(MessageUnit &&other) noexcept {
            if (this == &other)
                return *this;
            text = std::move(other.text);
            timer = other.timer;
            return *this;
        }

        const char *GetMessage() const {
            return text.c_str();
        }

        void SetMessage(const char *msg) {
            if (msg)
                text = msg;
        }

        float GetTimer() const {
            return timer;
        }

        void SetTimer(const float t) {
            timer = t;
        }
    };

    explicit MessageBoard(int size = 35);
    ~MessageBoard() override;

    ImGuiWindowFlags GetFlags() override;

    void OnBegin() override;
    void OnDraw() override;
    void OnAfterEnd() override;

    void AddMessage(const char *msg);
    void ClearMessages();

    float GetMaxTimer() const {
        return m_MaxTimer;
    }

    void SetMaxTimer(float maxTimer) {
        m_MaxTimer = maxTimer;
    }

    void SetCommandBarVisible(bool visible) {
        m_IsCommandBarVisible = visible;
    }

private:
    bool m_IsCommandBarVisible = false;
    int m_MessageCount = 0;
    int m_DisplayMessageCount = 0;
    std::vector<MessageUnit> m_Messages;
    float m_MaxTimer = 6000; // ms
};

#endif // BML_MESSAGEBOARD_H

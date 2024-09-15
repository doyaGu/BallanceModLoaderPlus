#ifndef BML_MESSAGEBOARD_H
#define BML_MESSAGEBOARD_H

#include "BML/Bui.h"

#define MSG_MAXSIZE 35

class MessageBoard : public Bui::Window {
public:
    MessageBoard();
    ~MessageBoard() override;

    ImGuiWindowFlags GetFlags() override;

    void OnBegin() override;
    void OnDraw() override;
    void OnAfterEnd() override;

    void AddMessage(const char *msg);
    void ClearMessages();

    float GetMaxTimer() const {
        return m_MsgMaxTimer;
    }

    void SetMaxTimer(float maxTimer) {
        m_MsgMaxTimer = maxTimer;
    }

    void SetCommandBarVisible(bool visible) {
        m_IsCommandBarVisible = visible;
    }

private:
    ImVec2 m_WindowPos;
    ImVec2 m_WindowSize;
    bool m_IsCommandBarVisible = false;
    int m_MsgCount = 0;
    int m_DisplayMsgCount = 0;
    struct {
        char Text[256] = {};
        float Timer = 0.0f;
    } m_Msgs[MSG_MAXSIZE] = {};
    float m_MsgMaxTimer = 6000; // ms
};

#endif // BML_MESSAGEBOARD_H

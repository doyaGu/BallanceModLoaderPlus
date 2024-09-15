#include "MessageBoard.h"

#include "imgui_internal.h"

#include "ModManager.h"

MessageBoard::MessageBoard() : Bui::Window("MessageBoard") {
    SetVisibility(false);
}

MessageBoard::~MessageBoard() = default;

ImGuiWindowFlags MessageBoard::GetFlags() {
    return ImGuiWindowFlags_NoDecoration |
           ImGuiWindowFlags_NoInputs |
           ImGuiWindowFlags_NoMove |
           ImGuiWindowFlags_NoResize |
           ImGuiWindowFlags_NoCollapse |
           ImGuiWindowFlags_NoBackground |
           ImGuiWindowFlags_NoNav |
           ImGuiWindowFlags_NoFocusOnAppearing |
           ImGuiWindowFlags_NoBringToFrontOnFocus;
}

void MessageBoard::OnBegin() {
    const int count = std::min(MSG_MAXSIZE, m_MsgCount);
    const float sy = static_cast<float>(count) * ImGui::GetTextLineHeightWithSpacing();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Bui::GetMenuColor());

    const ImVec2 vpSize = ImGui::GetMainViewport()->Size;
    m_WindowPos = ImVec2(vpSize.x * 0.02f, vpSize.y * 0.9f - sy);
    m_WindowSize = ImVec2(vpSize.x * 0.96f, sy);

    ImGui::SetNextWindowPos(m_WindowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(m_WindowSize, ImGuiCond_Always);
}

void MessageBoard::OnDraw() {
    const int count = std::min(MSG_MAXSIZE, m_MsgCount);
    const float ly = ImGui::GetTextLineHeightWithSpacing();
    ImVec4 bgColorVec4 = Bui::GetMenuColor();

    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    const ImVec2 contentSize = ImGui::GetContentRegionMax();

    ImVec2 msgPos = cursorPos;
    ImDrawList *drawList = ImGui::GetWindowDrawList();

    if (m_IsCommandBarVisible) {
        for (int i = 0; i < count; i++) {
            msgPos.y = cursorPos.y + static_cast<float>(count - i) * ly;

            drawList->AddRectFilled(msgPos, ImVec2(msgPos.x + contentSize.x, msgPos.y + ly), ImGui::GetColorU32(bgColorVec4));
            drawList->AddText(msgPos, IM_COL32_WHITE, m_Msgs[i].Text);
        }
    } else {
        for (int i = 0; i < count; i++) {
            const float timer = m_Msgs[i].Timer;
            if (timer > 0) {
                msgPos.y = cursorPos.y + static_cast<float>(count - i) * ly;
                bgColorVec4.w = std::min(110.0f, (timer / 20.0f)) / 255.0f;

                drawList->AddRectFilled(msgPos, ImVec2(msgPos.x + contentSize.x, msgPos.y + ly), ImGui::GetColorU32(bgColorVec4));
                drawList->AddText(msgPos, IM_COL32_WHITE, m_Msgs[i].Text);
            }
        }
    }
}

void MessageBoard::OnAfterEnd() {
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    CKStats stats;
    BML_GetCKContext()->GetProfileStats(&stats);

    const int count = std::min(MSG_MAXSIZE, m_MsgCount);
    for (int i = 0; i < count; i++) {
        m_Msgs[i].Timer -= stats.TotalFrameTime;
        if (m_Msgs[i].Timer == 0)
            --m_DisplayMsgCount;
    }

    if (m_DisplayMsgCount == 0)
        Hide();
}

void MessageBoard::AddMessage(const char *msg) {
    for (int i = std::min(MSG_MAXSIZE - 1, m_MsgCount) - 1; i >= 0; i--) {
        const char *text = m_Msgs[i].Text;
        strncpy(m_Msgs[i + 1].Text, text, 256);
        m_Msgs[i + 1].Timer = m_Msgs[i].Timer;
    }

    strncpy(m_Msgs[0].Text, msg, 256);
    m_Msgs[0].Timer = m_MsgMaxTimer;
    ++m_MsgCount;
    ++m_DisplayMsgCount;
}

void MessageBoard::ClearMessages() {
    m_MsgCount = 0;
    m_DisplayMsgCount = 0;
    for (auto &m_Msg : m_Msgs) {
        m_Msg.Text[0] = '\0';
        m_Msg.Timer = 0.0f;
    }
}



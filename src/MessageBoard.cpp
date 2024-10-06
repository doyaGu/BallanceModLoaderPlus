#include "MessageBoard.h"

#include "imgui_internal.h"

#include "ModManager.h"

MessageBoard::MessageBoard(const int size) : Bui::Window("MessageBoard"), m_Messages(size) {
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

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Bui::GetMenuColor());

    const ImVec2 vpSize = ImGui::GetMainViewport()->Size;

    const float sy = static_cast<float>(m_MessageCount) * ImGui::GetTextLineHeightWithSpacing();
    ImGui::SetNextWindowPos(ImVec2(vpSize.x * 0.02f, vpSize.y * 0.9f - sy), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(vpSize.x * 0.96f, sy), ImGuiCond_Always);
}

void MessageBoard::OnDraw() {
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    const float ly = ImGui::GetTextLineHeightWithSpacing();
    const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    const ImVec2 contentSize = ImGui::GetContentRegionAvail();

    ImVec2 msgPos = cursorPos;
    ImVec4 bgColorVec4 = Bui::GetMenuColor();
    ImDrawList *drawList = ImGui::GetWindowDrawList();

    if (m_IsCommandBarVisible) {
        for (int i = 0; i < m_MessageCount; i++) {
            msgPos.y = cursorPos.y + static_cast<float>(m_MessageCount - i) * ly;

            drawList->AddRectFilled(msgPos, ImVec2(msgPos.x + contentSize.x, msgPos.y + ly), ImGui::GetColorU32(bgColorVec4));
            drawList->AddText(msgPos, IM_COL32_WHITE, m_Messages[i].GetMessage());
        }
    } else {
        for (int i = 0; i < m_MessageCount; i++) {
            const float timer = m_Messages[i].GetTimer();
            if (timer > 0) {
                msgPos.y = cursorPos.y + static_cast<float>(m_MessageCount - i) * ly;
                bgColorVec4.w = std::min(110.0f, timer / 20.0f) / 255.0f;

                drawList->AddRectFilled(msgPos, ImVec2(msgPos.x + contentSize.x, msgPos.y + ly), ImGui::GetColorU32(bgColorVec4));
                drawList->AddText(msgPos, IM_COL32_WHITE, m_Messages[i].GetMessage());
            }
        }
    }
}

void MessageBoard::OnAfterEnd() {
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    CKStats stats;
    BML_GetCKContext()->GetProfileStats(&stats);

    for (int i = 0; i < m_MessageCount; i++) {
        m_Messages[i].timer -= stats.TotalFrameTime;
        if (m_Messages[i].timer == 0)
            --m_DisplayMessageCount;
    }

    if (m_DisplayMessageCount == 0)
        Hide();
}

void MessageBoard::AddMessage(const char *msg) {
    if (m_MessageCount == m_Messages.size() && m_Messages[m_MessageCount - 1].timer > 0) {
        --m_DisplayMessageCount;
    }

    for (int i = std::min(m_MessageCount, static_cast<int>(m_Messages.size()) - 1) - 1; i >= 0; i--)
        m_Messages[i + 1] = std::move(m_Messages[i]);
    m_Messages[0] = std::move(MessageUnit(msg, m_MaxTimer));

    if (m_MessageCount < static_cast<int>(m_Messages.size())) {
        ++m_MessageCount;
        ++m_DisplayMessageCount;
    }
}

void MessageBoard::ClearMessages() {
    m_MessageCount = 0;
    m_DisplayMessageCount = 0;
    for (auto &message : m_Messages) {
        message.text.clear();
        message.timer = 0.0f;
    }
}

void MessageBoard::ResizeMessages(int size) {
    if (size < 1)
        return;

    m_Messages.resize(size);
    m_MessageCount = std::min(m_MessageCount, size);
    m_DisplayMessageCount = std::min(m_DisplayMessageCount, size);
}



#include "HUD.h"

#include "ModContext.h"

HUD::HUD() : Bui::Window("HUD") {
    SetVisibility(true);
}

HUD::~HUD() = default;

ImGuiWindowFlags HUD::GetFlags() {
    return ImGuiWindowFlags_NoDecoration |
           ImGuiWindowFlags_NoBackground |
           ImGuiWindowFlags_NoResize |
           ImGuiWindowFlags_NoMove |
           ImGuiWindowFlags_NoInputs |
           ImGuiWindowFlags_NoBringToFrontOnFocus |
           ImGuiWindowFlags_NoSavedSettings;
}

void HUD::OnBegin() {
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
}

void HUD::OnDraw() {
    const ImVec2 contentSize = ImGui::GetContentRegionAvail();
    ImDrawList *drawList = ImGui::GetWindowDrawList();

    float oldScale = ImGui::GetFont()->Scale;
    ImGui::GetFont()->Scale *= 1.2f;
    ImGui::PushFont(ImGui::GetFont());

    if (m_ShowTitle) {
        constexpr auto TitleText = "BML Plus " BML_VERSION;
        const auto titleSize = ImGui::CalcTextSize(TitleText);
        drawList->AddText(ImVec2((contentSize.x - titleSize.x) / 2.0f, 0), IM_COL32_WHITE, TitleText);
    }

    if (m_ShowFPS) {
        drawList->AddText(ImVec2(0, 0), IM_COL32_WHITE, m_FPSText);
    }

    if (BML_GetModContext()->IsCheatEnabled()) {
        constexpr auto CheatText = "Cheat Mode Enabled";
        const auto cheatSize = ImGui::CalcTextSize(CheatText);
        drawList->AddText(ImVec2((contentSize.x - cheatSize.x) / 2.0f, contentSize.y * 0.85f), IM_COL32_WHITE, CheatText);
    }

    if (m_ShowSRTimer) {
        drawList->AddText(ImVec2(contentSize.x * 0.03f, contentSize.y * 0.8f), IM_COL32_WHITE, "SR Timer");
        auto srSize = ImGui::CalcTextSize(m_SRScore);
        drawList->AddText(ImVec2(contentSize.x * 0.05f, contentSize.y * 0.8f + srSize.y), IM_COL32_WHITE, m_SRScore);
    }

    ImGui::GetFont()->Scale = oldScale;
    ImGui::PopFont();
}

void HUD::OnProcess() {
    OnProcess_Fps();
    OnProcess_SRTimer();
}

void HUD::OnProcess_Fps() {
    CKStats stats;
    BML_GetCKContext()->GetProfileStats(&stats);
    m_FPSCount += int(1000 / stats.TotalFrameTime);
    if (++m_FPSTimer == 60) {
        sprintf(m_FPSText, "FPS: %d", m_FPSCount / 60);
        m_FPSTimer = 0;
        m_FPSCount = 0;
    }
}

void HUD::OnProcess_SRTimer() {
    if (m_SRActivated) {
        m_SRTimer += BML_GetCKContext()->GetTimeManager()->GetLastDeltaTime();
        int counter = int(m_SRTimer);
        int ms = counter % 1000;
        counter /= 1000;
        int s = counter % 60;
        counter /= 60;
        int m = counter % 60;
        counter /= 60;
        int h = counter % 100;
        sprintf(m_SRScore, "%02d:%02d:%02d.%03d", h, m, s, ms);
    }
}
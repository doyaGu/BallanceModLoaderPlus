#ifndef BML_HUD_H
#define BML_HUD_H

#include "BML/Bui.h"

class HUD : public Bui::Window {
public:
    HUD();
    ~HUD() override;

    ImGuiWindowFlags GetFlags() override;

    void OnBegin() override;
    void OnDraw() override;

    void OnProcess();

    void ShowTitle(bool show = true) {
        m_ShowTitle = show;
    }

    void ShowFPS(bool show = true) {
        m_ShowFPS = show;
    }

    void ActivateSRTimer(bool active = true) {
        m_SRActivated = active;
    }

    void ShowSRTimer(bool show = true) {
        m_ShowSRTimer = show;
    }

    void ResetSRTimer(bool activate = false, bool show = false) {
        m_SRTimer = 0.0f;
        strcpy(m_SRScore, "00:00:00.000");
        ActivateSRTimer(activate);
        ShowSRTimer(show);
    }

    float GetSRScore() const {
        return m_SRTimer;
    }

private:
    void OnProcess_Fps();
    void OnProcess_SRTimer();

    int m_FPSCount = 0;
    int m_FPSTimer = 0;
    char m_FPSText[16] = "FPS: 0";
    float m_SRTimer = 0.0f;
    char m_SRScore[16] = "00:00:00.000";
    bool m_SRActivated = false;
    bool m_ShowSRTimer = false;
    bool m_ShowFPS = false;
    bool m_ShowTitle = false;
};

#endif // BML_HUD_H
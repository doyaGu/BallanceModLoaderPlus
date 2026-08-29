#ifndef BML_BUILTINHUD_H
#define BML_BUILTINHUD_H

#include <cstdint>
#include <memory>

#include "FpsCounter.h"
#include "HUD.h"
#include "SRTimer.h"

class BuiltinHUD {
public:
    void OnLoad(bool showTitle, bool showFPS);
    void OnUnload();
    void OnProcess(float frameDeltaSeconds, float gameDeltaMilliseconds, bool cheatEnabled);

    void RestorePrimaryVisibility(bool showTitle, bool showFPS);
    void EndLevel();

    void ShowTitle(bool show);
    void ShowFPS(bool show);
    void ShowSRTimer(bool show);

    void StartSRTimer();
    void PauseSRTimer();
    void ResetSRTimer();
    float GetSRTime() const;

    void SetFPSUpdateFrequency(uint32_t frames);

    HUD &GetWindow() { return m_Window; }

private:
    void UpdateTimerDisplay();
    void UpdateCheatState(bool cheatEnabled);

    HUD m_Window;
    FpsCounter m_FPSCounter;
    SRTimer m_SRTimer;
    bool m_LastCheatState = false;

    std::shared_ptr<HUDElement> m_TitleElement;
    std::shared_ptr<HUDElement> m_FPSElement;
    std::shared_ptr<HUDElement> m_SRElement;
    std::shared_ptr<HUDElement> m_CheatElement;
};

#endif // BML_BUILTINHUD_H

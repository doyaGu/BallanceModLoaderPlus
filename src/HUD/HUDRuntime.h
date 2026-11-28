#ifndef BML_HUDRUNTIME_H
#define BML_HUDRUNTIME_H

#include <cstddef>
#include <cstdint>
#include <memory>

#include "HUD/FpsCounter.h"
#include "HUD/HUD.h"
#include "HUD/SRTimer.h"

class ModContext;
class Config;
class Property;
class HUDCommand;

enum HudTypes {
    HUD_TITLE = 1,
    HUD_FPS = 2,
    HUD_SR = 4,
};

class HUDRuntime {
public:
    void InitConfig(Config &config);
    void ApplyConfig();
    bool OnModifyConfig(const char *category, const char *key, Property *property);

    void OnLoad(ModContext &bml);
    void OnUnload();
    void OnProcess(uint32_t gameTick, float gameDeltaMilliseconds, bool hasGameplayTimer);
    void OnRenderFrame();

    void OnMenuStart();
    void OnLevelStart();
    void OnLevelExit();

    int GetMode() const;
    void SetMode(int mode);

    void ShowTitle(bool show);
    void ShowFPS(bool show);
    void ShowSRTimer(bool show);

    void StartSRTimer(uint32_t gameTick);
    void PauseSRTimer();
    void ResetSRTimer();
    void UpdateGameplayTimer(unsigned inputs, float deltaTimeMs);
    float GetSRTime() const;

private:
    friend class HUDCommand;

    HUD &GetWindow() { return m_Window; }
    void SetFPSUpdateFrequency(uint32_t frames);

    enum ApplyWhen : unsigned {
        Startup = 1U << 0,
        OnChange = 1U << 1,
        OnLevelInit = 1U << 2,
    };

    struct Setting {
        const char *key;
        Property *HUDRuntime::*property;
        void (*apply)(HUDRuntime &hud, Property *property);
        unsigned when;
        bool requiresIngame;
    };

    static const Setting *GetSettings(size_t &count);
    void ApplySettings(ApplyWhen when);
    void ApplySetting(const Setting &setting, Property *property, bool levelStarting = false);

    void UpdateFPSDisplay();
    void UpdateTimerDisplay();
    void UpdateSRTimerDisplay();
    void UpdateSRTimerFade();
    void UpdateCheatState(bool cheatEnabled);

    ModContext *m_BML = nullptr;
    HUD m_Window;
    FpsCounter m_FPSCounter;
    SRTimer m_SRTimer;
    bool m_LastCheatState = false;

    std::shared_ptr<HUDElement> m_TitleElement;
    std::shared_ptr<HUDElement> m_FPSElement;
    std::shared_ptr<HUDElement> m_SRElement;
    std::shared_ptr<HUDElement> m_CheatElement;

    Property *m_ShowTitle = nullptr;
    Property *m_ShowFPS = nullptr;
    Property *m_ShowSRTimer = nullptr;
    Property *m_FPSUpdateFrequency = nullptr;
};

#endif // BML_HUDRUNTIME_H

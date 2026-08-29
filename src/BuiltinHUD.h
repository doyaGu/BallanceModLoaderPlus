#ifndef BML_BUILTINHUD_H
#define BML_BUILTINHUD_H

#include <cstddef>
#include <cstdint>
#include <memory>

#include "FpsCounter.h"
#include "HUD.h"
#include "SRTimer.h"

class IBML;
class IConfig;
class IProperty;
class CommandHUD;

enum HudTypes {
    HUD_TITLE = 1,
    HUD_FPS = 2,
    HUD_SR = 4,
};

class BuiltinHUD {
public:
    void InitConfig(IConfig &config);
    void ApplyConfig();
    bool OnModifyConfig(const char *category, const char *key, IProperty *property);

    void OnLoad(IBML &bml);
    void OnUnload();
    void OnProcess(float frameDeltaSeconds, float gameDeltaMilliseconds);

    void OnMenuStart();
    void OnLevelStart();
    void OnLevelExit();

    int GetMode() const;
    void SetMode(int mode);

    void ShowTitle(bool show);
    void ShowFPS(bool show);
    void ShowSRTimer(bool show);

    void StartSRTimer();
    void PauseSRTimer();
    void ResetSRTimer();
    float GetSRTime() const;

    void SetFPSUpdateFrequency(uint32_t frames);

private:
    friend class CommandHUD;

    HUD &GetWindow() { return m_Window; }

    enum ApplyWhen : unsigned {
        Startup = 1U << 0,
        OnChange = 1U << 1,
        OnLevelInit = 1U << 2,
    };

    struct Setting {
        const char *key;
        IProperty *BuiltinHUD::*property;
        void (*apply)(BuiltinHUD &hud, IProperty *property);
        unsigned when;
        bool requiresIngame;
    };

    static const Setting *GetSettings(size_t &count);
    void ApplySettings(ApplyWhen when);
    void ApplySetting(const Setting &setting, IProperty *property);

    void UpdateTimerDisplay();
    void UpdateCheatState(bool cheatEnabled);

    IBML *m_BML = nullptr;
    HUD m_Window;
    FpsCounter m_FPSCounter;
    SRTimer m_SRTimer;
    bool m_LastCheatState = false;

    std::shared_ptr<HUDElement> m_TitleElement;
    std::shared_ptr<HUDElement> m_FPSElement;
    std::shared_ptr<HUDElement> m_SRElement;
    std::shared_ptr<HUDElement> m_CheatElement;

    IProperty *m_ShowTitle = nullptr;
    IProperty *m_ShowFPS = nullptr;
    IProperty *m_ShowSRTimer = nullptr;
    IProperty *m_FPSUpdateFrequency = nullptr;
};

#endif // BML_BUILTINHUD_H

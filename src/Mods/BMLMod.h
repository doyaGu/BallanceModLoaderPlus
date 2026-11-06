#ifndef BML_BMLMOD_H
#define BML_BMLMOD_H

#include <cstddef>

#include "Loader/ModRecord.h"
#include "BML/Behavior.hpp"

#include "Console/Console.h"
#include "CustomMaps/CustomMaps.h"
#include "Gameplay/GameEventHooks.h"
#include "Gameplay/GameplayTweaks.h"
#include "HUD/HUDRuntime.h"
#include "ModMenu/ModMenu.h"
#include "ModMenu/ModsMenuEntry.h"

class ModContext;
struct FontCommandContext;

class BMLMod {
public:
    ModRecord &Registration() noexcept { return m_Record; }

    const ModRecord &Registration() const noexcept { return m_Record; }

    explicit BMLMod(ModContext *context);

    // The builtin capabilities read the BML mod's bound runtime, never the
    // ambient BML_GetModContext() singleton.
    ModContext *GetRuntimeContext() const;

    const char *GetID() { return "BML"; }

    const char *GetVersion() { return BML_VERSION; }

    const char *GetName() { return "Ballance Mod Loader"; }

    const char *GetAuthor() { return "doyaGu & Gamepiaynmo & YingChe"; }

    const char *GetDescription() {
        return "Implementation of functions provided by Ballance Mod Loader."
               "\n\n https://github.com/doyaGu/BallanceModLoaderPlus";
    }

    BMLVersion GetBMLVersion() { return {}; }

    void OnLoad();
    void OnUnload();
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName,
                      CK_CLASSID filterClass, CKBOOL addToScene, CKBOOL reuseMeshes,
                      CKBOOL reuseMaterials, CKBOOL dynamic, XObjectArray *objArray,
                      CKObject *masterObj);
    void OnLoadScript(const char *filename, CKBehavior *script);
    void OnProcess();
    void OnModifyConfig(const char *category, const char *key, Property *prop);

    void OnPreStartMenu();
    void OnPostStartMenu();
    void OnExitGame();
    void OnPreLoadLevel();
    void OnStartLevel();
    void OnPostExitLevel();
    void OnPauseLevel();
    void OnUnpauseLevel();
    void OnBallNavActive();
    void OnCounterActive();
    void OnCounterInactive();

    void AddIngameMessage(const char *msg);
    void ClearIngameMessages();

    void OpenModsMenu();
    void CloseModsMenu();
    bool CloseModsMenuForShutdown() noexcept;

    void OpenMapMenu();
    void CloseMapMenu();

    int GetHSScore();

    void ApplyFrameRateSettings();
    void AdjustFrameRate(bool sync = false, float limit = 60.0f);


    int GetHUD();
    void SetHUD(int mode);

    // Built-in HUD element controls
    void ShowTitle(bool show);
    void ShowFPS(bool show);
    void ShowSRTimer(bool show);

    // Timer controls
    void StartSRTimer();
    void PauseSRTimer();
    void ResetSRTimer();
    float GetSRTime() const;

private:
    ModContext *m_BML;
    ModRecord m_Record;

    enum ApplyWhen : unsigned {
        OnDemand = 0,
        Startup = 1U << 0,
        OnChange = 1U << 1,
        OnLevelInit = 1U << 2,
    };

    struct Setting {
        const char *category;
        const char *key;
        Property *BMLMod::*property;
        void (*apply)(BMLMod &mod, Property *property);
        unsigned when;
        bool requiresIngame;
    };

    static const Setting *GetSettings(std::size_t &count);
    void BindSettings();
    void ApplySettings(ApplyWhen when);
    void ApplySetting(const Setting &setting, Property *property);

    void InitConfigs();
    void InitGUI();
    FontCommandContext GetFontCommandContext() const;
    void ConfigureUiFonts();
    void RefreshFontChoices();
    static void ApplyUiFontSetting(BMLMod &mod, Property *property);
    static void ApplyUnlockFrameRateSetting(BMLMod &mod, Property *property);
    static void ApplyFrameRateLimitSetting(BMLMod &mod, Property *property);
    static void ApplyWidescreenSetting(BMLMod &mod, Property *property);

    void OnEditScript_Menu_MenuInit(CKBehavior *script);
    void AcquireGameFonts();

    void OnProcess_Menu();

    CKContext *m_CKContext = nullptr;
    CKTimeManager *m_TimeManager = nullptr;

    HUDRuntime m_HUD;
    ModMenu m_ModMenu;
    CustomMaps m_CustomMaps;
    GameEventHooks m_GameEventHooks;
    GameplayTweaks m_GameplayTweaks;
    Console m_Console;
    BML::Behavior::Session m_Behavior;
    ModsMenuEntry m_ModsMenuEntry;

    std::string m_ImGuiIniFilename;
    std::string m_ImGuiLogFilename;

#ifndef NDEBUG
    bool m_ShowImGuiDemo = false;
#endif

    Property *m_FontFilename = nullptr;
    Property *m_FontSize = nullptr;
    Property *m_FontFallbacks = nullptr;
    Property *m_FontFallbackSize = nullptr;
    Property *m_UseSystemFontFallbacks = nullptr;
    Property *m_EnableIniSettings = nullptr;

    Property *m_UnlockFPS = nullptr;
    Property *m_FPSLimit = nullptr;
    Property *m_WidescreenFix = nullptr;
};

#endif // BML_BMLMOD_H

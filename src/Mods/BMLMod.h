#ifndef BML_BMLMOD_H
#define BML_BMLMOD_H

#include <cstddef>

#include "BML/IMod.h"
#include "BML/IBML.h"
#include "BML/Behavior.hpp"

#include "Console/Console.h"
#include "CustomMaps/CustomMaps.h"
#include "Gameplay/GameEventHooks.h"
#include "Gameplay/GameplayTweaks.h"
#include "HUD/HUDRuntime.h"
#include "ModMenu/ModMenu.h"
#include "Mods/ModsMenuEntry.h"

class ModContext;
struct FontCommandContext;

class BMLMod : public IMod {
public:
    explicit BMLMod(ModContext *context);

    // The builtin capabilities read the BML mod's bound runtime, never the
    // ambient BML_GetModContext() singleton.
    ModContext *GetRuntimeContext() const;

    const char *GetID() override { return "BML"; }
    const char *GetVersion() override { return BML_VERSION; }
    const char *GetName() override { return "Ballance Mod Loader"; }
    const char *GetAuthor() override { return "doyaGu & Gamepiaynmo & YingChe"; }
    const char *GetDescription() override {
        return "Implementation of functions provided by Ballance Mod Loader."
               "\n\n https://github.com/doyaGu/BallanceModLoaderPlus";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override;
    void OnUnload() override;
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName,
                      CK_CLASSID filterClass, CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials,
                      CKBOOL dynamic, XObjectArray *objArray, CKObject *masterObj) override;
    void OnLoadScript(const char *filename, CKBehavior *script) override;
    void OnProcess() override;
    void OnModifyConfig(const char *category, const char *key, IProperty *prop) override;


    void OnPreStartMenu() override;
    void OnPostStartMenu() override;
    void OnExitGame() override;
    void OnPreLoadLevel() override;
    void OnStartLevel() override;
    void OnPostExitLevel() override;
    void OnPauseLevel() override;
    void OnUnpauseLevel() override;
    void OnBallNavActive() override;
    void OnCounterActive() override;
    void OnCounterInactive() override;

    void AddIngameMessage(const char *msg);
    void ClearIngameMessages();

    void OpenModsMenu();
    void CloseModsMenu();
    void CloseModsMenuForShutdown();

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
    enum ApplyWhen : unsigned {
        OnDemand = 0,
        Startup = 1U << 0,
        OnChange = 1U << 1,
        OnLevelInit = 1U << 2,
    };

    struct Setting {
        const char *category;
        const char *key;
        IProperty *BMLMod::*property;
        void (*apply)(BMLMod &mod, IProperty *property);
        unsigned when;
        bool requiresIngame;
    };

    static const Setting *GetSettings(std::size_t &count);
    void BindSettings();
    void ApplySettings(ApplyWhen when);
    void ApplySetting(const Setting &setting, IProperty *property);

    void InitConfigs();
    void InitGUI();
    FontCommandContext GetFontCommandContext() const;
    void ConfigureUiFonts();
    void RefreshFontChoices();
    static void ApplyUiFontSetting(BMLMod &mod, IProperty *property);
    static void ApplyUnlockFrameRateSetting(BMLMod &mod, IProperty *property);
    static void ApplyFrameRateLimitSetting(BMLMod &mod, IProperty *property);
    static void ApplyWidescreenSetting(BMLMod &mod, IProperty *property);

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

    IProperty *m_FontFilename = nullptr;
    IProperty *m_FontSize = nullptr;
    IProperty *m_FontFallbacks = nullptr;
    IProperty *m_UseSystemFontFallbacks = nullptr;
    IProperty *m_EnableIniSettings = nullptr;

    IProperty *m_UnlockFPS = nullptr;
    IProperty *m_FPSLimit = nullptr;
    IProperty *m_WidescreenFix = nullptr;

};

#endif // BML_BMLMOD_H

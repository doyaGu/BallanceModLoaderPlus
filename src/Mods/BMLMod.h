#ifndef BML_BMLMOD_H
#define BML_BMLMOD_H

#include <cstddef>
#include <functional>
#include <memory>
#include <unordered_map>

#include "BML/Behavior.hpp"
#include "BML/Version.h"
#include "CKAll.h"

#include "Console/Console.h"
#include "CustomMaps/CustomMaps.h"
#include "Gameplay/GameEventHooks.h"
#include "Gameplay/GameplayTweaks.h"
#include "HUD/HUDRuntime.h"
#include "ModMenu/ModMenu.h"
#include "ModMenu/ModsMenuEntry.h"

class Config;
class Logger;
namespace BML { class GameFontCatalog; }
namespace BML::UI { class FontRuntime; }
struct FontCommandContext;

class BMLMod {
public:
    // The Loader composes these capabilities for its built-in features.
    // Runtime Context, registration identity and callback ownership stay with the Loader.
    class Host {
    public:
        virtual ~Host() = default;
        virtual CKContext &Engine() = 0;
        virtual Config &Settings() = 0;
        virtual Logger &Log() = 0;
        virtual ModMenu &Menu() = 0;
        virtual BML::UI::FontRuntime *UiFonts() = 0;
        virtual BML::GameFontCatalog &GameFonts() = 0;
        virtual std::string LoaderDirectory() = 0;
        virtual void StartFeatures(CustomMaps &, GameEventHooks &, GameplayTweaks &,
                                   Console &, HUDRuntime &, FontCommandContext) = 0;
        virtual void StopFeatures() = 0;
        virtual void LoadOptionsEntry(ModsMenuEntry &, BML::Behavior::Session &, CKBehavior *) = 0;
        virtual void ScheduleFontAcquisition(std::function<void()> callback) = 0;
        virtual void SetWidescreen(bool enabled) = 0;
        virtual void RenderDevTools() = 0;
#if BML_ENABLE_UI_AUTOMATION
        virtual void StartUiAutomation(BMLMod &) = 0;
#endif
    };

    explicit BMLMod(std::unique_ptr<Host> host);

    const char *GetID() { return "BML"; }

    const char *GetVersion() { return BML_VERSION; }

    const char *GetName() { return "Ballance Mod Loader"; }

    const char *GetAuthor() { return "doyaGu & Gamepiaynmo & YingChe"; }

    const char *GetDescription() {
        return "Implementation of functions provided by Ballance Mod Loader."
               "\n\n https://github.com/doyaGu/BallanceModLoaderPlus";
    }

    void OnLoad();
    void OnUnload();
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName,
                      CK_CLASSID filterClass, CKBOOL addToScene, CKBOOL reuseMeshes,
                      CKBOOL reuseMaterials, CKBOOL dynamic, XObjectArray *objArray,
                      CKObject *masterObj);
    void OnLoadScript(const char *filename, CKBehavior *script);
    void OnProcess();
    void OnPlayerRender();
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
    std::unique_ptr<Host> m_Host;

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

    void BindGameplayTimer(CKBehavior *script);
    void UnbindGameplayTimer();
    CKBehavior *GetGameplayTimer() const;
    static int ExecuteGameplayTimer(const CKBehaviorContext &context);

    CKContext *m_CKContext = nullptr;
    CKTimeManager *m_TimeManager = nullptr;
    CK_ID m_GameplayTimer = 0;
    CKBEHAVIORFCT m_GameplayTimerFunction = nullptr;
    static std::unordered_map<CKContext *, BMLMod *> s_GameplayTimerOwners;

    HUDRuntime m_HUD;
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

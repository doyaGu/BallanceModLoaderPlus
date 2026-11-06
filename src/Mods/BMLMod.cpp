#include "Mods/BMLMod.h"

#include <algorithm>
#include <bitset>
#include <vector>

#include "UI/BuiHost.h"
#include "UI/GuiHost.h"

#include "Config/Config.h"
#include "Loader/ModContext.h"
#include "Hooks/RenderHook.h"
#include "Gameplay/CheatCommand.h"
#include "HUD/HUDCommand.h"
#include "Mods/BMLCommand.h"
#include "Mods/BMLConfigMigration.h"
#include "UI/AnsiPalette.h"
#include "UI/FontCommand.h"
#include "UI/FontRuntime.h"
#include "UI/GameFontCatalog.h"
#include "UI/PaletteCommand.h"
#include "AngelScript/ScriptCommand.h"
#if BML_ENABLE_UI_AUTOMATION
#include "player/UiAutomation.h"
#endif
#include "StringUtils.h"
#include "PathUtils.h"
#include "Api/BuiltinCapabilities.h"
#if BML_ENABLE_ANGELSCRIPT
#include "AngelScript/ScriptDevToolsService.h"
#endif

namespace Ui = BML::UI;
namespace Behavior = BML::Behavior;

namespace {
constexpr std::size_t GameFontRoleCount = static_cast<std::size_t>(BML::GameFont::Count);
constexpr unsigned long GameFontAcquireDelay = 1;

using BoundGameFontRoles = std::bitset<GameFontRoleCount>;

void SetChoiceEditor(Property *property, const std::vector<std::string> &choices) {
    if (!property)
        return;

    std::vector<const char *> values;
    values.reserve(choices.size());
    for (const std::string &choice : choices)
        values.push_back(choice.c_str());
    if (!BML::ConfigMetadata::SetChoices(property, values.data(), values.size()))
        return;
    BML::ConfigMetadata::SetEditor(
        property, choices.empty() ? BML_CONFIG_EDITOR_DEFAULT
                                  : BML_CONFIG_EDITOR_CHOICE);
}

void ConfigureFontChoiceEditors(Property *primary, Property *fallbacks,
                                const Ui::FontRuntime *runtime) {
    std::vector<std::string> catalog = runtime
        ? runtime->ListLoaderFaces()
        : std::vector<std::string>();

    SetChoiceEditor(primary, catalog);

    std::vector<std::string> fallbackChoices;
    fallbackChoices.reserve(catalog.size() + 1);
    fallbackChoices.emplace_back();
    for (const std::string &face : catalog) {
        if (face.find(';') == std::string::npos)
            fallbackChoices.push_back(face);
    }
    SetChoiceEditor(fallbacks, fallbackChoices);
}

void CollectGameFontRoles(const Behavior::Graph &graph,
                          BML::GameFontCatalog &catalog,
                          BoundGameFontRoles &boundRoles) {
    for (const Behavior::Node node : graph.Nodes()) {
        if (node.Index() < 0)
            continue;
        if (node.Name() == "TT CreateFontEx") {
            auto runtimeName = graph.Read(node.Pin(0));
            auto font = graph.Read(node.Pout(0));
            const auto *name = runtimeName
                ? std::get_if<std::string>(&runtimeName->Data) : nullptr;
            const auto *handle = font
                ? std::get_if<std::int32_t>(&font->Data) : nullptr;
            BML::GameFont role = BML::GameFont::None;
            if (name && handle && catalog.Bind(*name, *handle, &role))
                boundRoles.set(static_cast<std::size_t>(role));
        }

        if (!node.IsGraph())
            continue;
        auto nested = graph.Inspect(node);
        if (nested)
            CollectGameFontRoles(nested.Value(), catalog, boundRoles);
    }
}

} // namespace

BMLMod::BMLMod(ModContext *context) : m_BML(context), m_Record(context), m_ModMenu(*context) {
    m_Record.Bind(*this);
}

ModContext *BMLMod::GetRuntimeContext() const {
    return m_BML;
}

const BMLMod::Setting *BMLMod::GetSettings(std::size_t &count) {
    static const Setting settings[] = {
        {"GUI", "FontFilename", &BMLMod::m_FontFilename, &BMLMod::ApplyUiFontSetting, Startup | OnChange, false},
        {"GUI", "FontSize", &BMLMod::m_FontSize, &BMLMod::ApplyUiFontSetting, Startup | OnChange, false},
        {"GUI", "FontFallbacks", &BMLMod::m_FontFallbacks, &BMLMod::ApplyUiFontSetting, Startup | OnChange, false},
        {"GUI", "FontFallbackSize", &BMLMod::m_FontFallbackSize, &BMLMod::ApplyUiFontSetting, Startup | OnChange, false},
        {"GUI", "UseSystemFontFallbacks", &BMLMod::m_UseSystemFontFallbacks, &BMLMod::ApplyUiFontSetting, Startup | OnChange, false},
        {"GUI", "EnableIniSettings", &BMLMod::m_EnableIniSettings, nullptr, OnDemand, false},

        {"Graphics", "UnlockFrameRate", &BMLMod::m_UnlockFPS, &BMLMod::ApplyUnlockFrameRateSetting, OnChange | OnLevelInit, false},
        {"Graphics", "SetMaxFrameRate", &BMLMod::m_FPSLimit, &BMLMod::ApplyFrameRateLimitSetting, OnChange, false},
        {"Graphics", "WidescreenFix", &BMLMod::m_WidescreenFix, &BMLMod::ApplyWidescreenSetting, Startup | OnChange, false},

    };
    static_assert(sizeof(settings) / sizeof(settings[0]) == 9,
                  "Every BMLMod-owned config property must have one settings-table entry");

    count = sizeof(settings) / sizeof(settings[0]);
    return settings;
}

void BMLMod::BindSettings() {
    std::size_t count = 0;
    const Setting *settings = GetSettings(count);
    Config *config = m_Record.GetConfig();
    for (std::size_t i = 0; i < count; ++i)
        this->*settings[i].property = config->GetProperty(settings[i].category, settings[i].key);
}

void BMLMod::ApplySetting(const Setting &setting, Property *property) {
    if (!setting.apply || !property)
        return;
    if (setting.requiresIngame && (!m_BML || !m_BML->IsIngame()))
        return;

    setting.apply(*this, property);
}

void BMLMod::ApplySettings(ApplyWhen when) {
    std::size_t count = 0;
    const Setting *settings = GetSettings(count);
    for (std::size_t i = 0; i < count; ++i) {
        if ((settings[i].when & static_cast<unsigned>(when)) == 0)
            continue;
        ApplySetting(settings[i], this->*settings[i].property);
    }
}

void BMLMod::OnLoad() {
    m_CKContext = m_BML->GetCKContext();
    m_TimeManager = m_BML->GetTimeManager();

    auto behavior = Behavior::Session::Open(GetID());
    if (behavior)
        m_Behavior = behavior.Take();
    else
        m_Record.GetLogger()->Warn("Behavior authoring is unavailable to the built-in Mod: %s",
                                   behavior.GetStatus().Message.empty()
                                       ? "could not open the BML session"
                                       : behavior.GetStatus().Message.c_str());

    // Configure AnsiPalette to use the ModLoader directory for config/themes
    AnsiPalette::SetLoaderDirProvider([]() -> std::wstring {
        return BML_GetModContext()->GetDirectory(BML_DIR_LOADER);
    });

    InitConfigs();
    ApplySettings(Startup);
    m_HUD.ApplyConfig();
    m_Console.ApplyConfig();
    m_CustomMaps.ApplyConfig();
    InitGUI();
    if (ModContext *context = GetRuntimeContext()) {
        m_CustomMaps.OnLoad(*m_BML, *m_Record.GetLogger(), context->GetDirectory(BML_DIR_GAME),
                            context->GetDirectory(BML_DIR_LOADER),
                            context->GetDirectory(BML_DIR_TEMP));
    } else {
        m_Record.GetLogger()->Error("Built-in Custom Maps requires the loader runtime context");
    }
    m_GameEventHooks.OnLoad(*m_BML, *m_Record.GetLogger());
    m_GameplayTweaks.OnLoad(*m_BML, *m_Record.GetLogger());
    m_Console.OnLoad(*m_BML, BML_GetModContext()->GetCommandContext(), *m_Record.GetLogger());
    m_HUD.OnLoad(*m_BML);
    m_BML->RegisterCommand(std::make_unique<BMLCommand>(GetRuntimeContext()));
    m_BML->RegisterCommand(std::make_unique<FontCommand>(GetFontCommandContext()));
    m_BML->RegisterCommand(std::make_unique<CheatCommand>());
    m_BML->RegisterCommand(std::make_unique<HUDCommand>(&m_HUD));
    m_BML->RegisterCommand(std::make_unique<PaletteCommand>());
    m_BML->RegisterCommand(std::make_unique<ScriptCommand>());

    if (ModContext *context = GetRuntimeContext())
        RegisterBuiltinCapabilities(*this, context->ObjectRefs(), m_Record.GetLogger());
}

void BMLMod::OnUnload() {
#if BML_ENABLE_UI_AUTOMATION
    UiAutomation::Shutdown();
#endif
    UnregisterBuiltinCapabilities(*this);

    m_Console.OnUnload();
    m_HUD.OnUnload();
    m_CustomMaps.OnUnload();
    m_GameplayTweaks.OnUnload();
    m_GameEventHooks.OnUnload();
    m_ModsMenuEntry.Unload();
    m_Behavior.Reset();

    BuiHost::CleanupResources(m_CKContext);

    if (m_EnableIniSettings->GetBoolean()) {
        ImGui::SaveIniSettingsToDisk(m_ImGuiIniFilename.c_str());
    }


    // Reset pointers to prevent use-after-free
    m_TimeManager = nullptr;
}

void BMLMod::OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                          CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                          XObjectArray *objArray, CKObject *masterObj) {
    m_CustomMaps.OnLoadObject(filename);

    if (!strcmp(filename, "3D Entities\\Menu.nmo")) {
        BGuiHost::Gui::InitMaterials();
        BuiHost::InitSounds(m_CKContext);
    }
}

void BMLMod::OnLoadScript(const char *filename, CKBehavior *script) {
    m_CustomMaps.OnLoadScript(script);

    if (!strcmp(script->GetName(), "Menu_Init"))
        OnEditScript_Menu_MenuInit(script);

    if (!strcmp(script->GetName(), "Menu_Options"))
        m_ModsMenuEntry.Load(m_Behavior, script, *m_BML, *m_Record.GetLogger());
}

void BMLMod::OnProcess() {
    m_ModsMenuEntry.OnProcess();
    m_HUD.OnProcess(ImGui::GetIO().DeltaTime, m_TimeManager->GetLastDeltaTime());
    OnProcess_Menu();
    m_Console.OnProcess();
#if BML_ENABLE_ANGELSCRIPT
    if (auto *context = BML_GetModContext()) {
        if (auto *devTools = context->GetModLoader().GetScriptDevTools())
            devTools->RenderPanel();
    }
#endif

#ifndef NDEBUG
    if (ImGui::IsKeyChordPressed(ImGuiMod_Shift | ImGuiMod_Alt | ImGuiKey_F5))
        m_ShowImGuiDemo = !m_ShowImGuiDemo;
    if (m_ShowImGuiDemo)
        ImGui::ShowDemoWindow(&m_ShowImGuiDemo);
#endif

}

void BMLMod::OnModifyConfig(const char *category, const char *key, Property *prop) {
    if (!prop)
        return;

    if (m_Console.OnModifyConfig(category, key, prop))
        return;

    if (m_HUD.OnModifyConfig(category, key, prop))
        return;

    if (m_CustomMaps.OnModifyConfig(category, key, prop))
        return;

    if (m_GameplayTweaks.OnModifyConfig(category, key, prop))
        return;

    std::size_t count = 0;
    const Setting *settings = GetSettings(count);
    for (std::size_t i = 0; i < count; ++i) {
        const Setting &setting = settings[i];
        if ((setting.when & OnChange) == 0 || this->*setting.property != prop)
            continue;
        if (!utils::CStringEqual(setting.category, category) || !utils::CStringEqual(setting.key, key))
            continue;

        ApplySetting(setting, prop);
        return;
    }
}

void BMLMod::OnPreStartMenu() {
    m_Console.CloseCommandBar();
    m_HUD.OnMenuStart();
}

void BMLMod::OnPostStartMenu() {
    ApplyFrameRateSettings();
    m_CustomMaps.OnPostStartMenu();
#if BML_ENABLE_UI_AUTOMATION
    // The Player acceptance suite must exercise the shipped start-up flow.
    // Starting here also guarantees that production ImGui frames are live
    // before the Test Engine begins driving application windows.
    UiAutomation::Start(*this);
#endif
}

void BMLMod::OnExitGame() {
    m_CustomMaps.OnExitGame();
    m_GameplayTweaks.OnExitGame();
#ifndef NDEBUG
    m_ShowImGuiDemo = false;
#endif
}

void BMLMod::OnPreLoadLevel() {
    m_Console.CloseCommandBar();
}

void BMLMod::OnStartLevel() {
    ApplySettings(OnLevelInit);

    m_HUD.OnLevelStart();
    m_CustomMaps.OnStartLevel();
#if BML_ENABLE_UI_AUTOMATION
    UiAutomation::OnStartLevel();
#endif
}

void BMLMod::OnPostExitLevel() {
    m_HUD.OnLevelExit();
}

void BMLMod::OnBallNavActive() {
#if BML_ENABLE_UI_AUTOMATION
    UiAutomation::OnBallNavActive();
#endif
}

void BMLMod::OnPauseLevel() {
    m_HUD.PauseSRTimer();
}

void BMLMod::OnUnpauseLevel() {
    m_HUD.StartSRTimer();
}

void BMLMod::OnCounterActive() {
    m_HUD.StartSRTimer();
}

void BMLMod::OnCounterInactive() {
    m_HUD.PauseSRTimer();
}

void BMLMod::AddIngameMessage(const char *msg) {
    m_Console.AddMessage(msg);
}

void BMLMod::ClearIngameMessages() {
    m_Console.ClearMessages();
}

void BMLMod::OpenModsMenu() {
    if (!m_ModMenu.IsOpen())
        RefreshFontChoices();
    if (!m_ModMenu.Open())
        m_Record.GetLogger()->Error("Cannot open the Mods menu route");
}

void BMLMod::CloseModsMenu() {
    m_ModMenu.Close();
}

bool BMLMod::CloseModsMenuForShutdown() noexcept {
    return m_ModMenu.CloseForShutdown();
}

void BMLMod::OpenMapMenu() {
    m_CustomMaps.Open();
}

void BMLMod::CloseMapMenu() {
    m_CustomMaps.Close();
}

int BMLMod::GetHSScore() {
    ModContext *context = GetRuntimeContext();
    if (!context)
        return 0;
    int score = 0;
    return ReadBuiltinGameplayHighScore(*context, score) == BML_OK ? score : 0;
}

void BMLMod::ApplyFrameRateSettings() {
    if (m_UnlockFPS->GetBoolean()) {
        AdjustFrameRate(false, 0);
    } else {
        int val = m_FPSLimit->GetInteger();
        if (val > 0)
            AdjustFrameRate(false, static_cast<float>(val));
        else
            AdjustFrameRate(true);
    }
}

void BMLMod::AdjustFrameRate(bool sync, float limit) {
    if (sync) {
        m_TimeManager->ChangeLimitOptions(CK_FRAMERATE_SYNC);
    } else if (limit > 0) {
        m_TimeManager->ChangeLimitOptions(CK_FRAMERATE_LIMIT);
        m_TimeManager->SetFrameRateLimit(limit);
    } else {
        m_TimeManager->ChangeLimitOptions(CK_FRAMERATE_FREE);
    }
}


int BMLMod::GetHUD() {
    return m_HUD.GetMode();
}

void BMLMod::SetHUD(int mode) {
    m_HUD.SetMode(mode);
}

void BMLMod::InitConfigs() {
    auto *config = m_Record.GetConfig();
    if (config && !MigrateBMLConfig(config->GetLoadedValues()))
        m_Record.GetLogger()->Warn(
            "Some 0.3.13 font paths need manual selection; use the font command to choose fonts.");

    BindSettings();
    m_HUD.InitConfig(*m_Record.GetConfig());
    m_Console.InitConfig(*m_Record.GetConfig());
    m_CustomMaps.InitConfig(*m_Record.GetConfig());
    m_GameplayTweaks.InitConfig(*m_Record.GetConfig());

    m_Record.GetConfig()->SetCategoryComment("GUI", "GUI Settings");

    m_FontFilename->SetComment(
        "Primary UI font. Add TTF/OTF/TTC files to ModLoader\\Fonts; use the font command for explicit paths.");
    m_FontFilename->SetDefaultString(Ui::DefaultPrimaryFont);

    m_FontSize->SetComment("Logical UI font size at a 1200-pixel viewport height (8-96).");
    m_FontSize->SetDefaultFloat(Ui::DefaultFontReferenceSize);

    m_FontFallbacks->SetComment(
        "Optional fallback UI fonts. The menu selects one file; use the font fallback command for an ordered list.");
    m_FontFallbacks->SetDefaultString(Ui::DefaultFallbackFont);
    RefreshFontChoices();

    m_FontFallbackSize->SetComment("Logical size of fallback UI fonts at a 1200-pixel viewport height (8-96).");
    m_FontFallbackSize->SetDefaultFloat(Ui::DefaultFallbackReferenceSize);

    m_UseSystemFontFallbacks->SetComment("Use Windows symbol and emoji fonts after configured fonts.");
    m_UseSystemFontFallbacks->SetDefaultBoolean(true);

    m_EnableIniSettings->SetComment("Enable loading and saving ImGui settings.");
    m_EnableIniSettings->SetDefaultBoolean(true);

    m_Record.GetConfig()->SetCategoryComment("Graphics", "Graphics Settings");

    m_UnlockFPS->SetComment("Unlock Frame Rate Limitation");
    m_UnlockFPS->SetDefaultBoolean(false);

    m_FPSLimit->SetComment("Set Frame Rate Limitation, this option will not work if frame rate is unlocked. Set to 0 will turn on VSync");
    m_FPSLimit->SetDefaultInteger(240);

    m_WidescreenFix->SetComment("Improve widescreen resolutions support");
    m_WidescreenFix->SetDefaultBoolean(false);

}

void BMLMod::ConfigureUiFonts() {
    ModContext *context = GetRuntimeContext();
    if (!context)
        return;

    Ui::FontRuntime *runtime = context->GetUiFontRuntime();
    if (!runtime)
        return;

    runtime->Configure(GetFontCommandContext().ReadProfile());
}

void BMLMod::RefreshFontChoices() {
    const ModContext *context = GetRuntimeContext();
    ConfigureFontChoiceEditors(m_FontFilename, m_FontFallbacks,
                               context ? context->GetUiFontRuntime() : nullptr);
}

FontCommandContext BMLMod::GetFontCommandContext() const {
    FontCommandContext commandContext;
    ModContext *modContext = GetRuntimeContext();
    commandContext.Runtime = modContext ? modContext->GetUiFontRuntime() : nullptr;
    commandContext.PrimaryFace = m_FontFilename;
    commandContext.ReferenceSize = m_FontSize;
    commandContext.FallbackFaces = m_FontFallbacks;
    commandContext.FallbackReferenceSize = m_FontFallbackSize;
    commandContext.UseWindowsFallbacks = m_UseSystemFontFallbacks;
    return commandContext;
}

void BMLMod::ApplyUiFontSetting(BMLMod &mod, Property *) {
    mod.ConfigureUiFonts();
}

void BMLMod::ApplyUnlockFrameRateSetting(BMLMod &mod, Property *) {
    mod.ApplyFrameRateSettings();
}

void BMLMod::ApplyFrameRateLimitSetting(BMLMod &mod, Property *property) {
    if (mod.m_UnlockFPS->GetBoolean())
        return;

    const int limit = property->GetInteger();
    if (limit > 0)
        mod.AdjustFrameRate(false, static_cast<float>(limit));
    else
        mod.AdjustFrameRate(true);
}

void BMLMod::ApplyWidescreenSetting(BMLMod &, Property *property) {
    RenderHook::EnableWidescreenFix(property->GetBoolean());
}

void BMLMod::InitGUI() {
    ImGuiIO &io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    const std::string path = BML_GetModContext()->GetDirectoryUtf8(BML_DIR_LOADER);

    m_ImGuiIniFilename = path + "\\ImGui.ini";
    m_ImGuiLogFilename = path + "\\ImGui.log";
    io.LogFilename = m_ImGuiLogFilename.c_str();

    if (m_EnableIniSettings->GetBoolean()) {
        if (utils::FileExistsUtf8(m_ImGuiIniFilename)) {
            ImGui::LoadIniSettingsFromDisk(m_ImGuiIniFilename.c_str());
        }
    }

    BuiHost::InitTextures(m_CKContext);
    BuiHost::InitMaterials(m_CKContext);
}

void BMLMod::OnEditScript_Menu_MenuInit(CKBehavior *script) {
    (void) script;
    m_BML->AddTimer(static_cast<CKDWORD>(GameFontAcquireDelay), [this]() { AcquireGameFonts(); });
}

void BMLMod::AcquireGameFonts() {
    m_Record.GetLogger()->Info("Acquire Game Fonts");
    ModContext *context = GetRuntimeContext();
    if (!context) {
        m_Record.GetLogger()->Warn("Cannot acquire Game Fonts without the loader runtime context");
        return;
    }

    BML::GameFontCatalog &catalog = context->GetGameFonts();
    catalog.Reset();

    CKBehavior *menuInit = m_BML->GetScriptByName("Menu_Init");
    if (!menuInit) {
        m_Record.GetLogger()->Warn("Cannot acquire Game Fonts: Menu_Init was not found");
        return;
    }

    if (!m_Behavior) {
        m_Record.GetLogger()->Warn("Cannot acquire Game Fonts: Behavior authoring is unavailable");
        return;
    }

    auto menu = m_Behavior.Inspect(menuInit, Behavior::View::Live);
    if (!menu) {
        m_Record.GetLogger()->Warn("Cannot inspect Menu_Init while acquiring Game Fonts: %s",
                                   menu.GetStatus().Message.c_str());
        return;
    }
    auto fontsNode = menu->Find(Behavior::Named("Fonts", 0));
    if (!fontsNode) {
        m_Record.GetLogger()->Warn("Cannot acquire Game Fonts: Menu_Init/Fonts was not found");
        return;
    }
    auto fonts = menu->Inspect(fontsNode.Value());
    if (!fonts) {
        m_Record.GetLogger()->Warn("Cannot inspect Menu_Init/Fonts while acquiring Game Fonts: %s",
                                   fonts.GetStatus().Message.c_str());
        return;
    }

    BoundGameFontRoles boundRoles;
    CollectGameFontRoles(fonts.Value(), catalog, boundRoles);

    constexpr std::size_t ExpectedFontCount = GameFontRoleCount - 1;
    const std::size_t boundCount = boundRoles.count();
    if (boundCount != ExpectedFontCount) {
        m_Record.GetLogger()->Warn("Acquired %d of %d Game Fonts; missing roles use legacy indices",
                                   static_cast<int>(boundCount),
                                   static_cast<int>(ExpectedFontCount));
    }
}

void BMLMod::OnProcess_Menu() {
    m_ModMenu.OnProcess();
    m_CustomMaps.OnProcess();
}

void BMLMod::ShowTitle(bool show) {
    m_HUD.ShowTitle(show);
}

void BMLMod::ShowFPS(bool show) {
    m_HUD.ShowFPS(show);
}

void BMLMod::ShowSRTimer(bool show) {
    m_HUD.ShowSRTimer(show);
}

void BMLMod::StartSRTimer() {
    m_HUD.StartSRTimer();
}

void BMLMod::PauseSRTimer() {
    m_HUD.PauseSRTimer();
}

void BMLMod::ResetSRTimer() {
    m_HUD.ResetSRTimer();
}

float BMLMod::GetSRTime() const {
    return m_HUD.GetSRTime();
}

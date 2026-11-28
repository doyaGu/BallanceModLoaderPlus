#include "Mods/BMLMod.h"

#include <algorithm>
#include <bitset>
#include <vector>

#include "BML/Guids/TT_Toolbox_RT.h"
#include "CKBehaviorPrototype.h"
#include "UI/BuiHost.h"
#include "UI/GuiHost.h"

#include "Config/Config.h"
#include "BML/Engine.hpp"
#include "BML/Gameplay.h"
#include "Logging/Logger.h"
#include "Mods/BMLConfigMigration.h"
#include "UI/AnsiPalette.h"
#include "UI/FontCommand.h"
#include "UI/FontRuntime.h"
#include "UI/GameFontCatalog.h"
#if BML_ENABLE_UI_AUTOMATION
#include "player/UiAutomation.h"
#endif
#include "StringUtils.h"
#include "PathUtils.h"

namespace Ui = BML::UI;
namespace Behavior = BML::Behavior;

namespace {
constexpr std::size_t GameFontRoleCount = static_cast<std::size_t>(BML::GameFont::Count);

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

std::unordered_map<CKContext *, BMLMod *> BMLMod::s_GameplayTimerOwners;

BMLMod::BMLMod(std::unique_ptr<Host> host) : m_Host(std::move(host)) {}

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
    Config &config = m_Host->Settings();
    for (std::size_t i = 0; i < count; ++i)
        this->*settings[i].property = config.GetProperty(settings[i].category, settings[i].key);
}

void BMLMod::ApplySetting(const Setting &setting, Property *property) {
    if (!setting.apply || !property)
        return;
    if (setting.requiresIngame) {
        BML::Gameplay::State state{};
        if (BML::Gameplay::ReadState(state) != BML_OK || !state.Ingame)
            return;
    }

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
    m_CKContext = &m_Host->Engine();
    m_TimeManager = m_CKContext->GetTimeManager();

    auto behavior = Behavior::Session::Open(GetID());
    if (behavior)
        m_Behavior = behavior.Take();
    else
        m_Host->Log().Warn("Behavior authoring is unavailable to the built-in Mod: %s",
                                   behavior.GetStatus().Message.empty()
                                       ? "could not open the BML session"
                                       : behavior.GetStatus().Message.c_str());

    // Configure AnsiPalette to use the ModLoader directory for config/themes
    AnsiPalette::SetLoaderDirProvider([path = utils::Utf8ToUtf16(m_Host->LoaderDirectory())] {
        return path;
    });

    InitConfigs();
    ApplySettings(Startup);
    m_HUD.ApplyConfig();
    m_Console.ApplyConfig();
    m_CustomMaps.ApplyConfig();
    InitGUI();
    m_Host->StartFeatures(m_CustomMaps, m_GameEventHooks, m_GameplayTweaks,
                          m_Console, m_HUD, GetFontCommandContext());
}

void BMLMod::OnUnload() {
#if BML_ENABLE_UI_AUTOMATION
    UiAutomation::Shutdown();
#endif
    m_Host->StopFeatures();
    AnsiPalette::SetLoaderDirProvider({});

    UnbindGameplayTimer();
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

    if (!strcmp(script->GetName(), "Gameplay_Energy"))
        BindGameplayTimer(script);

    if (!strcmp(script->GetName(), "Menu_Init"))
        OnEditScript_Menu_MenuInit(script);

    if (!strcmp(script->GetName(), "Menu_Options"))
        m_Host->LoadOptionsEntry(m_ModsMenuEntry, m_Behavior, script);
}

void BMLMod::BindGameplayTimer(CKBehavior *script) {
    for (int index = 0; index < script->GetSubBehaviorCount(); ++index) {
        CKBehavior *timer = script->GetSubBehavior(index);
        if (!timer || timer->GetPrototypeGuid() != TT_TOOLBOX_RT_TT_TIMER)
            continue;
        CKBEHAVIORFCT function = timer->GetFunction();
        if (!function || function == &ExecuteGameplayTimer)
            return;
        UnbindGameplayTimer();
        if (!s_GameplayTimerOwners.try_emplace(m_CKContext, this).second)
            return;
        m_GameplayTimer = timer->GetID();
        m_GameplayTimerFunction = function;
        timer->SetFunction(&ExecuteGameplayTimer);
        return;
    }
}

CKBehavior *BMLMod::GetGameplayTimer() const {
    if (!m_GameplayTimer || !m_CKContext)
        return nullptr;
    auto *timer = CKBehavior::Cast(m_CKContext->GetObject(m_GameplayTimer));
    return timer && timer->GetFunction() == &ExecuteGameplayTimer ? timer : nullptr;
}

void BMLMod::UnbindGameplayTimer() {
    if (auto *timer = GetGameplayTimer())
        timer->SetFunction(m_GameplayTimerFunction);
    if (m_GameplayTimer)
        s_GameplayTimerOwners.erase(m_CKContext);
    m_GameplayTimer = 0;
    m_GameplayTimerFunction = nullptr;
}

int BMLMod::ExecuteGameplayTimer(const CKBehaviorContext &context) {
    const auto owner = s_GameplayTimerOwners.find(context.Context);
    BMLMod *mod = owner != s_GameplayTimerOwners.end() ? owner->second : nullptr;
    if (!mod || mod->m_GameplayTimer != context.Behavior->GetID()) {
        auto *prototype = context.Behavior->GetPrototype();
        CKBEHAVIORFCT function = prototype ? prototype->GetFunction() : nullptr;
        return function && function != &ExecuteGameplayTimer ? function(context) : CKBR_OK;
    }

    // Observe the inputs before TT_Timer consumes them. Running on this node
    // also preserves the order if the graph invokes it twice in one CK tick.
    unsigned inputs = 0;
    for (int index = 0; index < 4; ++index) {
        if (context.Behavior->IsInputActive(index))
            inputs |= 1u << index;
    }
    const int result = mod->m_GameplayTimerFunction(context);
    mod->m_HUD.UpdateGameplayTimer(inputs, context.DeltaTime);
    return result;
}

void BMLMod::OnProcess() {
    m_ModsMenuEntry.OnProcess();
    m_HUD.OnProcess(m_TimeManager->GetMainTickCount(),
                    m_TimeManager->GetLastDeltaTime(), GetGameplayTimer() != nullptr);
    OnProcess_Menu();
    m_Console.OnProcess();
    m_Host->RenderDevTools();

#ifndef NDEBUG
    if (ImGui::IsKeyChordPressed(ImGuiMod_Shift | ImGuiMod_Alt | ImGuiKey_F5))
        m_ShowImGuiDemo = !m_ShowImGuiDemo;
    if (m_ShowImGuiDemo)
        ImGui::ShowDemoWindow(&m_ShowImGuiDemo);
#endif

}

void BMLMod::OnPlayerRender() {
    m_HUD.OnRenderFrame();
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
    m_Host->StartUiAutomation(*this);
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

    if (!GetGameplayTimer())
        ResetSRTimer();
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
    if (!GetGameplayTimer())
        PauseSRTimer();
}

void BMLMod::OnUnpauseLevel() {
    if (!GetGameplayTimer())
        StartSRTimer();
}

void BMLMod::OnCounterActive() {
    if (!GetGameplayTimer())
        StartSRTimer();
}

void BMLMod::OnCounterInactive() {
    if (!GetGameplayTimer())
        PauseSRTimer();
}

void BMLMod::AddIngameMessage(const char *msg) {
    m_Console.AddMessage(msg);
}

void BMLMod::ClearIngameMessages() {
    m_Console.ClearMessages();
}

void BMLMod::OpenModsMenu() {
    if (!m_Host->Menu().IsOpen())
        RefreshFontChoices();
    if (!m_Host->Menu().Open())
        m_Host->Log().Error("Cannot open the Mods menu route");
}

void BMLMod::CloseModsMenu() {
    m_Host->Menu().Close();
}

bool BMLMod::CloseModsMenuForShutdown() noexcept {
    return m_Host->Menu().CloseForShutdown();
}

void BMLMod::OpenMapMenu() {
    m_CustomMaps.Open();
}

void BMLMod::CloseMapMenu() {
    m_CustomMaps.Close();
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
    Config &config = m_Host->Settings();
    if (!MigrateBMLConfig(config.GetLoadedValues()))
        m_Host->Log().Warn(
            "Some 0.3.13 font paths need manual selection; use the font command to choose fonts.");

    BindSettings();
    m_HUD.InitConfig(config);
    m_Console.InitConfig(config);
    m_CustomMaps.InitConfig(config);
    m_GameplayTweaks.InitConfig(config);

    config.SetCategoryComment("GUI", "GUI Settings");

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

    config.SetCategoryComment("Graphics", "Graphics Settings");

    m_UnlockFPS->SetComment("Unlock Frame Rate Limitation");
    m_UnlockFPS->SetDefaultBoolean(false);

    m_FPSLimit->SetComment("Set Frame Rate Limitation, this option will not work if frame rate is unlocked. Set to 0 will turn on VSync");
    m_FPSLimit->SetDefaultInteger(240);

    m_WidescreenFix->SetComment("Improve widescreen resolutions support");
    m_WidescreenFix->SetDefaultBoolean(false);

}

void BMLMod::ConfigureUiFonts() {
    Ui::FontRuntime *runtime = m_Host->UiFonts();
    if (!runtime)
        return;

    runtime->Configure(GetFontCommandContext().ReadProfile());
}

void BMLMod::RefreshFontChoices() {
    ConfigureFontChoiceEditors(m_FontFilename, m_FontFallbacks, m_Host->UiFonts());
}

FontCommandContext BMLMod::GetFontCommandContext() const {
    FontCommandContext commandContext;
    commandContext.Runtime = m_Host->UiFonts();
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

void BMLMod::ApplyWidescreenSetting(BMLMod &mod, Property *property) {
    mod.m_Host->SetWidescreen(property->GetBoolean());
}

void BMLMod::InitGUI() {
    ImGuiIO &io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    const std::string path = m_Host->LoaderDirectory();

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
    m_Host->ScheduleFontAcquisition([this]() { AcquireGameFonts(); });
}

void BMLMod::AcquireGameFonts() {
    m_Host->Log().Info("Acquire Game Fonts");
    BML::GameFontCatalog &catalog = m_Host->GameFonts();
    catalog.Reset();

    CKBehavior *menuInit = BML::Engine::FindObjectByName<CKBehavior>(*m_CKContext, "Menu_Init");
    if (!menuInit) {
        m_Host->Log().Warn("Cannot acquire Game Fonts: Menu_Init was not found");
        return;
    }

    if (!m_Behavior) {
        m_Host->Log().Warn("Cannot acquire Game Fonts: Behavior authoring is unavailable");
        return;
    }

    auto menu = m_Behavior.Inspect(menuInit, Behavior::View::Live);
    if (!menu) {
        m_Host->Log().Warn("Cannot inspect Menu_Init while acquiring Game Fonts: %s",
                                   menu.GetStatus().Message.c_str());
        return;
    }
    auto fontsNode = menu->Find(Behavior::Named("Fonts", 0));
    if (!fontsNode) {
        m_Host->Log().Warn("Cannot acquire Game Fonts: Menu_Init/Fonts was not found");
        return;
    }
    auto fonts = menu->Inspect(fontsNode.Value());
    if (!fonts) {
        m_Host->Log().Warn("Cannot inspect Menu_Init/Fonts while acquiring Game Fonts: %s",
                                   fonts.GetStatus().Message.c_str());
        return;
    }

    BoundGameFontRoles boundRoles;
    CollectGameFontRoles(fonts.Value(), catalog, boundRoles);

    constexpr std::size_t ExpectedFontCount = GameFontRoleCount - 1;
    const std::size_t boundCount = boundRoles.count();
    if (boundCount != ExpectedFontCount) {
        m_Host->Log().Warn("Acquired %d of %d Game Fonts; missing roles use legacy indices",
                                   static_cast<int>(boundCount),
                                   static_cast<int>(ExpectedFontCount));
    }
}

void BMLMod::OnProcess_Menu() {
    m_Host->Menu().OnProcess();
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
    if (m_TimeManager)
        m_HUD.StartSRTimer(m_TimeManager->GetMainTickCount());
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

#include "Mods/BMLMod.h"

#include <algorithm>
#include <bitset>

#include "BML/Bui.h"
#include "BML/Gui.h"
#include "BML/ScriptHelper.h"
#include "BML/Guids/Interface.h"
#include "BML/Guids/TT_Toolbox_RT.h"

#include "Loader/ModContext.h"
#include "Config/Config.h"
#include "Hooks/RenderHook.h"
#include "Console/FontCommand.h"
#include "UI/AnsiPalette.h"
#include "UI/FontRuntime.h"
#include "UI/GameFontCatalog.h"
#if BML_ENABLE_UI_AUTOMATION
#include "UI/UiAutomation.h"
#endif
#include "Behavior/HookBlock.h"
#include "StringUtils.h"
#include "PathUtils.h"
#include "Api/BuiltinCapabilities.h"
#if BML_ENABLE_ANGELSCRIPT
#include "AngelScript/ScriptDevToolsService.h"
#endif

using namespace ScriptHelper;
namespace Ui = BML::UI;

namespace {
constexpr std::size_t GameFontRoleCount = static_cast<std::size_t>(BML::GameFont::Count);
constexpr unsigned long GameFontAcquireDelay = 1;

using BoundGameFontRoles = std::bitset<GameFontRoleCount>;

class GameFontCollector final {
public:
    GameFontCollector(BML::GameFontCatalog &catalog, BoundGameFontRoles &boundRoles)
        : m_Catalog(catalog), m_BoundRoles(boundRoles) {}

    bool operator()(CKBehavior *behavior) const {
        if (!behavior || behavior->GetInputParameterCount() < 1 || behavior->GetOutputParameterCount() < 1)
            return true;

        const auto *runtimeName = static_cast<const char *>(behavior->GetInputParameterReadDataPtr(0));
        int font = 0;
        BML::GameFont role = BML::GameFont::None;
        if (runtimeName &&
            behavior->GetOutputParameterValue(0, &font) == CK_OK &&
            m_Catalog.Bind(runtimeName, font, &role))
            m_BoundRoles.set(static_cast<std::size_t>(role));
        return true;
    }

private:
    BML::GameFontCatalog &m_Catalog;
    BoundGameFontRoles &m_BoundRoles;
};
} // namespace

ModContext *BMLMod::GetRuntimeContext() const {
    return dynamic_cast<ModContext *>(m_BML);
}

const BMLMod::Setting *BMLMod::GetSettings(size_t &count) {
    static const Setting settings[] = {
        {"GUI", "FontFilename", &BMLMod::m_FontFilename, &BMLMod::ApplyUiFontSetting, Startup | OnChange, false},
        {"GUI", "FontSize", &BMLMod::m_FontSize, &BMLMod::ApplyUiFontSetting, Startup | OnChange, false},
        {"GUI", "FontFallbacks", &BMLMod::m_FontFallbacks, &BMLMod::ApplyUiFontSetting, Startup | OnChange, false},
        {"GUI", "UseSystemFontFallbacks", &BMLMod::m_UseSystemFontFallbacks, &BMLMod::ApplyUiFontSetting, Startup | OnChange, false},
        {"GUI", "EnableIniSettings", &BMLMod::m_EnableIniSettings, nullptr, OnDemand, false},

        {"Graphics", "UnlockFrameRate", &BMLMod::m_UnlockFPS, &BMLMod::ApplyUnlockFrameRateSetting, OnChange | OnLevelInit, false},
        {"Graphics", "SetMaxFrameRate", &BMLMod::m_FPSLimit, &BMLMod::ApplyFrameRateLimitSetting, OnChange, false},
        {"Graphics", "WidescreenFix", &BMLMod::m_WidescreenFix, &BMLMod::ApplyWidescreenSetting, Startup | OnChange, false},

    };
    static_assert(sizeof(settings) / sizeof(settings[0]) == 8,
                  "Every BMLMod-owned config property must have one settings-table entry");

    count = sizeof(settings) / sizeof(settings[0]);
    return settings;
}

void BMLMod::BindSettings() {
    size_t count = 0;
    const Setting *settings = GetSettings(count);
    IConfig *config = GetConfig();
    for (size_t i = 0; i < count; ++i)
        this->*settings[i].property = config->GetProperty(settings[i].category, settings[i].key);
}

void BMLMod::ApplySetting(const Setting &setting, IProperty *property) {
    if (!setting.apply || !property)
        return;
    if (setting.requiresIngame && (!m_BML || !m_BML->IsIngame()))
        return;

    setting.apply(*this, property);
}

void BMLMod::ApplySettings(ApplyWhen when) {
    size_t count = 0;
    const Setting *settings = GetSettings(count);
    for (size_t i = 0; i < count; ++i) {
        if ((settings[i].when & static_cast<unsigned>(when)) == 0)
            continue;
        ApplySetting(settings[i], this->*settings[i].property);
    }
}

void BMLMod::OnLoad() {
    m_CKContext = m_BML->GetCKContext();
    m_TimeManager = m_BML->GetTimeManager();

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
    m_GameEventHooks.OnLoad(*m_BML, *GetLogger());
    m_GameplayTweaks.OnLoad(*m_BML, *GetLogger());
    m_Console.OnLoad(*m_BML, BML_GetModContext()->GetCommandContext(), *GetLogger(), m_HUD, GetFontCommandContext());

    m_HUD.OnLoad(*m_BML);

    if (ModContext *context = GetRuntimeContext())
        RegisterBuiltinCapabilities(*this, context->ObjectRefs(), GetLogger());
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

    Bui::CleanupResources(m_CKContext);

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
        BGui::Gui::InitMaterials();
        Bui::InitSounds(m_CKContext);
    }
}

void BMLMod::OnLoadScript(const char *filename, CKBehavior *script) {
    m_CustomMaps.OnLoadScript(script);

    if (!strcmp(script->GetName(), "Menu_Init"))
        OnEditScript_Menu_MenuInit(script);

    if (!strcmp(script->GetName(), "Menu_Options"))
        OnEditScript_Menu_OptionsMenu(script);
}

void BMLMod::OnProcess() {
    m_HUD.OnProcess(ImGui::GetIO().DeltaTime, m_TimeManager->GetLastDeltaTime());
    OnProcess_Menu();
    m_Console.OnProcess();
#if BML_ENABLE_ANGELSCRIPT
    if (auto *context = BML_GetModContext()) {
        if (auto *devTools = context->GetScriptDevTools())
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

void BMLMod::OnModifyConfig(const char *category, const char *key, IProperty *prop) {
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

    size_t count = 0;
    const Setting *settings = GetSettings(count);
    for (size_t i = 0; i < count; ++i) {
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
    m_HUD.OnMenuStart();
}

void BMLMod::OnPostStartMenu() {
    ApplyFrameRateSettings();
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
    m_ModMenu.Open("Mod List");
}

void BMLMod::CloseModsMenu() {
    m_ModMenu.Close();
}

void BMLMod::OpenMapMenu() {
    m_CustomMaps.Open();
}

void BMLMod::CloseMapMenu() {
    m_CustomMaps.Close();
}

int BMLMod::GetHSScore() {
    CKDataArray *energy = m_BML->GetArrayByName("Energy");
    if (!energy) return 0;
    int points = 0, lifes = 0;
    energy->GetElementValue(0, 0, &points);
    energy->GetElementValue(0, 1, &lifes);
    return points + lifes * 200;
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

void BMLMod::PrintHistory() {
    m_Console.PrintHistory();
}

void BMLMod::ClearHistory() {
    m_Console.ClearHistory();
}

void BMLMod::ExecuteHistory(int index) {
    m_Console.ExecuteHistory(index);
}

int BMLMod::GetHUD() {
    return m_HUD.GetMode();
}

void BMLMod::SetHUD(int mode) {
    m_HUD.SetMode(mode);
}

void BMLMod::InitConfigs() {
    Config *config = dynamic_cast<Config *>(GetConfig());
    const bool hadFallbackList = config && config->HasKey("GUI", "FontFallbacks");
    std::string migratedFallback;
    if (config && !hadFallbackList &&
        config->HasKey("GUI", "EnableSecondaryFont")) {
        IProperty *enabled = config->GetProperty("GUI", "EnableSecondaryFont");
        if (enabled && enabled->GetBoolean() &&
            config->HasKey("GUI", "SecondaryFontFilename")) {
            IProperty *filename = config->GetProperty("GUI", "SecondaryFontFilename");
            if (filename)
                migratedFallback = filename->GetString();
        }
    }

    if (config) {
        static const char *legacyFontKeys[] = {
            "FontRanges",
            "EnableSecondaryFont",
            "SecondaryFontFilename",
            "SecondaryFontSize",
            "SecondaryFontRanges",
        };
        for (const char *key : legacyFontKeys)
            config->RemoveProperty("GUI", key);
    }

    BindSettings();
    m_HUD.InitConfig(*GetConfig());
    m_Console.InitConfig(*GetConfig());
    m_CustomMaps.InitConfig(*GetConfig());
    m_GameplayTweaks.InitConfig(*GetConfig());

    GetConfig()->SetCategoryComment("GUI", "GUI Settings");

    m_FontFilename->SetComment("Primary UI font. Use a filename from ModLoader\\Fonts or an explicit TTF/OTF/TTC path.");
    m_FontFilename->SetDefaultString("unifont.otf");

    m_FontSize->SetComment("Logical UI font size at a 1200-pixel viewport height (8-96).");
    m_FontSize->SetDefaultFloat(32.0f);

    m_FontFallbacks->SetComment(
        "Optional fallback UI fonts, separated by semicolons. Each entry may be a filename from "
        "ModLoader\\Fonts or an explicit TTF/OTF/TTC path.");
    m_FontFallbacks->SetDefaultString("");
    if (!hadFallbackList && !migratedFallback.empty())
        m_FontFallbacks->SetString(migratedFallback.c_str());

    m_UseSystemFontFallbacks->SetComment("Use Windows symbol and emoji fonts after configured fonts.");
    m_UseSystemFontFallbacks->SetDefaultBoolean(true);

    m_EnableIniSettings->SetComment("Enable loading and saving ImGui settings.");
    m_EnableIniSettings->SetDefaultBoolean(true);

    GetConfig()->SetCategoryComment("Graphics", "Graphics Settings");

    m_UnlockFPS->SetComment("Unlock Frame Rate Limitation");
    m_UnlockFPS->SetDefaultBoolean(false);

    m_FPSLimit->SetComment("Set Frame Rate Limitation, this option will not work if frame rate is unlocked. Set to 0 will turn on VSync");
    m_FPSLimit->SetDefaultInteger(0);

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

FontCommandContext BMLMod::GetFontCommandContext() const {
    FontCommandContext commandContext;
    ModContext *modContext = GetRuntimeContext();
    commandContext.Runtime = modContext ? modContext->GetUiFontRuntime() : nullptr;
    commandContext.PrimaryFace = m_FontFilename;
    commandContext.ReferenceSize = m_FontSize;
    commandContext.FallbackFaces = m_FontFallbacks;
    commandContext.UseWindowsFallbacks = m_UseSystemFontFallbacks;
    return commandContext;
}

void BMLMod::ApplyUiFontSetting(BMLMod &mod, IProperty *) {
    mod.ConfigureUiFonts();
}

void BMLMod::ApplyUnlockFrameRateSetting(BMLMod &mod, IProperty *) {
    mod.ApplyFrameRateSettings();
}

void BMLMod::ApplyFrameRateLimitSetting(BMLMod &mod, IProperty *property) {
    if (mod.m_UnlockFPS->GetBoolean())
        return;

    const int limit = property->GetInteger();
    if (limit > 0)
        mod.AdjustFrameRate(false, static_cast<float>(limit));
    else
        mod.AdjustFrameRate(true);
}

void BMLMod::ApplyWidescreenSetting(BMLMod &, IProperty *property) {
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

    Bui::InitTextures(m_CKContext);
    Bui::InitMaterials(m_CKContext);

    m_ModMenu.Init();
    if (ModContext *context = GetRuntimeContext()) {
        m_CustomMaps.OnLoad(*m_BML, *GetLogger(),
                            context->GetDirectory(BML_DIR_LOADER),
                            context->GetDirectory(BML_DIR_TEMP));
    } else {
        GetLogger()->Error("Built-in Custom Maps requires the loader runtime context");
    }
}

void BMLMod::OnEditScript_Menu_MenuInit(CKBehavior *script) {
    (void) script;
    m_BML->AddTimer(GameFontAcquireDelay, [this]() { AcquireGameFonts(); });
}

void BMLMod::AcquireGameFonts() {
    GetLogger()->Info("Acquire Game Fonts");
    ModContext *context = GetRuntimeContext();
    if (!context) {
        GetLogger()->Warn("Cannot acquire Game Fonts without the loader runtime context");
        return;
    }

    BML::GameFontCatalog &catalog = context->GetGameFonts();
    catalog.Reset();

    CKBehavior *menuInit = m_BML->GetScriptByName("Menu_Init");
    if (!menuInit) {
        GetLogger()->Warn("Cannot acquire Game Fonts: Menu_Init was not found");
        return;
    }

    CKBehavior *fonts = FindFirstBB(menuInit, "Fonts");
    if (!fonts) {
        GetLogger()->Warn("Cannot acquire Game Fonts: Menu_Init/Fonts was not found");
        return;
    }

    BoundGameFontRoles boundRoles;
    FindBB(fonts, GameFontCollector(catalog, boundRoles), "TT CreateFontEx");

    constexpr std::size_t ExpectedFontCount = GameFontRoleCount - 1;
    const std::size_t boundCount = boundRoles.count();
    if (boundCount != ExpectedFontCount) {
        GetLogger()->Warn(
            "Acquired %d of %d Game Fonts; missing roles use legacy indices",
            static_cast<int>(boundCount),
            static_cast<int>(ExpectedFontCount));
    }
}

void BMLMod::OnEditScript_Menu_OptionsMenu(CKBehavior *script) {
    GetLogger()->Info("Start to insert Mods Button into Options Menu");

    char but_name[] = "M_Options_But_X";
    CK2dEntity *buttons[6] = {nullptr};
    buttons[0] = m_BML->Get2dEntityByName("M_Options_Title");
    for (int i = 1; i < 4; i++) {
        but_name[14] = '0' + i;
        buttons[i] = m_BML->Get2dEntityByName(but_name);
    }

    buttons[5] = m_BML->Get2dEntityByName("M_Options_But_Back");
    buttons[4] = (CK2dEntity *) m_CKContext->CopyObject(buttons[1]);
    buttons[4]->SetName("M_Options_But_4");
    for (int i = 0; i < 5; i++) {
        Vx2DVector pos;
        buttons[i]->GetPosition(pos, true);
        pos.y = 0.1f + 0.14f * i;
        buttons[i]->SetPosition(pos, true);
    }

    CKDataArray *array = m_BML->GetArrayByName("Menu_Options_ShowHide");
    array->InsertRow(3);
    array->SetElementObject(3, 0, buttons[4]);
    CKBOOL show = 1;
    array->SetElementValue(3, 1, &show, sizeof(show));
    m_BML->SetIC(array);

    CKBehavior *graph = FindFirstBB(script, "Options Menu");
    CKBehavior *up_sop = nullptr, *down_sop = nullptr, *up_ps = nullptr, *down_ps = nullptr;
    FindBB(graph, [graph, &up_sop, &down_sop](CKBehavior *beh) {
        CKBehavior *previous = FindPreviousBB(graph, beh);
        const char *name = previous->GetName();
        if (!strcmp(name, "Set 2D Material"))
            up_sop = beh;
        if (!strcmp(name, "Send Message"))
            down_sop = beh;
        return !(up_sop && down_sop);
    }, "Switch On Parameter");
    FindBB(graph, [graph, &up_ps, &down_ps](CKBehavior *beh) {
        CKBehavior *previous = FindNextBB(graph, beh);
        const char *name = previous->GetName();
        if (!strcmp(name, "Keyboard"))
            up_ps = beh;
        if (!strcmp(name, "Send Message"))
            down_ps = beh;
        return !(up_ps && down_ps);
    }, "Parameter Selector");

    CKParameterLocal *pin = CreateParamValue(graph, "Pin 5", CKPGUID_INT, 4);
    up_sop->CreateInputParameter("Pin 5", CKPGUID_INT)->SetDirectSource(pin);
    up_sop->AddOutput("Out 5");
    down_sop->CreateInputParameter("Pin 5", CKPGUID_INT)->SetDirectSource(pin);
    down_sop->AddOutput("Out 5");
    up_ps->CreateInputParameter("pIn 4", CKPGUID_INT)->SetDirectSource(pin);
    up_ps->AddInput("In 4");
    down_ps->CreateInputParameter("pIn 4", CKPGUID_INT)->SetDirectSource(pin);
    down_ps->AddInput("In 4");

    CKBehavior *text2d = CreateBB(graph, VT_INTERFACE_2DTEXT, true);
    CKBehavior *pushbutton = CreateBB(graph, TT_TOOLBOX_RT_TTPUSHBUTTON2, true);
    CKBehavior *text2dref = FindFirstBB(graph, "2D Text");
    CKBehavior *nop = FindFirstBB(graph, "Nop");
    CKParameterLocal *entity2d = CreateParamObject(graph, "Button", CKPGUID_2DENTITY, buttons[4]);
    CKParameterLocal *buttonname = CreateParamString(graph, "Text", "Mods");
    int textflags;
    text2dref->GetLocalParameterValue(0, &textflags);
    text2d->SetLocalParameterValue(0, &textflags, sizeof(textflags));

    text2d->GetTargetParameter()->SetDirectSource(entity2d);
    pushbutton->GetTargetParameter()->SetDirectSource(entity2d);
    text2d->GetInputParameter(0)->ShareSourceWith(text2dref->GetInputParameter(0));
    text2d->GetInputParameter(1)->SetDirectSource(buttonname);
    for (int i = 2; i < 6; i++)
        text2d->GetInputParameter(i)->ShareSourceWith(text2dref->GetInputParameter(i));

    FindNextLink(graph, up_sop, nullptr, 4, 0)->SetInBehaviorIO(up_sop->GetOutput(5));
    CreateLink(graph, up_sop, text2d, 4, 0);
    CreateLink(graph, text2d, nop, 0, 0);
    CreateLink(graph, text2d, pushbutton, 0, 0);
    FindPreviousLink(graph, up_ps, nullptr, 1, 3)->SetOutBehaviorIO(up_ps->GetInput(4));
    FindPreviousLink(graph, down_ps, nullptr, 2, 3)->SetOutBehaviorIO(down_ps->GetInput(4));
    CreateLink(graph, pushbutton, up_ps, 1, 3);
    CreateLink(graph, pushbutton, down_ps, 2, 3);
    graph->AddOutput("Button 5 Pressed");
    CreateLink(graph, down_sop, graph->GetOutput(4), 5);
    FindNextLink(script, graph, nullptr, 3, 0)->SetInBehaviorIO(graph->GetOutput(4));

    BML::Behavior::Internal::AttachResult hook = GetRuntimeContext()->Behaviors().AddToGraph(
        script, BML::Behavior::Internal::HookBlock::Make([](const CKBehaviorContext *, void *) -> int {
            BML_GetModContext()->OpenModsMenu();
            return CKBR_OK;
        }));
    CKBehavior *modsmenu = hook ? hook.Block : nullptr;
    CKBehavior *exit = FindFirstBB(script, "Exit", false, 1, 0);
    CreateLink(script, graph, modsmenu, 3, 0);
    CreateLink(script, modsmenu, exit, 0, 0);
    CKBehavior *keyboard = FindFirstBB(graph, "Keyboard");
    FindBB(keyboard, [keyboard](CKBehavior *beh) {
        CKParameter *endpoint = beh->GetInputParameter(0)->GetRealSource();
        if (GetParamValue<CKKEYBOARD>(endpoint) == CKKEY_ESCAPE) {
            CKBehavior *id = FindNextBB(keyboard, beh);
            SetParamValue(id->GetInputParameter(0)->GetRealSource(), 4);
            return false;
        }
        return true;
    }, "Secure Key");

    GetLogger()->Info("Mods Button inserted");
}

void BMLMod::OnProcess_Menu() {
    m_ModMenu.Render();
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

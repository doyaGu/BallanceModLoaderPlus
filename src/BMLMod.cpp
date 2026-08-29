#include "BMLMod.h"

#include <map>
#include <algorithm>
#include <random>

#include "BML/Bui.h"
#include "BML/Gui.h"
#include "BML/ExecuteBB.h"
#include "BML/ScriptHelper.h"
#include "BML/Guids/Logics.h"
#include "BML/Guids/Interface.h"
#include "BML/Guids/TT_Toolbox_RT.h"

#include "ModContext.h"
#include "RenderHook.h"
#include "AnsiPalette.h"
#include "StringUtils.h"
#include "PathUtils.h"
#include "EventHook.h"
#include "BuiltinCapabilities.h"
#if BML_ENABLE_ANGELSCRIPT
#include "AngelScript/ScriptDevToolsService.h"
#endif

namespace ExecuteBB {
    void Init();
    void InitFont(FontType type, int fontIndex);
}

using namespace ScriptHelper;

ModContext *BMLMod::GetRuntimeContext() const {
    return dynamic_cast<ModContext *>(m_BML);
}

const BMLMod::Setting *BMLMod::GetSettings(size_t &count) {
    static const Setting settings[] = {
        {"GUI", "FontFilename", &BMLMod::m_FontFilename, nullptr, OnDemand, false},
        {"GUI", "FontSize", &BMLMod::m_FontSize, nullptr, OnDemand, false},
        {"GUI", "FontRanges", &BMLMod::m_FontRanges, nullptr, OnDemand, false},
        {"GUI", "EnableSecondaryFont", &BMLMod::m_EnableSecondaryFont, nullptr, OnDemand, false},
        {"GUI", "SecondaryFontFilename", &BMLMod::m_SecondaryFontFilename, nullptr, OnDemand, false},
        {"GUI", "SecondaryFontSize", &BMLMod::m_SecondaryFontSize, nullptr, OnDemand, false},
        {"GUI", "SecondaryFontRanges", &BMLMod::m_SecondaryFontRanges, nullptr, OnDemand, false},
        {"GUI", "EnableIniSettings", &BMLMod::m_EnableIniSettings, nullptr, OnDemand, false},

        {"Graphics", "UnlockFrameRate", &BMLMod::m_UnlockFPS,
         [](BMLMod &mod, IProperty *) { mod.ApplyFrameRateSettings(); },
         OnChange | OnLevelInit, false},
        {"Graphics", "SetMaxFrameRate", &BMLMod::m_FPSLimit,
         [](BMLMod &mod, IProperty *property) {
             if (mod.m_UnlockFPS->GetBoolean())
                 return;
             const int limit = property->GetInteger();
             if (limit > 0)
                 mod.AdjustFrameRate(false, static_cast<float>(limit));
             else
                 mod.AdjustFrameRate(true);
         },
         OnChange, false},
        {"Graphics", "WidescreenFix", &BMLMod::m_WidescreenFix,
         [](BMLMod &, IProperty *property) { RenderHook::EnableWidescreenFix(property->GetBoolean()); },
         Startup | OnChange, false},

        {"Tweak", "LanternAlphaTest", &BMLMod::m_LanternAlphaTest,
         [](BMLMod &mod, IProperty *property) {
             CKMaterial *material = mod.m_BML->GetMaterialByName("Laterne_Verlauf");
             if (!material)
                 return;

             CKBOOL enabled = property->GetBoolean();
             material->EnableAlphaTest(enabled);
             VXCMPFUNC function = VXCMP_GREATEREQUAL;
             material->SetAlphaFunc(function);
             int reference = 0;
             material->SetAlphaRef(reference);
         },
         OnChange, false},
        {"Tweak", "FixLifeBallFreeze", &BMLMod::m_FixLifeBall, nullptr, OnDemand, false},
        {"Tweak", "Overclock", &BMLMod::m_Overclock,
         [](BMLMod &mod, IProperty *property) {
             const int target = property->GetBoolean() ? 1 : 0;
             for (int i = 0; i < 3; ++i) {
                 if (!mod.m_OverclockLinks[i] || !mod.m_OverclockLinkIO[i][target]) {
                     mod.GetLogger()->Warn("Overclock is unavailable for the current gameplay scripts");
                     return;
                 }
             }

             for (int i = 0; i < 3; ++i)
                 mod.m_OverclockLinks[i]->SetOutBehaviorIO(mod.m_OverclockLinkIO[i][target]);
         },
         OnChange, true},

        {"CustomMap", "LevelNumber", &BMLMod::m_CustomMapNumber, nullptr, OnDemand, false},
        {"CustomMap", "ShowTooltip", &BMLMod::m_CustomMapTooltip,
         [](BMLMod &mod, IProperty *property) { mod.m_MapMenu.SetShowTooltip(property->GetBoolean()); },
         Startup | OnChange, false},
        {"CustomMap", "MaxDepth", &BMLMod::m_CustomMapMaxDepth,
         [](BMLMod &mod, IProperty *property) { mod.m_MapMenu.SetMaxDepth(property->GetInteger()); },
         Startup | OnChange, false},
    };
    static_assert(sizeof(settings) / sizeof(settings[0]) == 17,
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

static ImFont *LoadFont(const char *filename, float size, const char *ranges, bool merge = false) {
    ImGuiIO &io = ImGui::GetIO();

    // Normalize size and defaults
    if (size <= 0)
        size = 32.0f;

    auto AddDefaultFont = [&]() -> ImFont * {
        ImFontConfig cfg;
        cfg.SizePixels = size;
        cfg.MergeMode = false;
        return io.Fonts->AddFontDefault(&cfg);
    };

    if (!filename || filename[0] == '\0') {
        return AddDefaultFont();
    }

    std::string path = filename;
    if (!utils::FileExistsUtf8(path)) {
        path = BML_GetModContext()->GetDirectoryUtf8(BML_DIR_LOADER);
        path.append("\\Fonts\\").append(filename);
    }

    if (!utils::FileExistsUtf8(path)) {
        return AddDefaultFont();
    }

    const ImWchar *glyphRanges = nullptr;
    if (strnicmp(ranges, "ChineseFull", 11) == 0) {
        glyphRanges = io.Fonts->GetGlyphRangesChineseFull();
    } else if (strnicmp(ranges, "Chinese", 7) == 0) {
        glyphRanges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    } else if (strnicmp(ranges, "Cyrillic", 8) == 0) {
        glyphRanges = io.Fonts->GetGlyphRangesCyrillic();
    } else if (strnicmp(ranges, "Greek", 5) == 0) {
        glyphRanges = io.Fonts->GetGlyphRangesGreek();
    } else if (strnicmp(ranges, "Korean", 6) == 0) {
        glyphRanges = io.Fonts->GetGlyphRangesKorean();
    } else if (strnicmp(ranges, "Japanese", 8) == 0) {
        glyphRanges = io.Fonts->GetGlyphRangesJapanese();
    } else if (strnicmp(ranges, "Thai", 4) == 0) {
        glyphRanges = io.Fonts->GetGlyphRangesThai();
    } else if (strnicmp(ranges, "Vietnamese", 10) == 0) {
        glyphRanges = io.Fonts->GetGlyphRangesVietnamese();
    }

    ImFontConfig config;
    config.SizePixels = size;
    config.MergeMode = merge;

    // Safety: if asked to merge but no base font exists, disable merge so we don't end up merging into "nothing".
    if (config.MergeMode && io.Fonts->Fonts.empty())
        config.MergeMode = false;

    ImFont *font = io.Fonts->AddFontFromFileTTF(path.c_str(), size, &config, glyphRanges);
    if (!font)
        return AddDefaultFont();

    return font;
}

static std::string CreateTempMapFile(const std::wstring &path) {
    if (path.empty() || !utils::FileExistsW(path))
        return "";

    std::wstring fileName = utils::GetFileNameW(path);
    std::wstring fileNameWithoutExt = utils::RemoveExtensionW(fileName);
    std::wstring extension = utils::GetExtensionW(path);

    size_t hash = utils::HashString(fileNameWithoutExt);

    std::wstring tempDir = BML_GetModContext()->GetDirectory(BML_DIR_TEMP);
    std::wstring mapsDir = utils::CombinePathW(tempDir, L"Maps");

    if (!utils::DirectoryExistsW(mapsDir))
        utils::CreateDirectoryW(mapsDir);

    wchar_t hashStr[9];
    swprintf(hashStr, sizeof(hashStr) / sizeof(wchar_t), L"%08X", static_cast<unsigned int>(hash));

    std::wstring destPath = utils::CombinePathW(mapsDir, std::wstring(hashStr) + extension);

    if (!utils::CopyFileW(path, destPath))
        return "";

    return utils::Utf16ToAnsi(destPath);
}

void BMLMod::OnLoad() {
    m_DataShare = BML_GetDataShare(nullptr);
    m_CKContext = m_BML->GetCKContext();
    m_RenderContext = m_BML->GetRenderContext();
    m_TimeManager = m_BML->GetTimeManager();
    m_RenderContext->Get2dRoot(TRUE)->GetRect(m_WindowRect);

    // Configure AnsiPalette to use the ModLoader directory for config/themes
    AnsiPalette::SetLoaderDirProvider([]() -> std::wstring {
        return BML_GetModContext()->GetDirectory(BML_DIR_LOADER);
    });

    ExecuteBB::Init();

    InitConfigs();
    ApplySettings(Startup);
    m_HUD.ApplyConfig();
    m_Console.ApplyConfig();
    InitGUI();
    m_Console.OnLoad(*m_BML, BML_GetModContext()->GetCommandContext(), *GetLogger(), this);

    m_HUD.OnLoad(*m_BML);

    if (ModContext *context = GetRuntimeContext())
        RegisterBuiltinCapabilities(*this, context->ObjectIdentities(), GetLogger());
}

void BMLMod::OnUnload() {
    UnregisterBuiltinCapabilities(*this);

    m_Console.OnUnload();
    m_HUD.OnUnload();

    Bui::CleanupResources(m_CKContext);

    if (m_EnableIniSettings->GetBoolean()) {
        ImGui::SaveIniSettingsToDisk(m_ImGuiIniFilename.c_str());
    }


    // Reset pointers to prevent use-after-free
    m_Level01 = nullptr;
    m_ExitStart = nullptr;
    m_TimeManager = nullptr;
    m_RenderContext = nullptr;

    // Clear containers
    m_WindowRect = VxRect();
    m_OldWindowRect = VxRect();
}

void BMLMod::OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                          CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                          XObjectArray *objArray, CKObject *masterObj) {
    if (!strcmp(filename, "3D Entities\\Menu.nmo")) {
        BGui::Gui::InitMaterials();
        Bui::InitSounds(m_CKContext);

        m_Level01 = m_BML->Get2dEntityByName("M_Start_But_01");
        CKBehavior *menuMain = m_BML->GetScriptByName("Menu_Start");
        m_ExitStart = FindFirstBB(menuMain, "Exit");
    }
}

void BMLMod::OnLoadScript(const char *filename, CKBehavior *script) {
    if (!strcmp(script->GetName(), "Event_handler"))
        OnEditScript_Base_EventHandler(script);

    if (!strcmp(script->GetName(), "Menu_Init"))
        OnEditScript_Menu_MenuInit(script);

    if (!strcmp(script->GetName(), "Menu_Options"))
        OnEditScript_Menu_OptionsMenu(script);

    if (!strcmp(script->GetName(), "Gameplay_Ingame"))
        OnEditScript_Gameplay_Ingame(script);

    if (!strcmp(script->GetName(), "Gameplay_Energy"))
        OnEditScript_Gameplay_Energy(script);

    if (!strcmp(script->GetName(), "Gameplay_Events"))
        OnEditScript_Gameplay_Events(script);

    if (!strcmp(script->GetName(), "Levelinit_build"))
        OnEditScript_Levelinit_build(script);

    if (m_FixLifeBall->GetBoolean()) {
        if (!strcmp(script->GetName(), "P_Extra_Life_Particle_Blob Script") ||
            !strcmp(script->GetName(), "P_Extra_Life_Particle_Fizz Script"))
            OnEditScript_ExtraLife_Fix(script);
    }
}

void BMLMod::OnProcess() {
    m_OldWindowRect = m_WindowRect;
    m_RenderContext->Get2dRoot(TRUE)->GetRect(m_WindowRect);
    if (m_WindowRect != m_OldWindowRect) {
        OnResize();
    }

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
}

void BMLMod::OnExitGame() {
    m_Level01 = nullptr;
#ifndef NDEBUG
    m_ShowImGuiDemo = false;
#endif
}

void BMLMod::OnStartLevel() {
    ApplySettings(OnLevelInit);

    m_HUD.OnLevelStart();
    SetParamValue(m_LoadCustom, FALSE);
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
    m_MapMenu.Open("Custom Maps");
}

void BMLMod::CloseMapMenu() {
    m_MapMenu.Close();
}

void BMLMod::LoadMap(const std::wstring &path) {
    if (path.empty())
        return;

    std::string filename = CreateTempMapFile(path);
    SetParamString(m_MapFile, filename.c_str());
    SetParamValue(m_LoadCustom, TRUE);
    int level = m_CustomMapNumber->GetInteger();
    if (level < 1 || level > 13) {
        static std::mt19937 s_Rng(std::random_device{}());
        level = std::uniform_int_distribution<int>(2, 11)(s_Rng);
    }
    m_CurLevel->SetElementValue(0, 0, &level);
    level--;
    SetParamValue(m_LevelRow, level);

    std::string mapPath = utils::ToString(path);
    BML_DataShare_Set(m_DataShare, "CustomMapName", mapPath.c_str(), mapPath.size() + 1);

    CKMessageManager *mm = m_CKContext->GetMessageManager();
    CKMessageType loadLevel = mm->AddMessageType((CKSTRING) "Load Level");
    CKMessageType loadMenu = mm->AddMessageType((CKSTRING) "Menu_Load");

    mm->SendMessageSingle(loadLevel, m_CKContext->GetCurrentLevel());
    mm->SendMessageSingle(loadMenu, m_BML->GetGroupByName("All_Sound"));
    m_BML->Get2dEntityByName("M_BlackScreen")->Show(CKHIDE);
    m_ExitStart->ActivateInput(0);
    m_ExitStart->Activate();
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
    BindSettings();
    m_HUD.InitConfig(*GetConfig());
    m_Console.InitConfig(*GetConfig());

    GetConfig()->SetCategoryComment("GUI", "GUI Settings");

    m_FontFilename->SetComment("The filename of TrueType font (the font filename should end with .ttf or .otf)");
    m_FontFilename->SetDefaultString("unifont.otf");

    m_FontSize->SetComment("The size of font (pixel).");
    m_FontSize->SetDefaultFloat(32.0f);

    m_FontRanges->SetComment("The Unicode ranges of font glyph."
                             " To display Chinese characters correctly, this option should be set to Chinese or ChineseFull");
    m_FontRanges->SetDefaultString("ChineseFull");

    m_EnableSecondaryFont->SetComment("Enable secondary font.");
    m_EnableSecondaryFont->SetDefaultBoolean(false);

    m_SecondaryFontFilename->SetComment("The filename of secondary font (the font filename should end with .ttf or .otf)");
    m_SecondaryFontFilename->SetDefaultString("unifont.otf");

    m_SecondaryFontSize->SetComment("The size of secondary font (pixel).");
    m_SecondaryFontSize->SetDefaultFloat(32.0f);

    m_SecondaryFontRanges->SetComment("The Unicode ranges of secondary font glyph."
                                      " To display Chinese characters correctly, this option should be set to Chinese or ChineseFull");
    m_SecondaryFontRanges->SetDefaultString("ChineseFull");

    m_EnableIniSettings->SetComment("Enable loading and saving ImGui settings.");
    m_EnableIniSettings->SetDefaultBoolean(true);

    GetConfig()->SetCategoryComment("Graphics", "Graphics Settings");

    m_UnlockFPS->SetComment("Unlock Frame Rate Limitation");
    m_UnlockFPS->SetDefaultBoolean(false);

    m_FPSLimit->SetComment("Set Frame Rate Limitation, this option will not work if frame rate is unlocked. Set to 0 will turn on VSync");
    m_FPSLimit->SetDefaultInteger(0);

    m_WidescreenFix->SetComment("Improve widescreen resolutions support");
    m_WidescreenFix->SetDefaultBoolean(false);

    GetConfig()->SetCategoryComment("Tweak", "Tweak Settings");

    m_LanternAlphaTest->SetComment("Enable alpha test for lantern material, this option can increase FPS");
    m_LanternAlphaTest->SetDefaultBoolean(true);

    m_FixLifeBall->SetComment("Game won't freeze when picking up life balls");
    m_FixLifeBall->SetDefaultBoolean(true);

    m_Overclock->SetComment("Remove delay of spawn / respawn");
    m_Overclock->SetDefaultBoolean(false);

    GetConfig()->SetCategoryComment("CustomMap", "Custom Map Settings");

    m_CustomMapNumber->SetComment("Level number to use for custom maps (affects level bonus and sky textures)."
                                  " Must be in the range of 1~13; 0 to randomly select one between 2 and 11");
    m_CustomMapNumber->SetDefaultInteger(0);

    m_CustomMapTooltip->SetComment("Show custom map's full name in tooltip");
    m_CustomMapTooltip->SetDefaultBoolean(false);

    m_CustomMapMaxDepth->SetComment("The max depth of the nested subdirectories.");
    m_CustomMapMaxDepth->SetDefaultInteger(8);
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

    // 1.92: FontGlobalScale moved to style.FontScaleMain
    ImGui::GetStyle().FontScaleMain = m_WindowRect.GetHeight() / 1200.0f;

    LoadFont(m_FontFilename->GetString(), m_FontSize->GetFloat(), m_FontRanges->GetString());
    if (m_EnableSecondaryFont->GetBoolean()) {
        if (strcmp(m_FontFilename->GetString(), m_SecondaryFontFilename->GetString()) != 0 ||
            strcmp(m_FontRanges->GetString(), m_SecondaryFontRanges->GetString()) != 0) {
            LoadFont(m_SecondaryFontFilename->GetString(), m_SecondaryFontSize->GetFloat(), m_SecondaryFontRanges->GetString(), true);
        }
    }
    io.Fonts->Build();

    Bui::InitTextures(m_CKContext);
    Bui::InitMaterials(m_CKContext);

    m_ModMenu.Init();
    m_MapMenu.Init();
}

void BMLMod::OnEditScript_Base_EventHandler(CKBehavior *script) {
    if (!m_BML) return;
    EventHookRegistrar registrar(*this, *m_BML);
    registrar.RegisterBaseEventHandler(script);
}

void BMLMod::OnEditScript_Menu_MenuInit(CKBehavior *script) {
    m_BML->AddTimer(1ul, [this]() {
        GetLogger()->Info("Acquire Game Fonts");
        CKBehavior *script = m_BML->GetScriptByName("Menu_Init");
        CKBehavior *fonts = FindFirstBB(script, "Fonts");
        CKBehavior *bbs[7] = {nullptr};
        int cnt = 0;
        FindBB(fonts, [&bbs, &cnt](CKBehavior *beh) {
            bbs[cnt++] = beh;
            return true;
        }, "TT CreateFontEx");

        std::map<std::string, ExecuteBB::FontType> fontid;
        fontid["GameFont_01"] = ExecuteBB::GAMEFONT_01;
        fontid["GameFont_02"] = ExecuteBB::GAMEFONT_02;
        fontid["GameFont_03"] = ExecuteBB::GAMEFONT_03;
        fontid["GameFont_03a"] = ExecuteBB::GAMEFONT_03A;
        fontid["GameFont_04"] = ExecuteBB::GAMEFONT_04;
        fontid["GameFont_Credits_Small"] = ExecuteBB::GAMEFONT_CREDITS_SMALL;
        fontid["GameFont_Credits_Big"] = ExecuteBB::GAMEFONT_CREDITS_BIG;

        for (int i = 0; i < 7; i++) {
            int font = 0;
            bbs[i]->GetOutputParameterValue(0, &font);
            ExecuteBB::InitFont(fontid[static_cast<const char *>(bbs[i]->GetInputParameterReadDataPtr(0))], font);
        }
    });
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

    CKBehavior *modsmenu = ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OpenModsMenu();
        return CKBR_OK;
    });
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

void BMLMod::OnEditScript_Gameplay_Ingame(CKBehavior *script) {
    if (!m_BML) return;
    EventHookRegistrar registrar(*this, *m_BML);
    registrar.RegisterGameplayIngame(script);
}

void BMLMod::OnEditScript_Gameplay_Energy(CKBehavior *script) {
    if (!m_BML) return;
    EventHookRegistrar registrar(*this, *m_BML);
    registrar.RegisterGameplayEnergy(script);
}

void BMLMod::OnEditScript_Gameplay_Events(CKBehavior *script) {
    if (!m_BML) return;
    EventHookRegistrar registrar(*this, *m_BML);
    registrar.RegisterGameplayEvents(script);
}

void BMLMod::OnEditScript_Levelinit_build(CKBehavior *script) {
    CKBehavior *loadLevel = FindFirstBB(script, "Load LevelXX");
    CKBehaviorLink *inLink = FindNextLink(loadLevel, loadLevel->GetInput(0));
    CKBehavior *op = FindNextBB(loadLevel, inLink->GetOutBehaviorIO()->GetOwner());
    m_LevelRow = op->GetOutputParameter(0)->GetDestination(0);
    CKBehavior *objLoad = FindFirstBB(loadLevel, "Object Load");
    CKBehavior *bin = CreateBB(loadLevel, VT_LOGICS_BINARYSWITCH);
    CreateLink(loadLevel, loadLevel->GetInput(0), bin, 0);
    m_LoadCustom = CreateLocalParameter(loadLevel, "Custom Level", CKPGUID_BOOL);
    bin->GetInputParameter(0)->SetDirectSource(m_LoadCustom);
    inLink->SetInBehaviorIO(bin->GetOutput(1));
    CreateLink(loadLevel, bin, objLoad);
    m_MapFile = objLoad->GetInputParameter(0)->GetDirectSource();

    CKBehavior *smat = FindFirstBB(script, "set Mapping and Textures");
    if (!smat) return;
    CKBehavior *sml = FindFirstBB(smat, "Set Mat Laterne");
    if (!sml) return;
    CKBehavior *sat = FindFirstBB(sml, "Set Alpha Test");
    if (!sat) return;

    CKParameter *sate = sat->GetInputParameter(0)->GetDirectSource();
    if (sate) {
        CKBOOL atest = m_LanternAlphaTest->GetBoolean();
        sate->SetValue(&atest);
    }
}

void BMLMod::OnEditScript_ExtraLife_Fix(CKBehavior *script) {
    CKBehavior *emitter = FindFirstBB(script, "SphericalParticleSystem");
    auto *rtm = emitter->CreateInputParameter("Real-Time Mode", CKPGUID_BOOL);
    if (rtm) rtm->SetDirectSource(CreateParamValue<CKBOOL>(script, "Real-Time Mode", CKPGUID_BOOL, 1));
    auto *dt = emitter->CreateInputParameter("DeltaTime", CKPGUID_FLOAT);
    if (dt) dt->SetDirectSource(CreateParamValue<float>(script, "DeltaTime", CKPGUID_FLOAT, 20.0f));
}

void BMLMod::OnProcess_Menu() {
    if (m_Level01 && m_Level01->IsVisible()) {
        const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::SetNextWindowPos(ImVec2(vpSize.x * 0.6238f, vpSize.y * 0.4f));

        constexpr ImGuiWindowFlags ButtonFlags = ImGuiWindowFlags_NoDecoration |
                                                 ImGuiWindowFlags_NoBackground |
                                                 ImGuiWindowFlags_NoMove |
                                                 ImGuiWindowFlags_NoNav |
                                                 ImGuiWindowFlags_AlwaysAutoResize |
                                                 ImGuiWindowFlags_NoFocusOnAppearing |
                                                 ImGuiWindowFlags_NoBringToFrontOnFocus |
                                                 ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin("Button_Custom_Maps", nullptr, ButtonFlags)) {
            if (Bui::RightButton("Enter_Custom_Maps")) {
                m_ExitStart->ActivateInput(0);
                m_ExitStart->Activate();
                OpenMapMenu();
            }
        }
        ImGui::End();

        ImGui::PopStyleVar(2);
    }

    m_ModMenu.Render();
    m_MapMenu.Render();
}

void BMLMod::OnResize() {
    ImGuiStyle &style = ImGui::GetStyle();
    style.FontScaleMain = m_WindowRect.GetHeight() / 1200.0f;
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

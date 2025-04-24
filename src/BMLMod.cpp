#include "BMLMod.h"

#include <map>

#include "BML/Bui.h"
#include "BML/Gui.h"
#include "BML/ExecuteBB.h"
#include "BML/ScriptHelper.h"
#include "BML/Guids/Logics.h"
#include "BML/Guids/Interface.h"
#include "BML/Guids/TT_Toolbox_RT.h"

#include "ModContext.h"
#include "RenderHook.h"
#include "Commands.h"
#include "StringUtils.h"
#include "PathUtils.h"

namespace ExecuteBB {
    void Init();
    void InitFont(FontType type, int fontIndex);
}

using namespace ScriptHelper;

static ImFont *LoadFont(const char *filename, float size, const char *ranges, bool merge = false) {
    ImGuiIO &io = ImGui::GetIO();

    if (!filename || filename[0] == '\0') {
        ImFontConfig config;
        config.SizePixels = 32.0f;
        return io.Fonts->AddFontDefault(&config);
    }

    std::string path = filename;
    if (!utils::FileExistsUtf8(path)) {
        path = BML_GetModContext()->GetDirectoryUtf8(BML_DIR_LOADER);
        path.append("\\Fonts\\").append(filename);
    }

    if (!utils::FileExistsUtf8(path)) {
        ImFontConfig config;
        config.SizePixels = 32.0f;
        return io.Fonts->AddFontDefault(&config);
    }

    if (size <= 0)
        size = 32.0f;

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
    if (strnicmp(ranges, "Default", 7) != 0) {
        // Set OversampleH/OversampleV to 1 to reduce the texture size.
        config.OversampleH = config.OversampleV = 1;
        config.PixelSnapH = true;
    }

    return io.Fonts->AddFontFromFileTTF(path.c_str(), size, &config, glyphRanges);
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

    ExecuteBB::Init();

    InitConfigs();
    InitGUI();

    auto &cc = BML_GetModContext()->GetCommandContext();
    cc.SetOutputCallback([](const char *msg, void *userdata) {
        auto *mod = static_cast<BMLMod *>(userdata);
        mod->AddIngameMessage(msg);
    }, this);

    RegisterCommands();

    m_CommandBar.LoadHistory();
}

void BMLMod::OnUnload() {
    m_CommandBar.SaveHistory();
    m_ModMenu.Shutdown();
    m_MapMenu.Shutdown();

    auto &cc = BML_GetModContext()->GetCommandContext();
    cc.ClearOutputCallback();

    if (m_EnableIniSettings->GetBoolean()) {
        ImGui::SaveIniSettingsToDisk(m_ImGuiIniFilename.c_str());
    } else if (utils::FileExistsUtf8(m_ImGuiIniFilename)) {
        utils::DeleteFileUtf8(m_ImGuiIniFilename);
    }
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

    if (m_FixLifeBall) {
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

    OnProcess_HUD();
    OnProcess_CommandBar();
    OnProcess_Menu();

#ifndef NDEBUG
    if (ImGui::IsKeyChordPressed(ImGuiMod_Shift | ImGuiMod_Alt | ImGuiKey_F5))
        m_ShowImGuiDemo = !m_ShowImGuiDemo;
    if (m_ShowImGuiDemo)
        ImGui::ShowDemoWindow(&m_ShowImGuiDemo);
#endif
}

void BMLMod::OnModifyConfig(const char *category, const char *key, IProperty *prop) {
    if (prop == m_UnlockFPS) {
        if (prop->GetBoolean()) {
            AdjustFrameRate(false, 0);
        } else {
            const int val = m_FPSLimit->GetInteger();
            if (val > 0)
                AdjustFrameRate(false, static_cast<float>(val));
            else
                AdjustFrameRate(true);
        }
    } else if (prop == m_FPSLimit && !m_UnlockFPS->GetBoolean()) {
        const int val = prop->GetInteger();
        if (val > 0)
            AdjustFrameRate(false, static_cast<float>(val));
        else
            AdjustFrameRate(true);
    } else if (prop == m_ShowTitle) {
        m_HUD.ShowTitle(m_ShowTitle->GetBoolean());
    } else if (prop == m_ShowFPS) {
        m_HUD.ShowFPS(m_ShowFPS->GetBoolean());
    } else if (prop == m_ShowSR && m_BML->IsIngame()) {
        m_HUD.ShowSRTimer(m_ShowSR->GetBoolean());
    } else if (prop == m_WidescreenFix) {
        RenderHook::EnableWidescreenFix(m_WidescreenFix->GetBoolean());
    } else if (prop == m_LanternAlphaTest) {
        CKMaterial *mat = m_BML->GetMaterialByName("Laterne_Verlauf");
        if (mat) {
            CKBOOL atest = m_LanternAlphaTest->GetBoolean();
            mat->EnableAlphaTest(atest);

            VXCMPFUNC afunc = VXCMP_GREATEREQUAL;
            mat->SetAlphaFunc(afunc);

            int ref = 0;
            mat->SetAlphaRef(ref);
        }
    } else if (prop == m_Overclock) {
        for (int i = 0; i < 3; i++)
            m_OverclockLinks[i]->SetOutBehaviorIO(m_OverclockLinkIO[i][m_Overclock->GetBoolean()]);
    } else if (prop == m_MsgDuration) {
        const float timer = m_MsgDuration->GetFloat() * 1000;
        m_MessageBoard.SetMaxTimer(timer);
        if (timer < 2000) {
            m_MsgDuration->SetFloat(2.0f);
        }
    } else if (prop == m_MsgCapability) {
        m_MessageBoard.ResizeMessages(m_MsgCapability->GetInteger());
    } else if (prop == m_CustomMapTooltip) {
        m_MapMenu.SetShowTooltip(m_CustomMapTooltip->GetBoolean());
    }
}

void BMLMod::OnPreStartMenu() {
    RenderHook::EnableWidescreenFix(m_WidescreenFix->GetBoolean());

    m_HUD.ShowTitle(m_ShowTitle->GetBoolean());
    m_HUD.ShowFPS(m_ShowFPS->GetBoolean());
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
    ApplyFrameRateSettings();

    m_HUD.ResetSRTimer();
    m_HUD.ShowSRTimer(m_ShowSR->GetBoolean());
    SetParamValue(m_LoadCustom, FALSE);
}

void BMLMod::OnPostExitLevel() {
    m_HUD.ShowSRTimer(false);
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
    m_MessageBoard.Show();
    m_MessageBoard.AddMessage(msg);
    GetLogger()->Info(msg);
}

void BMLMod::ClearIngameMessages() {
    m_MessageBoard.ClearMessages();
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
    level = (level >= 1 && level <= 13) ? level : rand() % 10 + 2;
    m_CurLevel->SetElementValue(0, 0, &level);
    level--;
    SetParamValue(m_LevelRow, level);

    std::string mapPath = utils::ToString(path);
    m_DataShare->Set("CustomMapName", mapPath.c_str(), mapPath.size() + 1);

    CKMessageManager *mm = m_CKContext->GetMessageManager();
    CKMessageType loadLevel = mm->AddMessageType((CKSTRING) "Load Level");
    CKMessageType loadMenu = mm->AddMessageType((CKSTRING) "Menu_Load");

    mm->SendMessageSingle(loadLevel, m_CKContext->GetCurrentLevel());
    mm->SendMessageSingle(loadMenu, m_BML->GetGroupByName("All_Sound"));
    m_BML->Get2dEntityByName("M_BlackScreen")->Show(CKHIDE);
    m_ExitStart->ActivateInput(0);
    m_ExitStart->Activate();
}

float BMLMod::GetSRScore() const {
    return m_HUD.GetSRTime();
}

int BMLMod::GetHSScore() {
    int points, lifes;
    CKDataArray *energy = m_BML->GetArrayByName("Energy");
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
    m_CommandBar.PrintHistory();
}

void BMLMod::ClearHistory() {
    m_CommandBar.ClearHistory();
}

void BMLMod::ExecuteHistory(int index) {
    m_CommandBar.ExecuteHistory(index);
}

int BMLMod::GetHUD() {
    int code = 0;
    if (m_ShowTitle->GetBoolean()) {
        code |= HUD_TITLE;
    }
    if (m_ShowFPS->GetBoolean()) {
        code |= HUD_FPS;
    }
    if (m_ShowSR->GetBoolean()) {
        code |= HUD_SR;
    }
    return code;
}

void BMLMod::SetHUD(int mode) {
    m_ShowTitle->SetBoolean((mode & HUD_TITLE) != 0);
    m_HUD.ShowTitle(m_ShowTitle->GetBoolean());

    m_ShowFPS->SetBoolean((mode & HUD_FPS) != 0);
    m_HUD.ShowFPS(m_ShowFPS->GetBoolean());

    m_ShowSR->SetBoolean((mode & HUD_SR) != 0);
    m_HUD.ShowSRTimer(m_ShowSR->GetBoolean());
}

void BMLMod::InitConfigs() {
    GetConfig()->SetCategoryComment("GUI", "GUI Settings");

    m_FontFilename = GetConfig()->GetProperty("GUI", "FontFilename");
    m_FontFilename->SetComment("The filename of TrueType font (the font filename should end with .ttf or .otf)");
    m_FontFilename->SetDefaultString("unifont.otf");

    m_FontSize = GetConfig()->GetProperty("GUI", "FontSize");
    m_FontSize->SetComment("The size of font (pixel).");
    m_FontSize->SetDefaultFloat(32.0f);

    m_FontRanges = GetConfig()->GetProperty("GUI", "FontRanges");
    m_FontRanges->SetComment("The Unicode ranges of font glyph."
                             " To display Chinese characters correctly, this option should be set to Chinese or ChineseFull");
    m_FontRanges->SetDefaultString("Default");

    m_EnableSecondaryFont = GetConfig()->GetProperty("GUI", "EnableSecondaryFont");
    m_EnableSecondaryFont->SetComment("Enable secondary font.");
    m_EnableSecondaryFont->SetDefaultBoolean(false);

    m_SecondaryFontFilename = GetConfig()->GetProperty("GUI", "SecondaryFontFilename");
    m_SecondaryFontFilename->SetComment("The filename of secondary font (the font filename should end with .ttf or .otf)");
    m_SecondaryFontFilename->SetDefaultString("unifont.otf");

    m_SecondaryFontSize = GetConfig()->GetProperty("GUI", "SecondaryFontSize");
    m_SecondaryFontSize->SetComment("The size of secondary font (pixel).");
    m_SecondaryFontSize->SetDefaultFloat(32.0f);

    m_SecondaryFontRanges = GetConfig()->GetProperty("GUI", "SecondaryFontRanges");
    m_SecondaryFontRanges->SetComment("The Unicode ranges of secondary font glyph."
                                      " To display Chinese characters correctly, this option should be set to Chinese or ChineseFull");
    m_SecondaryFontRanges->SetDefaultString("Default");

    m_EnableIniSettings = GetConfig()->GetProperty("GUI", "EnableIniSettings");
    m_EnableIniSettings->SetComment("Enable loading and saving ImGui settings.");
    m_EnableIniSettings->SetDefaultBoolean(true);

    GetConfig()->SetCategoryComment("HUD", "HUD Settings");

    m_ShowTitle = GetConfig()->GetProperty("HUD", "ShowTitle");
    m_ShowTitle->SetComment("Show BML Title at top");
    m_ShowTitle->SetDefaultBoolean(true);

    m_ShowFPS = GetConfig()->GetProperty("HUD", "ShowFPS");
    m_ShowFPS->SetComment("Show FPS at top-left corner");
    m_ShowFPS->SetDefaultBoolean(true);

    m_ShowSR = GetConfig()->GetProperty("HUD", "ShowSRTimer");
    m_ShowSR->SetComment("Show SR Timer above Time Score");
    m_ShowSR->SetDefaultBoolean(true);

    GetConfig()->SetCategoryComment("Graphics", "Graphics Settings");

    m_UnlockFPS = GetConfig()->GetProperty("Graphics", "UnlockFrameRate");
    m_UnlockFPS->SetComment("Unlock Frame Rate Limitation");
    m_UnlockFPS->SetDefaultBoolean(false);

    m_FPSLimit = GetConfig()->GetProperty("Graphics", "SetMaxFrameRate");
    m_FPSLimit->SetComment("Set Frame Rate Limitation, this option will not work if frame rate is unlocked. Set to 0 will turn on VSync");
    m_FPSLimit->SetDefaultInteger(0);

    m_WidescreenFix = GetConfig()->GetProperty("Graphics", "WidescreenFix");
    m_WidescreenFix->SetComment("Improve widescreen resolutions support");
    m_WidescreenFix->SetDefaultBoolean(false);

    GetConfig()->SetCategoryComment("Tweak", "Tweak Settings");

    m_LanternAlphaTest = GetConfig()->GetProperty("Tweak", "LanternAlphaTest");
    m_LanternAlphaTest->SetComment("Enable alpha test for lantern material, this option can increase FPS");
    m_LanternAlphaTest->SetDefaultBoolean(true);

    m_FixLifeBall = GetConfig()->GetProperty("Tweak", "FixLifeBallFreeze");
    m_FixLifeBall->SetComment("Game won't freeze when picking up life balls");
    m_FixLifeBall->SetDefaultBoolean(true);

    m_Overclock = GetConfig()->GetProperty("Tweak", "Overclock");
    m_Overclock->SetComment("Remove delay of spawn / respawn");
    m_Overclock->SetDefaultBoolean(false);

    GetConfig()->SetCategoryComment("CommandBar", "Command Bar Settings");

    m_MsgCapability = GetConfig()->GetProperty("CommandBar", "MessageCapability");
    m_MsgCapability->SetComment("The maximum number of messages that can be displayed at the same time (default: 35)");
    m_MsgCapability->SetDefaultInteger(35);
    m_MessageBoard.ResizeMessages(m_MsgCapability->GetInteger());

    m_MsgDuration = GetConfig()->GetProperty("CommandBar", "MessageDuration");
    m_MsgDuration->SetComment("Maximum visible time of each notification message, measured in seconds (default: 6)");
    m_MsgDuration->SetDefaultFloat(6);
    m_MessageBoard.SetMaxTimer(m_MsgDuration->GetFloat() * 1000);

    GetConfig()->SetCategoryComment("CustomMap", "Custom Map Settings");

    m_CustomMapNumber = GetConfig()->GetProperty("CustomMap", "LevelNumber");
    m_CustomMapNumber->SetComment("Level number to use for custom maps (affects level bonus and sky textures)."
                                  " Must be in the range of 1~13; 0 to randomly select one between 2 and 11");
    m_CustomMapNumber->SetDefaultInteger(0);

    m_CustomMapTooltip = GetConfig()->GetProperty("CustomMap", "ShowTooltip");
    m_CustomMapTooltip->SetComment("Show custom map's full name in tooltip");
    m_CustomMapTooltip->SetDefaultBoolean(false);
    m_MapMenu.SetShowTooltip(m_CustomMapTooltip->GetBoolean());

    m_CustomMapMaxDepth = GetConfig()->GetProperty("CustomMap", "MaxDepth");
    m_CustomMapMaxDepth->SetComment("The max depth of the nested subdirectories.");
    m_CustomMapMaxDepth->SetDefaultInteger(8);
    m_MapMenu.SetMaxDepth(m_CustomMapMaxDepth->GetInteger());
}

void BMLMod::InitGUI() {
    ImGuiIO &io = ImGui::GetIO();

    const std::string path = BML_GetModContext()->GetDirectoryUtf8(BML_DIR_LOADER);

    m_ImGuiIniFilename = path + "\\ImGui.ini";
    m_ImGuiLogFilename = path + "\\ImGui.log";
    io.LogFilename = m_ImGuiLogFilename.c_str();

    io.FontGlobalScale = m_WindowRect.GetHeight() / 1200.0f;

    // Make sure the font atlas doesn't get too large, otherwise weaker GPUs might reject it
    io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
    io.Fonts->TexDesiredWidth = 4096;

    LoadFont(m_FontFilename->GetString(), m_FontSize->GetFloat(), m_FontRanges->GetString());
    if (m_EnableSecondaryFont->GetBoolean()) {
        if (strcmp(m_FontFilename->GetString(), m_SecondaryFontFilename->GetString()) != 0 ||
            strcmp(m_FontRanges->GetString(), m_SecondaryFontRanges->GetString()) != 0)
            LoadFont(m_SecondaryFontFilename->GetString(), m_SecondaryFontSize->GetFloat(), m_SecondaryFontRanges->GetString(), true);
    }
    io.Fonts->Build();

    Bui::InitTextures(m_CKContext);
    Bui::InitMaterials(m_CKContext);

    m_ModMenu.Init();
    m_MapMenu.Init();
}

void BMLMod::RegisterCommands() {
    m_BML->RegisterCommand(new CommandBML());
    m_BML->RegisterCommand(new CommandHelp());
    m_BML->RegisterCommand(new CommandCheat());
    m_BML->RegisterCommand(new CommandEcho());
    m_BML->RegisterCommand(new CommandClear(this));
    m_BML->RegisterCommand(new CommandHistory(this));
    m_BML->RegisterCommand(new CommandExit());
    m_BML->RegisterCommand(new CommandHUD(this));
}

void BMLMod::OnEditScript_Base_EventHandler(CKBehavior *script) {
    CKBehavior *som = FindFirstBB(script, "Switch On Message", false, 2, 11, 11, 0);

    GetLogger()->Info("Insert message Start Menu Hook");
    InsertBB(script, FindNextLink(script, FindNextBB(script, som, nullptr, 0, 0)),
             ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                 BML_GetModContext()->OnPreStartMenu();
                 return CKBR_OK;
             }));
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 0, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModContext()->OnPostStartMenu();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Exit Game Hook");
    InsertBB(script, FindNextLink(script, FindNextBB(script, som, nullptr, 1, 0)),
             ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                 BML_GetModContext()->OnExitGame();
                 return CKBR_OK;
             }));

    GetLogger()->Info("Insert message Load Level Hook");
    CKBehaviorLink *link = FindNextLink(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, som, nullptr, 2, 0))));
    InsertBB(script, link, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnPreLoadLevel();
        return CKBR_OK;
    }));
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 2, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModContext()->OnPostLoadLevel();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Start Level Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 3, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModContext()->OnStartLevel();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Reset Level Hook");
    CKBehavior *rl = FindFirstBB(script, "reset Level");
    link = FindNextLink(rl, FindNextBB(rl, FindNextBB(rl, rl->GetInput(0))));
    InsertBB(script, link, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnPreResetLevel();
        return CKBR_OK;
    }));
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 4, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModContext()->OnPostResetLevel();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Pause Level Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 5, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModContext()->OnPauseLevel();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Unpause Level Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 6, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModContext()->OnUnpauseLevel();
                   return CKBR_OK;
               }));

    CKBehavior *bs = FindNextBB(script, FindFirstBB(script, "DeleteCollisionSurfaces"));

    GetLogger()->Info("Insert message Exit Level Hook");
    link = FindNextLink(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, som, nullptr, 7, 0))))));
    InsertBB(script, link, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnPreExitLevel();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, FindNextBB(script, bs, nullptr, 0, 0)),
             ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                 BML_GetModContext()->OnPostExitLevel();
                 return CKBR_OK;
             }));

    GetLogger()->Info("Insert message Next Level Hook");
    link = FindNextLink(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, som, nullptr, 8, 0))))));
    InsertBB(script, link, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnPreNextLevel();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, FindNextBB(script, bs, nullptr, 1, 0)),
             ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                 BML_GetModContext()->OnPostNextLevel();
                 return CKBR_OK;
             }));

    GetLogger()->Info("Insert message Dead Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 9, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModContext()->OnDead();
                   return CKBR_OK;
               }));

    CKBehavior *hs = FindFirstBB(script, "Highscore");
    hs->AddOutput("Out");
    FindBB(hs, [hs](CKBehavior *beh) {
        CreateLink(hs, beh, hs->GetOutput(0));
        return true;
    }, "Activate Script");

    GetLogger()->Info("Insert message End Level Hook");
    InsertBB(script, FindNextLink(script, FindNextBB(script, som, nullptr, 10, 0)),
             ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                 BML_GetModContext()->OnPreEndLevel();
                 return CKBR_OK;
             }));
    CreateLink(script, hs, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnPostEndLevel();
        return CKBR_OK;
    }));
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
        CKParameter *source = beh->GetInputParameter(0)->GetRealSource();
        if (GetParamValue<CKKEYBOARD>(source) == CKKEY_ESCAPE) {
            CKBehavior *id = FindNextBB(keyboard, beh);
            SetParamValue(id->GetInputParameter(0)->GetRealSource(), 4);
            return false;
        }
        return true;
    }, "Secure Key");

    GetLogger()->Info("Mods Button inserted");
}

void BMLMod::OnEditScript_Gameplay_Ingame(CKBehavior *script) {
    GetLogger()->Info("Insert Ball/Camera Active/Inactive Hook");
    CKBehavior *camonoff = FindFirstBB(script, "CamNav On/Off");
    CKBehavior *ballonoff = FindFirstBB(script, "BallNav On/Off");
    CKMessageManager *mm = m_BML->GetMessageManager();
    CKMessageType camon = mm->AddMessageType("CamNav activate");
    CKMessageType camoff = mm->AddMessageType("CamNav deactivate");
    CKMessageType ballon = mm->AddMessageType("BallNav activate");
    CKMessageType balloff = mm->AddMessageType("BallNav deactivate");
    CKBehavior *con, *coff, *bon, *boff;
    FindBB(camonoff, [camon, camoff, &con, &coff](CKBehavior *beh) {
        auto msg = GetParamValue<CKMessageType>(beh->GetInputParameter(0)->GetDirectSource());
        if (msg == camon) con = beh;
        if (msg == camoff) coff = beh;
        return true;
    }, "Wait Message");
    CreateLink(camonoff, con, ExecuteBB::CreateHookBlock(camonoff, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnCamNavActive();
        return CKBR_OK;
    }), 0, 0);
    CreateLink(camonoff, coff, ExecuteBB::CreateHookBlock(camonoff, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnCamNavInactive();
        return CKBR_OK;
    }), 0, 0);
    FindBB(ballonoff, [ballon, balloff, &bon, &boff](CKBehavior *beh) {
        auto msg = GetParamValue<CKMessageType>(beh->GetInputParameter(0)->GetDirectSource());
        if (msg == ballon) bon = beh;
        if (msg == balloff) boff = beh;
        return true;
    }, "Wait Message");
    CreateLink(ballonoff, bon, ExecuteBB::CreateHookBlock(ballonoff, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnBallNavActive();
        return CKBR_OK;
    }), 0, 0);
    CreateLink(ballonoff, boff, ExecuteBB::CreateHookBlock(ballonoff, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnBallNavInactive();
        return CKBR_OK;
    }), 0, 0);

    m_CurLevel = m_BML->GetArrayByName("CurrentLevel");

    CKBehavior *ballMgr = FindFirstBB(script, "BallManager");
    CKBehavior *deactBall = FindFirstBB(ballMgr, "Deactivate Ball");
    CKBehavior *pieces = FindFirstBB(deactBall, "reset Ballpieces");
    m_OverclockLinks[0] = FindNextLink(deactBall, pieces);
    CKBehavior *unphy = FindNextBB(deactBall, FindNextBB(deactBall, m_OverclockLinks[0]->GetOutBehaviorIO()->GetOwner()));
    m_OverclockLinkIO[0][1] = unphy->GetInput(1);

    CKBehavior *newBall = FindFirstBB(ballMgr, "New Ball");
    CKBehavior *physicsNewBall = FindFirstBB(newBall, "physicalize new Ball");
    m_OverclockLinks[1] = FindPreviousLink(newBall, FindPreviousBB(newBall, FindPreviousBB(newBall, FindPreviousBB(newBall, physicsNewBall))));
    m_OverclockLinkIO[1][1] = physicsNewBall->GetInput(0);
}

void BMLMod::OnEditScript_Gameplay_Energy(CKBehavior *script) {
    GetLogger()->Info("Insert Counter Active/Inactive Hook");
    CKBehavior *som = FindFirstBB(script, "Switch On Message");
    InsertBB(script, FindNextLink(script, som, nullptr, 3), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnCounterActive();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, som, nullptr, 1), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnCounterInactive();
        return CKBR_OK;
    }));

    GetLogger()->Info("Insert Life/Point Hooks");
    CKMessageManager *mm = m_BML->GetMessageManager();
    CKMessageType lifeup = mm->AddMessageType("Life_Up"), balloff = mm->AddMessageType("Ball Off"),
        sublife = mm->AddMessageType("Sub Life"), extrapoint = mm->AddMessageType("Extrapoint");
    CKBehavior *lu, *bo, *sl, *ep;
    FindBB(script, [lifeup, balloff, sublife, extrapoint, &lu, &bo, &sl, &ep](CKBehavior *beh) {
        auto msg = GetParamValue<CKMessageType>(beh->GetInputParameter(0)->GetDirectSource());
        if (msg == lifeup) lu = beh;
        if (msg == balloff) bo = beh;
        if (msg == sublife) sl = beh;
        if (msg == extrapoint) ep = beh;
        return true;
    }, "Wait Message");
    CKBehavior *luhook = ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnPreLifeUp();
        return CKBR_OK;
    });
    InsertBB(script, FindNextLink(script, lu, "add Life"), luhook);
    CreateLink(script, FindEndOfChain(script, luhook), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnPostLifeUp();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, bo, "Delayer"), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnBallOff();
        return CKBR_OK;
    }));
    CKBehavior *slhook = ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnPreSubLife();
        return CKBR_OK;
    });
    InsertBB(script, FindNextLink(script, sl, "sub Life"), slhook);
    CreateLink(script, FindEndOfChain(script, slhook), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnPostSubLife();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, ep, "Show"), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnExtraPoint();
        return CKBR_OK;
    }));

    CKBehavior *delay = FindFirstBB(script, "Delayer");
    m_OverclockLinks[2] = FindPreviousLink(script, delay);
    CKBehaviorLink *link = FindNextLink(script, delay);
    m_OverclockLinkIO[2][1] = link->GetOutBehaviorIO();

    for (int i = 0; i < 3; i++) {
        m_OverclockLinkIO[i][0] = m_OverclockLinks[i]->GetOutBehaviorIO();
        if (m_Overclock->GetBoolean())
            m_OverclockLinks[i]->SetOutBehaviorIO(m_OverclockLinkIO[i][1]);
    }
}

void BMLMod::OnEditScript_Gameplay_Events(CKBehavior *script) {
    GetLogger()->Info("Insert Checkpoint & GameOver Hooks");
    CKMessageManager *mm = m_BML->GetMessageManager();
    CKMessageType checkpoint = mm->AddMessageType("Checkpoint reached"),
        gameover = mm->AddMessageType("Game Over"),
        levelfinish = mm->AddMessageType("Level_Finish");
    CKBehavior *cp, *go, *lf;
    FindBB(script, [checkpoint, gameover, levelfinish, &cp, &go, &lf](CKBehavior *beh) {
        auto msg = GetParamValue<CKMessageType>(beh->GetInputParameter(0)->GetDirectSource());
        if (msg == checkpoint) cp = beh;
        if (msg == gameover) go = beh;
        if (msg == levelfinish) lf = beh;
        return true;
    }, "Wait Message");
    CKBehavior *hook = ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnPreCheckpointReached();
        return CKBR_OK;
    });
    InsertBB(script, FindNextLink(script, cp, "set Resetpoint"), hook);
    CreateLink(script, FindEndOfChain(script, hook), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnPostCheckpointReached();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, go, "Send Message"), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnGameOver();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, lf, "Send Message"), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModContext()->OnLevelFinish();
        return CKBR_OK;
    }));
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
    emitter->CreateInputParameter("Real-Time Mode", CKPGUID_BOOL)
        ->SetDirectSource(CreateParamValue<CKBOOL>(script, "Real-Time Mode", CKPGUID_BOOL, 1));
    emitter->CreateInputParameter("DeltaTime", CKPGUID_FLOAT)
        ->SetDirectSource(CreateParamValue<float>(script, "DeltaTime", CKPGUID_FLOAT, 20.0f));
}

void BMLMod::OnProcess_HUD() {
    m_HUD.OnProcess();
    m_HUD.Render();
}

void BMLMod::OnProcess_CommandBar() {
    bool visible = m_CommandBar.IsVisible();
    if (!visible && ImGui::IsKeyPressed(ImGuiKey_Slash)) {
        GetLogger()->Info("Toggle Command Bar");
        m_CommandBar.ToggleCommandBar();
    }

    m_CommandBar.Render();

    m_MessageBoard.SetCommandBarVisible(visible);
    m_MessageBoard.Render();
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
    ImGui::GetIO().FontGlobalScale = m_WindowRect.GetHeight() / 1200.0f;
}
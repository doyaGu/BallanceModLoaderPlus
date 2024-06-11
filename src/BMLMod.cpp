#include "BMLMod.h"

#include <map>
#include <io.h>

#include <oniguruma.h>
#include <utf8.h>

#include "BML/Bui.h"
#include "BML/Gui.h"
#include "BML/InputHook.h"
#include "BML/ExecuteBB.h"
#include "BML/ScriptHelper.h"
#include "ModManager.h"
#include "Commands.h"
#include "Config.h"
#include "StringUtils.h"
#include "PathUtils.h"
#include "imgui_internal.h"

namespace ExecuteBB {
    void Init();
    void InitFont(FontType type, int fontIndex);
}

using namespace ScriptHelper;

void BMLMod::OnLoad() {
    m_CKContext = m_BML->GetCKContext();
    m_RenderContext = m_BML->GetRenderContext();
    m_TimeManager = m_BML->GetTimeManager();
    m_InputHook = m_BML->GetInputManager();
    m_RenderContext->Get2dRoot(TRUE)->GetRect(m_WindowRect);

    ExecuteBB::Init();

    InitConfigs();
    RegisterCommands();
    InitGUI();
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

    if (!strcmp(filename, "3D Entities\\MenuLevel.nmo")) {
        if (m_AdaptiveCamera->GetBoolean()) {
            GetLogger()->Info("Adjust MenuLevel Camera");
            CKCamera *cam = m_BML->GetTargetCameraByName("Cam_MenuLevel");
            cam->SetAspectRatio((int)m_WindowRect.GetWidth(), (int)m_WindowRect.GetHeight());
            cam->SetFov(0.75f * m_WindowRect.GetWidth() / m_WindowRect.GetHeight());
            m_BML->SetIC(cam);
        }
    }

    if (!strcmp(filename, "3D Entities\\Camera.nmo")) {
        if (m_AdaptiveCamera->GetBoolean()) {
            GetLogger()->Info("Adjust Ingame Camera");
            CKCamera *cam = m_BML->GetTargetCameraByName("InGameCam");
            cam->SetAspectRatio((int)m_WindowRect.GetWidth(), (int)m_WindowRect.GetHeight());
            cam->SetFov(0.75f * m_WindowRect.GetWidth() / m_WindowRect.GetHeight());
            m_BML->SetIC(cam);
        }
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

    OnProcess_Fps();
    OnProcess_SRTimer();
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
            int val = m_FPSLimit->GetInteger();
            if (val > 0)
                AdjustFrameRate(false, static_cast<float>(val));
            else
                AdjustFrameRate(true);
        }
    } else if (prop == m_FPSLimit && !m_UnlockFPS->GetBoolean()) {
        int val = prop->GetInteger();
        if (val > 0)
            AdjustFrameRate(false, static_cast<float>(val));
        else
            AdjustFrameRate(true);
    } else if (prop == m_ShowSR && m_BML->IsIngame()) {
        m_SRShouldDraw = m_ShowSR->GetBoolean();
    } else if (prop == m_MsgDuration) {
        m_MsgMaxTimer = m_MsgDuration->GetFloat() * 1000;
        if (m_MsgMaxTimer < 2000) {
            m_MsgDuration->SetFloat(2.0f);
        }
    }
}

void BMLMod::OnPreStartMenu() {
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

void BMLMod::OnExitGame() {
    m_Level01 = nullptr;
#ifndef NDEBUG
    m_ShowImGuiDemo = false;
#endif
}

void BMLMod::OnStartLevel() {
    if (m_UnlockFPS->GetBoolean()) {
        AdjustFrameRate(false, 0);
    } else {
        int val = m_FPSLimit->GetInteger();
        if (val > 0)
            AdjustFrameRate(false, static_cast<float>(val));
        else
            AdjustFrameRate(true);
    }
    m_SRTimer = 0.0f;
    strcpy(m_SRScore, "00:00:00.000");
    if (m_ShowSR->GetBoolean()) {
        m_SRShouldDraw = true;
    }
    SetParamValue(m_LoadCustom, FALSE);
}

void BMLMod::OnPostExitLevel() {
    m_SRShouldDraw = false;
}

void BMLMod::OnPauseLevel() {
    m_SRActivated = false;
}

void BMLMod::OnUnpauseLevel() {
    m_SRActivated = true;
}

void BMLMod::OnCounterActive() {
    m_SRActivated = true;
}

void BMLMod::OnCounterInactive() {
    m_SRActivated = false;
}

void BMLMod::AddIngameMessage(const char *msg) {
    for (int i = std::min(MSG_MAXSIZE - 1, m_MsgCount) - 1; i >= 0; i--) {
        const char *text = m_Msgs[i].Text;
        strncpy(m_Msgs[i + 1].Text, text, 256);
        m_Msgs[i + 1].Timer = m_Msgs[i].Timer;
    }

    strncpy(m_Msgs[0].Text, msg, 256);
    m_Msgs[0].Timer = m_MsgMaxTimer;
    m_MsgCount++;

    GetLogger()->Info(msg);
}

void BMLMod::ClearIngameMessages() {
    m_MsgCount = 0;
    for (int i = 0; i < MSG_MAXSIZE; i++) {
        m_Msgs[i].Text[0] = '\0';
        m_Msgs[i].Timer = 0.0f;
    }
}

void BMLMod::OpenModsMenu() {
    ShowMenu(MENU_MOD_LIST);
    m_InputHook->Block(CK_INPUT_DEVICE_KEYBOARD);
}

void BMLMod::ExitModsMenu() {
    ShowPreviousMenu();
    HideMenu();

    CKBehavior *beh = m_BML->GetScriptByName("Menu_Options");
    m_CKContext->GetCurrentScene()->Activate(beh, true);
    m_BML->AddTimerLoop(1ul, [this] {
        if (m_InputHook->oIsKeyDown(CKKEY_ESCAPE) || m_InputHook->oIsKeyDown(CKKEY_RETURN))
            return true;
        m_InputHook->Unblock(CK_INPUT_DEVICE_KEYBOARD);
        return false;
    });
}

void BMLMod::OpenMapMenu() {
    ShowMenu(MENU_MAP_LIST);
    m_InputHook->Block(CK_INPUT_DEVICE_KEYBOARD);

    RefreshMaps();
}

void BMLMod::ExitMapMenu(bool backToMenu) {
    ShowPreviousMenu();
    HideMenu();

    if (backToMenu) {
        CKBehavior *beh = m_BML->GetScriptByName("Menu_Start");
        m_CKContext->GetCurrentScene()->Activate(beh, true);
    }

    m_BML->AddTimerLoop(1ul, [this] {
        if (m_InputHook->oIsKeyDown(CKKEY_ESCAPE) || m_InputHook->oIsKeyDown(CKKEY_RETURN))
            return true;
        m_InputHook->Unblock(CK_INPUT_DEVICE_KEYBOARD);
        return false;
    });
}

int BMLMod::GetHSScore() {
    int points, lifes;
    CKDataArray *energy = m_BML->GetArrayByName("Energy");
    energy->GetElementValue(0, 0, &points);
    energy->GetElementValue(0, 1, &lifes);
    return points + lifes * 200;
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
    m_ShowFPS->SetBoolean((mode & HUD_FPS) != 0);
    m_ShowSR->SetBoolean((mode & HUD_SR) != 0);
}

void BMLMod::ToggleCommandBar(bool on) {
    if (on) {
        m_CmdTyping = true;
        m_CmdBuf[0] = '\0';
        m_InputHook->Block(CK_INPUT_DEVICE_KEYBOARD);
        m_HistoryPos = (int)m_CmdHistory.size();
    } else {
        m_CmdTyping = false;
        m_CmdBuf[0] = '\0';
        m_BML->AddTimerLoop(1ul, [this] {
            if (m_InputHook->oIsKeyDown(CKKEY_ESCAPE) || m_InputHook->oIsKeyDown(CKKEY_RETURN))
                return true;
            m_InputHook->Unblock(CK_INPUT_DEVICE_KEYBOARD);
            return false;
        });
    }
}

int BMLMod::OnTextEdit(ImGuiInputTextCallbackData *data) {
    switch (data->EventFlag) {
        case ImGuiInputTextFlags_CallbackCompletion: {
            // Locate beginning of current word
            const char *wordEnd = data->Buf + data->CursorPos;
            const char *wordStart = wordEnd;
            while (wordStart > data->Buf) {
                const char c = wordStart[-1];
                if (isspace(c))
                    break;
                --wordStart;
            }

            int argc = 1;
            const char *cmdEnd;
            const char *cmdStart;
            if (wordStart == data->Buf) {
                cmdEnd = wordEnd;
                cmdStart = wordStart;
            } else {
                cmdEnd = wordStart;
                cmdStart = cmdEnd;
                while (cmdStart > data->Buf) {
                    const char c = cmdStart[-1];
                    if (isspace(c)) {
                        ++argc;
                        cmdEnd = &cmdStart[-1];
                    }
                    cmdStart--;
                }
            }

            // Build a list of candidates
            std::vector<std::string> candidates;
            if (argc == 1) {
                const int count = m_BML->GetCommandCount();
                for (int i = 0; i < count; ++i) {
                    ICommand *cmd = m_BML->GetCommand(i);
                    if (cmd) {
                        if (utf8ncasecmp(cmd->GetName().c_str(), cmdStart, (int)(cmdEnd - cmdStart)) == 0)
                            candidates.push_back(cmd->GetName());
                    }
                }
            } else {
                size_t size = utf8size(cmdStart);
                char *buf = new char[size + 1];
                utf8ncpy(buf, cmdStart, size);

                std::vector<std::string> args;

                char *lp = &buf[0];
                char *rp = lp;
                char *end = lp + size;
                utf8_int32_t cp, temp;
                utf8codepoint(rp, &cp);
                while (rp != end) {
                    if (std::isspace(*rp) || *rp == '\0') {
                        size_t len = rp - lp;
                        if (len != 0) {
                            char bk = *rp;
                            *rp = '\0';
                            args.emplace_back(lp);
                            *rp = bk;
                        }

                        if (*rp != '\0') {
                            while (std::isspace(*rp))
                                ++rp;
                            --rp;
                        }

                        lp = utf8codepoint(rp, &temp);
                    }

                    rp = utf8codepoint(rp, &cp);
                }

                delete[] buf;

                ICommand *cmd = m_BML->FindCommand(args[0].c_str());
                for (const auto &str : cmd->GetTabCompletion(m_BML, args)) {
                    if (utils::StringStartsWith(str, args[args.size() - 1]))
                        candidates.push_back(str);
                }
            }

            if (!candidates.empty()) {
                if (candidates.size() == 1) {
                    // Single match. Delete the beginning of the word and replace it entirely so we've got nice casing.
                    data->DeleteChars((int)(wordStart - data->Buf), (int)(wordEnd - wordStart));
                    data->InsertChars(data->CursorPos, candidates[0].c_str());
                    data->InsertChars(data->CursorPos, " ");
                } else {
                    // Multiple matches. Complete as much as we can..
                    // So inputting "C"+Tab will complete to "CL" then display "CLEAR" and "CLASSIFY" as matches.
                    int matchLen = (int)(wordEnd - wordStart);
                    for (;;)
                    {
                        int c = 0;
                        bool allCandidatesMatches = true;
                        for (size_t i = 0; i < candidates.size() && allCandidatesMatches; i++)
                            if (i == 0)
                                c = toupper(candidates[i][matchLen]);
                            else if (c == 0 || c != toupper(candidates[i][matchLen]))
                                allCandidatesMatches = false;
                        if (!allCandidatesMatches)
                            break;
                        matchLen++;
                    }

                    if (matchLen > 0)
                    {
                        data->DeleteChars((int)(wordStart - data->Buf), (int)(wordEnd - wordStart));
                        data->InsertChars(data->CursorPos, candidates[0].c_str(), candidates[0].c_str() + matchLen);
                        data->InsertChars(data->CursorPos, " ");
                    }
                }
            }
        }
            break;
        case ImGuiInputTextFlags_CallbackHistory: {
            const unsigned int prevHistoryPos = m_HistoryPos;
            if (data->EventKey == ImGuiKey_UpArrow) {
                if (m_HistoryPos == -1)
                    m_HistoryPos = (int)(m_CmdHistory.size() - 1);
                else if (m_HistoryPos > 0)
                    m_HistoryPos--;
            } else if (data->EventKey == ImGuiKey_DownArrow) {
                if (m_HistoryPos != -1)
                    if (++m_HistoryPos >= (int)m_CmdHistory.size())
                        m_HistoryPos = -1;
            }

            if (prevHistoryPos != m_HistoryPos) {
                const std::string &historyStr = (m_HistoryPos >= 0) ? m_CmdHistory[m_HistoryPos] : "";
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, historyStr.c_str());
            }
        }
            break;
        default:
            break;
    }

    return 0;
}

void BMLMod::InitConfigs() {
    GetConfig()->SetCategoryComment("Misc", "Miscellaneous");

    m_UnlockFPS = GetConfig()->GetProperty("Misc", "UnlockFrameRate");
    m_UnlockFPS->SetComment("Unlock Frame Rate Limitation");
    m_UnlockFPS->SetDefaultBoolean(true);

    m_FPSLimit = GetConfig()->GetProperty("Misc", "SetMaxFrameRate");
    m_FPSLimit->SetComment("Set Frame Rate Limitation, this option will not work if frame rate is unlocked. Set to 0 will turn on VSync.");
    m_FPSLimit->SetDefaultInteger(0);

    m_AdaptiveCamera = GetConfig()->GetProperty("Misc", "AdaptiveCamera");
    m_AdaptiveCamera->SetComment("Adjust cameras on screen mode changed");
    m_AdaptiveCamera->SetDefaultBoolean(true);

    m_ShowTitle = GetConfig()->GetProperty("Misc", "ShowTitle");
    m_ShowTitle->SetComment("Show BML Title at top");
    m_ShowTitle->SetDefaultBoolean(true);

    m_ShowFPS = GetConfig()->GetProperty("Misc", "ShowFPS");
    m_ShowFPS->SetComment("Show FPS at top-left corner");
    m_ShowFPS->SetDefaultBoolean(true);

    m_ShowSR = GetConfig()->GetProperty("Misc", "ShowSRTimer");
    m_ShowSR->SetComment("Show SR Timer above Time Score");
    m_ShowSR->SetDefaultBoolean(true);

    m_FixLifeBall = GetConfig()->GetProperty("Misc", "FixLifeBallFreeze");
    m_FixLifeBall->SetComment("Game won't freeze when picking up life balls");
    m_FixLifeBall->SetDefaultBoolean(true);

    m_MsgDuration = GetConfig()->GetProperty("Misc", "MessageDuration");
    m_MsgDuration->SetComment("Maximum visible time of each notification message, measured in seconds (default: 6)");
    m_MsgDuration->SetDefaultFloat(m_MsgMaxTimer / 1000);
    m_MsgMaxTimer = m_MsgDuration->GetFloat() * 1000;

    m_CustomMapNumber = GetConfig()->GetProperty("Misc", "CustomMapNumber");
    m_CustomMapNumber->SetComment("Level number to use for custom maps (affects level bonus and sky textures)."
                                  " Must be in the range of 1~13; 0 to randomly select one between 2 and 11");
    m_CustomMapNumber->SetDefaultInteger(0);

    GetConfig()->SetCategoryComment("GUI", "Settings for the GUI created by BML");

    m_FontFilename = GetConfig()->GetProperty("GUI", "FontFilename");
    m_FontFilename->SetComment("The filename of TrueType font (the font filename should end with .ttf or .otf)");
    m_FontFilename->SetDefaultString("unifont.otf");

    m_FontSize = GetConfig()->GetProperty("GUI", "FontSize");
    m_FontSize->SetComment("The size of font (pixel).");
    m_FontSize->SetDefaultFloat(32.0f);

    m_FontGlyphRanges = GetConfig()->GetProperty("GUI", "FontGlyphRanges");
    m_FontGlyphRanges->SetComment("The Unicode ranges of font glyph.");
    m_FontGlyphRanges->SetDefaultString("Default");
}

void BMLMod::RegisterCommands() {
    m_BML->RegisterCommand(new CommandBML());
    m_BML->RegisterCommand(new CommandHelp());
    m_BML->RegisterCommand(new CommandExit());
    m_BML->RegisterCommand(new CommandEcho());
    m_BML->RegisterCommand(new CommandCheat());
    m_BML->RegisterCommand(new CommandClear(this));
    m_BML->RegisterCommand(new CommandHUD(this));
}

void BMLMod::InitGUI() {
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard;
    io.FontGlobalScale = m_WindowRect.GetHeight() / 1200.0f;

    LoadFont();

    Bui::InitTextures(m_CKContext);
    Bui::InitMaterials(m_CKContext);
}

void BMLMod::LoadFont() {
    ImGuiIO &io = ImGui::GetIO();

    const char *filename = m_FontFilename->GetString();
    if (filename && filename[0] != '\0') {
        std::string path = BML_GetModManager()->GetDirectoryUtf8(BML_DIR_LOADER);
        path.append("\\Fonts\\").append(filename);

        float size = m_FontSize->GetFloat();
        if (size <= 0)
            size = 32.0f;

        const ImWchar *glyphRanges = nullptr;
        const char *ranges = m_FontGlyphRanges->GetString();
        if (strnicmp(ranges, "ChineseFull", 12) == 0) {
            glyphRanges = io.Fonts->GetGlyphRangesChineseFull();
        } else if (strnicmp(ranges, "Chinese", 8) == 0) {
            glyphRanges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
        }

        ImFontConfig config;
        config.SizePixels = size;
        if (strnicmp(ranges, "Chinese", 7) == 0) {
            // Set OversampleH/OversampleV to 1 to reduce the texture size.
            config.OversampleH = config.OversampleV = 1;
        }

        m_Font = io.Fonts->AddFontFromFileTTF(path.c_str(), size, &config, glyphRanges);
    }

    if (!m_Font) {
        ImFontConfig config;
        config.SizePixels = 32.0f;
        m_Font = io.Fonts->AddFontDefault(&config);
        GetLogger()->Warn("Can not load the specific font, use default font instead.");
    }
}

void BMLMod::RefreshMaps() {
    std::wstring path = BML_GetModManager()->GetDirectory(BML_DIR_LOADER);
    path.append(L"\\Maps");

    m_Maps.clear();
    ExploreMaps(path, m_Maps);
}

size_t BMLMod::ExploreMaps(const std::wstring &path, std::vector<MapInfo> &maps) {
    if (path.empty() || !utils::DirectoryExists(path))
        return 0;

    std::wstring p = path + L"\\*";
    _wfinddata_t fileinfo = {};
    auto handle = _wfindfirst(p.c_str(), &fileinfo);
    if (handle == -1)
        return 0;

    do {
        if ((fileinfo.attrib & _A_SUBDIR) && wcscmp(fileinfo.name, L".") != 0 && wcscmp(fileinfo.name, L"..") != 0) {
            ExploreMaps(p.assign(path).append(L"\\").append(fileinfo.name), maps);
        } else {
            std::wstring fullPath = path;
            fullPath.append(L"\\").append(fileinfo.name);

            wchar_t filename[1024];
            wchar_t ext[64];
            _wsplitpath(fileinfo.name, nullptr, nullptr, filename, ext);
            if (wcsicmp(ext, L".nmo") == 0) {
                MapInfo info;
                char buffer[1024];
                utils::Utf16ToUtf8(filename, buffer, sizeof(buffer));
                info.name = buffer;
                info.path = fullPath;
                maps.push_back(std::move(info));
            }
        }
    } while (_wfindnext(handle, &fileinfo) == 0);

    _findclose(handle);

    return maps.size();
}

void BMLMod::LoadMap(const std::wstring &path) {
    if (path.empty())
        return;

    std::string filename = CreateTempMapFile(path);
    SetParamString(m_MapFile, filename.c_str());
    SetParamValue(m_LoadCustom, TRUE);
    int level = GetConfig()->GetProperty("Misc", "CustomMapNumber")->GetInteger();
    level = (level >= 1 && level <= 13) ? level : rand() % 10 + 2;
    m_CurLevel->SetElementValue(0, 0, &level);
    level--;
    SetParamValue(m_LevelRow, level);

    CKMessageManager *mm = m_CKContext->GetMessageManager();
    CKMessageType loadLevel = mm->AddMessageType((CKSTRING) "Load Level");
    CKMessageType loadMenu = mm->AddMessageType((CKSTRING) "Menu_Load");

    mm->SendMessageSingle(loadLevel, m_CKContext->GetCurrentLevel());
    mm->SendMessageSingle(loadMenu, BML_GetGroupByName("All_Sound"));
    m_BML->Get2dEntityByName("M_BlackScreen")->Show(CKHIDE);
    m_ExitStart->ActivateInput(0);
    m_ExitStart->Activate();
}

std::string BMLMod::CreateTempMapFile(const std::wstring &path) {
    if (path.empty() || !utils::FileExists(path))
        return "";

    wchar_t filename[1024];
    wchar_t ext[64];
    _wsplitpath(path.c_str(), nullptr, nullptr, filename, ext);

    size_t hash = utils::HashString(filename);

    wchar_t buf[1024];
    _snwprintf(buf, sizeof(buf) / sizeof(wchar_t),
               L"%s\\Maps\\%8X%s", BML_GetModManager()->GetDirectory(BML_DIR_TEMP), hash, ext);
    buf[1023] = '\0';

    if (!utils::DuplicateFile(path, buf))
        return "";

    char str[1024];
    utils::Utf16ToAnsi(buf, str, sizeof(str));
    return str;
}

void BMLMod::OnEditScript_Base_EventHandler(CKBehavior *script) {
    CKBehavior *som = FindFirstBB(script, "Switch On Message", false, 2, 11, 11, 0);

    GetLogger()->Info("Insert message Start Menu Hook");
    InsertBB(script, FindNextLink(script, FindNextBB(script, som, nullptr, 0, 0)),
             ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                 BML_GetModManager()->OnPreStartMenu();
                 return CKBR_OK;
             }));
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 0, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModManager()->OnPostStartMenu();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Exit Game Hook");
    InsertBB(script, FindNextLink(script, FindNextBB(script, som, nullptr, 1, 0)),
             ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                 BML_GetModManager()->OnExitGame();
                 return CKBR_OK;
             }));

    GetLogger()->Info("Insert message Load Level Hook");
    CKBehaviorLink *link = FindNextLink(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, som, nullptr, 2, 0))));
    InsertBB(script, link, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPreLoadLevel();
        return CKBR_OK;
    }));
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 2, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModManager()->OnPostLoadLevel();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Start Level Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 3, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModManager()->OnStartLevel();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Reset Level Hook");
    CKBehavior *rl = FindFirstBB(script, "reset Level");
    link = FindNextLink(rl, FindNextBB(rl, FindNextBB(rl, rl->GetInput(0))));
    InsertBB(script, link, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPreResetLevel();
        return CKBR_OK;
    }));
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 4, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModManager()->OnPostResetLevel();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Pause Level Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 5, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModManager()->OnPauseLevel();
                   return CKBR_OK;
               }));

    GetLogger()->Info("Insert message Unpause Level Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 6, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModManager()->OnUnpauseLevel();
                   return CKBR_OK;
               }));

    CKBehavior *bs = FindNextBB(script, FindFirstBB(script, "DeleteCollisionSurfaces"));

    GetLogger()->Info("Insert message Exit Level Hook");
    link = FindNextLink(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, som, nullptr, 7, 0))))));
    InsertBB(script, link, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPreExitLevel();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, FindNextBB(script, bs, nullptr, 0, 0)),
             ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                 BML_GetModManager()->OnPostExitLevel();
                 return CKBR_OK;
             }));

    GetLogger()->Info("Insert message Next Level Hook");
    link = FindNextLink(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, som, nullptr, 8, 0))))));
    InsertBB(script, link, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPreNextLevel();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, FindNextBB(script, bs, nullptr, 1, 0)),
             ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                 BML_GetModManager()->OnPostNextLevel();
                 return CKBR_OK;
             }));

    GetLogger()->Info("Insert message Dead Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 9, 0)),
               ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
                   BML_GetModManager()->OnDead();
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
                 BML_GetModManager()->OnPreEndLevel();
                 return CKBR_OK;
             }));
    CreateLink(script, hs, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPostEndLevel();
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
        BML_GetModManager()->OpenModsMenu();
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
        BML_GetModManager()->OnCamNavActive();
        return CKBR_OK;
    }), 0, 0);
    CreateLink(camonoff, coff, ExecuteBB::CreateHookBlock(camonoff, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnCamNavInactive();
        return CKBR_OK;
    }), 0, 0);
    FindBB(ballonoff, [ballon, balloff, &bon, &boff](CKBehavior *beh) {
        auto msg = GetParamValue<CKMessageType>(beh->GetInputParameter(0)->GetDirectSource());
        if (msg == ballon) bon = beh;
        if (msg == balloff) boff = beh;
        return true;
    }, "Wait Message");
    CreateLink(ballonoff, bon, ExecuteBB::CreateHookBlock(ballonoff, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnBallNavActive();
        return CKBR_OK;
    }), 0, 0);
    CreateLink(ballonoff, boff, ExecuteBB::CreateHookBlock(ballonoff, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnBallNavInactive();
        return CKBR_OK;
    }), 0, 0);

    m_CurLevel = m_BML->GetArrayByName("CurrentLevel");
}

void BMLMod::OnEditScript_Gameplay_Energy(CKBehavior *script) {
    GetLogger()->Info("Insert Counter Active/Inactive Hook");
    CKBehavior *som = FindFirstBB(script, "Switch On Message");
    InsertBB(script, FindNextLink(script, som, nullptr, 3), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnCounterActive();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, som, nullptr, 1), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnCounterInactive();
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
        BML_GetModManager()->OnPreLifeUp();
        return CKBR_OK;
    });
    InsertBB(script, FindNextLink(script, lu, "add Life"), luhook);
    CreateLink(script, FindEndOfChain(script, luhook), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPostLifeUp();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, bo, "Delayer"), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnBallOff();
        return CKBR_OK;
    }));
    CKBehavior *slhook = ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPreSubLife();
        return CKBR_OK;
    });
    InsertBB(script, FindNextLink(script, sl, "sub Life"), slhook);
    CreateLink(script, FindEndOfChain(script, slhook), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPostSubLife();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, ep, "Show"), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnExtraPoint();
        return CKBR_OK;
    }));
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
        BML_GetModManager()->OnPreCheckpointReached();
        return CKBR_OK;
    });
    InsertBB(script, FindNextLink(script, cp, "set Resetpoint"), hook);
    CreateLink(script, FindEndOfChain(script, hook), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnPostCheckpointReached();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, go, "Send Message"), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnGameOver();
        return CKBR_OK;
    }));
    InsertBB(script, FindNextLink(script, lf, "Send Message"), ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext *, void *) -> int {
        BML_GetModManager()->OnLevelFinish();
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
}

void BMLMod::OnEditScript_ExtraLife_Fix(CKBehavior *script) {
    CKBehavior *emitter = FindFirstBB(script, "SphericalParticleSystem");
    emitter->CreateInputParameter("Real-Time Mode", CKPGUID_BOOL)
        ->SetDirectSource(CreateParamValue<CKBOOL>(script, "Real-Time Mode", CKPGUID_BOOL, 1));
    emitter->CreateInputParameter("DeltaTime", CKPGUID_FLOAT)
        ->SetDirectSource(CreateParamValue<float>(script, "DeltaTime", CKPGUID_FLOAT, 20.0f));
}

void BMLMod::OnProcess_Fps() {
    CKStats stats;
    m_CKContext->GetProfileStats(&stats);
    m_FPSCount += int(1000 / stats.TotalFrameTime);
    if (++m_FPSTimer == 60) {
        sprintf(m_FPSText, "FPS: %d", m_FPSCount / 60);
        m_FPSTimer = 0;
        m_FPSCount = 0;
    }
}

void BMLMod::OnProcess_SRTimer() {
    if (m_SRActivated) {
        m_SRTimer += m_TimeManager->GetLastDeltaTime();
        int counter = int(m_SRTimer);
        int ms = counter % 1000;
        counter /= 1000;
        int s = counter % 60;
        counter /= 60;
        int m = counter % 60;
        counter /= 60;
        int h = counter % 100;
        sprintf(m_SRScore, "%02d:%02d:%02d.%03d", h, m, s, ms);
    }
}

void BMLMod::OnProcess_HUD() {
    const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(vpSize);

    constexpr ImGuiWindowFlags HUDFlags = ImGuiWindowFlags_NoDecoration |
                                          ImGuiWindowFlags_NoBackground |
                                          ImGuiWindowFlags_NoResize |
                                          ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoInputs |
                                          ImGuiWindowFlags_NoBringToFrontOnFocus |
                                          ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("HUD", nullptr, HUDFlags)) {
        ImDrawList *drawList = ImGui::GetWindowDrawList();

        float oldScale = m_Font->Scale;
        m_Font->Scale *= 1.2f;
        ImGui::PushFont(m_Font);

        if (m_ShowTitle->GetBoolean()) {
            constexpr auto TitleText = "BML Plus " BML_VERSION;
            const auto titleSize = ImGui::CalcTextSize(TitleText);
            drawList->AddText(ImVec2((vpSize.x - titleSize.x) / 2.0f, 0), IM_COL32_WHITE, TitleText);
        }

        if (m_BML->IsCheatEnabled()) {
            constexpr auto CheatText = "Cheat Mode Enabled";
            const auto cheatSize = ImGui::CalcTextSize(CheatText);
            drawList->AddText(ImVec2((vpSize.x - cheatSize.x) / 2.0f, vpSize.y * 0.85f), IM_COL32_WHITE, CheatText);
        }

        if (m_ShowFPS->GetBoolean()) {
            drawList->AddText(ImVec2(0, 0), IM_COL32_WHITE, m_FPSText);
        }

        if (m_SRShouldDraw) {
            drawList->AddText(ImVec2(vpSize.x * 0.03f, vpSize.y * 0.8f), IM_COL32_WHITE, "SR Timer");
            auto srSize = ImGui::CalcTextSize(m_SRScore);
            drawList->AddText(ImVec2(vpSize.x * 0.05f, vpSize.y * 0.8f + srSize.y), IM_COL32_WHITE, m_SRScore);
        }

        m_Font->Scale = oldScale;
        ImGui::PopFont();
    }
    ImGui::End();
}

static int TextEditCallback(ImGuiInputTextCallbackData *data) {
    auto *mod = (BMLMod *) data->UserData;
    return mod->OnTextEdit(data);
}

void BMLMod::OnProcess_CommandBar() {
    bool prev = m_CmdTyping;
    if (!m_CmdTyping && ImGui::IsKeyPressed(ImGuiKey_Slash)) {
        GetLogger()->Info("Toggle Command Bar");
        ToggleCommandBar();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Bui::GetMenuColor());
    ImGui::PushFont(m_Font);

    const ImVec2 vpSize = ImGui::GetMainViewport()->Size;
    const ImVec2 size(vpSize.x * 0.96f, 0.0f);

    if (m_CmdTyping) {
        const ImVec2 pos(vpSize.x * 0.02f, vpSize.y * 0.93f);
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);

        if (!prev)
            ImGui::SetNextWindowFocus();

        ImGui::PushStyleColor(ImGuiCol_FrameBg, Bui::GetMenuColor());

        constexpr ImGuiWindowFlags BarFlags = ImGuiWindowFlags_NoDecoration |
                                              ImGuiWindowFlags_NoBackground |
                                              ImGuiWindowFlags_NoResize |
                                              ImGuiWindowFlags_NoMove |
                                              ImGuiWindowFlags_NoNav;
        ImGui::Begin("CommandBar", nullptr, BarFlags);

        ImGui::SetNextItemWidth(size.x);

        constexpr ImGuiInputTextFlags InputTextFlags = ImGuiInputTextFlags_EnterReturnsTrue |
                                                       ImGuiInputTextFlags_EscapeClearsAll |
                                                       ImGuiInputTextFlags_CallbackCompletion |
                                                       ImGuiInputTextFlags_CallbackHistory;
        if (ImGui::InputText("##CmdBar", m_CmdBuf, IM_ARRAYSIZE(m_CmdBuf), InputTextFlags,
                             &TextEditCallback, (void *) this)) {
            if (m_CmdBuf[0] != '\0') {
                m_CmdHistory.emplace_back(m_CmdBuf);
                m_BML->ExecuteCommand(m_CmdBuf);
            }
            ToggleCommandBar(false);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            ToggleCommandBar(false);

        ImGui::SetItemDefaultFocus();
        if (!prev)
            ImGui::SetKeyboardFocusHere(-1);

        ImGui::End();
        ImGui::PopStyleColor();
    }

    constexpr ImGuiWindowFlags MsgFlags = ImGuiWindowFlags_NoDecoration |
                                          ImGuiWindowFlags_NoInputs |
                                          ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoResize |
                                          ImGuiWindowFlags_NoNav |
                                          ImGuiWindowFlags_NoFocusOnAppearing |
                                          ImGuiWindowFlags_NoBringToFrontOnFocus;

    const int count = std::min(MSG_MAXSIZE, m_MsgCount);
    for (int i = 0; i < count; i++) {
        float &timer = m_Msgs[i].Timer;
        if (timer < 0) {
            m_Msgs[i].Text[0] = '\0';
        } else {
            const ImVec2 pos(vpSize.x * 0.02f, vpSize.y * 0.9f - (float) i * ImGui::GetTextLineHeightWithSpacing());
            ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(size, ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(std::min(110.0f, (timer / 20.0f)) / 255.0f);
            static char buf[16];
            snprintf(buf, 16, "Message%d", i);
            ImGui::Begin(buf, nullptr, MsgFlags);
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
            ImGui::Text("%s", m_Msgs[i].Text);
            ImGui::End();
        }
    }

    ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    CKStats stats;
    m_CKContext->GetProfileStats(&stats);
    for (int i = 0; i < count; i++) {
        m_Msgs[i].Timer -= stats.TotalFrameTime;
    }
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
                                                 ImGuiWindowFlags_NoBringToFrontOnFocus;

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

    OnDrawMenu();
}

void BMLMod::ShowMenu(MenuId id) {
    PushMenu(m_CurrentMenu);
    m_CurrentMenu = id;
}

void BMLMod::ShowPreviousMenu() {
    MenuId id = PopWindow();
    m_CurrentMenu = id;
}

void BMLMod::HideMenu() {
    m_CurrentMenu = MENU_NULL;
}

void BMLMod::PushMenu(MenuId id) {
    m_MenuStack.push(id);
}

MenuId BMLMod::PopWindow() {
    if (m_MenuStack.empty())
        return MENU_NULL;
    MenuId id = m_MenuStack.top();
    m_MenuStack.pop();
    return id;
}

constexpr ImGuiWindowFlags MenuWinFlags = ImGuiWindowFlags_NoDecoration |
                                          ImGuiWindowFlags_NoBackground |
                                          ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoScrollWithMouse |
                                          ImGuiWindowFlags_NoBringToFrontOnFocus |
                                          ImGuiWindowFlags_NoSavedSettings;

void BMLMod::OnDrawMenu() {
    if (m_CurrentMenu == MENU_NULL)
        return;

    ImGui::PushFont(m_Font);

    const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
    ImGui::SetNextWindowPos(ImVec2(vpSize.x * 0.3f, 0.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(vpSize.x * 0.7f, vpSize.y), ImGuiCond_Appearing);

    switch (m_CurrentMenu) {
        case MENU_MOD_LIST:
            OnDrawModList();
            break;
        case MENU_MOD_PAGE:
            OnDrawModPage();
            break;
        case MENU_MOD_OPTIONS:
            OnDrawModOptions();
            break;
        case MENU_MAP_LIST:
            OnDrawMapList();
            break;
        default:
            break;
    }

    ImGui::PopFont();
}

void BMLMod::OnDrawModList() {
    constexpr auto TitleText = "Mod List";

    ImGui::Begin(TitleText, nullptr, MenuWinFlags);

    {
        float oldScale = ImGui::GetFont()->Scale;
        ImGui::GetFont()->Scale *= 1.5f;
        ImGui::PushFont(ImGui::GetFont());

        const auto titleSize = ImGui::CalcTextSize(TitleText);
        const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
        ImGui::GetWindowDrawList()->AddText(ImVec2((vpSize.x - titleSize.x) / 2.0f, vpSize.y * 0.13f), IM_COL32_WHITE,
                                            TitleText);

        ImGui::GetFont()->Scale = oldScale;
        ImGui::PopFont();
    }

    const int modCount = m_BML->GetModCount();
    const int maxPage = ((modCount % 4) == 0) ? modCount / 4 : modCount / 4 + 1;

    if (m_ModListPage > 0) {
        ImGui::SetCursorScreenPos(Bui::CoordToScreenPos(ImVec2(0.36f, 0.124f)));
        if (Bui::LeftButton("ModListPrevPage")) {
            --m_ModListPage;
        }
    }

    if (maxPage > 1 && m_ModListPage < maxPage - 1) {
        ImGui::SetCursorScreenPos(Bui::CoordToScreenPos(ImVec2(0.6038f, 0.124f)));
        if (Bui::RightButton("ModListNextPage")) {
            ++m_ModListPage;
        }
    }

    {
        const int n = m_ModListPage * 4;
        for (int i = 0; i < 4 && n + i < modCount; ++i) {
            IMod *mod = m_BML->GetMod(n + i);

            char buf[256];
            sprintf(buf, "%s", mod->GetID());
            ImGui::SetCursorScreenPos(Bui::CoordToScreenPos(ImVec2(0.35f, 0.24f + (float) i * 0.14f)));
            if (Bui::MainButton(buf)) {
                m_CurrentMod = mod;
                ShowMenu(MENU_MOD_PAGE);
            }
        }
    }

    ImGui::SetCursorScreenPos(Bui::CoordToScreenPos(ImVec2(0.4031f, 0.85f)));
    if (Bui::BackButton("ModListBack") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        m_ModListPage = 0;
        ExitModsMenu();
    }

    ImGui::End();
}

void BMLMod::OnDrawModPage() {
    if (!m_CurrentMod)
        return;

    constexpr auto TitleText = "Mod Options";

    ImGui::Begin("Mod Page", nullptr, MenuWinFlags);

    ImGui::Dummy(Bui::CoordToScreenPos(ImVec2(0.375f, 0.1f)));

    ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
    float menuWidth = vpSize.x * 0.4f;

    {
        float oldScale = ImGui::GetFont()->Scale;
        ImGui::GetFont()->Scale *= 1.2f;
        ImGui::PushFont(ImGui::GetFont());

        const float textWidth = ImGui::CalcTextSize(m_CurrentMod->GetName()).x;
        ImGui::SetCursorPosX((menuWidth - textWidth) * 0.5f);
        ImGui::TextUnformatted(m_CurrentMod->GetName());

        ImGui::GetFont()->Scale = oldScale;
        ImGui::PopFont();
    }

    char buf[256];

    {
        snprintf(buf, sizeof(buf), "By %s", m_CurrentMod->GetAuthor());
        const float textWidth = ImGui::CalcTextSize(buf).x;
        const float indent = (menuWidth - textWidth) * 0.5f;
        if (indent > 0) {
            ImGui::SetCursorPosX(indent);
            ImGui::PushTextWrapPos(indent + textWidth);
        } else {
            ImGui::PushTextWrapPos(menuWidth);
        }
        ImGui::TextUnformatted(buf);
        ImGui::PopTextWrapPos();
    }

    {
        snprintf(buf, sizeof(buf), "v%s", m_CurrentMod->GetVersion());
        const float textWidth = ImGui::CalcTextSize(buf).x;
        ImGui::SetCursorPosX((menuWidth - textWidth) * 0.5f);
        ImGui::TextUnformatted(buf);
    }

    ImGui::NewLine();

    {
        const float textWidth = ImGui::CalcTextSize(m_CurrentMod->GetDescription()).x;
        const float indent = (menuWidth - textWidth) * 0.5f;
        if (indent > 0) {
            ImGui::SetCursorPosX(indent);
            ImGui::PushTextWrapPos(indent + textWidth);
        } else {
            ImGui::PushTextWrapPos(menuWidth);
        }
        ImGui::TextUnformatted(m_CurrentMod->GetDescription());
        ImGui::PopTextWrapPos();
    }

    auto *config = BML_GetModManager()->GetConfig(m_CurrentMod);
    if (config) {
        float y = ImGui::GetCursorScreenPos().y;

        {
            float oldScale = ImGui::GetFont()->Scale;
            ImGui::GetFont()->Scale *= 1.5f;
            ImGui::PushFont(ImGui::GetFont());

            const auto titleSize = ImGui::CalcTextSize(TitleText);
            ImGui::GetWindowDrawList()->AddText(ImVec2((vpSize.x - titleSize.x) / 2.0f, y + vpSize.y * 0.06f),
                                                IM_COL32_WHITE, TitleText);

            ImGui::GetFont()->Scale = oldScale;
            ImGui::PopFont();
        }

        const int categoryCount = (int) config->GetCategoryCount();
        const int maxPage = ((categoryCount % 4) == 0) ? categoryCount / 4 : categoryCount / 4 + 1;

        if (m_ModPage > 0) {
            ImGui::SetCursorScreenPos(ImVec2(vpSize.x * 0.36f, y + vpSize.y * 0.054f));
            if (Bui::LeftButton("ModPrevPage")) {
                --m_ModPage;
            }
        }

        if (maxPage > 1 && m_ModPage < maxPage - 1) {
            ImGui::SetCursorScreenPos(ImVec2(vpSize.x * 0.6038f, y + vpSize.y * 0.054f));
            if (Bui::RightButton("ModNextPage")) {
                ++m_ModPage;
            }
        }

        {
            bool v = true;
            const int n = m_ModPage * 4;
            for (int i = 0; i < 4 && n + i < categoryCount; ++i) {
                Category *category = config->GetCategory(n + i);
                ImGui::SetCursorScreenPos(ImVec2(vpSize.x * 0.4031f, y + vpSize.y * (0.13f + (float) i * 0.06f)));
                if (Bui::LevelButton(category->GetName(), &v)) {
                    m_CurrentCategory = category;
                    ShowMenu(MENU_MOD_OPTIONS);
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, Bui::GetMenuColor());

                    const ImVec2 commentBoxPos(vpSize.x * 0.725f, vpSize.y * 0.4f);
                    const ImVec2 commentBoxSize(vpSize.x * 0.25f, vpSize.y * 0.2f);
                    ImGui::SetCursorScreenPos(commentBoxPos);
                    ImGui::BeginChild("ModComment", commentBoxSize);

                    {
                        const float textWidth = ImGui::CalcTextSize(category->GetName()).x;
                        const float indent = (commentBoxSize.x - textWidth) * 0.5f;
                        if (indent > 0) {
                            ImGui::SetCursorPosX(indent);
                            ImGui::PushTextWrapPos(indent + textWidth);
                        } else {
                            ImGui::PushTextWrapPos(commentBoxSize.x);
                        }
                        ImGui::TextUnformatted(category->GetName());
                        ImGui::PopTextWrapPos();
                    }

                    {
                        const float textWidth = ImGui::CalcTextSize(category->GetComment()).x;
                        const float indent = (commentBoxSize.x - textWidth) * 0.5f;
                        if (indent > 0) {
                            ImGui::SetCursorPosX(indent);
                            ImGui::PushTextWrapPos(indent + textWidth);
                        } else {
                            ImGui::PushTextWrapPos(commentBoxSize.x);
                        }
                        ImGui::TextUnformatted(category->GetComment());
                        ImGui::PopTextWrapPos();
                    }

                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                }
            }
        }
    }

    ImGui::SetCursorScreenPos(Bui::CoordToScreenPos(ImVec2(0.4031f, 0.85f)));
    if (Bui::BackButton("ModBack") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        m_ModPage = 0;
        ShowPreviousMenu();
    }

    ImGui::End();
}

void BMLMod::OnDrawModOptions() {
    if (!m_CurrentCategory)
        return;

    ImGui::Begin("Mod Options", nullptr, MenuWinFlags);

    {
        float oldScale = ImGui::GetFont()->Scale;
        ImGui::GetFont()->Scale *= 1.5f;
        ImGui::PushFont(ImGui::GetFont());

        const auto titleSize = ImGui::CalcTextSize(m_CurrentCategory->GetName());
        const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
        ImGui::GetWindowDrawList()->AddText(ImVec2((vpSize.x - titleSize.x) / 2.0f, vpSize.y * 0.13f), IM_COL32_WHITE,
                                            m_CurrentCategory->GetName());

        ImGui::GetFont()->Scale = oldScale;
        ImGui::PopFont();
    }

    const int optionCount = (int) m_CurrentCategory->GetPropertyCount();
    const int maxPage = ((optionCount % 4) == 0) ? optionCount / 4 : optionCount / 4 + 1;

    if (m_ModOptionPage > 0) {
        ImGui::SetCursorScreenPos(Bui::CoordToScreenPos(ImVec2(0.36f, 0.124f)));
        if (Bui::LeftButton("ModOptionPrevPage")) {
            --m_ModOptionPage;
        }
    }

    if (maxPage > 1 && m_ModOptionPage < maxPage - 1) {
        ImGui::SetCursorScreenPos(Bui::CoordToScreenPos(ImVec2(0.6038f, 0.124f)));
        if (Bui::RightButton("ModOptionNextPage")) {
            ++m_ModOptionPage;
        }
    }

    {
        static char buffers[4][4096];
        static size_t bufferHashes[4];
        static bool keyToggled[4];
        static ImGuiKeyChord keyChord[4];

        const int n = m_ModOptionPage * 4;
        for (int i = 0; i < 4 && n + i < optionCount; ++i) {
            Property *property = m_CurrentCategory->GetProperty(n + i);
            ImGui::SetCursorScreenPos(Bui::CoordToScreenPos(ImVec2(0.35f, 0.24f + (float) i * 0.14f)));
            switch (property->GetType()) {
                case IProperty::STRING: {
                    if (bufferHashes[i] != property->GetHash()) {
                        bufferHashes[i] = property->GetHash();
                        strncpy(buffers[i], property->GetString(), property->GetStringSize() + 1);
                    }
                    Bui::InputTextButton(property->GetName(), buffers[i], sizeof(buffers[i]));
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        bufferHashes[i] = utils::HashString(buffers[i]);
                        if (bufferHashes[i] != property->GetHash())
                            property->SetString(buffers[i]);
                    }
                }
                    break;
                case IProperty::BOOLEAN:
                    if (Bui::YesNoButton(property->GetName(), property->GetBooleanPtr())) {
                        property->SetModified();
                    }
                    break;
                case IProperty::INTEGER:
                    if (Bui::InputIntButton(property->GetName(), property->GetIntegerPtr())) {
                        property->SetModified();
                    }
                    break;
                case IProperty::KEY:
                    keyChord[i] = Bui::CKKeyToImGuiKey(property->GetKey());
                    if (Bui::KeyButton(property->GetName(), &keyToggled[i], &keyChord[i])) {
                        keyChord[i] &= ~ImGuiMod_Mask_;
                        property->SetKey(Bui::ImGuiKeyToCKKey(static_cast<ImGuiKey>(keyChord[i])));
                    }
                    break;
                case IProperty::FLOAT:
                    if (Bui::InputFloatButton(property->GetName(), property->GetFloatPtr())) {
                        property->SetModified();
                    }
                    break;
                default:
                    ImGui::Dummy(Bui::GetButtonSize(Bui::BUTTON_OPTION));
                    break;
            }

            if (ImGui::IsItemHovered()) {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, Bui::GetMenuColor());

                ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
                const ImVec2 commentBoxPos(vpSize.x * 0.725f, vpSize.y * 0.4f);
                const ImVec2 commentBoxSize(vpSize.x * 0.25f, vpSize.y * 0.2f);
                ImGui::SetCursorScreenPos(commentBoxPos);
                ImGui::BeginChild("ModComment", commentBoxSize);

                {
                    const float textWidth = ImGui::CalcTextSize(property->GetName()).x;
                    const float indent = (commentBoxSize.x - textWidth) * 0.5f;
                    if (indent > 0) {
                        ImGui::SetCursorPosX(indent);
                        ImGui::PushTextWrapPos(indent + textWidth);
                    } else {
                        ImGui::PushTextWrapPos(commentBoxSize.x);
                    }
                    ImGui::TextUnformatted(property->GetName());
                    ImGui::PopTextWrapPos();
                }

                {
                    const float textWidth = ImGui::CalcTextSize(property->GetComment()).x;
                    const float indent = (commentBoxSize.x - textWidth) * 0.5f;
                    if (indent > 0) {
                        ImGui::SetCursorPosX(indent);
                        ImGui::PushTextWrapPos(indent + textWidth);
                    } else {
                        ImGui::PushTextWrapPos(commentBoxSize.x);
                    }
                    ImGui::TextUnformatted(property->GetComment());
                    ImGui::PopTextWrapPos();
                }

                ImGui::EndChild();
                ImGui::PopStyleColor();
            }
        }
    }

    ImGui::SetCursorScreenPos(Bui::CoordToScreenPos(ImVec2(0.4031f, 0.85f)));
    if (Bui::BackButton("ModOptionsBack") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        m_ModOptionPage = 0;
        ShowPreviousMenu();
    }

    ImGui::End();
}

void BMLMod::OnDrawMapList() {
    constexpr auto TitleText = "Custom Maps";

    ImGui::Begin(TitleText, nullptr, MenuWinFlags);

    const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;

    {
        float oldScale = ImGui::GetFont()->Scale;
        ImGui::GetFont()->Scale *= 1.5f;
        ImGui::PushFont(ImGui::GetFont());

        const auto titleSize = ImGui::CalcTextSize(TitleText);
        ImGui::GetWindowDrawList()->AddText(ImVec2((vpSize.x - titleSize.x) / 2.0f, vpSize.y * 0.07f), IM_COL32_WHITE,
                                            TitleText);

        ImGui::GetFont()->Scale = oldScale;
        ImGui::PopFont();
    }

    ImGui::PushStyleColor(ImGuiCol_FrameBg, Bui::GetMenuColor());

    ImGui::SetCursorScreenPos(ImVec2(vpSize.x * 0.4f, vpSize.y * 0.18f));
    ImGui::SetNextItemWidth(vpSize.x * 0.2f);

    if (ImGui::InputText("##SearchBar", m_MapSearchBuf, IM_ARRAYSIZE(m_MapSearchBuf))) {
        OnSearchMaps();
    }

    ImGui::PopStyleColor();

    int mapCount = (m_MapSearchBuf[0] == '\0') ? (int) m_Maps.size() : (int) m_MapSearchResult.size();
    int maxPage = ((mapCount % 10) == 0) ? mapCount / 10 : mapCount / 10 + 1;

    ImGui::SetCursorScreenPos(ImVec2(vpSize.x * 0.34f, vpSize.y * 0.4f));
    if (Bui::LeftButton("MapListPrevPage")) {
        if (m_MapPage == 0) {
            ExitMapMenu();
        } else if (m_MapPage > 0) {
            --m_MapPage;
        }
    }

    if (maxPage > 1 && m_MapPage < maxPage - 1) {
        ImGui::SetCursorScreenPos(ImVec2(vpSize.x * 0.6238f, vpSize.y * 0.4f));
        if (Bui::RightButton("MapListNextPage")) {
            ++m_MapPage;
        }
    }

    bool v = true;
    const int n = m_MapPage * 10;

    if (m_MapSearchBuf[0] == '\0') {
        for (int i = 0; i < 10 && n + i < mapCount; ++i) {
            auto &info = m_Maps[n + i];

            ImGui::SetCursorScreenPos(Bui::CoordToScreenPos(ImVec2(0.4031f, 0.23f + (float) i * 0.06f)));
            if (Bui::LevelButton(info.name.c_str(), &v)) {
                ExitMapMenu(false);
                LoadMap(info.path);
            }
        }
    } else {
        for (int i = 0; i < 10 && n + i < mapCount; ++i) {
            auto &info = m_Maps[m_MapSearchResult[n + i]];

            ImGui::SetCursorScreenPos(Bui::CoordToScreenPos(ImVec2(0.4031f, 0.23f + (float) i * 0.06f)));
            if (Bui::LevelButton(info.name.c_str(), &v)) {
                ExitMapMenu(false);
                LoadMap(info.path);
            }
        }
    }

    ImGui::SetCursorScreenPos(Bui::CoordToScreenPos(ImVec2(0.4031f, 0.85f)));
    if (Bui::BackButton("MapListBack") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        m_MapPage = 0;
        ExitMapMenu();
    }

    ImGui::End();
}

void BMLMod::OnSearchMaps() {
    m_MapSearchResult.clear();

    if (m_MapSearchBuf[0] == '\0')
        return;

    auto *pattern = (OnigUChar *) m_MapSearchBuf;
    regex_t *reg;
    OnigErrorInfo einfo;

    int r = onig_new(&reg, pattern, pattern + strlen((char *) pattern),
                     ONIG_OPTION_DEFAULT, ONIG_ENCODING_UTF8, ONIG_SYNTAX_ASIS, &einfo);
    if (r != ONIG_NORMAL) {
        char s[ONIG_MAX_ERROR_MESSAGE_LEN];
        onig_error_code_to_str((UChar *) s, r, &einfo);
        GetLogger()->Error(s);
        return;
    }

    for (size_t i = 0; i < m_Maps.size(); ++i) {
        auto &name = m_Maps[i].name;
        const auto *end = (const UChar *) (name.c_str() + name.size());
        const auto *start = (const UChar *) name.c_str();
        const auto *range = end;

        r = onig_search(reg, start, end, start, range, nullptr, ONIG_OPTION_NONE);
        if (r >= 0) {
            m_MapSearchResult.push_back(i);
        } else if (r != ONIG_MISMATCH) {
            char s[ONIG_MAX_ERROR_MESSAGE_LEN];
            onig_error_code_to_str((UChar *) s, r);
            GetLogger()->Error(s);
        }
    }

    onig_free(reg);
}

void BMLMod::OnResize() {
    ImGui::GetIO().FontGlobalScale = m_WindowRect.GetHeight() / 1200.0f;

    CKCamera *cams[] = {
        m_BML->GetTargetCameraByName("Cam_MenuLevel"),
        m_BML->GetTargetCameraByName("InGameCam")
    };

    for (CKCamera *cam : cams) {
        if (cam) {
            cam->SetAspectRatio((int)m_WindowRect.GetWidth(), (int)m_WindowRect.GetHeight());
            cam->SetFov(0.75f * m_WindowRect.GetWidth() / m_WindowRect.GetHeight());
            CKStateChunk *chunk = CKSaveObjectState(cam);

            m_BML->RestoreIC(cam);
            cam->SetAspectRatio((int)m_WindowRect.GetWidth(), (int)m_WindowRect.GetHeight());
            cam->SetFov(0.75f * m_WindowRect.GetWidth() / m_WindowRect.GetHeight());
            m_BML->SetIC(cam);

            CKReadObjectState(cam, chunk);
        }
    }
}
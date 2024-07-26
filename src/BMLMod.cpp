#include "BMLMod.h"

#include <map>
#include <io.h>

#include <oniguruma.h>
#include <utf8.h>
#include "imgui_internal.h"

#include "BML/Bui.h"
#include "BML/Gui.h"
#include "BML/InputHook.h"
#include "BML/ExecuteBB.h"
#include "BML/ScriptHelper.h"
#include "ModManager.h"
#include "Commands.h"
#include "Config.h"
#include "RenderHook.h"
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
        path = BML_GetModManager()->GetDirectoryUtf8(BML_DIR_LOADER);
        path.append("\\Fonts\\").append(filename);
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

    wchar_t filename[1024];
    wchar_t ext[64];
    _wsplitpath(path.c_str(), nullptr, nullptr, filename, ext);

    size_t hash = utils::HashString(filename);

    wchar_t buf[1024];
    _snwprintf(buf, sizeof(buf) / sizeof(wchar_t),
               L"%s\\Maps\\%8X%s", BML_GetModManager()->GetDirectory(BML_DIR_TEMP), hash, ext);
    buf[1023] = '\0';

    if (!utils::DuplicateFileW(path, buf))
        return "";

    char str[1024];
    utils::Utf16ToAnsi(buf, str, sizeof(str));
    return str;
}

void ModMenu::Init() {
    m_Pages.push_back(std::make_unique<ModListPage>(this));
    m_Pages.push_back(std::make_unique<ModPage>(this));
    m_Pages.push_back(std::make_unique<ModOptionPage>(this));
}

void ModMenu::Shutdown() {
    m_Pages.clear();
}

void ModMenu::OnOpen() {
    m_Mod->OnOpenModsMenu();
}

void ModMenu::OnClose() {
    for (auto &page : m_Pages) {
        page->SetPage(0);
    }
    m_Mod->OnCloseModsMenu();
}

IBML *ModMenu::GetBML() {
    return static_cast<IBML *>(BML_GetModManager());
}

Config *ModMenu::GetConfig(IMod *mod) {
    return BML_GetModManager()->GetConfig(mod);
}

ModMenuPage::ModMenuPage(ModMenu *menu, std::string name) : Page(std::move(name)), m_Menu(menu) {
    m_Menu->AddPage(this);
}

ModMenuPage::~ModMenuPage() {
    m_Menu->RemovePage(this);
}

void ModMenuPage::OnClose() {
    m_Menu->ShowPrevPage();
}

void ModListPage::OnAfterBegin() {
    int count = ModMenu::GetBML()->GetModCount();
    SetMaxPage(((count % 4) == 0) ? count / 4 : count / 4 + 1);

    Page::OnAfterBegin();
}

void ModListPage::OnDraw() {
    const int n = GetPage() * 4;

    DrawEntries([&](std::size_t index) {
        IMod *mod = ModMenu::GetBML()->GetMod((int)(n + index));
        if (!mod)
            return false;

        char buf[256];
        sprintf(buf, "%s", mod->GetID());
        if (Bui::MainButton(buf)) {
            m_Menu->SetCurrentMod(mod);
            m_Menu->ShowPage("Mod Page");
        }
        return true;
    });
}

void ModPage::OnAfterBegin() {
    if (!IsVisible())
        return;

    const auto menuPos = Bui::GetMenuPos();
    const auto menuSize = Bui::GetMenuSize();

    ImGui::SetCursorPosX(menuPos.x);
    ImGui::Dummy(Bui::CoordToPixel(ImVec2(0.375f, 0.1f)));

    auto *mod = m_Menu->GetCurrentMod();

    ImGui::SetCursorPosX(menuPos.x);
    WrappedText(mod->GetName(), menuSize.x, 1.2f);

    snprintf(m_TextBuf, sizeof(m_TextBuf), "By %s", mod->GetAuthor());
    ImGui::SetCursorPosX(menuPos.x);
    WrappedText(m_TextBuf, menuSize.x);

    snprintf(m_TextBuf, sizeof(m_TextBuf), "v%s", mod->GetVersion());
    ImGui::SetCursorPosX(menuPos.x);
    WrappedText(m_TextBuf, menuSize.x);

    ImGui::SetCursorPosX(menuPos.x);
    ImGui::NewLine();

    ImGui::SetCursorPosX(menuPos.x);
    WrappedText(mod->GetDescription(), menuSize.x);

    m_Config = ModMenu::GetConfig(mod);
    if (!m_Config)
        return;

    int count = (int) m_Config->GetCategoryCount();
    SetMaxPage(((count % 4) == 0) ? count / 4 : count / 4 + 1);

    if (m_PageIndex > 0 &&
        LeftButton("PrevPage")) {
        PrevPage();
    }

    if (m_PageCount > 1 && m_PageIndex < m_PageCount - 1 &&
        RightButton("NextPage")) {
        NextPage();
    }
}

void ModPage::OnDraw() {
    if (!m_Config)
        return;

    bool v = true;
    const int n = GetPage() * 4;

    DrawEntries([&](std::size_t index) {
        Category *category = m_Config->GetCategory((int)(n + index));
        if (!category)
            return false;

        if (Bui::LevelButton(category->GetName(), &v)) {
            m_Menu->SetCurrentCategory(category);
            m_Menu->ShowPage("Mod Options");
        }

        if (ImGui::IsItemHovered()) {
            ShowCommentBox(category);
        }
        return true;
    }, ImVec2(0.4031f, 0.5f), 0.06f, 4);
}

void ModPage::ShowCommentBox(Category *category) {
    if (!category)
        return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Bui::GetMenuColor());

    const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
    const ImVec2 commentBoxPos(vpSize.x * 0.725f, vpSize.y * 0.4f);
    const ImVec2 commentBoxSize(vpSize.x * 0.25f, vpSize.y * 0.2f);
    ImGui::SetCursorScreenPos(commentBoxPos);
    ImGui::BeginChild("ModComment", commentBoxSize);

    WrappedText(category->GetName(), commentBoxSize.x);
    WrappedText(category->GetComment(), commentBoxSize.x);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void ModOptionPage::OnAfterBegin() {
    m_Category = m_Menu->GetCurrentCategory();
    if (!m_Category)
        return;

    int count = (int) m_Category->GetPropertyCount();
    SetMaxPage(((count % 4) == 0) ? count / 4 : count / 4 + 1);

    Page::OnAfterBegin();
}

void ModOptionPage::OnDraw() {
    if (!m_Category)
        return;

    const int n = GetPage() * 4;

    DrawEntries([&](std::size_t index) {
        Property *property = m_Category->GetProperty((int)(n + index));
        if (!property)
            return false;

        const char *name = property->GetName();
        if (!name || name[0] == '\0')
            return true;

        switch (property->GetType()) {
            case IProperty::STRING: {
                if (m_BufferHashes[index] != property->GetHash()) {
                    m_BufferHashes[index] = property->GetHash();
                    strncpy(m_Buffers[index], property->GetString(), property->GetStringSize() + 1);
                }
                Bui::InputTextButton(property->GetName(), m_Buffers[index], sizeof(m_Buffers[index]));
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    m_BufferHashes[index] = utils::HashString(m_Buffers[index]);
                    if (m_BufferHashes[index] != property->GetHash())
                        property->SetString(m_Buffers[index]);
                }
            }
                break;
            case IProperty::BOOLEAN:
                if (Bui::YesNoButton(property->GetName(), property->GetBooleanPtr())) {
                    property->SetModified();
                }
                break;
            case IProperty::INTEGER:
                if (m_IntFlags[index] == 0) {
                    m_IntValues[index] = property->GetInteger();
                    m_IntFlags[index] = 1;
                }
                if (Bui::InputIntButton(property->GetName(), &m_IntValues[index])) {
                    if (m_IntValues[index] != property->GetInteger()) {
                        m_IntFlags[index] = 2;
                    }
                }
                if (ImGui::IsItemDeactivated() && m_IntFlags[index] == 2) {
                    property->SetInteger(m_IntValues[index]);
                    m_IntFlags[index] = 1;
                }
                break;
            case IProperty::KEY:
                m_KeyChord[index] = Bui::CKKeyToImGuiKey(property->GetKey());
                if (Bui::KeyButton(property->GetName(), &m_KeyToggled[index], &m_KeyChord[index])) {
                    m_KeyChord[index] &= ~ImGuiMod_Mask_;
                    property->SetKey(Bui::ImGuiKeyToCKKey(static_cast<ImGuiKey>(m_KeyChord[index])));
                }
                break;
            case IProperty::FLOAT:
                if (m_FloatFlags[index] == 0) {
                    m_FloatValues[index] = property->GetFloat();
                    m_FloatFlags[index] = 1;
                }
                if (Bui::InputFloatButton(property->GetName(), &m_FloatValues[index])) {
                    if (fabs(m_FloatValues[index] - property->GetFloat()) < EPSILON) {
                        m_FloatFlags[index] = 2;
                    }
                }
                if (ImGui::IsItemDeactivated() && m_FloatFlags[index] == 2) {
                    property->SetFloat(m_FloatValues[index]);
                    m_FloatFlags[index] = 1;
                }
                break;
            default:
                ImGui::Dummy(Bui::GetButtonSize(Bui::BUTTON_OPTION));
                break;
        }

        if (ImGui::IsItemHovered()) {
            ShowCommentBox(property);
        }

        return true;
    });
}

void ModOptionPage::OnClose() {
    memset(m_IntFlags, 0, sizeof(m_IntFlags));
    memset(m_FloatFlags, 0, sizeof(m_FloatFlags));
    ModMenuPage::OnClose();
}

void ModOptionPage::ShowCommentBox(Property *property) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Bui::GetMenuColor());

    const ImVec2& vpSize = ImGui::GetMainViewport()->Size;
    const ImVec2 commentBoxPos(vpSize.x * 0.725f, vpSize.y * 0.35f);
    const ImVec2 commentBoxSize(vpSize.x * 0.25f, vpSize.y * 0.3f);
    ImGui::SetCursorScreenPos(commentBoxPos);
    ImGui::BeginChild("ModOptionComment", commentBoxSize);

    WrappedText(property->GetName(), commentBoxSize.x);
    WrappedText(property->GetComment(), commentBoxSize.x);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void MapListPage::OnAfterBegin() {
    if (!IsVisible())
        return;

    DrawCenteredText(m_Title.c_str(), 0.07f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, Bui::GetMenuColor());

    const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
    ImGui::SetCursorScreenPos(ImVec2(vpSize.x * 0.4f, vpSize.y * 0.18f));
    ImGui::SetNextItemWidth(vpSize.x * 0.2f);

    if (ImGui::InputText("##SearchBar", m_MapSearchBuf, IM_ARRAYSIZE(m_MapSearchBuf))) {
        OnSearchMaps();
    }

    ImGui::PopStyleColor();

    int count = (m_MapSearchBuf[0] == '\0') ? (int) m_Maps.size() : (int) m_MapSearchResult.size();
    SetMaxPage(((count % 10) == 0) ? count / 10 : count / 10 + 1);

    if (m_PageIndex > 0 &&
        LeftButton("PrevPage", ImVec2(0.36f, 0.4f))) {
        PrevPage();
    }

    if (m_PageCount > 1 && m_PageIndex < m_PageCount - 1 &&
        RightButton("NextPage", ImVec2(0.6238f, 0.4f))) {
        NextPage();
    }
}

void MapListPage::OnDraw() {
    if (m_Maps.empty())
        return;

    bool v = true;
    const int n = GetPage() * 10;

    if (m_MapSearchBuf[0] == '\0') {
        DrawEntries([&](std::size_t index) {
            return OnDrawEntry(n + index, &v);
        }, ImVec2(0.4031f, 0.23f), 0.06f, 10);
    } else {
        DrawEntries([&](std::size_t index) {
            if (index >= m_MapSearchResult.size())
                return false;
            return OnDrawEntry(m_MapSearchResult[n + index], &v);
        }, ImVec2(0.4031f, 0.23f), 0.06f, 10);
    }
}

bool MapListPage::OnOpen() {
    m_Mod->OnOpenMapMenu();
    RefreshMaps();
    return true;
}

void MapListPage::OnClose() {
    m_Mod->OnCloseMapMenu();
}

void MapListPage::RefreshMaps() {
    std::wstring path = BML_GetModManager()->GetDirectory(BML_DIR_LOADER);
    path.append(L"\\Maps");

    m_Maps.clear();
    ExploreMaps(path, m_Maps);
}

size_t MapListPage::ExploreMaps(const std::wstring &path, std::vector<MapInfo> &maps) {
    if (path.empty() || !utils::DirectoryExistsW(path))
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

void MapListPage::OnSearchMaps() {
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
        m_Mod->GetLogger()->Error(s);
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
            m_Mod->GetLogger()->Error(s);
        }
    }

    onig_free(reg);
}

bool MapListPage::OnDrawEntry(std::size_t index, bool *v) {
    if (index >= m_Maps.size())
        return false;
    auto &info = m_Maps[index];
    if (Bui::LevelButton(info.name.c_str(), v)) {
        Hide();
        m_Mod->OnCloseMapMenu(false);
        m_Mod->LoadMap(info.path);
    }
    return true;
}

void BMLMod::OnLoad() {
    m_DataShare = BML_GetDataShare(nullptr);
    m_CKContext = m_BML->GetCKContext();
    m_RenderContext = m_BML->GetRenderContext();
    m_TimeManager = m_BML->GetTimeManager();
    m_InputHook = m_BML->GetInputManager();
    m_RenderContext->Get2dRoot(TRUE)->GetRect(m_WindowRect);

    ExecuteBB::Init();

    InitConfigs();
    RegisterCommands();
    InitGUI();

    m_ModMenu.Init();
    m_MapListPage = std::make_unique<MapListPage>(this);
}

void BMLMod::OnUnload() {
    m_ModMenu.Shutdown();
    m_MapListPage.reset();
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
    } else if (prop == m_WidescreenFix) {
        RenderHook::EnableWidescreenFix(m_WidescreenFix->GetBoolean());
    } else if (prop == m_Overclock) {
        for (int i = 0; i < 3; i++)
            m_OverclockLinks[i]->SetOutBehaviorIO(m_OverclockLinkIO[i][m_Overclock->GetBoolean()]);
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

    RenderHook::EnableWidescreenFix(m_WidescreenFix->GetBoolean());
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
    m_ModMenu.Open("Mod List");
}

void BMLMod::CloseModsMenu() {
    m_ModMenu.Close();
}

void BMLMod::OpenMapMenu() {
    m_MapListPage->Open();
}

void BMLMod::CloseMapMenu() {
    m_MapListPage->Close();
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

    std::string mapPath = utils::Utf16ToUtf8(path);
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

    m_ShowTitle = GetConfig()->GetProperty("Misc", "ShowTitle");
    m_ShowTitle->SetComment("Show BML Title at top");
    m_ShowTitle->SetDefaultBoolean(true);

    m_ShowFPS = GetConfig()->GetProperty("Misc", "ShowFPS");
    m_ShowFPS->SetComment("Show FPS at top-left corner");
    m_ShowFPS->SetDefaultBoolean(true);

    m_ShowSR = GetConfig()->GetProperty("Misc", "ShowSRTimer");
    m_ShowSR->SetComment("Show SR Timer above Time Score");
    m_ShowSR->SetDefaultBoolean(true);

    m_UnlockFPS = GetConfig()->GetProperty("Misc", "UnlockFrameRate");
    m_UnlockFPS->SetComment("Unlock Frame Rate Limitation");
    m_UnlockFPS->SetDefaultBoolean(false);

    m_FPSLimit = GetConfig()->GetProperty("Misc", "SetMaxFrameRate");
    m_FPSLimit->SetComment("Set Frame Rate Limitation, this option will not work if frame rate is unlocked. Set to 0 will turn on VSync");
    m_FPSLimit->SetDefaultInteger(0);

    m_LanternAlphaTest = GetConfig()->GetProperty("Misc", "LanternAlphaTest");
    m_LanternAlphaTest->SetComment("Enable alpha test for lantern material, this option can increase FPS");
    m_LanternAlphaTest->SetDefaultBoolean(true);

    m_WidescreenFix = GetConfig()->GetProperty("Misc", "WidescreenFix");
    m_WidescreenFix->SetComment("Improve widescreen resolutions support");
    m_WidescreenFix->SetDefaultBoolean(false);

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

    m_Overclock = GetConfig()->GetProperty("Misc", "Overclock");
    m_Overclock->SetComment("Remove delay of spawn / respawn");
    m_Overclock->SetDefaultBoolean(false);

    GetConfig()->SetCategoryComment("GUI", "Settings for BML's GUI");

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

    CKBehavior *smat = ScriptHelper::FindFirstBB(script, "set Mapping and Textures");
    if (!smat) return;
    CKBehavior *sml = ScriptHelper::FindFirstBB(smat, "Set Mat Laterne");
    if (!sml) return;
    CKBehavior *sat = ScriptHelper::FindFirstBB(sml, "Set Alpha Test");
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

        float oldScale = ImGui::GetFont()->Scale;
        ImGui::GetFont()->Scale *= 1.2f;
        ImGui::PushFont(ImGui::GetFont());

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

        ImGui::GetFont()->Scale = oldScale;
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
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Bui::GetMenuColor());

    const ImVec2 vpSize = ImGui::GetMainViewport()->Size;
    ImVec2 size(vpSize.x * 0.96f, 0.0f);

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
                                              ImGuiWindowFlags_NoCollapse |
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
                                          ImGuiWindowFlags_NoCollapse |
                                          ImGuiWindowFlags_NoBackground |
                                          ImGuiWindowFlags_NoNav |
                                          ImGuiWindowFlags_NoFocusOnAppearing |
                                          ImGuiWindowFlags_NoBringToFrontOnFocus;

    const int count = std::min(MSG_MAXSIZE, m_MsgCount);
    float ly = ImGui::GetTextLineHeightWithSpacing();
    ImVec2 pos(vpSize.x * 0.02f, vpSize.y * 0.9f - (float) count * ly);
    size.y = (float) count * ly;
    ImVec4 bgColorVec4 = Bui::GetMenuColor();

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::Begin("Messages", nullptr, MsgFlags);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    const ImVec2 contentSize = ImGui::GetContentRegionMax();

    ImVec2 msgPos = cursorPos;
    ImDrawList *drawList = ImGui::GetWindowDrawList();

    if (m_CmdTyping) {
        for (int i = 0; i < count; i++) {
            msgPos.y = cursorPos.y + (float) (count - i) * ly;

            drawList->AddRectFilled(msgPos, ImVec2(msgPos.x + contentSize.x, msgPos.y + ly), ImGui::GetColorU32(bgColorVec4));
            drawList->AddText(msgPos, IM_COL32_WHITE, m_Msgs[i].Text);
        }
    } else {
        for (int i = 0; i < count; i++) {
            const float timer = m_Msgs[i].Timer;
            if (timer >= 0) {
                msgPos.y = cursorPos.y + (float) (count - i) * ly;
                bgColorVec4.w = std::min(110.0f, (timer / 20.0f)) / 255.0f;

                drawList->AddRectFilled(msgPos, ImVec2(msgPos.x + contentSize.x, msgPos.y + ly), ImGui::GetColorU32(bgColorVec4));
                drawList->AddText(msgPos, IM_COL32_WHITE, m_Msgs[i].Text);
            }
        }
    }

    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

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

void BMLMod::OnDrawMenu() {
    m_ModMenu.Render();
    m_MapListPage->Render();
}

void BMLMod::OnResize() {
    ImGui::GetIO().FontGlobalScale = m_WindowRect.GetHeight() / 1200.0f;
}

void BMLMod::OnOpenModsMenu() {
    m_InputHook->Block(CK_INPUT_DEVICE_KEYBOARD);
}

void BMLMod::OnCloseModsMenu() {
    CKBehavior *beh = m_BML->GetScriptByName("Menu_Options");
    m_CKContext->GetCurrentScene()->Activate(beh, true);
    m_BML->AddTimerLoop(1ul, [this] {
        if (m_InputHook->oIsKeyDown(CKKEY_ESCAPE) || m_InputHook->oIsKeyDown(CKKEY_RETURN))
            return true;
        m_InputHook->Unblock(CK_INPUT_DEVICE_KEYBOARD);
        return false;
    });
}

void BMLMod::OnOpenMapMenu() {
    m_InputHook->Block(CK_INPUT_DEVICE_KEYBOARD);
}

void BMLMod::OnCloseMapMenu(bool backToMenu) {
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
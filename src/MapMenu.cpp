#include "MapMenu.h"

#include <io.h>

#include <oniguruma.h>

#include "BML/InputHook.h"
#include "BML/ScriptHelper.h"

#include "ModManager.h"
#include "StringUtils.h"
#include "PathUtils.h"

#include "BMLMod.h"

using namespace ScriptHelper;

void MapMenu::Init() {
    m_MapListPage = std::make_unique<MapListPage>(this);
}

void MapMenu::Shutdown() {
    m_MapListPage.reset();
}

void MapMenu::OnOpen() {
    BML_GetModManager()->GetInputManager()->Block(CK_INPUT_DEVICE_KEYBOARD);
}

void MapMenu::OnClose() {
    if (m_MapLoaded)
        OnClose(false);
    else
        OnClose(true);
    m_MapLoaded = false;
}

void MapMenu::OnClose(bool backToMenu) {
    auto *context = BML_GetCKContext();
    auto *modManager = BML_GetModManager();
    auto *inputHook = modManager->GetInputManager();

    if (backToMenu) {
        CKBehavior *beh = modManager->GetScriptByName("Menu_Start");
        context->GetCurrentScene()->Activate(beh, true);
    }

    modManager->AddTimerLoop(1ul, [this, inputHook] {
        if (inputHook->oIsKeyDown(CKKEY_ESCAPE) || inputHook->oIsKeyDown(CKKEY_RETURN))
            return true;
        inputHook->Unblock(CK_INPUT_DEVICE_KEYBOARD);
        return false;
    });
}

void MapMenu::LoadMap(const std::wstring &path) {
    m_Mod->LoadMap(path);
    m_MapLoaded = true;
    Close();
}

MapListPage::MapListPage(MapMenu *menu): Page("Custom Maps"), m_Menu(menu) {
    m_Menu->AddPage(this);
}

MapListPage::~MapListPage() {
    m_Menu->RemovePage(this);
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

void MapListPage::OnShow() {
    RefreshMaps();
}

void MapListPage::OnClose() {
    m_Menu->ShowPrevPage();
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
        BML_GetModManager()->GetLogger()->Error(s);
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
            BML_GetModManager()->GetLogger()->Error(s);
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
        m_Menu->LoadMap(info.path);
    }
    // if (ImGui::IsItemHovered()) {
    //     ImGui::SetTooltip("%s", info.name.c_str());
    // }
    return true;
}

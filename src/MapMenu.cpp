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

MapMenu::MapMenu(BMLMod *mod): m_Mod(mod), m_Maps(new MapEntry(nullptr, MAP_ENTRY_DIR)) {}

MapMenu::~MapMenu() {
    delete m_Maps;
}

void MapMenu::Init() {
    m_MapListPage = std::make_unique<MapListPage>(this);

    RefreshMaps();
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

void MapMenu::RefreshMaps() {
    std::wstring path = BML_GetModManager()->GetDirectory(BML_DIR_LOADER);
    path.append(L"\\Maps");

    if (!utils::DirectoryExistsW(path))
        return;

    auto *maps = new MapEntry(nullptr, MAP_ENTRY_DIR);
    maps->name = "Maps";
    maps->path = path;
    if (ExploreMaps(maps)) {
        delete m_Maps;
        m_Maps = maps;
    } else {
        delete maps;
    }

    ResetCurrentMaps();
}

bool MapMenu::ExploreMaps(MapEntry *maps, int depth) {
    if (depth <= 0)
        return false;

    if (!maps || maps->type != MAP_ENTRY_DIR || maps->path.empty())
        return false;

    const std::wstring p = maps->path + L"\\*";
    _wfinddata_t fileinfo = {};
    auto handle = _wfindfirst(p.c_str(), &fileinfo);
    if (handle == -1)
        return false;

    do {
        std::wstring fullPath = maps->path;
        fullPath.append(L"\\").append(fileinfo.name);

        if (fileinfo.attrib & _A_SUBDIR) {
            if (wcscmp(fileinfo.name, L".") != 0 && wcscmp(fileinfo.name, L"..") != 0) {
                wchar_t dir[1024];
                _wsplitpath(fileinfo.name, nullptr, nullptr, dir, nullptr);

                auto *entry = new MapEntry(maps, MAP_ENTRY_DIR);
                entry->name = utils::Utf16ToUtf8(dir);
                entry->path = fullPath;
                maps->children.push_back(entry);
                ExploreMaps(entry, depth - 1);
            }
        } else if (IsSupportedFileType(fileinfo.name)) {
            wchar_t filename[1024];
            _wsplitpath(fileinfo.name, nullptr, nullptr, filename, nullptr);

            auto *entry = new MapEntry(maps, MAP_ENTRY_FILE);
            entry->type = MAP_ENTRY_FILE;
            entry->name = utils::Utf16ToUtf8(filename);
            entry->path = fullPath;
            maps->children.push_back(entry);
        }
    } while (_wfindnext(handle, &fileinfo) == 0);

    _findclose(handle);

    std::sort(maps->children.begin(), maps->children.end(), [](const MapEntry *a, const MapEntry *b) {
        return a->type > b->type;
    });

    return !maps->children.empty();
}

bool MapMenu::IsSupportedFileType(const std::wstring &path) {
    if (path.empty())
        return false;

    wchar_t ext[64];
    _wsplitpath(path.c_str(), nullptr, nullptr, nullptr, ext);

    return wcsicmp(ext, L".nmo") == 0 || wcsicmp(ext, L".cmo") == 0;
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

    auto *maps = m_Menu->GetCurrentMaps();
    if (!maps || maps->children.empty()) {
        m_Count = 0;
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_FrameBg, Bui::GetMenuColor());

    const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
    ImGui::SetCursorScreenPos(ImVec2(vpSize.x * 0.4f, vpSize.y * 0.18f));
    ImGui::SetNextItemWidth(vpSize.x * 0.2f);

    if (ImGui::InputText("##SearchBar", m_MapSearchBuf, IM_ARRAYSIZE(m_MapSearchBuf))) {
        OnSearchMaps();
    }

    ImGui::PopStyleColor();

    m_Count = IsSearching() ? (int) m_MapSearchResult.size() : (int) maps->children.size();
    SetMaxPage(m_Count % 10 == 0 ? m_Count / 10 : m_Count / 10 + 1);

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
    if (m_Count == 0)
        return;

    bool v = true;
    const int n = GetPage() * 10;

    if (IsSearching()) {
        DrawEntries([&](std::size_t index) {
            if (index >= m_MapSearchResult.size())
                return false;
            return OnDrawEntry(m_MapSearchResult[n + index], &v);
        }, ImVec2(0.4031f, 0.23f), 0.06f, 10);
    } else {
        DrawEntries([&](std::size_t index) {
            return OnDrawEntry(n + index, &v);
        }, ImVec2(0.4031f, 0.23f), 0.06f, 10);
    }
}

void MapListPage::OnClose() {
    auto *maps = m_Menu->GetCurrentMaps();
    if (maps && maps->parent) {
        m_Menu->SetCurrentMaps(maps->parent);
        Show();
    } else {
        m_Menu->ShowPrevPage();
    }
}

bool MapListPage::IsSearching() const {
    return m_MapSearchBuf[0] != '\0';
}

void MapListPage::ClearSearch() {
    m_MapSearchBuf[0] = '\0';
    m_MapSearchResult.clear();
}

void MapListPage::OnSearchMaps() {
    m_MapSearchResult.clear();

    if (!IsSearching() || m_Count == 0)
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

    const auto &maps = m_Menu->GetCurrentMaps()->children;
    for (size_t i = 0; i < maps.size(); ++i) {
        const auto &name = maps[i]->name;
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

bool MapListPage::OnDrawEntry(size_t index, bool *v) {
    const auto &maps = m_Menu->GetCurrentMaps()->children;
    if (index >= maps.size())
        return false;
    auto &entry = maps[index];

    if (entry->type == MAP_ENTRY_FILE) {
        if (Bui::LevelButton(entry->name.c_str(), v)) {
            Close();
            m_Menu->LoadMap(entry->path);
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, 0xFFFFA500); // Orange Color

        if (Bui::LevelButton(entry->name.c_str(), v)) {
            m_Menu->SetCurrentMaps(entry);
            ClearSearch();
        }

        ImGui::PopStyleColor();
    }

    if (m_Menu->ShouldShowTooltip() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", entry->name.c_str());
    }
    return true;
}

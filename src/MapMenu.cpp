#include "MapMenu.h"

#include <io.h>

#include <oniguruma.h>

#include "BML/InputHook.h"
#include "BML/ScriptHelper.h"

#include "ModContext.h"
#include "StringUtils.h"
#include "PathUtils.h"

#include "BMLMod.h"

using namespace ScriptHelper;

MapMenu::MapMenu(BMLMod *mod): m_Mod(mod), m_Maps(new MapEntry(nullptr, MAP_ENTRY_DIR)) {
}

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
    BML_GetModContext()->GetInputManager()->Block(CK_INPUT_DEVICE_KEYBOARD);
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
    auto *modManager = BML_GetModContext();
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
    if (path.empty()) {
        BML_GetModContext()->GetLogger()->Error("Attempted to load empty map path");
        return;
    }

    if (!utils::FileExistsW(path)) {
        BML_GetModContext()->GetLogger()->Error("Map file does not exist: %s", utils::Utf16ToUtf8(path).c_str());
        return;
    }

    try {
        m_Mod->LoadMap(path);
        m_MapLoaded = true;
        Close();
    } catch (const std::exception &e) {
        BML_GetModContext()->GetLogger()->Error("Exception loading map: %s", e.what());
        m_MapLoaded = false;
    } catch (...) {
        BML_GetModContext()->GetLogger()->Error("Unknown exception loading map");
        m_MapLoaded = false;
    }
}

void MapMenu::RefreshMaps() {
    std::wstring path = BML_GetModContext()->GetDirectory(BML_DIR_LOADER);
    path.append(L"\\Maps");

    if (!utils::DirectoryExistsW(path)) {
        BML_GetModContext()->GetLogger()->Info("Maps directory does not exist: %s", utils::Utf16ToUtf8(path).c_str());
        return;
    }

    auto *newMaps = new(std::nothrow) MapEntry(nullptr, MAP_ENTRY_DIR);
    if (!newMaps) {
        BML_GetModContext()->GetLogger()->Error("Failed to allocate memory for maps root");
        return;
    }

    newMaps->name = "Maps";
    newMaps->path = path;

    try {
        if (ExploreMaps(newMaps, m_MaxDepth)) {
            // Only replace if successful
            delete m_Maps;
            m_Maps = newMaps;
            ResetCurrentMaps();
        } else {
            delete newMaps;
            BML_GetModContext()->GetLogger()->Warn("No maps found in directory");
        }
    } catch (...) {
        delete newMaps;
        BML_GetModContext()->GetLogger()->Error("Exception during map refresh");
    }
}

bool MapMenu::ExploreMaps(MapEntry *maps, int depth) {
    if (!maps || maps->type != MAP_ENTRY_DIR || maps->path.empty())
        return false;

    if (depth <= 0)
        return false;

    const std::wstring p = maps->path + L"\\*";
    _wfinddata_t fileinfo = {};
    auto handle = _wfindfirst(p.c_str(), &fileinfo);

    if (handle == -1) {
        int err = errno;
        std::string errMsg = "Failed to explore maps directory " + utils::Utf16ToUtf8(maps->path) + ": ";
        errMsg += strerror(err);
        BML_GetModContext()->GetLogger()->Error(errMsg.c_str());
        return false;
    }

    bool foundAny = false;
    try {
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
                    if (ExploreMaps(entry, depth - 1))
                        foundAny = true;
                }
            } else if (IsSupportedFileType(fileinfo.name)) {
                wchar_t filename[1024];
                _wsplitpath(fileinfo.name, nullptr, nullptr, filename, nullptr);

                auto *entry = new MapEntry(maps, MAP_ENTRY_FILE);
                entry->name = utils::Utf16ToUtf8(filename);
                entry->path = fullPath;
                maps->children.push_back(entry);
                foundAny = true;
            }
        } while (_wfindnext(handle, &fileinfo) == 0);
    } catch (const std::exception &e) {
        BML_GetModContext()->GetLogger()->Error("Exception in ExploreMaps: %s", e.what());
    }
    catch (...) {
        BML_GetModContext()->GetLogger()->Error("Unknown exception in ExploreMaps");
    }

    _findclose(handle);

    if (!maps->children.empty()) {
        std::sort(maps->children.begin(), maps->children.end(), [](const MapEntry *lhs, const MapEntry *rhs) {
            return *lhs < *rhs;
        });
        foundAny = true;
    }

    return foundAny;
}

bool MapMenu::IsSupportedFileType(const std::wstring &path) {
    if (path.empty())
        return false;

    size_t dotPos = path.find_last_of(L'.');
    if (dotPos == std::wstring::npos)
        return false;

    std::wstring ext = path.substr(dotPos);

    return _wcsicmp(ext.c_str(), L".nmo") == 0 || _wcsicmp(ext.c_str(), L".cmo") == 0;
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

    m_Count = IsSearching() ? static_cast<int>(m_MapSearchResult.size()) : static_cast<int>(maps->children.size());
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
        DrawEntries([&](size_t index) {
            if (n + index >= m_MapSearchResult.size())
                return false;
            return OnDrawEntry(m_MapSearchResult[n + index], &v);
        }, ImVec2(0.4031f, 0.23f), 0.06f, 10);
    } else {
        DrawEntries([&](size_t index) {
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

    SetPage(0);
}

bool MapListPage::IsSearching() const {
    return m_MapSearchBuf[0] != '\0';
}

void MapListPage::ClearSearch() {
    m_MapSearchBuf[0] = '\0';
    m_MapSearchResult.clear();
}

void MapListPage::OnSearchMaps() {
    SetPage(0);
    m_MapSearchResult.clear();

    if (!IsSearching() || m_Count == 0)
        return;

    auto *pattern = (OnigUChar *) m_MapSearchBuf;
    regex_t *reg = nullptr;
    OnigErrorInfo einfo;

    try {
        int r = onig_new(&reg, pattern, pattern + strlen((char *) pattern),
                         ONIG_OPTION_DEFAULT, ONIG_ENCODING_UTF8, ONIG_SYNTAX_ASIS, &einfo);
        if (r != ONIG_NORMAL) {
            char s[ONIG_MAX_ERROR_MESSAGE_LEN];
            onig_error_code_to_str((UChar *) s, r, &einfo);
            BML_GetModContext()->GetLogger()->Error(s);
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
                BML_GetModContext()->GetLogger()->Error(s);
            }
        }
    } catch (...) {
        if (reg)
            onig_free(reg);
        throw;
    }

    if (reg)
        onig_free(reg);
}

bool MapListPage::OnDrawEntry(size_t index, bool *v) {
    const auto &maps = m_Menu->GetCurrentMaps()->children;
    if (index >= maps.size())
        return false;
    auto &entry = maps[index];

    if (entry->type == MAP_ENTRY_FILE) {
        if (Bui::LevelButton(entry->name.c_str(), v)) {
            m_Menu->ResetCurrentMaps();
            SetPage(0);
            Hide();
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

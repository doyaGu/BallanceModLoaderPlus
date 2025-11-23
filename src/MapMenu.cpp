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

MapEntry::~MapEntry() {
    if (m_BeingDeleted) {
        return; // Prevent recursive deletion
    }
    m_BeingDeleted = true;

    // Safely delete all children first
    for (auto *child : children) {
        if (child && !child->m_BeingDeleted) {
            child->parent = nullptr; // Break parent link to prevent recursion
            delete child;
        }
    }
    children.clear();

    // Remove from parent's children list safely
    if (parent && !parent->m_BeingDeleted) {
        auto it = std::find(parent->children.begin(), parent->children.end(), this);
        if (it != parent->children.end()) {
            parent->children.erase(it);
        }
    }
}

bool MapEntry::operator<(const MapEntry &rhs) const {
    if (type != rhs.type) {
        return type < rhs.type;
    }
    return utils::CompareString(name, rhs.name) < 0;
}

MapMenu::MapMenu(BMLMod *mod): m_Mod(mod), m_Maps(new MapEntry(nullptr, MAP_ENTRY_DIR)) {}

MapMenu::~MapMenu() {
    delete m_Maps;
}

void MapMenu::Init() {
    CreatePage<MapListPage>();

    RefreshMaps();
}

void MapMenu::OnOpen() {
    Bui::BlockKeyboardInput();
}

void MapMenu::OnClose() {
    if (m_MapLoaded)
        OnClose(false);
    else
        OnClose(true);
    m_MapLoaded = false;
}

void MapMenu::OnClose(bool backToMenu) {
    if (backToMenu)
        Bui::TransitionToScriptAndUnblock("Menu_Start");
    else
        Bui::UnblockKeyboardAfterRelease();
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
    if (!maps || maps->type != MAP_ENTRY_DIR || maps->path.empty()) {
        return false;
    }

    if (depth <= 0) {
        return false;
    }

    std::wstring searchPath = maps->path;
    if (searchPath.length() > MAX_PATH) {
        BML_GetModContext()->GetLogger()->Error("Path too long: %s", utils::Utf16ToUtf8(maps->path).c_str());
        return false;
    }

    searchPath.append(L"\\*");

    _wfinddata_t fileinfo = {};
    intptr_t handle = _wfindfirst(searchPath.c_str(), &fileinfo);

    if (handle == -1) {
        int err = errno;
        if (err != ENOENT) {
            std::string errMsg = "Failed to explore maps directory " + utils::Utf16ToUtf8(maps->path) + ": ";
            errMsg += strerror(err);
            BML_GetModContext()->GetLogger()->Error(errMsg.c_str());
        }
        return false;
    }

    bool foundAny = false;

    struct FileHandleGuard {
        intptr_t handle;
        explicit FileHandleGuard(intptr_t h) : handle(h) {}
        ~FileHandleGuard() {
            if (handle != -1) {
                _findclose(handle);
            }
        }
    } guard(handle);

    try {
        do {
            // Skip current and parent directories
            if (wcscmp(fileinfo.name, L".") == 0 || wcscmp(fileinfo.name, L"..") == 0) {
                continue;
            }

            std::wstring fullPath = maps->path;
            if (fullPath.length() + wcslen(fileinfo.name) + 2 > MAX_PATH) {
                BML_GetModContext()->GetLogger()->Warn("Skipping file with path too long: %s\\%s",
                    utils::Utf16ToUtf8(maps->path).c_str(), utils::Utf16ToUtf8(fileinfo.name).c_str());
                continue;
            }

            fullPath.append(L"\\").append(fileinfo.name);

            if (fileinfo.attrib & _A_SUBDIR) {
                std::wstring dirName = fileinfo.name;

                auto *entry = new(std::nothrow) MapEntry(maps, MAP_ENTRY_DIR);
                if (!entry) {
                    BML_GetModContext()->GetLogger()->Error("Memory allocation failed for directory entry");
                    break;
                }

                entry->name = utils::Utf16ToUtf8(dirName);
                entry->path = fullPath;

                maps->children.push_back(entry);

                if (ExploreMaps(entry, depth - 1)) {
                    foundAny = true;
                }
            } else if (IsSupportedFileType(fileinfo.name)) {
                std::wstring filename = fileinfo.name;
                size_t dotPos = filename.find_last_of(L'.');
                if (dotPos != std::wstring::npos) {
                    filename = filename.substr(0, dotPos);
                }

                auto *entry = new(std::nothrow) MapEntry(maps, MAP_ENTRY_FILE);
                if (!entry) {
                    BML_GetModContext()->GetLogger()->Error("Memory allocation failed for file entry");
                    break;
                }

                entry->name = utils::Utf16ToUtf8(filename);
                entry->path = fullPath;

                maps->children.push_back(entry);
                foundAny = true;
            }
        } while (_wfindnext(handle, &fileinfo) == 0);

    } catch (const std::exception &e) {
        BML_GetModContext()->GetLogger()->Error("Exception in ExploreMaps: %s", e.what());
        return false;
    } catch (...) {
        BML_GetModContext()->GetLogger()->Error("Unknown exception in ExploreMaps");
        return false;
    }

    // Sort children safely
    if (!maps->children.empty()) {
        try {
            std::sort(maps->children.begin(), maps->children.end(),
                [](const MapEntry *lhs, const MapEntry *rhs) {
                    if (!lhs || !rhs) return false;
                    return *lhs < *rhs;
                });
            foundAny = true;
        } catch (...) {
            BML_GetModContext()->GetLogger()->Error("Failed to sort map entries");
        }
    }

    return foundAny;
}

bool MapMenu::IsSupportedFileType(const std::wstring &path) {
    if (path.empty()) {
        return false;
    }

    size_t dotPos = path.find_last_of(L'.');
    if (dotPos == std::wstring::npos || dotPos >= path.length() - 1) {
        return false;
    }

    std::wstring ext = path.substr(dotPos);

    // Convert to lowercase for comparison
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

    return ext == L".nmo" || ext == L".cmo";
}

void MapListPage::OnPostBegin() {
    Bui::Title(m_Title.c_str(), 0.07f);

    auto *maps = dynamic_cast<MapMenu *>(m_Menu)->GetCurrentMaps();
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

    // When not searching, ensure current directory entries are shown as "directories first, then by name"
    if (!IsSearching() && !maps->children.empty()) {
        try {
            std::sort(maps->children.begin(), maps->children.end(), [](const MapEntry *lhs, const MapEntry *rhs) {
                if (!lhs || !rhs) return false;
                return *lhs < *rhs; // operator< implements dir-first then name
            });
        } catch (...) {
            BML_GetModContext()->GetLogger()->Error("Failed to sort current directory entries");
        }
    }

    m_Count = IsSearching() ? static_cast<int>(m_MapSearchResult.size()) : static_cast<int>(maps->children.size());
    // Keep page count in sync with total entries (10 per page)
    SetPageCount(Bui::CalcPageCount(m_Count, 10));

    if (Bui::CanPrevPage(m_PageIndex) &&
        Bui::NavLeft(0.36f, 0.4f)) {
        PrevPage();
    }

    // Only show Next when there are more items beyond the current page
    if (Bui::CanNextPage(m_PageIndex, m_Count, 10) &&
        Bui::NavRight(0.6238f, 0.4f)) {
        NextPage();
    }
}

void MapListPage::OnPreEnd() {
    auto *mapMenu = dynamic_cast<MapMenu *>(m_Menu);
    if (!mapMenu) {
        Page::OnPreEnd();
        return;
    }

    MapEntry *current = mapMenu->GetCurrentMaps();
    if (!Bui::NavBack()) {
        return;
    }

    if (current && current->parent) {
        mapMenu->SetCurrentMaps(current->parent);
        SetPage(0);
        ClearSearch();
        return;
    }

    if (m_Menu) {
        m_Menu->OpenPrevPage();
    } else {
        Close();
    }
}

void MapListPage::OnDraw() {
    if (m_Count == 0)
        return;

    bool v = true;
    const int n = GetPage() * 10;

    if (IsSearching()) {
        Bui::Entries([&](size_t index) {
            if (n + index >= m_MapSearchResult.size())
                return false;
            return OnDrawEntry(m_MapSearchResult[n + index], &v);
        }, 0.4031f, 0.23f, 0.06f, 10);
    } else {
        auto *currentMaps = dynamic_cast<MapMenu *>(m_Menu)->GetCurrentMaps();
        const auto &maps = currentMaps->children;
        Bui::Entries([&](size_t index) {
            if (n + index >= maps.size())
                return false;
            return OnDrawEntry(maps[n + index], &v);
        }, 0.4031f, 0.23f, 0.06f, 10);
    }
}

void MapListPage::OnPostEnd() {
    if (m_ShouldClose) {
        m_ShouldClose = false;
        auto *maps = dynamic_cast<MapMenu *>(m_Menu)->GetCurrentMaps();
        if (maps && maps->parent) {
            dynamic_cast<MapMenu *>(m_Menu)->SetCurrentMaps(maps->parent);
            Show();
        } else {
            m_Menu->OpenPrevPage();
        }

        SetPage(0);
    }
}

bool MapListPage::IsSearching() const {
    return m_MapSearchBuf[0] != '\0';
}

void MapListPage::ClearSearch() {
    memset(m_MapSearchBuf, 0, sizeof(m_MapSearchBuf));
    m_MapSearchResult.clear();
}

void MapListPage::OnSearchMaps() {
    SetPage(0);
    m_MapSearchResult.clear();

    if (!IsSearching())
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

        // Depth-first traversal starting at current maps to search across subfolders
        auto *root = dynamic_cast<MapMenu *>(m_Menu)->GetCurrentMaps();
        if (!root) {
            onig_free(reg);
            return;
        }

        std::vector<MapEntry *> stack;
        stack.reserve(64);
        stack.push_back(root);

        while (!stack.empty()) {
            MapEntry *node = stack.back();
            stack.pop_back();

            for (MapEntry *child : node->children) {
                if (!child) continue;

                const auto &name = child->name;
                const auto *end = (const UChar *) (name.c_str() + name.size());
                const auto *start = (const UChar *) name.c_str();
                const auto *range = end;

                r = onig_search(reg, start, end, start, range, nullptr, ONIG_OPTION_NONE);
                if (r >= 0) {
                    m_MapSearchResult.push_back(child);
                } else if (r != ONIG_MISMATCH) {
                    char s[ONIG_MAX_ERROR_MESSAGE_LEN];
                    onig_error_code_to_str((UChar *) s, r);
                    BML_GetModContext()->GetLogger()->Error(s);
                }

                if (child->type == MAP_ENTRY_DIR) {
                    stack.push_back(child);
                }
            }
        }
    } catch (...) {
        if (reg)
            onig_free(reg);
        throw;
    }

    if (reg)
        onig_free(reg);

    // Sort search results: directories first, then by name
    if (!m_MapSearchResult.empty()) {
        try {
            std::sort(m_MapSearchResult.begin(), m_MapSearchResult.end(), [](const MapEntry *a, const MapEntry *b) {
                if (a->type != b->type) return a->type < b->type; // MAP_ENTRY_DIR < MAP_ENTRY_FILE
                return utils::CompareString(a->name, b->name) < 0;
            });
        } catch (...) {
            BML_GetModContext()->GetLogger()->Error("Failed to sort search results");
        }
    }
}

bool MapListPage::OnDrawEntry(MapEntry *entry, bool *v) {
    if (!entry) return false;

    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 0.8f);

    if (entry->type == MAP_ENTRY_FILE) {
        if (Bui::LevelButton(entry->name.c_str(), v)) {
            // Reset to root before loading map to close the menu properly
            dynamic_cast<MapMenu *>(m_Menu)->ResetCurrentMaps();
            dynamic_cast<MapMenu *>(m_Menu)->LoadMap(entry->path);
            m_ShouldClose = true;
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, 0xFFFFA500); // Orange Color for directory

        if (Bui::LevelButton(entry->name.c_str(), v)) {
            dynamic_cast<MapMenu *>(m_Menu)->SetCurrentMaps(entry);
            // When entering a folder from search or normal list, reset to first page and clear search
            SetPage(0);
            ClearSearch();
        }

        ImGui::PopStyleColor();
    }

    ImGui::PopFont();

    if (dynamic_cast<MapMenu *>(m_Menu)->ShouldShowTooltip() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", entry->name.c_str());
    }

    return true;
}

#include "MapMenu.h"

#include "BuiInternal.h"

#include <oniguruma.h>

#include "BML/InputHook.h"
#include "BML/ILogger.h"
#include "BML/ScriptHelper.h"

#include "StringUtils.h"
#include "PathUtils.h"

using namespace ScriptHelper;

MapMenu::MapMenu(MapMenuState::MapLoader loader)
    : m_State(std::move(loader)),
      m_Routes(
          [owner = this]() { Bui::BlockKeyboardInput(owner); },
          [state = &m_State, owner = this]() {
              if (owner->m_ShuttingDown) {
                  Bui::UnblockKeyboardAfterRelease(owner);
                  return;
              }
              if (state->TakeMapLoaded())
                  Bui::UnblockKeyboardAfterRelease(owner);
              else
                  Bui::TransitionToScriptAndUnblock("Menu_Start", owner);
          }) {}

MapMenu::~MapMenu() {
    Shutdown();
}

void MapMenu::Init(const std::wstring &mapsDirectory, ILogger &logger) {
    m_State.BindCatalog(mapsDirectory, logger);
    if (!m_Initialized) {
        if (!m_Routes.CreatePage<MapListPage>("Custom Maps", m_State)) {
            logger.Error("Failed to initialize the custom maps page");
            return;
        }
        m_Initialized = true;
    }

    m_Active = true;
    m_State.RefreshMaps();
}

void MapMenu::Shutdown() {
    if (!m_Active || m_ShuttingDown)
        return;
    m_ShuttingDown = true;
    m_Routes.Close();
    m_ShuttingDown = false;
    m_Active = false;
}

void MapMenu::SetMaxDepth(int depth) {
    if (m_State.SetMaxDepth(depth) && m_Active)
        m_State.RefreshMaps();
}

bool MapMenuState::SetMaxDepth(int depth) {
    depth = std::max(1, depth);
    if (m_MaxDepth == depth)
        return false;

    m_MaxDepth = depth;
    return true;
}

void MapMenuState::BindCatalog(std::wstring mapsDirectory, ILogger &logger) {
    m_MapsDirectory = std::move(mapsDirectory);
    m_Logger = &logger;
}

void MapMenuState::RefreshMaps() {
    const bool directoryExists = utils::DirectoryExistsW(m_MapsDirectory);

    if (!m_Catalog.Refresh(m_MapsDirectory, m_MaxDepth, m_Logger)) {
        if (m_Logger) {
            m_Logger->Error("Failed to refresh maps directory: %s",
                            utils::Utf16ToUtf8(m_MapsDirectory).c_str());
        }
        return;
    }

    if (!directoryExists && m_Logger) {
        m_Logger->Info("Maps directory does not exist: %s",
                       utils::Utf16ToUtf8(m_MapsDirectory).c_str());
    }

    ++m_CatalogRevision;
    ResetCurrentMaps();
    if (directoryExists && m_Catalog.GetRoot()->children.empty() && m_Logger)
        m_Logger->Warn("No maps found in directory");
}

void MapListPage::SyncCatalog() {
    if (m_CatalogRevision == m_State.GetCatalogRevision())
        return;

    m_CatalogRevision = m_State.GetCatalogRevision();
    ClearSearch();
    m_Count = 0;
    m_Pagination.Reset();
}

Bui::PageAction MapListPage::OnFrame() {
    SyncCatalog();
    Bui::Title("Custom Maps", 0.07f);

    MapEntry *maps = m_State.GetCurrentMaps();
    if (!maps || maps->children.empty()) {
        m_Count = 0;
        m_Pagination.Update(0, 10);
    } else {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Bui::GetMenuColor());

        const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
        ImGui::SetCursorScreenPos(ImVec2(vpSize.x * 0.4f, vpSize.y * 0.18f));
        ImGui::SetNextItemWidth(vpSize.x * 0.2f);

        if (ImGui::InputText("##SearchBar", m_MapSearchBuf, IM_ARRAYSIZE(m_MapSearchBuf))) {
            OnSearchMaps();
        }

        ImGui::PopStyleColor();

        if (!IsSearching() && !maps->children.empty()) {
            try {
                std::sort(maps->children.begin(), maps->children.end(), [](const MapEntry *lhs, const MapEntry *rhs) {
                    if (!lhs || !rhs) return false;
                    return *lhs < *rhs;
                });
            } catch (...) {
                if (ILogger *logger = m_State.GetLogger())
                    logger->Error("Failed to sort current directory entries");
            }
        }

        m_Count = IsSearching()
            ? static_cast<int>(m_MapSearchResult.size())
            : static_cast<int>(maps->children.size());
        m_Pagination.Update(m_Count, 10);

        if (m_Pagination.CanPrevious() && Bui::NavLeft(0.36f, 0.4f))
            m_Pagination.Previous();
        if (m_Pagination.CanNext() && Bui::NavRight(0.6238f, 0.4f))
            m_Pagination.Next();
    }

    Bui::PageAction action;
    if (m_Count > 0) {
        bool v = true;
        const int n = m_Pagination.GetFirstItem();

        if (IsSearching()) {
            Bui::Entries([&](size_t index) {
                if (n + index >= m_MapSearchResult.size())
                    return false;
                return OnDrawEntry(m_MapSearchResult[n + index], &v, action);
            }, 0.4031f, 0.23f, 0.06f, 10);
        } else {
            MapEntry *currentMaps = m_State.GetCurrentMaps();
            if (currentMaps) {
                const auto &entries = currentMaps->children;
                Bui::Entries([&](size_t index) {
                    if (n + index >= entries.size())
                        return false;
                    return OnDrawEntry(entries[n + index], &v, action);
                }, 0.4031f, 0.23f, 0.06f, 10);
            }
        }
    }

    if (!action.IsNone())
        return action;

    if (Bui::NavBack()) {
        MapEntry *current = m_State.GetCurrentMaps();
        if (current && current->parent) {
            m_State.SetCurrentMaps(current->parent);
            m_Pagination.Reset();
            ClearSearch();
        } else {
            return Bui::PageAction::Back();
        }
    }

    return Bui::PageAction::None();
}

bool MapListPage::IsSearching() const {
    return m_MapSearchBuf[0] != '\0';
}

void MapListPage::ClearSearch() {
    memset(m_MapSearchBuf, 0, sizeof(m_MapSearchBuf));
    m_MapSearchResult.clear();
}

void MapListPage::OnSearchMaps() {
    m_Pagination.Reset();
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
            if (ILogger *logger = m_State.GetLogger())
                logger->Error("%s", s);
            return;
        }

        // Depth-first traversal starting at current maps to search across subfolders
        auto *root = m_State.GetCurrentMaps();
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
                    if (ILogger *logger = m_State.GetLogger())
                        logger->Error("%s", s);
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
            if (ILogger *logger = m_State.GetLogger())
                logger->Error("Failed to sort search results");
        }
    }
}

bool MapListPage::OnDrawEntry(MapEntry *entry, bool *v, Bui::PageAction &action) {
    if (!entry) return false;

    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 0.8f);

    if (entry->type == MAP_ENTRY_FILE) {
        if (Bui::LevelButton(entry->name.c_str(), v)) {
            action = m_State.SelectMap(entry->path);
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 165, 0, 255)); // Orange Color for directory

        if (Bui::LevelButton(entry->name.c_str(), v)) {
            m_State.SetCurrentMaps(entry);
            // When entering a folder from search or normal list, reset to first page and clear search
            m_Pagination.Reset();
            ClearSearch();
        }

        ImGui::PopStyleColor();
    }

    ImGui::PopFont();

    if (m_State.ShouldShowTooltip() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", entry->name.c_str());
    }

    return true;
}

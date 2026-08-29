#ifndef BML_MAPMENU_H
#define BML_MAPMENU_H

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <functional>
#include <utility>

#include "BML/Bui.h"
#include "MapCatalog.h"

class ILogger;

class MapMenuState {
public:
    using MapLoader = std::function<bool(const std::wstring &)>;

    explicit MapMenuState(MapLoader loader)
        : m_LoadMap(std::move(loader)), m_Current(m_Catalog.GetRoot()) {}

    bool LoadMap(const std::wstring &path) {
        m_MapLoaded = false;
        if (!m_LoadMap || !m_LoadMap(path))
            return false;
        m_MapLoaded = true;
        return true;
    }

    Bui::PageAction SelectMap(const std::wstring &path) {
        if (!LoadMap(path))
            return Bui::PageAction::None();
        ResetCurrentMaps();
        return Bui::PageAction::Close();
    }

    void BindCatalog(std::wstring mapsDirectory, ILogger &logger);
    void RefreshMaps();

    MapEntry *GetCurrentMaps() const { return m_Current; }
    void SetCurrentMaps(MapEntry *entry) { m_Current = entry; }
    void ResetCurrentMaps() { m_Current = m_Catalog.GetRoot(); }
    uint64_t GetCatalogRevision() const { return m_CatalogRevision; }

    bool ShouldShowTooltip() const { return m_ShowTooltip; }
    void SetShowTooltip(bool show) { m_ShowTooltip = show; }
    ILogger *GetLogger() const { return m_Logger; }
    bool SetMaxDepth(int depth);
    bool TakeMapLoaded() {
        const bool loaded = m_MapLoaded;
        m_MapLoaded = false;
        return loaded;
    }

private:
    MapLoader m_LoadMap;
    bool m_MapLoaded = false;
    bool m_ShowTooltip = false;
    int m_MaxDepth = 8;
    std::wstring m_MapsDirectory;
    ILogger *m_Logger = nullptr;
    MapCatalog m_Catalog;
    MapEntry *m_Current;
    uint64_t m_CatalogRevision = 0;
};

class MapListPage : public Bui::Page {
public:
    explicit MapListPage(MapMenuState &state) : m_State(state) {}

    Bui::PageAction OnFrame() override;

private:
    void SyncCatalog();
    bool IsSearching() const;
    void ClearSearch();
    void OnSearchMaps();
    // Draw by direct entry pointer (used for both normal list and recursive search results)
    bool OnDrawEntry(MapEntry *entry, bool *v, Bui::PageAction &action);

    MapMenuState &m_State;
    Bui::Pagination m_Pagination;
    uint64_t m_CatalogRevision = 0;
    int m_Count = 0;
    char m_MapSearchBuf[1024] = {};
    // Store pointers to entries to support recursive results across subfolders
    std::vector<MapEntry *> m_MapSearchResult;
};

class MapMenu {
public:
    explicit MapMenu(MapMenuState::MapLoader loader);
    ~MapMenu();

    void Init(const std::wstring &mapsDirectory, ILogger &logger);
    void Shutdown();

    bool Open(const std::string &id) { return m_Active && !m_ShuttingDown && m_Routes.Open(id); }
    bool Close() { return m_Routes.Close(); }
    bool Render() { return m_Routes.Render(); }

    void SetShowTooltip(bool show) { m_State.SetShowTooltip(show); }
    void SetMaxDepth(int depth);

private:
    // Routes is declared last so its Pages are destroyed before their state.
    MapMenuState m_State;
    bool m_Initialized = false;
    bool m_Active = false;
    bool m_ShuttingDown = false;
    Bui::Menu m_Routes;
};

#endif // BML_MAPMENU_H

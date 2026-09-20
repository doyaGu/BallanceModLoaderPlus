#ifndef BML_MAPMENU_H
#define BML_MAPMENU_H

#include <cstdint>
#include <string>
#include <vector>

#include "BML/Bui.h"
#include "CustomMaps/MapMenuState.h"

class ILogger;

class MapListPage : public Bui::Page {
public:
    explicit MapListPage(MapMenuState &state) : m_State(state) {}

    void OnEnter(Bui::PageEnterReason) override;
    Bui::PageAction OnFrame() override;

private:
    void SyncCatalog();
    bool IsSearching() const;
    void ClearSearch();
    void OnSearchMaps();
    // Draw by direct entry pointer (used for both normal list and recursive search results)
    bool OnDrawEntry(MapEntry *entry, bool *v);

    MapMenuState &m_State;
    Bui::Pagination m_Pagination;
    std::uint64_t m_CatalogRevision = 0;
    int m_Count = 0;
    bool m_FocusFirstEntry = true;
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

    bool Open(const std::string &id);
    bool Close() { return m_Routes.Close(); }
    bool Render();
    bool IsOpen() const { return m_Routes.IsOpen(); }
    bool CompleteLoad(bool loaded) { return m_State.CompleteLoad(loaded); }
    void ResetLoad() { m_State.ResetLoad(); }

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

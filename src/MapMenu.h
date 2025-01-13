#ifndef BML_MAPMENU_H
#define BML_MAPMENU_H

#include <string>
#include <vector>
#include <memory>

#include "BML/Bui.h"

class BMLMod;
class MapMenu;

enum MapEntryType {
    MAP_ENTRY_FILE,
    MAP_ENTRY_DIR
};

struct MapEntry {
    MapEntry *parent = nullptr;
    MapEntryType type;
    std::string name;
    std::wstring path;
    std::vector<MapEntry *> children;

    explicit MapEntry(MapEntry *parent, MapEntryType entryType) : parent(parent), type(entryType) {}

    ~MapEntry() {
        for (auto *child : children)
            delete child;

        if (parent) {
            auto it = std::remove(parent->children.begin(), parent->children.end(), this);
            parent->children.erase(it, parent->children.end());
        }
    }
};

class MapListPage : public Bui::Page {
public:
    explicit MapListPage(MapMenu *menu);

    ~MapListPage() override;

    void OnAfterBegin() override;
    void OnDraw() override;
    void OnClose() override;

private:
    bool IsSearching() const;
    void ClearSearch();
    void OnSearchMaps();
    bool OnDrawEntry(size_t index, bool *v);

    MapMenu *m_Menu;
    int m_Count = 0;
    char m_MapSearchBuf[65536] = {};
    std::vector<size_t> m_MapSearchResult;
};

class MapMenu : public Bui::Menu {
public:
    explicit MapMenu(BMLMod *mod);

    ~MapMenu() override;

    void Init();
    void Shutdown();

    void OnOpen() override;
    void OnClose() override;
    void OnClose(bool backToMenu);

    void LoadMap(const std::wstring &path);
    MapEntry *GetMaps() const { return m_Maps; }
    MapEntry *GetCurrentMaps() const { return m_Current; }
    void SetCurrentMaps(MapEntry *entry) { m_Current = entry; }
    void ResetCurrentMaps() { m_Current = m_Maps; }
    void RefreshMaps();

    bool ShouldShowTooltip() const { return m_ShowTooltip; }
    void SetShowTooltip(bool show) { m_ShowTooltip = show; }

    int GetMaxDepth() const { return m_MaxDepth; }
    void SetMaxDepth(int depth) { m_MaxDepth = depth; }

private:
    bool ExploreMaps(MapEntry *maps, int depth = 8);
    static bool IsSupportedFileType(const std::wstring &path);

    BMLMod *m_Mod;
    bool m_MapLoaded = false;
    bool m_ShowTooltip = false;
    int m_MaxDepth = 8;
    MapEntry *m_Maps = nullptr;
    MapEntry *m_Current = nullptr;
    std::unique_ptr<MapListPage> m_MapListPage;
};

#endif // BML_MAPMENU_H

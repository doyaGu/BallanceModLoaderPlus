#ifndef BML_MAPMENU_H
#define BML_MAPMENU_H

#include <string>
#include <vector>
#include <memory>

#include "BML/Bui.h"

class BMLMod;
class MapMenu;

enum MapEntryType {
    MAP_ENTRY_DIR,
    MAP_ENTRY_FILE,
};

struct MapEntry {
    MapEntry *parent = nullptr;
    MapEntryType type;
    std::string name;
    std::wstring path;
    std::vector<MapEntry *> children;

    explicit MapEntry(MapEntry *parent, MapEntryType entryType) : parent(parent), type(entryType) {}

    ~MapEntry() {
        for (auto *child : children) {
            child->parent = nullptr;
            delete child;
        }
        children.clear();

        if (parent) {
            auto it = std::find(parent->children.begin(), parent->children.end(), this);
            if (it != parent->children.end()) {
                parent->children.erase(it);
            }
        }
    }

    bool operator<(const MapEntry &rhs) const {
        if (type < rhs.type)
            return true;
        if (rhs.type < type)
            return false;
        return path < rhs.path;
    }

    bool operator>(const MapEntry &rhs) const {
        return rhs < *this;
    }

    bool operator<=(const MapEntry &rhs) const {
        return !(*this > rhs);
    }

    bool operator>=(const MapEntry &rhs) const {
        return !(*this < rhs);
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
    char m_MapSearchBuf[1024] = {};
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

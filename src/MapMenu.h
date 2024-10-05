#ifndef BML_MAPMENU_H
#define BML_MAPMENU_H

#include <string>
#include <vector>
#include <memory>

#include "BML/Bui.h"

class BMLMod;
class MapMenu;

class MapListPage : public Bui::Page {
public:
    explicit MapListPage(MapMenu *menu);

    ~MapListPage() override;

    void OnAfterBegin() override;
    void OnDraw() override;
    void OnShow() override;
    void OnClose() override;

    void RefreshMaps();

private:
    struct MapInfo {
        std::string name;
        std::wstring path;
    };

    size_t ExploreMaps(const std::wstring &path, std::vector<MapInfo> &maps);
    void OnSearchMaps();
    bool OnDrawEntry(std::size_t index, bool *v);

    MapMenu *m_Menu;
    char m_MapSearchBuf[65536] = {};
    std::vector<size_t> m_MapSearchResult;
    std::vector<MapInfo> m_Maps;
};

class MapMenu : public Bui::Menu {
public:
    explicit MapMenu(BMLMod *mod) : m_Mod(mod) {}

    void Init();
    void Shutdown();

    void OnOpen() override;
    void OnClose() override;
    void OnClose(bool backToMenu);

    void LoadMap(const std::wstring &path);

    bool ShouldShowTooltip() const { return m_ShowTooltip; }
    void SetShowTooltip(bool show) { m_ShowTooltip = show; }

private:
    std::unique_ptr<MapListPage> m_MapListPage;
    BMLMod *m_Mod;
    bool m_MapLoaded = false;
    bool m_ShowTooltip = false;
};

#endif // BML_MAPMENU_H

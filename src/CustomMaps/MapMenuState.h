#ifndef BML_MAPMENUSTATE_H
#define BML_MAPMENUSTATE_H

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "CustomMaps/MapCatalog.h"

class ILogger;

class MapMenuState {
public:
    using MapLoader = std::function<bool(const std::wstring &)>;

    explicit MapMenuState(MapLoader loader)
        : m_LoadMap(std::move(loader)), m_Current(m_Catalog.GetRoot()) {}

    bool BeginLoad(const std::wstring &path);
    bool CompleteLoad(bool loaded);
    bool IsLoading() const;
    bool TakeCloseRequest();
    bool TakeMapLoaded();
    void ResetLoad();

    void BindCatalog(std::wstring mapsDirectory, ILogger &logger);
    void RefreshMaps();

    MapEntry *GetCurrentMaps() const { return m_Current; }
    void SetCurrentMaps(MapEntry *entry) { m_Current = entry; }
    void ResetCurrentMaps() { m_Current = m_Catalog.GetRoot(); }
    std::uint64_t GetCatalogRevision() const { return m_CatalogRevision; }

    bool ShouldShowTooltip() const { return m_ShowTooltip; }
    void SetShowTooltip(bool show) { m_ShowTooltip = show; }
    ILogger *GetLogger() const { return m_Logger; }
    bool SetMaxDepth(int depth);

private:
    enum class LoadState {
        Idle,
        Loading,
        CloseRequested,
        Closing,
    };

    MapLoader m_LoadMap;
    LoadState m_LoadState = LoadState::Idle;
    bool m_ShowTooltip = false;
    int m_MaxDepth = 8;
    std::wstring m_MapsDirectory;
    ILogger *m_Logger = nullptr;
    MapCatalog m_Catalog;
    MapEntry *m_Current;
    std::uint64_t m_CatalogRevision = 0;
};

#endif // BML_MAPMENUSTATE_H

#include "CustomMaps/MapMenuState.h"

#include <algorithm>

#include "BML/ILogger.h"

#include "PathUtils.h"
#include "StringUtils.h"

bool MapMenuState::BeginLoad(const std::wstring &path) {
    if (m_LoadState != LoadState::Idle || !m_LoadMap)
        return false;

    m_LoadState = LoadState::Loading;
    const bool started = m_LoadMap(path);
    if (!started && m_LoadState == LoadState::Loading)
        m_LoadState = LoadState::Idle;

    return m_LoadState != LoadState::Idle;
}

bool MapMenuState::CompleteLoad(bool loaded) {
    if (m_LoadState != LoadState::Loading)
        return false;

    m_LoadState = loaded ? LoadState::CloseRequested : LoadState::Idle;
    if (loaded)
        ResetCurrentMaps();
    return true;
}

bool MapMenuState::IsLoading() const {
    return m_LoadState == LoadState::Loading;
}

bool MapMenuState::TakeCloseRequest() {
    if (m_LoadState != LoadState::CloseRequested)
        return false;
    m_LoadState = LoadState::Closing;
    return true;
}

bool MapMenuState::TakeMapLoaded() {
    if (m_LoadState != LoadState::Closing)
        return false;
    m_LoadState = LoadState::Idle;
    return true;
}

void MapMenuState::ResetLoad() {
    m_LoadState = LoadState::Idle;
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

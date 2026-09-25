#ifndef BML_CUSTOM_MAPS_MAP_STAGING_H
#define BML_CUSTOM_MAPS_MAP_STAGING_H

#include <cstdint>
#include <string>

class CKPathManager;

namespace CustomMap {

struct StagedMap {
    std::wstring FilePath;
    std::string LoadPath;
};

class MapStaging {
public:
    MapStaging() = default;
    ~MapStaging();

    MapStaging(const MapStaging &) = delete;
    MapStaging &operator=(const MapStaging &) = delete;

    void Initialize(const std::wstring &gameDirectory, const std::wstring &tempDirectory);
    void Shutdown();

    bool Prepare(const std::wstring &sourcePath, std::uint64_t attempt,
                 CKPathManager *pathManager, StagedMap &staged, std::string &error) const;
    void AdoptLoaded(std::wstring filePath);
    void ReleaseLoaded();

private:
    static bool CanResolve(CKPathManager *pathManager, const std::string &loadPath);

    std::wstring m_TempDirectory;
    std::wstring m_RootDirectory;
    std::wstring m_SessionDirectory;
    std::wstring m_RelativeDirectory;
    std::wstring m_LoadedFile;
};

}

#endif // BML_CUSTOM_MAPS_MAP_STAGING_H

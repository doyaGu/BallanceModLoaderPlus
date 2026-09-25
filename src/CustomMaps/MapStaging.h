#ifndef BML_CUSTOM_MAPS_MAP_STAGING_H
#define BML_CUSTOM_MAPS_MAP_STAGING_H

#include <cstdint>
#include <string>
#include <vector>

class CKPathManager;

namespace CustomMap {

struct StagedMap {
    std::uint64_t Attempt = 0;
    std::string LoadPath;
};

enum class StagedMapCompletion {
    Loaded,
    Discarded,
    Retained,
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
                 CKPathManager *pathManager, StagedMap &staged, std::string &error);
    bool Complete(std::uint64_t attempt, StagedMapCompletion completion);
    void ReleaseLoaded();

private:
    static bool CanResolve(CKPathManager *pathManager, const std::string &loadPath);
    void DiscardFile(const std::wstring &path);

    std::wstring m_TempDirectory;
    std::wstring m_RootDirectory;
    std::wstring m_SessionDirectory;
    std::wstring m_RelativeDirectory;
    std::uint64_t m_PendingAttempt = 0;
    std::wstring m_PendingFile;
    std::wstring m_LoadedFile;
    std::vector<std::wstring> m_RetainedFiles;
};

}

#endif // BML_CUSTOM_MAPS_MAP_STAGING_H

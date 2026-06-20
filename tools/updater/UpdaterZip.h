#ifndef BML_UPDATER_ZIP_H
#define BML_UPDATER_ZIP_H

#include <string>
#include <vector>

#include "UpdaterTypes.h"

namespace bmlupdater {
    struct ZipEntryInfo {
        std::string path;
        uint64_t size{0};
        bool directory{false};
    };

    [[nodiscard]] Result EnumerateZipEntries(const std::wstring &zipPath, std::vector<ZipEntryInfo> &entries);
    [[nodiscard]] Result ValidateUpdaterZipEntries(const std::vector<ZipEntryInfo> &entries,
                                                   const UpdaterManifest &manifest);
    [[nodiscard]] Result ExtractUpdaterZipToStaging(const std::wstring &zipPath,
                                                    const UpdaterManifest &manifest,
                                                    const std::wstring &stagingRoot,
                                                    std::vector<StagedFile> &stagedFiles,
                                                    ProgressCallback progress = {});
} // namespace bmlupdater

#endif // BML_UPDATER_ZIP_H

#ifndef BML_CORE_PACKAGE_PATHS_H
#define BML_CORE_PACKAGE_PATHS_H

#include <filesystem>
#include <string>

namespace BML::Core {
    struct PackagePaths {
        std::filesystem::path mods_dir;
        std::filesystem::path packages_dir;
        std::filesystem::path archive_dir;
        std::filesystem::path rejected_dir;
        std::filesystem::path state_path;
        std::filesystem::path staging_dir;
    };

    inline PackagePaths GetPackagePaths(const std::wstring &modloader_dir) {
        std::filesystem::path root(modloader_dir);
        return {
            root / L"Mods",
            root / L"Packages",
            root / L"Packages" / L".bp-archive",
            root / L"Packages" / L".bp-rejected",
            root / L"Packages" / L".install-state" / L"packages.json",
            root / L"Packages" / L".staging",
        };
    }
} // namespace BML::Core

#endif // BML_CORE_PACKAGE_PATHS_H

#ifndef BML_CORE_PACKAGE_INSTALLER_H
#define BML_CORE_PACKAGE_INSTALLER_H

#include <string>
#include <vector>

namespace BML::Core {
    struct PackageRejectInfo {
        std::wstring rejected_bp_path;
        std::wstring sidecar_json_path;
        std::string stage;
        std::string message;
        std::string package_id;
        std::string version;
        std::string content_hash;
    };

    struct PackageSyncDiagnostics {
        std::vector<PackageRejectInfo> rejected;
        std::vector<std::string> warnings;
    };

    class PackageInstaller {
    public:
        [[nodiscard]] bool SyncPackages(const std::wstring &modloader_dir,
                                        PackageSyncDiagnostics &out_diag,
                                        std::string &out_error);
    };
} // namespace BML::Core

#endif // BML_CORE_PACKAGE_INSTALLER_H

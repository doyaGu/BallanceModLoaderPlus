#ifndef BML_CORE_PACKAGE_INSTALL_STATE_H
#define BML_CORE_PACKAGE_INSTALL_STATE_H

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace BML::Core {
    struct PackageRecord {
        std::string package_id;
        std::string version;
        std::string content_hash;
        std::wstring archive_path;
        std::string status;
        std::string entry;
        std::string manifest_hash;
        std::string installed_at_utc;
        std::string last_seen_at_utc;
    };

    struct InstalledRecord {
        std::string package_id;
        std::string current_hash;
        std::string version;
        std::wstring installed_path;
        std::string entry;
        std::string updated_at_utc;
    };

    struct PackageEvent {
        std::string time_utc;
        std::string type;
        std::string package_id;
        std::string content_hash;
        std::string message;
    };

    class PackageInstallState {
    public:
        [[nodiscard]] bool Load(const std::wstring &state_path, std::string &error);
        [[nodiscard]] bool Save(const std::wstring &state_path, std::string &error) const;
        [[nodiscard]] const std::string &GetLastLoadWarning() const;

        [[nodiscard]] const InstalledRecord *FindInstalled(std::string_view package_id) const;
        [[nodiscard]] const PackageRecord *FindPackageByHash(std::string_view content_hash) const;

        void UpsertInstalled(InstalledRecord record);
        void UpsertPackage(PackageRecord record);
        void AppendEvent(PackageEvent event);

    private:
        int m_SchemaVersion{1};
        std::unordered_map<std::string, InstalledRecord> m_Installed;
        std::unordered_map<std::string, PackageRecord> m_Packages;
        std::vector<PackageEvent> m_Events;
        std::string m_LastLoadWarning;
    };
} // namespace BML::Core

#endif // BML_CORE_PACKAGE_INSTALL_STATE_H

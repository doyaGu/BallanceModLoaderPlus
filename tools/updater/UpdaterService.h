#ifndef BML_UPDATER_SERVICE_H
#define BML_UPDATER_SERVICE_H

#include <optional>
#include <string>

#include "UpdaterTypes.h"

namespace bmlupdater {
    class UpdaterService {
    public:
        explicit UpdaterService(UpdaterContext context);

        [[nodiscard]] const UpdaterContext &Context() const noexcept;
        [[nodiscard]] StatusInfo GetStatus() const;
        [[nodiscard]] Result RunDoctor(std::vector<std::string> &diagnostics) const;
        [[nodiscard]] Result CheckForUpdates(const std::string &channel, std::vector<std::string> &diagnostics) const;
        [[nodiscard]] Result VerifyLocalPackage(const std::wstring &packagePath,
                                                LocalPackageVerification &verification,
                                                ProgressCallback progress = {}) const;
        [[nodiscard]] Result BuildApplyPlan(const LocalPackageVerification &verification, ApplyPlan &plan) const;
        [[nodiscard]] Result CheckApplyAccess(const ApplyPlan &plan) const;
        [[nodiscard]] Result Apply(const LocalPackageVerification &verification,
                                   const ApplyPlan &plan,
                                   ProgressCallback progress = {}) const;
        [[nodiscard]] Result CheckRollbackAccess() const;
        [[nodiscard]] Result Rollback(ProgressCallback progress = {}) const;

    private:
        [[nodiscard]] std::wstring StateFile() const;
        [[nodiscard]] std::wstring InstalledManifestFile() const;
        [[nodiscard]] std::wstring PendingFile() const;
        [[nodiscard]] std::optional<UpdaterManifest> LoadInstalledManifest() const;

        UpdaterContext m_Context;
    };

    [[nodiscard]] UpdaterContext CreateDefaultContext();
} // namespace bmlupdater

#endif // BML_UPDATER_SERVICE_H

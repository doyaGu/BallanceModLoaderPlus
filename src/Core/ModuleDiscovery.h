#ifndef BML_CORE_MODULE_DISCOVERY_H
#define BML_CORE_MODULE_DISCOVERY_H

#include <memory>
#include <string>
#include <vector>

#include "DependencyResolver.h"
#include "ModManifest.h"

namespace BML::Core {
    struct ManifestLoadResult {
        std::vector<std::unique_ptr<ModManifest>> manifests;
        std::vector<ManifestParseError> errors;
    };

    bool LoadManifestsFromDirectory(const std::wstring &mods_dir, ManifestLoadResult &out_result);

    bool BuildLoadOrder(const ManifestLoadResult &manifests,
                        std::vector<ResolvedNode> &out_order,
                        std::vector<DependencyWarning> &out_warnings,
                        DependencyResolutionError &out_error);
} // namespace BML::Core

#endif // BML_CORE_MODULE_DISCOVERY_H

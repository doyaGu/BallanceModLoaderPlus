#ifndef BML_CORE_MODULE_LOADER_H
#define BML_CORE_MODULE_LOADER_H

#include <memory>
#include <string>
#include <vector>

#include "bml_export.h"

#include "PlatformCompat.h"
#include "DependencyResolver.h"
#include "ModHandle.h"
#include "ModManifest.h"

namespace BML::Core {
    class Context;

    struct LoadedModule {
        std::string id;
        const ModManifest *manifest{nullptr};
        HMODULE handle{nullptr};
        PFN_BML_ModEntrypoint entrypoint{nullptr};
        std::wstring path;
        std::unique_ptr<BML_Mod_T> mod_handle;
    };

    struct ModuleLoadError {
        std::string id;
        std::wstring path;
        std::string message;
        long system_code{0};
    };

    bool LoadModules(const std::vector<ResolvedNode> &order,
                     Context &context,
                     PFN_BML_GetProcAddress get_proc,
                     PFN_BML_GetProcAddressById get_proc_by_id,
                     PFN_BML_GetApiId get_api_id,
                     std::vector<LoadedModule> &out_modules,
                     ModuleLoadError &out_error);

    void UnloadModules(std::vector<LoadedModule> &modules, BML_Context ctx);
} // namespace BML::Core

#endif // BML_CORE_MODULE_LOADER_H

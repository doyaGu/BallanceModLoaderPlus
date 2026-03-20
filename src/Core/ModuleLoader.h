#ifndef BML_CORE_MODULE_LOADER_H
#define BML_CORE_MODULE_LOADER_H

#include <memory>
#include <string>
#include <vector>

#include "bml_export.h"

#include "bml_module_runtime.h"

#include "PlatformCompat.h"
#include "DependencyResolver.h"
#include "ModHandle.h"
#include "ModManifest.h"

namespace BML::Core {
    class Context;

    struct LoadedModule {
        std::string id;
        const ModManifest *manifest{nullptr};
        HMODULE handle{nullptr};                    // null for non-native modules
        PFN_BML_ModEntrypoint entrypoint{nullptr};  // null for non-native modules
        std::wstring path;
        std::unique_ptr<BML_Mod_T> mod_handle;

        /// Non-null when this module was loaded by an external runtime
        /// provider. The pointer borrows the provider's vtable (owned by
        /// the provider module). Dependency ordering guarantees that
        /// provider modules unload AFTER their consumers (reverse order).
        /// If the provider is abnormally disabled, UnloadModules validates
        /// via FindRuntimeProvider before calling through the vtable.
        const BML_ModuleRuntimeProvider *runtime{nullptr};
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
                     std::vector<LoadedModule> &out_modules,
                     ModuleLoadError &out_error);

    void UnloadModules(std::vector<LoadedModule> &modules, BML_Context ctx);
} // namespace BML::Core

#endif // BML_CORE_MODULE_LOADER_H

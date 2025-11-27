#ifndef BML_CORE_MODULE_LOADER_H
#define BML_CORE_MODULE_LOADER_H

#include <memory>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "bml_export.h"

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
                     std::vector<LoadedModule> &out_modules,
                     ModuleLoadError &out_error);

    void UnloadModules(std::vector<LoadedModule> &modules, BML_Context ctx);
} // namespace BML::Core

#endif // BML_CORE_MODULE_LOADER_H

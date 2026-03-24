// Stub implementation of ModuleLoader for tests that include Context.cpp
// but don't need actual module loading functionality

#include "Core/ModuleLoader.h"

#include <string>

namespace BML::Core {

bool LoadModules(const std::vector<ResolvedNode> & /*order*/,
                 Context & /*context*/,
                 KernelServices & /*kernel*/,
                 const BML_Services * /*services*/,
                 std::vector<LoadedModule> &out_modules,
                 ModuleLoadError &out_error) {
    // Stub: no modules loaded in tests
    out_modules.clear();
    out_error = {};
    return true;
}

void UnloadModules(std::vector<LoadedModule> &modules,
                   Context & /*context*/,
                   KernelServices & /*kernel*/) {
    for (auto &mod : modules) {
        if (mod.handle) {
            // Don't actually call entrypoint or FreeLibrary in stub
            mod.handle = nullptr;
            mod.entrypoint = nullptr;
        }
    }
    modules.clear();
}

} // namespace BML::Core

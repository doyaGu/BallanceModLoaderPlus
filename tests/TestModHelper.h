#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "bml_types.h"
#include "Core/KernelServices.h"
#include "Core/ApiRegistry.h"

namespace BML::Core {
    class Context;
    struct ModManifest;
}

namespace BML::Core::Testing {

/// Helper for creating test mod handles with synthetic manifests.
///
/// Usage:
///   TestModHelper helper(kernel);
///   BML_Mod mod = helper.CreateMod("com.test.mymod");
///   // ... use mod in tests ...
///
/// Automatically creates temp directories and manages manifest lifetime.
class TestModHelper {
public:
    explicit TestModHelper(BML::Core::KernelServices &kernel);
    TestModHelper(BML::Core::KernelServices &kernel, const std::filesystem::path &temp_root);

    /// Create a fully registered mod (CreateModHandle + AddLoadedModule).
    BML_Mod CreateMod(const std::string &id);

    /// Create and set as lifecycle module (SetLifecycleModule).
    BML_Mod CreateAndActivate(const std::string &id);

    /// Get the synthetic host module from Context.
    BML_Mod HostMod();

    /// API lookup helper (returns nullptr if not found).
    template <typename Fn>
    Fn Lookup(const char *name) {
        return reinterpret_cast<Fn>(m_Kernel.api_registry->Get(name));
    }

    /// Cleanup: reset lifecycle module.
    void Deactivate();

    /// Get temp root for this helper.
    const std::filesystem::path &TempRoot() const { return m_TempRoot; }

private:
    BML::Core::KernelServices &m_Kernel;
    std::filesystem::path m_TempRoot;
    std::vector<std::unique_ptr<BML::Core::ModManifest>> m_Manifests;
    static std::atomic<uint64_t> s_Counter;
};

} // namespace BML::Core::Testing

#include "TestModHelper.h"

#include "Core/Context.h"
#include "Core/KernelServices.h"
#include "Core/ModHandle.h"
#include "Core/ModManifest.h"
#include "Core/ModuleLoader.h"

namespace BML::Core::Testing {

std::atomic<uint64_t> TestModHelper::s_Counter{0};

TestModHelper::TestModHelper(KernelServices &kernel)
    : m_Kernel(kernel) {
    auto id = s_Counter.fetch_add(1, std::memory_order_relaxed);
    m_TempRoot = std::filesystem::temp_directory_path() /
                 ("bml-test-" + std::to_string(id));
    std::filesystem::create_directories(m_TempRoot);
}

TestModHelper::TestModHelper(KernelServices &kernel, const std::filesystem::path &temp_root)
    : m_Kernel(kernel), m_TempRoot(temp_root) {
    if (!m_TempRoot.empty()) {
        std::filesystem::create_directories(m_TempRoot);
    }
}

BML_Mod TestModHelper::CreateMod(const std::string &id) {
    auto manifest = std::make_unique<ModManifest>();
    manifest->package.id = id;
    manifest->package.name = id;
    manifest->package.version = "1.0.0";
    manifest->package.parsed_version = {1, 0, 0};

    auto dir = m_TempRoot / id;
    std::filesystem::create_directories(dir);
    manifest->directory = dir.wstring();
    manifest->manifest_path = (dir / "mod.toml").wstring();

    auto handle = m_Kernel.context->CreateModHandle(*manifest);
    LoadedModule lm{};
    lm.id = id;
    lm.manifest = manifest.get();
    lm.mod_handle = std::move(handle);
    m_Kernel.context->AddLoadedModule(std::move(lm));

    BML_Mod mod = m_Kernel.context->GetModHandleById(id);
    m_Manifests.push_back(std::move(manifest));
    return mod;
}

BML_Mod TestModHelper::CreateAndActivate(const std::string &id) {
    BML_Mod mod = CreateMod(id);
    Context::SetLifecycleModule(mod);
    return mod;
}

BML_Mod TestModHelper::HostMod() {
    return m_Kernel.context ? m_Kernel.context->GetSyntheticHostModule() : nullptr;
}

void TestModHelper::Deactivate() {
    Context::SetLifecycleModule(nullptr);
}

} // namespace BML::Core::Testing

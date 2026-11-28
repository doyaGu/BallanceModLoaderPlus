#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#if defined(_MSC_VER)
#include <intrin.h>
#define BML_CALLER_ADDRESS() _ReturnAddress()
#else
#define BML_CALLER_ADDRESS() __builtin_extract_return_addr(__builtin_return_address(0))
#endif

namespace BML::Platform {
// Identity is the loaded image base, not the OS loader's reference-counted handle.
using ModuleIdentity = const void *;
ModuleIdentity ModuleFromAddress(const void *address) noexcept;
std::string ModulePath(const void *address);
bool MatchesPeImage(const std::filesystem::path &path, std::string_view architecture,
                    bool executable);

class Module {
public:
    static std::shared_ptr<Module> Open(const std::string &path, std::string &error);
    ~Module();
    Module(const Module &) = delete;
    Module &operator=(const Module &) = delete;
    void *Symbol(const char *name) const noexcept;
    const std::string &Path() const noexcept { return m_Path; }
    ModuleIdentity Identity() const noexcept { return m_Identity; }
private:
    Module(void *handle, std::string path, ModuleIdentity identity);
    void *m_Handle;
    std::string m_Path;
    ModuleIdentity m_Identity;
};
// Preserve the loader registry's image-base keys while retaining the separate
// OS load reference. The reference must outlive every returned symbol.
std::shared_ptr<void> RetainModule(const std::shared_ptr<Module> &module);
void *ModuleSymbol(const std::shared_ptr<void> &reference, const char *name) noexcept;
}

#include "Module.h"
#include "PathUtils.h"
#include "StringUtils.h"

#include <array>
#include <cstdint>
#include <fstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#include <system_error>
#include <utility>

namespace BML::Platform {
bool MatchesPeImage(const std::filesystem::path &path, std::string_view architecture,
                    bool executable) {
    const auto word = [](const unsigned char *bytes) {
        return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8);
    };
    std::ifstream file(path, std::ios::binary);
    std::array<unsigned char, 64> dos{};
    if (!file.read(reinterpret_cast<char *>(dos.data()), dos.size()) ||
        dos[0] != 'M' || dos[1] != 'Z')
        return false;
    const auto offset = word(dos.data() + 0x3c) | (word(dos.data() + 0x3e) << 16);
    std::array<unsigned char, 26> pe{};
    if (offset < dos.size() || !file.seekg(offset, std::ios::beg) ||
        !file.read(reinterpret_cast<char *>(pe.data()), pe.size()) ||
        pe[0] != 'P' || pe[1] != 'E' || pe[2] != 0 || pe[3] != 0)
        return false;
    const auto expectedMachine = architecture == "x86" ? 0x14cu :
        architecture == "x64" ? 0x8664u : architecture == "arm64" ? 0xaa64u : 0u;
    const auto expectedMagic = architecture == "x86" ? 0x10bu : 0x20bu;
    const bool dll = (word(pe.data() + 22) & 0x2000u) != 0;
    return expectedMachine != 0 && word(pe.data() + 4) == expectedMachine &&
        word(pe.data() + 24) == expectedMagic && dll != executable;
}

namespace {
struct ModuleReference {
    std::shared_ptr<Module> Owner;
    void operator()(void *) const noexcept {}
};
}
std::shared_ptr<void> RetainModule(const std::shared_ptr<Module> &module) {
    if (!module || !module->Identity()) return {};
    return {const_cast<void *>(module->Identity()), ModuleReference{module}};
}
void *ModuleSymbol(const std::shared_ptr<void> &reference, const char *name) noexcept {
    const auto *owner = std::get_deleter<ModuleReference>(reference);
    return owner && owner->Owner ? owner->Owner->Symbol(name) : nullptr;
}
ModuleIdentity ModuleFromAddress(const void *address) noexcept {
    if (!address) return nullptr;
#ifdef _WIN32
    HMODULE module = nullptr;
    return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<LPCWSTR>(address), &module) ? module : nullptr;
#else
    Dl_info info{};
    return dladdr(address, &info) ? info.dli_fbase : nullptr;
#endif
}

std::string ModulePath(const void *address) {
#ifdef _WIN32
    auto module = static_cast<HMODULE>(const_cast<void *>(ModuleFromAddress(address)));
    if (!module) return {};
    std::wstring path(512, L'\0');
    for (;;) {
        const auto length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
        if (!length) return {};
        if (length < path.size()) { path.resize(length); return utils::Utf16ToUtf8(path); }
        if (path.size() >= 32768) return {};
        path.resize(path.size() * 2);
    }
#else
    Dl_info info{};
    return address && dladdr(address, &info) && info.dli_fname
        ? std::string(info.dli_fname) : std::string{};
#endif
}

Module::Module(void *handle, std::string path, ModuleIdentity identity)
    : m_Handle(handle), m_Path(std::move(path)), m_Identity(identity) {}

std::shared_ptr<Module> Module::Open(const std::string &path, std::string &error) {
    error.clear();
    std::wstring resolved;
    if (!utils::TryGetFinalPathW(utils::Utf8ToUtf16(path), resolved)) {
        error = "Cannot resolve module path: " + path; return {};
    }
    const auto absolute = utils::Utf16ToUtf8(resolved);
#ifdef _WIN32
    void *handle = LoadLibraryExW(resolved.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!handle) {
        error = std::system_category().message(static_cast<int>(GetLastError()));
        return {};
    }
    const auto identity = handle;
#else
    void *handle = dlopen(absolute.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) { const char *detail = dlerror(); error = detail ? detail : "dlopen failed"; return {}; }
    // Identity is the image base, never the native loader's opaque handle.
    // The new Mod protocol has one entry; engine plugins keep their own one.
    const void *entry = dlsym(handle, "BML_GetModInterface");
    if (!entry) entry = dlsym(handle, "CKGetPluginInfo");
    const auto identity = ModuleFromAddress(entry);
#endif
    Module *module;
    try { module = new Module(handle, absolute, identity); }
    catch (...) {
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(handle));
#else
        dlclose(handle);
#endif
        throw;
    }
    return std::shared_ptr<Module>(module);
}

Module::~Module() {
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(m_Handle));
#else
    dlclose(m_Handle);
#endif
}

void *Module::Symbol(const char *name) const noexcept {
    if (!name) return nullptr;
#ifdef _WIN32
    return reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(m_Handle), name));
#else
    return dlsym(m_Handle, name);
#endif
}
}

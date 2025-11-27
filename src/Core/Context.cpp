#include "Context.h"

#include <cstdio>
#include <filesystem>
#include <limits>
#include <system_error>

#include "ConfigStore.h"
#include "ApiRegistry.h"
#include "ResourceApi.h"

namespace BML::Core {
    namespace {
        thread_local BML_Mod g_CurrentModule = nullptr;

        uint16_t ClampVersionComponent(int value) {
            if (value < 0)
                return 0;
            if (value > 0xFFFF)
                return 0xFFFF;
            return static_cast<uint16_t>(value);
        }

        constexpr wchar_t kInvalidFilenameChars[] = L"<>:\"/\\|?*";

        void FilterInvalidFilenameChars(std::wstring &value) {
            auto replace_if_invalid = [](wchar_t ch) -> wchar_t {
                if (ch < 32)
                    return L'_';
                for (wchar_t forbidden : kInvalidFilenameChars) {
                    if (forbidden == 0)
                        break;
                    if (ch == forbidden)
                        return L'_';
                }
                return ch;
            };

            for (auto &ch : value) {
                ch = replace_if_invalid(ch);
            }

            while (!value.empty() && (value.back() == L' ' || value.back() == L'.')) {
                value.back() = L'_';
            }
        }

        std::wstring FallbackSanitizedIdentifier(const std::string &value) {
            if (value.empty())
                return L"mod";

            std::wstring ascii;
            ascii.reserve(value.size());
            for (unsigned char ch : value) {
                wchar_t wide = (ch < 0x80) ? static_cast<wchar_t>(ch) : L'_';
                if (wide < 32)
                    wide = L'_';
                ascii.push_back(wide);
            }
            FilterInvalidFilenameChars(ascii);
            if (ascii.empty())
                ascii = L"mod";
            return ascii;
        }
    } // namespace

    std::wstring Context::SanitizeIdentifierForFilename(const std::string &value) {
        if (value.empty())
            return L"mod";

        auto try_convert = [&](DWORD flags, std::wstring &out) -> bool {
            if (value.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
                return false;
            int required = MultiByteToWideChar(CP_UTF8,
                                               flags,
                                               value.data(),
                                               static_cast<int>(value.size()),
                                               nullptr,
                                               0);
            if (required <= 0)
                return false;
            out.resize(required);
            int written = MultiByteToWideChar(CP_UTF8,
                                              flags,
                                              value.data(),
                                              static_cast<int>(value.size()),
                                              out.data(),
                                              required);
            return written > 0;
        };

        std::wstring converted;
        if (!try_convert(MB_ERR_INVALID_CHARS, converted)) {
            converted.clear();
            if (!try_convert(0, converted)) {
                return FallbackSanitizedIdentifier(value);
            }
        }

        FilterInvalidFilenameChars(converted);
        if (converted.empty())
            converted = L"mod";
        return converted;
    }

    namespace {
        std::wstring BuildLogPath(const ModManifest &manifest) {
            if (manifest.directory.empty())
                return {};

            std::filesystem::path base(manifest.directory);
            std::filesystem::path logsDir = base / L"logs";
            std::error_code ec;
            std::filesystem::create_directories(logsDir, ec);

            const std::string &identifier = !manifest.package.id.empty() ? manifest.package.id : manifest.package.name;
            std::wstring safeName = Context::SanitizeIdentifierForFilename(identifier);
            std::filesystem::path logPath = logsDir / (safeName + L".log");
            return logPath.wstring();
        }

        FILE *OpenLogFile(const std::wstring &path) {
            if (path.empty())
                return nullptr;

            FILE *file = nullptr;
#if defined(_MSC_VER)
            _wfopen_s(&file, path.c_str(), L"a, ccs=UTF-8");
#else
            file = _wfopen(path.c_str(), L"a, ccs=UTF-8");
#endif
            if (!file) {
#if defined(_MSC_VER)
                _wfopen_s(&file, path.c_str(), L"a");
#else
                file = _wfopen(path.c_str(), L"a");
#endif
            }
            return file;
        }
    }

    Context &Context::Instance() {
        static Context instance;
        return instance;
    }

    Context::Context() {
        // Constructor only sets defaults; actual initialization happens in Initialize()
        m_RuntimeVersion = {0, 4, 0};
        m_Initialized.store(false, std::memory_order_relaxed);
    }

    void Context::Initialize(const BML_Version &runtime_version) {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        if (m_Initialized.load(std::memory_order_acquire)) {
            OutputDebugStringA("[BML Context] Warning: Initialize() called on already-initialized context\n");
            return;
        }

        m_RuntimeVersion = runtime_version;
        m_CleanupRequested = false;
        m_Initialized.store(true, std::memory_order_release);
        OutputDebugStringA("[BML Context] Initialized\n");
    }

    void Context::Cleanup() {
        std::unique_lock<std::mutex> state_lock(m_StateMutex);
        if (!m_Initialized.load(std::memory_order_acquire))
            return;
        if (m_CleanupRequested)
            return;

        m_CleanupRequested = true;

        OutputDebugStringA("[BML Context] Starting cleanup...\n");

        {
            std::unique_lock<std::mutex> retain_lock(m_RetainMutex);
            m_RetainCv.wait(retain_lock, [this] {
                return m_RetainCount.load(std::memory_order_acquire) == 0;
            });
        }

        ShutdownModulesLocked();

        // Clear all manifests
        m_Manifests.clear();

        // Note: Extension registrations are cleaned up per-provider during module shutdown
        // ApiRegistry::Clear() is not called here as it would remove core APIs

        // Reset to default version
        m_RuntimeVersion = bmlGetApiVersion();
        m_Initialized.store(false, std::memory_order_release);
        m_CleanupRequested = false;

        OutputDebugStringA("[BML Context] Cleanup complete\n");
    }

    BML_Context Context::GetHandle() {
        return reinterpret_cast<BML_Context>(this);
    }

    void Context::RegisterManifest(std::unique_ptr<ModManifest> manifest) {
        if (!manifest)
            return;
        std::lock_guard<std::mutex> lock(m_StateMutex);
        if (!m_Initialized.load(std::memory_order_acquire) || m_CleanupRequested)
            return;
        m_Manifests.emplace_back(std::move(manifest));
    }

    const std::vector<std::unique_ptr<ModManifest>> &Context::GetManifests() const {
        return m_Manifests;
    }

    void Context::ClearManifests() {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        m_Manifests.clear();
    }

    void Context::AddLoadedModule(LoadedModule module) {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        if (!m_Initialized.load(std::memory_order_acquire) || m_CleanupRequested)
            return;
        if (module.mod_handle) {
            BML_Mod raw = module.mod_handle.get();
            m_ModHandlesById[module.id] = raw;
            if (module.handle) {
                m_ModHandlesByModule[module.handle] = raw;
            }
        }
        m_LoadedModules.emplace_back(std::move(module));
    }

    const std::vector<LoadedModule> &Context::GetLoadedModules() const {
        return m_LoadedModules;
    }

    void Context::ShutdownModules() {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        ShutdownModulesLocked();
    }

    std::unique_ptr<BML_Mod_T> Context::CreateModHandle(const ModManifest &manifest) const {
        auto handle = std::make_unique<BML_Mod_T>();
        handle->id = manifest.package.id;
        handle->manifest = &manifest;
        handle->version.major = ClampVersionComponent(manifest.package.parsed_version.major);
        handle->version.minor = ClampVersionComponent(manifest.package.parsed_version.minor);
        handle->version.patch = ClampVersionComponent(manifest.package.parsed_version.patch);
        handle->capabilities = manifest.capabilities;
        handle->log_path = BuildLogPath(manifest);
        handle->log_file.reset(OpenLogFile(handle->log_path));
        return handle;
    }

    BML_Mod_T *Context::ResolveModHandle(BML_Mod mod) {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        return FindModHandleLocked(mod);
    }

    const BML_Mod_T *Context::ResolveModHandle(BML_Mod mod) const {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        return FindModHandleLocked(mod);
    }

    BML_Mod Context::GetModHandleById(const std::string &id) const {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        auto it = m_ModHandlesById.find(id);
        return it != m_ModHandlesById.end() ? it->second : nullptr;
    }

    BML_Mod Context::GetModHandleByModule(HMODULE module) const {
        if (!module)
            return nullptr;
        std::lock_guard<std::mutex> lock(m_StateMutex);
        auto it = m_ModHandlesByModule.find(module);
        return it != m_ModHandlesByModule.end() ? it->second : nullptr;
    }

    void Context::AppendShutdownHook(BML_Mod mod, BML_ShutdownCallback callback, void *user_data) {
        if (!callback)
            return;
        std::lock_guard<std::mutex> lock(m_StateMutex);
        if (m_CleanupRequested)
            return;
        if (auto *handle = FindModHandleLocked(mod)) {
            handle->shutdown_hooks.push_back({callback, user_data});
        }
    }

    void Context::SetCurrentModule(BML_Mod mod) {
        g_CurrentModule = mod;
    }

    BML_Mod Context::GetCurrentModule() {
        return g_CurrentModule;
    }

    void Context::SetRuntimeVersion(const BML_Version &version) {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        m_RuntimeVersion = version;
    }

    const BML_Version &Context::GetRuntimeVersion() const {
        return m_RuntimeVersion;
    }

    void Context::ShutdownModulesLocked() {
        if (m_LoadedModules.empty())
            return;

        BML_Context ctx = GetHandle();

        for (auto module_it = m_LoadedModules.rbegin(); module_it != m_LoadedModules.rend(); ++module_it) {
            if (!module_it->mod_handle)
                continue;

            struct ModuleScope {
                explicit ModuleScope(BML_Mod_T *mod) : previous(Context::GetCurrentModule()) {
                    Context::SetCurrentModule(mod);
                }

                ~ModuleScope() {
                    Context::SetCurrentModule(previous);
                }

                BML_Mod previous;
            } scope(module_it->mod_handle.get());

            for (auto hook_it = module_it->mod_handle->shutdown_hooks.rbegin();
                 hook_it != module_it->mod_handle->shutdown_hooks.rend();
                 ++hook_it) {
                const auto &hook = *hook_it;
                if (!hook.callback)
                    continue;
                try {
                    hook.callback(ctx, hook.user_data);
                } catch (...) {
                    char buf[256];
                    snprintf(buf,
                             sizeof(buf),
                             "[BML Context] Exception in shutdown hook for module: %s\n",
                             module_it->mod_handle->id.c_str());
                    OutputDebugStringA(buf);
                }
            }
        }

        auto &apiRegistry = ApiRegistry::Instance();
        for (const auto &module : m_LoadedModules) {
            if (!module.mod_handle)
                continue;
            ConfigStore::Instance().FlushAndRelease(module.mod_handle.get());
            apiRegistry.UnregisterByProvider(module.mod_handle->id);
            UnregisterResourceTypesForProvider(module.mod_handle->id);
        }

        UnloadModules(m_LoadedModules, ctx);
        m_ModHandlesById.clear();
        m_ModHandlesByModule.clear();
    }

    BML_Mod_T *Context::FindModHandleLocked(BML_Mod mod) {
        if (!mod)
            return nullptr;
        for (auto &module : m_LoadedModules) {
            if (module.mod_handle && module.mod_handle.get() == mod)
                return module.mod_handle.get();
        }
        return mod;
    }

    const BML_Mod_T *Context::FindModHandleLocked(BML_Mod mod) const {
        if (!mod)
            return nullptr;
        for (const auto &module : m_LoadedModules) {
            if (module.mod_handle && module.mod_handle.get() == mod)
                return module.mod_handle.get();
        }
        return mod;
    }

    BML_Result Context::RetainHandle() {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        if (!m_Initialized.load(std::memory_order_acquire))
            return BML_RESULT_NOT_INITIALIZED;
        if (m_CleanupRequested)
            return BML_RESULT_INVALID_STATE;
        m_RetainCount.fetch_add(1, std::memory_order_acq_rel);
        return BML_RESULT_OK;
    }

    BML_Result Context::ReleaseHandle() {
        uint32_t current = m_RetainCount.load(std::memory_order_acquire);
        while (true) {
            if (current == 0)
                return BML_RESULT_INVALID_STATE;
            if (m_RetainCount.compare_exchange_weak(current,
                                                    current - 1,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                break;
            }
        }
        if (current == 1) {
            std::lock_guard<std::mutex> lock(m_RetainMutex);
            m_RetainCv.notify_all();
        }
        return BML_RESULT_OK;
    }

    uint32_t Context::GetRetainCountForTest() const {
        return m_RetainCount.load(std::memory_order_acquire);
    }
} // namespace BML::Core

#include "Context.h"

#include <cstdio>
#include <chrono>
#include <filesystem>
#include <limits>
#include <sstream>
#include <system_error>
#include <thread>

#include "ConfigStore.h"
#include "ApiRegistry.h"
#include "ExtensionStateHooks.h"
#include "ResourceApi.h"
#include "Logging.h"
#include "StringUtils.h"

namespace BML::Core {
    namespace {
        thread_local BML_Mod g_CurrentModule = nullptr;
        std::atomic<ExtensionProviderCleanupHook> g_ExtensionCleanupHook{nullptr};
        constexpr const char kContextLogCategory[] = "context";

        uint16_t ClampVersionComponent(int value) {
            if (value < 0)
                return 0;
            if (value > 0xFFFF)
                return 0xFFFF;
            return static_cast<uint16_t>(value);
        }

        constexpr wchar_t kInvalidFilenameChars[] = L"<>:\"/\\|?*";
        constexpr auto kCleanupRetainWarnAfter = std::chrono::seconds(2);
        constexpr auto kCleanupRetainWarnInterval = std::chrono::seconds(2);
        constexpr auto kCleanupRetainPollInterval = std::chrono::milliseconds(250);

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

        uint64_t GetThreadToken() {
            return static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        }
    } // namespace

    std::wstring Context::SanitizeIdentifierForFilename(const std::string &value) {
        if (value.empty())
            return L"mod";

        std::wstring converted = utils::Utf8ToUtf16(value);
        if (converted.empty()) {
            return FallbackSanitizedIdentifier(value);
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
            if (_wfopen_s(&file, path.c_str(), L"a") != 0) {
                file = nullptr;
            }
#elif defined(_WIN32)
            file = _wfopen(path.c_str(), L"a");
#else
            std::string utf8_path = utils::Utf16ToUtf8(path);
            if (!utf8_path.empty()) {
                file = std::fopen(utf8_path.c_str(), "a");
            }
#endif
            return file;
        }
    }

    Context::Context(ApiRegistry &api_registry, ConfigStore &config)
        : m_ApiRegistry(api_registry), m_Config(config) {
        // Constructor only sets defaults; actual initialization happens in Initialize()
        m_RuntimeVersion = {0, 4, 0};
        m_Initialized.store(false, std::memory_order_relaxed);
        m_ShutdownState.store(ShutdownState::Stopped, std::memory_order_relaxed);
    }

    void SetExtensionProviderCleanupHook(ExtensionProviderCleanupHook hook) {
        g_ExtensionCleanupHook.store(hook, std::memory_order_release);
    }

    void RunExtensionProviderCleanupHook(const std::string &provider_id) {
        auto hook = g_ExtensionCleanupHook.load(std::memory_order_acquire);
        if (!provider_id.empty() && hook) {
            hook(provider_id);
        }
    }

    void Context::Initialize(const BML_Version &runtime_version) {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        if (m_Initialized.load(std::memory_order_acquire)) {
            CoreLog(BML_LOG_WARN, kContextLogCategory, "Initialize() called on already-initialized context");
            return;
        }

        m_RuntimeVersion = runtime_version;
        m_CleanupRequested = false;
        m_ShutdownState.store(ShutdownState::Running, std::memory_order_release);
        m_CreatedModHandles.clear();
        m_HostModHandle = std::make_unique<BML_Mod_T>();
        if (m_HostModHandle) {
            m_HostModHandle->id = "ModLoader";
            m_HostModHandle->version = runtime_version;
            m_HostModHandle->capabilities.push_back("bml.internal.runtime");
            m_CreatedModHandles.insert(m_HostModHandle.get());
        }
        {
            std::lock_guard<std::mutex> retain_trace_lock(m_RetainTraceMutex);
            m_RetainTrace.clear();
            m_RetainTraceSequence = 0;
        }
        m_Initialized.store(true, std::memory_order_release);
        CoreLog(BML_LOG_INFO, kContextLogCategory, "Context initialized");
    }

    void Context::Cleanup() {
        std::unique_lock<std::mutex> state_lock(m_StateMutex);
        if (!m_Initialized.load(std::memory_order_acquire))
            return;
        if (m_CleanupRequested || m_ShutdownState.load(std::memory_order_acquire) != ShutdownState::Running)
            return;

        m_CleanupRequested = true;
        m_ShutdownState.store(ShutdownState::ShutdownRequested, std::memory_order_release);

        CoreLog(BML_LOG_INFO, kContextLogCategory, "Starting cleanup");

        {
            std::unique_lock<std::mutex> retain_lock(m_RetainMutex);
            m_ShutdownState.store(ShutdownState::DrainingRetains, std::memory_order_release);
            const auto wait_start = std::chrono::steady_clock::now();
            auto next_warning_at = wait_start + kCleanupRetainWarnAfter;
            while (m_RetainCount.load(std::memory_order_acquire) != 0) {
                m_RetainCv.wait_for(retain_lock, kCleanupRetainPollInterval, [this] {
                    return m_RetainCount.load(std::memory_order_acquire) == 0;
                });

                const auto now = std::chrono::steady_clock::now();
                if (m_RetainCount.load(std::memory_order_acquire) != 0 && now >= next_warning_at) {
                    LogRetainWaitStatus(
                        std::chrono::duration_cast<std::chrono::milliseconds>(now - wait_start));
                    next_warning_at = now + kCleanupRetainWarnInterval;
                }
            }
        }

        m_ShutdownState.store(ShutdownState::ShuttingDownModules, std::memory_order_release);
        ShutdownModulesLocked();
        m_CreatedModHandles.clear();

        // Clear all manifests
        m_Manifests.clear();

        // Clean up user data (call destructors)
        {
            std::lock_guard<std::mutex> user_data_lock(m_UserDataMutex);
            for (auto &entry : m_UserData) {
                if (entry.second.destructor && entry.second.data) {
                    entry.second.destructor(entry.second.data);
                }
            }
            m_UserData.clear();
        }

        // Clear runtime providers (modules that registered them are already unloaded)
        {
            std::lock_guard<std::mutex> provider_lock(m_RuntimeProviderMutex);
            m_RuntimeProviders.clear();
        }

        // Note: Extension registrations are cleaned up per-provider during module shutdown
        // ApiRegistry::Clear() is not called here as it would remove core APIs

        // Reset to default version
        m_RuntimeVersion = bmlGetApiVersion();
        m_Initialized.store(false, std::memory_order_release);
        m_CleanupRequested = false;
        m_ShutdownState.store(ShutdownState::Stopped, std::memory_order_release);

        CoreLog(BML_LOG_INFO, kContextLogCategory, "Cleanup complete");
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

    std::vector<ModManifest> Context::GetManifestSnapshot() const {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        std::vector<ModManifest> snapshot;
        snapshot.reserve(m_Manifests.size());
        for (const auto &manifest : m_Manifests) {
            if (manifest) {
                snapshot.push_back(*manifest);
            }
        }
        return snapshot;
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
            m_CreatedModHandles.insert(raw);
            m_ModHandlesById[module.id] = raw;
            if (module.handle) {
                m_ModHandlesByModule[module.handle] = raw;
            }
        }
        m_LoadedModules.emplace_back(std::move(module));
    }

    std::vector<LoadedModuleSnapshot> Context::GetLoadedModuleSnapshot() const {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        std::vector<LoadedModuleSnapshot> snapshot;
        snapshot.reserve(m_LoadedModules.size());
        for (const auto &module : m_LoadedModules) {
            LoadedModuleSnapshot entry;
            entry.id = module.id;
            if (module.manifest) {
                entry.manifest = *module.manifest;
            }
            entry.handle = module.handle;
            entry.entrypoint = module.entrypoint;
            entry.path = module.path;
            entry.mod_handle = module.mod_handle.get();
            snapshot.push_back(std::move(entry));
        }
        return snapshot;
    }

    uint32_t Context::GetLoadedModuleCount() const {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        return static_cast<uint32_t>(m_LoadedModules.size());
    }

    BML_Mod Context::GetLoadedModuleAt(uint32_t index) const {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        if (index >= m_LoadedModules.size()) return nullptr;
        return m_LoadedModules[index].mod_handle.get();
    }

    void Context::ShutdownModules() {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        ShutdownModulesLocked();
    }

    std::unique_ptr<BML_Mod_T> Context::CreateModHandle(const ModManifest &manifest) {
        auto handle = std::make_unique<BML_Mod_T>();
        handle->id = manifest.package.id;
        handle->manifest = &manifest;
        handle->version.major = ClampVersionComponent(manifest.package.parsed_version.major);
        handle->version.minor = ClampVersionComponent(manifest.package.parsed_version.minor);
        handle->version.patch = ClampVersionComponent(manifest.package.parsed_version.patch);
        handle->capabilities = manifest.capabilities;
        handle->log_path = BuildLogPath(manifest);
        handle->log_file.reset(OpenLogFile(handle->log_path));
        if (!manifest.directory.empty()) {
            handle->directory_utf8 = utils::Utf16ToUtf8(manifest.directory);
        }
        {
            std::lock_guard<std::mutex> lock(m_StateMutex);
            m_CreatedModHandles.insert(handle.get());
        }
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

    BML_Mod_T *Context::ResolveCurrentConsumer() {
        return const_cast<BML_Mod_T *>(static_cast<const Context *>(this)->ResolveCurrentConsumer());
    }

    const BML_Mod_T *Context::ResolveCurrentConsumer() const {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        if (!g_CurrentModule) {
            return m_HostModHandle.get();
        }
        return FindModHandleLocked(g_CurrentModule);
    }

    BML_Mod Context::GetModHandleById(const std::string &id) const {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        if (m_HostModHandle && m_HostModHandle->id == id) {
            return m_HostModHandle.get();
        }
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

    BML_Mod Context::GetSyntheticHostModule() const {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        return m_HostModHandle ? m_HostModHandle.get() : nullptr;
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

    BML_Version Context::GetRuntimeVersionCopy() const {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        return m_RuntimeVersion;
    }

    Context::ShutdownState Context::GetShutdownState() const {
        return m_ShutdownState.load(std::memory_order_acquire);
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
                    CoreLog(BML_LOG_WARN,
                            kContextLogCategory,
                            "Exception in shutdown hook for module '%s'",
                            module_it->mod_handle->id.c_str());
                }
            }
        }

        for (const auto &module : m_LoadedModules) {
            if (!module.mod_handle)
                continue;
            m_Config.FlushAndRelease(module.mod_handle.get());
            RunExtensionProviderCleanupHook(module.mod_handle->id);
            m_ApiRegistry.UnregisterByProvider(module.mod_handle->id);
            UnregisterResourceTypesForProvider(module.mod_handle->id);
        }

        UnloadModules(m_LoadedModules, ctx);
        m_ModHandlesById.clear();
        m_ModHandlesByModule.clear();
        m_CreatedModHandles.clear();
        m_HostModHandle.reset();
    }

    void Context::RemoveCreatedModHandle(BML_Mod mod) {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        m_CreatedModHandles.erase(mod);
    }

    BML_Mod_T *Context::FindModHandleLocked(BML_Mod mod) {
        return const_cast<BML_Mod_T *>(static_cast<const Context *>(this)->FindModHandleLocked(mod));
    }

    const BML_Mod_T *Context::FindModHandleLocked(BML_Mod mod) const {
        if (!mod)
            return nullptr;
        if (m_HostModHandle && m_HostModHandle.get() == mod)
            return m_HostModHandle.get();
        for (const auto &module : m_LoadedModules) {
            if (module.mod_handle && module.mod_handle.get() == mod)
                return module.mod_handle.get();
        }
        if (m_CreatedModHandles.find(mod) != m_CreatedModHandles.end())
            return mod;
        return nullptr;
    }

    BML_Result Context::RetainHandle() {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        if (m_ShutdownState.load(std::memory_order_acquire) != ShutdownState::Running)
            return BML_RESULT_INVALID_STATE;
        if (!m_Initialized.load(std::memory_order_acquire))
            return BML_RESULT_NOT_INITIALIZED;
        if (m_CleanupRequested)
            return BML_RESULT_INVALID_STATE;
        uint32_t count_after = m_RetainCount.fetch_add(1, std::memory_order_acq_rel) + 1;
        RecordRetainEvent(true, count_after);
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
                RecordRetainEvent(false, current - 1);
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

    void Context::RecordRetainEvent(bool is_retain, uint32_t count_after) {
        std::lock_guard<std::mutex> lock(m_RetainTraceMutex);
        constexpr size_t kMaxRetainTraceEntries = 16;
        RetainEvent event;
        event.sequence = ++m_RetainTraceSequence;
        event.thread_token = GetThreadToken();
        event.count_after = count_after;
        event.is_retain = is_retain;

        if (m_RetainTrace.size() < kMaxRetainTraceEntries) {
            m_RetainTrace.push_back(event);
        } else {
            const size_t index = static_cast<size_t>(m_RetainTraceSequence % kMaxRetainTraceEntries);
            m_RetainTrace[index] = event;
        }
    }

    void Context::LogRetainWaitStatus(std::chrono::milliseconds elapsed) const {
        const uint32_t retain_count = m_RetainCount.load(std::memory_order_acquire);

        std::vector<RetainEvent> retain_trace;
        {
            std::lock_guard<std::mutex> lock(m_RetainTraceMutex);
            retain_trace = m_RetainTrace;
        }

        std::sort(retain_trace.begin(), retain_trace.end(), [](const RetainEvent &lhs, const RetainEvent &rhs) {
            return lhs.sequence < rhs.sequence;
        });

        std::ostringstream oss;
        oss << "Cleanup is waiting for " << retain_count
            << " retained context handle(s) after " << elapsed.count() << " ms";
        if (!retain_trace.empty()) {
            oss << "; recent retain activity:";
            for (const auto &event : retain_trace) {
                oss << " [#" << event.sequence
                    << ' ' << (event.is_retain ? "retain" : "release")
                    << " thread=" << event.thread_token
                    << " count=" << event.count_after << ']';
            }
        }

        CoreLog(BML_LOG_WARN, kContextLogCategory, "%s", oss.str().c_str());
    }

    BML_Result Context::SetUserData(const char *key, void *data, BML_UserDataDestructor destructor) {
        if (!key)
            return BML_RESULT_INVALID_ARGUMENT;

        std::lock_guard<std::mutex> lock(m_UserDataMutex);

        // If key already exists, call destructor on old data first
        auto it = m_UserData.find(key);
        if (it != m_UserData.end()) {
            if (it->second.destructor && it->second.data) {
                it->second.destructor(it->second.data);
            }
            if (data) {
                it->second.data = data;
                it->second.destructor = destructor;
            } else {
                m_UserData.erase(it);
            }
        } else if (data) {
            m_UserData[key] = {data, destructor};
        }

        return BML_RESULT_OK;
    }

    BML_Result Context::GetUserData(const char *key, void **out_data) const {
        if (!key || !out_data)
            return BML_RESULT_INVALID_ARGUMENT;

        std::lock_guard<std::mutex> lock(m_UserDataMutex);

        auto it = m_UserData.find(key);
        if (it != m_UserData.end()) {
            *out_data = it->second.data;
            return BML_RESULT_OK;
        }

        *out_data = nullptr;
        return BML_RESULT_OK;
    }
    // ========================================================================
    // Runtime Provider Registry
    // ========================================================================

    BML_Result Context::RegisterRuntimeProvider(
            const BML_ModuleRuntimeProvider *provider,
            const std::string &owner_id) {
        if (!provider || provider->struct_size < sizeof(BML_ModuleRuntimeProvider))
            return BML_RESULT_INVALID_ARGUMENT;
        if (!provider->CanHandle || !provider->AttachModule ||
            !provider->DetachModule)
            return BML_RESULT_INVALID_ARGUMENT;
        if (owner_id.empty())
            return BML_RESULT_INVALID_ARGUMENT;

        std::lock_guard<std::mutex> lock(m_RuntimeProviderMutex);
        for (const auto &entry : m_RuntimeProviders) {
            if (entry.provider == provider)
                return BML_RESULT_ALREADY_EXISTS;
        }
        m_RuntimeProviders.push_back({provider, owner_id});
        CoreLog(BML_LOG_INFO, kContextLogCategory,
                "Runtime provider registered by '%s'", owner_id.c_str());
        return BML_RESULT_OK;
    }

    BML_Result Context::UnregisterRuntimeProvider(
            const BML_ModuleRuntimeProvider *provider) {
        if (!provider)
            return BML_RESULT_INVALID_ARGUMENT;

        std::lock_guard<std::mutex> lock(m_RuntimeProviderMutex);
        for (auto it = m_RuntimeProviders.begin(); it != m_RuntimeProviders.end(); ++it) {
            if (it->provider == provider) {
                CoreLog(BML_LOG_INFO, kContextLogCategory,
                        "Runtime provider unregistered (owner '%s')",
                        it->owner_id.c_str());
                m_RuntimeProviders.erase(it);
                return BML_RESULT_OK;
            }
        }
        return BML_RESULT_NOT_FOUND;
    }

    void Context::InvalidateRuntimeProvider(const std::string &owner_id) {
        if (owner_id.empty())
            return;

        std::lock_guard<std::mutex> lock(m_RuntimeProviderMutex);
        size_t before = m_RuntimeProviders.size();
        std::erase_if(m_RuntimeProviders,
                      [&](const RuntimeProviderEntry &e) { return e.owner_id == owner_id; });
        if (m_RuntimeProviders.size() < before) {
            CoreLog(BML_LOG_WARN, kContextLogCategory,
                    "Runtime provider invalidated for owner '%s'",
                    owner_id.c_str());
        }
    }

    const BML_ModuleRuntimeProvider *Context::FindRuntimeProvider(
            const std::string &entry_path_utf8) const {
        if (entry_path_utf8.empty())
            return nullptr;

        std::lock_guard<std::mutex> lock(m_RuntimeProviderMutex);
        for (const auto &entry : m_RuntimeProviders) {
            if (entry.provider->CanHandle(entry_path_utf8.c_str()))
                return entry.provider;
        }
        return nullptr;
    }
} // namespace BML::Core

#include "HotReloadCoordinator.h"

#include <algorithm>
#include <filesystem>

#include "Context.h"
#include "KernelServices.h"
#include "Logging.h"
#include "StringUtils.h"

namespace BML::Core {

    namespace {
        constexpr char kLogCategory[] = "hot.reload";

        bool IsModuleBinaryFile(const std::string& filename) {
            auto pos = filename.find_last_of('.');
            if (pos == std::string::npos) {
                return false;
            }

            std::string ext = filename.substr(pos);
            for (auto& c : ext) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }

            if (ext == ".dll") {
                return true;
            }
#if defined(__APPLE__)
            return ext == ".dylib";
#else
            return ext == ".so";
#endif
        }

        const char* ReloadResultToString(ReloadResult result) {
            switch (result) {
                case ReloadResult::Success: return "Success";
                case ReloadResult::NoChange: return "NoChange";
                case ReloadResult::LoadFailed: return "LoadFailed";
                case ReloadResult::EntrypointMissing: return "EntrypointMissing";
                case ReloadResult::InitFailed: return "InitFailed";
                case ReloadResult::Crashed: return "Crashed";
                case ReloadResult::RolledBack: return "RolledBack";
                default: return "Unknown";
            }
        }

        std::filesystem::path NormalizePath(const std::filesystem::path& path) {
            std::error_code ec;
            auto normalized = std::filesystem::weakly_canonical(path, ec);
            if (ec) {
                normalized = path.lexically_normal();
            }
            return normalized;
        }
    }

    HotReloadCoordinator::HotReloadCoordinator(Context& context, KernelServices &kernel)
        : m_Context(context)
        , m_Kernel(kernel)
        , m_Watcher(std::make_unique<FileSystemWatcher>()) {

        // Set up file change callback
        m_Watcher->SetCallback([this](const FileEvent& event) {
            OnFileChanged(event);
        });
    }

    HotReloadCoordinator::~HotReloadCoordinator() {
        Stop();

        // Cleanup all slots
        std::lock_guard lock(m_Mutex);
        for (auto& [id, entry] : m_Slots) {
            if (entry.slot) {
                entry.slot->Shutdown();
            }
        }
        m_Slots.clear();
    }

    void HotReloadCoordinator::Configure(const HotReloadSettings& settings) {
        std::lock_guard lock(m_Mutex);
        m_Settings = settings;
        CoreLog(BML_LOG_INFO, kLogCategory,
                "Hot reload configured: enabled=%s, debounce=%lldms",
                settings.enabled ? "true" : "false",
                static_cast<long long>(settings.debounce.count()));
    }

    void HotReloadCoordinator::SetServices(const BML_Services *services) noexcept {
        std::lock_guard lock(m_Mutex);
        m_Services = services;
    }

    bool HotReloadCoordinator::RegisterModule(const HotReloadModuleEntry& entry) {
        std::lock_guard lock(m_Mutex);

        if (entry.id.empty()) {
            CoreLog(BML_LOG_ERROR, kLogCategory, "Cannot register module with empty ID");
            return false;
        }

        if (m_Slots.find(entry.id) != m_Slots.end()) {
            CoreLog(BML_LOG_WARN, kLogCategory,
                    "Module '%s' already registered for hot reload", entry.id.c_str());
            return false;
        }

        // Create slot entry
        SlotEntry slot_entry;
        slot_entry.info = entry;
        slot_entry.slot = std::make_unique<ReloadableModuleSlot>(&m_Context);

        // Configure the slot
        ReloadableSlotConfig config;
        config.dll_path = entry.dll_path;
        config.temp_directory = m_Settings.temp_directory;
        config.mod_id = entry.id;
        config.manifest = entry.manifest;
        config.context = &m_Context;
        config.kernel = &m_Kernel;
        config.services = m_Services;

        if (!config.services) {
            CoreLog(BML_LOG_ERROR, kLogCategory,
                    "Cannot register module '%s' for hot reload without runtime services",
                    entry.id.c_str());
            return false;
        }

        if (!slot_entry.slot->Initialize(config)) {
            CoreLog(BML_LOG_ERROR, kLogCategory,
                    "Failed to initialize slot for module '%s'", entry.id.c_str());
            return false;
        }

        // Add watch if watcher is running
        if (m_Running && !entry.watch_path.empty()) {
            std::string watch_path_utf8 = utils::Utf16ToUtf8(entry.watch_path);
            slot_entry.watch_id = m_Watcher->Watch(watch_path_utf8, false);
        }

        m_Slots[entry.id] = std::move(slot_entry);

        CoreLog(BML_LOG_INFO, kLogCategory,
                "Registered module '%s' for hot reload", entry.id.c_str());
        return true;
    }

    void HotReloadCoordinator::UnregisterModule(const std::string& mod_id) {
        std::lock_guard lock(m_Mutex);

        auto it = m_Slots.find(mod_id);
        if (it == m_Slots.end()) {
            return;
        }

        // Remove watch
        if (it->second.watch_id >= 0) {
            m_Watcher->Unwatch(it->second.watch_id);
        }

        // Shutdown slot
        if (it->second.slot) {
            it->second.slot->Shutdown();
        }

        m_Slots.erase(it);

        // Remove from scheduled reloads
        m_Scheduled.erase(
            std::remove_if(m_Scheduled.begin(), m_Scheduled.end(),
                [&mod_id](const ScheduledReload& sr) { return sr.mod_id == mod_id; }),
            m_Scheduled.end());

        CoreLog(BML_LOG_INFO, kLogCategory,
                "Unregistered module '%s' from hot reload", mod_id.c_str());
    }

    std::vector<std::string> HotReloadCoordinator::GetRegisteredModules() const {
        std::lock_guard lock(m_Mutex);
        std::vector<std::string> result;
        result.reserve(m_Slots.size());
        for (const auto& [id, entry] : m_Slots) {
            result.push_back(id);
        }
        return result;
    }

    void HotReloadCoordinator::Start() {
        std::lock_guard lock(m_Mutex);

        if (m_Running) return;
        if (!m_Settings.enabled) {
            CoreLog(BML_LOG_DEBUG, kLogCategory, "Hot reload is disabled");
            return;
        }

        // Add watches for all registered modules
        for (auto& [id, entry] : m_Slots) {
            if (!entry.info.watch_path.empty() && entry.watch_id < 0) {
                std::string watch_path_utf8 = utils::Utf16ToUtf8(entry.info.watch_path);
                entry.watch_id = m_Watcher->Watch(watch_path_utf8, false);
            }
        }

        m_Watcher->Start();
        m_Running = true;

        CoreLog(BML_LOG_INFO, kLogCategory, "Hot reload coordinator started");
    }

    void HotReloadCoordinator::Stop() {
        std::lock_guard lock(m_Mutex);

        if (!m_Running) return;

        m_Watcher->Stop();
        m_Running = false;

        // Clear scheduled reloads
        m_Scheduled.clear();

        // Reset watch IDs
        for (auto& [id, entry] : m_Slots) {
            entry.watch_id = -1;
        }

        CoreLog(BML_LOG_INFO, kLogCategory, "Hot reload coordinator stopped");
    }

    bool HotReloadCoordinator::IsRunning() const {
        std::lock_guard lock(m_Mutex);
        return m_Running;
    }

    void HotReloadCoordinator::Update() {
        {
            std::lock_guard lock(m_Mutex);
            if (!m_Settings.enabled || !m_Running) {
                return;
            }
        }
        ProcessScheduledReloads();
    }

    ReloadResult HotReloadCoordinator::ForceReload(const std::string& mod_id) {
        ReloadNotifyCallback callback;
        unsigned int version = 0;
        ReloadFailure failure = ReloadFailure::None;
        ReloadResult result = ReloadResult::LoadFailed;

        {
            std::lock_guard lock(m_Mutex);

            auto it = m_Slots.find(mod_id);
            if (it == m_Slots.end()) {
                CoreLog(BML_LOG_ERROR, kLogCategory,
                        "Cannot force reload: module '%s' not registered", mod_id.c_str());
                return ReloadResult::LoadFailed;
            }

            if (!it->second.slot) {
                return ReloadResult::LoadFailed;
            }

            CoreLog(BML_LOG_INFO, kLogCategory,
                    "Force reloading module '%s'", mod_id.c_str());

            result = it->second.slot->ForceReload();
            version = it->second.slot->GetVersion();
            failure = it->second.slot->GetLastFailure();
            callback = m_NotifyCallback;
        }

        if (callback) {
            callback(mod_id, result, version, failure);
        }

        return result;
    }

    void HotReloadCoordinator::SetNotifyCallback(ReloadNotifyCallback callback) {
        std::lock_guard lock(m_Mutex);
        m_NotifyCallback = std::move(callback);
    }

    bool HotReloadCoordinator::IsModuleLoaded(const std::string& mod_id) const {
        std::lock_guard lock(m_Mutex);
        auto it = m_Slots.find(mod_id);
        if (it == m_Slots.end() || !it->second.slot) {
            return false;
        }
        return it->second.slot->IsLoaded();
    }

    unsigned int HotReloadCoordinator::GetModuleVersion(const std::string& mod_id) const {
        std::lock_guard lock(m_Mutex);
        auto it = m_Slots.find(mod_id);
        if (it == m_Slots.end() || !it->second.slot) {
            return 0;
        }
        return it->second.slot->GetVersion();
    }

    void HotReloadCoordinator::OnFileChanged(const FileEvent& event) {
        if (event.action != FileAction::Modified) {
            return;
        }

        std::string mod_id = FindModuleForEvent(event.directory, event.filename);
        if (mod_id.empty()) {
            CoreLog(BML_LOG_DEBUG, kLogCategory,
                    "Ignoring change to unregistered file: %s/%s",
                    event.directory.c_str(), event.filename.c_str());
            return;
        }

        CoreLog(BML_LOG_DEBUG, kLogCategory,
                "Detected watched change for module '%s': %s/%s",
                mod_id.c_str(), event.directory.c_str(), event.filename.c_str());

        // Modules with a runtime provider (e.g. scripting) use the provider's
        // in-process reload.  Native DLLs go through the slot's version-copy /
        // SEH / rollback path, and the notify callback triggers targeted reload
        // in ModuleRuntime.
        ScheduleReload(mod_id, HasRuntimeProvider(mod_id));
    }

    bool HotReloadCoordinator::HasRuntimeProvider(const std::string &mod_id) const {
        auto snapshot = m_Context.GetLoadedModuleSnapshot();
        for (const auto &m : snapshot) {
            if (m.id == mod_id && m.mod_handle) {
                std::string entry_utf8 = utils::Utf16ToUtf8(m.path);
                return m_Context.FindRuntimeProvider(entry_utf8) != nullptr;
            }
        }
        return false;
    }

    void HotReloadCoordinator::ScheduleReload(const std::string& mod_id, bool requires_runtime_reload) {
        std::lock_guard lock(m_Mutex);

        auto now = std::chrono::steady_clock::now();
        auto fire_time = now + m_Settings.debounce;

        // Check if already scheduled
        for (auto& sr : m_Scheduled) {
            if (sr.mod_id == mod_id) {
                // Update fire time (reset debounce)
                sr.fire_time = fire_time;
                sr.requires_runtime_reload = sr.requires_runtime_reload || requires_runtime_reload;
                CoreLog(BML_LOG_DEBUG, kLogCategory,
                        "Reset debounce for module '%s'", mod_id.c_str());
                return;
            }
        }

        // Add new scheduled reload
        m_Scheduled.push_back({mod_id, fire_time, requires_runtime_reload});
        CoreLog(BML_LOG_DEBUG, kLogCategory,
                "Scheduled reload for module '%s' (debounce %lldms, runtime_reload=%s)",
                mod_id.c_str(),
                static_cast<long long>(m_Settings.debounce.count()),
                requires_runtime_reload ? "true" : "false");
    }

    void HotReloadCoordinator::ProcessScheduledReloads() {
        std::vector<ScheduledReload> ready;
        {
            std::lock_guard lock(m_Mutex);
            if (!m_Running) {
                return;
            }

            const auto now = std::chrono::steady_clock::now();
            for (auto it = m_Scheduled.begin(); it != m_Scheduled.end();) {
                if (now >= it->fire_time) {
                    ready.push_back(*it);
                    it = m_Scheduled.erase(it);
                } else {
                    ++it;
                }
            }
        }

        for (const auto &scheduled : ready) {
            ReloadNotifyCallback callback;
            ReloadResult result = ReloadResult::LoadFailed;
            unsigned int version = 0;
            ReloadFailure failure = ReloadFailure::None;
            bool processed = false;

            {
                std::lock_guard lock(m_Mutex);
                auto it = m_Slots.find(scheduled.mod_id);
                if (it == m_Slots.end() || !it->second.slot) {
                    continue;
                }

                CoreLog(BML_LOG_INFO, kLogCategory,
                        "Processing scheduled reload for module '%s'", scheduled.mod_id.c_str());

                if (scheduled.requires_runtime_reload) {
                    // Check if a runtime provider owns this module
                    auto snapshot = m_Context.GetLoadedModuleSnapshot();
                    const BML_ModuleRuntimeProvider *provider = nullptr;
                    BML_Mod mod_handle = nullptr;
                    for (const auto &m : snapshot) {
                        if (m.id == scheduled.mod_id && m.mod_handle) {
                            std::string entry_utf8 = utils::Utf16ToUtf8(m.path);
                            provider = m_Context.FindRuntimeProvider(entry_utf8);
                            mod_handle = m.mod_handle;
                            break;
                        }
                    }
                    if (provider && provider->ReloadModule && mod_handle) {
                        BML_Result reload_result = provider->ReloadModule(mod_handle);
                        result = (reload_result == BML_RESULT_OK)
                            ? ReloadResult::Success : ReloadResult::LoadFailed;
                    } else {
                        result = ReloadResult::Success;
                    }
                    version = it->second.slot->GetVersion();
                    failure = ReloadFailure::None;
                    // Provider handled reload internally — do NOT fire notify
                    // callback, which would trigger a redundant full
                    // ReloadModules() via HandleHotReloadNotify().
                    // Only propagate failures so the runtime can fall back.
                    if (result == ReloadResult::Success) {
                        callback = {};
                    } else {
                        callback = m_NotifyCallback;
                    }
                } else {
                    result = it->second.slot->Reload();
                    version = it->second.slot->GetVersion();
                    failure = it->second.slot->GetLastFailure();
                    callback = m_NotifyCallback;
                }
                processed = true;
            }

            if (!processed) {
                continue;
            }

            CoreLog(BML_LOG_INFO, kLogCategory,
                    "Reload of '%s' completed: %s (version %u)",
                    scheduled.mod_id.c_str(), ReloadResultToString(result), version);

            if (callback) {
                callback(scheduled.mod_id, result, version, failure);
            }
        }
    }

    std::string HotReloadCoordinator::FindModuleForEvent(const std::string& dir,
                                                          const std::string& filename) {
        std::lock_guard lock(m_Mutex);

        const auto event_path = NormalizePath(std::filesystem::path(dir) / filename);

        // Search for a direct DLL match first.
        for (const auto& [id, entry] : m_Slots) {
            if (entry.info.dll_path.empty()) continue;

            if (event_path == NormalizePath(std::filesystem::path(entry.info.dll_path))) {
                return id;
            }
        }

        // Manifest edits should also trigger a runtime reload, but unrelated files
        // in the module root should not.
        for (const auto& [id, entry] : m_Slots) {
            if (!entry.info.manifest || entry.info.manifest->manifest_path.empty()) continue;

            if (event_path == NormalizePath(std::filesystem::path(entry.info.manifest->manifest_path))) {
                return id;
            }
        }

        // For runtime-managed script modules, changes to auxiliary script files
        // inside the module directory should also trigger a reload.
        for (const auto& [id, entry] : m_Slots) {
            if (!entry.info.manifest || entry.info.watch_path.empty() || entry.info.dll_path.empty()) continue;

            const auto entry_path = NormalizePath(std::filesystem::path(entry.info.dll_path));
            if (IsModuleBinaryFile(entry_path.string())) continue;

            const auto watch_root = NormalizePath(std::filesystem::path(entry.info.watch_path));
            const auto relative_path = event_path.lexically_relative(watch_root);
            if (relative_path.empty() || relative_path.native().empty() ||
                relative_path == std::filesystem::path(".") || relative_path.string().starts_with("..")) {
                continue;
            }

            if (event_path.extension() == entry_path.extension()) {
                return id;
            }
        }

        return {};
    }

    bool HotReloadCoordinator::GetSlotModuleInfo(const std::string &mod_id,
                                                    HMODULE *out_handle,
                                                    PFN_BML_ModEntrypoint *out_entrypoint) const {
        std::lock_guard lock(m_Mutex);
        auto it = m_Slots.find(mod_id);
        if (it == m_Slots.end() || !it->second.slot || !it->second.slot->IsLoaded()) {
            return false;
        }
        if (out_handle) *out_handle = it->second.slot->GetHandle();
        if (out_entrypoint) *out_entrypoint = it->second.slot->GetEntrypoint();
        return true;
    }

} // namespace BML::Core



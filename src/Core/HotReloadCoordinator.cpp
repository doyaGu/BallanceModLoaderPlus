#include "HotReloadCoordinator.h"

#include <algorithm>
#include <filesystem>

#include "Context.h"
#include "Logging.h"
#include "StringUtils.h"

namespace BML::Core {

    namespace {
        constexpr char kLogCategory[] = "hot.reload";

        bool IsDllFile(const std::string& filename) {
            if (filename.size() < 4) return false;
            std::string ext = filename.substr(filename.size() - 4);
            // Case-insensitive comparison
            for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return ext == ".dll";
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
    }

    HotReloadCoordinator::HotReloadCoordinator(Context& context)
        : m_Context(context)
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
        slot_entry.slot = std::make_unique<ReloadableModuleSlot>();

        // Configure the slot
        ReloadableSlotConfig config;
        config.dll_path = entry.dll_path;
        config.temp_directory = m_Settings.temp_directory;
        config.manifest = entry.manifest;
        config.context = &m_Context;
        config.get_proc = &bmlGetProcAddress;

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
        if (!m_Settings.enabled) return;

        std::lock_guard lock(m_Mutex);
        if (!m_Running) return;

        ProcessScheduledReloads();
    }

    ReloadResult HotReloadCoordinator::ForceReload(const std::string& mod_id) {
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

        auto result = it->second.slot->ForceReload();

        // Notify callback
        if (m_NotifyCallback) {
            m_NotifyCallback(mod_id, result,
                it->second.slot->GetVersion(),
                it->second.slot->GetLastFailure());
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
        // Only care about DLL modifications
        if (event.action != FileAction::Modified) return;
        if (!IsDllFile(event.filename)) return;

        // Find which module this file belongs to
        std::string mod_id = FindModuleByPath(event.directory, event.filename);
        if (mod_id.empty()) {
            CoreLog(BML_LOG_DEBUG, kLogCategory,
                    "Ignoring change to unregistered file: %s/%s",
                    event.directory.c_str(), event.filename.c_str());
            return;
        }

        CoreLog(BML_LOG_DEBUG, kLogCategory,
                "Detected change to module '%s' DLL", mod_id.c_str());

        // Schedule reload with debouncing
        ScheduleReload(mod_id);
    }

    void HotReloadCoordinator::ScheduleReload(const std::string& mod_id) {
        std::lock_guard lock(m_Mutex);

        auto now = std::chrono::steady_clock::now();
        auto fire_time = now + m_Settings.debounce;

        // Check if already scheduled
        for (auto& sr : m_Scheduled) {
            if (sr.mod_id == mod_id) {
                // Update fire time (reset debounce)
                sr.fire_time = fire_time;
                CoreLog(BML_LOG_DEBUG, kLogCategory,
                        "Reset debounce for module '%s'", mod_id.c_str());
                return;
            }
        }

        // Add new scheduled reload
        m_Scheduled.push_back({mod_id, fire_time});
        CoreLog(BML_LOG_DEBUG, kLogCategory,
                "Scheduled reload for module '%s' (debounce %lldms)",
                mod_id.c_str(), static_cast<long long>(m_Settings.debounce.count()));
    }

    void HotReloadCoordinator::ProcessScheduledReloads() {
        auto now = std::chrono::steady_clock::now();

        // Find reloads that are ready
        std::vector<std::string> ready;
        for (auto it = m_Scheduled.begin(); it != m_Scheduled.end(); ) {
            if (now >= it->fire_time) {
                ready.push_back(it->mod_id);
                it = m_Scheduled.erase(it);
            } else {
                ++it;
            }
        }

        // Process ready reloads
        for (const auto& mod_id : ready) {
            auto it = m_Slots.find(mod_id);
            if (it == m_Slots.end() || !it->second.slot) {
                continue;
            }

            CoreLog(BML_LOG_INFO, kLogCategory,
                    "Processing scheduled reload for module '%s'", mod_id.c_str());

            auto result = it->second.slot->Reload();

            CoreLog(BML_LOG_INFO, kLogCategory,
                    "Reload of '%s' completed: %s (version %u)",
                    mod_id.c_str(), ReloadResultToString(result),
                    it->second.slot->GetVersion());

            // Notify callback
            if (m_NotifyCallback) {
                m_NotifyCallback(mod_id, result,
                    it->second.slot->GetVersion(),
                    it->second.slot->GetLastFailure());
            }
        }
    }

    std::string HotReloadCoordinator::FindModuleByPath(const std::string& dir,
                                                        const std::string& filename) {
        std::lock_guard lock(m_Mutex);

        // Normalize the full path
        std::filesystem::path full_path = std::filesystem::path(dir) / filename;
        std::error_code ec;
        auto normalized = std::filesystem::weakly_canonical(full_path, ec);
        if (ec) {
            normalized = full_path;
        }

        // Search for matching module
        for (const auto& [id, entry] : m_Slots) {
            if (entry.info.dll_path.empty()) continue;

            std::filesystem::path dll_path(entry.info.dll_path);
            auto dll_normalized = std::filesystem::weakly_canonical(dll_path, ec);
            if (ec) {
                dll_normalized = dll_path;
            }

            if (normalized == dll_normalized) {
                return id;
            }
        }

        return {};
    }

} // namespace BML::Core

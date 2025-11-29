#ifndef BML_CORE_HOT_RELOAD_COORDINATOR_H
#define BML_CORE_HOT_RELOAD_COORDINATOR_H

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "FileSystemWatcher.h"
#include "ReloadableModuleSlot.h"

namespace BML::Core {
    class Context;
    struct ModManifest;

    /**
     * @brief Hot reload system configuration
     */
    struct HotReloadSettings {
        bool enabled{false};                     ///< Master enable switch
        std::chrono::milliseconds debounce{500}; ///< Debounce interval
        std::wstring temp_directory;             ///< Temp directory for DLL copies
    };

    /**
     * @brief Information for registering a module for hot reload
     */
    struct HotReloadModuleEntry {
        std::string id;                       ///< Module ID
        std::wstring dll_path;                ///< Path to the DLL
        std::wstring watch_path;              ///< Directory to watch for changes
        const ModManifest *manifest{nullptr}; ///< Module manifest
    };

    /**
     * @brief Callback for reload events
     *
     * Called after a reload attempt completes (success or failure).
     */
    using ReloadNotifyCallback = std::function<void(
        const std::string &mod_id, ///< Module that was reloaded
        ReloadResult result,       ///< Result of the reload
        unsigned int version,      ///< New version number
        ReloadFailure failure      ///< Failure reason (if any)
    )>;

    /**
     * @brief Coordinates hot reloading of multiple modules
     *
     * The coordinator:
     * - Watches directories for file changes using FileSystemWatcher
     * - Debounces rapid changes to avoid multiple reloads
     * - Schedules reloads to be processed in the main loop
     * - Manages ReloadableModuleSlot instances for each module
     *
     * Usage:
     * @code
     * HotReloadCoordinator coordinator(context);
     *
     * HotReloadSettings settings;
     * settings.enabled = true;
     * settings.debounce = std::chrono::milliseconds(500);
     * settings.temp_directory = L"C:/Temp/BML_HotReload";
     * coordinator.Configure(settings);
     *
     * HotReloadModuleEntry entry;
     * entry.id = "MyMod";
     * entry.dll_path = L"C:/Mods/MyMod/MyMod.dll";
     * entry.watch_path = L"C:/Mods/MyMod";
     * entry.manifest = &manifest;
     * coordinator.RegisterModule(entry);
     *
     * coordinator.Start();
     *
     * // In game loop:
     * coordinator.Update();  // Process pending reloads
     * @endcode
     */
    class HotReloadCoordinator {
    public:
        explicit HotReloadCoordinator(Context &context);
        ~HotReloadCoordinator();

        HotReloadCoordinator(const HotReloadCoordinator &) = delete;
        HotReloadCoordinator &operator=(const HotReloadCoordinator &) = delete;

        /**
         * @brief Configure the hot reload system
         * @param settings Configuration settings
         */
        void Configure(const HotReloadSettings &settings);

        /**
         * @brief Get current settings
         */
        const HotReloadSettings &GetSettings() const { return m_Settings; }

        /**
         * @brief Register a module for hot reloading
         * @param entry Module information
         * @return true if registration succeeded
         */
        bool RegisterModule(const HotReloadModuleEntry &entry);

        /**
         * @brief Unregister a module from hot reloading
         * @param mod_id Module ID to unregister
         */
        void UnregisterModule(const std::string &mod_id);

        /**
         * @brief Get list of registered module IDs
         */
        std::vector<std::string> GetRegisteredModules() const;

        /**
         * @brief Start watching for file changes
         */
        void Start();

        /**
         * @brief Stop watching for file changes
         */
        void Stop();

        /**
         * @brief Check if the coordinator is running
         */
        bool IsRunning() const;

        /**
         * @brief Process pending reloads
         *
         * Call this from the main game loop to process scheduled reloads.
         * Reloads are deferred to the main thread for safety.
         */
        void Update();

        /**
         * @brief Force immediate reload of a specific module
         * @param mod_id Module ID to reload
         * @return Result of the reload operation
         */
        ReloadResult ForceReload(const std::string &mod_id);

        /**
         * @brief Set callback for reload notifications
         * @param callback Function to call after reload attempts
         */
        void SetNotifyCallback(ReloadNotifyCallback callback);

        /**
         * @brief Check if a module is currently loaded
         * @param mod_id Module ID
         */
        bool IsModuleLoaded(const std::string &mod_id) const;

        /**
         * @brief Get current version of a module
         * @param mod_id Module ID
         * @return Version number, or 0 if not found
         */
        unsigned int GetModuleVersion(const std::string &mod_id) const;

    private:
        // File change handler
        void OnFileChanged(const FileEvent &event);

        // Schedule a module for reload (with debouncing)
        void ScheduleReload(const std::string &mod_id);

        // Process all scheduled reloads
        void ProcessScheduledReloads();

        // Find module ID by file path
        std::string FindModuleByPath(const std::string &dir, const std::string &filename);

        // Reference to BML context
        Context &m_Context;

        // Configuration
        HotReloadSettings m_Settings;

        // File system watcher
        std::unique_ptr<FileSystemWatcher> m_Watcher;

        // Per-module data
        struct SlotEntry {
            HotReloadModuleEntry info;
            std::unique_ptr<ReloadableModuleSlot> slot;
            long watch_id{-1};
        };

        mutable std::mutex m_Mutex;
        std::unordered_map<std::string, SlotEntry> m_Slots;

        // Scheduled reloads (debouncing)
        struct ScheduledReload {
            std::string mod_id;
            std::chrono::steady_clock::time_point fire_time; // When to actually reload
        };

        std::vector<ScheduledReload> m_Scheduled;

        // Notification callback
        ReloadNotifyCallback m_NotifyCallback;

        // Running state
        bool m_Running{false};
    };
} // namespace BML::Core

#endif // BML_CORE_HOT_RELOAD_COORDINATOR_H

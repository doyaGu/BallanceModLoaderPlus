#ifndef BML_CORE_RELOADABLE_MODULE_SLOT_H
#define BML_CORE_RELOADABLE_MODULE_SLOT_H

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "bml_export.h"
#include "ModManifest.h"
#include "ModHandle.h"
#include "PlatformCompat.h"

namespace BML::Core {
    class Context;
    struct KernelServices;

    /**
     * @brief Reload operation type
     */
    enum class ReloadOp {
        Load,       ///< After reload, initialize/restore state
        Unload,     ///< Before reload, save state and cleanup
        Close       ///< Final shutdown, no more reloads
    };

    /**
     * @brief Result of a reload operation
     */
    enum class ReloadResult {
        Success,            ///< Reload completed successfully
        NoChange,           ///< No reload needed (file unchanged)
        LoadFailed,         ///< DLL load failed
        EntrypointMissing,  ///< BML_ModEntrypoint export not found
        InitFailed,         ///< Entrypoint returned error
        UnloadBlocked,      ///< PREPARE_DETACH or gate rejected unload/reload
        Crashed,            ///< SEH caught exception during reload
        RolledBack          ///< Rolled back to previous version after failure
    };

    /**
     * @brief Failure reason for diagnostics
     */
    enum class ReloadFailure {
        None,               ///< No failure
        SegFault,           ///< Access violation (EXCEPTION_ACCESS_VIOLATION)
        IllegalInstruction, ///< Invalid instruction (EXCEPTION_ILLEGAL_INSTRUCTION)
        StackOverflow,      ///< Stack overflow (EXCEPTION_STACK_OVERFLOW)
        BadImage,           ///< DLL not ready or invalid
        StateInvalidated,   ///< State size mismatch (future use)
        UserError,          ///< Mod entrypoint returned negative value
        DetachBlocked,      ///< PREPARE_DETACH or dependency gate blocked reload
        InitialFailure,     ///< Version 1 crashed, cannot rollback
        SystemError,        ///< Windows API error
        Other               ///< Unknown error
    };

    /**
     * @brief Configuration for a reloadable module slot
     */
    struct ReloadableSlotConfig {
        std::wstring dll_path;          ///< Original DLL path (source)
        std::wstring temp_directory;    ///< Directory for versioned copies
        std::string mod_id;             ///< Module identifier
        std::optional<ModManifest> manifest;   ///< Mod manifest snapshot
        Context* context{nullptr};      ///< BML context
        KernelServices *kernel{nullptr}; ///< Runtime kernel services
        const BML_Services *services{nullptr}; ///< Runtime-owned service bundle
    };

    /**
     * @brief Manages a single hot-reloadable module
     *
     * This class implements the core hot-reload functionality:
     * - Copies DLL to temporary location with version suffix
     * - Loads/unloads DLL with proper lifecycle callbacks
     * - Catches crashes using SEH and rolls back to previous version
     * - Preserves user data across reloads
     *
     * Design inspired by cr.h but integrated natively with BML.
     *
     * Usage:
     * @code
     * ReloadableModuleSlot slot;
     * ReloadableSlotConfig config;
     * config.dll_path = L"C:/Mods/MyMod/MyMod.dll";
     * config.temp_directory = L"C:/Temp/BML_HotReload";
     * config.context = &context;
     * config.kernel = &kernel;
     * config.services = services;
     *
     * if (slot.Initialize(config)) {
     *     // In game loop:
     *     auto result = slot.Reload();  // Checks and reloads if needed
     * }
     * @endcode
     */
    class ReloadableModuleSlot {
    public:
        explicit ReloadableModuleSlot(class Context *ctx = nullptr);
        ~ReloadableModuleSlot();

        ReloadableModuleSlot(const ReloadableModuleSlot&) = delete;
        ReloadableModuleSlot& operator=(const ReloadableModuleSlot&) = delete;

        /**
         * @brief Initialize the slot with configuration
         * @param config Slot configuration
         * @return true if initialization succeeded
         */
        bool Initialize(const ReloadableSlotConfig& config);

        /**
         * @brief Shutdown and cleanup all versioned files
         */
        void Shutdown();

        /**
         * @brief Check if the source DLL has been modified
         * @return true if the file has changed since last load
         */
        bool HasChanged() const;

        /**
         * @brief Check for changes and reload if needed
         * @return Result of the reload operation
         */
        ReloadResult Reload();

        /**
         * @brief Force an immediate reload attempt
         * @return Result of the reload operation
         */
        ReloadResult ForceReload();

        /**
         * @brief Check if a module is currently loaded
         */
        bool IsLoaded() const { return m_Handle != nullptr; }

        /**
         * @brief Get the current version number
         *
         * Version 1 is the first load, increments with each reload.
         */
        unsigned int GetVersion() const { return m_Version; }

        /**
         * @brief Get the last failure reason
         */
        ReloadFailure GetLastFailure() const { return m_LastFailure; }

        /**
         * @brief Get the last system error code
         */
        DWORD GetLastSystemError() const { return m_LastSystemError; }

        /**
         * @brief Get the original DLL path
         */
        const std::wstring& GetPath() const { return m_Config.dll_path; }

        /**
         * @brief Get the mod ID
         */
        const std::string& GetModId() const;

        /**
         * @brief Get mod-controlled user data
         *
         * This pointer is preserved across reloads, allowing mods
         * to maintain state without serialization.
         */
        void* GetUserData() const { return m_UserData; }

        /**
         * @brief Set mod-controlled user data
         */
        void SetUserData(void* data) { m_UserData = data; }

    private:
        class Context *m_Context;

        // Core operations
        bool LoadVersion(unsigned int version, bool is_rollback);
        bool UnloadCurrent(bool is_rollback, bool is_close);
        bool TryRollback();

        // DLL file operations
        std::wstring MakeVersionPath(unsigned int version) const;
        bool CopyDllToTemp(unsigned int version);
        HMODULE LoadDll(const std::wstring& path);

        // State management (future expansion)
        void SaveState();
        void RestoreState();
        void BackupState();

        // Crash protection
        int InvokeEntrypoint(ReloadOp op);
        static int FilterException(unsigned long code, ReloadFailure& out_failure);

        // Cleanup
        void CleanupVersionFiles();

        // Configuration
        ReloadableSlotConfig m_Config;

        // Current loaded module
        HMODULE m_Handle{nullptr};
        PFN_BML_ModEntrypoint m_Entrypoint{nullptr};
        std::unique_ptr<BML_Mod_T> m_ModHandle;

        // Version tracking
        unsigned int m_Version{0};
        unsigned int m_NextVersion{1};
        unsigned int m_LastWorkingVersion{0};
        std::filesystem::file_time_type m_LastWriteTime;

        // State preservation
        void* m_UserData{nullptr};

        // Error tracking
        ReloadFailure m_LastFailure{ReloadFailure::None};
        DWORD m_LastSystemError{0};
    };

} // namespace BML::Core

#endif // BML_CORE_RELOADABLE_MODULE_SLOT_H

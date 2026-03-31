#ifndef BML_CORE_CONTEXT_H
#define BML_CORE_CONTEXT_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "bml_core.h"
#include "bml_errors.h"
#include "bml_types.h"

#include "bml_services.hpp"

#include "bml_module_runtime.h"

#include "PlatformCompat.h"
#include "ModHandle.h"
#include "ModManifest.h"
#include "ModuleLoader.h"

namespace BML::Core {
    class Context;
    struct KernelServices;
}

struct BML_Context_T {
    std::atomic<BML::Core::Context *> context{nullptr};
    std::atomic<BML::Core::KernelServices *> kernel{nullptr};
    std::atomic<bool> live{false};
};

namespace BML::Core {
    struct LoadedModuleSnapshot {
        std::string id;
        std::optional<ModManifest> manifest;
        HMODULE handle{nullptr};
        PFN_BML_ModEntrypoint entrypoint{nullptr};
        std::wstring path;
        BML_Mod mod_handle{nullptr};
    };

    /**
     * @brief Core runtime context -- manages module handles, lifecycle, and shutdown.
     *
     * Lock acquisition order (must be respected to prevent deadlock):
     *   1. m_StateMutex           -- module state, handle maps, shutdown gating
     *   2. m_RetainMutex          -- reference counting drain
     *   3. m_UserDataMutex        -- per-context user data storage
     *   4. m_RuntimeProviderMutex -- runtime provider registry
     *
     * m_RetainTraceMutex is independent (diagnostic only, never held with others).
     *
     * Cleanup() releases m_StateMutex before draining retains to avoid
     * blocking RetainHandle() callers that also need m_StateMutex.
     */
    class Context {
    public:
#if defined(BML_TEST)
        /**
         * @brief Scoped ambient module binding for lifecycle transitions.
         *
         * This wrapper exists only for internal runtime lifecycle code
         * that must temporarily establish a current module while running
         * attach/detach/shutdown paths. New business logic must not rely
         * on ambient module state for correctness.
         */
        class LifecycleModuleScope {
        public:
            explicit LifecycleModuleScope(BML_Mod mod) noexcept
#if defined(BML_TEST)
                : m_Previous(GetLifecycleModule())
#endif
            {
                SetLifecycleModule(mod);
            }

            ~LifecycleModuleScope() {
                SetLifecycleModule(m_Previous);
            }

            LifecycleModuleScope(const LifecycleModuleScope &) = delete;
            LifecycleModuleScope &operator=(const LifecycleModuleScope &) = delete;

        private:
            BML_Mod m_Previous{nullptr};
        };
#endif

        static Context *FromHandle(BML_Context handle) noexcept;
        static KernelServices *KernelFromHandle(BML_Context handle) noexcept;
        static Context *ContextFromMod(BML_Mod mod) noexcept;
        static KernelServices *KernelFromMod(BML_Mod mod) noexcept;

        enum class ShutdownState {
            Running,
            ShutdownRequested,
            DrainingRetains,
            ShuttingDownModules,
            Stopped,
        };

        BML_Context GetHandle();
        void BindKernel(KernelServices &kernel) noexcept;
        KernelServices *GetKernelServices() const noexcept { return m_Kernel; }

        /**
         * Initialize the context with runtime version.
         * Should be called once during microkernel startup.
         */
        void Initialize(const BML_Version &runtime_version);
        uint64_t GetMainThreadToken() const noexcept;
        bool IsMainThread() const noexcept;

        /**
         * Cleanup all resources and reset to initial state.
         * Should be called during microkernel shutdown.
         */
        void Cleanup(KernelServices &kernel);

        /**
         * Check if context is initialized.
         */
        bool IsInitialized() const { return m_Initialized.load(std::memory_order_acquire); }

        void RegisterManifest(std::unique_ptr<ModManifest> manifest);
        std::vector<ModManifest> GetManifestSnapshot() const;
        void ClearManifests();

        void AddLoadedModule(LoadedModule module);
        std::vector<LoadedModuleSnapshot> GetLoadedModuleSnapshot() const;
        uint32_t GetLoadedModuleCount() const;
        BML_Mod GetLoadedModuleAt(uint32_t index) const;
        void ShutdownModules(KernelServices &kernel);

        /**
         * @brief Update a loaded module's DLL handle and entrypoint after hot-reload.
         *
         * Used by the targeted reload path to synchronize Context's tracking
         * with the ReloadableModuleSlot's new DLL without a full teardown.
         * @return true if the module was found and updated.
         */
        bool UpdateLoadedModule(const std::string &mod_id,
                                HMODULE new_handle,
                                PFN_BML_ModEntrypoint new_entrypoint);

        std::unique_ptr<BML_Mod_T> CreateModHandle(const ModManifest &manifest);
        BML_Mod_T *ResolveModHandle(BML_Mod mod);
        const BML_Mod_T *ResolveModHandle(BML_Mod mod) const;
        BML_Mod GetModHandleById(const std::string &id) const;
        BML_Mod GetModHandleByModule(HMODULE module) const;
        BML_Mod GetSyntheticHostModule() const;
        void AppendShutdownHook(BML_Mod mod, BML_ShutdownCallback callback, void *user_data);
#if defined(BML_TEST)
        static void SetLifecycleModule(BML_Mod mod);
        static BML_Mod GetLifecycleModule();
#endif

        void RemoveCreatedModHandle(BML_Mod mod);

        void SetRuntimeVersion(const BML_Version &version);
        BML_Version GetRuntimeVersionCopy() const;
        ShutdownState GetShutdownState() const;

        BML_Result RetainHandle();
        BML_Result ReleaseHandle();
        uint32_t GetRetainCountForTest() const;

        // User data management
        BML_Result SetUserData(const char *key, void *data, BML_UserDataDestructor destructor);
        BML_Result GetUserData(const char *key, void **out_data) const;

        const bml::RuntimeServiceHub *GetServiceHub() const noexcept { return &m_Hub; }
        bml::RuntimeServiceHub *GetServiceHubMutable() noexcept { return &m_Hub; }

        // Runtime provider registry
        BML_Result RegisterRuntimeProvider(const BML_ModuleRuntimeProvider *provider,
                                           const std::string &owner_id);
        BML_Result UnregisterRuntimeProvider(const BML_ModuleRuntimeProvider *provider);
        void InvalidateRuntimeProvider(const std::string &owner_id);
        bool HasRuntimeProviderOwner(const std::string &owner_id) const;
        const BML_ModuleRuntimeProvider *FindRuntimeProvider(const std::string &entry_path_utf8) const;

        static std::wstring SanitizeIdentifierForFilename(const std::string &value);

        explicit Context(class ApiRegistry &api_registry, class ConfigStore &config,
                         class CrashDumpWriter &crash_dump, class FaultTracker &fault_tracker);
        ~Context() = default;

        class CrashDumpWriter &GetCrashDump() { return m_CrashDump; }
        class FaultTracker &GetFaultTracker() { return m_FaultTracker; }

    private:
        Context(const Context &) = delete;
        Context &operator=(const Context &) = delete;

        struct ShutdownModuleRecord {
            std::string id;
            BML_Mod mod{nullptr};
            HMODULE handle{nullptr};
        };

        class ApiRegistry &m_ApiRegistry;
        class ConfigStore &m_Config;
        class CrashDumpWriter &m_CrashDump;
        class FaultTracker &m_FaultTracker;

        std::vector<LoadedModule> ExtractLoadedModulesForShutdownLocked();
        std::vector<ShutdownModuleRecord> BuildShutdownModuleRecords(
            const std::vector<LoadedModule> &modules) const;
        void RunShutdownHooks(const std::vector<LoadedModule> &modules);
        void PreUnloadCleanup(const std::vector<LoadedModule> &modules);
        void FinalizeShutdownModuleRecords(const std::vector<ShutdownModuleRecord> &records);
        BML_Mod_T *FindModHandleLocked(BML_Mod mod);
        const BML_Mod_T *FindModHandleLocked(BML_Mod mod) const;
        void RecordRetainEvent(bool is_retain, uint32_t count_after);
        void LogRetainWaitStatus(std::chrono::milliseconds elapsed) const;

        struct RetainEvent {
            uint64_t sequence{0};
            uint64_t thread_token{0};
            uint32_t count_after{0};
            bool is_retain{false};
        };

        bml::RuntimeServiceHub m_Hub{};
        std::vector<std::unique_ptr<ModManifest>> m_Manifests;
        std::vector<LoadedModule> m_LoadedModules;
        std::unordered_map<std::string, BML_Mod> m_ModHandlesById;
        std::unordered_map<HMODULE, BML_Mod> m_ModHandlesByModule;
        std::unordered_map<BML_Mod, BML_Mod_T *> m_ModHandlesByPtr;
        std::unique_ptr<BML_Mod_T> m_HostModHandle;
        BML_Version m_RuntimeVersion{};
        BML_Context m_Handle{nullptr};
        KernelServices *m_Kernel{nullptr};
        mutable std::mutex m_StateMutex;
        mutable std::mutex m_RetainMutex;
        mutable std::mutex m_RetainTraceMutex;
        std::condition_variable m_RetainCv;
        std::atomic<uint32_t> m_RetainCount{0};
        std::atomic<ShutdownState> m_ShutdownState{ShutdownState::Stopped};
        std::atomic<uint64_t> m_MainThreadToken{0};
        bool m_CleanupRequested{false};
        std::atomic<bool> m_Initialized{false};
        std::vector<RetainEvent> m_RetainTrace;
        uint64_t m_RetainTraceSequence{0};

        // User data storage
        struct UserDataEntry {
            void *data;
            BML_UserDataDestructor destructor;
        };
        std::unordered_map<std::string, UserDataEntry> m_UserData;
        mutable std::mutex m_UserDataMutex;

        // Runtime provider registry
        struct RuntimeProviderEntry {
            const BML_ModuleRuntimeProvider *provider;
            std::string owner_id;
        };
        std::vector<RuntimeProviderEntry> m_RuntimeProviders;
        mutable std::mutex m_RuntimeProviderMutex;
    };
} // namespace BML::Core

#endif // BML_CORE_CONTEXT_H

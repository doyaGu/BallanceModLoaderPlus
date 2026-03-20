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
#include <unordered_set>
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
    struct LoadedModuleSnapshot {
        std::string id;
        std::optional<ModManifest> manifest;
        HMODULE handle{nullptr};
        PFN_BML_ModEntrypoint entrypoint{nullptr};
        std::wstring path;
        BML_Mod mod_handle{nullptr};
    };

    class Context {
    public:
        enum class ShutdownState {
            Running,
            ShutdownRequested,
            DrainingRetains,
            ShuttingDownModules,
            Stopped,
        };

        static Context &Instance();

        BML_Context GetHandle();

        /**
         * Initialize the context with runtime version.
         * Should be called once during microkernel startup.
         */
        void Initialize(const BML_Version &runtime_version);

        /**
         * Cleanup all resources and reset to initial state.
         * Should be called during microkernel shutdown.
         */
        void Cleanup();

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
        void ShutdownModules();

        std::unique_ptr<BML_Mod_T> CreateModHandle(const ModManifest &manifest);
        BML_Mod_T *ResolveModHandle(BML_Mod mod);
        const BML_Mod_T *ResolveModHandle(BML_Mod mod) const;
        BML_Mod_T *ResolveCurrentConsumer();
        const BML_Mod_T *ResolveCurrentConsumer() const;
        BML_Mod GetModHandleById(const std::string &id) const;
        BML_Mod GetModHandleByModule(HMODULE module) const;
        BML_Mod GetSyntheticHostModule() const;
        void AppendShutdownHook(BML_Mod mod, BML_ShutdownCallback callback, void *user_data);

        static void SetCurrentModule(BML_Mod mod);
        static BML_Mod GetCurrentModule();

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
        const BML_ModuleRuntimeProvider *FindRuntimeProvider(const std::string &entry_path_utf8) const;

        static std::wstring SanitizeIdentifierForFilename(const std::string &value);

    private:
        Context();
        ~Context() = default;
        Context(const Context &) = delete;
        Context &operator=(const Context &) = delete;

        void ShutdownModulesLocked();
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
        std::unordered_set<BML_Mod> m_CreatedModHandles;
        std::unique_ptr<BML_Mod_T> m_HostModHandle;
        BML_Version m_RuntimeVersion{};
        mutable std::mutex m_StateMutex;
        mutable std::mutex m_RetainMutex;
        mutable std::mutex m_RetainTraceMutex;
        std::condition_variable m_RetainCv;
        std::atomic<uint32_t> m_RetainCount{0};
        std::atomic<ShutdownState> m_ShutdownState{ShutdownState::Stopped};
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

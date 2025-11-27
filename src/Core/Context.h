#ifndef BML_CORE_CONTEXT_H
#define BML_CORE_CONTEXT_H

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "bml_errors.h"
#include "bml_types.h"

#include "ModHandle.h"
#include "ModManifest.h"
#include "ModuleLoader.h"

namespace BML::Core {
    class Context {
    public:
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
        const std::vector<std::unique_ptr<ModManifest>> &GetManifests() const;
        void ClearManifests();

        void AddLoadedModule(LoadedModule module);
        const std::vector<LoadedModule> &GetLoadedModules() const;
        void ShutdownModules();

        std::unique_ptr<BML_Mod_T> CreateModHandle(const ModManifest &manifest) const;
        BML_Mod_T *ResolveModHandle(BML_Mod mod);
        const BML_Mod_T *ResolveModHandle(BML_Mod mod) const;
        BML_Mod GetModHandleById(const std::string &id) const;
        BML_Mod GetModHandleByModule(HMODULE module) const;
        void AppendShutdownHook(BML_Mod mod, BML_ShutdownCallback callback, void *user_data);

        static void SetCurrentModule(BML_Mod mod);
        static BML_Mod GetCurrentModule();

        void SetRuntimeVersion(const BML_Version &version);
        const BML_Version &GetRuntimeVersion() const;

        BML_Result RetainHandle();
        BML_Result ReleaseHandle();
        uint32_t GetRetainCountForTest() const;

        static std::wstring SanitizeIdentifierForFilename(const std::string &value);

    private:
        Context();
        ~Context() = default;
        Context(const Context &) = delete;
        Context &operator=(const Context &) = delete;

        void ShutdownModulesLocked();
        BML_Mod_T *FindModHandleLocked(BML_Mod mod);
        const BML_Mod_T *FindModHandleLocked(BML_Mod mod) const;

        std::vector<std::unique_ptr<ModManifest>> m_Manifests;
        std::vector<LoadedModule> m_LoadedModules;
        std::unordered_map<std::string, BML_Mod> m_ModHandlesById;
        std::unordered_map<HMODULE, BML_Mod> m_ModHandlesByModule;
        BML_Version m_RuntimeVersion{};
        mutable std::mutex m_StateMutex;
        mutable std::mutex m_RetainMutex;
        std::condition_variable m_RetainCv;
        std::atomic<uint32_t> m_RetainCount{0};
        bool m_CleanupRequested{false};
        std::atomic<bool> m_Initialized{false};
    };
} // namespace BML::Core

#endif // BML_CORE_CONTEXT_H

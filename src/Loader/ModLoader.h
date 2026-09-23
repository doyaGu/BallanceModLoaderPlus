#ifndef BML_MODLOADER_H
#define BML_MODLOADER_H

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <type_traits>
#include <utility>
#include <vector>

#include "BML/IMod.h"
#include "Loader/ModInvocationGate.h"
#include "Loader/NativeModRegistry.h"
#include "UI/ImGuiStateRecovery.h"
#include "HookUtils.h"

class ModContext;
class BMLMod;
class NewBallTypeMod;

namespace BML {
class ScriptMod;
class ScriptDevToolsService;
class ScriptModHotReloadService;
struct ScriptModDefinition;
struct ScriptModLoadCandidate;
struct ScriptModReloadDiagnosticField;
}

class ModLoader final {
public:
    explicit ModLoader(ModContext &context);
    ~ModLoader();

    ModLoader(const ModLoader &) = delete;
    ModLoader &operator=(const ModLoader &) = delete;

    bool Start();
    void Stop();
    bool AreModsLoaded() const { return m_ModsLoaded; }
    bool AreModsInited() const { return m_ModsInited; }
    bool IsShuttingDown() const { return m_ShuttingDown; }

    int GetModCount();
    IMod *GetMod(int index);
    IMod *FindMod(const char *id) const;
    std::uint64_t GetModGeneration(const IMod *mod) const;
    std::uint64_t GetModRegistryRevision() const;

    int RegisterDependency(IMod *mod, const char *dependencyId, int major, int minor, int patch);
    int RegisterOptionalDependency(IMod *mod, const char *dependencyId, int major, int minor, int patch);
    int CheckDependencies(IMod *mod) const;
    int GetDependencyCount(IMod *mod) const;
    int GetDependencyInfo(IMod *mod, int index, char *dependencyId, int idSize,
                          int *major, int *minor, int *patch, int *optional) const;
    int ClearDependencies(IMod *mod);

    std::string GetNativeImcOwnerId(const void *callerAddress, const char *requestedOwnerId = nullptr) const;
    std::string GetNativeModOwnerId(const void *callerAddress, const char *requestedOwnerId = nullptr) const;
    bool NativeModOwnsAddress(const std::string &ownerId, const void *address) const;
    std::wstring GetModRootDirectory(const void *callerAddress, const char *modId) const;
    std::shared_ptr<void> FindNativeDll(const std::string &ownerId) const;
    BML::ModInvocationGate::CallLock LockModInvocation() const { return m_ModInvocationGate.LockCall(); }
    bool IsModInvocationActiveOnCurrentThread() const {
        return m_ModInvocationGate.IsCallActiveOnCurrentThread();
    }
    BML::ModInvocationGate &InvocationGate() { return m_ModInvocationGate; }

    BMLMod *GetBuiltinMod() const { return m_BMLMod; }
    NewBallTypeMod *GetBallTypeMod() const { return m_BallTypeMod; }

#if BML_ENABLE_ANGELSCRIPT
    bool ValidateScriptModReloadDependencies(const BML::ScriptMod *mod,
                                             const BML::ScriptModDefinition &candidate,
                                             std::string &diagnostic,
                                             std::vector<BML::ScriptModReloadDiagnosticField> *fields) const;
    bool PromoteFailedScriptModPlaceholder(BML::ScriptMod *mod, const std::string &oldId,
                                           const BML::ScriptModDefinition &candidate,
                                           std::string &diagnostic);
    void RestoreFailedScriptModPlaceholder(BML::ScriptMod *mod, const std::string &currentId,
                                           const BML::ScriptModDefinition &oldDefinition);
    BML::ScriptDevToolsService *GetScriptDevTools() const { return m_ScriptDevTools.get(); }
    void ProcessScriptState();
#endif

    template<typename T, typename... Args>
    std::enable_if_t<std::is_member_function_pointer<T>::value, void> BroadcastCallback(T callback, Args&&... args) {
        auto invocationLock = LockModInvocation();
        std::vector<IMod *> mods;
        {
            std::lock_guard<std::mutex> lock(m_StateMutex);
            auto it = m_CallbackMap.find(utils::TypeErase(callback));
            if (it != m_CallbackMap.end())
                mods = it->second;
        }
        for (IMod *mod : mods) {
            const Overlay::ImGuiStateSnapshot imguiState = Overlay::CaptureImGuiState();
            try {
                ModInvocation invocation(this, mod);
                (mod->*callback)(std::forward<Args>(args)...);
            } catch (const std::exception &e) {
                LogCallbackFailure(mod, e.what());
            } catch (...) {
                LogCallbackFailure(mod, nullptr);
            }
            if (Overlay::RecoverImGuiState(imguiState))
                LogImGuiRecovery(mod);
        }
    }

    template<typename T>
    std::enable_if_t<std::is_member_function_pointer<T>::value, void> BroadcastMessage(const char *msg, T callback) {
        LogMessage(msg);
        BroadcastCallback(callback);
    }

    void DispatchConfigChange(IMod *mod, const char *category, const char *key, IProperty *property);

private:
    class ModInvocation final {
    public:
        ModInvocation(const ModLoader *loader, IMod *mod) noexcept
            : m_Previous(s_Current), m_Loader(loader), m_Mod(mod) { s_Current = this; }
        ~ModInvocation() { s_Current = m_Previous; }
        ModInvocation(const ModInvocation &) = delete;
        ModInvocation &operator=(const ModInvocation &) = delete;
        static IMod *Current(const ModLoader *loader) noexcept {
            for (const ModInvocation *scope = s_Current; scope; scope = scope->m_Previous) {
                if (scope->m_Loader == loader)
                    return scope->m_Mod;
            }
            return nullptr;
        }
    private:
        inline static thread_local const ModInvocation *s_Current = nullptr;
        const ModInvocation *m_Previous = nullptr;
        const ModLoader *m_Loader = nullptr;
        IMod *m_Mod = nullptr;
    };

    bool LoadMods();
    void UnloadMods();
    bool InitMods();
    void ShutdownMods();
    void DeactivateActiveMods(bool dispatchPendingNotifications);
    void RollbackModActivation();
    void LogCallbackFailure(IMod *mod, const char *reason) const;
    void LogImGuiRecovery(IMod *mod) const;
    void LogMessage(const char *message) const;

    std::size_t ExploreMods(const std::wstring &path, std::vector<std::wstring> &mods);
#if BML_ENABLE_ANGELSCRIPT
    struct ModDependencySnapshot {
        std::string Id;
        BMLVersion MinVersion;
        bool Optional = false;
    };
    struct RegisteredModSnapshot {
        const IMod *Identity = nullptr;
        std::string Id;
        std::string Version;
        bool Failed = false;
        std::vector<ModDependencySnapshot> Dependencies;
    };
    std::vector<RegisteredModSnapshot> SnapshotModRegistry() const;
    std::size_t ExploreScriptMods(const std::wstring &path, std::vector<BML::ScriptModLoadCandidate> &candidates);
    IMod *LoadScriptMod(const BML::ScriptModLoadCandidate &candidate);
    void RegisterScriptModDependencies(IMod *mod, const BML::ScriptModDefinition &definition);
    void ProcessScriptModQueuedCallbacks();
    void ProcessScriptModFailureCleanup();
#endif
    std::shared_ptr<void> LoadLib(const wchar_t *path);
    bool UnloadLib(void *dllHandle);
    void DestroyNativeMod(void *dllHandle, IMod *mod, const char *modLabel) noexcept;
    IMod *LoadMod(const std::wstring &path);
    bool UnloadMod(const std::string &id);
    bool RegisterBuiltinMods();
    bool RegisterMod(IMod *mod, const std::shared_ptr<void> &dllHandle = nullptr);
    bool UnregisterMod(IMod *mod);
    IMod *FindModLocked(const std::string &id) const;
    int EvaluateDependencies(IMod *mod, std::string *diagnostic) const;
    int EvaluateActivationDependencies(IMod *mod, std::string *diagnostic) const;
    bool ResolveDependencies();
    void FillCallbackMap(IMod *mod);

    ModContext &m_Context;
    bool m_ModsLoaded = false;
    bool m_ModsInited = false;
    bool m_ShuttingDown = false;
    BMLMod *m_BMLMod = nullptr;
    NewBallTypeMod *m_BallTypeMod = nullptr;
#if BML_ENABLE_ANGELSCRIPT
    std::vector<std::unique_ptr<BML::ScriptMod>> m_ScriptMods;
    std::unique_ptr<BML::ScriptDevToolsService> m_ScriptDevTools;
    std::unique_ptr<BML::ScriptModHotReloadService> m_ScriptHotReload;
#endif
    NativeModRegistry m_NativeModRegistry;
    std::vector<IMod *> m_Mods;
    std::vector<IMod *> m_ActiveMods;
    std::unordered_map<std::string, std::size_t> m_ModIndex;
    std::unordered_map<const IMod *, std::uint64_t> m_ModGenerations;
    std::uint64_t m_NextModGeneration = 1;
    std::uint64_t m_ModRegistryRevision = 0;
    std::unordered_map<IMod *, std::vector<ModDependency>> m_ModDependencies;
    std::unordered_map<void *, std::vector<IMod *>> m_CallbackMap;

    // Hold the invocation gate before either state lock when a Mod or DLL
    // lifetime is involved. If both locks are needed, take state before
    // registry. Never invoke a Mod while holding either lock.
    mutable std::shared_mutex m_ModRegistryMutex;
    mutable BML::ModInvocationGate m_ModInvocationGate;
    mutable std::mutex m_StateMutex;
};

#endif // BML_MODLOADER_H

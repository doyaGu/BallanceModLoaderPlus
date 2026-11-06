#ifndef BML_MODLOADER_H
#define BML_MODLOADER_H

#include <atomic>
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
#include "Loader/NativeMod.h"
#include "Loader/NativeNotification.h"
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
    bool Stop();
    void Abandon() noexcept;
    bool AreModsLoaded() const { return m_ModsLoaded; }
    bool AreModsInited() const { return m_ModsInited; }
    bool IsShuttingDown() const { return m_ShuttingDown; }

    int GetModCount();
    IMod *GetMod(int index);
    IMod *FindMod(const char *id) const;
    std::uint64_t GetModGeneration(const IMod *mod) const;
    std::uint64_t GetModRegistryRevision() const;
    IMod *ResolveModContext(BML_ModContextHandle context) const;
    IMod *ResolveModHandle(BML_ModHandle handle) const;
    BML_ModContextHandle IssueModContext(IMod *mod);
    NativeMod *FindNativeMod(const IMod *mod) const;
    struct DependencyInfo { std::string Id; BMLVersion MinimumVersion; bool Optional = false; };
    std::vector<DependencyInfo> ReadDependencies(const IMod *) const;
    std::string InvokedOwnerId() const;
    bool CanDispatchOwner(const IMod *) const;
    void FailOwner(IMod *, int code, const char *operation);
    bool CanUseProvider(const IMod *consumer, const IMod *provider) const;
    bool HasLiveDependents(const IMod *provider) const;
    void ProcessFailedNativeMods();
    template<class Body> auto InvokeAs(IMod *mod, Body &&body) -> decltype(body()) {
        ModInvocation invocation(this, mod);
        return body();
    }


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
        std::shared_ptr<const std::vector<IMod *>> mods;
        {
            std::lock_guard<std::mutex> lock(m_StateMutex);
            auto it = m_CallbackMap.find(utils::TypeErase(callback));
            if (it != m_CallbackMap.end())
                mods = it->second;
        }
        if (!mods) return;
        NativeNotification nativeEvent(m_Context);
        nativeEvent.Prepare(callback, args...);
        for (IMod *mod : *mods) {
            if (!CanDispatchOwner(mod)) continue;
            const Overlay::ImGuiStateSnapshot imguiState = Overlay::CaptureImGuiState();
            try {
                ModInvocation invocation(this, mod);
                if (auto *native = FindNativeMod(mod)) {
                    if (!native->IsFailed()) {
                        const int code = nativeEvent.Dispatch(*native);
                        if (code != BML_OK) {
                            FailOwner(mod, code, nativeEvent.Name());
                            LogCallbackFailure(mod, native->Diagnostic().c_str());
                        }
                    }
                } else {
                    (mod->*callback)(std::forward<Args>(args)...);
                }
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
        ModInvocation(const ModLoader *loader, IMod *mod)
            : m_Call(loader->LockModInvocation()), m_Previous(s_Current), m_Loader(loader), m_Mod(mod) { s_Current = this; }
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
        BML::ModInvocationGate::CallLock m_Call;
        const ModInvocation *m_Previous = nullptr;
        const ModLoader *m_Loader = nullptr;
        IMod *m_Mod = nullptr;
    };

    bool LoadMods();
    void UnloadMods();
    void CleanupRejectedNativeEntry(const std::shared_ptr<void> &dllHandle);
    bool InitMods();
    void ShutdownMods();
    void DeactivateActiveMods(bool dispatchPendingNotifications);
    void RollbackModActivation();
    void LogCallbackFailure(IMod *mod, const char *reason) const;
    void LogImGuiRecovery(IMod *mod) const;
    void LogMessage(const char *message) const;

    using ModPackageDirectories = std::unordered_map<std::wstring, std::wstring>;
    std::size_t ExploreMods(const std::wstring &path, std::vector<std::wstring> &mods,
                            ModPackageDirectories &packages, std::size_t &nextPackage);
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
    std::size_t ExploreScriptMods(const std::wstring &path,
                                  std::vector<BML::ScriptModLoadCandidate> &candidates,
                                  const ModPackageDirectories &packages);
    IMod *LoadScriptMod(const BML::ScriptModLoadCandidate &candidate);
    void RegisterScriptModDependencies(IMod *mod, const BML::ScriptModDefinition &definition);
    void ProcessScriptModQueuedCallbacks();
    void ProcessScriptModFailureCleanup();
#endif
    std::shared_ptr<void> LoadLib(const wchar_t *path);
    bool UnloadLib(void *dllHandle);
    void DestroyNativeMod(const std::shared_ptr<void> &dllHandle, IMod *mod, const char *modLabel) noexcept;
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
    template<class T> static bool Subscribed(const NativeMod &mod, T callback) {
        const auto &c = mod.Callbacks();
        if constexpr (std::is_same_v<T, decltype(&IMessageReceiver::OnStartLevel)>) return c.OnGameEvent != nullptr;
        else if constexpr (std::is_same_v<T, decltype(&IMod::OnProcess)>) {
            if (callback == &IMod::OnLoad) return c.OnLoad != nullptr;
            if (callback == &IMod::OnUnload) return c.OnUnload != nullptr;
            return c.OnProcess != nullptr;
        }
        else if constexpr (std::is_same_v<T, decltype(&IMod::OnRender)>) return c.OnRender != nullptr;
        else if constexpr (std::is_same_v<T, decltype(&IMod::OnCheatEnabled)>) return c.OnCheatEnabled != nullptr;
        else if constexpr (std::is_same_v<T, decltype(&IMod::OnLoadObject)>) return c.OnLoadObject != nullptr;
        else if constexpr (std::is_same_v<T, decltype(&IMod::OnLoadScript)>) return c.OnLoadScript != nullptr;
        else if constexpr (std::is_same_v<T, decltype(&IMod::OnModifyConfig)>) return c.OnModifyConfig != nullptr;
        else if constexpr (std::is_same_v<T, decltype(&IMod::OnPhysicalize)>) return c.OnPhysicalize != nullptr;
        else if constexpr (std::is_same_v<T, decltype(&IMod::OnUnphysicalize)>) return c.OnUnphysicalize != nullptr;
        else if constexpr (std::is_same_v<T, decltype(&IMod::OnPreCommandExecute)>) return c.OnCommandEvent != nullptr;
        else return false;
    }


    ModContext &m_Context;
    std::atomic<bool> m_ModsLoaded{false};
    std::atomic<bool> m_ModsInited{false};
    std::atomic<bool> m_ShuttingDown{false};
    BMLMod *m_BMLMod = nullptr;
    NewBallTypeMod *m_BallTypeMod = nullptr;
#if BML_ENABLE_ANGELSCRIPT
    std::vector<std::unique_ptr<BML::ScriptMod>> m_ScriptMods;
    std::unique_ptr<BML::ScriptDevToolsService> m_ScriptDevTools;
    std::unique_ptr<BML::ScriptModHotReloadService> m_ScriptHotReload;
#endif
    std::unordered_map<IMod *, std::unique_ptr<NativeMod>> m_CNativeMods;
    std::unordered_map<BML_ModContextHandle, IMod *> m_ModContexts;
    NativeModRegistry m_NativeModRegistry;
    std::vector<std::shared_ptr<void>> m_RejectedNativeDlls;
    std::vector<IMod *> m_Mods;
    std::vector<IMod *> m_ActiveMods;
    std::vector<IMod *> m_UnloadedMods;
    std::unordered_map<std::string, std::size_t> m_ModIndex;
    std::unordered_map<const IMod *, std::uint64_t> m_ModGenerations;
    std::uint64_t m_NextModGeneration = 1;
    std::uint64_t m_ModRegistryRevision = 0;
    std::unordered_map<IMod *, std::vector<ModDependency>> m_ModDependencies;
    std::unordered_map<void *, std::shared_ptr<const std::vector<IMod *>>> m_CallbackMap;

    // Hold the invocation gate before either state lock when a Mod or DLL
    // lifetime is involved. If both locks are needed, take state before
    // registry. Never invoke a Mod while holding either lock.
    mutable std::shared_mutex m_ModRegistryMutex;
    mutable BML::ModInvocationGate m_ModInvocationGate;
    mutable std::mutex m_StateMutex;
};

#endif // BML_MODLOADER_H

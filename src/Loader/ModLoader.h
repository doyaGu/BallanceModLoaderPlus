#ifndef BML_MODLOADER_H
#define BML_MODLOADER_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <type_traits>
#include <utility>
#include <vector>

#include "Loader/ModRecord.h"
#include "Loader/ModInvocationGate.h"
#include "Loader/NativeModRegistry.h"
#include "Loader/NativeMod.h"
#include "Loader/NativeNotification.h"
#include "UI/ImGuiStateRecovery.h"

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
    ModRecord *GetMod(int index);
    ModRecord *FindMod(const char *id) const;
    std::uint64_t GetModGeneration(const ModRecord *mod) const;
    std::uint64_t GetModRegistryRevision() const;
    ModRecord *ResolveModContext(BML_ModContextHandle context) const;
    ModRecord *ResolveModHandle(BML_ModHandle handle) const;
    BML_ModContextHandle IssueModContext(ModRecord *mod);
    NativeMod *FindNativeMod(const ModRecord *mod) const;

    std::vector<ModDependency> ReadDependencies(const ModRecord *) const;
    std::optional<ModDependency> ReadDependency(const ModRecord *, int index) const;
    std::string InvokedOwnerId() const;
    bool CanDispatchOwner(const ModRecord *) const;
    void FailOwner(ModRecord *, int code, const char *operation);
    bool CanUseProvider(const ModRecord *consumer, const ModRecord *provider) const;
    bool HasLiveDependents(const ModRecord *provider) const;
    void ProcessFailedNativeMods();

    template <class Body> auto InvokeAs(ModRecord *mod, Body &&body) -> decltype(body()) {
        ModInvocation invocation(this, mod);
        return body();
    }

    int CheckDependencies(ModRecord *mod) const;
    int GetDependencyCount(ModRecord *mod) const;

    std::string GetNativeModOwnerId(const void *callerAddress, const char *requestedOwnerId = nullptr) const;
    bool NativeModOwnsAddress(const std::string &ownerId, const void *address) const;
    std::wstring GetModRootDirectory(const char *modId) const;
    std::shared_ptr<void> FindNativeDll(const std::string &ownerId) const;
    BML::ModInvocationGate::CallLock LockModInvocation() const { return m_ModInvocationGate.LockCall(); }
    bool IsModInvocationActiveOnCurrentThread() const {
        return m_ModInvocationGate.IsCallActiveOnCurrentThread();
    }
    BML::ModInvocationGate &InvocationGate() { return m_ModInvocationGate; }

    BMLMod *GetBuiltinMod() const { return m_BMLMod; }
    ModRecord *GetBuiltinRecord() const { return m_BuiltinRecord.get(); }
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

    template <ModEvent Event, typename... Args> void BroadcastCallback(Args &&...args) {
        auto invocationLock = LockModInvocation();
        std::shared_ptr<const std::vector<ModRecord *>> mods;
        {
            std::lock_guard<std::mutex> lock(m_StateMutex);
            auto it = m_CallbackMap.find(Event);
            if (it != m_CallbackMap.end())
                mods = it->second;
        }
        if (!mods) return;
        NativeNotification nativeEvent(m_Context);
        nativeEvent.Prepare(Event, args...);
        for (ModRecord *mod : *mods) {
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
                    mod->Notify<Event>(std::forward<Args>(args)...);
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

    template <ModEvent Event> void BroadcastMessage(const char *msg) {
        LogMessage(msg);
        BroadcastCallback<Event>();
    }

    void DispatchConfigChange(ModRecord *mod, const char *category, const char *key,
                              Property *property);

private:
    class ModInvocation final {
    public:
        ModInvocation(const ModLoader *loader, ModRecord *mod)
            : m_Call(loader->LockModInvocation()), m_Previous(s_Current), m_Loader(loader),
              m_Mod(mod) {
            s_Current = this;
        }
        ~ModInvocation() { s_Current = m_Previous; }
        ModInvocation(const ModInvocation &) = delete;
        ModInvocation &operator=(const ModInvocation &) = delete;

        static ModRecord *Current(const ModLoader *loader) noexcept {
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
        ModRecord *m_Mod = nullptr;
    };

    int RegisterDependency(ModRecord *mod, const ModDependency &dependency);
    int ClearDependencies(ModRecord *mod);
    bool LoadMods();
    void UnloadMods();
    bool InitMods();
    void ShutdownMods();
    void DeactivateActiveMods(bool dispatchPendingNotifications);
    void RollbackModActivation();
    void LogCallbackFailure(ModRecord *mod, const char *reason) const;
    void LogImGuiRecovery(ModRecord *mod) const;
    void LogMessage(const char *message) const;

    using ModPackageDirectories = std::unordered_map<std::wstring, std::wstring>;
    std::size_t ExploreMods(const std::wstring &path, std::vector<std::wstring> &mods,
                            ModPackageDirectories &packages, std::size_t &nextPackage);
#if BML_ENABLE_ANGELSCRIPT
    struct RegisteredModSnapshot {
        const ModRecord *Identity = nullptr;
        std::string Id;
        std::string Version;
        bool Failed = false;
        std::vector<ModDependency> Dependencies;
    };
    std::vector<RegisteredModSnapshot> SnapshotModRegistry() const;
    std::size_t ExploreScriptMods(const std::wstring &path,
                                  std::vector<BML::ScriptModLoadCandidate> &candidates,
                                  const ModPackageDirectories &packages);
    ModRecord *LoadScriptMod(const BML::ScriptModLoadCandidate &candidate);
    void RegisterScriptModDependencies(ModRecord *mod, const BML::ScriptModDefinition &definition);
    void ProcessScriptModQueuedCallbacks();
    void ProcessScriptModFailureCleanup();
#endif
    std::shared_ptr<void> LoadLib(const wchar_t *path);
    bool UnloadLib(void *dllHandle);
    void DestroyNativeMod(ModRecord *mod) noexcept;
    ModRecord *LoadMod(const std::wstring &path);
    bool UnloadMod(const std::string &id);
    bool RegisterBuiltinMods();
    bool RegisterMod(ModRecord *mod, const std::shared_ptr<void> &dllHandle = nullptr);
    bool UnregisterMod(ModRecord *mod);
    ModRecord *FindModLocked(const std::string &id) const;
    int EvaluateDependencies(ModRecord *mod, std::string *diagnostic) const;
    int EvaluateActivationDependencies(ModRecord *mod, std::string *diagnostic) const;
    bool ResolveDependencies();
    void FillCallbackMap(ModRecord *mod);
    static bool Subscribed(const NativeMod &mod, ModEvent event);

    ModContext &m_Context;
    std::atomic<bool> m_ModsLoaded{false};
    std::atomic<bool> m_ModsInited{false};
    std::atomic<bool> m_ShuttingDown{false};
    BMLMod *m_BMLMod = nullptr;
    NewBallTypeMod *m_BallTypeMod = nullptr;
    std::unique_ptr<ModRecord> m_BuiltinRecord;
    std::unique_ptr<ModRecord> m_BallTypeRecord;
#if BML_ENABLE_ANGELSCRIPT
    std::vector<std::unique_ptr<BML::ScriptMod>> m_ScriptMods;
    std::unique_ptr<BML::ScriptDevToolsService> m_ScriptDevTools;
    std::unique_ptr<BML::ScriptModHotReloadService> m_ScriptHotReload;
#endif
    std::unordered_map<ModRecord *, std::unique_ptr<NativeMod>> m_CNativeMods;
    std::unordered_map<BML_ModContextHandle, ModRecord *> m_ModContexts;
    NativeModRegistry m_NativeModRegistry;
    std::vector<ModRecord *> m_Mods;
    std::vector<ModRecord *> m_ActiveMods;
    std::vector<ModRecord *> m_UnloadedMods;
    std::unordered_map<std::string, std::size_t> m_ModIndex;
    std::unordered_map<const ModRecord *, std::uint64_t> m_ModGenerations;
    std::uint64_t m_NextModGeneration = 1;
    std::uint64_t m_ModRegistryRevision = 0;
    std::unordered_map<ModRecord *, std::vector<ModDependency>> m_ModDependencies;
    std::unordered_map<ModEvent, std::shared_ptr<const std::vector<ModRecord *>>> m_CallbackMap;

    // Hold the invocation gate before either state lock when a Mod or DLL
    // lifetime is involved. If both locks are needed, take state before
    // registry. Never invoke a Mod while holding either lock.
    mutable std::shared_mutex m_ModRegistryMutex;
    mutable BML::ModInvocationGate m_ModInvocationGate;
    mutable std::mutex m_StateMutex;
};

#endif // BML_MODLOADER_H

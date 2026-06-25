#ifndef BML_SCRIPTMODHOTRELOADSERVICE_H
#define BML_SCRIPTMODHOTRELOADSERVICE_H

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ScriptFileWatcherWin32.h"
#include "ScriptMod.h"

class ModContext;

namespace BML {

class ScriptLibraryReloadOperation;
class ScriptModReloadOperation;

class ScriptModHotReloadService {
public:
    explicit ScriptModHotReloadService(ModContext *context);
    ~ScriptModHotReloadService();

    void RegisterMod(ScriptMod *mod);
    void UnregisterMod(ScriptMod *mod);
    void Start();
    void Stop();
    void Process();

    bool QueueReload(const std::string &id, const ScriptModReloadOptions &options, std::string &message);
    bool QueueReloadLibrary(const std::string &id,
                            const std::string &version,
                            const ScriptModReloadOptions &options,
                            std::string &message);
    size_t QueueReloadAll(const ScriptModReloadOptions &options);
    bool SetAutomaticEnabled(bool enabled);
    bool SetWatchingEnabled(bool enabled) { return SetAutomaticEnabled(enabled); }
    bool IsAutomaticEnabled() const { return m_AutomaticEnabled; }
    bool IsWatchingEnabled() const { return m_AutomaticEnabled; }
    std::string GetStatus() const;

private:
    struct ModRecord {
        ScriptMod *Mod = nullptr;
        std::wstring WatchRoot;
        ScriptModReloadPolicy Policy = ScriptModReloadPolicy::Default;
    };

    struct PendingReload {
        ScriptModReloadOptions Options;
        std::chrono::steady_clock::time_point QueuedAt;
        std::chrono::steady_clock::time_point Due;
        std::chrono::steady_clock::time_point LastBlockedNotice;
        std::string Reason;
        size_t BlockedRetryCount = 0;
    };

    struct PendingLibraryReload {
        std::string Id;
        std::string Version;
        ScriptModReloadOptions Options;
        std::chrono::steady_clock::time_point QueuedAt;
        std::chrono::steady_clock::time_point Due;
        std::chrono::steady_clock::time_point LastBlockedNotice;
        std::string Reason;
        size_t BlockedRetryCount = 0;
    };

    struct WatchSpec {
        std::wstring Root;
        bool Recursive = true;
    };

    friend class ScriptLibraryReloadOperation;
    friend class ScriptModReloadOperation;

    ScriptMod *FindMod(const std::string &id) const;
    void RebuildWatches();
    void ClearAutomaticPendingReloads();
    void QueueReloadNow(ScriptMod *mod, const ScriptModReloadOptions &options, const std::string &reason);
    void QueueReloadDebounced(ScriptMod *mod, const ScriptModReloadOptions &options, const std::string &reason);
    void QueueLibraryReloadNow(const std::string &id,
                               const std::string &version,
                               const ScriptModReloadOptions &options,
                               const std::string &reason);
    void QueueLibraryReloadDebounced(const std::string &id,
                                     const std::string &version,
                                     const ScriptModReloadOptions &options,
                                     const std::string &reason);
    bool ShouldWatch(const ScriptMod *mod, ScriptModReloadPolicy *outPolicy = nullptr) const;
    std::wstring GetWatchRoot(const ScriptMod *mod) const;
    std::wstring GetModsRoot() const;
    std::wstring GetScriptLibsRoot() const;
    std::wstring MakeWatchKey(const std::wstring &root) const;
    void AddDesiredWatch(std::unordered_map<std::wstring, WatchSpec> &desired,
                         const std::wstring &root,
                         bool recursive) const;
    bool EventOverflowCanAffectMod(const ScriptFileWatcherWin32::Event &event, const ScriptMod *mod) const;
    bool EventLooksRelevant(const ScriptFileWatcherWin32::Event &event, const ScriptMod *mod) const;
    bool EventLooksRelevantToLibraryUse(const ScriptFileWatcherWin32::Event &event,
                                        const ScriptMod *mod,
                                        std::string *packageKey = nullptr) const;
    bool EventBelongsToKnownMod(const std::wstring &path, const ScriptMod *mod) const;
    bool ModUsesLibrary(const ScriptMod *mod, const std::string &id, const std::string &version) const;
    std::vector<ScriptMod *> GetLibraryConsumers(const std::string &id,
                                                 const std::string &version,
                                                 bool automaticOnly) const;
    size_t QueueLibraryConsumers(const std::string &id,
                                 const std::string &version,
                                 const ScriptModReloadOptions &options,
                                 const std::string &reason,
                                 bool debounced);
    bool ProcessLibraryReload(PendingLibraryReload &pending);
    void PublishReloadResult(const std::string &id,
                             const std::string &reason,
                             const ScriptModReloadOptions &options,
                             const ScriptModReloadResult &result);
    void PublishNewModRestartRequired(const ScriptFileWatcherWin32::Event &event);

    ModContext *m_Context = nullptr;
    bool m_Started = false;
    bool m_AutomaticEnabled = true;
    std::vector<ModRecord> m_Mods;
    std::unordered_map<std::string, PendingReload> m_Pending;
    std::unordered_map<std::string, PendingLibraryReload> m_PendingLibraries;
    std::unordered_map<std::wstring, WatchSpec> m_ActiveWatches;
    std::unordered_set<std::wstring> m_ReportedNewModRoots;
    ScriptFileWatcherWin32 m_Watcher;
    uint64_t m_LastWatcherDroppedEvents = 0;
};

} // namespace BML

#endif

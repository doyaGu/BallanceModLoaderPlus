#include "ScriptModHotReloadService.h"

#include <algorithm>
#include <cwctype>
#include <memory>
#include <sstream>

#include "Logger.h"
#include "ModContext.h"
#include "ScriptDevToolsService.h"
#include "ScriptModHotReloadPathFilter.h"
#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"

namespace BML {

namespace {
constexpr auto kDebounceDelay = std::chrono::milliseconds(500);
constexpr auto kRetryDelay = std::chrono::milliseconds(100);
constexpr auto kBlockedNoticeDelay = std::chrono::seconds(1);

bool SamePathInsensitive(const std::wstring &left, const std::wstring &right) {
    if (left.empty() || right.empty())
        return false;
    const std::wstring resolvedLeft = utils::ResolvePathW(left);
    const std::wstring resolvedRight = utils::ResolvePathW(right);
    return _wcsicmp(resolvedLeft.c_str(), resolvedRight.c_str()) == 0;
}

std::wstring ScriptEntryStem(const std::wstring &entryPath) {
    std::wstring fileName = utils::GetFileNameW(entryPath);
    constexpr wchar_t suffix[] = L".mod.as";
    constexpr size_t suffixLength = (sizeof(suffix) / sizeof(suffix[0])) - 1;
    if (fileName.size() > suffixLength &&
        _wcsicmp(fileName.c_str() + fileName.size() - suffixLength, suffix) == 0) {
        fileName.resize(fileName.size() - suffixLength);
    }
    return fileName;
}

std::string LibraryUseKey(const ScriptLibraryUse &library) {
    return library.Id + "@" + library.Version;
}

std::string LibraryPackageKey(const std::string &id, const std::string &version) {
    return id + "@" + version;
}

void SendReloadResultMessage(ModContext *context,
                             const std::string &id,
                             const ScriptModReloadOptions &options,
                             const ScriptModReloadResult &result) {
    if (!context)
        return;

    std::string message = "[script] ";
    if (options.DryRun && options.CheckStateHooks)
        message += "reload dry-run state-check ";
    else if (options.DryRun)
        message += "reload dry-run ";
    else
        message += "reload ";

    if (result.Success) {
        message += "ok: ";
        message += id;
    } else {
        message += "failed: ";
        message += id;
    }

    context->SendIngameMessage(message.c_str());
}

std::vector<ScriptDevEventField> BuildReloadResultFields(const std::string &reason,
                                                         const ScriptModReloadOptions &options,
                                                         const ScriptModReloadResult &result) {
    std::vector<ScriptDevEventField> fields = {
        {"reason", reason},
        {"automatic", options.Automatic ? "true" : "false"},
        {"dryRun", options.DryRun ? "true" : "false"},
        {"checkState", options.CheckStateHooks ? "true" : "false"},
        {"forceExports", options.ForceExports ? "true" : "false"},
    };
    for (const ScriptModReloadDiagnosticField &field : result.Fields)
        fields.push_back({field.Key, field.Value});
    return fields;
}

bool ReloadResultRolledBack(const ScriptModReloadResult &result) {
    for (const ScriptModReloadDiagnosticField &field : result.Fields) {
        if (field.Key == "rollback" && field.Value == "success")
            return true;
    }
    return false;
}

ScriptModReloadPolicy ResolvePolicy(const ScriptMod *mod) {
    if (!mod)
        return ScriptModReloadPolicy::Manual;
    ScriptModReloadPolicy policy = ParseScriptModReloadPolicy(mod->GetDefinition().ReloadPolicy);
    if (policy != ScriptModReloadPolicy::Default)
        return policy;
    return mod->GetEntry().SourceKind == ScriptModEntrySourceKind::ZipPackage
               ? ScriptModReloadPolicy::Manual
               : ScriptModReloadPolicy::Auto;
}
} // namespace

ScriptModHotReloadService::ScriptModHotReloadService(ModContext *context)
    : m_Context(context) {
}

ScriptModHotReloadService::~ScriptModHotReloadService() {
    Stop();
}

void ScriptModHotReloadService::RegisterMod(ScriptMod *mod) {
    if (!mod)
        return;
    const char *id = mod->GetID();
    if (!id || !*id)
        return;
    for (auto &record : m_Mods) {
        if (record.Mod && record.Mod->GetID() && std::string(record.Mod->GetID()) == id) {
            record.Mod = mod;
            record.Policy = ResolvePolicy(mod);
            record.WatchRoot = GetWatchRoot(mod);
            if (m_Started)
                RebuildWatches();
            return;
        }
    }
    ModRecord record;
    record.Mod = mod;
    record.Policy = ResolvePolicy(mod);
    record.WatchRoot = GetWatchRoot(mod);
    m_Mods.push_back(std::move(record));
    if (m_Started)
        RebuildWatches();
}

void ScriptModHotReloadService::UnregisterMod(ScriptMod *mod) {
    if (!mod)
        return;
    m_Mods.erase(std::remove_if(m_Mods.begin(), m_Mods.end(), [mod](const ModRecord &record) {
        return record.Mod == mod;
    }), m_Mods.end());
    if (const char *id = mod->GetID())
        m_Pending.erase(id);
    if (m_Started)
        RebuildWatches();
}

void ScriptModHotReloadService::Start() {
    if (m_Started)
        return;
    m_Started = true;
    RebuildWatches();
}

void ScriptModHotReloadService::Stop() {
    m_Started = false;
    m_Watcher.StopAll();
    m_ActiveWatches.clear();
    m_Pending.clear();
    m_PendingLibraries.clear();
}

void ScriptModHotReloadService::Process() {
    if (!m_Started)
        return;

    if (m_AutomaticEnabled) {
        const std::vector<ScriptFileWatcherWin32::Event> events = m_Watcher.DrainEvents();
        const uint64_t watcherDroppedEvents = m_Watcher.GetDroppedEventCount();
        if (watcherDroppedEvents != m_LastWatcherDroppedEvents) {
            if (m_Context && m_Context->GetScriptDevTools()) {
                m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Warn,
                                                             "ScriptWatchOverflow",
                                                             "",
                                                             "watch",
                                                             "",
                                                             "Script file watcher dropped events; all auto reload mods will be treated as changed.",
                                                             {{"dropped", std::to_string(watcherDroppedEvents)}});
            }
            m_LastWatcherDroppedEvents = watcherDroppedEvents;
        }
        ScriptModReloadOptions automaticOptions;
        automaticOptions.Automatic = true;
        for (const auto &event : events) {
            PublishNewModRestartRequired(event);
            std::unordered_set<std::string> queuedLibraryMods;
            std::unordered_set<std::string> queuedLibraryPackages;
            for (const auto &record : m_Mods) {
                if (!record.Mod || record.Policy != ScriptModReloadPolicy::Auto)
                    continue;
                std::string libraryPackage;
                if (EventLooksRelevantToLibraryUse(event, record.Mod, &libraryPackage)) {
                    if (!libraryPackage.empty() && queuedLibraryPackages.insert(libraryPackage).second) {
                        const size_t separator = libraryPackage.rfind('@');
                        if (separator != std::string::npos && separator > 0 && separator + 1 < libraryPackage.size()) {
                            QueueLibraryReloadDebounced(libraryPackage.substr(0, separator),
                                                        libraryPackage.substr(separator + 1),
                                                        automaticOptions,
                                                        "library changed " + libraryPackage);
                        }
                    } else {
                        const char *modId = record.Mod->GetID();
                        if (modId && queuedLibraryMods.insert(modId).second)
                            QueueReloadDebounced(record.Mod, automaticOptions, "library changed");
                    }
                    continue;
                }
                if (!EventLooksRelevant(event, record.Mod))
                    continue;
                QueueReloadDebounced(record.Mod, automaticOptions, "file changed");
            }
        }
    }

    const auto now = std::chrono::steady_clock::now();
    std::vector<std::string> readyLibraries;
    for (const auto &entry : m_PendingLibraries) {
        if (entry.second.Due <= now)
            readyLibraries.push_back(entry.first);
    }
    for (const std::string &key : readyLibraries) {
        auto pendingIt = m_PendingLibraries.find(key);
        if (pendingIt == m_PendingLibraries.end())
            continue;
        PendingLibraryReload pending = pendingIt->second;
        m_PendingLibraries.erase(pendingIt);
        if (pending.Options.Automatic && !m_AutomaticEnabled)
            continue;
        if (!ProcessLibraryReload(pending)) {
            pending.Due = std::chrono::steady_clock::now() + kRetryDelay;
            m_PendingLibraries[key] = std::move(pending);
        }
    }

    std::vector<std::string> ready;
    for (const auto &entry : m_Pending) {
        if (entry.second.Due <= now)
            ready.push_back(entry.first);
    }

    for (const std::string &id : ready) {
        auto pendingIt = m_Pending.find(id);
        if (pendingIt == m_Pending.end())
            continue;
        PendingReload pending = pendingIt->second;
        m_Pending.erase(pendingIt);
        if (pending.Options.Automatic && !m_AutomaticEnabled) {
            continue;
        }
        ScriptMod *mod = FindMod(id);
        if (!mod) {
            continue;
        }
        if (!mod->CanHotReloadNow()) {
            ++pending.BlockedRetryCount;
            const bool firstNotice = pending.LastBlockedNotice.time_since_epoch().count() == 0;
            if (firstNotice || now - pending.LastBlockedNotice >= kBlockedNoticeDelay) {
                pending.LastBlockedNotice = now;
                if (m_Context && m_Context->GetScriptDevTools()) {
                    const auto waitMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - pending.QueuedAt).count();
                    m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Warn,
                                                                 "ScriptReloadBlocked",
                                                                 id,
                                                                 "reload",
                                                                 "",
                                                                 "Script reload is waiting for active script calls to finish.",
                                                                 {{"reason", pending.Reason},
                                                                  {"automatic", pending.Options.Automatic ? "true" : "false"},
                                                                  {"dryRun", pending.Options.DryRun ? "true" : "false"},
                                                                  {"checkState", pending.Options.CheckStateHooks ? "true" : "false"},
                                                                  {"forceExports", pending.Options.ForceExports ? "true" : "false"},
                                                                  {"activeCalls", std::to_string(mod->GetActiveScriptCallCount())},
                                                                  {"queuedServiceCallbacks", std::to_string(mod->GetQueuedScriptServiceCallbackCount())},
                                                                  {"retries", std::to_string(pending.BlockedRetryCount)},
                                                                  {"waitMs", std::to_string(waitMs)}});
                }
            }
            pending.Due = now + kRetryDelay;
            m_Pending[id] = pending;
            continue;
        }

        if (m_Context && m_Context->GetScriptDevTools()) {
            m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                         pending.Options.DryRun ? "ScriptReloadDryRunStarted" : "ScriptReloadStarted",
                                                         id,
                                                         "reload",
                                                         "",
                                                         pending.Options.DryRun ? "Script reload dry-run started." : "Script reload started.",
                                                         {{"reason", pending.Reason},
                                                          {"automatic", pending.Options.Automatic ? "true" : "false"},
                                                          {"dryRun", pending.Options.DryRun ? "true" : "false"},
                                                          {"checkState", pending.Options.CheckStateHooks ? "true" : "false"},
                                                          {"forceExports", pending.Options.ForceExports ? "true" : "false"}});
        }
        ScriptModReloadResult result = pending.Options.DryRun
                                           ? mod->TryHotReloadDryRun(pending.Options)
                                           : mod->TryHotReload(pending.Options);
        if (result.RetryLater) {
            ++pending.BlockedRetryCount;
            if (pending.LastBlockedNotice.time_since_epoch().count() == 0 ||
                now - pending.LastBlockedNotice >= kBlockedNoticeDelay) {
                pending.LastBlockedNotice = now;
                if (m_Context && m_Context->GetScriptDevTools()) {
                    const auto waitMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - pending.QueuedAt).count();
                    m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Warn,
                                                                 "ScriptReloadBlocked",
                                                                 id,
                                                                 "reload",
                                                                 "",
                                                                 result.Diagnostic.empty()
                                                                     ? "Script reload asked to retry later."
                                                                     : result.Diagnostic,
                                                                 {{"reason", pending.Reason},
                                                                  {"automatic", pending.Options.Automatic ? "true" : "false"},
                                                                  {"dryRun", pending.Options.DryRun ? "true" : "false"},
                                                                  {"checkState", pending.Options.CheckStateHooks ? "true" : "false"},
                                                                  {"forceExports", pending.Options.ForceExports ? "true" : "false"},
                                                                  {"activeCalls", std::to_string(mod->GetActiveScriptCallCount())},
                                                                  {"queuedServiceCallbacks", std::to_string(mod->GetQueuedScriptServiceCallbackCount())},
                                                                  {"retries", std::to_string(pending.BlockedRetryCount)},
                                                                  {"waitMs", std::to_string(waitMs)}});
                }
            }
            pending.Due = now + kRetryDelay;
            m_Pending[id] = pending;
            continue;
        }

        const char *currentId = mod->GetID();
        const std::string resultId = currentId && *currentId ? currentId : id;
        PublishReloadResult(resultId, pending.Reason, pending.Options, result);
        if (result.Success && !pending.Options.DryRun)
            RegisterMod(mod);
    }
}

bool ScriptModHotReloadService::QueueReload(const std::string &id,
                                            const ScriptModReloadOptions &options,
                                            std::string &message) {
    ScriptMod *mod = FindMod(id);
    if (!mod) {
        message = "Script mod not found: " + id;
        return false;
    }
    QueueReloadNow(mod, options, "manual");
    message = options.DryRun ? "Queued script reload dry-run: " + id : "Queued script reload: " + id;
    return true;
}

bool ScriptModHotReloadService::QueueReloadLibrary(const std::string &id,
                                                   const std::string &version,
                                                   const ScriptModReloadOptions &options,
                                                   std::string &message) {
    if (id.empty() || version.empty()) {
        message = "Usage: script reload-lib <id> <version> [--dry-run] [--force-exports]";
        return false;
    }
    const size_t count = GetLibraryConsumers(id, version, false).size();
    if (count == 0) {
        message = "No active script mods use library " + LibraryPackageKey(id, version) + ".";
        return false;
    }
    QueueLibraryReloadNow(id,
                          version,
                          options,
                          "manual library reload " + LibraryPackageKey(id, version));
    message = options.DryRun
                  ? "Queued script library reload dry-run for " + std::to_string(count) + " consumer(s)."
                  : "Queued script library reload for " + std::to_string(count) + " consumer(s).";
    return true;
}

size_t ScriptModHotReloadService::QueueReloadAll(const ScriptModReloadOptions &options) {
    size_t count = 0;
    for (const auto &record : m_Mods) {
        if (!record.Mod)
            continue;
        QueueReloadNow(record.Mod, options, "manual-all");
        ++count;
    }
    return count;
}

bool ScriptModHotReloadService::SetAutomaticEnabled(bool enabled) {
    if (m_AutomaticEnabled == enabled)
        return true;
    m_AutomaticEnabled = enabled;
    if (!m_AutomaticEnabled)
        ClearAutomaticPendingReloads();
    RebuildWatches();
    return true;
}

std::string ScriptModHotReloadService::GetStatus() const {
    size_t autoCount = 0;
    std::unordered_set<std::string> activeLibraryPackages;
    for (const auto &record : m_Mods) {
        if (record.Policy == ScriptModReloadPolicy::Auto)
            ++autoCount;
        if (record.Mod) {
            for (const ScriptLibraryUse &library : record.Mod->GetDefinition().SourceLibraries)
                activeLibraryPackages.insert(LibraryUseKey(library));
        }
    }
    size_t blockedCount = 0;
    for (const auto &entry : m_Pending) {
        ScriptMod *mod = FindMod(entry.first);
        if (mod && !mod->CanHotReloadNow())
            ++blockedCount;
    }
    std::ostringstream stream;
    stream << "script hot reload: mods=" << m_Mods.size()
           << " auto=" << autoCount
           << " pending=" << m_Pending.size()
           << " pendingLibs=" << m_PendingLibraries.size()
           << " blocked=" << blockedCount
           << " sourceLibs=" << activeLibraryPackages.size()
           << " automatic=" << (m_AutomaticEnabled ? "on" : "off")
           << " watches=" << m_ActiveWatches.size()
           << " watcherDropped=" << m_Watcher.GetDroppedEventCount();
    return stream.str();
}

ScriptMod *ScriptModHotReloadService::FindMod(const std::string &id) const {
    for (const auto &record : m_Mods) {
        if (!record.Mod || !record.Mod->GetID())
            continue;
        if (id == record.Mod->GetID())
            return record.Mod;
    }
    return nullptr;
}

void ScriptModHotReloadService::RebuildWatches() {
    if (!m_Started || !m_AutomaticEnabled) {
        m_Watcher.StopAll();
        m_ActiveWatches.clear();
        return;
    }

    std::unordered_map<std::wstring, WatchSpec> desired;
    AddDesiredWatch(desired, GetModsRoot(), false);
    const std::wstring scriptLibsRoot = GetScriptLibsRoot();
    if (!scriptLibsRoot.empty() && utils::DirectoryExistsW(scriptLibsRoot))
        AddDesiredWatch(desired, scriptLibsRoot, true);
    for (auto &record : m_Mods) {
        if (!record.Mod)
            continue;
        record.Policy = ResolvePolicy(record.Mod);
        record.WatchRoot = GetWatchRoot(record.Mod);
        if (record.Policy != ScriptModReloadPolicy::Auto)
            continue;

        const ScriptModEntry &entry = record.Mod->GetEntry();
        if (entry.SourceKind == ScriptModEntrySourceKind::ZipPackage) {
            AddDesiredWatch(desired, utils::GetDirectoryW(entry.SourcePath), false);
        } else if (entry.SourceKind == ScriptModEntrySourceKind::SingleFile) {
            AddDesiredWatch(desired, utils::GetDirectoryW(entry.EntryPath), false);
            AddDesiredWatch(desired, entry.ResourceRootDirectory, true);
        } else {
            AddDesiredWatch(desired, entry.RootDirectory, true);
        }
    }

    for (auto it = m_ActiveWatches.begin(); it != m_ActiveWatches.end();) {
        const auto desiredIt = desired.find(it->first);
        if (desiredIt == desired.end() ||
            desiredIt->second.Recursive != it->second.Recursive) {
            m_Watcher.Unwatch(it->second.Root);
            it = m_ActiveWatches.erase(it);
            continue;
        }
        ++it;
    }

    for (const auto &entry : desired) {
        const auto activeIt = m_ActiveWatches.find(entry.first);
        if (activeIt != m_ActiveWatches.end())
            continue;
        if (m_Watcher.Watch(entry.second.Root, entry.second.Recursive)) {
            m_ActiveWatches.emplace(entry.first, entry.second);
        } else if (m_Context && m_Context->GetScriptDevTools()) {
            m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Warn,
                                                         "ScriptWatchFailed",
                                                         "",
                                                         "watch",
                                                         utils::Utf16ToUtf8(entry.second.Root),
                                                         "Failed to watch script mod directory.",
                                                         {{"recursive", entry.second.Recursive ? "true" : "false"}});
        }
    }
}

void ScriptModHotReloadService::ClearAutomaticPendingReloads() {
    for (auto it = m_Pending.begin(); it != m_Pending.end();) {
        if (it->second.Options.Automatic)
            it = m_Pending.erase(it);
        else
            ++it;
    }
    for (auto it = m_PendingLibraries.begin(); it != m_PendingLibraries.end();) {
        if (it->second.Options.Automatic)
            it = m_PendingLibraries.erase(it);
        else
            ++it;
    }
}

void ScriptModHotReloadService::QueueReloadNow(ScriptMod *mod,
                                               const ScriptModReloadOptions &options,
                                               const std::string &reason) {
    if (!mod || !mod->GetID())
        return;
    const std::string id = mod->GetID();
    auto existingIt = m_Pending.find(id);
    const bool replaced = existingIt != m_Pending.end();
    PendingReload previous;
    if (replaced)
        previous = existingIt->second;

    PendingReload &pending = m_Pending[id];
    pending.Options = options;
    pending.Due = std::chrono::steady_clock::now();
    pending.QueuedAt = pending.Due;
    pending.LastBlockedNotice = {};
    pending.BlockedRetryCount = 0;
    pending.Reason = reason;
    if (m_Context && m_Context->GetScriptDevTools()) {
        if (replaced) {
            m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                         "ScriptReloadPendingReplaced",
                                                         id,
                                                         "reload",
                                                         "",
                                                         "Pending script reload was replaced by a newer request.",
                                                         {{"previousReason", previous.Reason},
                                                          {"previousAutomatic", previous.Options.Automatic ? "true" : "false"},
                                                          {"previousDryRun", previous.Options.DryRun ? "true" : "false"},
                                                          {"previousCheckState", previous.Options.CheckStateHooks ? "true" : "false"},
                                                          {"previousForceExports", previous.Options.ForceExports ? "true" : "false"},
                                                          {"reason", reason},
                                                          {"automatic", options.Automatic ? "true" : "false"},
                                                          {"dryRun", options.DryRun ? "true" : "false"},
                                                          {"checkState", options.CheckStateHooks ? "true" : "false"},
                                                          {"forceExports", options.ForceExports ? "true" : "false"}});
        }
        const bool dryRun = options.DryRun;
        m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                     dryRun ? "ScriptReloadDryRunQueued" : "ScriptReloadQueued",
                                                     id,
                                                     "reload",
                                                     "",
                                                     dryRun ? "Script reload dry-run queued." : "Script reload queued.",
                                                     {{"reason", reason},
                                                      {"automatic", options.Automatic ? "true" : "false"},
                                                      {"dryRun", dryRun ? "true" : "false"},
                                                      {"checkState", options.CheckStateHooks ? "true" : "false"},
                                                      {"forceExports", options.ForceExports ? "true" : "false"}});
    }
}

void ScriptModHotReloadService::QueueReloadDebounced(ScriptMod *mod,
                                                     const ScriptModReloadOptions &options,
                                                     const std::string &reason) {
    if (!mod || !mod->GetID())
        return;
    if (options.Automatic && !m_AutomaticEnabled)
        return;
    const std::string id = mod->GetID();
    auto existingIt = m_Pending.find(id);
    if (options.Automatic &&
        existingIt != m_Pending.end() &&
        !existingIt->second.Options.Automatic) {
        if (m_Context && m_Context->GetScriptDevTools()) {
            m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                         "ScriptReloadAutoCoalesced",
                                                         id,
                                                         "reload",
                                                         "",
                                                         "Automatic script reload was coalesced into a pending manual reload.",
                                                         {{"reason", reason},
                                                          {"pendingReason", existingIt->second.Reason},
                                                          {"pendingDryRun", existingIt->second.Options.DryRun ? "true" : "false"},
                                                          {"pendingCheckState", existingIt->second.Options.CheckStateHooks ? "true" : "false"},
                                                          {"pendingForceExports", existingIt->second.Options.ForceExports ? "true" : "false"}});
        }
        return;
    }
    PendingReload &pending = m_Pending[id];
    pending.Options = options;
    pending.QueuedAt = std::chrono::steady_clock::now();
    pending.Due = pending.QueuedAt + kDebounceDelay;
    pending.LastBlockedNotice = {};
    pending.BlockedRetryCount = 0;
    pending.Reason = reason;
    if (m_Context && m_Context->GetScriptDevTools()) {
        const bool dryRun = options.DryRun;
        m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                     dryRun ? "ScriptReloadDryRunQueued" : "ScriptReloadQueued",
                                                     id,
                                                     "reload",
                                                     "",
                                                     dryRun ? "Script reload dry-run queued after debounce."
                                                            : "Script reload queued after debounce.",
                                                     {{"reason", reason},
                                                      {"automatic", options.Automatic ? "true" : "false"},
                                                      {"dryRun", dryRun ? "true" : "false"},
                                                      {"checkState", options.CheckStateHooks ? "true" : "false"},
                                                      {"forceExports", options.ForceExports ? "true" : "false"}});
    }
}

void ScriptModHotReloadService::QueueLibraryReloadNow(const std::string &id,
                                                      const std::string &version,
                                                      const ScriptModReloadOptions &options,
                                                      const std::string &reason) {
    if (id.empty() || version.empty())
        return;
    const std::string key = LibraryPackageKey(id, version);
    auto existingIt = m_PendingLibraries.find(key);
    const bool replaced = existingIt != m_PendingLibraries.end();
    PendingLibraryReload previous;
    if (replaced)
        previous = existingIt->second;

    PendingLibraryReload &pending = m_PendingLibraries[key];
    pending.Id = id;
    pending.Version = version;
    pending.Options = options;
    pending.Due = std::chrono::steady_clock::now();
    pending.QueuedAt = pending.Due;
    pending.LastBlockedNotice = {};
    pending.BlockedRetryCount = 0;
    pending.Reason = reason;

    if (m_Context && m_Context->GetScriptDevTools()) {
        if (replaced) {
            m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                         "ScriptLibraryReloadPendingReplaced",
                                                         key,
                                                         "reload",
                                                         "",
                                                         "Pending script library reload was replaced by a newer request.",
                                                         {{"previousReason", previous.Reason},
                                                          {"previousAutomatic", previous.Options.Automatic ? "true" : "false"},
                                                          {"previousDryRun", previous.Options.DryRun ? "true" : "false"},
                                                          {"reason", reason},
                                                          {"automatic", options.Automatic ? "true" : "false"},
                                                          {"dryRun", options.DryRun ? "true" : "false"}});
        }
        m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                     options.DryRun ? "ScriptLibraryReloadDryRunQueued" : "ScriptLibraryReloadQueued",
                                                     key,
                                                     "reload",
                                                     "",
                                                     options.DryRun ? "Script library reload dry-run queued."
                                                                    : "Script library reload queued.",
                                                     {{"reason", reason},
                                                      {"automatic", options.Automatic ? "true" : "false"},
                                                      {"dryRun", options.DryRun ? "true" : "false"},
                                                      {"forceExports", options.ForceExports ? "true" : "false"}});
    }
}

void ScriptModHotReloadService::QueueLibraryReloadDebounced(const std::string &id,
                                                            const std::string &version,
                                                            const ScriptModReloadOptions &options,
                                                            const std::string &reason) {
    if (id.empty() || version.empty())
        return;
    if (options.Automatic && !m_AutomaticEnabled)
        return;
    const std::string key = LibraryPackageKey(id, version);
    auto existingIt = m_PendingLibraries.find(key);
    if (options.Automatic &&
        existingIt != m_PendingLibraries.end() &&
        !existingIt->second.Options.Automatic) {
        if (m_Context && m_Context->GetScriptDevTools()) {
            m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                         "ScriptLibraryReloadAutoCoalesced",
                                                         key,
                                                         "reload",
                                                         "",
                                                         "Automatic script library reload was coalesced into a pending manual reload.",
                                                         {{"reason", reason},
                                                          {"pendingReason", existingIt->second.Reason},
                                                          {"pendingDryRun", existingIt->second.Options.DryRun ? "true" : "false"},
                                                          {"pendingForceExports", existingIt->second.Options.ForceExports ? "true" : "false"}});
        }
        return;
    }

    PendingLibraryReload &pending = m_PendingLibraries[key];
    pending.Id = id;
    pending.Version = version;
    pending.Options = options;
    pending.QueuedAt = std::chrono::steady_clock::now();
    pending.Due = pending.QueuedAt + kDebounceDelay;
    pending.LastBlockedNotice = {};
    pending.BlockedRetryCount = 0;
    pending.Reason = reason;

    if (m_Context && m_Context->GetScriptDevTools()) {
        m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                     options.DryRun ? "ScriptLibraryReloadDryRunQueued" : "ScriptLibraryReloadQueued",
                                                     key,
                                                     "reload",
                                                     "",
                                                     options.DryRun ? "Script library reload dry-run queued after debounce."
                                                                    : "Script library reload queued after debounce.",
                                                     {{"reason", reason},
                                                      {"automatic", options.Automatic ? "true" : "false"},
                                                      {"dryRun", options.DryRun ? "true" : "false"},
                                                      {"forceExports", options.ForceExports ? "true" : "false"}});
    }
}

bool ScriptModHotReloadService::ShouldWatch(const ScriptMod *mod, ScriptModReloadPolicy *outPolicy) const {
    const ScriptModReloadPolicy policy = ResolvePolicy(mod);
    if (outPolicy)
        *outPolicy = policy;
    return policy == ScriptModReloadPolicy::Auto;
}

std::wstring ScriptModHotReloadService::GetWatchRoot(const ScriptMod *mod) const {
    if (!mod)
        return {};
    const ScriptModEntry &entry = mod->GetEntry();
    if (entry.SourceKind == ScriptModEntrySourceKind::SingleFile)
        return utils::GetDirectoryW(entry.EntryPath);
    if (entry.SourceKind == ScriptModEntrySourceKind::ZipPackage)
        return utils::GetDirectoryW(entry.SourcePath);
    return entry.RootDirectory;
}

std::wstring ScriptModHotReloadService::GetModsRoot() const {
    if (!m_Context)
        return {};
    const wchar_t *loaderDir = m_Context->GetDirectory(BML_DIR_LOADER);
    if (!loaderDir || !*loaderDir)
        return {};
    return utils::CombinePathW(loaderDir, L"Mods");
}

std::wstring ScriptModHotReloadService::GetScriptLibsRoot() const {
    if (!m_Context)
        return {};
    const wchar_t *loaderDir = m_Context->GetDirectory(BML_DIR_LOADER);
    if (!loaderDir || !*loaderDir)
        return {};
    return utils::CombinePathW(loaderDir, L"ScriptLibs");
}

std::wstring ScriptModHotReloadService::MakeWatchKey(const std::wstring &root) const {
    std::wstring key = utils::ResolvePathW(root);
    if (key.empty())
        key = root;
    std::transform(key.begin(), key.end(), key.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return key;
}

void ScriptModHotReloadService::AddDesiredWatch(std::unordered_map<std::wstring, WatchSpec> &desired,
                                                const std::wstring &root,
                                                bool recursive) const {
    if (root.empty())
        return;
    const std::wstring key = MakeWatchKey(root);
    if (key.empty())
        return;
    auto it = desired.find(key);
    if (it == desired.end()) {
        desired.emplace(key, WatchSpec{root, recursive});
        return;
    }
    if (recursive && !it->second.Recursive)
        it->second.Recursive = true;
}

bool ScriptModHotReloadService::EventOverflowCanAffectMod(const ScriptFileWatcherWin32::Event &event,
                                                          const ScriptMod *mod) const {
    if (!mod)
        return false;
    return ScriptHotReloadOverflowCanAffectEntry(event, mod->GetEntry());
}

bool ScriptModHotReloadService::EventLooksRelevant(const ScriptFileWatcherWin32::Event &event,
                                                   const ScriptMod *mod) const {
    if (!mod)
        return false;
    return ScriptHotReloadEventLooksRelevant(event, mod->GetEntry());
}

bool ScriptModHotReloadService::EventLooksRelevantToLibraryUse(const ScriptFileWatcherWin32::Event &event,
                                                               const ScriptMod *mod,
                                                               std::string *packageKey) const {
    if (packageKey)
        packageKey->clear();
    if (!mod)
        return false;
    for (const ScriptLibraryUse &library : mod->GetDefinition().SourceLibraries) {
        if (!ScriptHotReloadEventLooksRelevantToLibraryUse(event, library))
            continue;
        if (packageKey)
            *packageKey = LibraryUseKey(library);
        return true;
    }
    return false;
}

bool ScriptModHotReloadService::EventBelongsToKnownMod(const std::wstring &path, const ScriptMod *mod) const {
    if (!mod || path.empty())
        return false;
    return ScriptHotReloadEventBelongsToEntry(path, mod->GetEntry());
}

bool ScriptModHotReloadService::ModUsesLibrary(const ScriptMod *mod,
                                               const std::string &id,
                                               const std::string &version) const {
    if (!mod)
        return false;
    for (const ScriptLibraryUse &library : mod->GetDefinition().SourceLibraries) {
        if (library.Id == id && library.Version == version)
            return true;
    }
    return false;
}

std::vector<ScriptMod *> ScriptModHotReloadService::GetLibraryConsumers(const std::string &id,
                                                                        const std::string &version,
                                                                        bool automaticOnly) const {
    std::vector<ScriptMod *> consumers;
    for (const auto &record : m_Mods) {
        if (!record.Mod || !ModUsesLibrary(record.Mod, id, version))
            continue;
        if (automaticOnly && record.Policy != ScriptModReloadPolicy::Auto)
            continue;
        consumers.push_back(record.Mod);
    }
    std::sort(consumers.begin(), consumers.end(), [](ScriptMod *left, ScriptMod *right) {
        const char *leftId = left ? left->GetID() : nullptr;
        const char *rightId = right ? right->GetID() : nullptr;
        return std::string(leftId ? leftId : "") < std::string(rightId ? rightId : "");
    });
    return consumers;
}

size_t ScriptModHotReloadService::QueueLibraryConsumers(const std::string &id,
                                                        const std::string &version,
                                                        const ScriptModReloadOptions &options,
                                                        const std::string &reason,
                                                        bool debounced) {
    size_t count = 0;
    for (const auto &record : m_Mods) {
        if (!record.Mod || !ModUsesLibrary(record.Mod, id, version))
            continue;
        if (record.Policy != ScriptModReloadPolicy::Auto && options.Automatic)
            continue;
        if (debounced)
            QueueReloadDebounced(record.Mod, options, reason);
        else
            QueueReloadNow(record.Mod, options, reason);
        ++count;
    }
    return count;
}

void ScriptModHotReloadService::PublishReloadResult(const std::string &id,
                                                    const std::string &reason,
                                                    const ScriptModReloadOptions &options,
                                                    const ScriptModReloadResult &result) {
    if (m_Context && m_Context->GetLogger()) {
        if (result.Success) {
            if (options.DryRun)
                m_Context->GetLogger()->Info("Script mod %s hot reload dry-run passed.", id.c_str());
            else
                m_Context->GetLogger()->Info("Script mod %s hot reload succeeded.", id.c_str());
        } else {
            if (options.DryRun) {
                m_Context->GetLogger()->Warn("Script mod %s hot reload dry-run failed: %s",
                                             id.c_str(),
                                             result.Diagnostic.c_str());
            } else {
                m_Context->GetLogger()->Warn("Script mod %s hot reload failed: %s",
                                             id.c_str(),
                                             result.Diagnostic.c_str());
            }
        }
    }
    SendReloadResultMessage(m_Context, id, options, result);
    if (m_Context && m_Context->GetScriptDevTools()) {
        const bool rolledBack = !result.Success && ReloadResultRolledBack(result);
        const char *code = "ScriptReloadRejected";
        if (options.DryRun)
            code = result.Success ? "ScriptReloadDryRunPassed" : "ScriptReloadDryRunFailed";
        else if (result.Success)
            code = "ScriptReloadCommitted";
        else if (rolledBack)
            code = "ScriptReloadRolledBack";
        std::vector<ScriptDevEventField> fields = BuildReloadResultFields(reason, options, result);
        const std::string successMessage = !result.Diagnostic.empty()
                                               ? result.Diagnostic
                                               : (options.DryRun ? "Script reload dry-run passed." : "Script reload committed.");
        m_Context->GetScriptDevTools()->PublishEvent(result.Success ? ScriptDevEventSeverity::Info : ScriptDevEventSeverity::Warn,
                                                     code,
                                                     id,
                                                     "reload",
                                                     result.SourcePath,
                                                     result.Success ? successMessage : result.Diagnostic,
                                                     fields,
                                                     result.ReloadAttemptId);
    }
}

bool ScriptModHotReloadService::ProcessLibraryReload(PendingLibraryReload &pending) {
    const std::string package = LibraryPackageKey(pending.Id, pending.Version);
    const auto now = std::chrono::steady_clock::now();
    std::vector<ScriptMod *> consumers = GetLibraryConsumers(pending.Id,
                                                             pending.Version,
                                                             pending.Options.Automatic);
    if (consumers.empty()) {
        if (m_Context && m_Context->GetScriptDevTools()) {
            m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Warn,
                                                         "ScriptLibraryReloadNoConsumers",
                                                         package,
                                                         "reload",
                                                         "",
                                                         "No active script mods use this script library.",
                                                         {{"reason", pending.Reason},
                                                          {"automatic", pending.Options.Automatic ? "true" : "false"},
                                                          {"dryRun", pending.Options.DryRun ? "true" : "false"}});
        }
        return true;
    }

    for (ScriptMod *mod : consumers) {
        const char *modId = mod ? mod->GetID() : nullptr;
        if (!modId || !*modId)
            continue;
        auto perModPending = m_Pending.find(modId);
        if (perModPending == m_Pending.end())
            continue;
        if (perModPending->second.Options.Automatic) {
            m_Pending.erase(perModPending);
            continue;
        }

        ++pending.BlockedRetryCount;
        const bool firstNotice = pending.LastBlockedNotice.time_since_epoch().count() == 0;
        if (firstNotice || now - pending.LastBlockedNotice >= kBlockedNoticeDelay) {
            pending.LastBlockedNotice = now;
            if (m_Context && m_Context->GetScriptDevTools()) {
                m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Warn,
                                                             "ScriptLibraryReloadBlocked",
                                                             package,
                                                             "reload",
                                                             "",
                                                             "Script library reload is waiting for a pending manual mod reload.",
                                                             {{"reason", pending.Reason},
                                                              {"mod", modId},
                                                              {"pendingReason", perModPending->second.Reason},
                                                              {"retries", std::to_string(pending.BlockedRetryCount)}});
            }
        }
        return false;
    }

    std::vector<ScriptMod *> leased;
    auto releaseLeases = [&]() {
        for (ScriptMod *mod : leased) {
            if (mod)
                mod->ReleaseReloadLease();
        }
        leased.clear();
    };

    for (ScriptMod *mod : consumers) {
        std::string diagnostic;
        if (!mod || !mod->TryAcquireReloadLease(diagnostic)) {
            ++pending.BlockedRetryCount;
            const bool firstNotice = pending.LastBlockedNotice.time_since_epoch().count() == 0;
            if (firstNotice || now - pending.LastBlockedNotice >= kBlockedNoticeDelay) {
                pending.LastBlockedNotice = now;
                if (m_Context && m_Context->GetScriptDevTools()) {
                    const char *modId = mod ? mod->GetID() : nullptr;
                    const auto waitMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - pending.QueuedAt).count();
                    m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Warn,
                                                                 "ScriptLibraryReloadBlocked",
                                                                 package,
                                                                 "reload",
                                                                 "",
                                                                 diagnostic.empty()
                                                                     ? "Script library reload is waiting for active script calls to finish."
                                                                     : diagnostic,
                                                                 {{"reason", pending.Reason},
                                                                  {"mod", modId ? modId : ""},
                                                                  {"activeCalls", mod ? std::to_string(mod->GetActiveScriptCallCount()) : "0"},
                                                                  {"queuedServiceCallbacks", mod ? std::to_string(mod->GetQueuedScriptServiceCallbackCount()) : "0"},
                                                                  {"retries", std::to_string(pending.BlockedRetryCount)},
                                                                  {"waitMs", std::to_string(waitMs)}});
                }
            }
            releaseLeases();
            return false;
        }
        leased.push_back(mod);
    }

    if (m_Context && m_Context->GetScriptDevTools()) {
        m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                     pending.Options.DryRun ? "ScriptLibraryReloadDryRunStarted" : "ScriptLibraryReloadStarted",
                                                     package,
                                                     "reload",
                                                     "",
                                                     pending.Options.DryRun ? "Script library reload dry-run batch started."
                                                                            : "Script library reload batch started.",
                                                     {{"reason", pending.Reason},
                                                      {"consumers", std::to_string(consumers.size())},
                                                      {"automatic", pending.Options.Automatic ? "true" : "false"},
                                                      {"dryRun", pending.Options.DryRun ? "true" : "false"},
                                                      {"forceExports", pending.Options.ForceExports ? "true" : "false"}});
    }

    std::vector<std::unique_ptr<ScriptModReloadCandidate>> candidates;
    candidates.reserve(consumers.size());
    for (ScriptMod *mod : consumers) {
        ScriptModReloadResult prepareResult;
        std::unique_ptr<ScriptModReloadCandidate> candidate = mod->PrepareReloadCandidate(pending.Options, prepareResult);
        const char *modId = mod->GetID();
        if (!candidate) {
            PublishReloadResult(modId ? modId : "", pending.Reason, pending.Options, prepareResult);
            candidates.clear();
            releaseLeases();
            if (m_Context && m_Context->GetScriptDevTools()) {
                m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Warn,
                                                             "ScriptLibraryReloadRejected",
                                                             package,
                                                             "reload",
                                                             prepareResult.SourcePath,
                                                             "Script library reload batch rejected before commit; all old runtimes were kept.",
                                                             {{"reason", pending.Reason},
                                                              {"failedMod", modId ? modId : ""},
                                                              {"consumers", std::to_string(consumers.size())},
                                                              {"diagnostic", prepareResult.Diagnostic}});
            }
            return true;
        }
        if (pending.Options.DryRun)
            PublishReloadResult(modId ? modId : "", pending.Reason, pending.Options, prepareResult);
        candidates.push_back(std::move(candidate));
    }

    if (pending.Options.DryRun) {
        candidates.clear();
        releaseLeases();
        if (m_Context && m_Context->GetScriptDevTools()) {
            m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                         "ScriptLibraryReloadDryRunPassed",
                                                         package,
                                                         "reload",
                                                         "",
                                                         "Script library reload dry-run batch passed.",
                                                         {{"reason", pending.Reason},
                                                          {"consumers", std::to_string(consumers.size())}});
        }
        return true;
    }

    std::vector<size_t> committed;
    for (size_t i = 0; i < consumers.size(); ++i) {
        ScriptModReloadResult commitResult = consumers[i]->CommitReloadCandidate(*candidates[i], pending.Options);
        const char *modId = consumers[i]->GetID();
        PublishReloadResult(modId ? modId : "", pending.Reason, pending.Options, commitResult);
        if (commitResult.Success) {
            committed.push_back(i);
            continue;
        }

        bool rollbackOk = true;
        for (auto it = committed.rbegin(); it != committed.rend(); ++it) {
            ScriptMod *rollbackMod = consumers[*it];
            ScriptModReloadResult rollbackResult = rollbackMod->RollbackCommittedCandidate(*candidates[*it]);
            const char *rollbackId = rollbackMod->GetID();
            ScriptModReloadOptions rollbackOptions = pending.Options;
            rollbackOptions.DryRun = false;
            PublishReloadResult(rollbackId ? rollbackId : "",
                                pending.Reason + " batch rollback",
                                rollbackOptions,
                                rollbackResult);
            rollbackOk = rollbackOk && rollbackResult.Success;
            RegisterMod(rollbackMod);
        }
        candidates.clear();
        releaseLeases();
        if (m_Context && m_Context->GetScriptDevTools()) {
            m_Context->GetScriptDevTools()->PublishEvent(rollbackOk ? ScriptDevEventSeverity::Warn : ScriptDevEventSeverity::Error,
                                                         rollbackOk ? "ScriptLibraryReloadRolledBack" : "ScriptLibraryReloadRestartRequired",
                                                         package,
                                                         "reload",
                                                         commitResult.SourcePath,
                                                         rollbackOk
                                                             ? "Script library reload batch failed during commit; committed consumers were rolled back."
                                                             : "Script library reload batch failed and at least one committed consumer could not roll back; restart is required.",
                                                         {{"reason", pending.Reason},
                                                          {"failedMod", modId ? modId : ""},
                                                          {"committedBeforeFailure", std::to_string(committed.size())},
                                                          {"rollback", rollbackOk ? "success" : "failed"},
                                                          {"diagnostic", commitResult.Diagnostic}});
        }
        return true;
    }

    for (ScriptMod *mod : consumers)
        RegisterMod(mod);
    candidates.clear();
    releaseLeases();
    if (m_Context && m_Context->GetScriptDevTools()) {
        m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                     "ScriptLibraryReloadCommitted",
                                                     package,
                                                     "reload",
                                                     "",
                                                     "Script library reload batch committed.",
                                                     {{"reason", pending.Reason},
                                                      {"consumers", std::to_string(consumers.size())}});
    }
    return true;
}

void ScriptModHotReloadService::PublishNewModRestartRequired(const ScriptFileWatcherWin32::Event &event) {
    if (event.Overflow || !IsScriptModEntryName(utils::GetFileNameW(event.Path).c_str()))
        return;

    for (const auto &record : m_Mods) {
        if (EventBelongsToKnownMod(event.Path, record.Mod))
            return;
    }

    const std::wstring modsRoot = GetModsRoot();
    const std::wstring entryDirectory = utils::GetDirectoryW(event.Path);
    std::wstring candidateRoot = entryDirectory;
    if (SamePathInsensitive(entryDirectory, modsRoot))
        candidateRoot = utils::CombinePathW(entryDirectory, ScriptEntryStem(event.Path));

    const std::wstring key = utils::ResolvePathW(candidateRoot);
    if (key.empty() || !m_ReportedNewModRoots.insert(key).second)
        return;

    std::wstring displayPath = event.Path;
    if (!modsRoot.empty() && utils::IsPathInsideRootW(event.Path, modsRoot))
        displayPath = utils::MakeRelativePathW(event.Path, modsRoot);
    const std::string source = utils::Utf16ToUtf8(displayPath);
    const std::string message = "New script mod entry detected; restart is required to load it.";
    if (m_Context && m_Context->GetLogger())
        m_Context->GetLogger()->Warn("%s %s", message.c_str(), source.c_str());
    if (m_Context && m_Context->GetScriptDevTools()) {
        m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Warn,
                                                     "ScriptModRestartRequired",
                                                     "",
                                                     "watch",
                                                     source,
                                                     message,
                                                     {{"reason", "new script mod"}});
    }
}

} // namespace BML

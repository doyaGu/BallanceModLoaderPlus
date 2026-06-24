#include "ScriptModHotReloadService.h"

#include <algorithm>
#include <sstream>

#include "Logger.h"
#include "ModContext.h"
#include "ScriptDevToolsService.h"
#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"

namespace BML {

namespace {
constexpr auto kDebounceDelay = std::chrono::milliseconds(500);
constexpr auto kRetryDelay = std::chrono::milliseconds(100);
constexpr auto kBlockedNoticeDelay = std::chrono::seconds(1);

bool EndsWithInsensitive(const std::wstring &value, const wchar_t *suffix) {
    if (!suffix)
        return false;
    const size_t suffixLength = std::wcslen(suffix);
    if (value.size() < suffixLength)
        return false;
    return _wcsicmp(value.c_str() + value.size() - suffixLength, suffix) == 0;
}

bool SamePathInsensitive(const std::wstring &left, const std::wstring &right) {
    if (left.empty() || right.empty())
        return false;
    const std::wstring resolvedLeft = utils::ResolvePathW(left);
    const std::wstring resolvedRight = utils::ResolvePathW(right);
    return _wcsicmp(resolvedLeft.c_str(), resolvedRight.c_str()) == 0;
}

bool IsCoveredByWatchedRoot(const std::wstring &root, const std::vector<std::wstring> &watchedRoots) {
    if (root.empty())
        return true;
    for (const auto &watched : watchedRoots) {
        if (SamePathInsensitive(watched, root) || utils::IsPathInsideRootW(root, watched))
            return true;
    }
    return false;
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

void SendReloadResultMessage(ModContext *context,
                             const std::string &id,
                             const ScriptModReloadOptions &options,
                             const ScriptModReloadResult &result) {
    if (!context)
        return;

    std::string message = "[script] ";
    if (options.DryRun)
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
                                                         const ScriptModReloadResult &result) {
    std::vector<ScriptDevEventField> fields = {
        {"reason", reason},
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
    m_Pending.clear();
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
            for (const auto &record : m_Mods) {
                if (!record.Mod || record.Policy != ScriptModReloadPolicy::Auto)
                    continue;
                if (!EventLooksRelevant(event, record.Mod))
                    continue;
                QueueReloadDebounced(record.Mod, automaticOptions, "file changed");
            }
        }
    }

    const auto now = std::chrono::steady_clock::now();
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
                                                         {{"reason", pending.Reason}});
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
        if (m_Context && m_Context->GetLogger()) {
            if (result.Success) {
                if (pending.Options.DryRun)
                    m_Context->GetLogger()->Info("Script mod %s hot reload dry-run passed.", resultId.c_str());
                else
                    m_Context->GetLogger()->Info("Script mod %s hot reload succeeded.", resultId.c_str());
            } else {
                if (pending.Options.DryRun) {
                    m_Context->GetLogger()->Warn("Script mod %s hot reload dry-run failed: %s",
                                                 resultId.c_str(),
                                                 result.Diagnostic.c_str());
                } else {
                    m_Context->GetLogger()->Warn("Script mod %s hot reload failed: %s",
                                                 resultId.c_str(),
                                                 result.Diagnostic.c_str());
                }
            }
        }
        SendReloadResultMessage(m_Context,
                                resultId,
                                pending.Options,
                                result);
        if (m_Context && m_Context->GetScriptDevTools()) {
            const bool rolledBack = !result.Success && ReloadResultRolledBack(result);
            const char *code = "ScriptReloadRejected";
            if (pending.Options.DryRun)
                code = result.Success ? "ScriptReloadDryRunPassed" : "ScriptReloadDryRunFailed";
            else if (result.Success)
                code = "ScriptReloadCommitted";
            else if (rolledBack)
                code = "ScriptReloadRolledBack";
            std::vector<ScriptDevEventField> fields = BuildReloadResultFields(pending.Reason, result);
            m_Context->GetScriptDevTools()->PublishEvent(result.Success ? ScriptDevEventSeverity::Info : ScriptDevEventSeverity::Warn,
                                                         code,
                                                         resultId,
                                                         "reload",
                                                         result.SourcePath,
                                                         result.Success
                                                             ? (pending.Options.DryRun ? "Script reload dry-run passed." : "Script reload committed.")
                                                             : result.Diagnostic,
                                                         fields,
                                                         result.ReloadAttemptId);
        }
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
    for (const auto &record : m_Mods) {
        if (record.Policy == ScriptModReloadPolicy::Auto)
            ++autoCount;
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
           << " blocked=" << blockedCount
           << " automatic=" << (m_AutomaticEnabled ? "on" : "off")
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
    m_Watcher.StopAll();
    if (!m_Started || !m_AutomaticEnabled)
        return;

    std::vector<std::wstring> watchedRoots;
    auto watchOnce = [&](const std::wstring &root) {
        if (root.empty())
            return;
        if (IsCoveredByWatchedRoot(root, watchedRoots))
            return;
        if (m_Watcher.Watch(root))
            watchedRoots.push_back(root);
    };

    watchOnce(GetModsRoot());
    for (auto &record : m_Mods) {
        if (!record.Mod)
            continue;
        record.Policy = ResolvePolicy(record.Mod);
        record.WatchRoot = GetWatchRoot(record.Mod);
        if (record.Policy == ScriptModReloadPolicy::Auto)
            watchOnce(record.WatchRoot);
    }
}

void ScriptModHotReloadService::ClearAutomaticPendingReloads() {
    for (auto it = m_Pending.begin(); it != m_Pending.end();) {
        if (it->second.Options.Automatic)
            it = m_Pending.erase(it);
        else
            ++it;
    }
}

void ScriptModHotReloadService::QueueReloadNow(ScriptMod *mod,
                                               const ScriptModReloadOptions &options,
                                               const std::string &reason) {
    if (!mod || !mod->GetID())
        return;
    PendingReload &pending = m_Pending[mod->GetID()];
    pending.Options = options;
    pending.Due = std::chrono::steady_clock::now();
    pending.QueuedAt = pending.Due;
    pending.LastBlockedNotice = {};
    pending.BlockedRetryCount = 0;
    pending.Reason = reason;
    if (m_Context && m_Context->GetScriptDevTools()) {
        const bool dryRun = options.DryRun;
        m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                     dryRun ? "ScriptReloadDryRunQueued" : "ScriptReloadQueued",
                                                     mod->GetID(),
                                                     "reload",
                                                     "",
                                                     dryRun ? "Script reload dry-run queued." : "Script reload queued.",
                                                     {{"reason", reason},
                                                      {"automatic", options.Automatic ? "true" : "false"},
                                                      {"dryRun", dryRun ? "true" : "false"},
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
    PendingReload &pending = m_Pending[mod->GetID()];
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
                                                     mod->GetID(),
                                                     "reload",
                                                     "",
                                                     dryRun ? "Script reload dry-run queued after debounce."
                                                            : "Script reload queued after debounce.",
                                                     {{"reason", reason},
                                                      {"automatic", options.Automatic ? "true" : "false"},
                                                      {"dryRun", dryRun ? "true" : "false"},
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

bool ScriptModHotReloadService::EventLooksRelevant(const ScriptFileWatcherWin32::Event &event,
                                                   const ScriptMod *mod) const {
    if (!mod)
        return false;
    if (event.Overflow)
        return true;
    if (!EventBelongsToKnownMod(event.Path, mod))
        return false;
    const ScriptModEntry &entry = mod->GetEntry();
    if (entry.SourceKind == ScriptModEntrySourceKind::ZipPackage)
        return EndsWithInsensitive(event.Path, L".zip") && SamePathInsensitive(event.Path, entry.SourcePath);
    return EndsWithInsensitive(event.Path, L".as");
}

bool ScriptModHotReloadService::EventBelongsToKnownMod(const std::wstring &path, const ScriptMod *mod) const {
    if (!mod || path.empty())
        return false;

    const ScriptModEntry &entry = mod->GetEntry();
    if (entry.SourceKind == ScriptModEntrySourceKind::ZipPackage)
        return SamePathInsensitive(path, entry.SourcePath);

    if (entry.SourceKind == ScriptModEntrySourceKind::SingleFile) {
        if (SamePathInsensitive(path, entry.EntryPath))
            return true;
        if (!entry.ResourceRootDirectory.empty() && utils::IsPathInsideRootW(path, entry.ResourceRootDirectory))
            return true;
        return false;
    }

    return !entry.RootDirectory.empty() && utils::IsPathInsideRootW(path, entry.RootDirectory);
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

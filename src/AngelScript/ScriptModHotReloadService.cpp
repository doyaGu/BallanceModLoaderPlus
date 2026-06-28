#include "ScriptModHotReloadService.h"

#include <algorithm>
#include <cwctype>
#include <memory>
#include <sstream>
#include <utility>

#include "Logger.h"
#include "ModContext.h"
#include "ScriptDevToolsService.h"
#include "ScriptLibraryServices.h"
#include "ScriptModHotReloadPathFilter.h"
#include "ScriptSourceSnapshotBuilder.h"
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

std::string LibraryPackageKey(const ScriptLibraryReloadPackage &package) {
    return LibraryPackageKey(package.Id, package.Version);
}

bool SameLibraryPackage(const ScriptLibraryReloadPackage &package,
                        const std::string &id,
                        const std::string &version) {
    return package.Id == id && package.Version == version;
}

bool AddLibraryPackage(std::vector<ScriptLibraryReloadPackage> &packages,
                       const std::string &id,
                       const std::string &version) {
    if (id.empty() || version.empty())
        return false;
    const auto existing = std::find_if(packages.begin(), packages.end(), [&](const ScriptLibraryReloadPackage &package) {
        return SameLibraryPackage(package, id, version);
    });
    if (existing != packages.end())
        return false;
    packages.push_back({id, version});
    std::sort(packages.begin(), packages.end(), [](const ScriptLibraryReloadPackage &left,
                                                   const ScriptLibraryReloadPackage &right) {
        if (left.Id == right.Id)
            return left.Version < right.Version;
        return left.Id < right.Id;
    });
    return true;
}

bool AddLibraryPackages(std::vector<ScriptLibraryReloadPackage> &packages,
                        const std::vector<ScriptLibraryReloadPackage> &additions) {
    bool changed = false;
    for (const ScriptLibraryReloadPackage &package : additions)
        changed = AddLibraryPackage(packages, package.Id, package.Version) || changed;
    return changed;
}

bool RemoveLibraryPackage(std::vector<ScriptLibraryReloadPackage> &packages,
                          const std::string &id,
                          const std::string &version) {
    const auto oldSize = packages.size();
    packages.erase(std::remove_if(packages.begin(), packages.end(), [&](const ScriptLibraryReloadPackage &package) {
        return SameLibraryPackage(package, id, version);
    }), packages.end());
    return packages.size() != oldSize;
}

bool HasLibraryPackage(const std::vector<ScriptLibraryReloadPackage> &packages,
                       const std::string &id,
                       const std::string &version) {
    return std::any_of(packages.begin(), packages.end(), [&](const ScriptLibraryReloadPackage &package) {
        return SameLibraryPackage(package, id, version);
    });
}

std::string FormatLibraryPackageList(const std::vector<ScriptLibraryReloadPackage> &packages) {
    std::string value;
    for (const ScriptLibraryReloadPackage &package : packages) {
        if (!value.empty())
            value += ", ";
        value += LibraryPackageKey(package);
    }
    return value;
}

std::string MakeLibraryPendingKey(const ScriptModReloadOptions &options) {
    std::ostringstream stream;
    stream << (options.Automatic ? "automatic" : "manual")
           << ":dry=" << (options.DryRun ? '1' : '0')
           << ":check=" << (options.CheckStateHooks ? '1' : '0')
           << ":force=" << (options.ForceExports ? '1' : '0');
    return stream.str();
}

std::string MakeLibraryPendingReason(const std::string &reason,
                                     const std::vector<ScriptLibraryReloadPackage> &packages) {
    if (packages.empty())
        return reason;
    const bool manualReason = utils::StartsWith(reason, "manual library reload");
    const bool libraryChangeReason = utils::StartsWith(reason, "library changed") ||
                                     utils::StartsWith(reason, "libraries changed");
    if (!manualReason && !libraryChangeReason && packages.size() <= 1)
        return reason;
    if (manualReason)
        return "manual library reload " + FormatLibraryPackageList(packages);
    if (packages.size() == 1)
        return "library changed " + LibraryPackageKey(packages.front());
    return "libraries changed " + FormatLibraryPackageList(packages);
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

const char *BoolField(bool value) {
    return value ? "true" : "false";
}

void AppendReloadOptionFields(std::vector<ScriptDevEventField> &fields,
                              const ScriptModReloadOptions &options,
                              const std::string &prefix = std::string(),
                              bool includeAutomatic = true,
                              bool includeCheckState = true,
                              bool includeForceExports = true) {
    const bool prefixed = !prefix.empty();
    if (includeAutomatic)
        fields.push_back({prefixed ? prefix + "Automatic" : "automatic", BoolField(options.Automatic)});
    fields.push_back({prefixed ? prefix + "DryRun" : "dryRun", BoolField(options.DryRun)});
    if (includeCheckState)
        fields.push_back({prefixed ? prefix + "CheckState" : "checkState", BoolField(options.CheckStateHooks)});
    if (includeForceExports)
        fields.push_back({prefixed ? prefix + "ForceExports" : "forceExports", BoolField(options.ForceExports)});
}

void AppendReloadRequestFields(std::vector<ScriptDevEventField> &fields,
                               const std::string &reason,
                               const ScriptModReloadOptions &options,
                               const std::string &prefix = std::string(),
                               bool includeCheckState = true,
                               bool includeForceExports = true) {
    const bool prefixed = !prefix.empty();
    fields.push_back({prefixed ? prefix + "Reason" : "reason", reason});
    AppendReloadOptionFields(fields, options, prefix, true, includeCheckState, includeForceExports);
}

std::vector<ScriptDevEventField> BuildReloadRequestFields(const std::string &reason,
                                                          const ScriptModReloadOptions &options,
                                                          bool includeCheckState = true,
                                                          bool includeForceExports = true) {
    std::vector<ScriptDevEventField> fields;
    AppendReloadRequestFields(fields, reason, options, std::string(), includeCheckState, includeForceExports);
    return fields;
}

std::vector<ScriptDevEventField> BuildLibraryBatchRequestFields(const std::string &reason,
                                                                const ScriptModReloadOptions &options,
                                                                size_t consumerCount,
                                                                const std::vector<ScriptLibraryReloadPackage> &packages) {
    std::vector<ScriptDevEventField> fields = BuildReloadRequestFields(reason, options, false, true);
    fields.push_back({"consumers", std::to_string(consumerCount)});
    fields.push_back({"libraries", std::to_string(packages.size())});
    fields.push_back({"packages", FormatLibraryPackageList(packages)});
    return fields;
}

std::vector<ScriptDevEventField> BuildReloadResultFields(const std::string &reason,
                                                         const ScriptModReloadOptions &options,
                                                         const ScriptModReloadResult &result) {
    std::vector<ScriptDevEventField> fields = BuildReloadRequestFields(reason, options);
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

class ScriptModReloadOperation final {
private:
    using PendingReload = ScriptModHotReloadService::PendingReload;

public:
    ScriptModReloadOperation(ScriptModHotReloadService &service,
                             const std::string &id,
                             PendingReload &pending)
        : m_Service(service), m_Id(id), m_Pending(pending) {
    }

    bool Run(std::chrono::steady_clock::time_point now);

private:
    ScriptDevToolsService *DevTools() const;
    void PublishBlocked(const std::string &message, std::chrono::steady_clock::time_point now);
    void PublishStarted();
    ScriptModReloadResult ExecuteReload();
    void PublishResult(const ScriptModReloadResult &result);

    ScriptModHotReloadService &m_Service;
    const std::string &m_Id;
    PendingReload &m_Pending;
    ScriptMod *m_Mod = nullptr;
};

ScriptDevToolsService *ScriptModReloadOperation::DevTools() const {
    return m_Service.m_Context ? m_Service.m_Context->GetScriptDevTools() : nullptr;
}

void ScriptModReloadOperation::PublishBlocked(const std::string &message,
                                              std::chrono::steady_clock::time_point now) {
    ++m_Pending.BlockedRetryCount;
    const bool firstNotice = m_Pending.LastBlockedNotice.time_since_epoch().count() == 0;
    if (!firstNotice && now - m_Pending.LastBlockedNotice < kBlockedNoticeDelay)
        return;

    m_Pending.LastBlockedNotice = now;
    ScriptDevToolsService *devTools = DevTools();
    if (!devTools)
        return;

    const auto waitMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_Pending.QueuedAt).count();
    std::vector<ScriptDevEventField> fields = BuildReloadRequestFields(m_Pending.Reason, m_Pending.Options);
    fields.push_back({"activeCalls", m_Mod ? std::to_string(m_Mod->GetActiveScriptCallCount()) : "0"});
    fields.push_back({"queuedServiceCallbacks", m_Mod ? std::to_string(m_Mod->GetQueuedScriptServiceCallbackCount()) : "0"});
    fields.push_back({"retries", std::to_string(m_Pending.BlockedRetryCount)});
    fields.push_back({"waitMs", std::to_string(waitMs)});
    devTools->PublishEvent(ScriptDevEventSeverity::Warn,
                           "ScriptReloadBlocked",
                           m_Id,
                           "reload",
                           "",
                           message,
                           fields);
}

void ScriptModReloadOperation::PublishStarted() {
    ScriptDevToolsService *devTools = DevTools();
    if (!devTools)
        return;

    devTools->PublishEvent(ScriptDevEventSeverity::Info,
                           m_Pending.Options.DryRun ? "ScriptReloadDryRunStarted" : "ScriptReloadStarted",
                           m_Id,
                           "reload",
                           "",
                           m_Pending.Options.DryRun ? "Script reload dry-run started." : "Script reload started.",
                           BuildReloadRequestFields(m_Pending.Reason, m_Pending.Options));
}

ScriptModReloadResult ScriptModReloadOperation::ExecuteReload() {
    return m_Pending.Options.DryRun
               ? m_Mod->TryHotReloadDryRun(m_Pending.Options)
               : m_Mod->TryHotReload(m_Pending.Options);
}

void ScriptModReloadOperation::PublishResult(const ScriptModReloadResult &result) {
    const char *currentId = m_Mod ? m_Mod->GetID() : nullptr;
    const std::string resultId = currentId && *currentId ? currentId : m_Id;
    m_Service.PublishReloadResult(resultId, m_Pending.Reason, m_Pending.Options, result);
    if (result.Success && !m_Pending.Options.DryRun)
        m_Service.RegisterMod(m_Mod);
}

bool ScriptModReloadOperation::Run(std::chrono::steady_clock::time_point now) {
    m_Mod = m_Service.FindMod(m_Id);
    if (!m_Mod)
        return true;

    if (!m_Mod->CanHotReloadNow()) {
        PublishBlocked("Script reload is waiting for active script calls to finish.", now);
        return false;
    }

    PublishStarted();
    const ScriptModReloadResult result = ExecuteReload();
    if (result.RetryLater) {
        PublishBlocked(result.Diagnostic.empty()
                           ? "Script reload asked to retry later."
                           : result.Diagnostic,
                       now);
        return false;
    }

    PublishResult(result);
    return true;
}

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
            RefreshRegisteredModOrder();
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
    RefreshRegisteredModOrder();
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
    RefreshRegisteredModOrder();
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
        ScriptModReloadOptions automaticOptions;
        automaticOptions.Automatic = true;
        std::vector<ScriptLibraryReloadPackage> changedLibraryPackages;
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
            AddLibraryPackages(changedLibraryPackages, GetActiveLibraryPackages(true));
            for (const auto &record : m_Mods) {
                if (record.Mod && record.Policy == ScriptModReloadPolicy::Auto)
                    QueueReloadDebounced(record.Mod, automaticOptions, "watch overflow");
            }
        }
        for (const auto &event : events) {
            PublishNewModRestartRequired(event);
            for (const auto &record : m_Mods) {
                if (!record.Mod || record.Policy != ScriptModReloadPolicy::Auto)
                    continue;
                const std::vector<ScriptLibraryReloadPackage> libraryPackages = GetEventAffectedLibraryPackages(event, record.Mod);
                if (!libraryPackages.empty()) {
                    AddLibraryPackages(changedLibraryPackages, libraryPackages);
                    continue;
                }
                if (!EventLooksRelevant(event, record.Mod))
                    continue;
                QueueReloadDebounced(record.Mod, automaticOptions, "file changed");
            }
        }
        if (!changedLibraryPackages.empty()) {
            QueueLibraryReloadDebounced(changedLibraryPackages,
                                        automaticOptions,
                                        MakeLibraryPendingReason("libraries changed", changedLibraryPackages));
        }
    }

    const auto now = std::chrono::steady_clock::now();
    auto sortReadyKeys = [](std::vector<std::string> &keys, const auto &pending) {
        std::sort(keys.begin(), keys.end(), [&](const std::string &left, const std::string &right) {
            const auto leftIt = pending.find(left);
            const auto rightIt = pending.find(right);
            if (leftIt == pending.end() || rightIt == pending.end())
                return left < right;
            if (leftIt->second.Due != rightIt->second.Due)
                return leftIt->second.Due < rightIt->second.Due;
            if (leftIt->second.QueuedAt != rightIt->second.QueuedAt)
                return leftIt->second.QueuedAt < rightIt->second.QueuedAt;
            return left < right;
        });
    };

    std::vector<std::string> readyLibraries;
    for (const auto &entry : m_PendingLibraries) {
        if (entry.second.Due <= now)
            readyLibraries.push_back(entry.first);
    }
    sortReadyKeys(readyLibraries, m_PendingLibraries);
    for (const std::string &key : readyLibraries) {
        auto pendingIt = m_PendingLibraries.find(key);
        if (pendingIt == m_PendingLibraries.end())
            continue;
        PendingLibraryReload pending = pendingIt->second;
        m_PendingLibraries.erase(pendingIt);
        if (pending.Packages.empty())
            continue;
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
    sortReadyKeys(ready, m_Pending);

    for (const std::string &id : ready) {
        auto pendingIt = m_Pending.find(id);
        if (pendingIt == m_Pending.end())
            continue;
        PendingReload pending = pendingIt->second;
        m_Pending.erase(pendingIt);
        if (pending.Options.Automatic && !m_AutomaticEnabled) {
            continue;
        }
        if (!ScriptModReloadOperation(*this, id, pending).Run(now)) {
            pending.Due = now + kRetryDelay;
            m_Pending[id] = pending;
        }
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
    size_t pendingLibraryPackageCount = 0;
    for (const auto &record : m_Mods) {
        if (record.Policy == ScriptModReloadPolicy::Auto)
            ++autoCount;
        if (record.Mod) {
            for (const ScriptLibraryUse &library : record.Mod->GetDefinition().SourceLibraries)
                activeLibraryPackages.insert(LibraryUseKey(library));
        }
    }
    for (const auto &entry : m_PendingLibraries)
        pendingLibraryPackageCount += entry.second.Packages.size();
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
           << " pendingLibPackages=" << pendingLibraryPackageCount
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

void ScriptModHotReloadService::RefreshRegisteredModOrder() {
    if (!m_Context || m_Mods.size() < 2)
        return;

    std::vector<ModRecord> ordered;
    ordered.reserve(m_Mods.size());
    std::unordered_set<std::string> orderedIds;
    orderedIds.reserve(m_Mods.size());

    for (int i = 0; i < m_Context->GetModCount(); ++i) {
        auto *scriptMod = dynamic_cast<ScriptMod *>(m_Context->GetMod(i));
        if (!scriptMod || !scriptMod->GetID())
            continue;

        const std::string id = scriptMod->GetID();
        auto it = std::find_if(m_Mods.begin(), m_Mods.end(), [&](const ModRecord &record) {
            return record.Mod && record.Mod->GetID() && id == record.Mod->GetID();
        });
        if (it == m_Mods.end())
            continue;

        ModRecord record = *it;
        record.Mod = scriptMod;
        record.Policy = ResolvePolicy(scriptMod);
        record.WatchRoot = GetWatchRoot(scriptMod);
        ordered.push_back(std::move(record));
        orderedIds.insert(id);
    }

    if (ordered.empty())
        return;

    for (const ModRecord &record : m_Mods) {
        const char *id = record.Mod ? record.Mod->GetID() : nullptr;
        if (!id || orderedIds.find(id) == orderedIds.end())
            ordered.push_back(record);
    }

    m_Mods = std::move(ordered);
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
            std::vector<ScriptDevEventField> fields;
            AppendReloadRequestFields(fields, previous.Reason, previous.Options, "previous");
            AppendReloadRequestFields(fields, reason, options);
            m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                         "ScriptReloadPendingReplaced",
                                                         id,
                                                         "reload",
                                                         "",
                                                         "Pending script reload was replaced by a newer request.",
                                                         fields);
        }
        const bool dryRun = options.DryRun;
        m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                     dryRun ? "ScriptReloadDryRunQueued" : "ScriptReloadQueued",
                                                     id,
                                                     "reload",
                                                     "",
                                                     dryRun ? "Script reload dry-run queued." : "Script reload queued.",
                                                     BuildReloadRequestFields(reason, options));
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
        !existingIt->second.Options.Automatic &&
        !existingIt->second.Options.DryRun) {
        if (m_Context && m_Context->GetScriptDevTools()) {
            std::vector<ScriptDevEventField> fields = {{"reason", reason},
                                                       {"pendingReason", existingIt->second.Reason}};
            AppendReloadOptionFields(fields, existingIt->second.Options, "pending", false, true, true);
            m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                         "ScriptReloadAutoCoalesced",
                                                         id,
                                                         "reload",
                                                         "",
                                                         "Automatic script reload was coalesced into a pending manual reload.",
                                                         fields);
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
                                                     BuildReloadRequestFields(reason, options));
    }
}

void ScriptModHotReloadService::QueueLibraryReloadNow(const std::string &id,
                                                      const std::string &version,
                                                      const ScriptModReloadOptions &options,
                                                      const std::string &reason) {
    if (id.empty() || version.empty())
        return;
    if (!options.Automatic && !options.DryRun) {
        for (auto it = m_PendingLibraries.begin(); it != m_PendingLibraries.end();) {
            if (it->second.Options.Automatic && RemoveLibraryPackage(it->second.Packages, id, version)) {
                if (it->second.Packages.empty()) {
                    it = m_PendingLibraries.erase(it);
                    continue;
                }
                it->second.Reason = MakeLibraryPendingReason(it->second.Reason, it->second.Packages);
            }
            ++it;
        }
    }

    const std::string key = MakeLibraryPendingKey(options);
    auto existingIt = m_PendingLibraries.find(key);
    const bool replaced = existingIt != m_PendingLibraries.end();
    PendingLibraryReload previous;
    if (replaced)
        previous = existingIt->second;

    PendingLibraryReload &pending = m_PendingLibraries[key];
    if (!replaced)
        pending.Packages.clear();
    const bool packageAdded = AddLibraryPackage(pending.Packages, id, version);
    pending.Options = options;
    pending.Due = std::chrono::steady_clock::now();
    pending.QueuedAt = replaced ? previous.QueuedAt : pending.Due;
    pending.LastBlockedNotice = {};
    pending.BlockedRetryCount = 0;
    pending.Reason = MakeLibraryPendingReason(reason, pending.Packages);

    if (m_Context && m_Context->GetScriptDevTools()) {
        if (replaced) {
            std::vector<ScriptDevEventField> fields;
            AppendReloadRequestFields(fields, previous.Reason, previous.Options, "previous", false, false);
            fields.push_back({"previousPackages", FormatLibraryPackageList(previous.Packages)});
            AppendReloadRequestFields(fields, pending.Reason, options, std::string(), false, false);
            fields.push_back({"packages", FormatLibraryPackageList(pending.Packages)});
            const bool merged = packageAdded && previous.Packages.size() != pending.Packages.size();
            m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                         merged ? "ScriptLibraryReloadPendingMerged"
                                                                : "ScriptLibraryReloadPendingReplaced",
                                                         key,
                                                         "reload",
                                                         "",
                                                         merged ? "Script library reload was coalesced into an existing pending batch."
                                                                : "Pending script library reload was replaced by a newer request.",
                                                         fields);
        }
        m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                     options.DryRun ? "ScriptLibraryReloadDryRunQueued" : "ScriptLibraryReloadQueued",
                                                     key,
                                                     "reload",
                                                     "",
                                                     options.DryRun ? "Script library reload dry-run queued."
                                                                    : "Script library reload queued.",
                                                     BuildLibraryBatchRequestFields(pending.Reason,
                                                                                    options,
                                                                                    GetLibraryConsumers(pending.Packages, options.Automatic).size(),
                                                                                    pending.Packages));
    }
}

void ScriptModHotReloadService::QueueLibraryReloadDebounced(const std::string &id,
                                                            const std::string &version,
                                                            const ScriptModReloadOptions &options,
                                                            const std::string &reason) {
    std::vector<ScriptLibraryReloadPackage> packages;
    AddLibraryPackage(packages, id, version);
    QueueLibraryReloadDebounced(packages, options, reason);
}

void ScriptModHotReloadService::QueueLibraryReloadDebounced(const std::vector<ScriptLibraryReloadPackage> &packages,
                                                            const ScriptModReloadOptions &options,
                                                            const std::string &reason) {
    std::vector<ScriptLibraryReloadPackage> pendingPackages;
    AddLibraryPackages(pendingPackages, packages);
    if (pendingPackages.empty())
        return;
    if (options.Automatic && !m_AutomaticEnabled)
        return;

    if (options.Automatic) {
        std::vector<ScriptLibraryReloadPackage> uncoalesced;
        for (const ScriptLibraryReloadPackage &package : pendingPackages) {
            bool coalesced = false;
            const std::string packageKey = LibraryPackageKey(package);
            for (const auto &entry : m_PendingLibraries) {
                if (entry.second.Options.Automatic ||
                    entry.second.Options.DryRun ||
                    !HasLibraryPackage(entry.second.Packages, package.Id, package.Version)) {
                    continue;
                }
                coalesced = true;
                if (m_Context && m_Context->GetScriptDevTools()) {
                    std::vector<ScriptDevEventField> fields = {{"reason", reason},
                                                               {"pendingReason", entry.second.Reason},
                                                               {"package", packageKey}};
                    AppendReloadOptionFields(fields, entry.second.Options, "pending", false, false, true);
                    m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                                 "ScriptLibraryReloadAutoCoalesced",
                                                                 packageKey,
                                                                 "reload",
                                                                 "",
                                                                 "Automatic script library reload was coalesced into a pending manual reload.",
                                                                 fields);
                }
                break;
            }
            if (!coalesced)
                AddLibraryPackage(uncoalesced, package.Id, package.Version);
        }
        pendingPackages = std::move(uncoalesced);
        if (pendingPackages.empty())
            return;
    } else if (!options.DryRun) {
        for (auto it = m_PendingLibraries.begin(); it != m_PendingLibraries.end();) {
            bool removed = false;
            if (it->second.Options.Automatic) {
                for (const ScriptLibraryReloadPackage &package : pendingPackages)
                    removed = RemoveLibraryPackage(it->second.Packages, package.Id, package.Version) || removed;
            }
            if (removed) {
                if (it->second.Packages.empty()) {
                    it = m_PendingLibraries.erase(it);
                    continue;
                }
                it->second.Reason = MakeLibraryPendingReason(it->second.Reason, it->second.Packages);
            }
            ++it;
        }
    }

    const std::string key = MakeLibraryPendingKey(options);
    auto existingIt = m_PendingLibraries.find(key);
    const bool replaced = existingIt != m_PendingLibraries.end();
    PendingLibraryReload previous;
    if (replaced)
        previous = existingIt->second;
    PendingLibraryReload &pending = m_PendingLibraries[key];
    if (!replaced)
        pending.Packages.clear();
    AddLibraryPackages(pending.Packages, pendingPackages);
    pending.Options = options;
    const auto queuedAt = std::chrono::steady_clock::now();
    pending.QueuedAt = replaced ? previous.QueuedAt : queuedAt;
    pending.Due = queuedAt + kDebounceDelay;
    pending.LastBlockedNotice = {};
    pending.BlockedRetryCount = 0;
    pending.Reason = MakeLibraryPendingReason(reason, pending.Packages);

    if (m_Context && m_Context->GetScriptDevTools()) {
        m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                     options.DryRun ? "ScriptLibraryReloadDryRunQueued" : "ScriptLibraryReloadQueued",
                                                     key,
                                                     "reload",
                                                     "",
                                                     options.DryRun ? "Script library reload dry-run queued after debounce."
                                                                    : "Script library reload queued after debounce.",
                                                     BuildLibraryBatchRequestFields(pending.Reason,
                                                                                    options,
                                                                                    GetLibraryConsumers(pending.Packages, options.Automatic).size(),
                                                                                    pending.Packages));
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

std::vector<ScriptLibraryReloadPackage> ScriptModHotReloadService::GetActiveLibraryPackages(bool automaticOnly) const {
    std::vector<ScriptLibraryReloadPackage> packages;
    for (const auto &record : m_Mods) {
        if (!record.Mod)
            continue;
        if (automaticOnly && record.Policy != ScriptModReloadPolicy::Auto)
            continue;
        for (const ScriptLibraryUse &library : record.Mod->GetDefinition().SourceLibraries)
            AddLibraryPackage(packages, library.Id, library.Version);
    }
    return packages;
}

std::vector<ScriptLibraryReloadPackage> ScriptModHotReloadService::GetEventAffectedLibraryPackages(
    const ScriptFileWatcherWin32::Event &event,
    const ScriptMod *mod) const {
    std::vector<ScriptLibraryReloadPackage> packages;
    if (!mod)
        return packages;
    for (const ScriptLibraryUse &library : mod->GetDefinition().SourceLibraries) {
        if (!ScriptHotReloadEventLooksRelevantToLibraryUse(event, library))
            continue;
        AddLibraryPackage(packages, library.Id, library.Version);
    }
    return packages;
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

bool ScriptModHotReloadService::ModUsesAnyLibrary(const ScriptMod *mod,
                                                  const std::vector<ScriptLibraryReloadPackage> &packages) const {
    if (!mod || packages.empty())
        return false;
    for (const ScriptLibraryReloadPackage &package : packages) {
        if (ModUsesLibrary(mod, package.Id, package.Version))
            return true;
    }
    return false;
}

std::vector<ScriptMod *> ScriptModHotReloadService::GetLibraryConsumers(const std::string &id,
                                                                        const std::string &version,
                                                                        bool automaticOnly) const {
    std::vector<ScriptLibraryReloadPackage> packages;
    AddLibraryPackage(packages, id, version);
    return GetLibraryConsumers(packages, automaticOnly);
}

std::vector<ScriptMod *> ScriptModHotReloadService::GetLibraryConsumers(const std::vector<ScriptLibraryReloadPackage> &packages,
                                                                        bool automaticOnly) const {
    std::vector<ScriptMod *> consumers;
    for (const auto &record : m_Mods) {
        if (!record.Mod || !ModUsesAnyLibrary(record.Mod, packages))
            continue;
        if (automaticOnly && record.Policy != ScriptModReloadPolicy::Auto)
            continue;
        consumers.push_back(record.Mod);
    }
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

class ScriptLibraryReloadOperation final {
private:
    using PendingLibraryReload = ScriptModHotReloadService::PendingLibraryReload;

    struct Batch {
        ~Batch() {
            DiscardResources();
        }

        void DiscardResources() {
            Candidates.clear();
            for (ScriptMod *mod : Leased) {
                if (mod)
                    mod->ReleaseReloadLease();
            }
            Leased.clear();
        }

        std::string Package;
        std::vector<ScriptLibraryReloadPackage> Packages;
        ScriptLibrarySourceCache SourceCache;
        std::vector<ScriptMod *> Consumers;
        std::vector<ScriptMod *> Leased;
        std::vector<std::unique_ptr<ScriptModReloadCandidate>> Candidates;
        std::vector<size_t> Committed;
    };

public:
    ScriptLibraryReloadOperation(ScriptModHotReloadService &service, PendingLibraryReload &pending)
        : m_Service(service), m_Pending(pending) {
        m_Batch.Packages = m_Pending.Packages;
        m_Batch.Package = FormatLibraryPackageList(m_Batch.Packages);
        m_Batch.Consumers = m_Service.GetLibraryConsumers(m_Batch.Packages,
                                                          m_Pending.Options.Automatic);
    }

    bool Run();

private:
    bool DeferForManualPending(std::chrono::steady_clock::time_point now);
    bool TryAcquireLeases(std::chrono::steady_clock::time_point now);
    bool CaptureLibrarySources();
    bool CapturedSourcesMatchActiveConsumers() const;
    bool PrepareBatch();
    bool CommitBatch();
    void PublishNoConsumers();
    void PublishNoSourceChanges();
    void PublishStarted();
    void PublishRejected(ScriptMod *failedMod, const ScriptModReloadResult &prepareResult);
    void PublishDryRunPassed();
    void PublishCommitFailure(ScriptMod *failedMod,
                              const ScriptModReloadResult &commitResult,
                              bool rollbackOk);
    void PublishCommitted();
    void ClearCoveredAutomaticModReloads();
    ScriptDevToolsService *DevTools() const;

    ScriptModHotReloadService &m_Service;
    PendingLibraryReload &m_Pending;
    Batch m_Batch;
};

ScriptDevToolsService *ScriptLibraryReloadOperation::DevTools() const {
    return m_Service.m_Context ? m_Service.m_Context->GetScriptDevTools() : nullptr;
}

void ScriptLibraryReloadOperation::PublishNoConsumers() {
    ScriptDevToolsService *devTools = DevTools();
    if (!devTools)
        return;

    devTools->PublishEvent(ScriptDevEventSeverity::Warn,
                           "ScriptLibraryReloadNoConsumers",
                           m_Batch.Package,
                           "reload",
                           "",
                           "No active script mods use these script libraries.",
                           BuildLibraryBatchRequestFields(m_Pending.Reason,
                                                          m_Pending.Options,
                                                          0,
                                                          m_Batch.Packages));
}

void ScriptLibraryReloadOperation::PublishNoSourceChanges() {
    ScriptDevToolsService *devTools = DevTools();
    if (!devTools)
        return;

    devTools->PublishEvent(ScriptDevEventSeverity::Info,
                           "ScriptLibraryReloadNoSourceChanges",
                           m_Batch.Package,
                           "reload",
                           "",
                           "Script library reload skipped because captured source hashes match active consumers.",
                           {{"reason", m_Pending.Reason},
                            {"libraries", std::to_string(m_Batch.Packages.size())},
                            {"packages", m_Batch.Package},
                            {"consumers", std::to_string(m_Batch.Consumers.size())},
                            {"capturedFiles", std::to_string(m_Batch.SourceCache.GetFileCount())}});
}

bool ScriptLibraryReloadOperation::DeferForManualPending(std::chrono::steady_clock::time_point now) {
    for (ScriptMod *mod : m_Batch.Consumers) {
        const char *modId = mod ? mod->GetID() : nullptr;
        if (!modId || !*modId)
            continue;

        auto perModPending = m_Service.m_Pending.find(modId);
        if (perModPending == m_Service.m_Pending.end())
            continue;
        if (perModPending->second.Options.Automatic) {
            m_Service.m_Pending.erase(perModPending);
            continue;
        }

        ++m_Pending.BlockedRetryCount;
        const bool firstNotice = m_Pending.LastBlockedNotice.time_since_epoch().count() == 0;
        if (firstNotice || now - m_Pending.LastBlockedNotice >= kBlockedNoticeDelay) {
            m_Pending.LastBlockedNotice = now;
            if (ScriptDevToolsService *devTools = DevTools()) {
                devTools->PublishEvent(ScriptDevEventSeverity::Warn,
                                       "ScriptLibraryReloadBlocked",
                                       m_Batch.Package,
                                       "reload",
                                       "",
                                       "Script library reload is waiting for a pending manual mod reload.",
                                       {{"reason", m_Pending.Reason},
                                        {"mod", modId},
                                        {"pendingReason", perModPending->second.Reason},
                                        {"retries", std::to_string(m_Pending.BlockedRetryCount)}});
            }
        }
        return true;
    }

    return false;
}

bool ScriptLibraryReloadOperation::TryAcquireLeases(std::chrono::steady_clock::time_point now) {
    for (ScriptMod *mod : m_Batch.Consumers) {
        std::string diagnostic;
        if (!mod || !mod->TryAcquireReloadLease(diagnostic)) {
            ++m_Pending.BlockedRetryCount;
            const bool firstNotice = m_Pending.LastBlockedNotice.time_since_epoch().count() == 0;
            if (firstNotice || now - m_Pending.LastBlockedNotice >= kBlockedNoticeDelay) {
                m_Pending.LastBlockedNotice = now;
                if (ScriptDevToolsService *devTools = DevTools()) {
                    const char *modId = mod ? mod->GetID() : nullptr;
                    const auto waitMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_Pending.QueuedAt).count();
                    devTools->PublishEvent(ScriptDevEventSeverity::Warn,
                                           "ScriptLibraryReloadBlocked",
                                           m_Batch.Package,
                                           "reload",
                                           "",
                                           diagnostic.empty()
                                               ? "Script library reload is waiting for active script calls to finish."
                                               : diagnostic,
                                           {{"reason", m_Pending.Reason},
                                            {"mod", modId ? modId : ""},
                                            {"activeCalls", mod ? std::to_string(mod->GetActiveScriptCallCount()) : "0"},
                                            {"queuedServiceCallbacks", mod ? std::to_string(mod->GetQueuedScriptServiceCallbackCount()) : "0"},
                                            {"retries", std::to_string(m_Pending.BlockedRetryCount)},
                                            {"waitMs", std::to_string(waitMs)}});
                }
            }
            m_Batch.DiscardResources();
            return false;
        }
        m_Batch.Leased.push_back(mod);
    }

    return true;
}

void ScriptLibraryReloadOperation::PublishStarted() {
    ScriptDevToolsService *devTools = DevTools();
    if (!devTools)
        return;

    devTools->PublishEvent(ScriptDevEventSeverity::Info,
                           m_Pending.Options.DryRun ? "ScriptLibraryReloadDryRunStarted" : "ScriptLibraryReloadStarted",
                           m_Batch.Package,
                           "reload",
                           "",
                           m_Pending.Options.DryRun ? "Script library reload dry-run batch started."
                                                    : "Script library reload batch started.",
                           BuildLibraryBatchRequestFields(m_Pending.Reason,
                                                          m_Pending.Options,
                                                          m_Batch.Consumers.size(),
                                                          m_Batch.Packages));
}

bool ScriptLibraryReloadOperation::CaptureLibrarySources() {
    ScriptDiagnostic diagnostic;
    ScriptLibraryRegistry registry = MakeInstalledScriptLibraryRegistry(m_Service.m_Context, diagnostic);
    if (diagnostic.Message.empty()) {
        bool captured = true;
        for (const ScriptLibraryReloadPackage &package : m_Batch.Packages) {
            if (!m_Batch.SourceCache.CapturePackage(registry, package.Id, package.Version, diagnostic)) {
                captured = false;
                if (diagnostic.EntryPath.empty())
                    diagnostic.EntryPath = LibraryPackageKey(package);
                break;
            }
        }
        if (captured)
            return true;
    }
    if (diagnostic.Message.empty()) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry,
                                          "Script library batch source capture failed.");
        diagnostic.EntryPath = m_Batch.Package;
    }

    ScriptModReloadResult result;
    result.Diagnostic = FormatScriptDiagnostic(diagnostic);
    result.SourcePath = diagnostic.EntryPath;
    result.Fields = {
        {"libraries", m_Batch.Package},
        {"phase", "capture"},
    };
    PublishRejected(nullptr, result);
    return false;
}

bool ScriptLibraryReloadOperation::CapturedSourcesMatchActiveConsumers() const {
    bool comparedAny = false;
    for (const ScriptMod *mod : m_Batch.Consumers) {
        if (!mod)
            return false;

        bool comparedConsumer = false;
        for (const ScriptSourceDependency &dependency : mod->GetDefinition().SourceDependencies) {
            if (!dependency.LibraryOwned ||
                !HasLibraryPackage(m_Batch.Packages, dependency.LibraryId, dependency.LibraryVersion)) {
                continue;
            }

            comparedAny = true;
            comparedConsumer = true;
            if (dependency.ContentHash.empty())
                return false;

            std::string capturedHash;
            if (!m_Batch.SourceCache.GetFileContentHash(dependency.PhysicalPath, capturedHash))
                return false;
            if (capturedHash != dependency.ContentHash)
                return false;
        }

        if (!comparedConsumer)
            return false;
    }

    return comparedAny;
}

bool ScriptLibraryReloadOperation::PrepareBatch() {
    m_Batch.Candidates.reserve(m_Batch.Consumers.size());
    for (ScriptMod *mod : m_Batch.Consumers) {
        ScriptModReloadResult prepareResult;
        ScriptModReloadOptions options = m_Pending.Options;
        options.LibrarySourceCache = &m_Batch.SourceCache;
        std::unique_ptr<ScriptModReloadCandidate> candidate = mod->PrepareReloadCandidate(options, prepareResult);
        const char *modId = mod->GetID();
        if (!candidate) {
            m_Service.PublishReloadResult(modId ? modId : "", m_Pending.Reason, options, prepareResult);
            PublishRejected(mod, prepareResult);
            return false;
        }
        if (m_Pending.Options.DryRun)
            m_Service.PublishReloadResult(modId ? modId : "", m_Pending.Reason, options, prepareResult);
        m_Batch.Candidates.push_back(std::move(candidate));
    }

    return true;
}

void ScriptLibraryReloadOperation::PublishRejected(ScriptMod *failedMod,
                                                   const ScriptModReloadResult &prepareResult) {
    ScriptDevToolsService *devTools = DevTools();
    if (!devTools)
        return;

    const char *modId = failedMod ? failedMod->GetID() : nullptr;
    devTools->PublishEvent(ScriptDevEventSeverity::Warn,
                           "ScriptLibraryReloadRejected",
                           m_Batch.Package,
                           "reload",
                           prepareResult.SourcePath,
                           "Script library reload batch rejected before commit; all old runtimes were kept.",
                           {{"reason", m_Pending.Reason},
                            {"libraries", std::to_string(m_Batch.Packages.size())},
                            {"packages", m_Batch.Package},
                            {"failedMod", modId ? modId : ""},
                            {"consumers", std::to_string(m_Batch.Consumers.size())},
                            {"diagnostic", prepareResult.Diagnostic}});
}

void ScriptLibraryReloadOperation::PublishDryRunPassed() {
    ScriptDevToolsService *devTools = DevTools();
    if (!devTools)
        return;

    devTools->PublishEvent(ScriptDevEventSeverity::Info,
                           "ScriptLibraryReloadDryRunPassed",
                           m_Batch.Package,
                           "reload",
                           "",
                           "Script library reload dry-run batch passed.",
                           {{"reason", m_Pending.Reason},
                            {"libraries", std::to_string(m_Batch.Packages.size())},
                            {"packages", m_Batch.Package},
                            {"consumers", std::to_string(m_Batch.Consumers.size())}});
}

bool ScriptLibraryReloadOperation::CommitBatch() {
    for (size_t i = 0; i < m_Batch.Consumers.size(); ++i) {
        ScriptModReloadResult commitResult = m_Batch.Consumers[i]->CommitReloadCandidate(*m_Batch.Candidates[i], m_Pending.Options);
        const char *modId = m_Batch.Consumers[i]->GetID();
        m_Service.PublishReloadResult(modId ? modId : "", m_Pending.Reason, m_Pending.Options, commitResult);
        if (commitResult.Success) {
            m_Batch.Committed.push_back(i);
            continue;
        }

        bool rollbackOk = true;
        for (auto it = m_Batch.Committed.rbegin(); it != m_Batch.Committed.rend(); ++it) {
            ScriptMod *rollbackMod = m_Batch.Consumers[*it];
            ScriptModReloadResult rollbackResult = rollbackMod->RollbackCommittedCandidate(*m_Batch.Candidates[*it]);
            const char *rollbackId = rollbackMod->GetID();
            ScriptModReloadOptions rollbackOptions = m_Pending.Options;
            rollbackOptions.DryRun = false;
            m_Service.PublishReloadResult(rollbackId ? rollbackId : "",
                                          m_Pending.Reason + " batch rollback",
                                          rollbackOptions,
                                          rollbackResult);
            rollbackOk = rollbackOk && rollbackResult.Success;
            m_Service.RegisterMod(rollbackMod);
        }
        PublishCommitFailure(m_Batch.Consumers[i], commitResult, rollbackOk);
        return false;
    }

    return true;
}

void ScriptLibraryReloadOperation::PublishCommitFailure(ScriptMod *failedMod,
                                                        const ScriptModReloadResult &commitResult,
                                                        bool rollbackOk) {
    ScriptDevToolsService *devTools = DevTools();
    if (!devTools)
        return;

    const char *modId = failedMod ? failedMod->GetID() : nullptr;
    devTools->PublishEvent(rollbackOk ? ScriptDevEventSeverity::Warn : ScriptDevEventSeverity::Error,
                           rollbackOk ? "ScriptLibraryReloadRolledBack" : "ScriptLibraryReloadRestartRequired",
                           m_Batch.Package,
                           "reload",
                           commitResult.SourcePath,
                           rollbackOk
                               ? "Script library reload batch failed during commit; committed consumers were rolled back."
                               : "Script library reload batch failed and at least one committed consumer could not roll back; restart is required.",
                           {{"reason", m_Pending.Reason},
                            {"libraries", std::to_string(m_Batch.Packages.size())},
                            {"packages", m_Batch.Package},
                            {"failedMod", modId ? modId : ""},
                            {"committedBeforeFailure", std::to_string(m_Batch.Committed.size())},
                            {"rollback", rollbackOk ? "success" : "failed"},
                            {"diagnostic", commitResult.Diagnostic}});
}

void ScriptLibraryReloadOperation::PublishCommitted() {
    ScriptDevToolsService *devTools = DevTools();
    if (!devTools)
        return;

    devTools->PublishEvent(ScriptDevEventSeverity::Info,
                           "ScriptLibraryReloadCommitted",
                           m_Batch.Package,
                           "reload",
                           "",
                           "Script library reload batch committed.",
                           {{"reason", m_Pending.Reason},
                            {"libraries", std::to_string(m_Batch.Packages.size())},
                            {"packages", m_Batch.Package},
                            {"consumers", std::to_string(m_Batch.Consumers.size())}});
}

void ScriptLibraryReloadOperation::ClearCoveredAutomaticModReloads() {
    for (ScriptMod *mod : m_Batch.Consumers) {
        const char *modId = mod ? mod->GetID() : nullptr;
        if (!modId || !*modId)
            continue;

        auto pendingIt = m_Service.m_Pending.find(modId);
        if (pendingIt != m_Service.m_Pending.end() && pendingIt->second.Options.Automatic)
            m_Service.m_Pending.erase(pendingIt);
    }
}

bool ScriptLibraryReloadOperation::Run() {
    if (m_Batch.Consumers.empty()) {
        PublishNoConsumers();
        return true;
    }

    const auto now = std::chrono::steady_clock::now();
    if (DeferForManualPending(now))
        return false;

    if (!TryAcquireLeases(now))
        return false;

    PublishStarted();

    if (!CaptureLibrarySources()) {
        m_Batch.DiscardResources();
        return true;
    }

    if (m_Pending.Options.Automatic &&
        !m_Pending.Options.DryRun &&
        CapturedSourcesMatchActiveConsumers()) {
        PublishNoSourceChanges();
        m_Batch.DiscardResources();
        return true;
    }

    if (!PrepareBatch()) {
        m_Batch.DiscardResources();
        return true;
    }

    if (m_Pending.Options.DryRun) {
        m_Batch.DiscardResources();
        PublishDryRunPassed();
        return true;
    }

    if (!CommitBatch()) {
        m_Batch.DiscardResources();
        return true;
    }

    ClearCoveredAutomaticModReloads();
    for (ScriptMod *mod : m_Batch.Consumers)
        m_Service.RegisterMod(mod);
    m_Batch.DiscardResources();
    PublishCommitted();
    return true;
}

bool ScriptModHotReloadService::ProcessLibraryReload(PendingLibraryReload &pending) {
    return ScriptLibraryReloadOperation(*this, pending).Run();
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

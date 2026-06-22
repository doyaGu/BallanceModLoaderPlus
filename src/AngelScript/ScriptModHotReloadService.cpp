#include "ScriptModHotReloadService.h"

#include <algorithm>
#include <sstream>

#include "Logger.h"
#include "ModContext.h"
#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"

namespace BML {

namespace {
constexpr auto kDebounceDelay = std::chrono::milliseconds(500);
constexpr auto kRetryDelay = std::chrono::milliseconds(100);

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

    if (m_WatchingEnabled) {
        const std::vector<ScriptFileWatcherWin32::Event> events = m_Watcher.DrainEvents();
        ScriptModReloadOptions automaticOptions;
        automaticOptions.Automatic = true;
        for (const auto &event : events) {
            for (const auto &record : m_Mods) {
                if (!record.Mod || record.Policy != ScriptModReloadPolicy::Auto)
                    continue;
                if (record.WatchRoot != event.Root)
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
        ScriptMod *mod = FindMod(id);
        if (!mod) {
            m_Pending.erase(pendingIt);
            continue;
        }
        if (!mod->CanHotReloadNow()) {
            pendingIt->second.Due = now + kRetryDelay;
            continue;
        }

        ScriptModReloadResult result = mod->TryHotReload(pending.Options);
        if (result.RetryLater) {
            pendingIt->second.Due = now + kRetryDelay;
            continue;
        }
        m_Pending.erase(pendingIt);

        if (m_Context && m_Context->GetLogger()) {
            if (result.Success) {
                m_Context->GetLogger()->Info("Script mod %s hot reload succeeded.", id.c_str());
            } else {
                m_Context->GetLogger()->Warn("Script mod %s hot reload failed: %s",
                                             id.c_str(),
                                             result.Diagnostic.c_str());
            }
        }
        if (result.Success)
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
    message = "Queued script reload: " + id;
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

bool ScriptModHotReloadService::SetWatchingEnabled(bool enabled) {
    if (m_WatchingEnabled == enabled)
        return true;
    m_WatchingEnabled = enabled;
    RebuildWatches();
    return true;
}

std::string ScriptModHotReloadService::GetStatus() const {
    size_t autoCount = 0;
    for (const auto &record : m_Mods) {
        if (record.Policy == ScriptModReloadPolicy::Auto)
            ++autoCount;
    }
    std::ostringstream stream;
    stream << "script hot reload: mods=" << m_Mods.size()
           << " auto=" << autoCount
           << " pending=" << m_Pending.size()
           << " watch=" << (m_WatchingEnabled ? "on" : "off");
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
    if (!m_Started || !m_WatchingEnabled)
        return;
    for (auto &record : m_Mods) {
        if (!record.Mod)
            continue;
        record.Policy = ResolvePolicy(record.Mod);
        record.WatchRoot = GetWatchRoot(record.Mod);
        if (record.Policy == ScriptModReloadPolicy::Auto && !record.WatchRoot.empty())
            m_Watcher.Watch(record.WatchRoot);
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
    pending.Reason = reason;
}

void ScriptModHotReloadService::QueueReloadDebounced(ScriptMod *mod,
                                                     const ScriptModReloadOptions &options,
                                                     const std::string &reason) {
    if (!mod || !mod->GetID())
        return;
    PendingReload &pending = m_Pending[mod->GetID()];
    pending.Options = options;
    pending.Due = std::chrono::steady_clock::now() + kDebounceDelay;
    pending.Reason = reason;
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

bool ScriptModHotReloadService::EventLooksRelevant(const ScriptFileWatcherWin32::Event &event,
                                                   const ScriptMod *mod) const {
    if (!mod)
        return false;
    if (event.Overflow)
        return true;
    const ScriptModEntry &entry = mod->GetEntry();
    if (entry.SourceKind == ScriptModEntrySourceKind::ZipPackage)
        return EndsWithInsensitive(event.Path, L".zip") && SamePathInsensitive(event.Path, entry.SourcePath);
    return EndsWithInsensitive(event.Path, L".as");
}

} // namespace BML

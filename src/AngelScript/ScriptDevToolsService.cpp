#include "ScriptDevToolsService.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "imgui.h"

#include "ModContext.h"
#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"

namespace BML {

namespace {

enum LogColumn {
    LogColumnSequence = 0,
    LogColumnTime,
    LogColumnLevel,
    LogColumnSource,
    LogColumnAttempt,
    LogColumnTag,
    LogColumnMessage,
    LogColumnCount,
};

struct LogColumnSpec {
    const char *Label;
    ImGuiTableColumnFlags Flags;
    float Width;
};

struct LogFilters {
    int SeverityIndex = 0;
    std::string Tag;
    std::string Source;
    std::string Attempt;
    std::string Text;
    std::string SelectedModId;
    bool SelectedModOnly = false;
    bool ReloadOnly = false;
};

const LogColumnSpec kLogColumns[LogColumnCount] = {
    {"#", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort |
              ImGuiTableColumnFlags_PreferSortDescending,
     52.0f},
    {"time", ImGuiTableColumnFlags_WidthFixed, 92.0f},
    {"level", ImGuiTableColumnFlags_WidthFixed, 64.0f},
    {"source", ImGuiTableColumnFlags_WidthFixed, 145.0f},
    {"attempt", ImGuiTableColumnFlags_WidthFixed, 70.0f},
    {"tag", ImGuiTableColumnFlags_WidthFixed, 185.0f},
    {"message", ImGuiTableColumnFlags_WidthStretch, 0.0f},
};

const char *kLogSeverityFilters[] = {"", "info", "warn", "error"};

uint64_t NowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

std::string FormatTimestamp(uint64_t timestampMs) {
    const std::time_t seconds = static_cast<std::time_t>(timestampMs / 1000);
    std::tm localTime{};
    localtime_s(&localTime, &seconds);
    std::ostringstream stream;
    stream << std::put_time(&localTime, "%H:%M:%S") << '.'
           << std::setw(3) << std::setfill('0') << (timestampMs % 1000);
    return stream.str();
}

std::string VersionToString(const BMLVersion &version) {
    std::ostringstream stream;
    stream << version.major << '.' << version.minor << '.' << version.patch;
    return stream.str();
}

std::string SourceKindToString(ScriptModEntrySourceKind kind) {
    switch (kind) {
    case ScriptModEntrySourceKind::SingleFile:
        return "file";
    case ScriptModEntrySourceKind::ZipPackage:
        return "zip";
    case ScriptModEntrySourceKind::Directory:
    default:
        return "directory";
    }
}

std::string EffectiveReloadPolicy(const ScriptMod *mod) {
    if (!mod)
        return "manual";
    ScriptModReloadPolicy policy = ParseScriptModReloadPolicy(mod->GetDefinition().ReloadPolicy);
    if (policy == ScriptModReloadPolicy::Default) {
        policy = mod->GetEntry().SourceKind == ScriptModEntrySourceKind::ZipPackage
                     ? ScriptModReloadPolicy::Manual
                     : ScriptModReloadPolicy::Auto;
    }
    return ToString(policy);
}

std::string SeverityToLower(ScriptDevEventSeverity severity) {
    switch (severity) {
    case ScriptDevEventSeverity::Warn:
        return "warn";
    case ScriptDevEventSeverity::Error:
        return "error";
    case ScriptDevEventSeverity::Info:
    default:
        return "info";
    }
}

bool SeverityMatches(const ScriptDevEvent &event, const std::string &severity) {
    if (severity.empty())
        return true;
    return SeverityToLower(event.Severity) == utils::ToLower(severity);
}

bool ContainsInsensitive(const std::string &text, const std::string &needle) {
    if (needle.empty())
        return true;
    return utils::ToLower(text).find(utils::ToLower(needle)) != std::string::npos;
}

std::string JoinFields(const std::vector<ScriptDevEventField> &fields) {
    std::ostringstream stream;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i != 0)
            stream << ' ';
        stream << fields[i].Key << '=' << fields[i].Value;
    }
    return stream.str();
}

std::string LogSource(const ScriptDevEvent &event) {
    return event.ModId.empty() ? event.SourcePath : event.ModId;
}

std::string LogSearchText(const ScriptDevEvent &event) {
    return event.Code + " " + event.ModId + " " + event.Phase + " " +
           (event.ReloadAttemptId == 0 ? "" : std::to_string(event.ReloadAttemptId)) + " " +
           event.SourcePath + " " + event.Message + " " + JoinFields(event.Fields);
}

bool IsReloadLog(const ScriptDevEvent &event) {
    return event.Phase == "reload" || event.Code.rfind("ScriptReload", 0) == 0;
}

bool LogMatchesFilters(const ScriptDevEvent &event, const LogFilters &filters) {
    if (!SeverityMatches(event, kLogSeverityFilters[filters.SeverityIndex]))
        return false;
    if (filters.SelectedModOnly && !filters.SelectedModId.empty() && event.ModId != filters.SelectedModId)
        return false;
    if (filters.ReloadOnly && !IsReloadLog(event))
        return false;
    if (!ContainsInsensitive(event.Code, filters.Tag))
        return false;

    const std::string sourceHaystack = LogSource(event) + " " + event.SourcePath;
    if (!ContainsInsensitive(sourceHaystack, filters.Source))
        return false;
    if (!filters.Attempt.empty() &&
        !ContainsInsensitive(event.ReloadAttemptId == 0 ? "" : std::to_string(event.ReloadAttemptId),
                             filters.Attempt)) {
        return false;
    }

    return ContainsInsensitive(LogSearchText(event), filters.Text);
}

int CompareText(const std::string &left, const std::string &right) {
    if (left < right)
        return -1;
    if (right < left)
        return 1;
    return 0;
}

int CompareLogs(const ScriptDevEvent &left, const ScriptDevEvent &right, int column) {
    switch (column) {
    case LogColumnSequence:
        return left.Sequence < right.Sequence ? -1 : (right.Sequence < left.Sequence ? 1 : 0);
    case LogColumnTime:
        return left.TimestampMs < right.TimestampMs ? -1 : (right.TimestampMs < left.TimestampMs ? 1 : 0);
    case LogColumnLevel:
        return static_cast<int>(left.Severity) - static_cast<int>(right.Severity);
    case LogColumnSource:
        return CompareText(LogSource(left), LogSource(right));
    case LogColumnAttempt:
        return left.ReloadAttemptId < right.ReloadAttemptId ? -1 : (right.ReloadAttemptId < left.ReloadAttemptId ? 1 : 0);
    case LogColumnTag:
        return CompareText(left.Code, right.Code);
    case LogColumnMessage:
        return CompareText(left.Message, right.Message);
    default:
        return 0;
    }
}

int LogColumnFromSortSpec(const ImGuiTableColumnSortSpecs &spec) {
    if (spec.ColumnUserID > 0) {
        const int column = static_cast<int>(spec.ColumnUserID - 1);
        if (column >= 0 && column < LogColumnCount)
            return column;
    }
    if (spec.ColumnIndex >= 0 && spec.ColumnIndex < LogColumnCount)
        return spec.ColumnIndex;
    return LogColumnSequence;
}

void SortLogs(std::vector<ScriptDevEvent> &logs, const ImGuiTableSortSpecs *sortSpecs) {
    if (!sortSpecs || sortSpecs->SpecsCount <= 0)
        return;

    std::stable_sort(logs.begin(), logs.end(), [sortSpecs](const ScriptDevEvent &left, const ScriptDevEvent &right) {
        for (int i = 0; i < sortSpecs->SpecsCount; ++i) {
            const ImGuiTableColumnSortSpecs &spec = sortSpecs->Specs[i];
            int cmp = CompareLogs(left, right, LogColumnFromSortSpec(spec));
            if (cmp == 0)
                continue;
            if (spec.SortDirection == ImGuiSortDirection_Descending)
                cmp = -cmp;
            return cmp < 0;
        }
        return left.Sequence < right.Sequence;
    });
}

ImGuiTableColumnFlags LogColumnFlags(int column, const bool *visible) {
    return kLogColumns[column].Flags | (visible[column] ? 0 : ImGuiTableColumnFlags_Disabled);
}

void SetupLogColumns(const bool *visible) {
    for (int column = 0; column < LogColumnCount; ++column)
        ImGui::TableSetupColumn(kLogColumns[column].Label,
                                LogColumnFlags(column, visible),
                                kLogColumns[column].Width,
                                static_cast<ImGuiID>(column + 1));
}

std::string LogCellText(const ScriptDevEvent &event, int column) {
    switch (column) {
    case LogColumnSequence:
        return std::to_string(event.Sequence);
    case LogColumnTime:
        return FormatTimestamp(event.TimestampMs);
    case LogColumnLevel:
        return ToString(event.Severity);
    case LogColumnSource:
        return LogSource(event);
    case LogColumnAttempt:
        return event.ReloadAttemptId == 0 ? "" : std::to_string(event.ReloadAttemptId);
    case LogColumnTag:
        return event.Code;
    case LogColumnMessage:
        return event.Message;
    default:
        return {};
    }
}

ImVec4 SeverityColor(ScriptDevEventSeverity severity) {
    switch (severity) {
    case ScriptDevEventSeverity::Warn:
        return ImVec4(0.92f, 0.76f, 0.34f, 1.0f);
    case ScriptDevEventSeverity::Error:
        return ImVec4(0.92f, 0.36f, 0.34f, 1.0f);
    case ScriptDevEventSeverity::Info:
    default:
        return ImVec4(0.78f, 0.82f, 0.86f, 1.0f);
    }
}

ImVec4 StateColor(const std::string &state) {
    if (state == "failed")
        return ImVec4(0.92f, 0.36f, 0.34f, 1.0f);
    if (state == "reloading")
        return ImVec4(0.50f, 0.62f, 0.70f, 1.0f);
    if (state == "loaded")
        return ImVec4(0.48f, 0.74f, 0.48f, 1.0f);
    return ImVec4(0.72f, 0.72f, 0.72f, 1.0f);
}

} // namespace

const char *ToString(ScriptDevEventSeverity severity) {
    switch (severity) {
    case ScriptDevEventSeverity::Warn:
        return "WARN";
    case ScriptDevEventSeverity::Error:
        return "ERROR";
    case ScriptDevEventSeverity::Info:
    default:
        return "INFO";
    }
}

ScriptDevEventSeverity ScriptDevEventSeverityFromLogLevel(const char *level) {
    if (!level)
        return ScriptDevEventSeverity::Info;
    const std::string normalized = utils::ToLower(std::string(level));
    if (normalized == "error")
        return ScriptDevEventSeverity::Error;
    if (normalized == "warn" || normalized == "warning")
        return ScriptDevEventSeverity::Warn;
    return ScriptDevEventSeverity::Info;
}

ScriptDevToolsService::ScriptDevToolsService(ModContext *context)
    : Bui::Window("Script Developer Tools"),
      m_Context(context),
      m_Events(kEventCapacity) {
    m_Visible = false;
}

void ScriptDevToolsService::PublishEvent(ScriptDevEventSeverity severity,
                                         const std::string &code,
                                         const std::string &modId,
                                         const std::string &phase,
                                         const std::string &sourcePath,
                                         const std::string &message,
                                         const std::vector<ScriptDevEventField> &fields,
                                         unsigned int reloadAttemptId) {
    ScriptDevEvent event;
    event.TimestampMs = NowMs();
    event.ReloadAttemptId = reloadAttemptId;
    event.Severity = severity;
    event.Code = code;
    event.ModId = modId;
    event.Phase = phase;
    event.SourcePath = sourcePath;
    event.Message = message;
    event.Fields = fields;

    std::lock_guard<std::mutex> lock(m_EventMutex);
    event.Sequence = m_NextEventSequence++;
    if (m_EventCount < kEventCapacity) {
        const size_t index = (m_EventStart + m_EventCount) % kEventCapacity;
        m_Events[index] = std::move(event);
        ++m_EventCount;
    } else {
        m_Events[m_EventStart] = std::move(event);
        m_EventStart = (m_EventStart + 1) % kEventCapacity;
        ++m_DroppedEvents;
    }
    ++m_EventGeneration;
}

void ScriptDevToolsService::PublishDiagnostic(ScriptDevEventSeverity severity,
                                              const std::string &code,
                                              const std::string &modId,
                                              const ScriptDiagnostic &diagnostic,
                                              unsigned int reloadAttemptId) {
    std::vector<ScriptDevEventField> fields = {
        {"status", std::to_string(static_cast<int>(diagnostic.Status))},
        {"asCode", std::to_string(diagnostic.AngelScriptCode)},
        {"hasStack", diagnostic.StackTrace.empty() ? "false" : "true"},
    };
    if (!diagnostic.StackTrace.empty())
        fields.push_back({"stack", diagnostic.StackTrace});

    PublishEvent(severity,
                 code,
                 modId,
                 ScriptDiagnosticPhaseName(diagnostic.Phase),
                 diagnostic.EntryPath,
                 diagnostic.Message,
                 fields,
                 reloadAttemptId);
}

void ScriptDevToolsService::PublishLogLine(const char *level, const char *source, const std::string &message) {
    PublishEvent(ScriptDevEventSeverityFromLogLevel(level),
                 "LogLine",
                 source ? source : "",
                 "log",
                 "",
                 message,
                 {{"level", level ? level : ""}});
}

void ScriptDevToolsService::EnqueueAction(const ScriptDevAction &action) {
    {
        std::lock_guard<std::mutex> lock(m_ActionMutex);
        m_Actions.push_back(action);
    }
    std::string target = action.ModId.empty() ? "all" : action.ModId;
    PublishEvent(ScriptDevEventSeverity::Info,
                 "ScriptActionQueued",
                 target,
                 "action",
                 "",
                 "Script action queued.",
                 {{"kind", std::to_string(static_cast<int>(action.Kind))}});
}

void ScriptDevToolsService::ProcessActions() {
    std::deque<ScriptDevAction> actions;
    {
        std::lock_guard<std::mutex> lock(m_ActionMutex);
        actions.swap(m_Actions);
    }

    for (const ScriptDevAction &action : actions)
        ExecuteAction(action);
    if (!actions.empty())
        RefreshSnapshotsIfNeeded(true);
}

void ScriptDevToolsService::ExecuteAction(const ScriptDevAction &action) {
    switch (action.Kind) {
    case ScriptDevActionKind::Reload:
        ExecuteReloadAction(action);
        break;
    case ScriptDevActionKind::ReloadAll:
        ExecuteReloadAllAction(action);
        break;
    case ScriptDevActionKind::Watch:
        if (m_Context) {
            m_Context->SetScriptHotReloadWatching(action.WatchEnabled);
            PublishEvent(ScriptDevEventSeverity::Info,
                         "ScriptWatchChanged",
                         "",
                         "watch",
                         "",
                         action.WatchEnabled ? "Script watch enabled." : "Script watch disabled.");
        }
        break;
    case ScriptDevActionKind::ClearEvents:
        ClearEvents();
        PublishEvent(ScriptDevEventSeverity::Info, "ScriptLogsCleared", "", "logs", "", "Script logs cleared.");
        break;
    case ScriptDevActionKind::TogglePanel:
        Toggle();
        PublishEvent(ScriptDevEventSeverity::Info,
                     IsVisible() ? "ScriptPanelOpened" : "ScriptPanelClosed",
                     "",
                     "panel",
                     "",
                     IsVisible() ? "Script developer panel opened." : "Script developer panel closed.");
        break;
    }
}

void ScriptDevToolsService::ExecuteReloadAction(const ScriptDevAction &action) {
    if (!m_Context)
        return;
    std::string message;
    const bool queued = m_Context->QueueScriptModReload(action.ModId, action.ReloadOptions, message);
    if (!queued) {
        PublishEvent(ScriptDevEventSeverity::Warn,
                     action.ReloadOptions.DryRun ? "ScriptReloadDryRunRejected" : "ScriptReloadRejected",
                     action.ModId,
                     "reload",
                     "",
                     message,
                     {{"dryRun", action.ReloadOptions.DryRun ? "true" : "false"},
                      {"forceExports", action.ReloadOptions.ForceExports ? "true" : "false"}});
    }
}

void ScriptDevToolsService::ExecuteReloadAllAction(const ScriptDevAction &action) {
    if (!m_Context)
        return;

    const size_t count = m_Context->QueueAllScriptModReloads(action.ReloadOptions);
    PublishEvent(ScriptDevEventSeverity::Info,
                 action.ReloadOptions.DryRun ? "ScriptReloadDryRunAllQueued" : "ScriptReloadAllQueued",
                 "all",
                 "reload",
                 "",
                 action.ReloadOptions.DryRun ? "Queued script reload dry-runs." : "Queued script reloads.",
                 {{"count", std::to_string(count)},
                  {"dryRun", action.ReloadOptions.DryRun ? "true" : "false"},
                  {"forceExports", action.ReloadOptions.ForceExports ? "true" : "false"}});
}

void ScriptDevToolsService::ClearEvents() {
    std::lock_guard<std::mutex> lock(m_EventMutex);
    m_EventStart = 0;
    m_EventCount = 0;
    m_DroppedEvents = 0;
    ++m_EventGeneration;
}

ScriptMod *ScriptDevToolsService::FindScriptMod(const std::string &id) const {
    if (!m_Context)
        return nullptr;
    return dynamic_cast<ScriptMod *>(m_Context->FindMod(id.c_str()));
}

void ScriptDevToolsService::RefreshSnapshotsIfNeeded(bool force) {
    if (!m_Context)
        return;

    uint64_t observed = static_cast<uint64_t>(m_Context->GetModCount());
    auto mixObserved = [&](uint64_t value) {
        observed = observed * 1315423911u + value + 0x9e3779b97f4a7c15ull;
    };
    for (int i = 0; i < m_Context->GetModCount(); ++i) {
        auto *mod = dynamic_cast<ScriptMod *>(m_Context->GetMod(i));
        if (!mod)
            continue;
        mixObserved(mod->GetModGeneration());
        mixObserved(mod->GetRuntimeGeneration());
        mixObserved(mod->GetReloadAttemptId());
        mixObserved(mod->GetActiveTimerCount());
        mixObserved(mod->GetActiveCommandCount());
        mixObserved(mod->GetActiveDataShareRequestCount());
        mixObserved(mod->GetHostRegistrationCount());
        mixObserved(static_cast<uint64_t>(std::max(0, mod->GetActiveScriptCallCount())));
    }

    if (!force && observed == m_LastObservedGeneration)
        return;

    std::vector<ScriptModSnapshot> snapshots;
    for (int i = 0; i < m_Context->GetModCount(); ++i) {
        auto *mod = dynamic_cast<ScriptMod *>(m_Context->GetMod(i));
        if (mod)
            snapshots.push_back(BuildSnapshot(mod));
    }
    std::sort(snapshots.begin(), snapshots.end(), [](const ScriptModSnapshot &left, const ScriptModSnapshot &right) {
        return left.Id < right.Id;
    });

    m_Snapshots = std::move(snapshots);
    m_LastObservedGeneration = observed;
    ++m_SnapshotGeneration;

    if (m_SelectedModId.empty() && !m_Snapshots.empty())
        m_SelectedModId = m_Snapshots.front().Id;
}

ScriptModSnapshot ScriptDevToolsService::BuildSnapshot(ScriptMod *mod) const {
    ScriptModSnapshot snapshot;
    if (!mod)
        return snapshot;

    const ScriptModDefinition &definition = mod->GetDefinition();
    const ScriptModEntry &entry = mod->GetEntry();
    snapshot.Id = mod->GetID() ? mod->GetID() : definition.Id;
    snapshot.Name = mod->GetName() ? mod->GetName() : definition.Name;
    snapshot.Version = mod->GetVersion() ? mod->GetVersion() : definition.Version;
    snapshot.State = mod->IsReloading() ? "reloading" : (mod->IsFailed() ? "failed" : (mod->IsLoaded() ? "loaded" : "registered"));
    snapshot.ReloadPolicy = EffectiveReloadPolicy(mod);
    snapshot.SourceKind = SourceKindToString(entry.SourceKind);
    snapshot.SourcePath = utils::Utf16ToUtf8(entry.SourcePath.empty() ? entry.RootDirectory : entry.SourcePath);
    snapshot.EntryPath = utils::Utf16ToUtf8(entry.EntryPath);
    snapshot.LastDiagnostic = mod->GetLastDiagnostic();
    snapshot.LastReloadDiagnostic = mod->GetLastReloadDiagnostic();
    snapshot.ModGeneration = mod->GetModGeneration();
    snapshot.RuntimeGeneration = mod->GetRuntimeGeneration();
    snapshot.ReloadAttemptId = mod->GetReloadAttemptId();

    for (const auto &dependency : definition.Dependencies) {
        ScriptDependencySnapshot item;
        item.Id = dependency.Id;
        item.MinVersion = VersionToString(dependency.MinVersion);
        item.Optional = dependency.Optional;
        IMod *target = m_Context ? m_Context->FindMod(dependency.Id.c_str()) : nullptr;
        item.Satisfied = dependency.Optional;
        if (!dependency.Optional && target && !IsFailedScriptMod(target)) {
            item.Satisfied = ParseBmlVersion(target->GetVersion() ? target->GetVersion() : "0.0.0") >=
                             dependency.MinVersion;
        }
        snapshot.Dependencies.push_back(std::move(item));
    }

    const int exportCount = mod->GetExportCount();
    for (int i = 0; i < exportCount; ++i) {
        ScriptExportSnapshot item;
        if (mod->GetExportInfo(i, item.Name, item.Signature))
            snapshot.Exports.push_back(std::move(item));
    }

    mod->GetCallbackNames(snapshot.Callbacks);
    snapshot.Resources.Timers = mod->GetActiveTimerCount();
    snapshot.Resources.Commands = mod->GetActiveCommandCount();
    snapshot.Resources.DataShareRequests = mod->GetActiveDataShareRequestCount();
    snapshot.Resources.HostRegistrations = mod->GetHostRegistrationCount();
    snapshot.Resources.Callbacks = snapshot.Callbacks.size();
    snapshot.Resources.Exports = snapshot.Exports.size();
    snapshot.Resources.ActiveCalls = mod->GetActiveScriptCallCount();
    return snapshot;
}

std::vector<ScriptModSnapshot> ScriptDevToolsService::GetModSnapshots(bool force) {
    RefreshSnapshotsIfNeeded(force);
    return m_Snapshots;
}

std::vector<std::string> ScriptDevToolsService::GetScriptModIds() {
    RefreshSnapshotsIfNeeded(false);
    std::vector<std::string> ids;
    ids.reserve(m_Snapshots.size());
    for (const auto &snapshot : m_Snapshots)
        ids.push_back(snapshot.Id);
    return ids;
}

std::vector<ScriptDevEvent> ScriptDevToolsService::GetEventSnapshot() const {
    std::vector<ScriptDevEvent> events;
    std::lock_guard<std::mutex> lock(m_EventMutex);
    events.reserve(m_EventCount);
    for (size_t i = 0; i < m_EventCount; ++i)
        events.push_back(m_Events[(m_EventStart + i) % kEventCapacity]);
    return events;
}

uint64_t ScriptDevToolsService::GetDroppedEventCount() const {
    std::lock_guard<std::mutex> lock(m_EventMutex);
    return m_DroppedEvents;
}

ScriptDevStatusSnapshot ScriptDevToolsService::GetStatusSnapshot() {
    RefreshSnapshotsIfNeeded(false);
    ScriptDevStatusSnapshot status;
    status.HotReloadStatus = m_Context ? m_Context->GetScriptHotReloadStatus() : "script hot reload: unavailable";
    status.ModCount = m_Snapshots.size();
    for (const auto &snapshot : m_Snapshots) {
        if (snapshot.State == "loaded")
            ++status.LoadedCount;
        else if (snapshot.State == "failed")
            ++status.FailedCount;
        else if (snapshot.State == "reloading")
            ++status.ReloadingCount;
    }
    {
        std::lock_guard<std::mutex> lock(m_EventMutex);
        status.DroppedEvents = m_DroppedEvents;
        status.EventCount = m_EventCount;
    }
    return status;
}

std::string ScriptDevToolsService::FormatStatus() {
    const ScriptDevStatusSnapshot status = GetStatusSnapshot();
    std::ostringstream stream;
    stream << status.HotReloadStatus
           << " loaded=" << status.LoadedCount
           << " failed=" << status.FailedCount
           << " reloading=" << status.ReloadingCount
           << " logs=" << status.EventCount
           << " dropped=" << status.DroppedEvents;
    return stream.str();
}

std::vector<std::string> ScriptDevToolsService::FormatLogs(const std::string &severity) {
    std::vector<ScriptDevEvent> events = GetEventSnapshot();
    std::vector<std::string> lines;
    lines.push_back("Recent script logs:");
    size_t emitted = 0;
    for (auto it = events.rbegin(); it != events.rend() && emitted < 25; ++it) {
        if (!SeverityMatches(*it, severity))
            continue;
        std::ostringstream stream;
        stream << '#' << it->Sequence << ' ' << FormatTimestamp(it->TimestampMs)
               << ' ' << ToString(it->Severity)
               << ' ' << it->Code;
        if (!it->ModId.empty())
            stream << " mod=" << it->ModId;
        if (!it->Phase.empty())
            stream << " phase=" << it->Phase;
        if (it->ReloadAttemptId != 0)
            stream << " attempt=" << it->ReloadAttemptId;
        if (!it->SourcePath.empty())
            stream << " source=" << it->SourcePath;
        stream << " - " << it->Message;
        lines.push_back(stream.str());
        ++emitted;
    }
    if (emitted == 0)
        lines.push_back("  none");
    return lines;
}

std::vector<std::string> ScriptDevToolsService::FormatList() {
    RefreshSnapshotsIfNeeded(false);
    std::vector<std::string> lines;
    lines.push_back("Script mods:");
    for (const auto &snapshot : m_Snapshots) {
        std::ostringstream stream;
        stream << "  " << snapshot.Id << " [" << snapshot.State << "]"
               << " reload=" << snapshot.ReloadPolicy
               << " source=" << snapshot.SourceKind
               << " v" << snapshot.Version;
        lines.push_back(stream.str());
    }
    if (m_Snapshots.empty())
        lines.push_back("  none");
    return lines;
}

const ScriptModSnapshot *ScriptDevToolsService::FindSnapshot(const std::string &id) {
    RefreshSnapshotsIfNeeded(false);
    auto it = std::find_if(m_Snapshots.begin(), m_Snapshots.end(), [&](const ScriptModSnapshot &snapshot) {
        return snapshot.Id == id;
    });
    return it == m_Snapshots.end() ? nullptr : &*it;
}

std::vector<std::string> ScriptDevToolsService::FormatInfo(const std::string &id) {
    const ScriptModSnapshot *snapshot = FindSnapshot(id);
    if (!snapshot)
        return {"Script mod not found: " + id};
    return {
        "Script mod " + snapshot->Id,
        "  name=" + snapshot->Name + " version=" + snapshot->Version + " state=" + snapshot->State,
        "  reload=" + snapshot->ReloadPolicy + " source=" + snapshot->SourceKind,
        "  entry=" + snapshot->EntryPath,
        "  modGeneration=" + std::to_string(snapshot->ModGeneration) +
            " runtimeGeneration=" + std::to_string(snapshot->RuntimeGeneration) +
            " reloadAttemptId=" + std::to_string(snapshot->ReloadAttemptId),
    };
}

std::vector<std::string> ScriptDevToolsService::FormatDiag(const std::string &id) {
    const ScriptModSnapshot *snapshot = FindSnapshot(id);
    if (!snapshot)
        return {"Script mod not found: " + id};
    std::vector<std::string> lines;
    lines.push_back("Diagnostics for " + snapshot->Id + ":");
    lines.push_back("  state=" + snapshot->State);
    lines.push_back("  last=" + (snapshot->LastDiagnostic.empty() ? "<none>" : snapshot->LastDiagnostic));
    lines.push_back("  reload=" + (snapshot->LastReloadDiagnostic.empty() ? "<none>" : snapshot->LastReloadDiagnostic));
    lines.push_back("  source=" + snapshot->EntryPath);
    return lines;
}

std::vector<std::string> ScriptDevToolsService::FormatDeps(const std::string &id) {
    const ScriptModSnapshot *snapshot = FindSnapshot(id);
    if (!snapshot)
        return {"Script mod not found: " + id};
    std::vector<std::string> lines;
    lines.push_back("Dependencies for " + snapshot->Id + ":");
    for (const auto &dependency : snapshot->Dependencies) {
        lines.push_back("  " + dependency.Id + " >= " + dependency.MinVersion +
                        (dependency.Optional ? " optional" : " required") +
                        (dependency.Satisfied ? " ok" : " missing"));
    }
    if (snapshot->Dependencies.empty())
        lines.push_back("  none");
    return lines;
}

std::vector<std::string> ScriptDevToolsService::FormatExports(const std::string &id) {
    const ScriptModSnapshot *snapshot = FindSnapshot(id);
    if (!snapshot)
        return {"Script mod not found: " + id};
    std::vector<std::string> lines;
    lines.push_back("Exports for " + snapshot->Id + ":");
    for (const auto &item : snapshot->Exports)
        lines.push_back("  " + item.Name + " : " + item.Signature);
    if (snapshot->Exports.empty())
        lines.push_back("  none");
    return lines;
}

std::vector<std::string> ScriptDevToolsService::FormatResources(const std::string &id) {
    const ScriptModSnapshot *snapshot = FindSnapshot(id);
    if (!snapshot)
        return {"Script mod not found: " + id};
    const ScriptResourceSnapshot &resources = snapshot->Resources;
    return {
        "Resources for " + snapshot->Id + ":",
        "  timers=" + std::to_string(resources.Timers) +
            " commands=" + std::to_string(resources.Commands) +
            " datashare=" + std::to_string(resources.DataShareRequests),
        "  callbacks=" + std::to_string(resources.Callbacks) +
            " exports=" + std::to_string(resources.Exports) +
            " hostRegistrations=" + std::to_string(resources.HostRegistrations),
        "  activeCalls=" + std::to_string(resources.ActiveCalls),
    };
}

std::vector<std::string> ScriptDevToolsService::HandleCommand(const std::vector<std::string> &args) {
    if (args.size() < 2 || args[1] == "status")
        return {FormatStatus()};

    const std::string &command = args[1];
    if (command == "panel") {
        ScriptDevAction action;
        action.Kind = ScriptDevActionKind::TogglePanel;
        EnqueueAction(action);
        return {"Queued script developer panel toggle."};
    }
    if (command == "logs" || command == "log") {
        if (args.size() >= 3 && args[2] == "clear") {
            ScriptDevAction action;
            action.Kind = ScriptDevActionKind::ClearEvents;
            EnqueueAction(action);
            return {"Queued script log clear."};
        }
        return FormatLogs(args.size() >= 3 ? args[2] : "");
    }
    if (command == "list")
        return FormatList();
    if (command == "info" || command == "diag" || command == "deps" || command == "exports" || command == "resources") {
        if (args.size() < 3)
            return {"Usage: script " + command + " <id>"};
        if (command == "info")
            return FormatInfo(args[2]);
        if (command == "diag")
            return FormatDiag(args[2]);
        if (command == "deps")
            return FormatDeps(args[2]);
        if (command == "exports")
            return FormatExports(args[2]);
        return FormatResources(args[2]);
    }
    if (command == "reload") {
        ScriptModReloadOptions options;
        std::string target = "all";
        size_t optionStart = 2;
        if (args.size() >= 3 && !args[2].empty() && args[2][0] != '-') {
            target = args[2];
            optionStart = 3;
        }
        for (size_t i = optionStart; i < args.size(); ++i) {
            if (args[i] == "--dry-run")
                options.DryRun = true;
            else if (args[i] == "--force-exports")
                options.ForceExports = true;
        }
        ScriptDevAction action;
        action.Kind = target == "all" ? ScriptDevActionKind::ReloadAll : ScriptDevActionKind::Reload;
        action.ModId = target == "all" ? "" : target;
        action.ReloadOptions = options;
        EnqueueAction(action);
        return {options.DryRun ? "Queued script reload dry-run request." : "Queued script reload request."};
    }
    if (command == "watch" || command == "auto") {
        if (args.size() != 3 || (args[2] != "on" && args[2] != "off"))
            return {"Usage: script watch <on|off>"};
        ScriptDevAction action;
        action.Kind = ScriptDevActionKind::Watch;
        action.WatchEnabled = args[2] == "on";
        EnqueueAction(action);
        return {action.WatchEnabled ? "Queued script watch on." : "Queued script watch off."};
    }

    return {"Usage: script <status|panel|logs|list|info|diag|deps|exports|resources|reload|watch>"};
}

std::vector<std::string> ScriptDevToolsService::CompleteCommand(const std::vector<std::string> &args) {
    if (args.size() == 2)
        return {"status", "panel", "logs", "list", "info", "diag", "deps", "exports", "resources", "reload", "watch"};
    if (args.size() == 3 && (args[1] == "logs" || args[1] == "log"))
        return {"info", "warn", "error", "clear"};
    if (args.size() == 3 && (args[1] == "info" || args[1] == "diag" || args[1] == "deps" ||
                             args[1] == "exports" || args[1] == "resources")) {
        return GetScriptModIds();
    }
    if (args.size() == 3 && args[1] == "reload") {
        std::vector<std::string> ids = {"all"};
        std::vector<std::string> modIds = GetScriptModIds();
        ids.insert(ids.end(), modIds.begin(), modIds.end());
        return ids;
    }
    if (args.size() >= 4 && args[1] == "reload")
        return {"--dry-run", "--force-exports"};
    if (args.size() == 3 && (args[1] == "watch" || args[1] == "auto"))
        return {"on", "off"};
    return {};
}

void ScriptDevToolsService::RenderPanel() {
    Render();
}

ImGuiWindowFlags ScriptDevToolsService::GetFlags() {
    return ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
}

void ScriptDevToolsService::OnPreBegin() {
    BlockGameInput();

    const ImVec2 viewportSize = ImGui::GetMainViewport()->Size;
    ImGui::SetNextWindowPos(ImVec2(viewportSize.x * 0.06f, viewportSize.y * 0.11f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(viewportSize.x * 0.88f, viewportSize.y * 0.78f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(720.0f, 420.0f), ImGui::GetMainViewport()->Size);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(7.0f, 5.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.050f, 0.052f, 0.056f, 0.985f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.060f, 0.064f, 0.068f, 0.970f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.055f, 0.058f, 0.062f, 0.990f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.095f, 0.104f, 0.112f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.120f, 0.135f, 0.145f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ImVec4(0.095f, 0.104f, 0.112f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.45f, 0.48f, 0.50f, 0.48f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.13f, 0.14f, 0.15f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.20f, 0.22f, 0.24f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.20f, 0.22f, 0.24f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.28f, 0.31f, 0.33f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.20f, 0.22f, 0.94f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.29f, 0.31f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.15f, 0.16f, 0.17f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.28f, 0.31f, 0.33f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_TabSelected, ImVec4(0.23f, 0.25f, 0.27f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.070f, 0.074f, 0.078f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.105f, 0.110f, 0.116f, 0.92f));
}

void ScriptDevToolsService::OnPostBegin() {
    ImGui::SetWindowFontScale(0.82f);
}

void ScriptDevToolsService::OnPostEnd() {
    ImGui::PopStyleColor(18);
    ImGui::PopStyleVar(4);
}

void ScriptDevToolsService::OnShow() {
    BlockGameInput();
}

void ScriptDevToolsService::OnHide() {
    UnblockGameInput();
}

void ScriptDevToolsService::BlockGameInput() {
    if (m_InputBlockTokenActive)
        return;
    if (!m_Context || !m_Context->GetInputManager())
        return;

    InputHook *input = m_Context->GetInputManager();
    input->Block(CK_INPUT_DEVICE_KEYBOARD);
    input->Block(CK_INPUT_DEVICE_MOUSE);
    input->Block(CK_INPUT_DEVICE_JOYSTICK);
    m_InputBlockTokenActive = true;
}

void ScriptDevToolsService::UnblockGameInput() {
    if (!m_InputBlockTokenActive)
        return;
    if (!m_Context || !m_Context->GetInputManager()) {
        m_InputBlockTokenActive = false;
        return;
    }

    InputHook *input = m_Context->GetInputManager();
    input->Unblock(CK_INPUT_DEVICE_JOYSTICK);
    input->Unblock(CK_INPUT_DEVICE_MOUSE);
    input->Unblock(CK_INPUT_DEVICE_KEYBOARD);
    m_InputBlockTokenActive = false;
}

void ScriptDevToolsService::OnDraw() {
    RefreshSnapshotsIfNeeded(false);
    if (m_SelectedModId.empty() && !m_Snapshots.empty())
        m_SelectedModId = m_Snapshots.front().Id;

    DrawStatusBar();
    ImGui::Separator();

    const float bottomHeight = ImGui::GetFrameHeightWithSpacing() + 8.0f;
    const float contentWidth = ImGui::GetContentRegionAvail().x;
    const float listWidth = std::min(320.0f, std::max(235.0f, contentWidth * 0.22f));
    if (ImGui::BeginChild("script-dev-mods", ImVec2(listWidth, -bottomHeight), true))
        DrawModList();
    ImGui::EndChild();

    ImGui::SameLine();
    const ScriptModSnapshot *selected = FindSnapshot(m_SelectedModId);
    if (ImGui::BeginChild("script-dev-details", ImVec2(0, -bottomHeight), true)) {
        if (ImGui::BeginTabBar("script-dev-tabs")) {
            if (ImGui::BeginTabItem("Diag")) {
                DrawDiagnosticsTab(selected);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Reload")) {
                DrawReloadTab(selected);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Res")) {
                DrawResourcesTab(selected);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Exports")) {
                DrawExportsTab(selected);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Deps")) {
                DrawDependenciesTab(selected);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Logs")) {
                DrawLogsTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::EndChild();

    DrawBottomBar(selected);
}

void ScriptDevToolsService::DrawStatusBar() {
    const ScriptDevStatusSnapshot status = GetStatusSnapshot();
    ImGui::TextColored(ImVec4(0.70f, 0.77f, 0.82f, 1.0f), "%s", status.HotReloadStatus.c_str());
    ImGui::Text("loaded %zu", status.LoadedCount);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.92f, 0.36f, 0.34f, 1.0f), "failed %zu", status.FailedCount);
    ImGui::SameLine();
    ImGui::Text("logs %llu", static_cast<unsigned long long>(status.EventCount));
    ImGui::SameLine();
    ImGui::Text("dropped %llu", static_cast<unsigned long long>(status.DroppedEvents));
}

void ScriptDevToolsService::DrawModList() {
    if (ImGui::BeginTable("script-dev-mod-table", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("state", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableSetupColumn("src", ImGuiTableColumnFlags_WidthFixed, 54.0f);
        ImGui::TableHeadersRow();
        for (const auto &snapshot : m_Snapshots) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const bool selected = snapshot.Id == m_SelectedModId;
            if (ImGui::Selectable(snapshot.Id.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
                m_SelectedModId = snapshot.Id;
            ImGui::TableNextColumn();
            ImGui::TextColored(StateColor(snapshot.State), "%s", snapshot.State.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(snapshot.SourceKind.c_str());
        }
        ImGui::EndTable();
    }
}

void ScriptDevToolsService::DrawDiagnosticsTab(const ScriptModSnapshot *selected) {
    if (!selected) {
        ImGui::TextUnformatted("No script mod selected.");
        return;
    }
    ImGui::Text("%s", selected->Name.c_str());
    ImGui::SameLine();
    ImGui::TextColored(StateColor(selected->State), "%s", selected->State.c_str());
    ImGui::Text("id %s", selected->Id.c_str());
    ImGui::Text("version %s", selected->Version.c_str());
    ImGui::Text("entry %s", selected->EntryPath.c_str());
    ImGui::Separator();
    ImGui::TextWrapped("last diagnostic: %s", selected->LastDiagnostic.empty() ? "<none>" : selected->LastDiagnostic.c_str());
    ImGui::TextWrapped("last reload: %s", selected->LastReloadDiagnostic.empty() ? "<none>" : selected->LastReloadDiagnostic.c_str());
    ImGui::Separator();
    ImGui::Text("modGeneration %u", selected->ModGeneration);
    ImGui::SameLine();
    ImGui::Text("runtimeGeneration %u", selected->RuntimeGeneration);
    ImGui::SameLine();
    ImGui::Text("reloadAttemptId %u", selected->ReloadAttemptId);
}

void ScriptDevToolsService::DrawReloadTab(const ScriptModSnapshot *selected) {
    if (!selected) {
        ImGui::TextUnformatted("No script mod selected.");
        return;
    }
    ImGui::Text("policy %s", selected->ReloadPolicy.c_str());
    ImGui::Text("source %s", selected->SourcePath.c_str());
    ImGui::TextWrapped("dry-run compiles and validates without calling candidate OnLoad or replacing runtime.");
}

void ScriptDevToolsService::DrawResourcesTab(const ScriptModSnapshot *selected) {
    if (!selected) {
        ImGui::TextUnformatted("No script mod selected.");
        return;
    }
    const ScriptResourceSnapshot &resources = selected->Resources;
    if (ImGui::BeginTable("script-dev-resource-table", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        auto row = [](const char *name, size_t value) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(name);
            ImGui::TableNextColumn();
            ImGui::Text("%zu", value);
        };
        row("timers", resources.Timers);
        row("commands", resources.Commands);
        row("datashare requests", resources.DataShareRequests);
        row("callbacks", resources.Callbacks);
        row("exports", resources.Exports);
        row("host registrations", resources.HostRegistrations);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("active calls");
        ImGui::TableNextColumn();
        ImGui::Text("%d", resources.ActiveCalls);
        ImGui::EndTable();
    }
    if (!selected->Callbacks.empty()) {
        ImGui::Separator();
        for (const std::string &callback : selected->Callbacks)
            ImGui::BulletText("%s", callback.c_str());
    }
}

void ScriptDevToolsService::DrawExportsTab(const ScriptModSnapshot *selected) {
    if (!selected) {
        ImGui::TextUnformatted("No script mod selected.");
        return;
    }
    if (ImGui::BeginTable("script-dev-export-table", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("signature", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (const auto &item : selected->Exports) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(item.Name.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(item.Signature.c_str());
        }
        ImGui::EndTable();
    }
}

void ScriptDevToolsService::DrawDependenciesTab(const ScriptModSnapshot *selected) {
    if (!selected) {
        ImGui::TextUnformatted("No script mod selected.");
        return;
    }
    if (ImGui::BeginTable("script-dev-dep-table", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("min", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("type", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("state", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableHeadersRow();
        for (const auto &item : selected->Dependencies) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(item.Id.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(item.MinVersion.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(item.Optional ? "optional" : "required");
            ImGui::TableNextColumn();
            ImGui::TextColored(item.Satisfied ? ImVec4(0.48f, 0.74f, 0.48f, 1.0f) : ImVec4(0.92f, 0.36f, 0.34f, 1.0f),
                               "%s",
                               item.Satisfied ? "ok" : "missing");
        }
        ImGui::EndTable();
    }
}

void ScriptDevToolsService::DrawLogsTab() {
    DrawLogFilters();
    RebuildLogCacheIfNeeded();

    const float availableHeight = ImGui::GetContentRegionAvail().y;
    const float detailHeight = std::max(115.0f, availableHeight * 0.34f);
    const float tableHeight = std::max(150.0f, availableHeight - detailHeight - ImGui::GetFrameHeightWithSpacing() - 10.0f);

    DrawLogColumnsMenu();
    DrawLogTable(tableHeight);

    const ScriptDevEvent *selectedLog = FindSelectedLog();
    if (selectedLog && ImGui::Button("Copy Full Log"))
        CopyLogToClipboard(*selectedLog);
    if (selectedLog) {
        ImGui::SameLine();
        if (ImGui::Button("Copy Message"))
            CopyLogMessageToClipboard(*selectedLog);
    }
    DrawLogDetail(selectedLog);
}

void ScriptDevToolsService::DrawLogFilters() {
    const ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
    if (ImGui::BeginTable("script-dev-log-filters", 4, flags)) {
        ImGui::TableSetupColumn("label-a", ImGuiTableColumnFlags_WidthFixed, 52.0f);
        ImGui::TableSetupColumn("filter-a", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("label-b", ImGuiTableColumnFlags_WidthFixed, 56.0f);
        ImGui::TableSetupColumn("filter-b", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("level");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(96.0f);
        ImGui::Combo("##script-dev-severity", &m_EventSeverityFilter, "all\0info\0warn\0error\0");
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted("source");
        ImGui::TableSetColumnIndex(3);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##script-dev-source", m_EventSourceFilter, sizeof(m_EventSourceFilter));

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("tag");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##script-dev-code", m_EventCodeFilter, sizeof(m_EventCodeFilter));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted("text");
        ImGui::TableSetColumnIndex(3);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##script-dev-search", m_EventSearch, sizeof(m_EventSearch));

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("scope");
        ImGui::TableSetColumnIndex(1);
        ImGui::Checkbox("Selected Mod", &m_LogSelectedModOnly);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Only show logs whose mod id matches the selected mod.");
        ImGui::SameLine();
        ImGui::Checkbox("Reload", &m_LogReloadOnly);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Only show reload lifecycle logs.");
        ImGui::SameLine();
        ImGui::Checkbox("Pause", &m_PauseEventScroll);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Pause auto scroll.");
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted("attempt");
        ImGui::TableSetColumnIndex(3);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##script-dev-attempt", m_EventAttemptFilter, sizeof(m_EventAttemptFilter));

        ImGui::EndTable();
    }
}

void ScriptDevToolsService::RebuildLogCacheIfNeeded() {
    uint64_t eventGeneration = 0;
    {
        std::lock_guard<std::mutex> lock(m_EventMutex);
        eventGeneration = m_EventGeneration;
    }
    const LogFilters filters = {m_EventSeverityFilter,
                                m_EventCodeFilter,
                                m_EventSourceFilter,
                                m_EventAttemptFilter,
                                m_EventSearch,
                                m_SelectedModId,
                                m_LogSelectedModOnly,
                                m_LogReloadOnly};
    if (eventGeneration != m_FilteredEventCacheGeneration ||
        m_EventSeverityFilter != m_FilteredEventSeverityFilter ||
        filters.Tag != m_FilteredEventCodeFilter ||
        filters.Source != m_FilteredEventSourceFilter ||
        filters.Attempt != m_FilteredEventAttemptFilter ||
        filters.Text != m_FilteredEventSearch ||
        filters.SelectedModId != m_FilteredEventSelectedModId ||
        filters.SelectedModOnly != m_FilteredEventSelectedModOnly ||
        filters.ReloadOnly != m_FilteredEventReloadOnly) {
        std::vector<ScriptDevEvent> events = GetEventSnapshot();
        std::vector<ScriptDevEvent> filtered;
        filtered.reserve(events.size());
        for (const auto &event : events) {
            if (LogMatchesFilters(event, filters))
                filtered.push_back(event);
        }
        if (filtered.size() > 1000)
            filtered.erase(filtered.begin(), filtered.end() - 1000);
        m_FilteredEventCache = std::move(filtered);
        m_FilteredEventCacheGeneration = eventGeneration;
        m_FilteredEventSeverityFilter = m_EventSeverityFilter;
        m_FilteredEventCodeFilter = filters.Tag;
        m_FilteredEventSourceFilter = filters.Source;
        m_FilteredEventAttemptFilter = filters.Attempt;
        m_FilteredEventSearch = filters.Text;
        m_FilteredEventSelectedModId = filters.SelectedModId;
        m_FilteredEventSelectedModOnly = filters.SelectedModOnly;
        m_FilteredEventReloadOnly = filters.ReloadOnly;
    }
}

void ScriptDevToolsService::DrawLogColumnsMenu() {
    if (ImGui::Button("Columns"))
        ImGui::OpenPopup("script-dev-log-columns");
    if (ImGui::BeginPopup("script-dev-log-columns")) {
        for (int column = 0; column < LogColumnCount; ++column)
            ImGui::MenuItem(kLogColumns[column].Label, nullptr, &m_LogColumnVisible[column]);
        ImGui::EndPopup();
    }
}

void ScriptDevToolsService::DrawLogTable(float tableHeight) {
    if (ImGui::BeginChild("script-dev-logs-table", ImVec2(0, tableHeight), true)) {
        const ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                           ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                                           ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti |
                                           ImGuiTableFlags_ScrollX;
        if (ImGui::BeginTable("logs", LogColumnCount, tableFlags)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            SetupLogColumns(m_LogColumnVisible);
            ImGui::TableHeadersRow();

            std::vector<ScriptDevEvent> displayLogs = m_FilteredEventCache;
            SortLogs(displayLogs, ImGui::TableGetSortSpecs());
            for (const auto &event : displayLogs)
                DrawLogRow(event);
            ImGui::EndTable();
        }
        if (!m_PauseEventScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 8.0f)
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

void ScriptDevToolsService::DrawLogRow(const ScriptDevEvent &event) {
    ImGui::TableNextRow();
    bool selectableDrawn = false;
    const bool selected = m_SelectedEventSequence == event.Sequence;
    for (int column = 0; column < LogColumnCount; ++column) {
        if (!m_LogColumnVisible[column] || !ImGui::TableSetColumnIndex(column))
            continue;

        const std::string text = LogCellText(event, column);
        if (!selectableDrawn) {
            const std::string label = text + "##script-dev-log-row-" + std::to_string(event.Sequence);
            if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
                m_SelectedEventSequence = event.Sequence;
            selectableDrawn = true;
        } else if (column == LogColumnLevel) {
            ImGui::TextColored(SeverityColor(event.Severity), "%s", text.c_str());
        } else {
            ImGui::TextUnformatted(text.c_str());
        }
    }
}

const ScriptDevEvent *ScriptDevToolsService::FindSelectedLog() const {
    for (const auto &event : m_FilteredEventCache) {
        if (event.Sequence == m_SelectedEventSequence) {
            return &event;
        }
    }
    return nullptr;
}

void ScriptDevToolsService::CopyLogToClipboard(const ScriptDevEvent &event) const {
    std::ostringstream stream;
    stream << "sequence: " << event.Sequence << '\n'
           << "time: " << FormatTimestamp(event.TimestampMs) << '\n'
           << "severity: " << ToString(event.Severity) << '\n'
           << "tag: " << event.Code << '\n'
           << "mod: " << event.ModId << '\n'
           << "phase: " << event.Phase << '\n'
           << "attempt: " << event.ReloadAttemptId << '\n'
           << "source: " << event.SourcePath << '\n'
           << "message: " << event.Message << '\n';
    if (!event.Fields.empty()) {
        stream << "fields:\n";
        for (const auto &field : event.Fields)
            stream << "  " << field.Key << ": " << field.Value << '\n';
    }
    ImGui::SetClipboardText(stream.str().c_str());
}

void ScriptDevToolsService::CopyLogMessageToClipboard(const ScriptDevEvent &event) const {
    ImGui::SetClipboardText(event.Message.c_str());
}

void ScriptDevToolsService::DrawLogDetail(const ScriptDevEvent *selectedLog) {
    if (ImGui::BeginChild("script-dev-log-detail", ImVec2(0, 0), true)) {
        if (selectedLog) {
            ImGui::Text("#%llu  %s  %s",
                        static_cast<unsigned long long>(selectedLog->Sequence),
                        FormatTimestamp(selectedLog->TimestampMs).c_str(),
                        ToString(selectedLog->Severity));
            ImGui::SameLine();
            ImGui::TextColored(SeverityColor(selectedLog->Severity), "%s", selectedLog->Code.c_str());
            const std::string source = LogSource(*selectedLog);
            if (!source.empty())
                ImGui::Text("source %s", source.c_str());
            if (selectedLog->ReloadAttemptId != 0)
                ImGui::Text("attempt %u", selectedLog->ReloadAttemptId);
            if (!selectedLog->ModId.empty() && selectedLog->ModId != source)
                ImGui::Text("mod %s", selectedLog->ModId.c_str());
            if (!selectedLog->SourcePath.empty() && selectedLog->SourcePath != source)
                ImGui::TextWrapped("path %s", selectedLog->SourcePath.c_str());
            ImGui::Separator();
            ImGui::TextWrapped("%s", selectedLog->Message.c_str());
            if (!selectedLog->Fields.empty()) {
                ImGui::Separator();
                for (const auto &field : selectedLog->Fields) {
                    if (field.Key == "stack") {
                        ImGui::TextUnformatted("stack");
                        ImGui::TextWrapped("%s", field.Value.c_str());
                    } else {
                        ImGui::TextWrapped("%s: %s", field.Key.c_str(), field.Value.c_str());
                    }
                }
            }
        } else {
            ImGui::TextUnformatted("Select a log line to inspect the full message.");
        }
    }
    ImGui::EndChild();
}

void ScriptDevToolsService::DrawBottomBar(const ScriptModSnapshot *selected) {
    ImGui::Separator();
    const bool hasSelection = selected != nullptr;
    if (!hasSelection)
        ImGui::BeginDisabled();
    if (ImGui::Button("Dry Run")) {
        ScriptDevAction action;
        action.Kind = ScriptDevActionKind::Reload;
        action.ModId = selected ? selected->Id : "";
        action.ReloadOptions.DryRun = true;
        EnqueueAction(action);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        ScriptDevAction action;
        action.Kind = ScriptDevActionKind::Reload;
        action.ModId = selected ? selected->Id : "";
        EnqueueAction(action);
    }
    if (!hasSelection)
        ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Watch On")) {
        ScriptDevAction action;
        action.Kind = ScriptDevActionKind::Watch;
        action.WatchEnabled = true;
        EnqueueAction(action);
    }
    ImGui::SameLine();
    if (ImGui::Button("Watch Off")) {
        ScriptDevAction action;
        action.Kind = ScriptDevActionKind::Watch;
        action.WatchEnabled = false;
        EnqueueAction(action);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Logs")) {
        ScriptDevAction action;
        action.Kind = ScriptDevActionKind::ClearEvents;
        EnqueueAction(action);
    }
    ImGui::SameLine();
    if (ImGui::Button("Close"))
        Hide();
}

} // namespace BML

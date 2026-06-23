#ifndef BML_SCRIPTDEVTOOLSSERVICE_H
#define BML_SCRIPTDEVTOOLSSERVICE_H

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "BML/Bui.h"
#include "ScriptDevEvents.h"
#include "ScriptMod.h"

class IBML;
class ModContext;

namespace BML {

struct ScriptDependencySnapshot {
    std::string Id;
    std::string MinVersion;
    bool Optional = false;
    bool Satisfied = false;
};

struct ScriptExportSnapshot {
    std::string Name;
    std::string Signature;
};

struct ScriptResourceSnapshot {
    size_t Timers = 0;
    size_t Commands = 0;
    size_t DataShareRequests = 0;
    size_t HostRegistrations = 0;
    size_t Callbacks = 0;
    size_t Exports = 0;
    int ActiveCalls = 0;
};

struct ScriptModSnapshot {
    std::string Id;
    std::string Name;
    std::string Version;
    std::string State;
    std::string ReloadPolicy;
    std::string SourceKind;
    std::string SourcePath;
    std::string EntryPath;
    std::string LastDiagnostic;
    std::string LastReloadDiagnostic;
    unsigned int ModGeneration = 0;
    unsigned int RuntimeGeneration = 0;
    unsigned int ReloadAttemptId = 0;
    std::vector<ScriptDependencySnapshot> Dependencies;
    std::vector<ScriptExportSnapshot> Exports;
    std::vector<std::string> Callbacks;
    ScriptResourceSnapshot Resources;
};

struct ScriptDevStatusSnapshot {
    std::string HotReloadStatus;
    size_t ModCount = 0;
    size_t LoadedCount = 0;
    size_t FailedCount = 0;
    size_t ReloadingCount = 0;
    uint64_t DroppedEvents = 0;
    uint64_t EventCount = 0;
};

enum class ScriptDevActionKind {
    Reload,
    ReloadAll,
    Watch,
    ClearEvents,
    TogglePanel,
};

struct ScriptDevAction {
    ScriptDevActionKind Kind = ScriptDevActionKind::Reload;
    std::string ModId;
    ScriptModReloadOptions ReloadOptions;
    bool WatchEnabled = false;
};

class ScriptDevToolsService final : public Bui::Window {
public:
    explicit ScriptDevToolsService(ModContext *context);

    void ProcessActions();
    void RenderPanel();

    void PublishEvent(ScriptDevEventSeverity severity,
                      const std::string &code,
                      const std::string &modId,
                      const std::string &phase,
                      const std::string &sourcePath,
                      const std::string &message,
                      const std::vector<ScriptDevEventField> &fields = {},
                      unsigned int reloadAttemptId = 0);
    void PublishDiagnostic(ScriptDevEventSeverity severity,
                           const std::string &code,
                           const std::string &modId,
                           const ScriptDiagnostic &diagnostic,
                           unsigned int reloadAttemptId = 0);
    void PublishLogLine(const char *level, const char *source, const std::string &message);

    void EnqueueAction(const ScriptDevAction &action);
    std::vector<std::string> HandleCommand(const std::vector<std::string> &args);
    std::vector<std::string> CompleteCommand(const std::vector<std::string> &args);

    std::vector<std::string> GetScriptModIds();
    ScriptDevStatusSnapshot GetStatusSnapshot();
    std::vector<ScriptModSnapshot> GetModSnapshots(bool force = false);
    std::vector<ScriptDevEvent> GetEventSnapshot() const;
    uint64_t GetDroppedEventCount() const;

    ImGuiWindowFlags GetFlags() override;
    void OnPreBegin() override;
    void OnPostBegin() override;
    void OnPostEnd() override;
    void OnDraw() override;
    void OnShow() override;
    void OnHide() override;

private:
    static constexpr size_t kEventCapacity = 8192;

    ScriptMod *FindScriptMod(const std::string &id) const;
    void RefreshSnapshotsIfNeeded(bool force);
    ScriptModSnapshot BuildSnapshot(ScriptMod *mod) const;
    std::string FormatStatus();
    std::vector<std::string> FormatLogs(const std::string &severity);
    std::vector<std::string> FormatList();
    std::vector<std::string> FormatInfo(const std::string &id);
    std::vector<std::string> FormatDiag(const std::string &id);
    std::vector<std::string> FormatDeps(const std::string &id);
    std::vector<std::string> FormatExports(const std::string &id);
    std::vector<std::string> FormatResources(const std::string &id);
    const ScriptModSnapshot *FindSnapshot(const std::string &id);

    void ExecuteAction(const ScriptDevAction &action);
    void ExecuteReloadAction(const ScriptDevAction &action);
    void ExecuteReloadAllAction(const ScriptDevAction &action);
    void ClearEvents();
    void BlockGameInput();
    void UnblockGameInput();

    void DrawStatusBar();
    void DrawModList();
    void DrawDiagnosticsTab(const ScriptModSnapshot *selected);
    void DrawReloadTab(const ScriptModSnapshot *selected);
    void DrawResourcesTab(const ScriptModSnapshot *selected);
    void DrawExportsTab(const ScriptModSnapshot *selected);
    void DrawDependenciesTab(const ScriptModSnapshot *selected);
    void DrawLogsTab();
    void DrawLogFilters();
    void RebuildLogCacheIfNeeded();
    void DrawLogColumnsMenu();
    void DrawLogTable(float tableHeight);
    void DrawLogRow(const ScriptDevEvent &event);
    void DrawLogDetail(const ScriptDevEvent *selectedLog);
    const ScriptDevEvent *FindSelectedLog() const;
    void CopyLogToClipboard(const ScriptDevEvent &event) const;
    void CopyLogMessageToClipboard(const ScriptDevEvent &event) const;
    void DrawBottomBar(const ScriptModSnapshot *selected);

    ModContext *m_Context = nullptr;

    mutable std::mutex m_EventMutex;
    ScriptDevEventRingBuffer m_EventStore;

    mutable std::mutex m_ActionMutex;
    std::deque<ScriptDevAction> m_Actions;

    std::vector<ScriptModSnapshot> m_Snapshots;
    uint64_t m_SnapshotGeneration = 0;
    uint64_t m_LastObservedGeneration = 0;

    std::string m_SelectedModId;
    uint64_t m_SelectedEventSequence = 0;
    int m_EventSeverityFilter = 0;
    char m_EventCodeFilter[64] = {};
    char m_EventSourceFilter[128] = {};
    char m_EventAttemptFilter[32] = {};
    char m_EventSearch[128] = {};
    bool m_PauseEventScroll = false;
    bool m_InputBlockTokenActive = false;
    bool m_LogSelectedModOnly = false;
    bool m_LogReloadOnly = false;
    bool m_ShowAdvancedLogFilters = false;
    bool m_LogColumnVisible[7] = {true, true, true, true, false, false, true};
    std::vector<ScriptDevEvent> m_FilteredEventCache;
    uint64_t m_FilteredEventCacheGeneration = 0;
    int m_FilteredEventSeverityFilter = -1;
    std::string m_FilteredEventCodeFilter;
    std::string m_FilteredEventSourceFilter;
    std::string m_FilteredEventAttemptFilter;
    std::string m_FilteredEventSearch;
    std::string m_FilteredEventSelectedModId;
    bool m_FilteredEventSelectedModOnly = false;
    bool m_FilteredEventReloadOnly = false;
};

} // namespace BML

#endif

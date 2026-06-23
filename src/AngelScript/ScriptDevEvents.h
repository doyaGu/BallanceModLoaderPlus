#ifndef BML_SCRIPTDEVEVENTS_H
#define BML_SCRIPTDEVEVENTS_H

#include <cstdint>
#include <string>
#include <vector>

namespace BML {

enum class ScriptDevEventSeverity {
    Info,
    Warn,
    Error,
};

struct ScriptDevEventField {
    std::string Key;
    std::string Value;
};

struct ScriptDevEvent {
    uint64_t Sequence = 0;
    uint64_t TimestampMs = 0;
    unsigned int ReloadAttemptId = 0;
    ScriptDevEventSeverity Severity = ScriptDevEventSeverity::Info;
    std::string Code;
    std::string ModId;
    std::string Phase;
    std::string SourcePath;
    std::string Message;
    std::vector<ScriptDevEventField> Fields;
};

struct ScriptDevLogFilters {
    std::string Severity;
    std::string Tag;
    std::string Source;
    std::string Attempt;
    std::string Text;
    std::string SelectedModId;
    bool SelectedModOnly = false;
    bool ReloadOnly = false;
};

class ScriptDevEventRingBuffer {
public:
    explicit ScriptDevEventRingBuffer(size_t capacity);

    uint64_t Append(ScriptDevEvent event);
    void Clear();

    std::vector<ScriptDevEvent> Snapshot() const;
    uint64_t GetDroppedCount() const { return m_DroppedEvents; }
    uint64_t GetGeneration() const { return m_Generation; }
    size_t GetCount() const { return m_EventCount; }
    size_t GetCapacity() const { return m_Events.size(); }

private:
    std::vector<ScriptDevEvent> m_Events;
    size_t m_EventStart = 0;
    size_t m_EventCount = 0;
    uint64_t m_NextSequence = 1;
    uint64_t m_DroppedEvents = 0;
    uint64_t m_Generation = 0;
};

const char *ToString(ScriptDevEventSeverity severity);
ScriptDevEventSeverity ScriptDevEventSeverityFromLogLevel(const char *level);
bool ScriptDevEventSeverityMatches(const ScriptDevEvent &event, const std::string &severity);
std::string JoinScriptDevEventFields(const std::vector<ScriptDevEventField> &fields);
std::string ScriptDevLogSource(const ScriptDevEvent &event);
std::string ScriptDevLogSearchText(const ScriptDevEvent &event);
bool ScriptDevLogMatchesFilters(const ScriptDevEvent &event, const ScriptDevLogFilters &filters);
std::vector<ScriptDevEvent> FilterScriptDevEvents(const std::vector<ScriptDevEvent> &events,
                                                  const ScriptDevLogFilters &filters,
                                                  size_t limit = 0);

} // namespace BML

#endif

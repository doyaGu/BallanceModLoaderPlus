#include "ScriptDevEvents.h"

#include <algorithm>
#include <sstream>

#include "Utils/StringUtils.h"

namespace BML {

namespace {

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

bool ContainsInsensitive(const std::string &text, const std::string &needle) {
    if (needle.empty())
        return true;
    return utils::ToLower(text).find(utils::ToLower(needle)) != std::string::npos;
}

bool IsReloadLog(const ScriptDevEvent &event) {
    return event.Phase == "reload" || event.Code.rfind("ScriptReload", 0) == 0;
}

} // namespace

ScriptDevEventRingBuffer::ScriptDevEventRingBuffer(size_t capacity)
    : m_Events(capacity) {
}

uint64_t ScriptDevEventRingBuffer::Append(ScriptDevEvent event) {
    event.Sequence = m_NextSequence++;
    const uint64_t sequence = event.Sequence;

    if (m_Events.empty()) {
        ++m_DroppedEvents;
        ++m_Generation;
        return sequence;
    }

    if (m_EventCount < m_Events.size()) {
        const size_t index = (m_EventStart + m_EventCount) % m_Events.size();
        m_Events[index] = std::move(event);
        ++m_EventCount;
    } else {
        m_Events[m_EventStart] = std::move(event);
        m_EventStart = (m_EventStart + 1) % m_Events.size();
        ++m_DroppedEvents;
    }
    ++m_Generation;
    return sequence;
}

void ScriptDevEventRingBuffer::Clear() {
    m_EventStart = 0;
    m_EventCount = 0;
    m_DroppedEvents = 0;
    ++m_Generation;
}

std::vector<ScriptDevEvent> ScriptDevEventRingBuffer::Snapshot() const {
    std::vector<ScriptDevEvent> events;
    events.reserve(m_EventCount);
    if (m_Events.empty())
        return events;
    for (size_t i = 0; i < m_EventCount; ++i)
        events.push_back(m_Events[(m_EventStart + i) % m_Events.size()]);
    return events;
}

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

bool ScriptDevEventSeverityMatches(const ScriptDevEvent &event, const std::string &severity) {
    if (severity.empty())
        return true;
    return SeverityToLower(event.Severity) == utils::ToLower(severity);
}

std::string JoinScriptDevEventFields(const std::vector<ScriptDevEventField> &fields) {
    std::ostringstream stream;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i != 0)
            stream << ' ';
        stream << fields[i].Key << '=' << fields[i].Value;
    }
    return stream.str();
}

std::string ScriptDevLogSource(const ScriptDevEvent &event) {
    return event.ModId.empty() ? event.SourcePath : event.ModId;
}

std::string ScriptDevLogSearchText(const ScriptDevEvent &event) {
    return event.Code + " " + event.ModId + " " + event.Phase + " " +
           (event.ReloadAttemptId == 0 ? "" : std::to_string(event.ReloadAttemptId)) + " " +
           event.SourcePath + " " + event.Message + " " + JoinScriptDevEventFields(event.Fields);
}

bool ScriptDevLogMatchesFilters(const ScriptDevEvent &event, const ScriptDevLogFilters &filters) {
    if (!ScriptDevEventSeverityMatches(event, filters.Severity))
        return false;
    if (filters.SelectedModOnly && !filters.SelectedModId.empty() && event.ModId != filters.SelectedModId)
        return false;
    if (filters.ReloadOnly && !IsReloadLog(event))
        return false;
    if (!ContainsInsensitive(event.Code, filters.Code))
        return false;

    const std::string sourceHaystack = ScriptDevLogSource(event) + " " + event.SourcePath;
    if (!ContainsInsensitive(sourceHaystack, filters.Source))
        return false;
    if (!filters.Attempt.empty() &&
        !ContainsInsensitive(event.ReloadAttemptId == 0 ? "" : std::to_string(event.ReloadAttemptId),
                             filters.Attempt)) {
        return false;
    }

    return ContainsInsensitive(ScriptDevLogSearchText(event), filters.Text);
}

std::vector<ScriptDevEvent> FilterScriptDevEvents(const std::vector<ScriptDevEvent> &events,
                                                  const ScriptDevLogFilters &filters,
                                                  size_t limit) {
    std::vector<ScriptDevEvent> filtered;
    filtered.reserve(events.size());
    for (const auto &event : events) {
        if (ScriptDevLogMatchesFilters(event, filters))
            filtered.push_back(event);
    }
    if (limit != 0 && filtered.size() > limit)
        filtered.erase(filtered.begin(), filtered.end() - limit);
    return filtered;
}

} // namespace BML

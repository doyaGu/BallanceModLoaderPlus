#ifndef BML_DEVTOOLS_LOGSTREAMPANEL_H
#define BML_DEVTOOLS_LOGSTREAMPANEL_H

#include "Panel.h"
#include "ConcurrentRingBuffer.h"
#include "bml_logging.h"
#include "imgui.h"

#include <cstring>
#include <deque>

namespace devtools {

struct LogEntry {
    uint64_t timestamp;
    BML_LogSeverity severity;
    char mod_id[64];
    char tag[64];
    char message[512];
};

class LogStreamPanel : public Panel {
    ConcurrentRingBuffer<LogEntry, 2048> m_Buffer;
    std::deque<LogEntry> m_Display;
    uint64_t m_BaseTimestamp = 0;
    uint64_t m_PerfFreq = 1;
    bool m_AutoScroll = true;
    bool m_Available = false;
    int m_MinSeverity = BML_LOG_INFO;
    char m_ModFilter[64]{};
    char m_TagFilter[64]{};

    static constexpr size_t kMaxDisplay = 2048;

    static void ListenerCallback(BML_Context, const BML_LogMessageInfo *info, void *ud) {
        auto *self = static_cast<LogStreamPanel *>(ud);
        LogEntry entry{};
        LARGE_INTEGER qpc;
        QueryPerformanceCounter(&qpc);
        entry.timestamp = static_cast<uint64_t>(qpc.QuadPart);
        entry.severity = info->severity;
        if (info->mod_id)  std::strncpy(entry.mod_id, info->mod_id, sizeof(entry.mod_id) - 1);
        if (info->tag)     std::strncpy(entry.tag, info->tag, sizeof(entry.tag) - 1);
        if (info->message) std::strncpy(entry.message, info->message, sizeof(entry.message) - 1);
        self->m_Buffer.TryPush(entry);
    }

    static ImVec4 SeverityColor(BML_LogSeverity s) {
        switch (s) {
            case BML_LOG_TRACE: return {0.53f, 0.53f, 0.53f, 1.0f};
            case BML_LOG_DEBUG: return {0.80f, 0.80f, 0.80f, 1.0f};
            case BML_LOG_INFO:  return {0.27f, 1.00f, 0.27f, 1.0f};
            case BML_LOG_WARN:  return {1.00f, 1.00f, 0.27f, 1.0f};
            case BML_LOG_ERROR: return {1.00f, 0.27f, 0.27f, 1.0f};
            case BML_LOG_FATAL: return {1.00f, 0.00f, 0.00f, 1.0f};
            default:            return {1.00f, 1.00f, 1.00f, 1.0f};
        }
    }

    static const char *SeverityLabel(BML_LogSeverity s) {
        switch (s) {
            case BML_LOG_TRACE: return "TRACE";
            case BML_LOG_DEBUG: return "DEBUG";
            case BML_LOG_INFO:  return "INFO";
            case BML_LOG_WARN:  return "WARN";
            case BML_LOG_ERROR: return "ERROR";
            case BML_LOG_FATAL: return "FATAL";
            default:            return "?";
        }
    }

    bool PassesFilter(const LogEntry &e) const {
        if (static_cast<int>(e.severity) < m_MinSeverity) return false;
        if (m_ModFilter[0] && !std::strstr(e.mod_id, m_ModFilter)) return false;
        if (m_TagFilter[0] && !std::strstr(e.tag, m_TagFilter)) return false;
        return true;
    }

public:
    const char *Id() const override { return "log_stream"; }
    const char *Label(const bml::Locale &loc) const override { return loc["tab.log_stream"]; }

    void OnShow(const bml::ModuleServices &svc) override {
        const auto *log = svc.Interfaces().Logging;
        if (!log) return;
        if (log->header.struct_size < offsetof(BML_CoreLoggingInterface, AddLogListener)
                                      + sizeof(log->AddLogListener))
            return;
        if (!log->AddLogListener) return;
        if (log->AddLogListener(svc.Handle(), ListenerCallback, this) == BML_RESULT_OK) {
            m_Available = true;
            LARGE_INTEGER freq, now;
            QueryPerformanceFrequency(&freq);
            QueryPerformanceCounter(&now);
            m_PerfFreq = static_cast<uint64_t>(freq.QuadPart);
            m_BaseTimestamp = static_cast<uint64_t>(now.QuadPart);
        }
    }

    void OnHide(const bml::ModuleServices &svc) override {
        if (!m_Available) return;
        const auto *log = svc.Interfaces().Logging;
        if (log && log->RemoveLogListener)
            log->RemoveLogListener(svc.Handle(), ListenerCallback);
        m_Available = false;
    }

    void Sample(const bml::ModuleServices &) override {
        if (!m_Available) return;
        LogEntry batch[128];
        size_t n = m_Buffer.Drain(batch, 128);
        for (size_t i = 0; i < n; ++i) m_Display.push_back(batch[i]);
        while (m_Display.size() > kMaxDisplay) m_Display.pop_front();
    }

    void Draw(const bml::ModuleServices &svc) override {
        const auto &loc = svc.Locale();
        if (!m_Available) {
            ImGui::TextColored(ImVec4(1, 0.6f, 0.2f, 1), "%s", loc["label.feature_unavailable"]);
            return;
        }
        ImGui::Text("%s:", loc["label.severity_filter"]);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        const char *levels[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
        ImGui::Combo("##severity", &m_MinSeverity, levels, 6);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        ImGui::InputText(loc["column.module"], m_ModFilter, sizeof(m_ModFilter));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        ImGui::InputText(loc["column.tag"], m_TagFilter, sizeof(m_TagFilter));
        ImGui::SameLine();
        ImGui::Checkbox(loc["label.auto_scroll"], &m_AutoScroll);
        ImGui::SameLine();
        if (ImGui::Button(loc["label.clear"])) m_Display.clear();
        ImGui::Separator();
        if (ImGui::BeginTable("LogStream", 5,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
                | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 0))) {
            ImGui::TableSetupColumn(loc["column.time"], 0, 60);
            ImGui::TableSetupColumn("Level", 0, 50);
            ImGui::TableSetupColumn(loc["column.module"], 0, 100);
            ImGui::TableSetupColumn(loc["column.tag"], 0, 80);
            ImGui::TableSetupColumn(loc["column.message"], 0, 300);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            for (const auto &e : m_Display) {
                if (!PassesFilter(e)) continue;
                float t = static_cast<float>(
                    static_cast<double>(e.timestamp - m_BaseTimestamp)
                    / static_cast<double>(m_PerfFreq));
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%.3f", t);
                ImGui::TableNextColumn();
                ImGui::TextColored(SeverityColor(e.severity), "%s", SeverityLabel(e.severity));
                ImGui::TableNextColumn(); ImGui::TextUnformatted(e.mod_id);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(e.tag);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(e.message);
            }
            if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndTable();
        }
    }
};

} // namespace devtools

#endif // BML_DEVTOOLS_LOGSTREAMPANEL_H

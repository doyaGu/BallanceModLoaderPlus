#ifndef BML_DEVTOOLS_PROFILINGPANEL_H
#define BML_DEVTOOLS_PROFILINGPANEL_H

#include "Panel.h"
#include "RingBuffer.h"
#include "bml_profiling.h"
#include "imgui.h"

namespace devtools {

class ProfilingPanel : public Panel {
    BML_ProfilingStats m_Stats = BML_PROFILING_STATS_INIT;
    BML_ProfilingStats m_PrevStats = BML_PROFILING_STATS_INIT;
    RingBuffer<float, 300> m_EventsPerFrame;
    uint64_t m_CpuFreq = 0;

public:
    const char *Id() const override { return "profiling"; }
    const char *Label(const bml::Locale &) const override { return "Profiling"; }

    void Sample(const bml::ModuleServices &svc) override {
        const auto *prof = svc.Interfaces().Profiling;
        if (!prof) return;

        m_PrevStats = m_Stats;
        if (prof->GetProfilingStats) {
            m_Stats = BML_PROFILING_STATS_INIT;
            prof->GetProfilingStats(prof->Context, &m_Stats);
        }
        if (prof->GetCpuFrequency) m_CpuFreq = prof->GetCpuFrequency(prof->Context);

        float delta = static_cast<float>(m_Stats.total_events - m_PrevStats.total_events);
        m_EventsPerFrame.Push(delta);
    }

    void Refresh(const bml::ModuleServices &svc) override { Sample(svc); }

    void Draw(const bml::ModuleServices &) override {
        if (ImGui::BeginTable("ProfStats", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();

            auto row = [](const char *label, const char *fmt, auto val) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(label);
                ImGui::TableNextColumn(); ImGui::Text(fmt, val);
            };
            row("Total Events", "%llu", m_Stats.total_events);
            row("Total Scopes", "%llu", m_Stats.total_scopes);
            row("Active Scopes", "%llu", m_Stats.active_scopes);
            row("Dropped Events", "%llu", m_Stats.dropped_events);
            row("Memory Used", "%llu bytes", m_Stats.memory_used_bytes);
            row("CPU Frequency", "%llu Hz", m_CpuFreq);
            ImGui::EndTable();
        }

        ImGui::Separator();
        char overlay[64];
        float last = m_EventsPerFrame.Count() > 0
            ? m_EventsPerFrame[m_EventsPerFrame.Count() - 1] : 0.0f;
        snprintf(overlay, sizeof(overlay), "%.0f evt/frame", last);
        ImGui::PlotLines("Events/Frame", m_EventsPerFrame.Data(), m_EventsPerFrame.Size(),
                          m_EventsPerFrame.Offset(), overlay, 0.0f, 0.0f, ImVec2(0, 80));
    }
};

} // namespace devtools

#endif // BML_DEVTOOLS_PROFILINGPANEL_H

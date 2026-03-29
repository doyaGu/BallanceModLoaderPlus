#ifndef BML_DEVTOOLS_MEMORYPANEL_H
#define BML_DEVTOOLS_MEMORYPANEL_H

#include "Panel.h"
#include "RingBuffer.h"
#include "bml_memory.h"
#include "imgui.h"

#include <cstdio>

namespace devtools {

class MemoryPanel : public Panel {
    BML_MemoryStats m_Stats{};
    RingBuffer<float, 300> m_AllocHistory;
    RingBuffer<float, 300> m_ActiveHistory;

public:
    const char *Id() const override { return "memory"; }
    const char *Label(const bml::Locale &) const override { return "Memory"; }

    void Sample(const bml::ModuleServices &svc) override {
        const auto *mem = svc.Interfaces().Memory;
        if (!mem || !mem->GetMemoryStats) return;
        mem->GetMemoryStats(mem->Context, &m_Stats);
        m_AllocHistory.Push(static_cast<float>(m_Stats.total_allocated) / (1024.0f * 1024.0f));
        m_ActiveHistory.Push(static_cast<float>(m_Stats.active_alloc_count));
    }

    void Refresh(const bml::ModuleServices &svc) override { Sample(svc); }

    void Draw(const bml::ModuleServices &) override {
        float currentMB = static_cast<float>(m_Stats.total_allocated) / (1024.0f * 1024.0f);
        float peakMB = static_cast<float>(m_Stats.peak_allocated) / (1024.0f * 1024.0f);
        ImGui::Text("Allocated: %.2f MB   Peak: %.2f MB   Active: %llu",
                     currentMB, peakMB, m_Stats.active_alloc_count);
        ImGui::Text("Total Allocs: %llu   Frees: %llu   Delta: %lld",
                     m_Stats.total_alloc_count, m_Stats.total_free_count,
                     static_cast<int64_t>(m_Stats.total_alloc_count) - static_cast<int64_t>(m_Stats.total_free_count));
        ImGui::Separator();

        char overlay[64];
        std::snprintf(overlay, sizeof(overlay), "%.2f MB", currentMB);
        ImGui::PlotLines("Allocated (MB)", m_AllocHistory.Data(), m_AllocHistory.Size(),
                          m_AllocHistory.Offset(), overlay, 0.0f, peakMB * 1.2f, ImVec2(0, 80));

        std::snprintf(overlay, sizeof(overlay), "%llu", m_Stats.active_alloc_count);
        ImGui::PlotLines("Active Allocs", m_ActiveHistory.Data(), m_ActiveHistory.Size(),
                          m_ActiveHistory.Offset(), overlay, 0.0f, 0.0f, ImVec2(0, 80));
    }
};

} // namespace devtools

#endif // BML_DEVTOOLS_MEMORYPANEL_H

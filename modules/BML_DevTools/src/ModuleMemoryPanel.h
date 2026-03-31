#ifndef BML_DEVTOOLS_MODULEMEMORYPANEL_H
#define BML_DEVTOOLS_MODULEMEMORYPANEL_H

#include "Panel.h"
#include "bml_memory.h"
#include "imgui.h"

#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>

namespace devtools {

struct ModuleMemoryRow {
    std::string module_id;
    uint64_t total_allocated;
    uint64_t peak_allocated;
    uint64_t active_alloc_count;
    float total_mb;
};

class ModuleMemoryPanel : public Panel {
    std::vector<ModuleMemoryRow> m_Rows;
    bool m_TrackingEnabled = false;

public:
    const char *Id() const override { return "module_memory"; }
    const char *Label(const bml::Locale &loc) const override {
        return loc["tab.module_memory"];
    }

    void Refresh(const bml::ModuleServices &svc) override {
        const auto *mem = svc.Interfaces().Memory;
        if (!mem || !mem->EnumerateModuleMemory) return;

        m_Rows.clear();

        auto callback = [](const BML_ModuleMemoryStats *stats, void *user_data) {
            auto *rows = static_cast<std::vector<ModuleMemoryRow> *>(user_data);
            ModuleMemoryRow row;
            row.module_id = stats->module_id ? stats->module_id : "";
            row.total_allocated = stats->total_allocated;
            row.peak_allocated = stats->peak_allocated;
            row.active_alloc_count = stats->active_alloc_count;
            row.total_mb = static_cast<float>(stats->total_allocated) / (1024.0f * 1024.0f);
            rows->push_back(row);
        };

        mem->EnumerateModuleMemory(mem->Context, callback, &m_Rows);

        // Sort by total_allocated descending
        std::sort(m_Rows.begin(), m_Rows.end(),
            [](const ModuleMemoryRow &a, const ModuleMemoryRow &b) {
                return a.total_allocated > b.total_allocated;
            });
    }

    void Draw(const bml::ModuleServices &svc) override {
        const auto &loc = svc.Locale();

        // Toggle tracking
        const auto *mem = svc.Interfaces().Memory;
        if (!mem) {
            ImGui::TextDisabled("Memory interface not available");
            return;
        }

        ImGui::Text("%s", loc["label.tracking_active"]);
        ImGui::SameLine();
        if (ImGui::Checkbox("##tracking", &m_TrackingEnabled)) {
            if (mem->EnableModuleMemoryTracking) {
                mem->EnableModuleMemoryTracking(mem->Context, m_TrackingEnabled);
            }
        }
        ImGui::TextDisabled("%s", loc["label.tracking_note"]);
        ImGui::Separator();

        if (!m_TrackingEnabled) {
            ImGui::TextDisabled("Enable tracking to see per-module memory usage");
            return;
        }

        // Calculate totals
        uint64_t total_allocated = 0;
        for (const auto &row : m_Rows) {
            total_allocated += row.total_allocated;
        }
        float total_mb = static_cast<float>(total_allocated) / (1024.0f * 1024.0f);

        ImGui::Text(loc["label.total_across"], total_mb, static_cast<unsigned>(m_Rows.size()));
        ImGui::Separator();

        // Table
        if (ImGui::BeginTable("ModuleMemoryTable", 5,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn(loc["column.module"], ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(loc["column.allocated"], ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn(loc["column.peak"], ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn(loc["column.active"], ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn(loc["column.trend"], ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            for (const auto &row : m_Rows) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(row.module_id.c_str());

                ImGui::TableSetColumnIndex(1);
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.2f MB", row.total_mb);
                ImGui::Text("%s", buf);

                ImGui::TableSetColumnIndex(2);
                float peak_mb = static_cast<float>(row.peak_allocated) / (1024.0f * 1024.0f);
                std::snprintf(buf, sizeof(buf), "%.2f MB", peak_mb);
                ImGui::Text("%s", buf);

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%llu", row.active_alloc_count);

                ImGui::TableSetColumnIndex(4);
                float pct = row.peak_allocated > 0
                    ? (static_cast<float>(row.total_allocated) / static_cast<float>(row.peak_allocated)) * 100.0f
                    : 0.0f;
                std::snprintf(buf, sizeof(buf), "%.0f%%", pct);
                ImGui::Text("%s", buf);
            }

            ImGui::EndTable();
        }
    }
};

} // namespace devtools

#endif // BML_DEVTOOLS_MODULEMEMORYPANEL_H

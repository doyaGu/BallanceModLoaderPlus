#ifndef BML_DEVTOOLS_IMCPANEL_H
#define BML_DEVTOOLS_IMCPANEL_H

#include "Panel.h"
#include "bml_imc.h"
#include "imgui.h"

#include <cstring>
#include <vector>

namespace devtools {

struct ImcTopicEntry {
    uint32_t id;
    char name[256];
    size_t subscriber_count;
    uint64_t message_count;
};

class ImcPanel : public Panel {
    std::vector<ImcTopicEntry> m_Topics;
    BML_ImcStats m_Stats{};

public:
    const char *Id() const override { return "imc"; }
    const char *Label(const bml::Locale &loc) const override { return loc["tab.imc"]; }

    void Sample(const bml::ModuleServices &svc) override {
        const auto *bus = svc.Interfaces().ImcBus;
        if (bus && bus->GetStats) {
            m_Stats = BML_IMC_STATS_INIT;
            bus->GetStats(bus->Context, &m_Stats);
        }
    }

    void Refresh(const bml::ModuleServices &svc) override {
        Sample(svc);
        m_Topics.clear();
        const auto *bus = svc.Interfaces().ImcBus;
        if (!bus || !bus->GetTopicInfo) return;
        for (uint32_t id = 1; id < 1000; ++id) {
            BML_TopicInfo info = BML_TOPIC_INFO_INIT;
            if (bus->GetTopicInfo(bus->Context, id, &info) == BML_RESULT_OK) {
                ImcTopicEntry entry{};
                entry.id = info.topic_id;
                std::memcpy(entry.name, info.name, sizeof(entry.name));
                entry.subscriber_count = info.subscriber_count;
                entry.message_count = info.message_count;
                m_Topics.push_back(entry);
            }
        }
    }

    void Draw(const bml::ModuleServices &svc) override {
        const auto &loc = svc.Locale();
        ImGui::Text("Published: %llu  Delivered: %llu  Dropped: %llu",
                     m_Stats.total_messages_published,
                     m_Stats.total_messages_delivered,
                     m_Stats.total_messages_dropped);
        ImGui::Text("Topics: %zu  Subscriptions: %zu  RPCs: %zu",
                     m_Stats.active_topics,
                     m_Stats.active_subscriptions,
                     m_Stats.active_rpc_handlers);
        ImGui::Separator();
        if (ImGui::BeginTable("Topics", 4,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn(loc["column.id"]);
            ImGui::TableSetupColumn(loc["column.name"]);
            ImGui::TableSetupColumn(loc["column.subs"]);
            ImGui::TableSetupColumn(loc["column.messages"]);
            ImGui::TableHeadersRow();
            for (const auto &t : m_Topics) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%u", t.id);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(t.name);
                ImGui::TableNextColumn(); ImGui::Text("%zu", t.subscriber_count);
                ImGui::TableNextColumn(); ImGui::Text("%llu", t.message_count);
            }
            ImGui::EndTable();
        }
    }
};

} // namespace devtools

#endif // BML_DEVTOOLS_IMCPANEL_H

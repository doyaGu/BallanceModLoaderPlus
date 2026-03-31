#ifndef BML_DEVTOOLS_IMCFLOWPANEL_H
#define BML_DEVTOOLS_IMCFLOWPANEL_H

#include "Panel.h"
#include "ConcurrentRingBuffer.h"
#include "RingBuffer.h"
#include "bml_imc.h"
#include "imgui.h"

#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <unordered_map>

namespace devtools {

struct FlowEntry {
    BML_TopicId topic;
    BML_Mod owner;
    uint64_t timestamp_qpc;
    size_t payload_size;
    uint32_t priority;
    uint32_t flags;
};

struct FlowDisplayEntry {
    float relative_time;
    std::string topic_name;
    std::string publisher_id;
    size_t payload_size;
    uint32_t priority;
};

class ImcFlowPanel : public Panel {
    ConcurrentRingBuffer<FlowEntry, 4096> m_Buffer;
    std::deque<FlowDisplayEntry> m_Display;
    std::unordered_map<uint32_t, std::string> m_TopicNames;
    std::unordered_map<BML_Mod, std::string> m_PublisherNames;
    RingBuffer<float, 300> m_Throughput;
    uint64_t m_BaseTimestamp = 0;
    uint64_t m_PerfFreq = 1;
    bool m_Paused = false;
    bool m_Available = false;
    int m_FrameCount = 0;
    int m_FrameMessages = 0;
    char m_Filter[128]{};

    static constexpr size_t kMaxDisplay = 2048;

    static void TapCallback(const BML_ImcMessageTrace *trace, void *ud) {
        auto *self = static_cast<ImcFlowPanel *>(ud);
        FlowEntry entry{};
        entry.topic = trace->topic;
        entry.owner = trace->owner;
        entry.timestamp_qpc = trace->timestamp_qpc;
        entry.payload_size = trace->payload_size;
        entry.priority = trace->priority;
        entry.flags = trace->flags;
        self->m_Buffer.TryPush(entry);
    }

    const char *ResolveTopic(const bml::ModuleServices &svc, uint32_t id) {
        auto it = m_TopicNames.find(id);
        if (it != m_TopicNames.end()) return it->second.c_str();
        char buf[256]{};
        const auto *bus = svc.Interfaces().ImcBus;
        if (bus && bus->GetTopicName)
            bus->GetTopicName(bus->Context, id, buf, sizeof(buf), nullptr);
        auto [ins, _] = m_TopicNames.emplace(id, buf);
        return ins->second.c_str();
    }

    const char *ResolvePublisher(const bml::ModuleServices &svc, BML_Mod mod) {
        if (!mod) return "<core>";
        auto it = m_PublisherNames.find(mod);
        if (it != m_PublisherNames.end()) return it->second.c_str();
        const char *id = nullptr;
        const auto *modIface = svc.Interfaces().Module;
        if (modIface && modIface->GetModId)
            modIface->GetModId(mod, &id);
        auto [ins, _] = m_PublisherNames.emplace(mod, id ? id : "<unknown>");
        return ins->second.c_str();
    }

    static const char *PriorityLabel(uint32_t p) {
        switch (p) {
            case BML_IMC_PRIORITY_LOW:    return "LOW";
            case BML_IMC_PRIORITY_NORMAL: return "NORM";
            case BML_IMC_PRIORITY_HIGH:   return "HIGH";
            case BML_IMC_PRIORITY_URGENT: return "URG";
            default: return "?";
        }
    }

public:
    const char *Id() const override { return "imc_flow"; }
    const char *Label(const bml::Locale &loc) const override { return loc["tab.imc_flow"]; }

    void OnShow(const bml::ModuleServices &svc) override {
        const auto *bus = svc.Interfaces().ImcBus;
        if (!bus) return;
        if (bus->header.struct_size < offsetof(BML_ImcBusInterface, RegisterMessageTap)
                                      + sizeof(bus->RegisterMessageTap))
            return;
        if (!bus->RegisterMessageTap) return;
        if (bus->RegisterMessageTap(svc.Handle(), TapCallback, this) == BML_RESULT_OK) {
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
        const auto *bus = svc.Interfaces().ImcBus;
        if (bus && bus->UnregisterMessageTap)
            bus->UnregisterMessageTap(svc.Handle());
        m_Available = false;
    }

    void Sample(const bml::ModuleServices &svc) override {
        if (!m_Available || m_Paused) return;
        FlowEntry batch[256];
        size_t n = m_Buffer.Drain(batch, 256);
        m_FrameMessages += static_cast<int>(n);
        for (size_t i = 0; i < n; ++i) {
            FlowDisplayEntry d;
            d.relative_time = static_cast<float>(
                static_cast<double>(batch[i].timestamp_qpc - m_BaseTimestamp)
                / static_cast<double>(m_PerfFreq));
            d.topic_name = ResolveTopic(svc, batch[i].topic);
            d.publisher_id = ResolvePublisher(svc, batch[i].owner);
            d.payload_size = batch[i].payload_size;
            d.priority = batch[i].priority;
            m_Display.push_back(std::move(d));
        }
        while (m_Display.size() > kMaxDisplay) m_Display.pop_front();
        if (++m_FrameCount >= 60) {
            m_Throughput.Push(static_cast<float>(m_FrameMessages));
            m_FrameMessages = 0;
            m_FrameCount = 0;
        }
    }

    void Draw(const bml::ModuleServices &svc) override {
        const auto &loc = svc.Locale();
        if (!m_Available) {
            ImGui::TextColored(ImVec4(1, 0.6f, 0.2f, 1), "%s", loc["label.feature_unavailable"]);
            return;
        }
        char overlay[64];
        float last = m_Throughput.Count() > 0 ? m_Throughput[m_Throughput.Count() - 1] : 0.0f;
        std::snprintf(overlay, sizeof(overlay), "%.0f msg/s", last);
        ImGui::PlotLines(loc["label.throughput"], m_Throughput.Data(), m_Throughput.Size(),
                          m_Throughput.Offset(), overlay, 0.0f, 0.0f, ImVec2(0, 50));
        if (ImGui::Button(m_Paused ? "Resume" : loc["label.pause"])) m_Paused = !m_Paused;
        ImGui::SameLine();
        if (ImGui::Button(loc["label.clear"])) m_Display.clear();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        ImGui::InputText(loc["label.filter"], m_Filter, sizeof(m_Filter));
        ImGui::Separator();
        if (ImGui::BeginTable("FlowTable", 5,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
                | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 0))) {
            ImGui::TableSetupColumn(loc["column.time"], 0, 60);
            ImGui::TableSetupColumn(loc["column.topic"], 0, 200);
            ImGui::TableSetupColumn(loc["column.source"], 0, 120);
            ImGui::TableSetupColumn(loc["column.size"], 0, 60);
            ImGui::TableSetupColumn("Prio", 0, 50);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            for (const auto &e : m_Display) {
                if (m_Filter[0] && e.topic_name.find(m_Filter) == std::string::npos) continue;
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%.3f", e.relative_time);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(e.topic_name.c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(e.publisher_id.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%zu", e.payload_size);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(PriorityLabel(e.priority));
            }
            ImGui::EndTable();
        }
    }
};

} // namespace devtools

#endif // BML_DEVTOOLS_IMCFLOWPANEL_H

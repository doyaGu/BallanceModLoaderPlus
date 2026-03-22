#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"
#include "bml_builtin_interfaces.h"
#include "bml_hook.h"
#include "bml_interface.hpp"
#include "bml_topics.h"
#include "bml_ui.hpp"

#include "imgui.h"

namespace {

struct ImcTopicEntry {
    uint32_t id;
    char name[256];
    size_t subscriber_count;
    uint64_t message_count;
};

struct InterfaceEntry {
    std::string id;
    uint16_t major;
    uint16_t minor;
    std::string provider;
    uint64_t flags;
    uint32_t lease_count;
};

using HookEntry = bml::HookInfo;

struct ModuleEntry {
    std::string id;
    BML_Version version;
};

class DevToolsMod : public bml::Module {
    bml::ui::DrawRegistration m_DrawReg;
    bml::imc::SubscriptionManager m_Subs;
    bool m_Visible = false;
    bool m_ShowImcPanel = true;
    bool m_ShowInterfacePanel = false;
    bool m_ShowHookPanel = false;
    bool m_ShowModulePanel = false;

    std::vector<ImcTopicEntry> m_Topics;
    std::vector<InterfaceEntry> m_Interfaces;
    std::vector<HookEntry> m_Hooks;
    std::vector<ModuleEntry> m_Modules;
    BML_ImcStats m_ImcStats{};

    static void OnDraw(const BML_UIDrawContext *, void *userData) {
        static_cast<DevToolsMod *>(userData)->Draw();
    }

    void RefreshData() {
        RefreshTopics();
        RefreshInterfaces();
        RefreshHooks();
        RefreshModules();
        RefreshImcStats();
    }

    void RefreshTopics() {
        m_Topics.clear();
        const auto *bus = Services().Builtins().ImcBus;
        if (!bus || !bus->GetTopicInfo) return;
        for (uint32_t id = 1; id < 1000; ++id) {
            BML_TopicInfo info = BML_TOPIC_INFO_INIT;
            if (bus->GetTopicInfo(id, &info) == BML_RESULT_OK) {
                ImcTopicEntry entry{};
                entry.id = info.topic_id;
                std::memcpy(entry.name, info.name, sizeof(entry.name));
                entry.subscriber_count = info.subscriber_count;
                entry.message_count = info.message_count;
                m_Topics.push_back(entry);
            }
        }
    }

    void RefreshInterfaces() {
        m_Interfaces.clear();
        const auto *diag = Services().Builtins().Diagnostic;
        if (!diag || !diag->EnumerateInterfaces) return;
        diag->EnumerateInterfaces([](const BML_InterfaceRuntimeDesc *desc, void *ud) {
            auto *self = static_cast<DevToolsMod *>(ud);
            InterfaceEntry entry;
            entry.id = desc->interface_id ? desc->interface_id : "";
            entry.major = desc->abi_version.major;
            entry.minor = desc->abi_version.minor;
            entry.provider = desc->provider_id ? desc->provider_id : "";
            entry.flags = desc->flags;
            entry.lease_count = 0;
            if (self->Services().Builtins().Diagnostic->GetLeaseCount)
                entry.lease_count = self->Services().Builtins().Diagnostic->GetLeaseCount(desc->interface_id);
            self->m_Interfaces.push_back(std::move(entry));
        }, this, 0);
    }

    void RefreshHooks() {
        m_Hooks = Services().Hooks().GetAll();
    }

    void RefreshModules() {
        m_Modules.clear();
        const auto *mod = Services().Builtins().Module;
        if (!mod || !mod->GetLoadedModuleCount || !mod->GetLoadedModuleAt) return;
        uint32_t count = mod->GetLoadedModuleCount();
        for (uint32_t i = 0; i < count; ++i) {
            BML_Mod handle = mod->GetLoadedModuleAt(i);
            if (!handle) continue;
            const char *id = nullptr;
            mod->GetModId(handle, &id);
            BML_Version ver{};
            mod->GetModVersion(handle, &ver);
            ModuleEntry entry;
            entry.id = id ? id : "";
            entry.version = ver;
            m_Modules.push_back(std::move(entry));
        }
    }

    void RefreshImcStats() {
        const auto *bus = Services().Builtins().ImcBus;
        if (bus && bus->GetStats) {
            m_ImcStats = BML_IMC_STATS_INIT;
            bus->GetStats(&m_ImcStats);
        }
    }

    void Draw() {
        if (!m_Visible) return;

        const auto &loc = Services().Locale();

        ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(loc["window.title"], &m_Visible, ImGuiWindowFlags_MenuBar)) {
            ImGui::End();
            return;
        }

        if (ImGui::BeginMenuBar()) {
            if (ImGui::MenuItem(loc["menu.imc"], nullptr, m_ShowImcPanel)) m_ShowImcPanel = !m_ShowImcPanel;
            if (ImGui::MenuItem(loc["menu.interfaces"], nullptr, m_ShowInterfacePanel)) m_ShowInterfacePanel = !m_ShowInterfacePanel;
            if (ImGui::MenuItem(loc["menu.hooks"], nullptr, m_ShowHookPanel)) m_ShowHookPanel = !m_ShowHookPanel;
            if (ImGui::MenuItem(loc["menu.modules"], nullptr, m_ShowModulePanel)) m_ShowModulePanel = !m_ShowModulePanel;
            ImGui::Separator();
            if (ImGui::MenuItem(loc["menu.refresh"])) RefreshData();
            ImGui::EndMenuBar();
        }

        if (m_ShowImcPanel) DrawImcPanel();
        if (m_ShowInterfacePanel) DrawInterfacePanel();
        if (m_ShowHookPanel) DrawHookPanel();
        if (m_ShowModulePanel) DrawModulePanel();

        ImGui::End();
    }

    void DrawImcPanel() {
        const auto &loc = Services().Locale();
        if (!ImGui::CollapsingHeader(loc["panel.imc_bus"], ImGuiTreeNodeFlags_DefaultOpen))
            return;
        ImGui::Text("Published: %llu  Delivered: %llu  Dropped: %llu",
                     m_ImcStats.total_messages_published,
                     m_ImcStats.total_messages_delivered,
                     m_ImcStats.total_messages_dropped);
        ImGui::Text("Topics: %zu  Subscriptions: %zu  RPCs: %zu",
                     m_ImcStats.active_topics,
                     m_ImcStats.active_subscriptions,
                     m_ImcStats.active_rpc_handlers);
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

    void DrawInterfacePanel() {
        const auto &loc = Services().Locale();
        if (!ImGui::CollapsingHeader(loc["panel.interfaces"])) return;
        if (ImGui::BeginTable("Interfaces", 5,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn(loc["column.interface_id"]);
            ImGui::TableSetupColumn(loc["column.version"]);
            ImGui::TableSetupColumn(loc["column.provider"]);
            ImGui::TableSetupColumn(loc["column.flags"]);
            ImGui::TableSetupColumn(loc["column.leases"]);
            ImGui::TableHeadersRow();
            for (const auto &iface : m_Interfaces) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(iface.id.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%u.%u", iface.major, iface.minor);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(iface.provider.c_str());
                ImGui::TableNextColumn(); ImGui::Text("0x%llX", iface.flags);
                ImGui::TableNextColumn(); ImGui::Text("%u", iface.lease_count);
            }
            ImGui::EndTable();
        }
    }

    void DrawHookPanel() {
        const auto &loc = Services().Locale();
        if (!ImGui::CollapsingHeader(loc["panel.hooks"])) return;
        if (ImGui::BeginTable("Hooks", 5,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn(loc["column.owner"]);
            ImGui::TableSetupColumn(loc["column.target"]);
            ImGui::TableSetupColumn(loc["column.address"]);
            ImGui::TableSetupColumn(loc["column.priority"]);
            ImGui::TableSetupColumn(loc["column.flags"]);
            ImGui::TableHeadersRow();
            for (const auto &h : m_Hooks) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(h.owner.c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(h.name.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%p", h.address);
                ImGui::TableNextColumn(); ImGui::Text("%d", h.priority);
                ImGui::TableNextColumn();
                if (h.flags & BML_HOOK_FLAG_CONFLICT)
                    ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", loc["status.conflict"]);
                else
                    ImGui::Text("0x%X", h.flags);
            }
            ImGui::EndTable();
        }
    }

    void DrawModulePanel() {
        const auto &loc = Services().Locale();
        if (!ImGui::CollapsingHeader(loc["panel.loaded_modules"])) return;
        if (ImGui::BeginTable("Modules", 2,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn(loc["column.module_id"]);
            ImGui::TableSetupColumn(loc["column.version"]);
            ImGui::TableHeadersRow();
            for (const auto &m : m_Modules) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(m.id.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%u.%u.%u",
                    m.version.major, m.version.minor, m.version.patch);
            }
            ImGui::EndTable();
        }
    }

public:
    BML_Result OnAttach(bml::ModuleServices &services) override {
        Services().Locale().Load(nullptr);
        services.Log().Info("devtools", "DevTools module attached");

        m_DrawReg = bml::ui::RegisterDraw(
            Handle(), "bml.devtools.overlay", 900, OnDraw, this);

        m_Subs = services.CreateSubscriptions();
        m_Subs.Add(BML_TOPIC_INPUT_KEY_DOWN, [this](const bml::imc::Message &msg) {
            // DIK_F12 = 0x58
            if (msg.Size() >= sizeof(uint32_t)) {
                uint32_t key = *static_cast<const uint32_t *>(msg.Data());
                if (key == 0x58) {
                    m_Visible = !m_Visible;
                    if (m_Visible) RefreshData();
                }
            }
        });

        return BML_RESULT_OK;
    }

    void OnDetach() override {
        m_Subs.Clear();
        m_DrawReg.Reset();
    }
};

} // namespace

BML_DEFINE_MODULE(DevToolsMod)

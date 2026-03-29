#ifndef BML_DEVTOOLS_INTERFACEPANEL_H
#define BML_DEVTOOLS_INTERFACEPANEL_H

#include "Panel.h"
#include "bml_interface.hpp"
#include "imgui.h"

#include <string>
#include <vector>

namespace devtools {

struct InterfaceEntry {
    std::string id;
    uint16_t major;
    uint16_t minor;
    std::string provider;
    uint64_t flags;
    uint32_t lease_count;
};

class InterfacePanel : public Panel {
    std::vector<InterfaceEntry> m_Entries;

public:
    const char *Id() const override { return "interfaces"; }
    const char *Label(const bml::Locale &loc) const override { return loc["tab.interfaces"]; }

    void Refresh(const bml::ModuleServices &svc) override {
        m_Entries.clear();
        const auto *diag = svc.Interfaces().Diagnostic;
        if (!diag || !diag->Context || !diag->EnumerateInterfaces) return;
        struct Ctx { InterfacePanel *self; const BML_CoreDiagnosticInterface *diag; };
        Ctx ctx{this, diag};
        diag->EnumerateInterfaces(diag->Context, [](const BML_InterfaceRuntimeDesc *desc, void *ud) {
            auto *c = static_cast<Ctx *>(ud);
            InterfaceEntry entry;
            entry.id = desc->interface_id ? desc->interface_id : "";
            entry.major = desc->abi_version.major;
            entry.minor = desc->abi_version.minor;
            entry.provider = desc->provider_id ? desc->provider_id : "";
            entry.flags = desc->flags;
            entry.lease_count = 0;
            if (c->diag->GetLeaseCount)
                entry.lease_count = c->diag->GetLeaseCount(c->diag->Context, desc->interface_id);
            c->self->m_Entries.push_back(std::move(entry));
        }, &ctx, 0);
    }

    void Draw(const bml::ModuleServices &svc) override {
        const auto &loc = svc.Locale();
        if (ImGui::BeginTable("Interfaces", 5,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn(loc["column.interface_id"]);
            ImGui::TableSetupColumn(loc["column.version"]);
            ImGui::TableSetupColumn(loc["column.provider"]);
            ImGui::TableSetupColumn(loc["column.flags"]);
            ImGui::TableSetupColumn(loc["column.leases"]);
            ImGui::TableHeadersRow();
            for (const auto &e : m_Entries) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(e.id.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%u.%u", e.major, e.minor);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(e.provider.c_str());
                ImGui::TableNextColumn(); ImGui::Text("0x%llX", e.flags);
                ImGui::TableNextColumn(); ImGui::Text("%u", e.lease_count);
            }
            ImGui::EndTable();
        }
    }
};

} // namespace devtools

#endif // BML_DEVTOOLS_INTERFACEPANEL_H

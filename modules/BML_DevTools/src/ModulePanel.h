#ifndef BML_DEVTOOLS_MODULEPANEL_H
#define BML_DEVTOOLS_MODULEPANEL_H

#include "Panel.h"
#include "imgui.h"

#include <string>
#include <vector>

namespace devtools {

struct ModuleInfoEntry {
    std::string id;
    std::string name;
    std::string description;
    std::string authors;
    std::string directory;
    BML_Version version{};
};

class ModulePanel : public Panel {
    std::vector<ModuleInfoEntry> m_Modules;

public:
    const char *Id() const override { return "modules"; }
    const char *Label(const bml::Locale &loc) const override { return loc["tab.modules"]; }

    void Refresh(const bml::ModuleServices &svc) override {
        m_Modules.clear();
        const auto *mod = svc.Interfaces().Module;
        if (!mod || !mod->Context || !mod->GetLoadedModuleCount || !mod->GetLoadedModuleAt) return;
        uint32_t count = mod->GetLoadedModuleCount(mod->Context);
        for (uint32_t i = 0; i < count; ++i) {
            BML_Mod handle = mod->GetLoadedModuleAt(mod->Context, i);
            if (!handle) continue;

            ModuleInfoEntry entry;
            const char *s = nullptr;

            if (mod->GetModId(handle, &s) == BML_RESULT_OK && s) entry.id = s;
            mod->GetModVersion(handle, &entry.version);
            if (mod->GetModName && mod->GetModName(handle, &s) == BML_RESULT_OK && s) entry.name = s;
            if (mod->GetModDescription && mod->GetModDescription(handle, &s) == BML_RESULT_OK && s) entry.description = s;
            if (mod->GetModDirectory && mod->GetModDirectory(handle, &s) == BML_RESULT_OK && s) entry.directory = s;

            if (mod->GetModAuthorCount && mod->GetModAuthorAt) {
                uint32_t ac = 0;
                mod->GetModAuthorCount(handle, &ac);
                for (uint32_t a = 0; a < ac; ++a) {
                    if (mod->GetModAuthorAt(handle, a, &s) == BML_RESULT_OK && s) {
                        if (!entry.authors.empty()) entry.authors += ", ";
                        entry.authors += s;
                    }
                }
            }

            m_Modules.push_back(std::move(entry));
        }
    }

    void Draw(const bml::ModuleServices &svc) override {
        const auto &loc = svc.Locale();
        if (ImGui::BeginTable("Modules", 5,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn(loc["column.module_id"]);
            ImGui::TableSetupColumn(loc["column.name"]);
            ImGui::TableSetupColumn(loc["column.version"]);
            ImGui::TableSetupColumn("Authors");
            ImGui::TableSetupColumn("Directory");
            ImGui::TableHeadersRow();
            for (const auto &m : m_Modules) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(m.id.c_str());
                if (!m.description.empty() && ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", m.description.c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(m.name.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%u.%u.%u", m.version.major, m.version.minor, m.version.patch);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(m.authors.c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(m.directory.c_str());
            }
            ImGui::EndTable();
        }
    }
};

} // namespace devtools

#endif // BML_DEVTOOLS_MODULEPANEL_H

#ifndef BML_DEVTOOLS_HOOKPANEL_H
#define BML_DEVTOOLS_HOOKPANEL_H

#include "Panel.h"
#include "bml_hook.hpp"
#include "imgui.h"

#include <vector>

namespace devtools {

class HookPanel : public Panel {
    std::vector<bml::HookInfo> m_Hooks;

    static const char *TypeName(uint32_t t) {
        switch (t) {
            case BML_HOOK_TYPE_VTABLE:    return "VTable";
            case BML_HOOK_TYPE_INLINE:    return "Inline";
            case BML_HOOK_TYPE_WIN32_API: return "Win32";
            case BML_HOOK_TYPE_BEHAVIOR:  return "Behavior";
            default:                      return "Unknown";
        }
    }

public:
    const char *Id() const override { return "hooks"; }
    const char *Label(const bml::Locale &loc) const override { return loc["tab.hooks"]; }

    void Refresh(const bml::ModuleServices &svc) override {
        m_Hooks = svc.Hooks().GetAll();
    }

    void Draw(const bml::ModuleServices &svc) override {
        const auto &loc = svc.Locale();
        if (ImGui::BeginTable("Hooks", 6,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn(loc["column.owner"]);
            ImGui::TableSetupColumn(loc["column.target"]);
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn(loc["column.address"]);
            ImGui::TableSetupColumn(loc["column.priority"]);
            ImGui::TableSetupColumn(loc["column.flags"]);
            ImGui::TableHeadersRow();
            for (const auto &h : m_Hooks) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(h.owner.c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(h.name.c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(TypeName(h.hook_type));
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
};

} // namespace devtools

#endif // BML_DEVTOOLS_HOOKPANEL_H

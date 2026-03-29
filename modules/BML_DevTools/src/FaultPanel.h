#ifndef BML_DEVTOOLS_FAULTPANEL_H
#define BML_DEVTOOLS_FAULTPANEL_H

#include "Panel.h"
#include "bml_interface.hpp"
#include "imgui.h"

#include <cstdio>

namespace devtools {

class FaultPanel : public Panel {
    bool m_HasError = false;
    int32_t m_ResultCode = 0;
    char m_Message[512]{};
    char m_ApiName[128]{};
    char m_SourceFile[256]{};
    int32_t m_SourceLine = 0;

public:
    const char *Id() const override { return "faults"; }
    const char *Label(const bml::Locale &) const override { return "Faults"; }

    void Refresh(const bml::ModuleServices &svc) override {
        const auto *diag = svc.Interfaces().Diagnostic;
        if (!diag || !diag->Context || !diag->GetLastError) {
            m_HasError = false;
            return;
        }
        BML_ErrorInfo info = BML_ERROR_INFO_INIT;
        if (diag->GetLastError(diag->Context, &info) == BML_RESULT_OK && info.result_code != 0) {
            m_HasError = true;
            m_ResultCode = info.result_code;
            std::snprintf(m_Message, sizeof(m_Message), "%s", info.message ? info.message : "");
            std::snprintf(m_ApiName, sizeof(m_ApiName), "%s", info.api_name ? info.api_name : "");
            std::snprintf(m_SourceFile, sizeof(m_SourceFile), "%s", info.source_file ? info.source_file : "");
            m_SourceLine = info.source_line;
        } else {
            m_HasError = false;
        }
    }

    void Draw(const bml::ModuleServices &svc) override {
        const auto *diag = svc.Interfaces().Diagnostic;

        if (!m_HasError) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "No errors recorded");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Last Error:");
            ImGui::Separator();
            if (ImGui::BeginTable("ErrorInfo", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Value");
                ImGui::TableHeadersRow();

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted("Result Code");
                ImGui::TableNextColumn(); ImGui::Text("0x%08X (%d)", m_ResultCode, m_ResultCode);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted("API");
                ImGui::TableNextColumn(); ImGui::TextUnformatted(m_ApiName);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted("Message");
                ImGui::TableNextColumn(); ImGui::TextWrapped("%s", m_Message);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted("Source");
                ImGui::TableNextColumn(); ImGui::Text("%s:%d", m_SourceFile, m_SourceLine);

                ImGui::EndTable();
            }
        }

        if (diag && diag->ClearLastError) {
            if (ImGui::Button("Clear Error")) {
                diag->ClearLastError(diag->Context);
                m_HasError = false;
            }
        }
    }
};

} // namespace devtools

#endif // BML_DEVTOOLS_FAULTPANEL_H

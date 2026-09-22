#include "SegmentGui.h"
#include <cmath>
#include <cstddef>

void SegmentGui::update() {
    if (!visible_)
        return;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, bg_color);

    const bool doScale = font_scale_ != 1.0f;
    if (doScale) {
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * font_scale_);
    }

    constexpr auto WinFlags = ImGuiWindowFlags_NoTitleBar |
                              ImGuiWindowFlags_AlwaysAutoResize |
                              ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoFocusOnAppearing |
                              ImGuiWindowFlags_NoBringToFrontOnFocus |
                              ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("Segments", nullptr, WinFlags)) {
        ImGui::TextUnformatted(current_level_name_.c_str());

        if (ImGui::BeginTable("##Segments", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Sector", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Delta", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableHeadersRow();
            for (std::size_t i = 0; i < state_.size(); ++i) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("#%zu", i + 1);
                ImGui::TableSetColumnIndex(1);
                const auto time = state_.segment(i);
                ImGui::Text("%.3fs", time);
                ImGui::TableSetColumnIndex(2);
                auto &time_to_compare = state_.segment_target(i);
                ImGui::PushID(static_cast<int>(i));
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::DragFloat("##target", &time_to_compare,
                                 0.1f, 0.0f, 1e6f,
                                 time_to_compare >= 0.0f ? "%.3fs" : "----");
                ImGui::PopID();
                ImGui::TableSetColumnIndex(3);
                if (time_to_compare < 0.f)
                    ImGui::Text("----");
                else
                    ImGui::Text("%+.3fs", time - time_to_compare);

                if (cursor_visible_ && i == state_.get_current_segment()) {
                    ImVec4 color = lag_color;
                    if (time_to_compare < 0 || std::abs(time - time_to_compare) < 1e-7f)
                        color = even_color;
                    else if (time < time_to_compare)
                        color = lead_color;
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(color));
                }
            }

            ImGui::EndTable();
        }

        if (settings_visible_ && ImGui::TreeNode("History Settings")) {
            if (ImGui::Button("Clear History"))
                state_.clear_history();
            ImGui::Checkbox("Update History", &state_.is_saving_);
            ImGui::TreePop();
        }
    }
    ImGui::End();

    if (doScale) {
        ImGui::PopFont();
    }
    ImGui::PopStyleColor();
}

#pragma once
#include <format>
#include <string>
#include <string_view>
#include <imgui.h>

#include "PerLevelSegmentState.h"

class SegmentGui {
public:
    SegmentGui(PerLevelSegmentState &state, const int level)
        : state_(state), current_level_name_(std::format("Level {}", level)) {}

    SegmentGui(PerLevelSegmentState &state, const std::string_view level_name)
        : state_(state), current_level_name_(level_name) {}

    void update();

    void set_visible(bool visible) { visible_ = visible; }
    void set_cursor_visible(bool visible) { cursor_visible_ = visible; }
    void set_settings_visible(bool visible) { settings_visible_ = visible; }
    void set_font_scale(float scale) { font_scale_ = scale; }

    PerLevelSegmentState &state_;
    const std::string current_level_name_;
    bool visible_ = true;
    bool cursor_visible_ = true;
    bool settings_visible_ = false;
    float font_scale_ = 0.7f;

    static constexpr ImVec4 lead_color{0.2f, 0.8f, 0.2f, 0.75f};
    static constexpr ImVec4 even_color{1.0f, 0.66f, 0.0f, 0.75f};
    static constexpr ImVec4 lag_color{0.85f, 0.08f, 0.25f, 0.75f};
    static constexpr ImVec4 bg_color{0.5f, 0.5f, 0.5f, 0.3f};
};

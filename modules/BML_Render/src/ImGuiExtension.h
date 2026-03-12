#ifndef BML_RENDER_EXTENSION_H
#define BML_RENDER_EXTENSION_H

#include "imgui.h"

// BML_EXT_ImGui Extension API v1.0
// Exposes ImGui functionality to other BML modules

struct BML_ImGuiExtension {
    // Version info
    uint16_t major;
    uint16_t minor;
    
    // Window management
    bool (*Begin)(const char* name, bool* p_open, ImGuiWindowFlags flags);
    void (*End)();
    
    // Widgets
    void (*Text)(const char* fmt, ...);
    bool (*Button)(const char* label, float width, float height);
    bool (*Checkbox)(const char* label, bool* v);
    bool (*InputText)(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags);
    bool (*InputFloat)(const char* label, float* v, float step, float step_fast, const char* format, ImGuiInputTextFlags flags);
    bool (*InputInt)(const char* label, int* v, int step, int step_fast, ImGuiInputTextFlags flags);
    bool (*SliderFloat)(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags);
    bool (*SliderInt)(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags);
    bool (*ColorEdit3)(const char* label, float col[3], ImGuiColorEditFlags flags);
    bool (*ColorEdit4)(const char* label, float col[4], ImGuiColorEditFlags flags);
    
    // Layout
    void (*SameLine)(float offset_from_start_x, float spacing);
    void (*Separator)();
    void (*Spacing)();
    void (*NewLine)();
    
    // Cursor/Layout positioning
    void (*SetNextWindowPos)(float x, float y, ImGuiCond cond);
    void (*SetNextWindowSize)(float width, float height, ImGuiCond cond);
    
    // Context info (read-only)
    ImGuiContext* (*GetCurrentContext)();
};

#endif // BML_RENDER_EXTENSION_H

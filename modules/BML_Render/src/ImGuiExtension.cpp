#include "ImGuiExtension.h"
#include "Overlay.h"
#include "imgui.h"

// Forward declarations of wrapper functions
static bool ImGui_Begin(const char* name, bool* p_open, ImGuiWindowFlags flags) {
    Overlay::ImGuiContextScope scope;
    return ImGui::Begin(name, p_open, flags);
}

static void ImGui_End() {
    Overlay::ImGuiContextScope scope;
    ImGui::End();
}

static void ImGui_Text(const char* fmt, ...) {
    Overlay::ImGuiContextScope scope;
    va_list args;
    va_start(args, fmt);
    ImGui::TextV(fmt, args);
    va_end(args);
}

static bool ImGui_Button(const char* label, float width, float height) {
    Overlay::ImGuiContextScope scope;
    return ImGui::Button(label, ImVec2(width, height));
}

static bool ImGui_Checkbox(const char* label, bool* v) {
    Overlay::ImGuiContextScope scope;
    return ImGui::Checkbox(label, v);
}

static bool ImGui_InputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags) {
    Overlay::ImGuiContextScope scope;
    return ImGui::InputText(label, buf, buf_size, flags);
}

static bool ImGui_InputFloat(const char* label, float* v, float step, float step_fast, const char* format, ImGuiInputTextFlags flags) {
    Overlay::ImGuiContextScope scope;
    return ImGui::InputFloat(label, v, step, step_fast, format, flags);
}

static bool ImGui_InputInt(const char* label, int* v, int step, int step_fast, ImGuiInputTextFlags flags) {
    Overlay::ImGuiContextScope scope;
    return ImGui::InputInt(label, v, step, step_fast, flags);
}

static bool ImGui_SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
    Overlay::ImGuiContextScope scope;
    return ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
}

static bool ImGui_SliderInt(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) {
    Overlay::ImGuiContextScope scope;
    return ImGui::SliderInt(label, v, v_min, v_max, format, flags);
}

static bool ImGui_ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags) {
    Overlay::ImGuiContextScope scope;
    return ImGui::ColorEdit3(label, col, flags);
}

static bool ImGui_ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags) {
    Overlay::ImGuiContextScope scope;
    return ImGui::ColorEdit4(label, col, flags);
}

static void ImGui_SameLine(float offset_from_start_x, float spacing) {
    Overlay::ImGuiContextScope scope;
    ImGui::SameLine(offset_from_start_x, spacing);
}

static void ImGui_Separator() {
    Overlay::ImGuiContextScope scope;
    ImGui::Separator();
}

static void ImGui_Spacing() {
    Overlay::ImGuiContextScope scope;
    ImGui::Spacing();
}

static void ImGui_NewLine() {
    Overlay::ImGuiContextScope scope;
    ImGui::NewLine();
}

static void ImGui_SetNextWindowPos(float x, float y, ImGuiCond cond) {
    Overlay::ImGuiContextScope scope;
    ImGui::SetNextWindowPos(ImVec2(x, y), cond);
}

static void ImGui_SetNextWindowSize(float width, float height, ImGuiCond cond) {
    Overlay::ImGuiContextScope scope;
    ImGui::SetNextWindowSize(ImVec2(width, height), cond);
}

static ImGuiContext* ImGui_GetCurrentContext() {
    return Overlay::GetImGuiContext();
}

// Global extension instance
static BML_ImGuiExtension g_ImGuiExtension = {
    1,  // major version
    0,  // minor version
    ImGui_Begin,
    ImGui_End,
    ImGui_Text,
    ImGui_Button,
    ImGui_Checkbox,
    ImGui_InputText,
    ImGui_InputFloat,
    ImGui_InputInt,
    ImGui_SliderFloat,
    ImGui_SliderInt,
    ImGui_ColorEdit3,
    ImGui_ColorEdit4,
    ImGui_SameLine,
    ImGui_Separator,
    ImGui_Spacing,
    ImGui_NewLine,
    ImGui_SetNextWindowPos,
    ImGui_SetNextWindowSize,
    ImGui_GetCurrentContext
};

BML_ImGuiExtension* GetImGuiExtension() {
    return &g_ImGuiExtension;
}

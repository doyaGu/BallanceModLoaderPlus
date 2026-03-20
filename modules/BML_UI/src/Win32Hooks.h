#ifndef BML_UI_WIN32_HOOKS_H
#define BML_UI_WIN32_HOOKS_H

#include <Windows.h>

struct ImGuiContext;

namespace bml_ui {

// Install/uninstall Win32 message pump hooks for ImGui input
bool InstallWin32Hooks();
bool UninstallWin32Hooks();

// Called by UIMod to tell the hooks which window/context to use
void SetHookTarget(HWND hwnd, ImGuiContext *ctx);

} // namespace bml_ui

#endif // BML_UI_WIN32_HOOKS_H

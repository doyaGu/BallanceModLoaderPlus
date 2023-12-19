#include "Overlay.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <Psapi.h>

#include "CKRenderContext.h"

#include "imgui_internal.h"
#include "backends/imgui_impl_ck2.h"
#include "backends/imgui_impl_win32.h"

#include "Hooks.h"

extern HookApi *g_HookApi;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Overlay {
    typedef LRESULT (CALLBACK *LPFNWNDPROC)(HWND, UINT, WPARAM, LPARAM);

    ImGuiContext *g_ImGuiContext = nullptr;
    bool g_ImGuiReady = false;
    bool g_RenderReady = false;

    LPFNWNDPROC g_WndProc = nullptr;
    LPFNWNDPROC g_MainWndProc = nullptr;
    LPFNWNDPROC g_RenderWndProc = nullptr;

    LRESULT OnWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        ImGuiContext *const backupContext = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(g_ImGuiContext);

        LRESULT res = ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);

        ImGui::SetCurrentContext(backupContext);

        return res;
    }

    LRESULT MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (OnWndProc(hWnd, msg, wParam, lParam))
            return 1;
        return g_MainWndProc(hWnd, msg, wParam, lParam);
    }

    LRESULT RenderWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (OnWndProc(hWnd, msg, wParam, lParam))
            return 1;
        return g_RenderWndProc(hWnd, msg, wParam, lParam);
    }

    void HookWndProc(HWND hMainWnd, HWND hRenderWnd) {
        g_MainWndProc = reinterpret_cast<LPFNWNDPROC>(GetWindowLongPtr(hMainWnd, GWLP_WNDPROC));
        SetWindowLongPtr(hMainWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(MainWndProc));
        if (hMainWnd != hRenderWnd) {
            g_RenderWndProc = reinterpret_cast<LPFNWNDPROC>(GetWindowLongPtr(hRenderWnd, GWLP_WNDPROC));
            SetWindowLongPtr(hRenderWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(RenderWndProc));
        }
    }

    void UnhookWndProc(HWND hMainWnd, HWND hRenderWnd) {
        if (g_MainWndProc)
            SetWindowLongPtr(hMainWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_MainWndProc));
        if (g_RenderWndProc)
            SetWindowLongPtr(hRenderWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_RenderWndProc));
    }


    void *GetModuleBaseAddress(HMODULE hModule) {
        if (!hModule)
            return nullptr;

        MODULEINFO moduleInfo;
        ::GetModuleInformation(::GetCurrentProcess(), hModule, &moduleInfo, sizeof(moduleInfo));

        return moduleInfo.lpBaseOfDll;
    }

    ImGuiContext *GetImGuiContext() {
        return g_ImGuiContext;
    }

    bool ImGuiInit(CKContext *context, bool originalPlayer) {
        if (!g_ImGuiReady) {
            // Setup Dear ImGui context
            IMGUI_CHECKVERSION();

            ImGuiContext *const backupContext = ImGui::GetCurrentContext();
            g_ImGuiContext = ImGui::CreateContext();
            if (!g_ImGuiContext)
                return false;

            ImGuiIO &io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard;

            ImFontConfig fontCfg;
            fontCfg.SizePixels = 32.0f;
            io.Fonts->AddFontDefault(&fontCfg);

            if (originalPlayer) {
                g_WndProc = (LPFNWNDPROC)((char *) GetModuleBaseAddress(::GetModuleHandleA("Player.exe")) + 0x39F0);
                g_HookApi->CreateHook((void *) g_WndProc, (void *) MainWndProc, (void **) &g_MainWndProc);
                g_HookApi->EnableHook((void *) g_WndProc);
            } else {
                HookWndProc((HWND) context->GetMainWindow(),
                            (HWND) context->GetPlayerRenderContext()->GetWindowHandle());
            }

            if (!ImGui_ImplCK2_Init(context))
                return false;

            if (!ImGui_ImplWin32_Init(context->GetMainWindow()))
                return false;

            // Restore previous context in case this was called from a new runtime.
            ImGui::SetCurrentContext(backupContext);
            g_ImGuiReady = true;
        }

        g_RenderReady = false;
        return true;
    }

    void ImGuiShutdown(CKContext *context, bool originalPlayer) {
        g_RenderReady = false;

        if (g_ImGuiReady) {
            ImGuiContext *const backupContext = ImGui::GetCurrentContext();
            ImGui::SetCurrentContext(g_ImGuiContext);

            ImGui_ImplWin32_Shutdown();
            ImGui_ImplCK2_Shutdown();

            if (originalPlayer) {
                g_HookApi->DisableHook((void *) g_WndProc);
                g_HookApi->RemoveHook((void *) g_WndProc);
                g_WndProc = nullptr;
            } else {
                UnhookWndProc((HWND) context->GetMainWindow(),
                              (HWND) context->GetPlayerRenderContext()->GetWindowHandle());
            }
            ImGui::DestroyContext();

            ImGui::SetCurrentContext(backupContext);
            g_ImGuiReady = false;
        }
    }

    void ImGuiNewFrame() {
        if (g_ImGuiReady) {
            ImGui_ImplCK2_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
        }
    }

    void ImGuiRender() {
        if (g_ImGuiReady) {
            ImGui::Render();
            g_RenderReady = true;
        }
    }

    void ImGuiOnRender() {
        if (g_RenderReady) {
            ImGuiContext *const backupContext = ImGui::GetCurrentContext();
            ImGui::SetCurrentContext(g_ImGuiContext);

            ImGui_ImplCK2_RenderDrawData(ImGui::GetDrawData());

            ImGui::SetCurrentContext(backupContext);
        }
    }
}
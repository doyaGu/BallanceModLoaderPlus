#include "Overlay.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <MinHook.h>

#include "imgui_internal.h"
#include "imgui_impl_ck2.h"
#include "backends/imgui_impl_win32.h"

#include "HookUtils.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Overlay {
    typedef LRESULT (CALLBACK *LPFNWNDPROC)(HWND, UINT, WPARAM, LPARAM);

    ImGuiContext *g_ImGuiContext = nullptr;
    ImGuiContext *g_PrevImGuiContext = nullptr;
    bool g_ImGuiReady = false;
    bool g_RenderReady = false;

    LPFNWNDPROC g_WndProc = nullptr;
    LPFNWNDPROC g_MainWndProc = nullptr;

    LRESULT OnWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        ImGuiSetCurrentContext();

        LRESULT res;
        if (msg == WM_IME_COMPOSITION) {
            if (lParam & GCS_RESULTSTR) {
                HIMC hIMC = ImmGetContext(hWnd);
                LONG len = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, nullptr, 0) / (LONG) sizeof(WCHAR);
                auto *buf = new WCHAR[len + 1];
                ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, buf, len * sizeof(WCHAR));
                buf[len] = '\0';
                ImmReleaseContext(hWnd, hIMC);
                for (int i = 0; i < len; ++i) {
                    ImGui::GetIO().AddInputCharacterUTF16(buf[i]);
                }
                delete[] buf;
            }
            res = 1;
        } else {
            res = ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
        }

        ImGuiRestorePreviousContext();
        return res;
    }

    LRESULT MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (OnWndProc(hWnd, msg, wParam, lParam))
            return 1;
        return g_MainWndProc(hWnd, msg, wParam, lParam);
    }

    void HookWndProc(HWND hMainWnd) {
        g_MainWndProc = reinterpret_cast<LPFNWNDPROC>(GetWindowLongPtr(hMainWnd, GWLP_WNDPROC));
        SetWindowLongPtr(hMainWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(MainWndProc));
    }

    void UnhookWndProc(HWND hMainWnd) {
        if (g_MainWndProc)
            SetWindowLongPtr(hMainWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_MainWndProc));
    }

    ImGuiContext *GetImGuiContext() {
        return g_ImGuiContext;
    }

    void ImGuiSetCurrentContext() {
        g_PrevImGuiContext = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(g_ImGuiContext);
    }

    void ImGuiRestorePreviousContext() {
        ImGui::SetCurrentContext(g_PrevImGuiContext);
    }

    bool ImGuiCreateContext(CKContext *context, bool originalPlayer) {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();

        g_PrevImGuiContext = ImGui::GetCurrentContext();
        g_ImGuiContext = ImGui::CreateContext();
        if (!g_ImGuiContext)
            return false;

        if (originalPlayer) {
            g_WndProc = (LPFNWNDPROC) ((char *) utils::GetModuleBaseAddress(::GetModuleHandleA("Player.exe")) + 0x39F0);
            MH_CreateHook((void *) g_WndProc, (void *) MainWndProc, (void **) &g_MainWndProc);
            MH_EnableHook((void *) g_WndProc);
        } else {
            HookWndProc((HWND) context->GetMainWindow());
        }

        ImGuiRestorePreviousContext();
        return true;
    }

    void ImGuiDestroyContext(CKContext *context, bool originalPlayer) {
        ImGuiSetCurrentContext();

        if (originalPlayer) {
            MH_DisableHook((void *) g_WndProc);
            MH_RemoveHook((void *) g_WndProc);
            g_WndProc = nullptr;
        } else {
            UnhookWndProc((HWND) context->GetMainWindow());
        }

        ImGui::DestroyContext();
        ImGuiRestorePreviousContext();
    }

    bool ImGuiInit(CKContext *context) {
        if (!g_ImGuiReady) {
            ImGuiSetCurrentContext();

            if (!ImGui_ImplWin32_Init(context->GetMainWindow()))
                return false;

            if (!ImGui_ImplCK2_Init(context))
                return false;

            ImGuiRestorePreviousContext();

            g_RenderReady = false;
            g_ImGuiReady = true;
        }

        return true;
    }

    void ImGuiShutdown() {
        if (g_ImGuiReady) {
            ImGuiSetCurrentContext();

            ImGui_ImplCK2_Shutdown();
            ImGui_ImplWin32_Shutdown();

            ImGuiRestorePreviousContext();

            g_RenderReady = false;
            g_ImGuiReady = false;
        }
    }

    void ImGuiNewFrame() {
        if (g_ImGuiReady) {
            g_RenderReady = false;

            ImGui_ImplWin32_NewFrame();
            ImGui_ImplCK2_NewFrame();
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
            ImGuiSetCurrentContext();

            ImGui_ImplCK2_RenderDrawData(ImGui::GetDrawData());

            ImGuiRestorePreviousContext();
        }
    }
}
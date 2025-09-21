#include "Overlay.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <CommCtrl.h>

#include <vector>

#include "imgui_internal.h"
#include "imgui_impl_ck2.h"
#define IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
#include "backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Overlay {
    namespace {
        constexpr UINT_PTR kImGuiSubclassId = 0x1A2B3C4D;
        HWND g_SubclassTarget = nullptr;
        bool g_SubclassInstalled = false;

        LRESULT CALLBACK ImGuiSubProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

        bool RemoveImGuiSubclass() {
            if (!g_SubclassInstalled || !g_SubclassTarget) {
                g_SubclassTarget = nullptr;
                g_SubclassInstalled = false;
                return true;
            }

            bool removed = ::RemoveWindowSubclass(g_SubclassTarget, ImGuiSubProc, kImGuiSubclassId) != FALSE;
            g_SubclassTarget = nullptr;
            g_SubclassInstalled = false;
            return removed;
        }

        bool InstallImGuiSubclass(HWND hwnd) {
            if (!hwnd)
                return false;

            if (g_SubclassInstalled) {
                if (g_SubclassTarget == hwnd)
                    return true;
                if (!RemoveImGuiSubclass())
                    return false;
            }

            if (!::SetWindowSubclass(hwnd, ImGuiSubProc, kImGuiSubclassId, 0))
                return false;

            g_SubclassTarget = hwnd;
            g_SubclassInstalled = true;
            return true;
        }

        void HandleImeComposition(HWND hWnd, LPARAM lParam) {
            if ((lParam & GCS_RESULTSTR) == 0)
                return;

            HIMC hImc = ImmGetContext(hWnd);
            if (!hImc)
                return;

            LONG lengthInBytes = ImmGetCompositionStringW(hImc, GCS_RESULTSTR, nullptr, 0);
            if (lengthInBytes > 0) {
                size_t length = static_cast<size_t>(lengthInBytes) / sizeof(WCHAR);
                std::vector<WCHAR> buffer(length + 1, L'\0');
                ImmGetCompositionStringW(hImc, GCS_RESULTSTR, buffer.data(), lengthInBytes);
                for (size_t i = 0; i < length; ++i) {
                    ImGui::GetIO().AddInputCharacterUTF16(buffer[i]);
                }
            }

            ImmReleaseContext(hWnd, hImc);
        }

        LRESULT CALLBACK ImGuiSubProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR /*dwRefData*/) {
            if (!GetImGuiContext())
                return DefSubclassProc(hWnd, msg, wParam, lParam);

            ImGuiContextScope scope;

            if (msg == WM_IME_COMPOSITION) {
                HandleImeComposition(hWnd, lParam);
                return 0;
            }

            if (msg == WM_NCDESTROY) {
                RemoveImGuiSubclass();
            }

            LRESULT imguiResult = ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
            if (imguiResult != 0)
                return imguiResult;

            return DefSubclassProc(hWnd, msg, wParam, lParam);
        }
    }

    ImGuiContext *g_ImGuiContext = nullptr;
    bool g_ImGuiReady = false;
    bool g_RenderReady = false;
    bool g_NewFrame = false;

    ImGuiContext *GetImGuiContext() {
        return g_ImGuiContext;
    }

    ImGuiContext *ImGuiCreateContext() {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();

        ImGuiContext *previousContext = ImGui::GetCurrentContext();
        g_ImGuiContext = ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::SetCurrentContext(previousContext);

        return g_ImGuiContext;
    }

    void ImGuiDestroyContext() {
        ImGuiContextScope scope;
        ImGui::DestroyContext();
    }

    bool ImGuiInitPlatform(CKContext *context) {
        ImGuiContextScope scope;

        HWND hwnd = context ? (HWND) context->GetMainWindow() : nullptr;
        if (!hwnd)
            return false;

        if (!ImGui_ImplWin32_Init(hwnd))
            return false;

        if (!InstallImGuiSubclass(hwnd))
            return false;

        return true;
    }

    bool ImGuiInitRenderer(CKContext *context) {
        ImGuiContextScope scope;

        if (!ImGui_ImplCK2_Init(context))
            return false;

        g_RenderReady = false;
        g_ImGuiReady = true;

        return true;
    }

    void ImGuiShutdownPlatform(CKContext *context) {
        (void) context;
        ImGuiContextScope scope;

        RemoveImGuiSubclass();
        ImGui_ImplWin32_Shutdown();

        g_RenderReady = false;
        g_ImGuiReady = false;
    }

    void ImGuiShutdownRenderer(CKContext *context) {
        ImGuiContextScope scope;

        ImGui_ImplCK2_Shutdown();

        g_RenderReady = false;
        g_ImGuiReady = false;
    }

    void ImGuiNewFrame() {
        if (g_ImGuiReady && !g_NewFrame) {
            g_RenderReady = false;

            ImGui_ImplWin32_NewFrame();
            ImGui_ImplCK2_NewFrame();
            ImGui::NewFrame();

            g_NewFrame = true;
        }
    }

    void ImGuiEndFrame() {
        if (g_NewFrame) {
            ImGui::EndFrame();
            g_NewFrame = false;
        }
    }

    void ImGuiRender() {
        if (g_NewFrame) {
            ImGui::Render();
            g_NewFrame = false;
            g_RenderReady = true;
        }
    }

    void ImGuiOnRender() {
        if (g_RenderReady) {
            ImGuiContextScope scope;
            ImGui_ImplCK2_RenderDrawData(ImGui::GetDrawData());
        }
    }
}

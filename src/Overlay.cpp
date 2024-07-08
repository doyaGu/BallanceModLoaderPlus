#include "Overlay.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <MinHook.h>

#include "imgui_internal.h"
#include "imgui_impl_ck2.h"
#define IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
#include "backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Overlay {
    typedef BOOL (WINAPI *LPFNPEEKMESSAGEA)(LPMSG, HWND, UINT, UINT, UINT);
    typedef BOOL (WINAPI *LPFNGETMESSAGEA)(LPMSG, HWND, UINT, UINT);

    LPFNPEEKMESSAGEA g_OrigPeekMessageA = nullptr;
    LPFNGETMESSAGEA g_OrigGetMessageA = nullptr;
    ImGuiContext *g_ImGuiContext = nullptr;
    bool g_ImGuiReady = false;
    bool g_RenderReady = false;

    LRESULT OnWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        ImGuiContextScope scope;

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

        return res;
    }

    extern "C" BOOL WINAPI HookPeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg) {
        if (!g_OrigPeekMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg))
            return FALSE;

        if (lpMsg->hwnd != nullptr && (wRemoveMsg & PM_REMOVE) != 0 && OnWndProc(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam)) {
            TranslateMessage(lpMsg);
            lpMsg->message = WM_NULL;
        }

        return TRUE;
    }

    extern "C" BOOL WINAPI HookGetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax) {
        if (!g_OrigGetMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax))
            return FALSE;

        if (lpMsg->hwnd != nullptr && OnWndProc(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam)) {
            TranslateMessage(lpMsg);
            lpMsg->message = WM_NULL;
        }
        return lpMsg->message != WM_QUIT;
    }

    bool ImGuiInstallWin32Hooks() {
        if (MH_CreateHookApi(L"user32", "PeekMessageA", (LPVOID) &HookPeekMessageA, (LPVOID *) &g_OrigPeekMessageA) != MH_OK ||
            MH_EnableHook((LPVOID) &PeekMessageA) != MH_OK) {
            return false;
        }
        if (MH_CreateHookApi(L"user32", "GetMessageA", (LPVOID) &HookGetMessageA, (LPVOID *) &g_OrigGetMessageA) != MH_OK ||
            MH_EnableHook((LPVOID) &GetMessageA) != MH_OK) {
            return false;
        }
        return true;
    }

    bool ImGuiUninstallWin32Hooks() {
        if (MH_DisableHook((LPVOID) &PeekMessageA) != MH_OK)
            return false;
        if (MH_DisableHook((LPVOID) &GetMessageA) != MH_OK)
            return false;
        return true;
    }

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

        if (!ImGui_ImplWin32_Init(context->GetMainWindow()))
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
        ImGuiContextScope scope;

        ImGui_ImplWin32_Shutdown();
    }

    void ImGuiShutdownRenderer(CKContext *context) {
        ImGuiContextScope scope;

        ImGui_ImplCK2_Shutdown();

        g_RenderReady = false;
        g_ImGuiReady = false;
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
            ImGuiContextScope scope;
            ImGui_ImplCK2_RenderDrawData(ImGui::GetDrawData());
        }
    }
}
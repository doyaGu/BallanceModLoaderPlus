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
    typedef BOOL (WINAPI *LPFNPEEKMESSAGE)(LPMSG, HWND, UINT, UINT, UINT);
    typedef BOOL (WINAPI *LPFNGETMESSAGE)(LPMSG, HWND, UINT, UINT);

    LPFNPEEKMESSAGE g_OrigPeekMessageA = nullptr;
    LPFNPEEKMESSAGE g_OrigPeekMessageW = nullptr;
    LPFNGETMESSAGE g_OrigGetMessageA = nullptr;
    LPFNGETMESSAGE g_OrigGetMessageW = nullptr;

    ImGuiContext *g_ImGuiContext = nullptr;
    bool g_ImGuiReady = false;
    bool g_RenderReady = false;
    bool g_NewFrame = false;

    static bool ShouldFeedImGui(UINT msg) {
        switch (msg) {
        // Mouse
        case WM_MOUSEMOVE:
        case WM_NCMOUSEMOVE:
        case WM_MOUSELEAVE:
        case WM_NCMOUSELEAVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        // Keyboard
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        case WM_CHAR:
        case WM_SYSCHAR:
        // IME
        case WM_IME_STARTCOMPOSITION:
        case WM_IME_COMPOSITION:
        case WM_IME_ENDCOMPOSITION:
        case WM_INPUTLANGCHANGE:
        // Focus
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
        // Cursor
        case WM_SETCURSOR:
        // Device
        case WM_DEVICECHANGE:
            return true;
        default:
            return false;
        }
    }

    LRESULT OnWndProcA(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        ImGuiContextScope scope;

        if (msg == WM_IME_COMPOSITION) {
            if (lParam & GCS_RESULTSTR) {
                HIMC hIMC = ImmGetContext(hWnd);
                LONG len = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, nullptr, 0) / (LONG) sizeof(WCHAR);
                auto *buf = new WCHAR[len + 1];
                ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, buf, len * sizeof(WCHAR));
                buf[len] = L'\0';
                ImmReleaseContext(hWnd, hIMC);
                for (int i = 0; i < len; ++i) {
                    ImGui::GetIO().AddInputCharacterUTF16(buf[i]);
                }
                delete[] buf;
            }
            return 1;
        }

        return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
    }

    LRESULT OnWndProcW(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        ImGuiContextScope scope;

        return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
    }

    extern "C" BOOL WINAPI HookPeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg) {
        if (!g_OrigPeekMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg))
            return FALSE;

        if (lpMsg->hwnd && (wRemoveMsg & PM_REMOVE) != 0 && ShouldFeedImGui(lpMsg->message)) {
            if (OnWndProcA(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam)) {
                lpMsg->message = WM_NULL;
            }
        }

        return TRUE;
    }

    extern "C" BOOL WINAPI HookPeekMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg) {
        if (!g_OrigPeekMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg))
            return FALSE;

        if (lpMsg->hwnd && (wRemoveMsg & PM_REMOVE) != 0 && ShouldFeedImGui(lpMsg->message)) {
            if (OnWndProcW(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam)) {
                lpMsg->message = WM_NULL;
            }
        }

        return TRUE;
    }

    extern "C" BOOL WINAPI HookGetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax) {
        BOOL ret = g_OrigGetMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
        if (ret == -1) return ret;
        if (ret == 0)  return 0;

        if (lpMsg->hwnd && ShouldFeedImGui(lpMsg->message)) {
            if (OnWndProcA(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam)) {
                lpMsg->message = WM_NULL;
            }
        }

        return 1;
    }

    extern "C" BOOL WINAPI HookGetMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax) {
        BOOL ret = g_OrigGetMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
        if (ret == -1) return ret;
        if (ret == 0)  return 0;

        if (lpMsg->hwnd && ShouldFeedImGui(lpMsg->message)) {
            if (OnWndProcW(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam)) {
                lpMsg->message = WM_NULL;
            }
        }

        return 1;
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
        if (MH_CreateHookApi(L"user32", "PeekMessageW", (LPVOID) &HookPeekMessageW, (LPVOID *) &g_OrigPeekMessageW) != MH_OK ||
            MH_EnableHook((LPVOID) &PeekMessageW) != MH_OK) {
            return false;
        }
        if (MH_CreateHookApi(L"user32", "GetMessageW", (LPVOID) &HookGetMessageW, (LPVOID *) &g_OrigGetMessageW) != MH_OK ||
            MH_EnableHook((LPVOID) &GetMessageW) != MH_OK) {
            return false;
        }
        return true;
    }

    bool ImGuiUninstallWin32Hooks() {
        if (MH_DisableHook((LPVOID) &PeekMessageA) != MH_OK)
            return false;
        if (MH_DisableHook((LPVOID) &GetMessageA) != MH_OK)
            return false;
        if (MH_DisableHook((LPVOID) &PeekMessageW) != MH_OK)
            return false;
        if (MH_DisableHook((LPVOID) &GetMessageW) != MH_OK)
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
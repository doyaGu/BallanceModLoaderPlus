#include "Win32Hooks.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <MinHook.h>

#include "imgui.h"
#define IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
#include "backends/imgui_impl_win32.h"

#include <atomic>
#include <string>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace bml_ui {

typedef BOOL (WINAPI *LPFNPEEKMESSAGE)(LPMSG, HWND, UINT, UINT, UINT);
typedef BOOL (WINAPI *LPFNGETMESSAGE)(LPMSG, HWND, UINT, UINT);

static LPFNPEEKMESSAGE g_OrigPeekMessageA = nullptr;
static LPFNPEEKMESSAGE g_OrigPeekMessageW = nullptr;
static LPFNGETMESSAGE g_OrigGetMessageA = nullptr;
static LPFNGETMESSAGE g_OrigGetMessageW = nullptr;

struct HookTarget {
    HWND hwnd = nullptr;
    ImGuiContext *ctx = nullptr;
};

static HookTarget g_Targets[2]{};
static std::atomic<int> g_ActiveIndex{0};

void SetHookTarget(HWND hwnd, ImGuiContext *ctx) {
    int next = 1 - g_ActiveIndex.load(std::memory_order_relaxed);
    g_Targets[next] = {hwnd, ctx};
    g_ActiveIndex.store(next, std::memory_order_release);
}

static LRESULT OnWndProcA(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    const auto &target = g_Targets[g_ActiveIndex.load(std::memory_order_acquire)];
    if (!target.ctx) return 0;
    ImGuiContext *prev = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(target.ctx);

    if (msg == WM_IME_COMPOSITION) {
        if (lParam & GCS_RESULTSTR) {
            HIMC hIMC = ImmGetContext(hWnd);
            LONG len = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, nullptr, 0) / (LONG) sizeof(WCHAR);

            // Use stack buffer for typical IME strings, heap fallback for long ones
            constexpr LONG kStackBufSize = 64;
            WCHAR stackBuf[kStackBufSize];
            WCHAR *buf = stackBuf;
            std::wstring heapBuf;
            if (len + 1 > kStackBufSize) {
                heapBuf.resize(static_cast<size_t>(len) + 1);
                buf = heapBuf.data();
            }

            ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, buf, len * sizeof(WCHAR));
            buf[len] = L'\0';
            ImmReleaseContext(hWnd, hIMC);
            for (LONG i = 0; i < len; ++i) {
                ImGui::GetIO().AddInputCharacterUTF16(buf[i]);
            }
        }
        ImGui::SetCurrentContext(prev);
        return 1;
    }

    LRESULT result = ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
    ImGui::SetCurrentContext(prev);
    return result;
}

static LRESULT OnWndProcW(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    const auto &target = g_Targets[g_ActiveIndex.load(std::memory_order_acquire)];
    if (!target.ctx) return 0;
    ImGuiContext *prev = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(target.ctx);
    LRESULT result = ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
    ImGui::SetCurrentContext(prev);
    return result;
}

extern "C" BOOL WINAPI HookPeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg) {
    if (!g_OrigPeekMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg))
        return FALSE;

    const auto &target = g_Targets[g_ActiveIndex.load(std::memory_order_acquire)];
    if (target.hwnd && lpMsg->hwnd == target.hwnd && (wRemoveMsg & PM_REMOVE) != 0) {
        if (OnWndProcA(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam)) {
            lpMsg->message = WM_NULL;
        }
    }

    return TRUE;
}

extern "C" BOOL WINAPI HookPeekMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg) {
    if (!g_OrigPeekMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg))
        return FALSE;

    const auto &target = g_Targets[g_ActiveIndex.load(std::memory_order_acquire)];
    if (target.hwnd && lpMsg->hwnd == target.hwnd && (wRemoveMsg & PM_REMOVE) != 0) {
        if (OnWndProcW(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam)) {
            lpMsg->message = WM_NULL;
        }
    }

    return TRUE;
}

extern "C" BOOL WINAPI HookGetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax) {
    if (!g_OrigGetMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax))
        return FALSE;

    const auto &target = g_Targets[g_ActiveIndex.load(std::memory_order_acquire)];
    if (target.hwnd && lpMsg->hwnd == target.hwnd && OnWndProcA(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam)) {
        lpMsg->message = WM_NULL;
    }

    return lpMsg->message != WM_QUIT;
}

extern "C" BOOL WINAPI HookGetMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax) {
    if (!g_OrigGetMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax))
        return FALSE;

    const auto &target = g_Targets[g_ActiveIndex.load(std::memory_order_acquire)];
    if (target.hwnd && lpMsg->hwnd == target.hwnd && OnWndProcW(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam)) {
        lpMsg->message = WM_NULL;
    }

    return lpMsg->message != WM_QUIT;
}

bool InstallWin32Hooks() {
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

bool UninstallWin32Hooks() {
    bool ok = true;
    ok &= (MH_DisableHook((LPVOID) &PeekMessageA) == MH_OK);
    ok &= (MH_DisableHook((LPVOID) &GetMessageA) == MH_OK);
    ok &= (MH_DisableHook((LPVOID) &PeekMessageW) == MH_OK);
    ok &= (MH_DisableHook((LPVOID) &GetMessageW) == MH_OK);
    return ok;
}

} // namespace bml_ui

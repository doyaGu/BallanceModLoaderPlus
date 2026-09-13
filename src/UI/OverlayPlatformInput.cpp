#include "UI/OverlayPlatformInput.h"

#include "UI/Ime/Runtime.h"
#include "UI/Overlay.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <CommCtrl.h>
#include <imm.h>

#include <algorithm>
#include <atomic>
#include <vector>

#define IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
#include "backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Overlay::PlatformInput {
    namespace {
        DWORD g_MessageThreadId = 0;
        std::atomic<HWND> g_MessageRoot{nullptr};
        HHOOK g_ImeProcessKeyHook = nullptr;
        std::vector<HWND> g_SubclassedWindows;
        constexpr UINT_PTR OverlayInputSubclassId = 0x424d4c49;

        LRESULT CALLBACK OverlayInputSubclass(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
                                               UINT_PTR subclassId, DWORD_PTR referenceData);

        LRESULT FeedImGui(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
            ImGuiContextScope scope;
            if (!scope.IsActive())
                return 0;
            return ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam);
        }

        bool IsOverlayMessageWindow(HWND window) {
            const HWND messageRoot = g_MessageRoot.load(std::memory_order_acquire);
            if (!messageRoot || !window)
                return false;

            HWND windowRoot = ::GetAncestor(window, GA_ROOT);
            if (!windowRoot)
                windowRoot = window;
            return windowRoot == messageRoot;
        }

        bool IsImeProcessKeyMessage(const MSG &message) {
            return message.message == WM_KEYDOWN &&
                   message.wParam == VK_PROCESSKEY &&
                   IsOverlayMessageWindow(message.hwnd);
        }

        void FeedImeProcessKey(const MSG &message, UINT key) {
            if (key != VK_TAB)
                return;

            if (Ime::Runtime::WantsCandidateNavigation()) {
                FeedImGui(message.hwnd, message.message, key, message.lParam);
            }
        }

        LRESULT CALLBACK ImeProcessKeyHook(int code, WPARAM removeMode, LPARAM messageAddress) {
            auto *message = reinterpret_cast<MSG *>(messageAddress);
            const bool processKey = code == HC_ACTION &&
                                    removeMode == PM_REMOVE && message &&
                                    IsImeProcessKeyMessage(*message);
            const UINT key = processKey
                ? ::ImmGetVirtualKey(message->hwnd)
                : VK_PROCESSKEY;
            const LRESULT next = ::CallNextHookEx(g_ImeProcessKeyHook, code, removeMode, messageAddress);
            if (processKey)
                FeedImeProcessKey(*message, key);
            return next;
        }

        bool InstallOverlayInputSubclass(HWND window) {
            if (!IsOverlayMessageWindow(window))
                return false;
            if (::GetWindowThreadProcessId(window, nullptr) != g_MessageThreadId) {
                return true;
            }
            if (std::find(g_SubclassedWindows.begin(),
                          g_SubclassedWindows.end(), window) !=
                g_SubclassedWindows.end()) {
                return true;
            }
            if (!::SetWindowSubclass(window, &OverlayInputSubclass, OverlayInputSubclassId, 0)) {
                return false;
            }
            g_SubclassedWindows.push_back(window);
            return true;
        }

        BOOL CALLBACK InstallChildOverlayInputSubclass(HWND window, LPARAM successAddress) {
            auto *success = reinterpret_cast<bool *>(successAddress);
            if (!InstallOverlayInputSubclass(window))
                *success = false;
            return TRUE;
        }

        bool InstallOverlayInputSubclasses(HWND root) {
            bool success = InstallOverlayInputSubclass(root);
            ::EnumChildWindows(root, &InstallChildOverlayInputSubclass, reinterpret_cast<LPARAM>(&success));
            return success;
        }

        bool RemoveOverlayInputSubclasses() {
            bool success = true;
            for (auto it = g_SubclassedWindows.begin();
                 it != g_SubclassedWindows.end();) {
                const HWND window = *it;
                if (!::IsWindow(window) || ::RemoveWindowSubclass(window, &OverlayInputSubclass,
                                                                 OverlayInputSubclassId)) {
                    it = g_SubclassedWindows.erase(it);
                } else {
                    success = false;
                    ++it;
                }
            }
            return success;
        }

        LRESULT CALLBACK OverlayInputSubclass(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
                                               UINT_PTR, DWORD_PTR) {
            if (message == WM_NCDESTROY) {
                const auto position = std::find(g_SubclassedWindows.begin(), g_SubclassedWindows.end(), window);
                if (position != g_SubclassedWindows.end())
                    g_SubclassedWindows.erase(position);
                ::RemoveWindowSubclass(window, &OverlayInputSubclass, OverlayInputSubclassId);
            }

            // Detach clears the root before removing subclasses. If Windows
            // refuses a removal, the remaining callback must stay dormant and
            // never reach either IME state or a shut-down ImGui backend.
            if (!IsOverlayMessageWindow(window))
                return ::DefSubclassProc(window, message, wParam, lParam);

            if (message == WM_PARENTNOTIFY && LOWORD(wParam) == WM_CREATE)
                InstallOverlayInputSubclass(reinterpret_cast<HWND>(lParam));

            const Ime::NativePresentation::MessageDisposition disposition =
                Ime::Runtime::HandleNativeMessage(window, message, wParam, lParam);
            const LRESULT backendResult = FeedImGui(window, message, wParam, lParam);

            if (disposition.replaceLParam)
                return ::DefSubclassProc(window, message, wParam, static_cast<LPARAM>(disposition.lParam));
            if (disposition.suppress || backendResult != 0)
                return 0;
            return ::DefSubclassProc(window, message, wParam, lParam);
        }
    }

    bool Attach(void *window) {
        HWND nativeWindow = static_cast<HWND>(window);
        if (!nativeWindow || !::IsWindow(nativeWindow))
            return false;

        DWORD processId = 0;
        const DWORD threadId = ::GetWindowThreadProcessId(nativeWindow, &processId);
        if (!threadId || processId != ::GetCurrentProcessId() ||
            threadId != ::GetCurrentThreadId()) {
            return false;
        }

        HWND messageRoot = ::GetAncestor(nativeWindow, GA_ROOT);
        if (!messageRoot)
            messageRoot = nativeWindow;

        if (g_MessageThreadId != 0 || g_ImeProcessKeyHook ||
            !g_SubclassedWindows.empty()) {
            const bool rootSubclassed = std::find(
                g_SubclassedWindows.begin(), g_SubclassedWindows.end(),
                messageRoot) != g_SubclassedWindows.end();
            if (g_MessageThreadId == threadId && g_ImeProcessKeyHook && rootSubclassed &&
                g_MessageRoot.load(std::memory_order_acquire) == messageRoot) {
                return true;
            }
            if (!Detach())
                return false;
        }

        g_MessageRoot.store(messageRoot, std::memory_order_release);
        g_MessageThreadId = threadId;
        Ime::Runtime::Attach(messageRoot);
        g_ImeProcessKeyHook = ::SetWindowsHookExW(WH_GETMESSAGE, &ImeProcessKeyHook, nullptr, threadId);
        if (g_ImeProcessKeyHook && InstallOverlayInputSubclasses(messageRoot)) {
            return true;
        }

        Ime::Runtime::Detach();
        g_MessageRoot.store(nullptr, std::memory_order_release);
        RemoveOverlayInputSubclasses();
        if (g_ImeProcessKeyHook &&
            ::UnhookWindowsHookEx(g_ImeProcessKeyHook)) {
            g_ImeProcessKeyHook = nullptr;
        }
        if (g_SubclassedWindows.empty())
            g_MessageThreadId = 0;
        return false;
    }

    bool Detach() {
        if (g_MessageThreadId != 0 &&
            g_MessageThreadId != ::GetCurrentThreadId()) {
            return false;
        }

        Ime::Runtime::Detach();
        // Disable new routing first. A subclass that cannot be removed is then
        // dormant and cannot reach the ImGui backend during its shutdown.
        g_MessageRoot.store(nullptr, std::memory_order_release);
        const bool subclassesRemoved = RemoveOverlayInputSubclasses();
        const bool hookRemoved = !g_ImeProcessKeyHook ||
            ::UnhookWindowsHookEx(g_ImeProcessKeyHook) != FALSE;
        if (hookRemoved)
            g_ImeProcessKeyHook = nullptr;
        if (!subclassesRemoved || !hookRemoved)
            return false;

        g_MessageThreadId = 0;
        return true;
    }
}

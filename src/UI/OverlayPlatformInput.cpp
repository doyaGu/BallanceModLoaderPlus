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
        class MessageRoute final {
        public:
            void Enable(HWND rootWindow, HWND backendWindow) noexcept {
                m_BackendWindow.store(backendWindow, std::memory_order_release);
                m_RootWindow.store(rootWindow, std::memory_order_release);
            }

            void Disable() noexcept {
                m_RootWindow.store(nullptr, std::memory_order_release);
                m_BackendWindow.store(nullptr, std::memory_order_release);
            }

            HWND RootWindow() const noexcept {
                return m_RootWindow.load(std::memory_order_acquire);
            }

            HWND BackendWindow() const noexcept {
                return m_BackendWindow.load(std::memory_order_acquire);
            }

            bool Contains(HWND window) const noexcept {
                const HWND rootWindow = RootWindow();
                if (!rootWindow || !window)
                    return false;

                HWND windowRoot = ::GetAncestor(window, GA_ROOT);
                if (!windowRoot)
                    windowRoot = window;
                return windowRoot == rootWindow;
            }

            bool DeliversTo(HWND window) const noexcept {
                return RootWindow() && window == BackendWindow();
            }

        private:
            std::atomic<HWND> m_RootWindow{nullptr};
            std::atomic<HWND> m_BackendWindow{nullptr};
        };

        DWORD g_MessageThreadId = 0;
        MessageRoute g_MessageRoute;
        HHOOK g_ImeProcessKeyHook = nullptr;
        UINT g_CandidateMoveMessage = 0;
        std::vector<HWND> g_SubclassedWindows;
        constexpr UINT_PTR OverlayInputSubclassId = 0x424d4c49;
        constexpr wchar_t CandidateMoveMessageName[] = L"BMLPlus.Ime.CandidateMove";

        LRESULT CALLBACK OverlayInputSubclass(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
                                               UINT_PTR subclassId, DWORD_PTR referenceData);

        LRESULT FeedImGui(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
            ImGuiContextScope scope;
            if (!scope.IsActive())
                return 0;
            return ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam);
        }

        bool IsOverlayMessageWindow(HWND window) {
            return g_MessageRoute.Contains(window);
        }

        bool IsImeProcessKeyMessage(const MSG &message) {
            return message.message == WM_KEYDOWN &&
                   message.wParam == VK_PROCESSKEY &&
                   IsOverlayMessageWindow(message.hwnd);
        }

        void RouteImeProcessKey(UINT key, bool reverse) {
            if (key != VK_TAB || !Ime::Runtime::WantsCandidateNavigation())
                return;

            const Ime::CandidateDirection direction = reverse
                ? Ime::CandidateDirection::Previous
                : Ime::CandidateDirection::Next;
            const HWND backendWindow = g_MessageRoute.BackendWindow();
            if (backendWindow && g_CandidateMoveMessage != 0)
                ::PostMessageW(backendWindow, g_CandidateMoveMessage, static_cast<WPARAM>(direction), 0);
        }

        LRESULT CALLBACK ImeProcessKeyHook(int code, WPARAM removeMode, LPARAM messageAddress) {
            auto *message = reinterpret_cast<MSG *>(messageAddress);
            const bool processKey = code == HC_ACTION &&
                                    removeMode == PM_REMOVE && message &&
                                    IsImeProcessKeyMessage(*message);
            const UINT key = processKey
                ? ::ImmGetVirtualKey(message->hwnd)
                : VK_PROCESSKEY;
            const bool reverse = processKey && (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
            const LRESULT next = ::CallNextHookEx(g_ImeProcessKeyHook, code, removeMode, messageAddress);
            if (processKey)
                RouteImeProcessKey(key, reverse);
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

            if (g_CandidateMoveMessage != 0 && message == g_CandidateMoveMessage) {
                if (wParam == static_cast<WPARAM>(Ime::CandidateDirection::Previous)) {
                    Ime::Runtime::MoveCandidate(Ime::CandidateDirection::Previous);
                } else if (wParam == static_cast<WPARAM>(Ime::CandidateDirection::Next)) {
                    Ime::Runtime::MoveCandidate(Ime::CandidateDirection::Next);
                }
                return 0;
            }

            const Ime::NativePresentation::MessageDisposition disposition =
                Ime::Runtime::HandleNativeMessage(window, message, wParam, lParam);
            const LRESULT backendResult = g_MessageRoute.DeliversTo(window)
                ? FeedImGui(window, message, wParam, lParam)
                : 0;

            if (disposition.replaceLParam)
                return ::DefSubclassProc(window, message, wParam, static_cast<LPARAM>(disposition.lParam));
            if (disposition.suppress || backendResult != 0)
                return 0;
            return ::DefSubclassProc(window, message, wParam, lParam);
        }
    }

    bool Attach(void *window) {
        const HWND backendWindow = static_cast<HWND>(window);
        if (!backendWindow || !::IsWindow(backendWindow))
            return false;

        const UINT candidateMoveMessage = ::RegisterWindowMessageW(CandidateMoveMessageName);
        if (candidateMoveMessage == 0)
            return false;

        DWORD processId = 0;
        const DWORD threadId = ::GetWindowThreadProcessId(backendWindow, &processId);
        if (!threadId || processId != ::GetCurrentProcessId() ||
            threadId != ::GetCurrentThreadId()) {
            return false;
        }

        HWND rootWindow = ::GetAncestor(backendWindow, GA_ROOT);
        if (!rootWindow)
            rootWindow = backendWindow;

        if (g_MessageThreadId != 0 || g_ImeProcessKeyHook ||
            !g_SubclassedWindows.empty()) {
            const bool rootSubclassed = std::find(
                g_SubclassedWindows.begin(), g_SubclassedWindows.end(),
                rootWindow) != g_SubclassedWindows.end();
            const bool backendSubclassed = backendWindow == rootWindow || std::find(
                g_SubclassedWindows.begin(), g_SubclassedWindows.end(),
                backendWindow) != g_SubclassedWindows.end();
            if (g_MessageThreadId == threadId && g_ImeProcessKeyHook &&
                rootSubclassed && backendSubclassed &&
                g_MessageRoute.RootWindow() == rootWindow &&
                g_MessageRoute.BackendWindow() == backendWindow) {
                return true;
            }
            if (!Detach())
                return false;
        }

        g_MessageThreadId = threadId;
        g_CandidateMoveMessage = candidateMoveMessage;
        Ime::Runtime::Attach(rootWindow, backendWindow);
        g_MessageRoute.Enable(rootWindow, backendWindow);
        g_ImeProcessKeyHook = ::SetWindowsHookExW(WH_GETMESSAGE, &ImeProcessKeyHook, nullptr, threadId);
        if (g_ImeProcessKeyHook && InstallOverlayInputSubclasses(rootWindow)) {
            return true;
        }

        g_MessageRoute.Disable();
        Ime::Runtime::Detach();
        RemoveOverlayInputSubclasses();
        if (g_ImeProcessKeyHook &&
            ::UnhookWindowsHookEx(g_ImeProcessKeyHook)) {
            g_ImeProcessKeyHook = nullptr;
        }
        if (!g_ImeProcessKeyHook && g_SubclassedWindows.empty())
            g_MessageThreadId = 0;
        if (g_MessageThreadId == 0)
            g_CandidateMoveMessage = 0;
        return false;
    }

    bool Detach() {
        if (g_MessageThreadId != 0 &&
            g_MessageThreadId != ::GetCurrentThreadId()) {
            return false;
        }

        // Disable new routing first. A subclass that cannot be removed is then
        // dormant and cannot reach the ImGui backend during its shutdown.
        g_MessageRoute.Disable();
        Ime::Runtime::Detach();
        const bool subclassesRemoved = RemoveOverlayInputSubclasses();
        const bool hookRemoved = !g_ImeProcessKeyHook ||
            ::UnhookWindowsHookEx(g_ImeProcessKeyHook) != FALSE;
        if (hookRemoved)
            g_ImeProcessKeyHook = nullptr;
        if (!subclassesRemoved || !hookRemoved)
            return false;

        g_MessageThreadId = 0;
        g_CandidateMoveMessage = 0;
        return true;
    }
}

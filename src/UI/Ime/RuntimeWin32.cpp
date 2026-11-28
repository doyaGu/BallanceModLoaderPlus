#include "UI/Ime/Runtime.h"
#include "UI/Ime/NativePresentation.h"
#include "UI/Ime/PresentationOwnership.h"
#include "UI/Ime/Tsf.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <imm.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace Overlay::Ime {
    namespace {
        struct PresentationSnapshotCache {
            Snapshot imm;
            Snapshot combined;
            std::uint64_t stateRevision = ~std::uint64_t{0};
            std::uint64_t stateCandidateRevision = ~std::uint64_t{0};
            std::uint64_t tsfRevision = ~std::uint64_t{0};
            std::uint64_t revision = 0;
            std::uint64_t candidateRevision = 0;
        };

        class PresentationTarget final {
        public:
            void Attach(HWND window) noexcept {
                m_Window.store(window, std::memory_order_release);
                m_Ownership.Attach(window && ::GetFocus() == window);
            }

            void Detach() noexcept {
                m_Window.store(nullptr, std::memory_order_release);
                m_Ownership.Detach();
            }

            HWND Window() const noexcept {
                return m_Window.load(std::memory_order_acquire);
            }

            bool HasFocus(HWND messageWindow = nullptr) const noexcept {
                const HWND window = Window();
                return window && (!messageWindow || messageWindow == window) &&
                       m_Ownership.IsFocused();
            }

            bool OwnsPresentation(HWND messageWindow = nullptr) const noexcept {
                const HWND window = Window();
                return window && (!messageWindow || messageWindow == window) &&
                       m_Ownership.IsVisible();
            }

            bool ObserveFocus(HWND messageWindow, std::uint32_t message,
                              std::uintptr_t wParam) noexcept {
                const HWND window = Window();
                if (message == WM_SETFOCUS ||
                    (message == WM_IME_SETCONTEXT && wParam != 0)) {
                    const bool focused = messageWindow == window;
                    return m_Ownership.SetFocused(focused);
                } else if (messageWindow == window &&
                           (message == WM_KILLFOCUS || message == WM_NCDESTROY ||
                            (message == WM_IME_SETCONTEXT && wParam == 0))) {
                    return m_Ownership.SetFocused(false);
                }
                return false;
            }

            bool ExchangeVisible(bool visible) noexcept {
                return m_Ownership.ExchangeVisible(visible);
            }

            bool IsVisible() const noexcept {
                return OwnsPresentation();
            }

        private:
            std::atomic<HWND> m_Window{nullptr};
            PresentationOwnership m_Ownership;
        };

        std::mutex g_StateMutex;
        State g_State;
        HWND g_RootWindow = nullptr;
        PresentationTarget g_PresentationTarget;
        PresentationSnapshotCache g_PresentationCache;

        class ImmContext final {
        public:
            ImmContext() = default;
            ImmContext(HWND window, HIMC context)
                : m_Window(window), m_Context(context) {}
            ImmContext(const ImmContext &) = delete;
            ImmContext &operator=(const ImmContext &) = delete;
            ImmContext(ImmContext &&other) noexcept
                : m_Window(std::exchange(other.m_Window, nullptr)),
                  m_Context(std::exchange(other.m_Context, nullptr)) {}
            ImmContext &operator=(ImmContext &&other) noexcept {
                if (this != &other) {
                    Release();
                    m_Window = std::exchange(other.m_Window, nullptr);
                    m_Context = std::exchange(other.m_Context, nullptr);
                }
                return *this;
            }
            ~ImmContext() { Release(); }

            explicit operator bool() const noexcept {
                return m_Context != nullptr;
            }
            HIMC Get() const noexcept { return m_Context; }

        private:
            void Release() noexcept {
                if (m_Window && m_Context)
                    ::ImmReleaseContext(m_Window, m_Context);
                m_Window = nullptr;
                m_Context = nullptr;
            }

            HWND m_Window = nullptr;
            HIMC m_Context = nullptr;
        };

        HWND NormalizeRoot(HWND window) {
            if (!window)
                return nullptr;
            if (HWND root = ::GetAncestor(window, GA_ROOT))
                return root;
            return window;
        }

        bool BelongsToRoot(HWND window, HWND root) {
            return window && root && ::IsWindow(window) &&
                   NormalizeRoot(window) == root;
        }

        HWND CopyRootWindow() {
            std::lock_guard lock(g_StateMutex);
            return g_RootWindow;
        }

        bool IsCompatibilityNavigationLanguage(HKL layout) noexcept {
            const LANGID language = LOWORD(reinterpret_cast<ULONG_PTR>(layout));
            switch (PRIMARYLANGID(language)) {
            case LANG_CHINESE:
            case LANG_JAPANESE:
            case LANG_KOREAN:
                return true;
            default:
                return false;
            }
        }

        bool HasConflictingNavigationModifier() noexcept {
            return (::GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 ||
                   (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0 ||
                   (::GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 ||
                   (::GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;
        }

        bool CanUseCompatibilityNavigation(HWND root, HWND owner) {
            const HWND foreground = ::GetForegroundWindow();
            if (!root || NormalizeRoot(foreground) != root)
                return false;

            const HWND focus = ::GetFocus();
            if (!owner || focus != owner || !BelongsToRoot(owner, root) ||
                HasConflictingNavigationModifier())
                return false;

            const DWORD focusThread = ::GetWindowThreadProcessId(focus, nullptr);
            return focusThread == ::GetCurrentThreadId() &&
                   IsCompatibilityNavigationLanguage(::GetKeyboardLayout(focusThread));
        }

        bool SendCompatibilityNavigation(CandidateDirection direction) {
            const HWND root = CopyRootWindow();
            const HWND owner = g_PresentationTarget.Window();
            if (!CanUseCompatibilityNavigation(root, owner))
                return false;

            // IMM compatibility IMEs can expose a readable candidate list while
            // rejecting NI_SELECTCANDIDATESTR. Re-enter their normal keyboard
            // path with the CJK candidate navigation keys instead. Keep this
            // fallback local to the focused Player thread: SendInput is not a
            // vendor-neutral candidate control interface.
            const WORD key = direction == CandidateDirection::Previous ? VK_UP : VK_DOWN;
            INPUT inputs[2]{};
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = key;
            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wVk = key;
            inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

            const UINT inserted = ::SendInput(2, inputs, sizeof(INPUT));
            if (inserted == 1)
                return ::SendInput(1, &inputs[1], sizeof(INPUT)) == 1;
            return inserted == 2;
        }

        struct ContextWindowOrder {
            std::array<HWND, 3> windows{};
            std::size_t count = 0;
        };

        ContextWindowOrder BuildContextWindowOrder(HWND messageWindow, HWND focusWindow, HWND root) {
            const std::array<HWND, 3> requested{
                messageWindow,
                focusWindow,
                root,
            };

            ContextWindowOrder order;
            for (const HWND candidate : requested) {
                if (!BelongsToRoot(candidate, root))
                    continue;

                bool duplicate = false;
                for (std::size_t index = 0; index < order.count; ++index)
                    duplicate |= order.windows[index] == candidate;
                if (!duplicate)
                    order.windows[order.count++] = candidate;
            }
            return order;
        }

        ImmContext AcquireImmContext(HWND messageWindow) {
            const HWND root = CopyRootWindow();
            if (!root)
                return {};

            const ContextWindowOrder candidates = BuildContextWindowOrder(messageWindow, ::GetFocus(), root);
            for (std::size_t index = 0; index < candidates.count; ++index) {
                const HWND candidate = candidates.windows[index];
                if (HIMC context = ::ImmGetContext(candidate))
                    return ImmContext(candidate, context);
            }
            return {};
        }

        bool ReadCompositionBytes(HIMC context, DWORD index, std::vector<std::byte> &output) {
            const LONG required = ::ImmGetCompositionStringW(context, index, nullptr, 0);
            if (required < 0 || static_cast<std::size_t>(required) > MaxImmPayloadBytes)
                return false;

            std::vector<std::byte> bytes(static_cast<std::size_t>(required));
            if (required != 0) {
                const LONG copied = ::ImmGetCompositionStringW(context, index, bytes.data(),
                                                               static_cast<DWORD>(bytes.size()));
                if (copied < 0 || copied != required)
                    return false;
            }
            output = std::move(bytes);
            return true;
        }

        std::vector<TextRange> BuildTargetRanges(const std::vector<std::byte> &attributes) {
            std::vector<TextRange> ranges;
            for (std::size_t begin = 0; begin < attributes.size();) {
                const auto attribute = std::to_integer<std::uint8_t>(attributes[begin]);
                const bool target = attribute == ATTR_TARGET_CONVERTED ||
                                    attribute == ATTR_TARGET_NOTCONVERTED;
                std::size_t end = begin + 1;
                while (end < attributes.size()) {
                    const auto next = std::to_integer<std::uint8_t>(attributes[end]);
                    if ((next == ATTR_TARGET_CONVERTED ||
                         next == ATTR_TARGET_NOTCONVERTED) != target) {
                        break;
                    }
                    ++end;
                }
                if (target) {
                    ranges.push_back({
                        static_cast<std::uint32_t>(begin),
                        static_cast<std::uint32_t>(end),
                    });
                }
                begin = end;
            }
            return ranges;
        }

        Snapshot CopyImmSnapshot() {
            Snapshot snapshot;
            std::lock_guard lock(g_StateMutex);
            snapshot = g_State.GetSnapshot();
            return snapshot;
        }

        const PresentationSnapshotCache &RefreshPresentationSnapshot() {
            bool changed = false;
            bool candidatesChanged = false;
            {
                std::lock_guard lock(g_StateMutex);
                const std::uint64_t revision = g_State.GetRevision();
                if (revision != g_PresentationCache.stateRevision) {
                    g_PresentationCache.imm = g_State.GetSnapshot();
                    g_PresentationCache.stateRevision = revision;
                    changed = true;
                }
                const std::uint64_t candidateRevision = g_State.GetCandidateRevision();
                if (candidateRevision != g_PresentationCache.stateCandidateRevision) {
                    g_PresentationCache.stateCandidateRevision = candidateRevision;
                    candidatesChanged = true;
                }
            }

            const std::uint64_t tsfRevision = Tsf::Revision();
            if (tsfRevision != g_PresentationCache.tsfRevision) {
                changed = true;
                candidatesChanged = true;
            }

            if (changed) {
                g_PresentationCache.combined = g_PresentationCache.imm;
                const std::uint64_t appliedTsfRevision = Tsf::ApplyCandidates(g_PresentationCache.combined);
                candidatesChanged |= appliedTsfRevision != g_PresentationCache.tsfRevision;
                g_PresentationCache.tsfRevision = appliedTsfRevision;
                ++g_PresentationCache.revision;
                if (candidatesChanged)
                    ++g_PresentationCache.candidateRevision;
            }
            return g_PresentationCache;
        }

        bool StateIsActive() {
            std::lock_guard lock(g_StateMutex);
            return g_State.GetSnapshot().IsActive();
        }

        bool StateHasCandidates() {
            std::lock_guard lock(g_StateMutex);
            return g_State.GetSnapshot().HasCandidates();
        }

        void ResetState() {
            {
                std::lock_guard lock(g_StateMutex);
                g_State.Reset();
            }
            Tsf::ClearCandidates();
        }

        void ObserveComposition(HWND window, LPARAM flags) {
            if (flags == 0) {
                ResetState();
                return;
            }

            CompositionUpdate update;
            update.hasResult = (flags & GCS_RESULTSTR) != 0;

            ImmContext context = AcquireImmContext(window);
            if (context) {
                std::vector<std::byte> bytes;
                if ((flags & (GCS_COMPSTR | CS_INSERTCHAR)) != 0 &&
                    ReadCompositionBytes(context.Get(), GCS_COMPSTR, bytes) &&
                    ParseUtf16(bytes, update.text)) {
                    update.hasText = true;
                }

                const Snapshot previous = CopyImmSnapshot();
                const std::size_t compositionLength = update.hasText ? update.text.size() : previous.composition.size();

                if ((flags & GCS_COMPATTR) != 0 &&
                    ReadCompositionBytes(context.Get(), GCS_COMPATTR, bytes) &&
                    bytes.size() == compositionLength) {
                    update.targetRanges = BuildTargetRanges(bytes);
                    update.hasTargetRanges = true;
                }

                if ((flags & GCS_COMPCLAUSE) != 0 &&
                    ReadCompositionBytes(context.Get(), GCS_COMPCLAUSE, bytes) &&
                    ParseClauses(bytes, compositionLength, update.clauseBoundaries)) {
                    update.hasClauseBoundaries = true;
                }

                if ((flags & (GCS_CURSORPOS | CS_INSERTCHAR)) != 0) {
                    const LONG cursor = ::ImmGetCompositionStringW(
                        context.Get(), GCS_CURSORPOS, nullptr, 0);
                    if (cursor >= 0) {
                        update.hasCursor = true;
                        update.cursor = static_cast<std::uint32_t>(cursor);
                    }
                }
            }

            if (update.hasResult || update.hasText ||
                update.hasTargetRanges || update.hasClauseBoundaries ||
                update.hasCursor) {
                if (update.hasResult)
                    Tsf::ClearCandidates();
                std::lock_guard lock(g_StateMutex);
                g_State.ApplyComposition(std::move(update));
            }
        }

        void RefreshCandidateLists(HWND window, std::uint32_t mask) {
            if (mask == 0)
                mask = 1;

            ImmContext context = AcquireImmContext(window);
            if (!context) {
                std::lock_guard lock(g_StateMutex);
                g_State.CloseCandidateLists(mask);
                return;
            }

            struct CandidateUpdate {
                std::size_t index;
                std::optional<CandidateListSnapshot> list;
            };
            std::vector<CandidateUpdate> updates;
            for (std::size_t index = 0;
                 index < MaxCandidateLists; ++index) {
                if ((mask & (std::uint32_t{1} << index)) == 0)
                    continue;

                CandidateUpdate update{index, std::nullopt};
                const DWORD required = ::ImmGetCandidateListW(
                    context.Get(), static_cast<DWORD>(index), nullptr, 0);
                if (required != 0 &&
                    required <= MaxImmPayloadBytes) {
                    std::vector<std::byte> bytes(required);
                    const DWORD copied = ::ImmGetCandidateListW(
                        context.Get(), static_cast<DWORD>(index),
                        reinterpret_cast<CANDIDATELIST *>(bytes.data()),
                        required);
                    if (copied != 0 && copied <= bytes.size()) {
                        bytes.resize(copied);
                        CandidateListSnapshot parsed;
                        if (ParseCandidateList(bytes, parsed))
                            update.list = std::move(parsed);
                    }
                }
                updates.push_back(std::move(update));
            }

            std::lock_guard lock(g_StateMutex);
            for (auto &update : updates) {
                if (update.list) {
                    g_State.SetCandidateList(update.index,
                                             std::move(*update.list));
                } else {
                    g_State.CloseCandidateLists(
                        std::uint32_t{1} << update.index);
                }
            }
        }

        bool NavigateCandidate(HWND window, const Snapshot &snapshot,
                               CandidateDirection direction) {
            if (Tsf::MoveSelection(direction))
                return true;

            const std::optional<CandidateSelectionRequest> request =
                PlanCandidateSelection(snapshot, direction);
            if (request) {
                bool accepted = false;
                {
                    ImmContext context = AcquireImmContext(window);
                    if (context) {
                        accepted = ::ImmNotifyIME(context.Get(), NI_SELECTCANDIDATESTR,
                                                  static_cast<DWORD>(request->listIndex),
                                                  request->itemIndex) != FALSE;
                    }
                }
                if (accepted) {
                    RefreshCandidateLists(window, std::uint32_t{1} << request->listIndex);
                    return true;
                }
            }

            return SendCompatibilityNavigation(direction);
        }

    }
}

namespace Overlay::Ime::Runtime {
    void Attach(void *rootWindow, void *presentationWindow) {
        const HWND root = NormalizeRoot(static_cast<HWND>(rootWindow));
        HWND owner = static_cast<HWND>(presentationWindow);
        if (!BelongsToRoot(owner, root))
            owner = nullptr;
        {
            std::lock_guard lock(g_StateMutex);
            g_RootWindow = root;
            g_State.Reset();
        }
        g_PresentationTarget.Attach(owner);
        Tsf::Attach();
        Tsf::SetPresentationOwned(false);
    }

    void Detach() {
        g_PresentationTarget.Detach();
        Tsf::SetPresentationOwned(false);
        Tsf::Detach();
        std::lock_guard lock(g_StateMutex);
        g_State.Reset();
        g_RootWindow = nullptr;
    }

    NativePresentation::MessageDisposition HandleNativeMessage(void *messageWindow, std::uint32_t message,
                                                               std::uintptr_t wParam, std::intptr_t lParam) {
        const HWND window = static_cast<HWND>(messageWindow);
        const HWND root = CopyRootWindow();
        const HWND owner = g_PresentationTarget.Window();
        if (!BelongsToRoot(window, root))
            return {};

        const bool focusChanged = g_PresentationTarget.ObserveFocus(window, message, wParam);
        const bool presentationOwned = g_PresentationTarget.IsVisible();
        const bool ownsPresentation = presentationOwned && window == owner;
        if (focusChanged) {
            Tsf::SetPresentationOwned(presentationOwned);
            if (!g_PresentationTarget.HasFocus())
                ResetState();
        }
        switch (message) {
        case WM_IME_STARTCOMPOSITION: {
            if (!ownsPresentation) {
                ResetState();
                break;
            }
            Tsf::ClearCandidates();
            std::lock_guard lock(g_StateMutex);
            g_State.StartComposition();
            break;
        }
        case WM_IME_COMPOSITION:
            if (ownsPresentation)
                ObserveComposition(window, static_cast<LPARAM>(lParam));
            break;
        case WM_IME_ENDCOMPOSITION:
        case WM_INPUTLANGCHANGE:
        case WM_KILLFOCUS:
            ResetState();
            break;
        case WM_NCDESTROY:
            if (window == root || window == owner)
                ResetState();
            break;
        case WM_IME_SETCONTEXT:
            if (!wParam)
                ResetState();
            break;
        case WM_IME_NOTIFY:
            if (!ownsPresentation)
                break;
            if (wParam == IMN_OPENCANDIDATE ||
                wParam == IMN_CHANGECANDIDATE) {
                RefreshCandidateLists(window, static_cast<std::uint32_t>(lParam));
            } else if (wParam == IMN_CLOSECANDIDATE) {
                std::lock_guard lock(g_StateMutex);
                g_State.CloseCandidateLists(static_cast<std::uint32_t>(lParam));
            } else if (wParam == IMN_SETOPENSTATUS) {
                ImmContext context = AcquireImmContext(window);
                if (!context || !::ImmGetOpenStatus(context.Get()))
                    ResetState();
            }
            break;
        default:
            break;
        }

        return NativePresentation::Decide(message, wParam, lParam, ownsPresentation);
    }

    bool WantsCandidateNavigation() {
        if (!g_PresentationTarget.IsVisible())
            return false;
        return StateHasCandidates() || Tsf::HasCandidates();
    }

    bool PreparePresentationFrame(bool visible, PresentationFrame &frame) {
        const bool presentationVisible = visible && g_PresentationTarget.HasFocus();
        const bool wasVisible = g_PresentationTarget.ExchangeVisible(presentationVisible);
        const bool isVisible = g_PresentationTarget.IsVisible();
        if (wasVisible != isVisible)
            Tsf::SetPresentationOwned(isVisible);
        if (!isVisible) {
            if (wasVisible)
                ResetState();
            return false;
        }

        const PresentationSnapshotCache &cache = RefreshPresentationSnapshot();
        if (frame.revision != cache.revision) {
            frame.snapshot = cache.combined;
            frame.revision = cache.revision;
            frame.candidateRevision = cache.candidateRevision;
        }
        return frame.snapshot.HasContent();
    }

    bool IsPresentationActive() {
        if (!g_PresentationTarget.IsVisible())
            return false;
        return StateIsActive() || Tsf::HasCandidates();
    }

    Diagnostics GetDiagnostics() {
        Diagnostics diagnostics;
        const HWND root = CopyRootWindow();
        const HWND presentation = g_PresentationTarget.Window();
        const HWND focus = ::GetFocus();
        const Snapshot snapshot = CopyImmSnapshot();

        diagnostics.RootWindow = reinterpret_cast<std::uintptr_t>(root);
        diagnostics.PresentationWindow = reinterpret_cast<std::uintptr_t>(presentation);
        diagnostics.FocusWindow = reinterpret_cast<std::uintptr_t>(focus);
        diagnostics.PresentationVisible = g_PresentationTarget.IsVisible();
        diagnostics.CompositionActive = snapshot.composing;
        diagnostics.HasCandidates = snapshot.HasCandidates();
        diagnostics.ImmContextAvailable = static_cast<bool>(AcquireImmContext(presentation));
        diagnostics.TsfAttached = Tsf::IsAttached();
        diagnostics.TsfCandidates = Tsf::HasCandidates();
        return diagnostics;
    }

    bool MoveCandidate(CandidateDirection direction) {
        if (!g_PresentationTarget.IsVisible())
            return false;
        const PresentationSnapshotCache &cache = RefreshPresentationSnapshot();
        return cache.combined.HasCandidates() && NavigateCandidate(nullptr, cache.combined, direction);
    }
}

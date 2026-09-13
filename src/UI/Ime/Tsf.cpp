#include "UI/Ime/Tsf.h"

#include "UI/Ime/State.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <msctf.h>
#include <oleauto.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <new>
#include <optional>
#include <cstdint>
#include <utility>
#include <vector>

namespace Overlay::Ime::Tsf {
    namespace {
        using Microsoft::WRL::ComPtr;

        std::atomic_uint64_t g_CandidateRevision{1};
        std::atomic_bool g_HasCandidates{false};

        void PublishCandidateState(bool hasCandidates) noexcept {
            g_HasCandidates.store(hasCandidates, std::memory_order_release);
            g_CandidateRevision.fetch_add(1, std::memory_order_acq_rel);
        }

        void PublishCandidatesCleared() noexcept {
            if (g_HasCandidates.exchange(false, std::memory_order_acq_rel))
                g_CandidateRevision.fetch_add(1, std::memory_order_acq_rel);
        }

        class CandidateController final : public ITfUIElementSink {
        public:
            CandidateController() = default;
            CandidateController(const CandidateController &) = delete;
            CandidateController &operator=(const CandidateController &) = delete;

            bool Initialize() {
                m_ThreadId = ::GetCurrentThreadId();
                const HRESULT apartment = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                if (SUCCEEDED(apartment)) {
                    m_UninitializeCom = true;
                } else if (apartment != RPC_E_CHANGED_MODE) {
                    return false;
                }

                HRESULT result = ::CoCreateInstance(
                    CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER,
                    IID_PPV_ARGS(m_ThreadManager.GetAddressOf()));
                if (FAILED(result)) {
                    Shutdown();
                    return false;
                }

                result = m_ThreadManager->ActivateEx(
                    &m_ClientId, TF_TMAE_UIELEMENTENABLEDONLY);
                if (FAILED(result)) {
                    Shutdown();
                    return false;
                }
                m_Activated = true;

                result = m_ThreadManager.As(&m_UiElements);
                if (FAILED(result)) {
                    Shutdown();
                    return false;
                }
                result = m_ThreadManager.As(&m_Source);
                if (FAILED(result)) {
                    Shutdown();
                    return false;
                }
                result = m_Source->AdviseSink(__uuidof(ITfUIElementSink),
                                              static_cast<ITfUIElementSink *>(this), &m_SinkCookie);
                if (FAILED(result)) {
                    Shutdown();
                    return false;
                }
                return true;
            }

            void Shutdown() {
                if (m_Source && m_SinkCookie != TF_INVALID_COOKIE) {
                    m_Source->UnadviseSink(m_SinkCookie);
                    m_SinkCookie = TF_INVALID_COOKIE;
                }
                if (m_ThreadManager && m_Activated) {
                    m_ThreadManager->Deactivate();
                    m_Activated = false;
                }

                m_Source.Reset();
                m_UiElements.Reset();
                m_ThreadManager.Reset();
                {
                    std::lock_guard lock(m_StateMutex);
                    m_ElementIds.clear();
                    m_Candidates.reset();
                    PublishCandidatesCleared();
                }
                if (m_UninitializeCom) {
                    ::CoUninitialize();
                    m_UninitializeCom = false;
                }
                m_ThreadId = 0;
            }

            std::uint64_t CopyCandidates(std::optional<CandidateListSnapshot> &candidates) const {
                std::lock_guard lock(m_StateMutex);
                candidates = m_Candidates;
                return g_CandidateRevision.load(std::memory_order_acquire);
            }

            void ClearCandidates() {
                std::lock_guard lock(m_StateMutex);
                m_ElementIds.clear();
                m_Candidates.reset();
                PublishCandidatesCleared();
            }

            bool MoveSelection(CandidateDirection direction) {
                if (::GetCurrentThreadId() != m_ThreadId)
                    return false;

                const std::vector<DWORD> ids = CopyElementIds();
                for (auto position = ids.rbegin(); position != ids.rend(); ++position) {
                    ComPtr<ITfCandidateListUIElementBehavior> behavior;
                    if (!GetCandidate(*position, behavior))
                        continue;

                    UINT count = 0;
                    if (behavior->GetCount(&count) != S_OK || count == 0 ||
                        count > MaxCandidateCount) {
                        continue;
                    }

                    UINT selected = 0;
                    const HRESULT selectionResult = behavior->GetSelection(&selected);
                    const std::optional<std::uint32_t> selection =
                        selectionResult == S_OK && selected < count
                            ? std::optional<std::uint32_t>(selected)
                            : std::nullopt;
                    const std::optional<std::uint32_t> target =
                        StepCandidateIndex(count, selection, direction);
                    if (!target)
                        continue;

                    const HRESULT result = behavior->SetSelection(*target);
                    if (FAILED(result))
                        continue;

                    Refresh(*position);
                    return true;
                }
                return false;
            }

            HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **object) override {
                if (!object)
                    return E_POINTER;
                *object = nullptr;
                if (iid == __uuidof(IUnknown) ||
                    iid == __uuidof(ITfUIElementSink)) {
                    *object = static_cast<ITfUIElementSink *>(this);
                    AddRef();
                    return S_OK;
                }
                return E_NOINTERFACE;
            }

            ULONG STDMETHODCALLTYPE AddRef() override {
                return ++m_References;
            }

            ULONG STDMETHODCALLTYPE Release() override {
                const ULONG references = --m_References;
                if (references == 0)
                    delete this;
                return references;
            }

            HRESULT STDMETHODCALLTYPE BeginUIElement(DWORD elementId, BOOL *show) override {
                if (!show)
                    return E_INVALIDARG;

                ComPtr<ITfCandidateListUIElement> candidate;
                if (!GetCandidate(elementId, candidate))
                    return S_OK;

                {
                    std::lock_guard lock(m_StateMutex);
                    const auto position = std::find(
                        m_ElementIds.begin(), m_ElementIds.end(), elementId);
                    if (position != m_ElementIds.end())
                        m_ElementIds.erase(position);
                    if (m_ElementIds.size() >= MaxCandidateLists)
                        m_ElementIds.erase(m_ElementIds.begin());
                    m_ElementIds.push_back(elementId);
                    m_Candidates.reset();
                    PublishCandidatesCleared();
                }
                *show = FALSE;
                Refresh(elementId, candidate.Get());
                return S_OK;
            }

            HRESULT STDMETHODCALLTYPE UpdateUIElement(DWORD elementId) override {
                if (IsTracked(elementId))
                    Refresh(elementId);
                return S_OK;
            }

            HRESULT STDMETHODCALLTYPE EndUIElement(DWORD elementId) override {
                DWORD replacement = TF_INVALID_UIELEMENTID;
                {
                    std::lock_guard lock(m_StateMutex);
                    const auto position = std::find(
                        m_ElementIds.begin(), m_ElementIds.end(), elementId);
                    if (position == m_ElementIds.end())
                        return S_OK;
                    const bool current = position + 1 == m_ElementIds.end();
                    m_ElementIds.erase(position);
                    if (m_ElementIds.empty()) {
                        m_Candidates.reset();
                    } else if (current) {
                        replacement = m_ElementIds.back();
                        m_Candidates.reset();
                    }
                    if (current)
                        PublishCandidatesCleared();
                }
                if (replacement != TF_INVALID_UIELEMENTID)
                    Refresh(replacement);
                return S_OK;
            }

        private:
            ~CandidateController() = default;

            template <typename Candidate>
            bool GetCandidate(DWORD elementId, ComPtr<Candidate> &candidate) const {
                if (!m_UiElements)
                    return false;
                ComPtr<ITfUIElement> element;
                if (FAILED(m_UiElements->GetUIElement(
                        elementId, element.GetAddressOf()))) {
                    return false;
                }
                return SUCCEEDED(element.As(&candidate));
            }

            std::vector<DWORD> CopyElementIds() const {
                std::lock_guard lock(m_StateMutex);
                return m_ElementIds;
            }

            bool IsTracked(DWORD elementId) const {
                std::lock_guard lock(m_StateMutex);
                return std::find(m_ElementIds.begin(), m_ElementIds.end(),
                                 elementId) != m_ElementIds.end();
            }

            void Refresh(DWORD elementId) {
                ComPtr<ITfCandidateListUIElement> candidate;
                if (GetCandidate(elementId, candidate))
                    Refresh(elementId, candidate.Get());
            }

            void Refresh(DWORD elementId, ITfCandidateListUIElement *candidate) {
                CandidateListSnapshot list;
                const bool read = ReadCandidateList(candidate, list);
                std::optional<CandidateListSnapshot> refreshed;
                if (read)
                    refreshed = std::move(list);

                std::lock_guard lock(m_StateMutex);
                if (m_ElementIds.empty() ||
                    m_ElementIds.back() != elementId) {
                    return;
                }
                if (m_Candidates == refreshed)
                    return;
                m_Candidates = std::move(refreshed);
                PublishCandidateState(m_Candidates.has_value() && !m_Candidates->items.empty());
            }

            static bool ReadCandidateList(ITfCandidateListUIElement *candidate,
                                          CandidateListSnapshot &output) {
                if (!candidate)
                    return false;

                UINT count = 0;
                if (candidate->GetCount(&count) != S_OK ||
                    count > MaxCandidateCount) {
                    return false;
                }

                CandidateListSnapshot list;
                list.items.reserve(count);
                for (UINT index = 0; index < count; ++index) {
                    BSTR text = nullptr;
                    if (candidate->GetString(index, &text) != S_OK || !text) {
                        ::SysFreeString(text);
                        return false;
                    }
                    const UINT length = ::SysStringLen(text);
                    std::u16string item;
                    item.reserve(length);
                    for (UINT character = 0; character < length; ++character)
                        item.push_back(static_cast<char16_t>(text[character]));
                    list.items.push_back(std::move(item));
                    ::SysFreeString(text);
                }

                UINT selection = 0;
                if (candidate->GetSelection(&selection) == S_OK &&
                    selection < count) {
                    list.selection = selection;
                    list.hasSelection = true;
                }
                ReadPage(candidate, count, list);
                output = std::move(list);
                return true;
            }

            static void ReadPage(ITfCandidateListUIElement *candidate, UINT count,
                                 CandidateListSnapshot &list) {
                if (count == 0)
                    return;

                UINT pageCount = 0;
                candidate->GetPageIndex(nullptr, 0, &pageCount);
                if (pageCount == 0 || pageCount > count)
                    return;

                std::vector<UINT> starts(pageCount);
                UINT copiedPageCount = pageCount;
                if (candidate->GetPageIndex(
                        starts.data(), pageCount, &copiedPageCount) != S_OK ||
                    copiedPageCount == 0 || copiedPageCount > pageCount) {
                    return;
                }
                starts.resize(copiedPageCount);

                UINT currentPage = 0;
                if (candidate->GetCurrentPage(&currentPage) != S_OK ||
                    currentPage >= starts.size()) {
                    return;
                }

                const UINT begin = starts[currentPage];
                const UINT end = currentPage + 1 < starts.size()
                    ? starts[currentPage + 1]
                    : count;
                if (begin >= count || end <= begin || end > count)
                    return;
                list.pageStart = begin;
                list.pageSize = end - begin;
            }

            std::atomic_ulong m_References{1};
            DWORD m_ThreadId = 0;
            TfClientId m_ClientId = TF_CLIENTID_NULL;
            DWORD m_SinkCookie = TF_INVALID_COOKIE;
            bool m_UninitializeCom = false;
            bool m_Activated = false;
            ComPtr<ITfThreadMgrEx> m_ThreadManager;
            ComPtr<ITfUIElementMgr> m_UiElements;
            ComPtr<ITfSource> m_Source;
            mutable std::mutex m_StateMutex;
            std::vector<DWORD> m_ElementIds;
            std::optional<CandidateListSnapshot> m_Candidates;
        };

        std::mutex g_ControllerMutex;
        ComPtr<CandidateController> g_Controller;

        ComPtr<CandidateController> CopyController() {
            std::lock_guard lock(g_ControllerMutex);
            return g_Controller;
        }
    }

    void Attach() {
        if (CopyController())
            return;

        ComPtr<CandidateController> controller;
        controller.Attach(new (std::nothrow) CandidateController());
        if (!controller || !controller->Initialize())
            return;

        std::lock_guard lock(g_ControllerMutex);
        g_Controller = std::move(controller);
    }

    void Detach() {
        ComPtr<CandidateController> controller;
        {
            std::lock_guard lock(g_ControllerMutex);
            controller.Swap(g_Controller);
        }
        if (controller)
            controller->Shutdown();
    }

    void ClearCandidates() {
        const ComPtr<CandidateController> controller = CopyController();
        if (controller) {
            controller->ClearCandidates();
        } else {
            PublishCandidatesCleared();
        }
    }

    std::uint64_t Revision() noexcept {
        return g_CandidateRevision.load(std::memory_order_acquire);
    }

    bool HasCandidates() noexcept {
        return g_HasCandidates.load(std::memory_order_acquire);
    }

    std::uint64_t ApplyCandidates(Snapshot &snapshot) {
        const ComPtr<CandidateController> controller = CopyController();
        if (!controller)
            return Revision();

        std::optional<CandidateListSnapshot> candidates;
        const std::uint64_t revision = controller->CopyCandidates(candidates);
        if (!candidates)
            return revision;

        for (auto &list : snapshot.candidateLists)
            list.reset();
        if (!candidates->items.empty())
            snapshot.candidateLists.front() = std::move(*candidates);
        return revision;
    }

    bool MoveSelection(CandidateDirection direction) {
        const ComPtr<CandidateController> controller = CopyController();
        return controller && controller->MoveSelection(direction);
    }
}

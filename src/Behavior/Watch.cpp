#include "Behavior/Watch.h"

#include <utility>

namespace BML::Behavior {
namespace {

Status Failure(Error error, std::string message) {
    return {error, CK_OK, CKBR_BEHAVIORERROR, std::move(message)};
}

} // namespace

WatchBinding::WatchBinding(PlanCallbackState state, Function function)
    : m_State(std::move(state)), m_Lease(m_State.OpenLease()),
      m_Function(std::move(function)) {}

Status WatchBinding::Invoke(const WatchEvent &event) noexcept {
    CallbackCall call = InvokeCallback(
        m_Lease, CKBR_BEHAVIORERROR, [&] {
            if (m_Function)
                m_Function(event);
            return CKBR_OK;
        });
    if (!call.Fault)
        return {};
    return Failure(call.Fault.Code == CallbackError::Exception
                       ? Error::CallbackFailed : Error::InvalidState,
                   call.Fault.Message);
}

void WatchBinding::CloseAdmission() noexcept {
    if (m_Retired.exchange(true, std::memory_order_acq_rel))
        return;
    (void) m_Lease.Close();
    m_State.Retire();
}

bool WatchBinding::RetireAtSafePoint() noexcept {
    CloseAdmission();
    return m_State.Collect();
}

Status Watch::Open(GraphSource &source, WatchSpec spec,
    PlanCallbackState state,
                   WatchBinding::Function callback,
                   std::shared_ptr<Watch> &out) {
    out.reset();
    if (!callback)
        return Failure(Error::InvalidState,
                       "A Behavior Watch requires a callback.");

    auto watch = std::shared_ptr<Watch>(new Watch(
        source, std::move(spec), nullptr));
    Status status = watch->ReadBaseline();
    if (!status)
        return status;
    watch->m_Binding = std::make_shared<WatchBinding>(
        std::move(state), std::move(callback));
    out = std::move(watch);
    return {};
}

Status Watch::ReadBaseline() {
    switch (m_Spec.Kind) {
    case WatchKind::GraphChanged:
        return m_Source.GraphFingerprint(
            m_Spec.Root, m_Spec.View, m_Fingerprint);
    case WatchKind::LayoutChanged:
        return m_Source.LayoutFingerprint(m_Spec.Node, m_Fingerprint);
    case WatchKind::SampledValueChanged:
        return m_Source.ReadValue(
            m_Spec.Node, m_Spec.ValueSlot, m_Spec.Read, m_Value);
    }
    return Failure(Error::InvalidState, "The Behavior Watch kind is invalid.");
}

Status Watch::Poll(std::uint64_t frame) {
    WatchInfo info = Read();
    if (info.State == WatchState::Failed)
        return info.Diagnostic;
    if (!m_Open.load(std::memory_order_acquire))
        return Failure(Error::InvalidState,
                       "The Behavior Watch is closed.");

    WatchEvent event;
    event.Kind = m_Spec.Kind;
    event.Frame = frame;
    bool changed = false;
    Status status;
    switch (m_Spec.Kind) {
    case WatchKind::GraphChanged:
    case WatchKind::LayoutChanged: {
        std::uint64_t current = 0;
        status = m_Spec.Kind == WatchKind::GraphChanged
            ? m_Source.GraphFingerprint(m_Spec.Root, m_Spec.View, current)
            : m_Source.LayoutFingerprint(m_Spec.Node, current);
        if (!status)
            break;
        changed = current != m_Fingerprint;
        event.Before = m_Fingerprint;
        event.After = current;
        m_Fingerprint = current;
        break;
    }
    case WatchKind::SampledValueChanged: {
        GraphValue current;
        status = m_Source.ReadValue(
            m_Spec.Node, m_Spec.ValueSlot, m_Spec.Read, current);
        if (!status)
            break;
        changed = current != m_Value;
        event.PreviousValue = m_Value;
        event.CurrentValue = current;
        m_Value = std::move(current);
        break;
    }
    }

    if (!status) {
        Fail(status);
        return status;
    }
    if (!changed)
        return {};
    event.Sequence = ++m_Sequence;
    status = m_Binding->Invoke(event);
    if (!status)
        Fail(status);
    return status;
}

WatchInfo Watch::Read() const {
    std::lock_guard<std::mutex> lock(m_StateMutex);
    return m_Info;
}

void Watch::Fail(Status status) {
    std::lock_guard<std::mutex> lock(m_StateMutex);
    if (m_Info.State == WatchState::Failed)
        return;
    m_Info.State = WatchState::Failed;
    m_Info.Diagnostic = std::move(status);
    Close();
}

void Watch::Close() noexcept {
    if (!m_Open.exchange(false, std::memory_order_acq_rel))
        return;
    if (m_Binding)
        m_Binding->CloseAdmission();
}

bool Watch::RetireAtSafePoint() noexcept {
    Close();
    return !m_Binding || m_Binding->RetireAtSafePoint();
}

} // namespace BML::Behavior

#include "Behavior/Watch.h"

#include <cstdint>
#include <functional>
#include <utility>

namespace BML::Behavior::Internal {
namespace {

Status Failure(Error error, std::string message) {
    return {error, CK_OK, CKBR_BEHAVIORERROR, std::move(message)};
}

std::size_t Mix(std::size_t seed, std::size_t value) noexcept {
    // boost::hash_combine's inexpensive avalanche is sufficient for native
    // identities that are already well distributed pointers and object ids.
    return seed ^ (value + static_cast<std::size_t>(0x9e3779b9u) +
                   (seed << 6u) + (seed >> 2u));
}

std::size_t HashRef(GraphSource *source, const NativeRef &ref) noexcept {
    std::size_t hash = std::hash<GraphSource *>{}(source);
    hash = Mix(hash, std::hash<std::uint64_t>{}(ref.Id));
    return Mix(hash, std::hash<const void *>{}(ref.Address));
}

} // namespace

std::size_t WatchReadings::GraphKeyHash::operator()(
    const GraphKey &key) const noexcept {
    return Mix(HashRef(key.Source, key.Root),
               std::hash<unsigned>{}(static_cast<unsigned>(key.View)));
}

std::size_t WatchReadings::LayoutKeyHash::operator()(
    const LayoutKey &key) const noexcept {
    return HashRef(key.Source, key.Node);
}

template <class Readings>
void WatchReadings::ForgetUnused(Readings &readings) {
    for (auto reading = readings.begin(); reading != readings.end();) {
        // This maintenance runs infrequently. Keeping the previous frame
        // preserves allocations for stable Watches while bounding identities
        // left behind by closed Watches and destroyed CK objects.
        if (reading->second.Frame + 1u < m_Frame)
            reading = readings.erase(reading);
        else
            ++reading;
    }
}

void WatchReadings::BeginFrame(std::uint64_t frame) noexcept {
    if (frame == 0) {
        m_Graphs.clear();
        m_Layouts.clear();
        m_Frame = 1;
        return;
    }
    m_Frame = frame;
    if ((m_Frame & 0xffu) == 0) {
        ForgetUnused(m_Graphs);
        ForgetUnused(m_Layouts);
    }
}

Status WatchReadings::GraphFingerprint(
    GraphSource &source, const NativeRef &root, GraphView view,
    std::uint64_t &out) {
    GraphReading &reading = m_Graphs[{&source, root, view}];
    if (reading.Frame != m_Frame) {
        reading.Result = source.GraphFingerprint(
            root, view, reading.Fingerprint);
        reading.Frame = m_Frame;
    }
    out = reading.Fingerprint;
    return reading.Result;
}

Status WatchReadings::LayoutFingerprint(
    GraphSource &source, const NativeRef &node, std::uint64_t &out) {
    LayoutReading &reading = m_Layouts[{&source, node}];
    if (reading.Frame != m_Frame) {
        reading.Result = source.LayoutFingerprint(node, reading.Fingerprint);
        reading.Frame = m_Frame;
    }
    out = reading.Fingerprint;
    return reading.Result;
}

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
    Status status = Failure(call.Fault.Code == CallbackError::Exception
                                ? Error::CallbackFailed : Error::InvalidState,
                            call.Fault.Message);
    status.Details.Stage = Phase::LifecycleCallback;
    return status;
}

void WatchBinding::CloseAdmission() noexcept {
    if (m_Retired.exchange(true, std::memory_order_acq_rel))
        return;
    (void) m_Lease.Close();
    m_State.Retire();
}

bool WatchBinding::IsOpen() const noexcept {
    return m_Lease.State() == CallbackLeaseState::Open;
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
    if (!watch->m_Binding->IsOpen()) {
        // Fail Open now instead of on the first observed change.
        (void) watch->m_Binding->RetireAtSafePoint();
        return Failure(Error::CallbackFailed,
                       "The Behavior Watch callback state is already retired.");
    }
    out = std::move(watch);
    return {};
}

Status Watch::ReadBaseline() {
    switch (m_Spec.Kind) {
    case WatchKind::GraphChanged:
        return m_Source.GraphFingerprint(
            m_Spec.Root, m_Spec.View, m_Fingerprint);
    case WatchKind::LayoutChanged:
        if (m_Spec.LayoutGeneration) {
            Layout layout;
            Status status = m_Source.ReadLayout(m_Spec.Node, layout);
            if (!status)
                return status;
            if (layout.Generation != m_Spec.LayoutGeneration)
                return Failure(
                    Error::StaleLayout,
                    "The watched Node belongs to an older Behavior Layout.");
        }
        return m_Source.LayoutFingerprint(m_Spec.Node, m_Fingerprint);
    case WatchKind::SampledValueChanged:
        return m_Source.ReadValue(
            m_Spec.Node, m_Spec.LayoutGeneration, m_Spec.ValueSlot,
            m_Spec.Read, m_Value);
    }
    return Failure(Error::InvalidState, "The Behavior Watch kind is invalid.");
}

Status Watch::Poll(std::uint64_t frame) {
    return Poll(frame, nullptr);
}

Status Watch::Poll(std::uint64_t frame, WatchReadings &readings) {
    return Poll(frame, &readings);
}

Status Watch::Poll(std::uint64_t frame, WatchReadings *readings) {
    if (!m_Open.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        if (m_Info.State == WatchState::Failed)
            return m_Info.Diagnostic;
        return Failure(Error::InvalidState,
                       "The Behavior Watch is closed.");
    }

    WatchEvent event;
    event.Kind = m_Spec.Kind;
    event.Frame = frame;
    bool changed = false;
    Status status;
    switch (m_Spec.Kind) {
    case WatchKind::GraphChanged:
    case WatchKind::LayoutChanged: {
        std::uint64_t current = 0;
        if (m_Spec.Kind == WatchKind::GraphChanged) {
            status = readings
                ? readings->GraphFingerprint(
                      m_Source, m_Spec.Root, m_Spec.View, current)
                : m_Source.GraphFingerprint(m_Spec.Root, m_Spec.View, current);
        } else {
            status = readings
                ? readings->LayoutFingerprint(m_Source, m_Spec.Node, current)
                : m_Source.LayoutFingerprint(m_Spec.Node, current);
        }
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
            m_Spec.Node, m_Spec.LayoutGeneration, m_Spec.ValueSlot,
            m_Spec.Read, current);
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

} // namespace BML::Behavior::Internal

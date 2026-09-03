#include "Behavior/Callback.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>

namespace BML::Behavior {

struct PlanCallbackState::Control {
    mutable std::mutex Mutex;
    // Owns the storage Value points at when the caller does not. It outlives
    // every lease because a lease holds this Control.
    std::shared_ptr<void> Owner;
    void *Value = nullptr;
    Reference Retain = nullptr;
    Reference Release = nullptr;
    std::size_t Leases = 0;
    std::size_t Invocations = 0;
    bool ReferenceHeld = false;
    // Retain is author code and runs without Mutex held. While it is in
    // progress, another lease must not observe a reference that has not yet
    // been acquired. OpenLease never waits on that author callback.
    bool ReferencePending = false;
    bool RetireRequested = false;
    bool Released = false;
};

struct CallbackInvocation::LeaseControl {
    mutable std::mutex Mutex;
    std::shared_ptr<PlanCallbackState::Control> Plan;
    CallbackLeaseState State = CallbackLeaseState::Open;
    std::size_t Invocations = 0;
    bool RetireRequested = false;
    std::atomic<bool> Counted{true};
};

namespace {

thread_local std::vector<const void *> g_InvocationStack;

void RetireLease(const std::shared_ptr<CallbackInvocation::LeaseControl> &lease) {
    if (!lease || !lease->Counted)
        return;
    if (!lease->Counted.exchange(false, std::memory_order_acq_rel))
        return;
    std::lock_guard<std::mutex> planLock(lease->Plan->Mutex);
    if (lease->Plan->Leases > 0)
        --lease->Plan->Leases;
}

} // namespace

PlanCallbackState PlanCallbackState::Retained(void *state, Reference retain,
                                              Reference release) {
    auto control = std::make_shared<Control>();
    control->Value = state;
    control->Retain = retain;
    control->Release = release;
    return PlanCallbackState(std::move(control));
}

PlanCallbackState PlanCallbackState::Retained(std::shared_ptr<void> owner,
                                              void *state, Reference retain,
                                              Reference release) {
    auto control = std::make_shared<Control>();
    control->Owner = std::move(owner);
    control->Value = state;
    control->Retain = retain;
    control->Release = release;
    return PlanCallbackState(std::move(control));
}

PlanCallbackState PlanCallbackState::Static(void *state) {
    return Retained(state, nullptr, nullptr);
}

CallbackLease PlanCallbackState::OpenLease() const {
    if (!m_Control)
        return {};

    // Allocate everything that may throw before changing the plan ledger.
    auto lease = std::make_shared<CallbackInvocation::LeaseControl>();
    lease->Plan = m_Control;

    Reference retain = nullptr;
    void *value = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_Control->Mutex);
        if (m_Control->RetireRequested || m_Control->Released ||
            m_Control->ReferencePending)
            return {};
        if (!m_Control->ReferenceHeld) {
            retain = m_Control->Retain;
            value = m_Control->Value;
            if (retain) {
                m_Control->ReferencePending = true;
            } else {
                m_Control->ReferenceHeld = true;
            }
        }
        if (!retain) {
            ++m_Control->Leases;
            return CallbackLease(std::move(lease));
        }
    }

    try {
        retain(value);
    } catch (...) {
        std::lock_guard<std::mutex> lock(m_Control->Mutex);
        m_Control->ReferencePending = false;
        throw;
    }

    {
        std::lock_guard<std::mutex> lock(m_Control->Mutex);
        m_Control->ReferencePending = false;
        m_Control->ReferenceHeld = true;
        // Retire may have been requested reentrantly by author Retain. The
        // reference remains in the ledger for Collect to release at a safe
        // point, but no new callback admission is opened.
        if (m_Control->RetireRequested || m_Control->Released)
            return {};
        ++m_Control->Leases;
    }
    return CallbackLease(std::move(lease));
}

void PlanCallbackState::Retire() const noexcept {
    if (!m_Control)
        return;
    std::lock_guard<std::mutex> lock(m_Control->Mutex);
    m_Control->RetireRequested = true;
}

bool PlanCallbackState::Collect() const noexcept {
    if (!m_Control)
        return true;

    Reference release = nullptr;
    void *value = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_Control->Mutex);
        if (!m_Control->RetireRequested || m_Control->ReferencePending ||
            m_Control->Leases != 0 ||
            m_Control->Invocations != 0) {
            return false;
        }
        if (!m_Control->Released) {
            m_Control->Released = true;
            if (m_Control->ReferenceHeld) {
                release = m_Control->Release;
                value = m_Control->Value;
                m_Control->ReferenceHeld = false;
            }
        }
    }
    if (release) {
        try {
            release(value);
        } catch (...) {
            // A foreign callback must never escape a Loader safe point. The
            // reference was already retired from this ledger and is not
            // invoked twice after an author Release reports failure.
        }
    }
    return true;
}

bool PlanCallbackState::Retired() const noexcept {
    if (!m_Control)
        return true;
    std::lock_guard<std::mutex> lock(m_Control->Mutex);
    return m_Control->RetireRequested;
}

void *PlanCallbackState::State() const noexcept {
    if (!m_Control)
        return nullptr;
    std::lock_guard<std::mutex> lock(m_Control->Mutex);
    return m_Control->Value;
}

CallbackInvocation::CallbackInvocation(std::shared_ptr<LeaseControl> lease)
    : m_Lease(std::move(lease)) {
    g_InvocationStack.push_back(m_Lease.get());
}

CallbackInvocation::CallbackInvocation(CallbackInvocation &&other) noexcept
    : m_Lease(std::move(other.m_Lease)) {}

CallbackInvocation &CallbackInvocation::operator=(
    CallbackInvocation &&other) noexcept {
    if (this == &other)
        return *this;
    Leave();
    m_Lease = std::move(other.m_Lease);
    return *this;
}

CallbackInvocation::~CallbackInvocation() {
    Leave();
}

bool CallbackInvocation::IsCurrent() const noexcept {
    return m_Lease && !g_InvocationStack.empty() &&
           g_InvocationStack.back() == m_Lease.get();
}

bool CallbackInvocation::Active() noexcept {
    return !g_InvocationStack.empty();
}

void CallbackInvocation::Leave() noexcept {
    if (!m_Lease)
        return;
    auto position = std::find(g_InvocationStack.rbegin(),
                              g_InvocationStack.rend(), m_Lease.get());
    if (position != g_InvocationStack.rend())
        g_InvocationStack.erase(std::next(position).base());

    bool retire = false;
    {
        std::lock_guard<std::mutex> lock(m_Lease->Mutex);
        if (m_Lease->Invocations > 0)
            --m_Lease->Invocations;
        retire = m_Lease->Invocations == 0 &&
                 m_Lease->State == CallbackLeaseState::Closing &&
                 m_Lease->RetireRequested;
        if (retire)
            m_Lease->State = CallbackLeaseState::Closed;
    }
    {
        std::lock_guard<std::mutex> lock(m_Lease->Plan->Mutex);
        if (m_Lease->Plan->Invocations > 0)
            --m_Lease->Plan->Invocations;
    }
    if (retire)
        RetireLease(m_Lease);
    m_Lease.reset();
}

CallbackLease::~CallbackLease() {
    Close();
}

CallbackInvocation CallbackLease::Enter() const {
    if (!m_Lease)
        return {};
    {
        std::lock_guard<std::mutex> lock(m_Lease->Mutex);
        if (m_Lease->State != CallbackLeaseState::Open)
            return {};
        ++m_Lease->Invocations;
    }
    {
        std::lock_guard<std::mutex> lock(m_Lease->Plan->Mutex);
        ++m_Lease->Plan->Invocations;
    }
    return CallbackInvocation(m_Lease);
}

CallbackCloseResult CallbackLease::CloseAdmission() noexcept {
    if (!m_Lease)
        return CallbackCloseResult::Closed;

    {
        std::lock_guard<std::mutex> lock(m_Lease->Mutex);
        if (m_Lease->State == CallbackLeaseState::Closed)
            return CallbackCloseResult::Closed;
        m_Lease->State = CallbackLeaseState::Closing;
        return m_Lease->Invocations == 0
            ? CallbackCloseResult::Ready
            : CallbackCloseResult::Queued;
    }
}

CallbackCloseResult CallbackLease::Close() noexcept {
    if (!m_Lease)
        return CallbackCloseResult::Closed;

    bool retire = false;
    CallbackCloseResult result = CallbackCloseResult::Queued;
    {
        std::lock_guard<std::mutex> lock(m_Lease->Mutex);
        if (m_Lease->State == CallbackLeaseState::Closed)
            return CallbackCloseResult::Closed;
        m_Lease->State = CallbackLeaseState::Closing;
        m_Lease->RetireRequested = true;
        if (m_Lease->Invocations == 0) {
            m_Lease->State = CallbackLeaseState::Closed;
            retire = true;
            result = CallbackCloseResult::Ready;
        }
    }
    if (retire)
        RetireLease(m_Lease);
    return result;
}

CallbackLeaseState CallbackLease::State() const noexcept {
    if (!m_Lease)
        return CallbackLeaseState::Closed;
    std::lock_guard<std::mutex> lock(m_Lease->Mutex);
    return m_Lease->State;
}

bool CallbackLease::IsCurrentInvocation() const noexcept {
    return m_Lease &&
        std::find(g_InvocationStack.begin(), g_InvocationStack.end(),
                  m_Lease.get()) != g_InvocationStack.end();
}

} // namespace BML::Behavior

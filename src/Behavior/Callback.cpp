#include "Behavior/Callback.h"

#include <algorithm>
#include <vector>

namespace BML::Behavior::Internal {

struct PlanCallbackState::Control {
    // Owns the storage Value points at when the caller does not. It outlives
    // every lease because a lease holds this Control.
    std::shared_ptr<void> Owner;
    void *Value = nullptr;
    Reference Retain = nullptr;
    Reference Release = nullptr;
    std::size_t Leases = 0;
    std::size_t Invocations = 0;
    bool ReferenceHeld = false;
    // Retain is author code. While it is in progress, a reentrant lease must
    // not observe a reference that has not yet been acquired.
    bool ReferencePending = false;
    bool RetireRequested = false;
    bool Released = false;
};

struct CallbackInvocation::LeaseControl {
    std::shared_ptr<PlanCallbackState::Control> Plan;
    std::vector<std::shared_ptr<const CallbackAdmission>> Admissions;
    CallbackLeaseState State = CallbackLeaseState::Open;
    std::size_t Invocations = 0;
    bool RetireRequested = false;
    bool Counted = true;
};

namespace {

std::vector<const void *> g_InvocationStack;

struct PendingRelease {
    ReleaseScope::Reference Release = nullptr;
    void *State = nullptr;
};

std::size_t g_ReleaseDepth = 0;
std::vector<PendingRelease> g_PendingReleases;

void InvokeRelease(ReleaseScope::Reference release, void *state) noexcept {
    try {
        release(state);
    } catch (...) {
        // A foreign callback must never escape a Loader safe point. The
        // reference was already retired from its ledger and is not invoked
        // twice after an author Release reports failure.
    }
}

void RetireLease(const std::shared_ptr<CallbackInvocation::LeaseControl> &lease) {
    if (!lease || !lease->Counted)
        return;
    lease->Counted = false;
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

    if (m_Control->RetireRequested || m_Control->Released ||
        m_Control->ReferencePending)
        return {};
    Reference retain = nullptr;
    if (!m_Control->ReferenceHeld) {
        retain = m_Control->Retain;
        if (retain)
            m_Control->ReferencePending = true;
        else
            m_Control->ReferenceHeld = true;
    }
    if (!retain) {
        ++m_Control->Leases;
        return CallbackLease(std::move(lease));
    }

    try {
        retain(m_Control->Value);
    } catch (...) {
        m_Control->ReferencePending = false;
        throw;
    }

    m_Control->ReferencePending = false;
    m_Control->ReferenceHeld = true;
    // Retire may have been requested reentrantly by author Retain. The
    // reference remains in the ledger for Collect to release at a safe point,
    // but no new callback admission is opened.
    if (m_Control->RetireRequested || m_Control->Released)
        return {};
    ++m_Control->Leases;
    return CallbackLease(std::move(lease));
}

void PlanCallbackState::Retire() const noexcept {
    if (m_Control)
        m_Control->RetireRequested = true;
}

bool PlanCallbackState::Collect() const noexcept {
    if (!m_Control)
        return true;

    if (!m_Control->RetireRequested || m_Control->ReferencePending ||
        m_Control->Leases != 0 || m_Control->Invocations != 0) {
        return false;
    }
    if (m_Control->Released)
        return true;
    m_Control->Released = true;
    const Reference release =
        m_Control->ReferenceHeld ? m_Control->Release : nullptr;
    m_Control->ReferenceHeld = false;
    if (release)
        ReleaseScope::Release(release, m_Control->Value);
    return true;
}

bool PlanCallbackState::Retired() const noexcept {
    return !m_Control || m_Control->RetireRequested;
}

void *PlanCallbackState::State() const noexcept {
    return m_Control ? m_Control->Value : nullptr;
}

ReleaseScope::ReleaseScope() noexcept {
    ++g_ReleaseDepth;
}

ReleaseScope::~ReleaseScope() {
    if (g_ReleaseDepth > 1) {
        --g_ReleaseDepth;
        return;
    }
    // The scope stays open while the queue drains, so a Release that drops
    // another reference appends it here instead of running inside this one.
    for (std::size_t index = 0; index < g_PendingReleases.size(); ++index) {
        const PendingRelease pending = g_PendingReleases[index];
        InvokeRelease(pending.Release, pending.State);
    }
    g_PendingReleases.clear();
    --g_ReleaseDepth;
}

void ReleaseScope::Release(Reference release, void *state) noexcept {
    if (!release)
        return;
    if (g_ReleaseDepth != 0) {
        try {
            g_PendingReleases.push_back({release, state});
            return;
        } catch (...) {
            // Without room to queue it, releasing now is still better than
            // leaking the author reference.
        }
    }
    InvokeRelease(release, state);
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

    if (m_Lease->Invocations > 0)
        --m_Lease->Invocations;
    const bool retire = m_Lease->Invocations == 0 &&
                        m_Lease->State == CallbackLeaseState::Closing &&
                        m_Lease->RetireRequested;
    if (retire)
        m_Lease->State = CallbackLeaseState::Closed;
    if (m_Lease->Plan->Invocations > 0)
        --m_Lease->Plan->Invocations;
    if (retire)
        RetireLease(m_Lease);
    m_Lease.reset();
}

CallbackLease::~CallbackLease() {
    Close();
}

void CallbackLease::AdmitThrough(std::shared_ptr<const CallbackAdmission> admission) {
    if (!m_Lease || !admission)
        return;
    if (std::find(m_Lease->Admissions.begin(), m_Lease->Admissions.end(), admission)
            == m_Lease->Admissions.end())
        m_Lease->Admissions.push_back(std::move(admission));
}

CallbackInvocation CallbackLease::Enter() const {
    if (!m_Lease || m_Lease->State != CallbackLeaseState::Open)
        return {};
    for (const auto &admission : m_Lease->Admissions) {
        if (!admission->IsOpen())
            return {};
    }
    ++m_Lease->Invocations;
    ++m_Lease->Plan->Invocations;
    return CallbackInvocation(m_Lease);
}

CallbackCloseResult CallbackLease::CloseAdmission() noexcept {
    if (!m_Lease || m_Lease->State == CallbackLeaseState::Closed)
        return CallbackCloseResult::Closed;
    m_Lease->State = CallbackLeaseState::Closing;
    return m_Lease->Invocations == 0
        ? CallbackCloseResult::Ready
        : CallbackCloseResult::Queued;
}

CallbackCloseResult CallbackLease::Close() noexcept {
    if (!m_Lease || m_Lease->State == CallbackLeaseState::Closed)
        return CallbackCloseResult::Closed;
    m_Lease->RetireRequested = true;
    if (m_Lease->Invocations != 0) {
        m_Lease->State = CallbackLeaseState::Closing;
        return CallbackCloseResult::Queued;
    }
    m_Lease->State = CallbackLeaseState::Closed;
    RetireLease(m_Lease);
    return CallbackCloseResult::Ready;
}

CallbackLeaseState CallbackLease::State() const noexcept {
    if (!m_Lease)
        return CallbackLeaseState::Closed;
    if (m_Lease->State == CallbackLeaseState::Open) {
        for (const auto &admission : m_Lease->Admissions) {
            if (!admission->IsOpen())
                return CallbackLeaseState::Closing;
        }
    }
    return m_Lease->State;
}

bool CallbackLease::IsCurrentInvocation() const noexcept {
    return m_Lease &&
        std::find(g_InvocationStack.begin(), g_InvocationStack.end(),
                  m_Lease.get()) != g_InvocationStack.end();
}

} // namespace BML::Behavior::Internal

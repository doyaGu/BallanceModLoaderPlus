#include "Behavior/Lifecycle.h"

#include <utility>

namespace BML::Behavior {

namespace {

LifecycleFault Fault(LifecycleError code, std::string message,
                     int nativeCode = 0) {
    return {code, nativeCode, std::move(message)};
}

void SupplyFault(LifecycleFault &fault, LifecycleError code,
                 const char *message) {
    if (!fault)
        fault = Fault(code, message);
}

} // namespace

Lifecycle::Lifecycle(Lifecycle &&other) noexcept {
    *this = std::move(other);
}

Lifecycle &Lifecycle::operator=(Lifecycle &&other) noexcept {
    if (this == &other)
        return *this;
    m_State = other.m_State;
    m_Ledger = other.m_Ledger;
    m_Failure = std::move(other.m_Failure);
    m_Identity = std::move(other.m_Identity);
    m_HasIdentity = other.m_HasIdentity;
    m_CloseRequested.store(
        other.m_CloseRequested.load(std::memory_order_acquire),
        std::memory_order_release);
    m_ResetRequested.store(
        other.m_ResetRequested.load(std::memory_order_acquire),
        std::memory_order_release);
    other.m_State = LifecycleState::Closed;
    other.m_Ledger = {};
    other.m_HasIdentity = false;
    other.m_CloseRequested.store(false, std::memory_order_release);
    other.m_ResetRequested.store(false, std::memory_order_release);
    return *this;
}

bool Lifecycle::Configure(const LifecyclePlan &plan,
                          LifecycleAdapter &adapter) {
    return Create(plan, adapter) && Edit(adapter);
}

bool Lifecycle::Create(const LifecyclePlan &plan,
                       LifecycleAdapter &adapter) {
    if (m_State != LifecycleState::New) {
        RecordFailure(Fault(LifecycleError::InvalidState,
                             "Behavior lifecycle was configured more than once."));
        return false;
    }

    m_State = LifecycleState::Configuring;
    LifecycleLayout layout;
    LifecycleFault fault;
    if (!adapter.InitializeAndReflect(layout, fault)) {
        SupplyFault(fault, LifecycleError::InitializationFailed,
                    "Behavior initialization or initial reflection failed.");
        return FailConfiguration(std::move(fault), adapter);
    }

    const bool hasStageZero = !plan.SettingStages.empty() &&
                              plan.SettingStages.front();
    if (hasStageZero && !adapter.WriteSettingStage(0, layout, fault)) {
        SupplyFault(fault, LifecycleError::SettingFailed,
                    "The first Setting stage could not be written.");
        return FailConfiguration(std::move(fault), adapter);
    }

    if (!adapter.AlignRelationsAndPlace(fault)) {
        SupplyFault(fault, LifecycleError::RelationFailed,
                    "Behavior relations could not be aligned and placed.");
        return FailConfiguration(std::move(fault), adapter);
    }
    m_Ledger.Placed = true;

    if (!adapter.CaptureIdentity(m_Identity, fault)) {
        SupplyFault(fault, LifecycleError::IdentityChanged,
                    "Behavior lifecycle identity could not be captured.");
        return FailConfiguration(std::move(fault), adapter);
    }
    m_HasIdentity = true;

    if (!InvokeConfigured(adapter, LifecycleCallback::Create, m_Identity,
                          layout, fault, &m_Ledger.Created)) {
        return FailConfiguration(std::move(fault), adapter);
    }
    if (hasStageZero && !adapter.WriteSettingStage(0, layout, fault)) {
        SupplyFault(fault, LifecycleError::SettingFailed,
                    "The first Setting stage could not be replayed after CREATE.");
        return FailConfiguration(std::move(fault), adapter);
    }

    if (plan.HasOwner) {
        if (!InvokeConfigured(adapter, LifecycleCallback::Attach, m_Identity,
                              layout, fault, &m_Ledger.Attached)) {
            return FailConfiguration(std::move(fault), adapter);
        }
        if (hasStageZero && !adapter.WriteSettingStage(0, layout, fault)) {
            SupplyFault(fault, LifecycleError::SettingFailed,
                        "The first Setting stage could not be replayed after ATTACH.");
            return FailConfiguration(std::move(fault), adapter);
        }
    }

    if (hasStageZero &&
        !InvokeConfigured(adapter, LifecycleCallback::SettingsEdited,
                          m_Identity, layout, fault)) {
        return FailConfiguration(std::move(fault), adapter);
    }

    for (std::size_t stage = 1; stage < plan.SettingStages.size(); ++stage) {
        if (!plan.SettingStages[stage])
            continue;
        if (!adapter.WriteSettingStage(stage, layout, fault)) {
            SupplyFault(fault, LifecycleError::SettingFailed,
                        "A later Setting stage could not be written.");
            return FailConfiguration(std::move(fault), adapter);
        }
        if (!InvokeConfigured(adapter, LifecycleCallback::SettingsEdited,
                              m_Identity, layout, fault)) {
            return FailConfiguration(std::move(fault), adapter);
        }
    }

    if (plan.HasInterface) {
        if (!adapter.ApplyInterface(fault)) {
            SupplyFault(fault, LifecycleError::BindingFailed,
                        "Behavior interface could not be created.");
            return FailConfiguration(std::move(fault), adapter);
        }
        if (!adapter.Reflect(layout, fault)) {
            SupplyFault(fault, LifecycleError::LayoutFailed,
                        "Behavior layout could not be reflected after its interface was created.");
            return FailConfiguration(std::move(fault), adapter);
        }
    }

    if (CloseRequested()) {
        RecordFailure(Fault(LifecycleError::Cancelled,
                            "Behavior lifecycle was closed during creation."));
        Drain(adapter);
        return false;
    }
    return true;
}

bool Lifecycle::Edit(LifecycleAdapter &adapter) {
    if (m_State != LifecycleState::Configuring) {
        RecordFailure(Fault(
            LifecycleError::InvalidState,
            "Behavior lifecycle was edited outside configuration."));
        return false;
    }

    LifecycleLayout layout;
    LifecycleFault fault;
    if (!adapter.Reflect(layout, fault)) {
        SupplyFault(fault, LifecycleError::LayoutFailed,
                    "Behavior layout could not be reflected before EDITED.");
        return FailConfiguration(std::move(fault), adapter);
    }

    if (!adapter.ApplyBindings(fault)) {
        SupplyFault(fault, LifecycleError::BindingFailed,
                    "Behavior bindings could not be applied.");
        return FailConfiguration(std::move(fault), adapter);
    }

    LifecycleIdentity bindingIdentity;
    if (!adapter.CaptureIdentity(bindingIdentity, fault)) {
        SupplyFault(fault, LifecycleError::IdentityChanged,
                    "Configured binding identity could not be captured.");
        return FailConfiguration(std::move(fault), adapter);
    }
    m_Identity = std::move(bindingIdentity);
    if (!InvokeConfigured(adapter, LifecycleCallback::Edited, m_Identity,
                          layout, fault)) {
        return FailConfiguration(std::move(fault), adapter);
    }
    if (!adapter.ReconcileBindings(layout, fault)) {
        SupplyFault(fault, LifecycleError::BindingFailed,
                    "Behavior bindings no longer resolve after EDITED.");
        return FailConfiguration(std::move(fault), adapter);
    }

    if (CloseRequested()) {
        RecordFailure(Fault(LifecycleError::Cancelled,
                             "Behavior lifecycle was closed during configuration."));
        Drain(adapter);
        return false;
    }

    m_State = LifecycleState::Ready;
    return true;
}

bool Lifecycle::RefreshAfterCallback(LifecycleAdapter &adapter,
                                     LifecycleIdentity &identity,
                                     LifecycleLayout &layout,
                                     LifecycleFault &fault) {
    if (!adapter.Revalidate(identity, fault)) {
        SupplyFault(fault, LifecycleError::IdentityChanged,
                    "A native callback changed Behavior lifecycle identity.");
        return false;
    }
    if (!adapter.Reflect(layout, fault)) {
        SupplyFault(fault, LifecycleError::LayoutFailed,
                    "Behavior layout could not be reflected after a callback.");
        return false;
    }
    LifecycleIdentity refreshed;
    if (!adapter.CaptureIdentity(refreshed, fault)) {
        SupplyFault(fault, LifecycleError::IdentityChanged,
                    "Behavior identity could not be recaptured after layout reflection.");
        return false;
    }
    identity = std::move(refreshed);
    if (CloseRequested()) {
        fault = Fault(LifecycleError::Cancelled,
                      "Behavior lifecycle was closed by a native callback.");
        return false;
    }
    return true;
}

bool Lifecycle::InvokeConfigured(LifecycleAdapter &adapter,
                                 LifecycleCallback callback,
                                 LifecycleIdentity &identity,
                                 LifecycleLayout &layout,
                                 LifecycleFault &fault,
                                 bool *completed) {
    if (!adapter.Invoke(callback, fault)) {
        SupplyFault(fault, LifecycleError::CallbackFailed,
                    "A native Behavior lifecycle callback failed.");
        return false;
    }
    if (completed)
        *completed = true;
    return RefreshAfterCallback(adapter, identity, layout, fault);
}

bool Lifecycle::FailConfiguration(LifecycleFault fault,
                                  LifecycleAdapter &adapter) {
    RecordFailure(std::move(fault));
    RequestClose(false);
    Drain(adapter);
    return false;
}

void Lifecycle::RequestClose(bool reset) noexcept {
    if (reset)
        m_ResetRequested.store(true, std::memory_order_release);
    m_CloseRequested.store(true, std::memory_order_release);
}

bool Lifecycle::Drain(LifecycleAdapter &adapter) {
    if (m_State == LifecycleState::Closed)
        return true;
    if (!CloseRequested())
        return false;

    m_State = LifecycleState::Closing;
    LifecycleFault fault;
    if (!adapter.Deactivate(fault)) {
        SupplyFault(fault, LifecycleError::TeardownFailed,
                    "Behavior deactivation failed during teardown.");
        RecordFailure(std::move(fault));
    }

    const LifecycleIdentity *identity = m_HasIdentity ? &m_Identity : nullptr;
    if (m_ResetRequested.load(std::memory_order_acquire) && m_Ledger.Created)
        TeardownCallback(adapter, LifecycleCallback::Reset, identity);
    if (m_Ledger.Attached)
        TeardownCallback(adapter, LifecycleCallback::Detach, identity);
    if (m_Ledger.Created)
        TeardownCallback(adapter, LifecycleCallback::Delete, identity);

    fault = {};
    if (!adapter.DisconnectAndDestroy(fault)) {
        SupplyFault(fault, LifecycleError::TeardownFailed,
                    "Behavior native destruction failed.");
        RecordFailure(std::move(fault));
    }

    m_Ledger = {};
    m_State = LifecycleState::Closed;
    return true;
}

bool Lifecycle::TeardownCallback(LifecycleAdapter &adapter,
                                 LifecycleCallback callback,
                                 const LifecycleIdentity *identity) {
    LifecycleFault fault;
    bool success = adapter.Invoke(callback, fault);
    if (!success) {
        SupplyFault(fault, LifecycleError::CallbackFailed,
                    "A native Behavior teardown callback failed.");
        RecordFailure(std::move(fault));
        return false;
    }

    if (identity && !adapter.Revalidate(*identity, fault)) {
        SupplyFault(fault, LifecycleError::IdentityChanged,
                    "A native teardown callback changed Behavior identity.");
        RecordFailure(std::move(fault));
        return false;
    }
    return true;
}

void Lifecycle::RecordFailure(LifecycleFault fault) noexcept {
    if (!m_Failure && fault)
        m_Failure = std::move(fault);
}

} // namespace BML::Behavior

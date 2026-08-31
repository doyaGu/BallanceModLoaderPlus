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

bool Lifecycle::Configure(const LifecyclePlan &plan,
                          LifecycleAdapter &adapter) {
    if (m_State != LifecycleState::New) {
        RecordTerminal(Fault(LifecycleError::InvalidState,
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
        RecordTerminal(Fault(LifecycleError::Cancelled,
                             "Behavior lifecycle was closed during configuration."));
        Drain(adapter);
        return false;
    }

    m_State = LifecycleState::Ready;
    return true;
}

bool Lifecycle::RefreshAfterCallback(LifecycleAdapter &adapter,
                                     const LifecycleIdentity &identity,
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
    if (CloseRequested()) {
        fault = Fault(LifecycleError::Cancelled,
                      "Behavior lifecycle was closed by a native callback.");
        return false;
    }
    return true;
}

bool Lifecycle::InvokeConfigured(LifecycleAdapter &adapter,
                                 LifecycleCallback callback,
                                 const LifecycleIdentity &identity,
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
    RecordTerminal(std::move(fault));
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
        RecordTerminal(std::move(fault));
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
        RecordTerminal(std::move(fault));
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
        RecordTerminal(std::move(fault));
        return false;
    }

    if (identity && !adapter.Revalidate(*identity, fault)) {
        SupplyFault(fault, LifecycleError::IdentityChanged,
                    "A native teardown callback changed Behavior identity.");
        RecordTerminal(std::move(fault));
        return false;
    }
    return true;
}

void Lifecycle::RecordTerminal(LifecycleFault fault) noexcept {
    if (!m_TerminalError && fault)
        m_TerminalError = std::move(fault);
}

} // namespace BML::Behavior

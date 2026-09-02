#ifndef BML_BEHAVIOR_LIFECYCLE_H
#define BML_BEHAVIOR_LIFECYCLE_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace BML::Behavior {

enum class LifecycleState {
    New,
    Configuring,
    Ready,
    Closing,
    Closed,
};

enum class LifecycleError {
    None,
    InvalidState,
    InitializationFailed,
    SettingFailed,
    RelationFailed,
    CallbackFailed,
    IdentityChanged,
    LayoutFailed,
    BindingFailed,
    TeardownFailed,
    Cancelled,
};

struct LifecycleFault {
    LifecycleError Code = LifecycleError::None;
    int NativeCode = 0;
    std::string Message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return Code != LifecycleError::None;
    }
};

enum class LifecycleCallback {
    Create,
    Attach,
    SettingsEdited,
    Edited,
    Reset,
    Detach,
    Delete,
};

struct LifecycleLayout {
    std::uint64_t Generation = 0;
};

struct LifecycleObject {
    std::uint64_t Id = 0;
    std::uintptr_t Address = 0;

    [[nodiscard]] bool operator==(const LifecycleObject &other) const noexcept {
        return Id == other.Id && Address == other.Address;
    }
};

struct LifecycleIdentity {
    LifecycleObject Behavior;
    LifecycleObject Prototype;
    LifecycleObject Owner;
    LifecycleObject Parent;
    LifecycleObject Target;
    std::vector<LifecycleObject> Sources;

    [[nodiscard]] bool operator==(const LifecycleIdentity &other) const noexcept {
        return Behavior == other.Behavior && Prototype == other.Prototype &&
               Owner == other.Owner && Parent == other.Parent &&
               Target == other.Target && Sources == other.Sources;
    }
};

struct LifecyclePlan {
    bool HasOwner = false;
    // An entry records whether that stage contains at least one Setting. Stage
    // zero is always present conceptually, even when this vector is empty.
    std::vector<bool> SettingStages;
};

struct LifecycleLedger {
    bool Placed = false;
    bool Created = false;
    bool Attached = false;
};

class LifecycleAdapter {
public:
    virtual ~LifecycleAdapter() = default;

    virtual bool InitializeAndReflect(LifecycleLayout &layout,
                                      LifecycleFault &fault) = 0;
    virtual bool WriteSettingStage(std::size_t stage,
                                   const LifecycleLayout &layout,
                                   LifecycleFault &fault) = 0;
    virtual bool AlignRelationsAndPlace(LifecycleFault &fault) = 0;
    virtual bool CaptureIdentity(LifecycleIdentity &identity,
                                 LifecycleFault &fault) = 0;
    virtual bool Invoke(LifecycleCallback callback, LifecycleFault &fault) = 0;
    virtual bool Revalidate(const LifecycleIdentity &identity,
                            LifecycleFault &fault) = 0;
    virtual bool Reflect(LifecycleLayout &layout, LifecycleFault &fault) = 0;
    virtual bool ApplyBindings(LifecycleFault &fault) = 0;
    virtual bool ReconcileBindings(const LifecycleLayout &layout,
                                   LifecycleFault &fault) = 0;

    virtual bool Deactivate(LifecycleFault &fault) = 0;
    virtual bool DisconnectAndDestroy(LifecycleFault &fault) = 0;
};

class Lifecycle final {
public:
    Lifecycle() = default;
    Lifecycle(const Lifecycle &) = delete;
    Lifecycle &operator=(const Lifecycle &) = delete;
    Lifecycle(Lifecycle &&other) noexcept;
    Lifecycle &operator=(Lifecycle &&other) noexcept;

    bool Configure(const LifecyclePlan &plan, LifecycleAdapter &adapter);

    // RequestClose only closes admission. Native callbacks and graph mutation
    // are performed by Drain at a game-thread safe point.
    void RequestClose(bool reset = false) noexcept;
    bool Drain(LifecycleAdapter &adapter);

    [[nodiscard]] LifecycleState State() const noexcept { return m_State; }
    [[nodiscard]] const LifecycleLedger &Ledger() const noexcept {
        return m_Ledger;
    }
    [[nodiscard]] const LifecycleFault &Failure() const noexcept {
        return m_Failure;
    }
    [[nodiscard]] bool CloseRequested() const noexcept {
        return m_CloseRequested.load(std::memory_order_acquire);
    }

private:
    bool RefreshAfterCallback(LifecycleAdapter &adapter,
                              LifecycleIdentity &identity,
                              LifecycleLayout &layout,
                              LifecycleFault &fault);
    bool InvokeConfigured(LifecycleAdapter &adapter,
                          LifecycleCallback callback,
                          LifecycleIdentity &identity,
                          LifecycleLayout &layout,
                          LifecycleFault &fault,
                          bool *completed = nullptr);
    bool FailConfiguration(LifecycleFault fault, LifecycleAdapter &adapter);
    void RecordFailure(LifecycleFault fault) noexcept;
    bool TeardownCallback(LifecycleAdapter &adapter,
                          LifecycleCallback callback,
                          const LifecycleIdentity *identity);

    LifecycleState m_State = LifecycleState::New;
    LifecycleLedger m_Ledger;
    LifecycleFault m_Failure;
    LifecycleIdentity m_Identity;
    bool m_HasIdentity = false;
    std::atomic<bool> m_CloseRequested{false};
    std::atomic<bool> m_ResetRequested{false};
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_LIFECYCLE_H

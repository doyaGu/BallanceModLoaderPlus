#ifndef BML_BEHAVIOR_CALLBACK_H
#define BML_BEHAVIOR_CALLBACK_H

#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace BML::Behavior {

enum class CallbackLeaseState {
    Open,
    Closing,
    Closed,
};

enum class CallbackCloseResult {
    Ready,
    Queued,
    Closed,
};

enum class CallbackError {
    None,
    AdmissionClosed,
    Exception,
};

struct CallbackFault {
    CallbackError Code = CallbackError::None;
    std::string Message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return Code != CallbackError::None;
    }
};

struct CallbackCall {
    bool Invoked = false;
    int ReturnCode = 0;
    CallbackFault Fault;
};

class CallbackResource {
public:
    virtual ~CallbackResource() = default;
    virtual void CloseAdmission() noexcept = 0;
    [[nodiscard]] virtual bool RetireAtSafePoint() noexcept = 0;
};

class CallbackLease;
class CallbackInvocation;

class PlanCallbackState final {
public:
    using Reference = void (*)(void *);

    PlanCallbackState() = default;

    static PlanCallbackState Retained(void *state, Reference retain,
                                      Reference release);
    // Keeps owner alive for as long as any copy of this state, any lease, or
    // any resource built from it can still read the state pointer. Release is
    // still invoked only by Collect, and only when a lease was opened.
    static PlanCallbackState Retained(std::shared_ptr<void> owner, void *state,
                                      Reference retain, Reference release);
    static PlanCallbackState Static(void *state = nullptr);

    CallbackLease OpenLease() const;
    void Retire() const noexcept;

    // Collect is the only operation that invokes Release. The owner calls it
    // at a game-thread safe point after closing every lease.
    [[nodiscard]] bool Collect() const;
    [[nodiscard]] bool Retired() const noexcept;
    [[nodiscard]] void *State() const noexcept;

private:
    struct Control;
    explicit PlanCallbackState(std::shared_ptr<Control> control)
        : m_Control(std::move(control)) {}

    std::shared_ptr<Control> m_Control;

    friend class CallbackLease;
    friend class CallbackInvocation;
};

class CallbackInvocation final {
public:
    struct LeaseControl;

    CallbackInvocation() = default;
    CallbackInvocation(const CallbackInvocation &) = delete;
    CallbackInvocation &operator=(const CallbackInvocation &) = delete;
    CallbackInvocation(CallbackInvocation &&other) noexcept;
    CallbackInvocation &operator=(CallbackInvocation &&other) noexcept;
    ~CallbackInvocation();

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Lease != nullptr;
    }
    [[nodiscard]] bool IsCurrent() const noexcept;
    [[nodiscard]] static bool Active() noexcept;

private:
    explicit CallbackInvocation(std::shared_ptr<LeaseControl> lease);
    void Leave() noexcept;

    std::shared_ptr<LeaseControl> m_Lease;

    friend class CallbackLease;
};

class CallbackLease final {
public:
    CallbackLease() = default;
    CallbackLease(const CallbackLease &) = delete;
    CallbackLease &operator=(const CallbackLease &) = delete;
    CallbackLease(CallbackLease &&other) noexcept = default;
    CallbackLease &operator=(CallbackLease &&other) noexcept = default;
    ~CallbackLease();

    [[nodiscard]] CallbackInvocation Enter() const;
    CallbackCloseResult CloseAdmission() noexcept;
    CallbackCloseResult Close() noexcept;
    [[nodiscard]] CallbackLeaseState State() const noexcept;
    [[nodiscard]] bool IsCurrentInvocation() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Lease != nullptr;
    }

private:
    using LeaseControl = CallbackInvocation::LeaseControl;
    explicit CallbackLease(std::shared_ptr<LeaseControl> lease)
        : m_Lease(std::move(lease)) {}

    std::shared_ptr<LeaseControl> m_Lease;

    friend class PlanCallbackState;
};

template <typename Callable>
CallbackCall InvokeCallback(CallbackLease &lease, int exceptionReturnCode,
                            Callable &&callable) noexcept {
    CallbackInvocation invocation = lease.Enter();
    if (!invocation) {
        return {false, exceptionReturnCode,
                {CallbackError::AdmissionClosed,
                 "Behavior callback admission is closed."}};
    }
    try {
        return {true, std::invoke(std::forward<Callable>(callable)), {}};
    } catch (const std::exception &exception) {
        return {true, exceptionReturnCode,
                {CallbackError::Exception, exception.what()}};
    } catch (...) {
        return {true, exceptionReturnCode,
                {CallbackError::Exception,
                 "Behavior callback threw an unknown exception."}};
    }
}

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_CALLBACK_H

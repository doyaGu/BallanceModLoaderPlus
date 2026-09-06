#ifndef BML_BEHAVIOR_HOOKBLOCK_H
#define BML_BEHAVIOR_HOOKBLOCK_H

#include "Behavior/Runtime.h"

namespace BML::Behavior::Internal::HookBlock {

using Callback = int (*)(const CKBehaviorContext *context, void *argument);

// Native return code the C seam hands back when an author callback reported
// BML_BEHAVIOR_HOOK_FAULT. Negative so it can never collide with a CKBR_* code.
constexpr int CallbackFaulted = -2;

class Hook;

class Binding final : public CallbackResource {
public:
    Binding(PlanCallbackState state, Callback callback, void *argument);
    ~Binding() override;

    [[nodiscard]] CallbackCall Invoke(const CKBehaviorContext *context) noexcept;
    void CloseAdmission() noexcept override;
    [[nodiscard]] bool RetireAtSafePoint() noexcept override;
    [[nodiscard]] CallbackLeaseState State() const noexcept;
    [[nodiscard]] CallbackFault Diagnostic() const;
    [[nodiscard]] void *Argument() const noexcept { return m_Argument; }

private:
    Binding(PlanCallbackState state, Callback callback, void *argument,
            bool ownsState);

    PlanCallbackState m_State;
    CallbackLease m_Lease;
    Callback m_Callback = nullptr;
    void *m_Argument = nullptr;
    bool m_OwnsState = true;
    mutable std::mutex m_DiagnosticMutex;
    CallbackFault m_Diagnostic;

    friend class Hook;
};

// One author callback occurrence in a durable graph edit. Copies share the
// occurrence, while every live installation receives its own Binding lease.
class Hook final {
public:
    Hook() = default;
    Hook(Callback callback, void *argument = nullptr)
        : Hook(PlanCallbackState::Static(argument), callback, argument) {}
    Hook(PlanCallbackState state, Callback callback,
         void *argument = nullptr)
        : m_Occurrence(callback
              ? std::make_shared<Occurrence>(
                    std::move(state), callback, argument)
              : nullptr) {}

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Occurrence != nullptr;
    }
    [[nodiscard]] std::shared_ptr<Binding> Bind() const;

private:
    struct Occurrence {
        Occurrence(PlanCallbackState state, Callback callback,
                   void *argument)
            : State(std::move(state)), Function(callback),
              Argument(argument) {}
        ~Occurrence() {
            State.Retire();
            (void) State.Collect();
        }

        PlanCallbackState State;
        Callback Function = nullptr;
        void *Argument = nullptr;
    };

    std::shared_ptr<Occurrence> m_Occurrence;
};

std::shared_ptr<Binding> Bind(Callback callback, void *argument = nullptr);
std::shared_ptr<Binding> Bind(PlanCallbackState state, Callback callback,
                              void *argument = nullptr);

BlockSpec Make(std::shared_ptr<Binding> binding,
          int inputCount = 1, int outputCount = 1);

BlockSpec Make(Callback callback, void *argument = nullptr,
          int inputCount = 1, int outputCount = 1);

void Register(XObjectDeclarationArray *registry);

} // namespace BML::Behavior::Internal::HookBlock

#endif // BML_BEHAVIOR_HOOKBLOCK_H

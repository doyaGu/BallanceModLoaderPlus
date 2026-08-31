#ifndef BML_BEHAVIOR_HOOKBLOCK_H
#define BML_BEHAVIOR_HOOKBLOCK_H

#include "Behavior/Runtime.h"

namespace BML::Behavior::HookBlock {

using Callback = int (*)(const CKBehaviorContext *context, void *argument);

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
    PlanCallbackState m_State;
    CallbackLease m_Lease;
    Callback m_Callback = nullptr;
    void *m_Argument = nullptr;
    mutable std::mutex m_DiagnosticMutex;
    CallbackFault m_Diagnostic;
};

std::shared_ptr<Binding> Bind(Callback callback, void *argument = nullptr);
std::shared_ptr<Binding> Bind(PlanCallbackState state, Callback callback,
                              void *argument = nullptr);

Spec Make(std::shared_ptr<Binding> binding,
          int inputCount = 1, int outputCount = 1);

Spec Make(Callback callback, void *argument = nullptr,
          int inputCount = 1, int outputCount = 1);

void Register(XObjectDeclarationArray *registry);

} // namespace BML::Behavior::HookBlock

#endif // BML_BEHAVIOR_HOOKBLOCK_H

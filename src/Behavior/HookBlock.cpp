#include "Behavior/HookBlock.h"

#include "BML/Guids/Hooks.h"

namespace BML::Behavior::HookBlock {
namespace {

int Run(const CKBehaviorContext &context) {
    CKBehavior *behavior = context.Behavior;
    for (int i = 0; i < behavior->GetInputCount(); ++i)
        behavior->ActivateInput(i, FALSE);

    Binding *binding = nullptr;
    behavior->GetLocalParameterValue(0, &binding);
    CallbackCall call;
    if (binding)
        call = binding->Invoke(&context);
    if (!call.Invoked)
        return CKBR_OK;
    if (call.Fault)
        return call.ReturnCode;

    CKBOOL autoActivateOutputs = TRUE;
    if (behavior->GetLocalParameterCount() > 2)
        behavior->GetLocalParameterValue(2, &autoActivateOutputs);
    if (autoActivateOutputs) {
        for (int i = 0; i < behavior->GetOutputCount(); ++i)
            behavior->ActivateOutput(i);
    }
    return call.ReturnCode;
}

CKERROR CreatePrototype(CKBehaviorPrototype **prototype) {
    CKBehaviorPrototype *created = CreateCKBehaviorPrototype("HookBlock");
    if (!created)
        return CKERR_OUTOFMEMORY;

    created->DeclareLocalParameter("Callback Binding", CKPGUID_POINTER);
    created->DeclareLocalParameter("Argument", CKPGUID_POINTER);
    created->DeclareLocalParameter("Auto Activate Outputs", CKPGUID_BOOL);
    created->SetBehaviorFlags(static_cast<CK_BEHAVIOR_FLAGS>(
        CKBEHAVIOR_VARIABLEINPUTS | CKBEHAVIOR_VARIABLEOUTPUTS));
    created->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    created->SetFunction(Run);
    *prototype = created;
    return CK_OK;
}

CKObjectDeclaration *Declaration() {
    CKObjectDeclaration *declaration = CreateCKObjectDeclaration("HookBlock");
    declaration->SetDescription("Hook building blocks");
    declaration->SetCategory("Hook");
    declaration->SetType(CKDLL_BEHAVIORPROTOTYPE);
    declaration->SetGuid(HOOKS_HOOKBLOCK_GUID);
    declaration->SetAuthorGuid(CKGUID(0x3a086b4d, 0x2f4a4f01));
    declaration->SetAuthorName("Kakuty");
    declaration->SetVersion(0x00010000);
    declaration->SetCreationFunction(CreatePrototype);
    declaration->SetCompatibleClassId(CKCID_BEOBJECT);
    return declaration;
}

} // namespace

Binding::Binding(PlanCallbackState state, Callback callback, void *argument)
    : m_State(std::move(state)), m_Lease(m_State.OpenLease()),
      m_Callback(callback), m_Argument(argument) {}

Binding::~Binding() {
    CloseAdmission();
    (void) RetireAtSafePoint();
}

CallbackCall Binding::Invoke(const CKBehaviorContext *context) noexcept {
    if (!m_Callback)
        return {false, CKBR_OK, {}};
    CallbackCall call = InvokeCallback(
        m_Lease, CKBR_BEHAVIORERROR,
        [&] { return m_Callback(context, m_Argument); });
    if (call.Fault) {
        std::lock_guard<std::mutex> lock(m_DiagnosticMutex);
        if (!m_Diagnostic)
            m_Diagnostic = call.Fault;
    }
    return call;
}

void Binding::CloseAdmission() noexcept {
    (void) m_Lease.Close();
}

bool Binding::RetireAtSafePoint() noexcept {
    CloseAdmission();
    m_State.Retire();
    return m_State.Collect();
}

CallbackLeaseState Binding::State() const noexcept {
    return m_Lease.State();
}

CallbackFault Binding::Diagnostic() const {
    std::lock_guard<std::mutex> lock(m_DiagnosticMutex);
    return m_Diagnostic;
}

std::shared_ptr<Binding> Bind(Callback callback, void *argument) {
    return Bind(PlanCallbackState::Static(argument), callback, argument);
}

std::shared_ptr<Binding> Bind(PlanCallbackState state, Callback callback,
                              void *argument) {
    if (!callback)
        return {};
    return std::make_shared<Binding>(std::move(state), callback, argument);
}

Spec Make(std::shared_ptr<Binding> binding, int inputCount, int outputCount) {
    Spec spec(HOOKS_HOOKBLOCK_GUID);
    if (!binding || inputCount < 0 || outputCount < 0)
        return Spec();
    CKBOOL autoActivate = TRUE;
    Binding *nativeBinding = binding.get();
    void *argument = binding->Argument();
    spec.Local(Slot::At(SlotKind::Local, 0, CKPGUID_POINTER),
               Value::From(CKPGUID_POINTER, nativeBinding))
        .Local(Slot::At(SlotKind::Local, 1, CKPGUID_POINTER),
               Value::From(CKPGUID_POINTER, argument))
        .Local(Slot::At(SlotKind::Local, 2, CKPGUID_BOOL),
               Value::From(CKPGUID_BOOL, autoActivate))
        .KeepAlive(std::move(binding));
    for (int i = 0; i < inputCount; ++i)
        spec.AddInput("In " + std::to_string(i));
    for (int i = 0; i < outputCount; ++i)
        spec.AddOutput("Out " + std::to_string(i));
    return spec;
}

Spec Make(Callback callback, void *argument, int inputCount, int outputCount) {
    return Make(Bind(callback, argument), inputCount, outputCount);
}

void Register(XObjectDeclarationArray *registry) {
    CKStoreDeclaration(registry, Declaration());
}

} // namespace BML::Behavior::HookBlock

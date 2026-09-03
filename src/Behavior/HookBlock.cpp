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
    // A Block whose author callback did not run stays transparent. The Hook is
    // spliced into a chain the host script owns, so swallowing the activation
    // would stop that script while the Patch is merely closing or Conflicted.
    // A callback that faulted is treated the same way: the Binding has kept
    // its diagnostic and closed further admission, and the Block passes this
    // activation through. Only a callback that completed and reported a CKBR
    // error code stops the chain. CK2 discards a sub-behavior's return code
    // (CKBehavior::Execute), so leaving every Out inactive is the only way to
    // stop it.
    const bool completed = call.Invoked && !call.Fault;
    const bool reportedError = completed &&
        (call.ReturnCode & CKBR_GENERICERROR) == CKBR_GENERICERROR;
    if (reportedError)
        return call.ReturnCode;

    CKBOOL autoActivateOutputs = TRUE;
    if (behavior->GetLocalParameterCount() > 2)
        behavior->GetLocalParameterValue(2, &autoActivateOutputs);
    if (autoActivateOutputs) {
        for (int i = 0; i < behavior->GetOutputCount(); ++i)
            behavior->ActivateOutput(i);
    }
    return completed ? call.ReturnCode : CKBR_OK;
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

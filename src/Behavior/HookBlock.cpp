#include "Behavior/HookBlock.h"

#include "BML/Guids/Hooks.h"

namespace BML::Behavior::HookBlock {
namespace {

int Run(const CKBehaviorContext &context) {
    CKBehavior *behavior = context.Behavior;
    for (int i = 0; i < behavior->GetInputCount(); ++i)
        behavior->ActivateInput(i, FALSE);

    int result = CKBR_OK;
    Callback callback = nullptr;
    behavior->GetLocalParameterValue(0, &callback);
    if (callback) {
        void *argument = nullptr;
        behavior->GetLocalParameterValue(1, &argument);
        result = callback(&context, argument);
    }

    CKBOOL autoActivateOutputs = TRUE;
    if (behavior->GetLocalParameterCount() > 2)
        behavior->GetLocalParameterValue(2, &autoActivateOutputs);
    if (autoActivateOutputs) {
        for (int i = 0; i < behavior->GetOutputCount(); ++i)
            behavior->ActivateOutput(i);
    }
    return result;
}

CKERROR CreatePrototype(CKBehaviorPrototype **prototype) {
    CKBehaviorPrototype *created = CreateCKBehaviorPrototype("HookBlock");
    if (!created)
        return CKERR_OUTOFMEMORY;

    created->DeclareLocalParameter("Callback", CKPGUID_POINTER);
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

Spec Make(Callback callback, void *argument, int inputCount, int outputCount) {
    Spec spec(HOOKS_HOOKBLOCK_GUID);
    if (!callback || inputCount < 0 || outputCount < 0)
        return Spec();
    CKBOOL autoActivate = TRUE;
    spec.Local(Slot::At(SlotKind::Local, 0, CKPGUID_POINTER),
               Value::From(CKPGUID_POINTER, callback))
        .Local(Slot::At(SlotKind::Local, 1, CKPGUID_POINTER),
               Value::From(CKPGUID_POINTER, argument))
        .Local(Slot::At(SlotKind::Local, 2, CKPGUID_BOOL),
               Value::From(CKPGUID_BOOL, autoActivate));
    for (int i = 0; i < inputCount; ++i)
        spec.AddInput("In " + std::to_string(i));
    for (int i = 0; i < outputCount; ++i)
        spec.AddOutput("Out " + std::to_string(i));
    return spec;
}

void Register(XObjectDeclarationArray *registry) {
    CKStoreDeclaration(registry, Declaration());
}

} // namespace BML::Behavior::HookBlock

/**
 * @file HookBlock.cpp
 * @brief Building block hooking behavior for Virtools
 * 
 * This behavior allows intercepting and extending building block execution
 * through callback registration. Used by ModLoader to provide mod callbacks
 * at specific points in the game's behavior graph execution.
 */

#include "CKAll.h"

typedef int (*CKBehaviorCallback)(const CKBehaviorContext *behcontext, void *arg);

// Forward declarations
CKObjectDeclaration *FillBehaviorHookBlockDecl();
CKERROR CreateHookBlockProto(CKBehaviorPrototype **pproto);
int HookBlock(const CKBehaviorContext &behcontext);

/**
 * @brief Creates the HookBlock object declaration
 * 
 * Registers the HookBlock building block with the Virtools engine.
 * This block is used to intercept behavior graph execution.
 */
CKObjectDeclaration *FillBehaviorHookBlockDecl() {
    CKObjectDeclaration *od = CreateCKObjectDeclaration("HookBlock");
    od->SetDescription("Hook building blocks");
    od->SetCategory("Hook");
    od->SetType(CKDLL_BEHAVIORPROTOTYPE);
    od->SetGuid(CKGUID(0x19038c0, 0x663902da));
    od->SetAuthorGuid(CKGUID(0x3a086b4d, 0x2f4a4f01));
    od->SetAuthorName("Kakuty");
    od->SetVersion(0x00010000);
    od->SetCreationFunction(CreateHookBlockProto);
    od->SetCompatibleClassId(CKCID_BEOBJECT);
    return od;
}

/**
 * @brief Creates the HookBlock behavior prototype
 * 
 * Defines the behavior's parameters and settings:
 * - Local parameter 0: Callback function pointer
 * - Local parameter 1: User argument pointer
 * - Variable inputs/outputs for flexible hooking
 */
CKERROR CreateHookBlockProto(CKBehaviorPrototype **pproto) {
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("HookBlock");
    if (!proto) return CKERR_OUTOFMEMORY;

    proto->DeclareLocalParameter("Callback", CKPGUID_POINTER);
    proto->DeclareLocalParameter("Argument", CKPGUID_POINTER);

    proto->SetBehaviorFlags((CK_BEHAVIOR_FLAGS)(CKBEHAVIOR_VARIABLEINPUTS | CKBEHAVIOR_VARIABLEOUTPUTS));
    proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    proto->SetFunction(HookBlock);

    *pproto = proto;
    return CK_OK;
}

/**
 * @brief HookBlock behavior execution function
 * 
 * Deactivates all inputs, invokes the registered callback (if any),
 * then activates all outputs. The callback can control the return value.
 * 
 * @param behcontext The behavior context containing execution state
 * @return CKBR_OK on success, or callback-defined return value
 */
int HookBlock(const CKBehaviorContext &behcontext) {
    CKBehavior *beh = behcontext.Behavior;

    // Deactivate all inputs
    int i, count = beh->GetInputCount();
    for (i = 0; i < count; ++i) {
        beh->ActivateInput(i, FALSE);
    }

    // Invoke registered callback
    int ret = CKBR_OK;
    CKBehaviorCallback cb = nullptr;
    beh->GetLocalParameterValue(0, &cb);
    if (cb) {
        void *arg = nullptr;
        beh->GetLocalParameterValue(1, &arg);
        ret = cb(&behcontext, arg);
    }

    // Activate all outputs
    count = beh->GetOutputCount();
    for (i = 0; i < count; ++i) {
        beh->ActivateOutput(i);
    }

    return ret;
}
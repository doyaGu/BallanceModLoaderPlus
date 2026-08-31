#include "BehaviorTransportFixtureApi.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "CKAll.h"

namespace {

const CKGUID kPluginGuid(0x31fb0228, 0x64d14aa7);
const CKGUID kAuthorGuid(0x3a086b4d, 0x2f4a4f01);

enum InputIndex {
    InputRun,
    InputMakeObject,
    InputDeleteObject,
};

enum OutputIndex {
    OutputDone,
    OutputDeleted,
};

enum PoutIndex {
    PoutInteger,
    PoutFloat,
    PoutVector,
    PoutText,
    PoutObject,
};

enum LocalIndex {
    SettingRetry,
    LocalExecutions,
    LocalObjectId,
};

int Run(const CKBehaviorContext &context) {
    CKBehavior *behavior = context.Behavior;
    if (!behavior || !context.Context)
        return CKBR_BEHAVIORERROR;

    int executions = 0;
    behavior->GetLocalParameterValue(LocalExecutions, &executions);
    ++executions;
    behavior->SetLocalParameterValue(LocalExecutions, &executions);

    if (behavior->IsInputActive(InputDeleteObject)) {
        CK_ID objectId = 0;
        behavior->GetLocalParameterValue(LocalObjectId, &objectId);
        CKObject *object = objectId ? context.Context->GetObject(objectId) : nullptr;
        if (object)
            context.Context->DestroyObject(object);
        objectId = 0;
        behavior->SetLocalParameterValue(LocalObjectId, &objectId);
        behavior->SetOutputParameterObject(PoutObject, nullptr);
        behavior->ActivateOutput(OutputDeleted);
        return CKBR_OK;
    }

    const int integer = 42;
    const float real = 1.5f;
    const VxVector vector(1.0f, 2.0f, 3.0f);
    behavior->SetOutputParameterValue(PoutInteger, &integer);
    behavior->SetOutputParameterValue(PoutFloat, &real);
    behavior->SetOutputParameterValue(PoutVector, &vector);
    CKParameterOut *text = behavior->GetOutputParameter(PoutText);
    if (!text || text->SetStringValue("transport") != CK_OK)
        return CKBR_BEHAVIORERROR;

    if (behavior->IsInputActive(InputMakeObject)) {
        CKObject *object = context.Context->CreateObject(
            CKCID_BEOBJECT, "__BML_BehaviorTransport_Object",
            CK_OBJECTCREATION_DYNAMIC);
        if (!object)
            return CKBR_BEHAVIORERROR;
        const CK_ID objectId = object->GetID();
        behavior->SetLocalParameterValue(LocalObjectId, &objectId);
        behavior->SetOutputParameterObject(PoutObject, object);
    } else {
        behavior->SetOutputParameterObject(PoutObject, nullptr);
    }

    CKBOOL retry = FALSE;
    behavior->GetLocalParameterValue(SettingRetry, &retry);
    if (retry && executions == 1)
        return CKBR_ACTIVATENEXTFRAME;

    behavior->ActivateOutput(OutputDone);
    return CKBR_OK;
}

CKERROR Lifecycle(const CKBehaviorContext &context) {
    if (!context.Behavior || !context.Context)
        return CKERR_INVALIDPARAMETER;
    if (context.CallbackMessage != CKM_BEHAVIORDELETE)
        return CK_OK;
    CK_ID objectId = 0;
    context.Behavior->GetLocalParameterValue(LocalObjectId, &objectId);
    CKObject *object = objectId ? context.Context->GetObject(objectId) : nullptr;
    if (object)
        context.Context->DestroyObject(object);
    return CK_OK;
}

CKERROR CreatePrototype(CKBehaviorPrototype **prototype) {
    if (!prototype)
        return CKERR_INVALIDPARAMETER;
    CKBehaviorPrototype *created =
        CreateCKBehaviorPrototype("BML Behavior Transport Fixture");
    if (!created)
        return CKERR_OUTOFMEMORY;
    created->DeclareInput("Run");
    created->DeclareInput("Make Object");
    created->DeclareInput("Delete Object");
    created->DeclareOutput("Done");
    created->DeclareOutput("Deleted");
    created->DeclareOutParameter("Number", CKPGUID_INT, "0");
    created->DeclareOutParameter("Number", CKPGUID_FLOAT, "0");
    created->DeclareOutParameter("Vector", CKPGUID_VECTOR, "0,0,0");
    created->DeclareOutParameter("Text", CKPGUID_STRING, "");
    created->DeclareOutParameter("Object", CKPGUID_BEOBJECT);
    created->DeclareSetting("Retry", CKPGUID_BOOL, "FALSE");
    created->DeclareLocalParameter("Executions", CKPGUID_INT, "0");
    created->DeclareLocalParameter("Object ID", CKPGUID_INT, "0");
    created->SetFunction(Run);
    created->SetBehaviorCallbackFct(Lifecycle, CKCB_BEHAVIORDELETE);
    created->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    *prototype = created;
    return CK_OK;
}

CKObjectDeclaration *Declaration() {
    CKObjectDeclaration *declaration =
        CreateCKObjectDeclaration("BML Behavior Transport Fixture");
    declaration->SetDescription("BML test-only Behavior outcome fixture");
    declaration->SetCategory("BML/Test");
    declaration->SetType(CKDLL_BEHAVIORPROTOTYPE);
    declaration->SetGuid(BML_BEHAVIOR_TRANSPORT_FIXTURE_GUID);
    declaration->SetAuthorGuid(kAuthorGuid);
    declaration->SetAuthorName("BML+");
    declaration->SetVersion(0x00010000);
    declaration->SetCreationFunction(CreatePrototype);
    declaration->SetCompatibleClassId(CKCID_BEOBJECT);
    return declaration;
}

CKPluginInfo g_PluginInfo;

} // namespace

PLUGIN_EXPORT int CKGetPluginInfoCount() {
    return 1;
}

PLUGIN_EXPORT CKPluginInfo *CKGetPluginInfo(int) {
    g_PluginInfo.m_Author = "BML+";
    g_PluginInfo.m_Description = "BML Behavior transport test fixture";
    g_PluginInfo.m_Extension = "";
    g_PluginInfo.m_Type = CKPLUGIN_BEHAVIOR_DLL;
    g_PluginInfo.m_Version = 0x000001;
    g_PluginInfo.m_InitInstanceFct = nullptr;
    g_PluginInfo.m_ExitInstanceFct = nullptr;
    g_PluginInfo.m_GUID = kPluginGuid;
    g_PluginInfo.m_Summary = "Behavior outcome transport fixture";
    return &g_PluginInfo;
}

PLUGIN_EXPORT void RegisterBehaviorDeclarations(XObjectDeclarationArray *registry) {
    if (registry)
        CKStoreDeclaration(registry, Declaration());
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) {
    return TRUE;
}

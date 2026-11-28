#include "CKAll.h"

namespace {
CKPluginInfo g_Info;

CKERROR CreatePrototype(CKBehaviorPrototype **prototype) {
    auto *created = CreateCKBehaviorPrototype("BML Win32 prototype fixture");
    if (!created)
        return CKERR_OUTOFMEMORY;
    created->DeclareInput("In");
    created->DeclareOutput("Out");
    created->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    created->SetFunction([](const CKBehaviorContext &context) -> int {
        context.Behavior->ActivateOutput(0);
        return CKBR_OK;
    });
    *prototype = created;
    return CK_OK;
}
}

PLUGIN_EXPORT int CKGetPluginInfoCount() {
    return 1;
}

PLUGIN_EXPORT CKPluginInfo *CKGetPluginInfo(int) {
    g_Info.m_Author = "BML+";
    g_Info.m_Description = "BML Win32 prototype retirement fixture";
    g_Info.m_Extension = "";
    g_Info.m_Type = CKPLUGIN_BEHAVIOR_DLL;
    g_Info.m_Version = 1;
    g_Info.m_GUID = CKGUID(0x63e4f18a, 0x16bfc0e3);
    g_Info.m_Summary = "BML Win32 prototype retirement fixture";
    return &g_Info;
}

PLUGIN_EXPORT void RegisterBehaviorDeclarations(XObjectDeclarationArray *registry) {
    auto *declaration = CreateCKObjectDeclaration("BML Win32 prototype fixture");
    if (!declaration)
        return;
    declaration->SetType(CKDLL_BEHAVIORPROTOTYPE);
    declaration->SetGuid(CKGUID(0x63e4f18a, 0x16bfc0e4));
    declaration->SetCategory("BML/Test");
    declaration->SetVersion(1);
    declaration->SetCompatibleClassId(CKCID_BEOBJECT);
    declaration->SetCreationFunction(CreatePrototype);
    CKStoreDeclaration(registry, declaration);
}

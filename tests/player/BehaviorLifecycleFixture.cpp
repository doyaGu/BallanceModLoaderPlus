#include "BehaviorLifecycleFixtureApi.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "CKAll.h"

#include <algorithm>
#include <cstring>
#include <iterator>

namespace {

const CKGUID kPluginGuid(0x6f736d3a, 0x741ec900);
const CKGUID kAuthorGuid(0x3a086b4d, 0x2f4a4f01);

BMLLifecycleFixtureTrace g_Trace;
BMLLifecycleFixtureMode g_Mode = BMLLifecycleFixtureMode::Normal;
BMLLifecycleFixtureCloseHook g_CloseHook = nullptr;
void *g_CloseArgument = nullptr;

bool HasConnectedLink(CKBehavior *parent, CKBehavior *behavior) {
    if (!parent || !behavior)
        return false;
    for (int index = 0; index < parent->GetSubBehaviorLinkCount(); ++index) {
        CKBehaviorLink *link = parent->GetSubBehaviorLink(index);
        CKBehaviorIO *source = link ? link->GetInBehaviorIO() : nullptr;
        CKBehaviorIO *destination = link ? link->GetOutBehaviorIO() : nullptr;
        if ((source && source->GetOwner() == behavior) ||
            (destination && destination->GetOwner() == behavior)) {
            return true;
        }
    }
    return false;
}

void Count(CKDWORD message) {
    switch (message) {
    case CKM_BEHAVIORCREATE: ++g_Trace.CreateCount; break;
    case CKM_BEHAVIORATTACH: ++g_Trace.AttachCount; break;
    case CKM_BEHAVIORSETTINGSEDITED: ++g_Trace.SettingsEditedCount; break;
    case CKM_BEHAVIOREDITED: ++g_Trace.EditedCount; break;
    case CKM_BEHAVIORRESET: ++g_Trace.ResetCount; break;
    case CKM_BEHAVIORDETACH: ++g_Trace.DetachCount; break;
    case CKM_BEHAVIORDELETE: ++g_Trace.DeleteCount; break;
    default: break;
    }
}

CKERROR LifecycleCallback(const CKBehaviorContext &context) {
    CKBehavior *behavior = context.Behavior;
    if (!behavior)
        return CKERR_INVALIDPARAMETER;

    int setting = 0;
    if (behavior->GetLocalParameterCount() > 0)
        behavior->GetLocalParameterValue(0, &setting);

    Count(context.CallbackMessage);
    if (g_Trace.EventCount < std::size(g_Trace.Events)) {
        BMLLifecycleFixtureEvent &event =
            g_Trace.Events[g_Trace.EventCount++];
        event.Message = context.CallbackMessage;
        event.BehaviorId = static_cast<std::uint32_t>(behavior->GetID());
        event.SettingValue = setting;
        event.OwnerVisible = behavior->GetOwner() ? 1u : 0u;
        event.ParentVisible = behavior->GetParent() ? 1u : 0u;
        event.LinkVisible = HasConnectedLink(behavior->GetParent(), behavior)
            ? 1u : 0u;
        CKParameterIn *input = behavior->GetInputParameterCount() > 0
            ? behavior->GetInputParameter(0) : nullptr;
        event.SourceVisible = input && input->GetRealSource() ? 1u : 0u;
    }

    if (context.CallbackMessage == CKM_BEHAVIORCREATE ||
        context.CallbackMessage == CKM_BEHAVIORATTACH) {
        const int nativeDefault = 15;
        behavior->SetLocalParameterValue(0, &nativeDefault);
    } else if (context.CallbackMessage == CKM_BEHAVIORSETTINGSEDITED) {
        g_Trace.SettingsEditedObserved = setting;
        const int normalized = 77;
        behavior->SetLocalParameterValue(0, &normalized);
        g_Trace.FinalNormalizedValue = normalized;
    } else if (context.CallbackMessage == CKM_BEHAVIOREDITED &&
               g_Mode == BMLLifecycleFixtureMode::CloseOnEdited &&
               g_CloseHook) {
        ++g_Trace.CloseHookCalls;
        if (g_CloseHook(behavior, g_CloseArgument) != 0)
            ++g_Trace.CloseHookAccepted;
    }
    return CK_OK;
}

int Run(const CKBehaviorContext &context) {
    if (context.Behavior && context.Behavior->GetOutputCount() > 0)
        context.Behavior->ActivateOutput(0);
    return CKBR_OK;
}

CKERROR CreatePrototype(CKBehaviorPrototype **prototype) {
    if (!prototype)
        return CKERR_INVALIDPARAMETER;
    CKBehaviorPrototype *created =
        CreateCKBehaviorPrototype("BML Lifecycle Fixture");
    if (!created)
        return CKERR_OUTOFMEMORY;
    created->DeclareInput("In");
    created->DeclareOutput("Out");
    created->DeclareSetting("Value", CKPGUID_INT, "15");
    created->DeclareInParameter("Source", CKPGUID_INT, "3");
    created->SetFunction(Run);
    created->SetBehaviorCallbackFct(
        LifecycleCallback,
        CKCB_BEHAVIORCREATE | CKCB_BEHAVIORATTACH |
        CKCB_BEHAVIORSETTINGSEDITED | CKCB_BEHAVIOREDITED |
        CKCB_BEHAVIORRESET | CKCB_BEHAVIORDETACH |
        CKCB_BEHAVIORDELETE);
    created->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    *prototype = created;
    return CK_OK;
}

CKObjectDeclaration *Declaration() {
    CKObjectDeclaration *declaration =
        CreateCKObjectDeclaration("BML Lifecycle Fixture");
    declaration->SetDescription("BML test-only native lifecycle fixture");
    declaration->SetCategory("BML/Test");
    declaration->SetType(CKDLL_BEHAVIORPROTOTYPE);
    declaration->SetGuid(BML_LIFECYCLE_FIXTURE_GUID);
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
    g_PluginInfo.m_Description = "BML Behavior lifecycle test fixture";
    g_PluginInfo.m_Extension = "";
    g_PluginInfo.m_Type = CKPLUGIN_BEHAVIOR_DLL;
    g_PluginInfo.m_Version = 0x000001;
    g_PluginInfo.m_InitInstanceFct = nullptr;
    g_PluginInfo.m_ExitInstanceFct = nullptr;
    g_PluginInfo.m_GUID = kPluginGuid;
    g_PluginInfo.m_Summary = "Behavior lifecycle fixture";
    return &g_PluginInfo;
}

PLUGIN_EXPORT void RegisterBehaviorDeclarations(XObjectDeclarationArray *registry) {
    if (registry)
        CKStoreDeclaration(registry, Declaration());
}

extern "C" __declspec(dllexport) void BMLLifecycleFixtureResetTrace() {
    g_Trace = {};
    g_Trace.Size = sizeof(g_Trace);
    g_Trace.Version = 1;
}

extern "C" __declspec(dllexport) void BMLLifecycleFixtureSetMode(
    BMLLifecycleFixtureMode mode) {
    g_Mode = mode;
}

extern "C" __declspec(dllexport) void BMLLifecycleFixtureSetCloseHook(
    BMLLifecycleFixtureCloseHook hook, void *argument) {
    g_CloseHook = hook;
    g_CloseArgument = argument;
}

extern "C" __declspec(dllexport) int BMLLifecycleFixtureReadTrace(
    BMLLifecycleFixtureTrace *trace) {
    if (!trace || trace->Size != sizeof(BMLLifecycleFixtureTrace))
        return 0;
    *trace = g_Trace;
    return 1;
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) {
    return TRUE;
}

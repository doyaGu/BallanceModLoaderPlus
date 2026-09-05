#include "BehaviorTransportFixtureApi.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "CKAll.h"

namespace {

const CKGUID kAuthorGuid(0x3a086b4d, 0x2f4a4f01);
BML_BehaviorTransportGraphTrace g_GraphTrace{};

CKERROR GraphStep(BML_BehaviorTransportGraphStage stage,
                  CKERROR error = CK_OK) {
    g_GraphTrace.Stage = stage;
    g_GraphTrace.Error = error;
    return error;
}

enum InputIndex {
    InputRun,
    InputMakeObject,
    InputDeleteObject,
    InputEcho,
    InputReadTarget,
    InputDuplicate0,
    InputDuplicate1,
    InputEchoNumber,
};

enum OutputIndex {
    OutputDone,
    OutputDeleted,
};

enum PoutIndex {
    PoutBool,
    PoutInteger,
    PoutFloat,
    PoutText,
    PoutVec2,
    PoutVector,
    PoutQuaternion,
    PoutEuler,
    PoutRect,
    PoutColor,
    PoutBox,
    PoutMatrix,
    PoutObject,
    PoutTarget,
};

enum PinIndex {
    PinBool,
    PinInteger,
    PinFloat,
    PinText,
    PinVec2,
    PinVector,
    PinQuaternion,
    PinEuler,
    PinRect,
    PinColor,
    PinBox,
    PinMatrix,
    PinObject,
};

enum LocalIndex {
    SettingRetry,
    SettingExtendedLayout,
    LocalExecutions,
    LocalObjectId,
};

bool CopyPin(CKBehavior *behavior, int pinIndex, int poutIndex) {
    CKParameterIn *pin = behavior ? behavior->GetInputParameter(pinIndex) : nullptr;
    CKParameter *source = pin ? pin->GetRealSource() : nullptr;
    CKParameterOut *pout = behavior ? behavior->GetOutputParameter(poutIndex) : nullptr;
    return source && pout && pout->CopyValue(source, TRUE) == CK_OK;
}

bool SetStaticValues(CKBehavior *behavior) {
    const CKBOOL boolean = TRUE;
    const int integer = 42;
    const float real = 1.5f;
    const Vx2DVector vec2(4.0f, 5.0f);
    const VxVector vec3(1.0f, 2.0f, 3.0f);
    const VxQuaternion quaternion(0.1f, 0.2f, 0.3f, 0.4f);
    const float euler[3] = {0.5f, 0.6f, 0.7f};
    const VxRect rect(1.0f, 2.0f, 3.0f, 4.0f);
    const VxColor color(0.2f, 0.4f, 0.6f, 0.8f);
    const VxBbox box(VxVector(-1.0f, -2.0f, -3.0f),
                     VxVector(4.0f, 5.0f, 6.0f));
    VxMatrix matrix;
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column)
            matrix[row][column] = static_cast<float>(row * 10 + column);
    }

    CKParameterOut *text = behavior
        ? behavior->GetOutputParameter(PoutText) : nullptr;
    return behavior &&
        behavior->SetOutputParameterValue(PoutBool, &boolean) == CK_OK &&
        behavior->SetOutputParameterValue(PoutInteger, &integer) == CK_OK &&
        behavior->SetOutputParameterValue(PoutFloat, &real) == CK_OK &&
        text && text->SetStringValue("transport") == CK_OK &&
        behavior->SetOutputParameterValue(PoutVec2, &vec2) == CK_OK &&
        behavior->SetOutputParameterValue(PoutVector, &vec3) == CK_OK &&
        behavior->SetOutputParameterValue(PoutQuaternion, &quaternion) == CK_OK &&
        behavior->SetOutputParameterValue(PoutEuler, euler) == CK_OK &&
        behavior->SetOutputParameterValue(PoutRect, &rect) == CK_OK &&
        behavior->SetOutputParameterValue(PoutColor, &color) == CK_OK &&
        behavior->SetOutputParameterValue(PoutBox, &box) == CK_OK &&
        behavior->SetOutputParameterValue(PoutMatrix, &matrix) == CK_OK &&
        behavior->SetOutputParameterObject(PoutObject, nullptr) == CK_OK;
}

bool EchoPins(CKBehavior *behavior) {
    for (int pin = PinBool; pin <= PinObject; ++pin) {
        if (!CopyPin(behavior, pin, PoutBool + pin))
            return false;
    }
    return true;
}

int FindInput(CKBehavior *behavior, const char *name) {
    if (!behavior)
        return -1;
    for (int index = 0; index < behavior->GetInputCount(); ++index) {
        CKBehaviorIO *input = behavior->GetInput(index);
        if (input && input->GetName() &&
            std::strcmp(input->GetName(), name) == 0)
            return index;
    }
    return -1;
}

int FindOutput(CKBehavior *behavior, const char *name) {
    if (!behavior)
        return -1;
    for (int index = 0; index < behavior->GetOutputCount(); ++index) {
        CKBehaviorIO *output = behavior->GetOutput(index);
        if (output && output->GetName() &&
            std::strcmp(output->GetName(), name) == 0)
            return index;
    }
    return -1;
}

int FindPin(CKBehavior *behavior, const char *name) {
    if (!behavior)
        return -1;
    for (int index = 0; index < behavior->GetInputParameterCount(); ++index) {
        CKParameterIn *pin = behavior->GetInputParameter(index);
        if (pin && pin->GetName() && std::strcmp(pin->GetName(), name) == 0)
            return index;
    }
    return -1;
}

int FindPout(CKBehavior *behavior, const char *name) {
    if (!behavior)
        return -1;
    for (int index = 0; index < behavior->GetOutputParameterCount(); ++index) {
        CKParameterOut *pout = behavior->GetOutputParameter(index);
        if (pout && pout->GetName() && std::strcmp(pout->GetName(), name) == 0)
            return index;
    }
    return -1;
}

int FindLocal(CKBehavior *behavior, const char *name) {
    if (!behavior)
        return -1;
    for (int index = 0; index < behavior->GetLocalParameterCount(); ++index) {
        CKParameterLocal *local = behavior->GetLocalParameter(index);
        if (local && local->GetName() &&
            std::strcmp(local->GetName(), name) == 0)
            return index;
    }
    return -1;
}

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

    if (behavior->IsInputActive(InputEcho)) {
        if (!EchoPins(behavior))
            return CKBR_BEHAVIORERROR;
    } else if (behavior->IsInputActive(InputEchoNumber)) {
        if (!CopyPin(behavior, PinInteger, PoutInteger))
            return CKBR_BEHAVIORERROR;
    } else if (!SetStaticValues(behavior)) {
        return CKBR_BEHAVIORERROR;
    }

    if (behavior->IsInputActive(InputMakeObject)) {
        CKObject *object = context.Context->CreateObject(
            CKCID_BEOBJECT, "__BML_BehaviorTransport_Object",
            CK_OBJECTCREATION_DYNAMIC);
        if (!object)
            return CKBR_BEHAVIORERROR;
        const CK_ID objectId = object->GetID();
        behavior->SetLocalParameterValue(LocalObjectId, &objectId);
        behavior->SetOutputParameterObject(PoutObject, object);
    } else if (!behavior->IsInputActive(InputEcho)) {
        behavior->SetOutputParameterObject(PoutObject, nullptr);
    }
    behavior->SetOutputParameterObject(PoutTarget, behavior->GetTarget());

    const int dynamicInput = FindInput(behavior, "Dynamic Run");
    if (dynamicInput >= 0 && behavior->IsInputActive(dynamicInput)) {
        const int dynamicPin = FindPin(behavior, "Dynamic Value");
        const int dynamicPout = FindPout(behavior, "Dynamic Value");
        const int dynamicOutput = FindOutput(behavior, "Dynamic Done");
        if (dynamicPin < 0 || dynamicPout < 0 || dynamicOutput < 0 ||
            !CopyPin(behavior, dynamicPin, dynamicPout))
            return CKBR_BEHAVIORERROR;
        behavior->ActivateOutput(dynamicOutput);
    }

    const int changeLayout = FindInput(behavior, "Change Layout");
    if (changeLayout >= 0 && behavior->IsInputActive(changeLayout) &&
        FindLocal(behavior, "Created During Execute") < 0 &&
        !behavior->CreateLocalParameter("Created During Execute", CKPGUID_INT))
        return CKBR_BEHAVIORERROR;

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
    if (context.CallbackMessage == CKM_BEHAVIORSETTINGSEDITED) {
        CKBOOL extended = FALSE;
        context.Behavior->GetLocalParameterValue(
            SettingExtendedLayout, &extended);
        if (extended) {
            if (FindInput(context.Behavior, "Dynamic Run") < 0 &&
                !context.Behavior->CreateInput("Dynamic Run"))
                return CKERR_OUTOFMEMORY;
            if (FindOutput(context.Behavior, "Dynamic Done") < 0 &&
                !context.Behavior->CreateOutput("Dynamic Done"))
                return CKERR_OUTOFMEMORY;
            if (FindPin(context.Behavior, "Dynamic Value") < 0 &&
                !context.Behavior->CreateInputParameter(
                    "Dynamic Value", CKPGUID_INT))
                return CKERR_OUTOFMEMORY;
            if (FindPout(context.Behavior, "Dynamic Value") < 0 &&
                !context.Behavior->CreateOutputParameter(
                    "Dynamic Value", CKPGUID_INT))
                return CKERR_OUTOFMEMORY;
            if (FindInput(context.Behavior, "Change Layout") < 0 &&
                !context.Behavior->CreateInput("Change Layout"))
                return CKERR_OUTOFMEMORY;
        }
        return CK_OK;
    }
    if (context.CallbackMessage != CKM_BEHAVIORDELETE)
        return CK_OK;
    CK_ID objectId = 0;
    context.Behavior->GetLocalParameterValue(LocalObjectId, &objectId);
    CKObject *object = objectId ? context.Context->GetObject(objectId) : nullptr;
    if (object)
        context.Context->DestroyObject(object);
    return CK_OK;
}

CKERROR CreateProviderValue(CKParameter *parameter) {
    if (!parameter)
        return CKERR_INVALIDPARAMETER;
    int *value = new int(0);
    const CKERROR error = parameter->SetValue(&value, sizeof(value));
    if (error != CK_OK)
        delete value;
    return error;
}

void DeleteProviderValue(CKParameter *parameter) {
    int *value = nullptr;
    if (parameter)
        (void) parameter->GetValue(&value, FALSE);
    delete value;
    value = nullptr;
    if (parameter)
        (void) parameter->SetValue(&value, sizeof(value));
}

void CopyProviderValue(CKParameter *destination, CKParameter *source) {
    int *left = nullptr;
    int *right = nullptr;
    if (!destination || !source ||
        destination->GetValue(&left, FALSE) != CK_OK ||
        source->GetValue(&right, FALSE) != CK_OK || !right) {
        return;
    }
    if (!left) {
        left = new int(0);
        if (destination->SetValue(&left, sizeof(left)) != CK_OK) {
            delete left;
            return;
        }
    }
    *left = *right;
}

void SaveProviderValue(CKParameter *parameter, CKStateChunk **chunk,
                       CKBOOL load) {
    if (!parameter || !chunk)
        return;
    if (!load) {
        int *value = nullptr;
        (void) parameter->GetValue(&value, FALSE);
        CKStateChunk *saved = CreateCKStateChunk(CKCID_PARAMETER, nullptr);
        if (!saved)
            return;
        saved->StartWrite();
        saved->WriteInt(value ? *value : 0);
        saved->CloseChunk();
        *chunk = saved;
        return;
    }
    if (!*chunk)
        return;
    (*chunk)->StartRead();
    const int saved = (*chunk)->ReadInt();
    int *value = nullptr;
    if (parameter->GetValue(&value, FALSE) == CK_OK && value)
        *value = saved;
}

int StringProviderValue(CKParameter *parameter, char *text,
                        CKBOOL readFromString) {
    if (!parameter)
        return 0;
    int *value = nullptr;
    if (parameter->GetValue(&value, FALSE) != CK_OK || !value)
        return 0;
    if (readFromString) {
        if (!text)
            return 0;
        *value = static_cast<int>(std::strtol(text, nullptr, 10));
        return 0;
    }
    const int size = std::snprintf(nullptr, 0, "%d", *value) + 1;
    if (text)
        std::snprintf(text, static_cast<std::size_t>(size), "%d", *value);
    return size;
}

CKERROR InitFixture(CKContext *context) {
    CKParameterManager *parameters = context
        ? context->GetParameterManager() : nullptr;
    if (!parameters)
        return CKERR_INVALIDPARAMETER;
    CKParameterTypeDesc type;
    type.Guid = BML_BEHAVIOR_PROVIDER_VALUE_GUID;
    type.DerivedFrom = CKPGUID_INT;
    type.TypeName = "BML Provider Value";
    type.DefaultSize = sizeof(int *);
    type.CreateDefaultFunction = CreateProviderValue;
    type.DeleteFunction = DeleteProviderValue;
    type.SaveLoadFunction = SaveProviderValue;
    type.CopyFunction = CopyProviderValue;
    type.StringFunction = StringProviderValue;
    return parameters->RegisterParameterType(&type);
}

CKERROR ExitFixture(CKContext *context) {
    CKParameterManager *parameters = context
        ? context->GetParameterManager() : nullptr;
    return parameters
        ? parameters->UnRegisterParameterType(
              BML_BEHAVIOR_PROVIDER_VALUE_GUID)
        : CKERR_INVALIDPARAMETER;
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
    created->DeclareInput("Echo");
    created->DeclareInput("Read Target");
    created->DeclareInput("Duplicate");
    created->DeclareInput("Duplicate");
    created->DeclareInput("Echo Number");
    created->DeclareOutput("Done");
    created->DeclareOutput("Deleted");
    created->DeclareOutParameter("Bool", CKPGUID_BOOL, "FALSE");
    created->DeclareOutParameter("Number", CKPGUID_INT, "0");
    created->DeclareOutParameter("Number", CKPGUID_FLOAT, "0");
    created->DeclareOutParameter("Text", CKPGUID_STRING, "");
    created->DeclareOutParameter("Vec2", CKPGUID_2DVECTOR, "0,0");
    created->DeclareOutParameter("Vector", CKPGUID_VECTOR, "0,0,0");
    created->DeclareOutParameter("Quaternion", CKPGUID_QUATERNION);
    created->DeclareOutParameter("Euler", CKPGUID_EULERANGLES);
    created->DeclareOutParameter("Rect", CKPGUID_RECT);
    created->DeclareOutParameter("Color", CKPGUID_COLOR);
    created->DeclareOutParameter("Box", CKPGUID_BOX);
    created->DeclareOutParameter("Matrix", CKPGUID_MATRIX);
    created->DeclareOutParameter("Object", CKPGUID_BEOBJECT);
    created->DeclareOutParameter("Target", CKPGUID_BEOBJECT);
    created->DeclareInParameter("Bool", CKPGUID_BOOL, "FALSE");
    created->DeclareInParameter("Number", CKPGUID_INT, "0");
    created->DeclareInParameter("Number", CKPGUID_FLOAT, "0");
    created->DeclareInParameter("Text", CKPGUID_STRING, "");
    created->DeclareInParameter("Vec2", CKPGUID_2DVECTOR, "0,0");
    created->DeclareInParameter("Vector", CKPGUID_VECTOR, "0,0,0");
    created->DeclareInParameter("Quaternion", CKPGUID_QUATERNION);
    created->DeclareInParameter("Euler", CKPGUID_EULERANGLES);
    created->DeclareInParameter("Rect", CKPGUID_RECT);
    created->DeclareInParameter("Color", CKPGUID_COLOR);
    created->DeclareInParameter("Box", CKPGUID_BOX);
    created->DeclareInParameter("Matrix", CKPGUID_MATRIX);
    created->DeclareInParameter("Object", CKPGUID_BEOBJECT);
    created->DeclareSetting("Retry", CKPGUID_BOOL, "FALSE");
    created->DeclareSetting("Extended Layout", CKPGUID_BOOL, "FALSE");
    created->DeclareLocalParameter("Executions", CKPGUID_INT, "0");
    created->DeclareLocalParameter("Object ID", CKPGUID_INT, "0");
    created->SetFunction(Run);
    created->SetBehaviorCallbackFct(
        Lifecycle, CKCB_BEHAVIORSETTINGSEDITED | CKCB_BEHAVIORDELETE);
    created->SetBehaviorFlags(static_cast<CK_BEHAVIOR_FLAGS>(
        CKBEHAVIOR_TARGETABLE | CKBEHAVIOR_INTERNALLYCREATEDINPUTS |
        CKBEHAVIOR_INTERNALLYCREATEDOUTPUTS |
        CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS |
        CKBEHAVIOR_INTERNALLYCREATEDOUTPUTPARAMS |
        CKBEHAVIOR_INTERNALLYCREATEDLOCALPARAMS));
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

CKERROR GraphLifecycle(const CKBehaviorContext &context);

CKERROR CreateGraphPrototype(CKBehaviorPrototype **prototype) {
    if (!prototype)
        return CKERR_INVALIDPARAMETER;
    CKBehaviorPrototype *created =
        CreateCKBehaviorPrototype("BML Behavior Graph Fixture");
    if (!created)
        return CKERR_OUTOFMEMORY;
    created->DeclareInput("Enter");
    created->DeclareOutput("Exit");
    created->DeclareSetting("Run Owned", CKPGUID_BOOL, "FALSE");
    created->SetApplyToClassID(CKCID_3DENTITY);
    created->SetBehaviorCallbackFct(GraphLifecycle, CKCB_BEHAVIORCREATE);
    created->SetBehaviorFlags(CKBEHAVIOR_SCRIPT);
    created->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
    *prototype = created;
    return CK_OK;
}

CKERROR GraphLifecycle(const CKBehaviorContext &context) {
    GraphStep(BML_BEHAVIOR_TRANSPORT_GRAPH_CREATE);
    if (!context.Behavior || !context.Context)
        return GraphStep(BML_BEHAVIOR_TRANSPORT_GRAPH_CREATE,
                         CKERR_INVALIDPARAMETER);
    if (context.CallbackMessage != CKM_BEHAVIORCREATE)
        return CK_OK;
    CKBOOL runOwned = FALSE;
    if (context.Behavior->GetLocalParameterValue(0, &runOwned) != CK_OK)
        return GraphStep(BML_BEHAVIOR_TRANSPORT_GRAPH_CREATE,
                         CKERR_INVALIDPARAMETER);
    context.Behavior->SetName(
        runOwned ? "__BML_BehaviorTransport_RunGraph"
                 : "__BML_BehaviorTransport_Graph");
    // A Prototype without an Execute function is callback-only. The provider,
    // not Runtime, changes this instance into a graph while CREATE still owns
    // the native callback state.
    context.Behavior->UseGraph();
    CKBehaviorIO *input = context.Behavior->GetInput(0);
    CKBehaviorIO *output = context.Behavior->GetOutput(0);
    if (!input || !output)
        return GraphStep(BML_BEHAVIOR_TRANSPORT_GRAPH_PARENT_IO,
                         CKERR_INVALIDOBJECT);
    GraphStep(BML_BEHAVIOR_TRANSPORT_GRAPH_PARENT_IO);
    auto *child = static_cast<CKBehavior *>(context.Context->CreateObject(
        CKCID_BEHAVIOR, "Graph Function", CK_OBJECTCREATION_DYNAMIC));
    if (!child)
        return GraphStep(BML_BEHAVIOR_TRANSPORT_GRAPH_CHILD_CREATED,
                         CKERR_OUTOFMEMORY);
    GraphStep(BML_BEHAVIOR_TRANSPORT_GRAPH_CHILD_CREATED);
    CKERROR error = child->InitFromGuid(BML_BEHAVIOR_TRANSPORT_FIXTURE_GUID);
    if (error != CK_OK) {
        GraphStep(BML_BEHAVIOR_TRANSPORT_GRAPH_CHILD_INITIALIZED, error);
        context.Context->DestroyObject(child);
        return error;
    }
    GraphStep(BML_BEHAVIOR_TRANSPORT_GRAPH_CHILD_INITIALIZED);
    error = context.Behavior->AddSubBehavior(child);
    if (error != CK_OK) {
        GraphStep(BML_BEHAVIOR_TRANSPORT_GRAPH_CHILD_ADDED, error);
        context.Context->DestroyObject(child);
        return error;
    }
    GraphStep(BML_BEHAVIOR_TRANSPORT_GRAPH_CHILD_ADDED);
    const auto addLink = [&](CKBehaviorIO *source, CKBehaviorIO *destination,
                             BML_BehaviorTransportGraphStage created,
                             BML_BehaviorTransportGraphStage sourceSet,
                             BML_BehaviorTransportGraphStage destinationSet,
                             BML_BehaviorTransportGraphStage added) {
        auto *link = static_cast<CKBehaviorLink *>(
            context.Context->CreateObject(
                CKCID_BEHAVIORLINK, nullptr, CK_OBJECTCREATION_DYNAMIC));
        if (!link)
            return GraphStep(created, CKERR_OUTOFMEMORY);
        GraphStep(created);
        CKERROR linkError = link->SetInBehaviorIO(source);
        if (linkError != CK_OK) {
            GraphStep(sourceSet, linkError);
            context.Context->DestroyObject(link);
            return linkError;
        }
        GraphStep(sourceSet);
        linkError = link->SetOutBehaviorIO(destination);
        if (linkError != CK_OK) {
            GraphStep(destinationSet, linkError);
            context.Context->DestroyObject(link);
            return linkError;
        }
        GraphStep(destinationSet);
        linkError = context.Behavior->AddSubBehaviorLink(link);
        if (linkError != CK_OK) {
            GraphStep(added, linkError);
            context.Context->DestroyObject(link);
            return linkError;
        }
        return GraphStep(added);
    };
    if (!child->GetInput(0) || !child->GetOutput(0))
        return GraphStep(BML_BEHAVIOR_TRANSPORT_GRAPH_CHILD_ADDED,
                         CKERR_INVALIDOBJECT);
    error = addLink(input, child->GetInput(0),
                    BML_BEHAVIOR_TRANSPORT_GRAPH_ENTRY_LINK_CREATED,
                    BML_BEHAVIOR_TRANSPORT_GRAPH_ENTRY_SOURCE_SET,
                    BML_BEHAVIOR_TRANSPORT_GRAPH_ENTRY_DESTINATION_SET,
                    BML_BEHAVIOR_TRANSPORT_GRAPH_ENTRY_LINK_ADDED);
    if (error != CK_OK)
        return error;
    error = addLink(child->GetOutput(0), output,
                    BML_BEHAVIOR_TRANSPORT_GRAPH_EXIT_LINK_CREATED,
                    BML_BEHAVIOR_TRANSPORT_GRAPH_EXIT_SOURCE_SET,
                    BML_BEHAVIOR_TRANSPORT_GRAPH_EXIT_DESTINATION_SET,
                    BML_BEHAVIOR_TRANSPORT_GRAPH_EXIT_LINK_ADDED);
    return error == CK_OK
        ? GraphStep(BML_BEHAVIOR_TRANSPORT_GRAPH_READY)
        : error;
}

CKObjectDeclaration *GraphDeclaration() {
    CKObjectDeclaration *declaration =
        CreateCKObjectDeclaration("BML Behavior Graph Fixture");
    declaration->SetDescription("BML test-only graph Prototype fixture");
    declaration->SetCategory("BML/Test/Graph");
    declaration->SetType(CKDLL_BEHAVIORPROTOTYPE);
    declaration->SetGuid(BML_BEHAVIOR_TRANSPORT_GRAPH_FIXTURE_GUID);
    declaration->SetAuthorGuid(kAuthorGuid);
    declaration->SetAuthorName("BML+");
    declaration->SetVersion(0x00010001);
    declaration->SetCreationFunction(CreateGraphPrototype);
    declaration->SetCompatibleClassId(CKCID_3DENTITY);
    declaration->NeedManager(TIME_MANAGER_GUID);
    return declaration;
}

CKPluginInfo g_PluginInfo;

} // namespace

extern "C" __declspec(dllexport)
const BML_BehaviorTransportGraphTrace *
BMLBehaviorTransportReadGraphTrace() {
    return &g_GraphTrace;
}

PLUGIN_EXPORT int CKGetPluginInfoCount() {
    return 1;
}

PLUGIN_EXPORT CKPluginInfo *CKGetPluginInfo(int) {
    g_PluginInfo.m_Author = "BML+";
    g_PluginInfo.m_Description = "BML Behavior transport test fixture";
    g_PluginInfo.m_Extension = "";
    g_PluginInfo.m_Type = CKPLUGIN_BEHAVIOR_DLL;
    g_PluginInfo.m_Version = 0x000001;
    g_PluginInfo.m_InitInstanceFct = InitFixture;
    g_PluginInfo.m_ExitInstanceFct = ExitFixture;
    g_PluginInfo.m_GUID = BML_BEHAVIOR_TRANSPORT_PLUGIN_GUID;
    g_PluginInfo.m_Summary = "Behavior outcome transport fixture";
    return &g_PluginInfo;
}

PLUGIN_EXPORT void RegisterBehaviorDeclarations(XObjectDeclarationArray *registry) {
    if (registry) {
        CKStoreDeclaration(registry, Declaration());
        CKStoreDeclaration(registry, GraphDeclaration());
    }
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) {
    return TRUE;
}

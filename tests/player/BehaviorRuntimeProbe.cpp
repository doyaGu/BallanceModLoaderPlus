#include "BehaviorRuntimeProbe.h"

#include "Behavior/HookBlock.h"
#include "Behavior/ObjectLoad.h"
#include "Behavior/Physicalize.h"
#include "Behavior/PhysicsImpulse.h"
#include "Behavior/Runtime.h"
#include "Behavior/Text2D.h"
#include "BML/Guids/physics_RT.h"

#include <sstream>

namespace {

using namespace BML::Behavior;

struct ExecutionProbe {
    CKContext *Context = nullptr;
    CKBehavior *Behavior = nullptr;
    int Calls = 0;
    int FirstResult = CKBR_OK;
    bool ContextMatched = true;
};

int ProbeExecution(const CKBehaviorContext *context, void *argument) {
    auto *probe = static_cast<ExecutionProbe *>(argument);
    if (!probe || !context)
        return CKBR_BEHAVIORERROR;

    ++probe->Calls;
    probe->ContextMatched = probe->ContextMatched &&
                            context->Context == probe->Context &&
                            context->Behavior == probe->Behavior;
    return probe->Calls == 1 ? probe->FirstResult : CKBR_OK;
}

void AppendFailure(std::ostringstream &details, const char *failure) {
    if (details.tellp() > 0)
        details << ',';
    details << failure;
}

bool HasDuplicateSettingLocal(const Layout &layout) {
    for (const SlotInfo &setting : layout.Slots) {
        if (setting.Kind != SlotKind::Setting)
            continue;
        for (const SlotInfo &local : layout.Slots) {
            if (local.Kind == SlotKind::Local &&
                local.NativeIndex == setting.NativeIndex) {
                return true;
            }
        }
    }
    return false;
}

bool HasSettings(const Layout &layout, int expectedCount) {
    int count = 0;
    for (const SlotInfo &slot : layout.Slots) {
        if (slot.Kind == SlotKind::Setting)
            ++count;
    }
    return count == expectedCount && !HasDuplicateSettingLocal(layout);
}

Spec PhysicsForceWithOperation(CK3dObject *owner, CKGUID operationGuid) {
    const VxVector zero(0.0f, 0.0f, 0.0f);
    const VxVector direction(1.0f, 0.0f, 0.0f);
    const float left = 2.0f;
    const float right = 3.0f;

    Spec spec(PHYSICS_RT_PHYSICSFORCE);
    spec.Target(CKPGUID_3DENTITY, owner)
        .Input(Slot::At(SlotKind::InputParameter, 0, CKPGUID_VECTOR),
               Value::From(CKPGUID_VECTOR, zero))
        .Input(Slot::At(SlotKind::InputParameter, 1, CKPGUID_3DENTITY),
               Value::Object(CKPGUID_3DENTITY, nullptr))
        .Input(Slot::At(SlotKind::InputParameter, 2, CKPGUID_VECTOR),
               Value::From(CKPGUID_VECTOR, direction))
        .Input(Slot::At(SlotKind::InputParameter, 3, CKPGUID_3DENTITY),
               Value::Object(CKPGUID_3DENTITY, nullptr))
        .Input(Slot::At(SlotKind::InputParameter, 4, CKPGUID_FLOAT),
               Operation(operationGuid)
                   .Result(CKPGUID_FLOAT)
                   .Input1(Value::From(CKPGUID_FLOAT, left))
                   .Input2(Value::From(CKPGUID_FLOAT, right)));
    return spec;
}

} // namespace

BehaviorRuntimeProbeResult RunBehaviorRuntimeProbe(CKContext *context,
                                                   CK3dObject *owner) {
    BehaviorRuntimeProbeResult result;
    std::ostringstream failures;
    if (!context || !owner) {
        result.Detail = "invalid-context";
        return result;
    }

    Runtime runtime(context);

    Physicalize::Options physicalize;
    physicalize.Target = owner;
    CreateResult dynamic = runtime.Instantiate(owner, Physicalize::Ball(physicalize));
    if (!dynamic) {
        AppendFailure(failures, "dynamic-create");
    } else if (HasDuplicateSettingLocal(dynamic.Descriptor)) {
        AppendFailure(failures, "setting-alias");
    } else {
        Status concave = runtime.Reconfigure(
            dynamic.Handle, Physicalize::Concave(physicalize));
        CKBehavior *behavior = dynamic.Handle.Get();
        CKParameterIn *shape = behavior && behavior->GetInputParameterCount() == 12
            ? behavior->GetInputParameter(11) : nullptr;
        const bool concaveLayout = concave && shape && shape->GetName() &&
                                   std::string(shape->GetName()) == "concave 1";
        Status ball = runtime.Reconfigure(
            dynamic.Handle, Physicalize::Ball(physicalize));
        behavior = dynamic.Handle.Get();
        CKParameterIn *position = behavior && behavior->GetInputParameterCount() == 13
            ? behavior->GetInputParameter(11) : nullptr;
        CKParameterIn *radius = behavior && behavior->GetInputParameterCount() == 13
            ? behavior->GetInputParameter(12) : nullptr;
        const bool ballLayout = ball && position && radius &&
            position->GetName() && radius->GetName() &&
            std::string(position->GetName()) == "ball position 1" &&
            std::string(radius->GetName()) == "ball radius 1";
        if (!concaveLayout || !ballLayout)
            AppendFailure(failures, "dynamic-reconfigure");
    }

    ObjectLoad::Options objectLoad;
    CreateResult loader = runtime.Instantiate(nullptr, ObjectLoad::Make(objectLoad));
    Text2D::Options text;
    CreateResult text2d = runtime.Instantiate(nullptr, Text2D::Make(text));
    PhysicsImpulse::Options impulse;
    impulse.Target = owner;
    impulse.DirectionAsPoint = TRUE;
    CreateResult physicsImpulse = runtime.Instantiate(
        owner, PhysicsImpulse::Make(impulse));
    CKBehavior *impulseBehavior = physicsImpulse.Handle.Get();
    CKParameterIn *secondPosition = impulseBehavior
        ? impulseBehavior->GetInputParameter(2) : nullptr;
    if (!loader || !HasSettings(loader.Descriptor, 1) ||
        !text2d || !HasSettings(text2d.Descriptor, 1) ||
        !physicsImpulse || !HasSettings(physicsImpulse.Descriptor, 2) ||
        !secondPosition || !secondPosition->GetName() ||
        std::string(secondPosition->GetName()) != "Position 2") {
        AppendFailure(failures, "setting-specs");
    }

    ExecutionProbe retry;
    retry.Context = context;
    retry.FirstResult = CKBR_BEHAVIORERROR_RETRY;
    CreateResult retryInstance = runtime.Instantiate(
        owner, HookBlock::Make(ProbeExecution, &retry, 1, 2));
    if (!retryInstance) {
        AppendFailure(failures, "retry-create");
    } else {
        retry.Behavior = retryInstance.Handle.Get();
        RunResult first = runtime.StartTask(
            retryInstance.Handle, Slot::At(SlotKind::Input, 0));
        const bool firstOk = first.ReturnCode == CKBR_BEHAVIORERROR_RETRY &&
                             first.State == RunState::Continuing &&
                             first.ActiveOutputs.size() == 2 &&
                             runtime.IsTaskActive(retryInstance.Handle);
        runtime.ProcessFrame();
        const bool resumed = retry.Calls == 2 && retry.ContextMatched &&
                             !runtime.IsTaskActive(retryInstance.Handle);
        if (!firstOk || !resumed)
            AppendFailure(failures, "retry-semantics");
    }

    ExecutionProbe breakpoint;
    breakpoint.Context = context;
    breakpoint.FirstResult = CKBR_BREAK;
    CreateResult breakInstance = runtime.Instantiate(
        owner, HookBlock::Make(ProbeExecution, &breakpoint));
    if (!breakInstance) {
        AppendFailure(failures, "break-create");
    } else {
        breakpoint.Behavior = breakInstance.Handle.Get();
        RunResult first = runtime.StartTask(
            breakInstance.Handle, Slot::At(SlotKind::Input, 0));
        const bool suspended = first.ReturnCode == CKBR_BREAK &&
                               first.State == RunState::Suspended &&
                               runtime.IsTaskActive(breakInstance.Handle);
        runtime.ProcessFrame();
        if (!suspended || breakpoint.Calls != 2 || !breakpoint.ContextMatched ||
            runtime.IsTaskActive(breakInstance.Handle)) {
            AppendFailure(failures, "break-semantics");
        }
    }

    CKParameterManager *parameters = context->GetParameterManager();
    const CKGUID addition = parameters
        ? parameters->OperationNameToGuid(const_cast<char *>("Addition"))
        : CKGUID();
    if (!addition.IsValid()) {
        AppendFailure(failures, "addition-missing");
    } else {
        CreateResult operation = runtime.Instantiate(
            owner, PhysicsForceWithOperation(owner, addition));
        CKParameterIn *magnitude = operation && operation.Handle.Get()
            ? operation.Handle.Get()->GetInputParameter(4) : nullptr;
        CKParameter *operationOutput = magnitude ? magnitude->GetDirectSource() : nullptr;
        CKObject *operationObject = operationOutput ? operationOutput->GetOwner() : nullptr;
        const CK_ID operationId = operationObject &&
            CKIsChildClassOf(operationObject, CKCID_PARAMETEROPERATION)
            ? operationObject->GetID() : 0;
        if (!operation || !operationId) {
            AppendFailure(failures, "operation-create");
        } else {
            const float replacement = 7.0f;
            Status rebound = runtime.SetInput(
                operation.Handle,
                Slot::At(SlotKind::InputParameter, 4, CKPGUID_FLOAT),
                Value::From(CKPGUID_FLOAT, replacement));
            runtime.ProcessFrame();
            runtime.ProcessFrame();
            runtime.ProcessFrame();
            if (!rebound || context->GetObject(operationId) != nullptr)
                AppendFailure(failures, "operation-retained");
        }

        auto *graph = static_cast<CKBehavior *>(
            context->CreateObject(CKCID_BEHAVIOR, nullptr,
                                  CK_OBJECTCREATION_DYNAMIC));
        if (!graph) {
            AppendFailure(failures, "graph-create");
        } else {
            graph->UseGraph();
            const CKERROR ownerStatus = graph->SetOwner(owner, FALSE);
            AttachResult attached = ownerStatus == CK_OK
                ? runtime.AddToGraph(graph,
                                     PhysicsForceWithOperation(owner, addition))
                : AttachResult{};
            CKBehavior *block = attached.Block;
            CKParameterIn *magnitude = block
                ? block->GetInputParameter(4) : nullptr;
            CKParameter *operationOutput = magnitude
                ? magnitude->GetDirectSource() : nullptr;
            CKObject *operationObject = operationOutput
                ? operationOutput->GetOwner() : nullptr;
            const CK_ID blockId = block ? block->GetID() : 0;
            const CK_ID graphOperationId = operationObject
                ? operationObject->GetID() : 0;
            const bool placed = attached && block &&
                block->GetParent() == graph && block->GetOwner() == owner &&
                graph->GetSubBehaviorCount() == 1 &&
                graph->GetSubBehavior(0) == block &&
                graph->GetParameterOperationCount() == 1 &&
                graph->GetParameterOperation(0) == operationObject &&
                operationObject &&
                CKIsChildClassOf(operationObject, CKCID_PARAMETEROPERATION);

            runtime.ResetWorld();
            const bool cleaned = blockId != 0 && graphOperationId != 0 &&
                context->GetObject(blockId) == nullptr &&
                context->GetObject(graphOperationId) == nullptr &&
                graph->GetSubBehaviorCount() == 0 &&
                graph->GetParameterOperationCount() == 0;
            if (!placed || !cleaned)
                AppendFailure(failures, "graph-ownership");
            context->DestroyObject(graph);
        }
    }

    result.Detail = failures.str();
    result.Passed = result.Detail.empty();
    if (result.Passed)
        result.Detail = "complete";
    return result;
}

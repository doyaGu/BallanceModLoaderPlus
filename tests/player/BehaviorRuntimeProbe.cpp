#include "BehaviorRuntimeProbe.h"

#include "Behavior/HookBlock.h"
#include "Behavior/ObjectLoad.h"
#include "Behavior/Physicalize.h"
#include "Behavior/PhysicsImpulse.h"
#include "Behavior/Runtime.h"
#include "Behavior/Text2D.h"
#include "BML/Guids/physics_RT.h"

#include <sstream>
#include <utility>

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
    CKBehaviorManager *manager = context->Context
        ? context->Context->GetBehaviorManager() : nullptr;
    probe->ContextMatched = probe->ContextMatched &&
                            context->Context == probe->Context &&
                            context->Behavior == probe->Behavior &&
                            manager && manager->m_CurrentBehavior == probe->Behavior;
    return probe->Calls == 1 ? probe->FirstResult : CKBR_OK;
}

struct ContextSnapshot {
    CKBehaviorContext Context;
    CKBehaviorManager *Manager = nullptr;
    CKBehavior *CurrentBehavior = nullptr;
};

ContextSnapshot CaptureContext(CKContext *context) {
    ContextSnapshot snapshot;
    snapshot.Context = context->m_BehaviorContext;
    snapshot.Manager = context->GetBehaviorManager();
    snapshot.CurrentBehavior = snapshot.Manager
        ? snapshot.Manager->m_CurrentBehavior : nullptr;
    return snapshot;
}

bool ContextRestored(CKContext *context, const ContextSnapshot &snapshot) {
    const CKBehaviorContext &left = context->m_BehaviorContext;
    const CKBehaviorContext &right = snapshot.Context;
    return left.Behavior == right.Behavior &&
           left.DeltaTime == right.DeltaTime &&
           left.Context == right.Context &&
           left.CurrentLevel == right.CurrentLevel &&
           left.CurrentScene == right.CurrentScene &&
           left.PreviousScene == right.PreviousScene &&
           left.CurrentRenderContext == right.CurrentRenderContext &&
           left.ParameterManager == right.ParameterManager &&
           left.MessageManager == right.MessageManager &&
           left.AttributeManager == right.AttributeManager &&
           left.TimeManager == right.TimeManager &&
           left.CallbackMessage == right.CallbackMessage &&
           left.CallbackArg == right.CallbackArg &&
           context->GetBehaviorManager() == snapshot.Manager &&
           (!snapshot.Manager ||
            snapshot.Manager->m_CurrentBehavior == snapshot.CurrentBehavior);
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

Spec PhysicsForceBase(CK3dObject *owner) {
    const VxVector zero(0.0f, 0.0f, 0.0f);
    const VxVector direction(1.0f, 0.0f, 0.0f);

    Spec spec(PHYSICS_RT_PHYSICSFORCE);
    spec.Target(CKPGUID_3DENTITY, owner)
        .Input(Slot::At(SlotKind::InputParameter, 0, CKPGUID_VECTOR),
               Value::From(CKPGUID_VECTOR, zero))
        .Input(Slot::At(SlotKind::InputParameter, 1, CKPGUID_3DENTITY),
               Value::Object(CKPGUID_3DENTITY, nullptr))
        .Input(Slot::At(SlotKind::InputParameter, 2, CKPGUID_VECTOR),
               Value::From(CKPGUID_VECTOR, direction))
        .Input(Slot::At(SlotKind::InputParameter, 3, CKPGUID_3DENTITY),
               Value::Object(CKPGUID_3DENTITY, nullptr));
    return spec;
}

Spec PhysicsForceWithOperation(CK3dObject *owner, CKGUID operationGuid) {
    const float left = 2.0f;
    const float right = 3.0f;
    Spec spec = PhysicsForceBase(owner);
    spec.Input(Slot::At(SlotKind::InputParameter, 4, CKPGUID_FLOAT),
               Operation(operationGuid)
                   .Result(CKPGUID_FLOAT)
                   .Input1(Value::From(CKPGUID_FLOAT, left))
                   .Input2(Value::From(CKPGUID_FLOAT, right)));
    return spec;
}

Spec PhysicsForceWithSource(CK3dObject *owner, CKParameter *magnitude) {
    Spec spec = PhysicsForceBase(owner);
    spec.Input(Slot::At(SlotKind::InputParameter, 4, CKPGUID_FLOAT),
               Value::DirectSource(magnitude));
    return spec;
}

Spec PhysicsForceWithMagnitude(CK3dObject *owner, float magnitude) {
    Spec spec = PhysicsForceBase(owner);
    spec.Input(Slot::At(SlotKind::InputParameter, 4, CKPGUID_FLOAT),
               Value::From(CKPGUID_FLOAT, magnitude));
    return spec;
}

} // namespace

class BehaviorRuntimeProbe::Impl final {
public:
    Impl(CKContext *context, CK3dObject *owner)
        : m_Context(context), m_Owner(owner), m_Runtime(context),
          m_ConsumerRuntime(context) {}

    void Advance(int playerFrame) {
        if (m_State == State::Complete)
            return;
        if (!m_Context || !m_Owner) {
            Fail("invalid-context");
            Finish();
            return;
        }
        if (playerFrame <= m_LastPlayerFrame) {
            Fail("player-frame-not-advanced");
            Finish();
            return;
        }
        m_LastPlayerFrame = playerFrame;

        switch (m_State) {
        case State::StaticChecks: RunStaticChecks(); break;
        case State::RetryStart: StartRetry(); break;
        case State::RetryResume: ResumeRetry(); break;
        case State::BreakStart: StartBreak(); break;
        case State::BreakResume: ResumeBreak(); break;
        case State::OperationCreate: CreateSharedOperation(); break;
        case State::OperationShared1:
        case State::OperationShared2:
        case State::OperationShared3: AdvanceSharedOperation(); break;
        case State::OperationRelease: ReleaseSharedOperation(); break;
        case State::OperationCleanup1:
        case State::OperationCleanup2:
        case State::OperationCleanup3: AdvanceOperationCleanup(); break;
        case State::GraphOwnership: CheckGraphOwnership(); break;
        case State::Complete: break;
        }
    }

    [[nodiscard]] bool Done() const { return m_State == State::Complete; }

    [[nodiscard]] BehaviorRuntimeProbeResult Result() const {
        BehaviorRuntimeProbeResult result;
        result.Detail = m_Failures.str();
        result.Passed = Done() && result.Detail.empty();
        if (result.Passed)
            result.Detail = "complete";
        else if (result.Detail.empty())
            result.Detail = "not-complete";
        return result;
    }

private:
    enum class State {
        StaticChecks,
        RetryStart,
        RetryResume,
        BreakStart,
        BreakResume,
        OperationCreate,
        OperationShared1,
        OperationShared2,
        OperationShared3,
        OperationRelease,
        OperationCleanup1,
        OperationCleanup2,
        OperationCleanup3,
        GraphOwnership,
        Complete,
    };

    void Fail(const char *failure) {
        if (m_Failures.tellp() > 0)
            m_Failures << ',';
        m_Failures << failure;
    }

    template <class Action>
    auto WithContextCheck(const char *failure, Action &&action)
        -> decltype(action()) {
        const ContextSnapshot before = CaptureContext(m_Context);
        auto result = action();
        if (!ContextRestored(m_Context, before))
            Fail(failure);
        return result;
    }

    void ProcessRuntimeFrame(const char *failure) {
        const ContextSnapshot before = CaptureContext(m_Context);
        m_Runtime.ProcessFrame();
        if (!ContextRestored(m_Context, before))
            Fail(failure);
    }

    void ProcessConsumerFrame(const char *failure) {
        const ContextSnapshot before = CaptureContext(m_Context);
        m_ConsumerRuntime.ProcessFrame();
        if (!ContextRestored(m_Context, before))
            Fail(failure);
    }

    void RunStaticChecks() {
        Physicalize::Options physicalize;
        physicalize.Target = m_Owner;
        CreateResult dynamic = m_Runtime.Instantiate(
            m_Owner, Physicalize::Ball(physicalize));
        if (!dynamic) {
            Fail("dynamic-create");
        } else if (HasDuplicateSettingLocal(dynamic.Descriptor)) {
            Fail("setting-alias");
        } else {
            Status concave = m_Runtime.Reconfigure(
                dynamic.Handle, Physicalize::Concave(physicalize));
            CKBehavior *behavior = dynamic.Handle.Get();
            CKParameterIn *shape = behavior &&
                behavior->GetInputParameterCount() == 12
                ? behavior->GetInputParameter(11) : nullptr;
            const bool concaveLayout = concave && shape && shape->GetName() &&
                std::string(shape->GetName()) == "concave 1";
            Status ball = m_Runtime.Reconfigure(
                dynamic.Handle, Physicalize::Ball(physicalize));
            behavior = dynamic.Handle.Get();
            CKParameterIn *position = behavior &&
                behavior->GetInputParameterCount() == 13
                ? behavior->GetInputParameter(11) : nullptr;
            CKParameterIn *radius = behavior &&
                behavior->GetInputParameterCount() == 13
                ? behavior->GetInputParameter(12) : nullptr;
            const bool ballLayout = ball && position && radius &&
                position->GetName() && radius->GetName() &&
                std::string(position->GetName()) == "ball position 1" &&
                std::string(radius->GetName()) == "ball radius 1";
            if (!concaveLayout || !ballLayout)
                Fail("dynamic-reconfigure");
        }

        ObjectLoad::Options objectLoad;
        CreateResult loader = m_Runtime.Instantiate(
            nullptr, ObjectLoad::Make(objectLoad));
        Text2D::Options text;
        CreateResult text2d = m_Runtime.Instantiate(nullptr, Text2D::Make(text));
        PhysicsImpulse::Options impulse;
        impulse.Target = m_Owner;
        impulse.DirectionAsPoint = TRUE;
        CreateResult physicsImpulse = m_Runtime.Instantiate(
            m_Owner, PhysicsImpulse::Make(impulse));
        CKBehavior *impulseBehavior = physicsImpulse.Handle.Get();
        CKParameterIn *secondPosition = impulseBehavior
            ? impulseBehavior->GetInputParameter(2) : nullptr;
        if (!loader || !HasSettings(loader.Descriptor, 1) ||
            !text2d || !HasSettings(text2d.Descriptor, 1) ||
            !physicsImpulse || !HasSettings(physicsImpulse.Descriptor, 2) ||
            !secondPosition || !secondPosition->GetName() ||
            std::string(secondPosition->GetName()) != "Position 2") {
            Fail("setting-specs");
        }

        CreateResult literalOwner = m_Runtime.Instantiate(
            m_Owner, PhysicsForceWithMagnitude(m_Owner, 2.0f));
        CKParameterIn *ownerMagnitude = literalOwner && literalOwner.Handle.Get()
            ? literalOwner.Handle.Get()->GetInputParameter(4) : nullptr;
        CKParameter *sharedLiteral = ownerMagnitude
            ? ownerMagnitude->GetDirectSource() : nullptr;
        CreateResult literalConsumer = m_ConsumerRuntime.Instantiate(
            m_Owner, PhysicsForceWithSource(m_Owner, sharedLiteral));
        CKParameterIn *consumerMagnitude = literalConsumer &&
            literalConsumer.Handle.Get()
            ? literalConsumer.Handle.Get()->GetInputParameter(4) : nullptr;
        Status literalRebound = literalOwner
            ? m_Runtime.SetInput(
                  literalOwner.Handle,
                  Slot::At(SlotKind::InputParameter, 4, CKPGUID_FLOAT),
                  Value::From(CKPGUID_FLOAT, 7.0f))
            : Status{};
        const bool literalIsolated = literalOwner && literalConsumer &&
            literalRebound && ownerMagnitude && consumerMagnitude &&
            ownerMagnitude->GetDirectSource() != sharedLiteral &&
            consumerMagnitude->GetDirectSource() == sharedLiteral;
        if (!literalIsolated)
            Fail("literal-shared");
        m_State = State::RetryStart;
    }

    void StartRetry() {
        m_Retry.Context = m_Context;
        m_Retry.FirstResult = CKBR_BEHAVIORERROR_RETRY;
        CreateResult created = m_Runtime.Instantiate(
            m_Owner, HookBlock::Make(ProbeExecution, &m_Retry, 1, 2));
        if (!created) {
            Fail("retry-create");
            m_State = State::BreakStart;
            return;
        }
        m_RetryInstance = std::move(created.Handle);
        m_Retry.Behavior = m_RetryInstance.Get();
        RunResult first = WithContextCheck("retry-start-context-restore", [&] {
            return m_Runtime.StartTask(
                m_RetryInstance, Slot::At(SlotKind::Input, 0));
        });
        const bool firstOk = first.ReturnCode == CKBR_BEHAVIORERROR_RETRY &&
                             first.State == RunState::Continuing &&
                             first.ActiveOutputs.size() == 2 &&
                             m_Runtime.IsTaskActive(m_RetryInstance);
        if (!firstOk)
            Fail("retry-start");
        m_State = State::RetryResume;
    }

    void ResumeRetry() {
        ProcessRuntimeFrame("retry-frame-context-restore");
        if (m_Retry.Calls != 2 || !m_Retry.ContextMatched ||
            m_Runtime.IsTaskActive(m_RetryInstance)) {
            Fail("retry-semantics");
        }
        m_RetryInstance.Reset();
        m_State = State::BreakStart;
    }

    void StartBreak() {
        m_Breakpoint.Context = m_Context;
        m_Breakpoint.FirstResult = CKBR_BREAK;
        CreateResult created = m_Runtime.Instantiate(
            m_Owner, HookBlock::Make(ProbeExecution, &m_Breakpoint));
        if (!created) {
            Fail("break-create");
            m_State = State::OperationCreate;
            return;
        }
        m_BreakInstance = std::move(created.Handle);
        m_Breakpoint.Behavior = m_BreakInstance.Get();
        RunResult first = WithContextCheck("break-start-context-restore", [&] {
            return m_Runtime.StartTask(
                m_BreakInstance, Slot::At(SlotKind::Input, 0));
        });
        const bool suspended = first.ReturnCode == CKBR_BREAK &&
                               first.State == RunState::Suspended &&
                               m_Runtime.IsTaskActive(m_BreakInstance);
        if (!suspended)
            Fail("break-start");
        m_State = State::BreakResume;
    }

    void ResumeBreak() {
        ProcessRuntimeFrame("break-frame-context-restore");
        if (m_Breakpoint.Calls != 2 || !m_Breakpoint.ContextMatched ||
            m_Runtime.IsTaskActive(m_BreakInstance)) {
            Fail("break-semantics");
        }
        m_BreakInstance.Reset();
        m_State = State::OperationCreate;
    }

    void CreateSharedOperation() {
        CKParameterManager *parameters = m_Context->GetParameterManager();
        const CKGUID addition = parameters
            ? parameters->OperationNameToGuid(const_cast<char *>("Addition"))
            : CKGUID();
        if (!addition.IsValid()) {
            Fail("addition-missing");
            m_State = State::GraphOwnership;
            return;
        }
        m_Addition = addition;

        CreateResult operation = m_Runtime.Instantiate(
            m_Owner, PhysicsForceWithOperation(m_Owner, addition));
        m_OperationInstance = std::move(operation.Handle);
        CKParameterIn *magnitude = operation && m_OperationInstance.Get()
            ? m_OperationInstance.Get()->GetInputParameter(4) : nullptr;
        m_OperationOutput = magnitude ? magnitude->GetDirectSource() : nullptr;
        CKObject *operationObject = m_OperationOutput
            ? m_OperationOutput->GetOwner() : nullptr;
        m_OperationId = operationObject &&
            CKIsChildClassOf(operationObject, CKCID_PARAMETEROPERATION)
            ? operationObject->GetID() : 0;
        if (!operation || !m_OperationId) {
            Fail("operation-create");
            m_State = State::GraphOwnership;
            return;
        }

        CreateResult consumer = m_ConsumerRuntime.Instantiate(
            m_Owner, PhysicsForceWithSource(m_Owner, m_OperationOutput));
        m_ConsumerInstance = std::move(consumer.Handle);
        m_ConsumerMagnitude = consumer && m_ConsumerInstance.Get()
            ? m_ConsumerInstance.Get()->GetInputParameter(4) : nullptr;
        const float replacement = 7.0f;
        Status rebound = m_Runtime.SetInput(
            m_OperationInstance,
            Slot::At(SlotKind::InputParameter, 4, CKPGUID_FLOAT),
            Value::From(CKPGUID_FLOAT, replacement));
        if (!consumer || !rebound)
            Fail("operation-bind");
        m_State = State::OperationShared1;
    }

    void AdvanceSharedOperation() {
        ProcessRuntimeFrame("operation-frame-context-restore");
        ProcessConsumerFrame("consumer-frame-context-restore");
        if (m_State == State::OperationShared1) {
            m_State = State::OperationShared2;
        } else if (m_State == State::OperationShared2) {
            m_State = State::OperationShared3;
        } else {
            const bool retained = m_Context->GetObject(m_OperationId) != nullptr &&
                m_ConsumerMagnitude &&
                m_ConsumerMagnitude->GetDirectSource() == m_OperationOutput;
            if (!retained)
                Fail("operation-shared");
            m_State = State::OperationRelease;
        }
    }

    void ReleaseSharedOperation() {
        const float replacement = 7.0f;
        Status rebound = m_ConsumerRuntime.SetInput(
            m_ConsumerInstance,
            Slot::At(SlotKind::InputParameter, 4, CKPGUID_FLOAT),
            Value::From(CKPGUID_FLOAT, replacement));
        if (!rebound)
            Fail("operation-consumer-rebind");
        m_State = State::OperationCleanup1;
    }

    void AdvanceOperationCleanup() {
        ProcessRuntimeFrame("operation-cleanup-context-restore");
        ProcessConsumerFrame("consumer-cleanup-context-restore");
        if (m_State == State::OperationCleanup1) {
            m_State = State::OperationCleanup2;
        } else if (m_State == State::OperationCleanup2) {
            m_State = State::OperationCleanup3;
        } else {
            if (m_Context->GetObject(m_OperationId) != nullptr)
                Fail("operation-retained");
            m_OperationOutput = nullptr;
            m_OperationInstance.Reset();
            m_ConsumerInstance.Reset();
            m_State = State::GraphOwnership;
        }
    }

    void CheckGraphOwnership() {
        auto *graph = static_cast<CKBehavior *>(m_Context->CreateObject(
            CKCID_BEHAVIOR, nullptr, CK_OBJECTCREATION_DYNAMIC));
        if (!graph) {
            Fail("graph-create");
            Finish();
            return;
        }

        graph->UseGraph();
        const CKERROR ownerStatus = graph->SetOwner(m_Owner, FALSE);
        AttachResult attached = ownerStatus == CK_OK
            ? m_Runtime.AddToGraph(
                  graph, PhysicsForceWithOperation(m_Owner, m_Addition))
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
            block->GetParent() == graph && block->GetOwner() == m_Owner &&
            graph->GetSubBehaviorCount() == 1 &&
            graph->GetSubBehavior(0) == block &&
            graph->GetParameterOperationCount() == 1 &&
            graph->GetParameterOperation(0) == operationObject &&
            operationObject &&
            CKIsChildClassOf(operationObject, CKCID_PARAMETEROPERATION);

        const ContextSnapshot before = CaptureContext(m_Context);
        m_Runtime.ResetWorld();
        if (!ContextRestored(m_Context, before))
            Fail("reset-context-restore");
        const bool cleaned = blockId != 0 && graphOperationId != 0 &&
            m_Context->GetObject(blockId) == nullptr &&
            m_Context->GetObject(graphOperationId) == nullptr &&
            graph->GetSubBehaviorCount() == 0 &&
            graph->GetParameterOperationCount() == 0;
        if (!placed || !cleaned)
            Fail("graph-ownership");
        m_Context->DestroyObject(graph);
        Finish();
    }

    void Finish() { m_State = State::Complete; }

    CKContext *m_Context = nullptr;
    CK3dObject *m_Owner = nullptr;
    Runtime m_Runtime;
    Runtime m_ConsumerRuntime;
    State m_State = State::StaticChecks;
    int m_LastPlayerFrame = -1;
    std::ostringstream m_Failures;
    ExecutionProbe m_Retry;
    ExecutionProbe m_Breakpoint;
    Instance m_RetryInstance;
    Instance m_BreakInstance;
    Instance m_OperationInstance;
    Instance m_ConsumerInstance;
    CKGUID m_Addition = CKGUID();
    CKParameter *m_OperationOutput = nullptr;
    CKParameterIn *m_ConsumerMagnitude = nullptr;
    CK_ID m_OperationId = 0;
};

BehaviorRuntimeProbe::BehaviorRuntimeProbe(CKContext *context, CK3dObject *owner)
    : m_Impl(std::make_unique<Impl>(context, owner)) {}

BehaviorRuntimeProbe::~BehaviorRuntimeProbe() = default;

void BehaviorRuntimeProbe::Advance(int playerFrame) {
    m_Impl->Advance(playerFrame);
}

bool BehaviorRuntimeProbe::Done() const {
    return m_Impl->Done();
}

BehaviorRuntimeProbeResult BehaviorRuntimeProbe::Result() const {
    return m_Impl->Result();
}

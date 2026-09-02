#include "BehaviorRuntimeSemantics.h"
#include "BehaviorLifecycleFixtureApi.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "Behavior/HookBlock.h"
#include "Behavior/CKEdit.h"
#include "Behavior/ObjectLoad.h"
#include "Behavior/Physicalize.h"
#include "Behavior/PhysicsImpulse.h"
#include "Behavior/Runtime.h"
#include "Behavior/Text2D.h"
#include "BML/Guids/Logics.h"
#include "BML/Guids/physics_RT.h"

#include <memory>
#include <initializer_list>
#include <sstream>
#include <thread>
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

int CloseLifecycleFixture(CKBehavior *behavior, void *argument) {
    auto *runtime = static_cast<Runtime *>(argument);
    return runtime && runtime->Close(behavior) ? 1 : 0;
}

CKBehaviorLink *CreateBehaviorLink(CKContext *context, CKBehaviorIO *source,
                                   CKBehaviorIO *destination, int delay) {
    auto *link = static_cast<CKBehaviorLink *>(context->CreateObject(
        CKCID_BEHAVIORLINK, nullptr, CK_OBJECTCREATION_DYNAMIC));
    if (!link)
        return nullptr;
    if (link->SetInBehaviorIO(source) != CK_OK ||
        link->SetOutBehaviorIO(destination) != CK_OK) {
        context->DestroyObject(link);
        return nullptr;
    }
    link->SetInitialActivationDelay(delay);
    link->SetActivationDelay(delay);
    return link;
}

struct RecursivePumpProbe {
    Runtime *Owner = nullptr;
    int Calls = 0;
};

struct ReentrantPulseProbe {
    Runtime *Owner = nullptr;
    Instance *Handle = nullptr;
    int Calls = 0;
    bool Queued = false;
};

int ProbeReentrantPulse(const CKBehaviorContext *, void *argument) {
    auto *probe = static_cast<ReentrantPulseProbe *>(argument);
    if (!probe || !probe->Owner || !probe->Handle)
        return CKBR_BEHAVIORERROR;
    ++probe->Calls;
    if (probe->Calls == 1) {
        RunResult queued = probe->Owner->Pulse(
            *probe->Handle, Slot::Named(SlotKind::Input, "In 1"));
        probe->Queued = queued.State == RunState::Pending &&
                        queued.Admission == AdmissionState::Queued;
    }
    return CKBR_OK;
}

struct CountedExecutionProbe {
    int Calls = 0;
    int CompleteAfter = 1;
};

int ProbeCountedExecution(const CKBehaviorContext *, void *argument) {
    auto *probe = static_cast<CountedExecutionProbe *>(argument);
    if (!probe)
        return CKBR_BEHAVIORERROR;
    ++probe->Calls;
    return probe->Calls < probe->CompleteAfter
        ? CKBR_ACTIVATENEXTFRAME : CKBR_OK;
}

struct PatchCloseProbe {
    CKEdit *Editor = nullptr;
    Patch *Target = nullptr;
    const Edit *Candidate = nullptr;
    Patch *CandidatePatch = nullptr;
    CKBehavior *Graph = nullptr;
    int NodesBefore = 0;
    int Calls = 0;
    bool Queued = false;
    bool ApplyQueued = false;
    bool MutationDeferred = false;
    bool InvocationObserved = false;
};

int ProbePatchClose(const CKBehaviorContext *context, void *argument) {
    auto *probe = static_cast<PatchCloseProbe *>(argument);
    if (!probe || !probe->Editor || !probe->Target)
        return CKBR_BEHAVIORERROR;
    ++probe->Calls;
    CKBehaviorManager *manager = context && context->Context
        ? context->Context->GetBehaviorManager() : nullptr;
    probe->InvocationObserved = context && context->Behavior && manager &&
                                manager->m_CurrentBehavior == context->Behavior;
    const Status status = probe->Editor->Close(*probe->Target);
    probe->Queued = static_cast<bool>(status) &&
                    probe->Target->State() == PatchState::Closing;
    if (probe->Candidate && probe->CandidatePatch && probe->Graph) {
        const Status applied = probe->Editor->Apply(
            *probe->Candidate, *probe->CandidatePatch);
        probe->ApplyQueued = static_cast<bool>(applied) &&
                             probe->CandidatePatch->State() ==
                                 PatchState::Pending;
        probe->MutationDeferred =
            probe->Graph->GetSubBehaviorCount() == probe->NodesBefore;
    }
    return CKBR_OK;
}

int ProbeRecursivePump(const CKBehaviorContext *, void *argument) {
    auto *probe = static_cast<RecursivePumpProbe *>(argument);
    if (!probe || !probe->Owner)
        return CKBR_BEHAVIORERROR;
    ++probe->Calls;
    if (probe->Calls == 2)
        probe->Owner->ProcessFrame();
    return probe->Calls < 3 ? CKBR_BEHAVIORERROR_RETRY : CKBR_OK;
}

struct ReentrantReleaseProbe {
    Instance *Handle = nullptr;
    int Calls = 0;
};

int ProbeReentrantRelease(const CKBehaviorContext *, void *argument) {
    auto *probe = static_cast<ReentrantReleaseProbe *>(argument);
    if (!probe || !probe->Handle)
        return CKBR_BEHAVIORERROR;
    ++probe->Calls;
    probe->Handle->Reset();
    return CKBR_ACTIVATENEXTFRAME;
}

struct SelfDeleteProbe {
    CK_ID BehaviorId = 0;
    int Calls = 0;
    bool Deferred = false;
    bool Requested = false;
};

int ProbeSelfDelete(const CKBehaviorContext *context, void *argument) {
    auto *probe = static_cast<SelfDeleteProbe *>(argument);
    if (!probe || !context || !context->Context || !context->Behavior)
        return CKBR_BEHAVIORERROR;
    ++probe->Calls;
    probe->BehaviorId = context->Behavior->GetID();
    probe->Deferred = context->Context->m_DeferDestroyObjects != FALSE;
    if (probe->Deferred) {
        probe->Requested = context->Context->DestroyObject(context->Behavior) == CK_OK;
    }
    return CKBR_OK;
}

struct ContextSnapshot {
    CKBehaviorContext Context;
    CKBehaviorManager *Manager = nullptr;
    CKBehavior *CurrentBehavior = nullptr;
    CKDWORD DeferDestroyObjects = FALSE;
};

ContextSnapshot CaptureContext(CKContext *context) {
    ContextSnapshot snapshot;
    snapshot.Context = context->m_BehaviorContext;
    snapshot.Manager = context->GetBehaviorManager();
    snapshot.CurrentBehavior = snapshot.Manager
        ? snapshot.Manager->m_CurrentBehavior : nullptr;
    snapshot.DeferDestroyObjects = context->m_DeferDestroyObjects;
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
           context->m_DeferDestroyObjects == snapshot.DeferDestroyObjects &&
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

class BehaviorRuntimeSemantics::Impl final {
public:
    Impl(CKContext *context, CK3dObject *owner)
        : m_Context(context), m_Owner(owner), m_Runtime(context),
          m_ConsumerRuntime(context) {
        m_EditGraph = MakeCKGraphSource(
            context, m_Runtime, [](const void *value) {
                auto *object = const_cast<CKObject *>(
                    static_cast<const CKObject *>(value));
                return object
                    ? ObjectRef{0x424d4c45u,
                                static_cast<std::uint32_t>(object->GetID()), 1}
                    : ObjectRef{};
            });
        if (m_EditGraph)
            m_Editor = std::make_unique<CKEdit>(
                context, m_Runtime, nullptr, *m_EditGraph);
    }

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
        if (m_Editor)
            m_Editor->ProcessFrame();

        switch (m_State) {
        case State::StaticChecks: RunStaticChecks(); break;
        case State::RetryStart: StartRetry(); break;
        case State::RetryResume: ResumeRetry(); break;
        case State::BreakStart: StartBreak(); break;
        case State::BreakResume: ResumeBreak(); break;
        case State::WaitAllStart: StartWaitAll(); break;
        case State::WaitAllResume: ResumeWaitAll(); break;
        case State::TerminalPulseStart: StartTerminalPulse(); break;
        case State::TerminalPulseResume: ResumeTerminalPulse(); break;
        case State::ReentrantPulseStart: StartReentrantPulse(); break;
        case State::ReentrantPulseResume: ResumeReentrantPulse(); break;
        case State::FrameLatestStart: StartLatestFrames(); break;
        case State::FrameLatestResume1:
        case State::FrameLatestResume2:
        case State::FrameLatestResume3: ResumeLatestFrames(); break;
        case State::FrameFullStart: StartFullFrames(); break;
        case State::FrameFullCheck: CheckFullFrames(); break;
        case State::FrameFullStopped: CheckFullFramesStopped(); break;
        case State::DetachedGraphStart: StartDetachedGraph(); break;
        case State::DetachedGraphWait: ObserveDetachedGraph(); break;
        case State::RecursivePumpStart: StartRecursivePump(); break;
        case State::RecursivePumpResume1:
        case State::RecursivePumpResume2: ResumeRecursivePump(); break;
        case State::ReentrantReleaseStart: StartReentrantRelease(); break;
        case State::ReentrantReleaseCleanup1:
        case State::ReentrantReleaseCleanup2: CleanupReentrantRelease(); break;
        case State::SelfDeleteStart: StartSelfDelete(); break;
        case State::SelfDeleteWait1:
        case State::SelfDeleteWait2: WaitForSelfDelete(); break;
        case State::OperationCreate: CreateSharedOperation(); break;
        case State::OperationShared1:
        case State::OperationShared2:
        case State::OperationShared3: AdvanceSharedOperation(); break;
        case State::OperationRelease: ReleaseSharedOperation(); break;
        case State::OperationCleanup1:
        case State::OperationCleanup2:
        case State::OperationCleanup3: AdvanceOperationCleanup(); break;
        case State::RuntimeCloseStart: StartRuntimeClose(); break;
        case State::RuntimeCloseRelease: ReleaseRuntimeCloseConsumer(); break;
        case State::RuntimeCloseCleanup1:
        case State::RuntimeCloseCleanup2:
        case State::RuntimeCloseCleanup3: AdvanceRuntimeCloseCleanup(); break;
        case State::GraphSchedulerStart: StartGraphScheduler(); break;
        case State::GraphSchedulerWaitFirst:
        case State::GraphSchedulerWaitSecond:
        case State::GraphSchedulerWaitThird: ObserveGraphScheduler(); break;
        case State::GraphSchedulerCleanup: CleanupGraphScheduler(); break;
        case State::GraphOwnership: CheckGraphOwnership(); break;
        case State::SpliceStart: StartSplice(); break;
        case State::SplicePending: ApplySplice(); break;
        case State::SpliceWait: ObserveSplice(); break;
        case State::SpliceClose: CloseSplice(); break;
        case State::AdditiveEditStart: StartAdditiveEdit(); break;
        case State::AdditiveEditWait: ObserveAdditiveEdit(); break;
        case State::AdditiveEditClose: CloseAdditiveEdit(); break;
        case State::LifecycleFixture: CheckLifecycleFixture(); break;
        case State::Complete: break;
        }
    }

    [[nodiscard]] bool Done() const { return m_State == State::Complete; }

    [[nodiscard]] BehaviorRuntimeSemanticsResult Result() const {
        BehaviorRuntimeSemanticsResult result;
        result.Detail = m_Failures.str();
        result.LifecyclePassed = m_LifecyclePassed;
        result.AdditiveEditPassed = m_AdditiveEditPassed;
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
        WaitAllStart,
        WaitAllResume,
        TerminalPulseStart,
        TerminalPulseResume,
        ReentrantPulseStart,
        ReentrantPulseResume,
        FrameLatestStart,
        FrameLatestResume1,
        FrameLatestResume2,
        FrameLatestResume3,
        FrameFullStart,
        FrameFullCheck,
        FrameFullStopped,
        DetachedGraphStart,
        DetachedGraphWait,
        RecursivePumpStart,
        RecursivePumpResume1,
        RecursivePumpResume2,
        ReentrantReleaseStart,
        ReentrantReleaseCleanup1,
        ReentrantReleaseCleanup2,
        SelfDeleteStart,
        SelfDeleteWait1,
        SelfDeleteWait2,
        OperationCreate,
        OperationShared1,
        OperationShared2,
        OperationShared3,
        OperationRelease,
        OperationCleanup1,
        OperationCleanup2,
        OperationCleanup3,
        RuntimeCloseStart,
        RuntimeCloseRelease,
        RuntimeCloseCleanup1,
        RuntimeCloseCleanup2,
        RuntimeCloseCleanup3,
        GraphSchedulerStart,
        GraphSchedulerWaitFirst,
        GraphSchedulerWaitSecond,
        GraphSchedulerWaitThird,
        GraphSchedulerCleanup,
        GraphOwnership,
        SpliceStart,
        SplicePending,
        SpliceWait,
        SpliceClose,
        AdditiveEditStart,
        AdditiveEditWait,
        AdditiveEditClose,
        LifecycleFixture,
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
                             first.State == RunState::Pending &&
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
            m_State = State::WaitAllStart;
            return;
        }
        m_BreakInstance = std::move(created.Handle);
        m_Breakpoint.Behavior = m_BreakInstance.Get();
        RunResult first = WithContextCheck("break-start-context-restore", [&] {
            return m_Runtime.StartTask(
                m_BreakInstance, Slot::At(SlotKind::Input, 0));
        });
        const Status failure = m_Runtime.InstanceFailure(m_BreakInstance);
        const bool rejected = first.ReturnCode == CKBR_BREAK &&
                              first.State == RunState::Failed &&
                              !m_Runtime.IsTaskActive(m_BreakInstance) &&
                              failure.Code == Error::UnsupportedBreak;
        if (!rejected)
            Fail("break-start");
        m_State = State::BreakResume;
    }

    void ResumeBreak() {
        ProcessRuntimeFrame("break-frame-context-restore");
        if (m_Breakpoint.Calls != 1 || !m_Breakpoint.ContextMatched ||
            m_Runtime.IsTaskActive(m_BreakInstance)) {
            Fail("break-semantics");
        }
        m_BreakInstance.Reset();
        m_State = State::WaitAllStart;
    }

    void StartWaitAll() {
        CreateResult created = m_Runtime.Instantiate(
            m_Owner, Spec(VT_LOGICS_WAITFORALL));
        if (!created) {
            Fail("wait-all-create");
            m_State = State::TerminalPulseStart;
            return;
        }
        m_WaitAllInstance = std::move(created.Handle);
        RunResult first = WithContextCheck("wait-all-first-context-restore", [&] {
            return m_Runtime.Pulse(
                m_WaitAllInstance,
                Slot::Named(SlotKind::Input, "In 0"));
        });
        CKBehavior *behavior = m_WaitAllInstance.Get();
        RunResult second = WithContextCheck("wait-all-second-context-restore", [&] {
            return m_Runtime.Pulse(
                m_WaitAllInstance,
                Slot::Named(SlotKind::Input, "In 1"));
        });
        const std::vector<RunFrame> firstFrames =
            m_Runtime.Take(m_WaitAllInstance);
        const bool firstPending = first.State == RunState::Pending &&
            second.State == RunState::Pending &&
            second.Admission == AdmissionState::Queued && behavior &&
            behavior->IsInputActive(0) && !behavior->IsInputActive(1) &&
            m_Runtime.IsTaskActive(m_WaitAllInstance) &&
            firstFrames.size() == 1 &&
            firstFrames[0].Sequence == 1 &&
            firstFrames[0].NativeContinuation &&
            !firstFrames[0].QueuedInput;
        if (!firstPending)
            Fail("wait-all-first");
        m_State = State::WaitAllResume;
    }

    void ResumeWaitAll() {
        ProcessRuntimeFrame("wait-all-frame-context-restore");
        CKBehavior *behavior = m_WaitAllInstance.Get();
        const std::vector<RunFrame> frames =
            m_Runtime.Take(m_WaitAllInstance);
        const bool completed = behavior && !behavior->IsInputActive(0) &&
            !behavior->IsInputActive(1) && !behavior->IsOutputActive(0) &&
            !m_Runtime.IsTaskActive(m_WaitAllInstance) &&
            m_Runtime.State(m_WaitAllInstance) == ExecutionState::Idle &&
            frames.size() == 1 && frames[0].Sequence == 2 &&
            !frames[0].NativeContinuation && !frames[0].QueuedInput &&
            frames[0].ActiveOutputs.size() == 1 &&
            frames[0].ActiveOutputs[0].Name == "Out";
        if (!completed)
            Fail("wait-all-complete");
        m_WaitAllInstance.Reset();
        m_State = State::TerminalPulseStart;
    }

    void StartTerminalPulse() {
        m_TerminalPulse.Context = m_Context;
        CreateResult created = m_Runtime.Instantiate(
            m_Owner, HookBlock::Make(ProbeExecution, &m_TerminalPulse));
        if (!created) {
            Fail("terminal-pulse-create");
            m_State = State::ReentrantPulseStart;
            return;
        }
        m_TerminalPulseInstance = std::move(created.Handle);
        m_TerminalPulse.Behavior = m_TerminalPulseInstance.Get();
        RunResult first = m_Runtime.Pulse(
            m_TerminalPulseInstance,
            Slot::Named(SlotKind::Input, "In 0"));
        RunResult second = m_Runtime.Pulse(
            m_TerminalPulseInstance,
            Slot::Named(SlotKind::Input, "In 0"));
        if (first.State != RunState::Ready ||
            second.State != RunState::Pending ||
            second.Admission != AdmissionState::Queued || m_TerminalPulse.Calls != 1 ||
            m_Runtime.State(m_TerminalPulseInstance) != ExecutionState::Pending) {
            Fail("terminal-pulse-queued");
        }
        m_State = State::TerminalPulseResume;
    }

    void ResumeTerminalPulse() {
        ProcessRuntimeFrame("terminal-pulse-context-restore");
        const std::vector<RunFrame> frames =
            m_Runtime.Take(m_TerminalPulseInstance);
        if (m_TerminalPulse.Calls != 2 || !m_TerminalPulse.ContextMatched ||
            m_Runtime.State(m_TerminalPulseInstance) != ExecutionState::Idle ||
            frames.size() != 2 || frames[0].Sequence != 1 ||
            frames[1].Sequence != 2 || frames[1].NativeContinuation ||
            frames[1].QueuedInput) {
            Fail("terminal-pulse-complete");
        }
        m_TerminalPulseInstance.Reset();
        m_State = State::ReentrantPulseStart;
    }

    void StartReentrantPulse() {
        m_ReentrantPulse.Owner = &m_Runtime;
        CreateResult created = m_Runtime.Instantiate(
            m_Owner,
            HookBlock::Make(ProbeReentrantPulse, &m_ReentrantPulse, 2, 1));
        if (!created) {
            Fail("reentrant-pulse-create");
            m_State = State::FrameLatestStart;
            return;
        }
        m_ReentrantPulseInstance = std::move(created.Handle);
        m_ReentrantPulse.Handle = &m_ReentrantPulseInstance;
        RunResult first = WithContextCheck("reentrant-pulse-context-restore", [&] {
            return m_Runtime.Pulse(
                m_ReentrantPulseInstance,
                Slot::Named(SlotKind::Input, "In 0"));
        });
        if (first.State != RunState::Pending || !m_ReentrantPulse.Queued ||
            m_ReentrantPulse.Calls != 1 ||
            !m_Runtime.IsTaskActive(m_ReentrantPulseInstance)) {
            Fail("reentrant-pulse-first");
        }
        m_State = State::ReentrantPulseResume;
    }

    void ResumeReentrantPulse() {
        ProcessRuntimeFrame("reentrant-pulse-frame-context-restore");
        if (m_ReentrantPulse.Calls != 2 ||
            m_Runtime.IsTaskActive(m_ReentrantPulseInstance) ||
            m_Runtime.State(m_ReentrantPulseInstance) != ExecutionState::Idle) {
            Fail("reentrant-pulse-complete");
        }
        m_ReentrantPulse.Handle = nullptr;
        m_ReentrantPulseInstance.Reset();
        m_State = State::FrameLatestStart;
    }

    void StartLatestFrames() {
        m_LatestFrames.CompleteAfter = 4;
        Spec spec = HookBlock::Make(
            ProbeCountedExecution, &m_LatestFrames, 1, 0);
        spec.Frames(FrameRetention::Latest());
        CreateResult created = m_Runtime.Instantiate(m_Owner, spec);
        if (!created) {
            Fail("frame-latest-create");
            m_State = State::FrameFullStart;
            return;
        }
        m_LatestFrameInstance = std::move(created.Handle);
        RunResult first = m_Runtime.StartTask(
            m_LatestFrameInstance,
            Slot::Named(SlotKind::Input, "In 0"));
        if (first.State != RunState::Pending)
            Fail("frame-latest-start");
        m_State = State::FrameLatestResume1;
    }

    void ResumeLatestFrames() {
        ProcessRuntimeFrame("frame-latest-context-restore");
        if (m_State == State::FrameLatestResume1) {
            m_State = State::FrameLatestResume2;
            return;
        }
        if (m_State == State::FrameLatestResume2) {
            m_State = State::FrameLatestResume3;
            return;
        }
        const std::vector<RunFrame> frames =
            m_Runtime.Take(m_LatestFrameInstance);
        if (m_LatestFrames.Calls != 4 || frames.size() != 2 ||
            frames[0].Sequence != 3 || frames[1].Sequence != 4 ||
            frames[1].NativeContinuation || frames[1].QueuedInput ||
            m_Runtime.IsTaskActive(m_LatestFrameInstance)) {
            Fail("frame-latest-sequence");
        }
        m_LatestFrameInstance.Reset();
        m_State = State::FrameFullStart;
    }

    void StartFullFrames() {
        m_FullFrames.CompleteAfter = 10;
        Spec spec = HookBlock::Make(
            ProbeCountedExecution, &m_FullFrames, 1, 0);
        spec.Frames(FrameRetention::EachFrame(1));
        CreateResult created = m_Runtime.Instantiate(m_Owner, spec);
        if (!created) {
            Fail("frame-full-create");
            m_State = State::DetachedGraphStart;
            return;
        }
        m_FullFrameInstance = std::move(created.Handle);
        RunResult first = m_Runtime.StartTask(
            m_FullFrameInstance,
            Slot::Named(SlotKind::Input, "In 0"));
        if (first.State != RunState::Pending)
            Fail("frame-full-start");
        m_State = State::FrameFullCheck;
    }

    void CheckFullFrames() {
        ProcessRuntimeFrame("frame-full-context-restore");
        const Status failure = m_Runtime.InstanceFailure(m_FullFrameInstance);
        const std::vector<RunFrame> frames =
            m_Runtime.Take(m_FullFrameInstance);
        const bool full = m_FullFrames.Calls == 2 &&
            m_Runtime.State(m_FullFrameInstance) == ExecutionState::Failed &&
            !m_Runtime.IsTaskActive(m_FullFrameInstance) &&
            failure.Code == Error::FrameQueueFull && frames.size() == 2 &&
            frames[0].Sequence == 1 && frames[1].Sequence == 2 &&
            frames[1].Fault.Code == ExecutionError::FrameQueueFull &&
            frames[1].Overflow && frames[1].Overflow->Capacity == 1;
        if (!full)
            Fail("frame-full-failure");
        m_State = State::FrameFullStopped;
    }

    void CheckFullFramesStopped() {
        ProcessRuntimeFrame("frame-full-stopped-context-restore");
        if (m_FullFrames.Calls != 2)
            Fail("frame-full-rescheduled");
        m_FullFrameInstance.Reset();
        m_State = State::DetachedGraphStart;
    }

    void StartDetachedGraph() {
        CreateResult created = m_Runtime.Instantiate(
            m_Owner,
            HookBlock::Make(ProbeExecution, &m_DetachedOuter, 1, 1));
        if (!created) {
            Fail("detached-graph-create");
            m_State = State::RecursivePumpStart;
            return;
        }
        m_DetachedGraphInstance = std::move(created.Handle);
        m_DetachedGraph = m_DetachedGraphInstance.Get();
        if (!m_DetachedGraph) {
            Fail("detached-graph-missing");
            m_State = State::RecursivePumpStart;
            return;
        }
        m_DetachedGraph->UseGraph();

        m_DetachedSource.Context = m_Context;
        m_DetachedDestination.Context = m_Context;
        AttachResult source = m_Runtime.AddToGraph(
            m_DetachedGraph,
            HookBlock::Make(ProbeExecution, &m_DetachedSource, 1, 1));
        AttachResult destination = m_Runtime.AddToGraph(
            m_DetachedGraph,
            HookBlock::Make(ProbeExecution, &m_DetachedDestination, 1, 1));
        m_DetachedSource.Behavior = source.Block;
        m_DetachedDestination.Behavior = destination.Block;
        if (source && destination) {
            m_DetachedEntryLink = CreateBehaviorLink(
                m_Context, m_DetachedGraph->GetInput(0),
                source.Block->GetInput(0), 0);
            m_DetachedDelayLink = CreateBehaviorLink(
                m_Context, source.Block->GetOutput(0),
                destination.Block->GetInput(0), 2);
            m_DetachedExitLink = CreateBehaviorLink(
                m_Context, destination.Block->GetOutput(0),
                m_DetachedGraph->GetOutput(0), 0);
        }
        auto add = [&](CKBehaviorLink *link) {
            return link &&
                m_DetachedGraph->AddSubBehaviorLink(link) == CK_OK;
        };
        if (!source || !destination || !add(m_DetachedEntryLink) ||
            !add(m_DetachedDelayLink) || !add(m_DetachedExitLink)) {
            Fail("detached-graph-setup");
            m_Runtime.ResetWorld();
            m_State = State::RecursivePumpStart;
            return;
        }

        RunResult first = WithContextCheck("detached-graph-context-restore", [&] {
            return m_Runtime.StartTask(
                m_DetachedGraphInstance,
                Slot::Named(SlotKind::Input, "In 0"));
        });
        const std::vector<RunFrame> firstFrames =
            m_Runtime.Take(m_DetachedGraphInstance);
        if (first.ReturnCode != CKBR_OK || first.State != RunState::Pending ||
            !m_Runtime.IsTaskActive(m_DetachedGraphInstance) ||
            firstFrames.size() != 1 ||
            !firstFrames[0].NativeContinuation) {
            Fail("detached-graph-native-continuation");
        }
        m_DetachedGraphFrames = 0;
        m_State = State::DetachedGraphWait;
    }

    void ObserveDetachedGraph() {
        ProcessRuntimeFrame("detached-graph-frame-context-restore");
        ++m_DetachedGraphFrames;
        if (m_DetachedDestination.Calls == 0 && m_DetachedGraphFrames < 4)
            return;

        const std::vector<RunFrame> frames =
            m_Runtime.Take(m_DetachedGraphInstance);
        const bool completed = m_DetachedSource.Calls == 1 &&
            m_DetachedDestination.Calls == 1 &&
            m_DetachedSource.ContextMatched &&
            m_DetachedDestination.ContextMatched &&
            !m_Runtime.IsTaskActive(m_DetachedGraphInstance) &&
            m_Runtime.State(m_DetachedGraphInstance) == ExecutionState::Idle &&
            !frames.empty() && !frames.back().NativeContinuation &&
            !frames.back().QueuedInput &&
            frames.back().ActiveOutputs.size() == 1;
        if (!completed)
            Fail("detached-graph-complete");
        m_Runtime.ResetWorld();
        m_DetachedGraph = nullptr;
        m_DetachedEntryLink = nullptr;
        m_DetachedDelayLink = nullptr;
        m_DetachedExitLink = nullptr;
        m_State = State::RecursivePumpStart;
    }

    void StartRecursivePump() {
        m_RecursivePump.Owner = &m_Runtime;
        CreateResult created = m_Runtime.Instantiate(
            m_Owner, HookBlock::Make(ProbeRecursivePump, &m_RecursivePump));
        if (!created) {
            Fail("recursive-pump-create");
            m_State = State::ReentrantReleaseStart;
            return;
        }
        m_RecursivePumpInstance = std::move(created.Handle);
        RunResult first = WithContextCheck("recursive-pump-start-context-restore", [&] {
            return m_Runtime.StartTask(
                m_RecursivePumpInstance, Slot::At(SlotKind::Input, 0));
        });
        if (first.State != RunState::Pending ||
            !m_Runtime.IsTaskActive(m_RecursivePumpInstance)) {
            Fail("recursive-pump-start");
        }
        m_State = State::RecursivePumpResume1;
    }

    void ResumeRecursivePump() {
        ProcessRuntimeFrame("recursive-pump-context-restore");
        if (m_State == State::RecursivePumpResume1) {
            m_State = State::RecursivePumpResume2;
            return;
        }
        if (m_RecursivePump.Calls != 3 ||
            m_Runtime.IsTaskActive(m_RecursivePumpInstance)) {
            Fail("recursive-pump-semantics");
        }
        m_RecursivePumpInstance.Reset();
        m_State = State::ReentrantReleaseStart;
    }

    void StartReentrantRelease() {
        CreateResult created = m_Runtime.Instantiate(
            m_Owner, HookBlock::Make(
                ProbeReentrantRelease, &m_ReentrantRelease));
        if (!created) {
            Fail("reentrant-release-create");
            m_State = State::SelfDeleteStart;
            return;
        }
        m_ReentrantReleaseInstance = std::move(created.Handle);
        m_ReentrantRelease.Handle = &m_ReentrantReleaseInstance;
        CKBehavior *behavior = m_ReentrantReleaseInstance.Get();
        m_ReentrantReleaseId = behavior ? behavior->GetID() : 0;
        RunResult run = WithContextCheck("reentrant-release-context-restore", [&] {
            return m_Runtime.Pulse(
                m_ReentrantReleaseInstance, Slot::At(SlotKind::Input, 0));
        });
        if (m_ReentrantRelease.Calls != 1 || m_ReentrantReleaseInstance ||
            run.State != RunState::Failed ||
            run.Detail.Code != Error::ExecutionCancelled ||
            !m_ReentrantReleaseId) {
            Fail("reentrant-release-result");
        }
        m_State = State::ReentrantReleaseCleanup1;
    }

    void CleanupReentrantRelease() {
        ProcessRuntimeFrame("reentrant-release-cleanup-context-restore");
        if (m_State == State::ReentrantReleaseCleanup1) {
            m_State = State::ReentrantReleaseCleanup2;
            return;
        }
        if (m_Context->GetObject(m_ReentrantReleaseId) != nullptr)
            Fail("reentrant-release-retained");
        m_State = State::SelfDeleteStart;
    }

    void StartSelfDelete() {
        CreateResult created = m_Runtime.Instantiate(
            m_Owner, HookBlock::Make(ProbeSelfDelete, &m_SelfDelete));
        if (!created) {
            Fail("self-delete-create");
            m_State = State::OperationCreate;
            return;
        }
        m_SelfDeleteInstance = std::move(created.Handle);
        RunResult run = WithContextCheck("self-delete-context-restore", [&] {
            return m_Runtime.Pulse(
                m_SelfDeleteInstance, Slot::At(SlotKind::Input, 0));
        });
        if (run.State != RunState::Ready)
            Fail("self-delete-run-state");
        if (run.Detail.Code != Error::None)
            Fail("self-delete-run-error");
        if (m_SelfDelete.Calls != 1)
            Fail("self-delete-call-count");
        if (!m_SelfDelete.Deferred)
            Fail("self-delete-not-deferred");
        if (!m_SelfDelete.Requested)
            Fail("self-delete-not-requested");
        if (!m_SelfDelete.BehaviorId)
            Fail("self-delete-no-id");
        if (m_SelfDelete.Calls != 1 || !m_SelfDelete.Deferred ||
            !m_SelfDelete.Requested || !m_SelfDelete.BehaviorId) {
            m_SelfDeleteInstance.Reset();
        }
        m_State = State::SelfDeleteWait1;
    }

    void WaitForSelfDelete() {
        ProcessRuntimeFrame("self-delete-frame-context-restore");
        if (m_State == State::SelfDeleteWait1) {
            m_State = State::SelfDeleteWait2;
            return;
        }
        if (m_Context->GetObject(m_SelfDelete.BehaviorId) != nullptr ||
            m_SelfDeleteInstance.Get() != nullptr) {
            Fail("self-delete-retained");
        }
        m_SelfDeleteInstance.Reset();
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
        CKBehavior *producerBehavior = m_OperationInstance.Get();
        m_OperationProducerId = producerBehavior ? producerBehavior->GetID() : 0;
        CKParameterIn *magnitude = operation && producerBehavior
            ? producerBehavior->GetInputParameter(4) : nullptr;
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
        m_OperationInstance.Reset();
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
                m_ConsumerMagnitude->GetDirectSource() == m_OperationOutput &&
                m_Context->GetObject(m_OperationProducerId) == nullptr;
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
            m_ConsumerInstance.Reset();
            m_State = State::RuntimeCloseStart;
        }
    }

    void StartRuntimeClose() {
        m_ClosingRuntime = std::make_unique<Runtime>(m_Context);
        CreateResult operation = m_ClosingRuntime->Instantiate(
            m_Owner, PhysicsForceWithOperation(m_Owner, m_Addition));
        m_ClosingProducerInstance = std::move(operation.Handle);
        CKBehavior *producer = m_ClosingProducerInstance.Get();
        m_ClosingProducerId = producer ? producer->GetID() : 0;
        CKParameterIn *magnitude = operation && producer
            ? producer->GetInputParameter(4) : nullptr;
        m_ClosingOutput = magnitude ? magnitude->GetDirectSource() : nullptr;
        CKObject *operationObject = m_ClosingOutput
            ? m_ClosingOutput->GetOwner() : nullptr;
        m_ClosingOperationId = operationObject &&
            CKIsChildClassOf(operationObject, CKCID_PARAMETEROPERATION)
            ? operationObject->GetID() : 0;

        CreateResult consumer = m_ConsumerRuntime.Instantiate(
            m_Owner, PhysicsForceWithSource(m_Owner, m_ClosingOutput));
        m_ClosingConsumerInstance = std::move(consumer.Handle);
        m_ClosingConsumerMagnitude = consumer && m_ClosingConsumerInstance.Get()
            ? m_ClosingConsumerInstance.Get()->GetInputParameter(4) : nullptr;
        if (!operation || !consumer || !m_ClosingProducerId ||
            !m_ClosingOperationId || !m_ClosingConsumerMagnitude) {
            Fail("runtime-close-create");
        }

        m_ClosingRuntime.reset();
        const bool producerClosed =
            m_Context->GetObject(m_ClosingProducerId) == nullptr;
        const bool handleExpired = !m_ClosingProducerInstance &&
            m_ClosingProducerInstance.Get() == nullptr &&
            m_ClosingProducerInstance.LayoutGeneration() == 0;
        const bool sharedOperationSurvived =
            m_Context->GetObject(m_ClosingOperationId) != nullptr &&
            m_ClosingConsumerMagnitude &&
            m_ClosingConsumerMagnitude->GetDirectSource() == m_ClosingOutput;
        m_ClosingProducerInstance.Reset();
        if (!producerClosed || !handleExpired || !sharedOperationSurvived)
            Fail("runtime-close-shared");
        m_State = State::RuntimeCloseRelease;
    }

    void ReleaseRuntimeCloseConsumer() {
        const float replacement = 11.0f;
        Status rebound = m_ConsumerRuntime.SetInput(
            m_ClosingConsumerInstance,
            Slot::At(SlotKind::InputParameter, 4, CKPGUID_FLOAT),
            Value::From(CKPGUID_FLOAT, replacement));
        if (!rebound || !m_ClosingConsumerMagnitude ||
            m_ClosingConsumerMagnitude->GetDirectSource() == m_ClosingOutput) {
            Fail("runtime-close-rebind");
        }
        m_State = State::RuntimeCloseCleanup1;
    }

    void AdvanceRuntimeCloseCleanup() {
        ProcessRuntimeFrame("runtime-close-main-context-restore");
        ProcessConsumerFrame("runtime-close-consumer-context-restore");
        if (m_State == State::RuntimeCloseCleanup1) {
            m_State = State::RuntimeCloseCleanup2;
        } else if (m_State == State::RuntimeCloseCleanup2) {
            m_State = State::RuntimeCloseCleanup3;
        } else {
            if (m_Context->GetObject(m_ClosingOperationId) != nullptr)
                Fail("runtime-close-retained");
            m_ClosingOutput = nullptr;
            m_ClosingConsumerInstance.Reset();
            m_State = State::GraphSchedulerStart;
        }
    }

    void StartGraphScheduler() {
        m_Graph = static_cast<CKBehavior *>(m_Context->CreateObject(
            CKCID_BEHAVIOR, nullptr, CK_OBJECTCREATION_DYNAMIC));
        if (!m_Graph) {
            Fail("graph-scheduler-create");
            m_State = State::GraphOwnership;
            return;
        }
        m_Graph->UseGraph();
        m_Graph->SetType(CKBEHAVIORTYPE_SCRIPT);
        CKBehaviorIO *graphInput = m_Graph->CreateInput("In");
        CKBehaviorIO *graphOutput = m_Graph->CreateOutput("Out");
        CKScene *scene = m_Context->GetCurrentScene();
        if (!graphInput || !graphOutput || !scene ||
            m_Owner->AddScript(m_Graph) != CK_OK) {
            Fail("graph-scheduler-script");
            m_Context->DestroyObject(m_Graph);
            m_Graph = nullptr;
            m_State = State::GraphOwnership;
            return;
        }

        AttachResult immediateSource = m_Runtime.AddToGraph(
            m_Graph, HookBlock::Make(ProbeExecution, &m_GraphImmediateSource, 1, 1));
        AttachResult immediateDestination = m_Runtime.AddToGraph(
            m_Graph, HookBlock::Make(ProbeExecution, &m_GraphImmediateDestination, 1, 0));
        AttachResult delayedDestination = m_Runtime.AddToGraph(
            m_Graph, HookBlock::Make(ProbeExecution, &m_GraphDelayedDestination, 1, 1));
        m_GraphImmediateSource.Behavior = immediateSource.Block;
        m_GraphImmediateDestination.Behavior = immediateDestination.Block;
        m_GraphDelayedDestination.Behavior = delayedDestination.Block;
        m_GraphImmediateSource.Context = m_Context;
        m_GraphImmediateDestination.Context = m_Context;
        m_GraphDelayedDestination.Context = m_Context;

        const bool blocksReady = immediateSource && immediateDestination &&
            delayedDestination && immediateSource.Block->GetInput(0) &&
            immediateSource.Block->GetOutput(0) &&
            immediateDestination.Block->GetInput(0) &&
            delayedDestination.Block->GetInput(0) &&
            delayedDestination.Block->GetOutput(0);
        if (blocksReady) {
            m_GraphEntryLink = CreateBehaviorLink(
                m_Context, graphInput, immediateSource.Block->GetInput(0), 0);
            m_GraphImmediateLink = CreateBehaviorLink(
                m_Context, immediateSource.Block->GetOutput(0),
                immediateDestination.Block->GetInput(0), 0);
            m_GraphDelayedLink = CreateBehaviorLink(
                m_Context, immediateSource.Block->GetOutput(0),
                delayedDestination.Block->GetInput(0), 2);
            m_GraphExitLink = CreateBehaviorLink(
                m_Context, delayedDestination.Block->GetOutput(0), graphOutput, 0);
        }
        auto addLink = [&](CKBehaviorLink *&link) {
            if (link && m_Graph->AddSubBehaviorLink(link) == CK_OK)
                return true;
            if (link)
                m_Context->DestroyObject(link);
            link = nullptr;
            return false;
        };
        bool linksReady = true;
        linksReady = addLink(m_GraphEntryLink) && linksReady;
        linksReady = addLink(m_GraphImmediateLink) && linksReady;
        linksReady = addLink(m_GraphDelayedLink) && linksReady;
        linksReady = addLink(m_GraphExitLink) && linksReady;
        if (!blocksReady || !linksReady) {
            Fail("graph-scheduler-setup");
            m_State = State::GraphSchedulerCleanup;
            return;
        }

        scene->Activate(m_Graph, TRUE);
        m_GraphStartFrame = m_LastPlayerFrame;
        m_State = State::GraphSchedulerWaitFirst;
    }

    void ObserveGraphScheduler() {
        if (m_State == State::GraphSchedulerWaitFirst) {
            if (m_GraphImmediateSource.Calls == 0 &&
                m_LastPlayerFrame - m_GraphStartFrame <= 3) {
                return;
            }
            const bool firstFrame = m_GraphImmediateSource.Calls == 1 &&
                m_GraphImmediateDestination.Calls == 1 &&
                m_GraphDelayedDestination.Calls == 0 && m_Graph->IsActive();
            const bool contextsMatched = m_GraphImmediateSource.ContextMatched &&
                m_GraphImmediateDestination.ContextMatched;
            if (!firstFrame || !contextsMatched)
                Fail("graph-scheduler-immediate");
            m_State = State::GraphSchedulerWaitSecond;
            return;
        }
        if (m_State == State::GraphSchedulerWaitSecond) {
            if (m_GraphDelayedDestination.Calls != 0 || !m_Graph->IsActive())
                Fail("graph-scheduler-delay-pending");
            m_State = State::GraphSchedulerWaitThird;
            return;
        }
        if (m_GraphDelayedDestination.Calls != 1 ||
            !m_GraphDelayedDestination.ContextMatched || m_Graph->IsActive() ||
            !m_Graph->GetOutput(0)->IsActive())
            Fail("graph-scheduler-delay-complete");
        m_State = State::GraphSchedulerCleanup;
    }

    void CleanupGraphScheduler() {
        if (!m_Graph) {
            m_State = State::GraphOwnership;
            return;
        }
        const CK_ID entryLinkId = m_GraphEntryLink
            ? m_GraphEntryLink->GetID() : 0;
        const CK_ID immediateLinkId = m_GraphImmediateLink
            ? m_GraphImmediateLink->GetID() : 0;
        const CK_ID delayedLinkId = m_GraphDelayedLink
            ? m_GraphDelayedLink->GetID() : 0;
        const CK_ID exitLinkId = m_GraphExitLink
            ? m_GraphExitLink->GetID() : 0;
        const CK_ID sourceId = m_GraphImmediateSource.Behavior
            ? m_GraphImmediateSource.Behavior->GetID() : 0;
        const CK_ID immediateDestinationId = m_GraphImmediateDestination.Behavior
            ? m_GraphImmediateDestination.Behavior->GetID() : 0;
        const CK_ID delayedDestinationId = m_GraphDelayedDestination.Behavior
            ? m_GraphDelayedDestination.Behavior->GetID() : 0;
        const ContextSnapshot before = CaptureContext(m_Context);
        m_Runtime.ResetWorld();
        if (!ContextRestored(m_Context, before))
            Fail("graph-scheduler-reset-context");
        const bool graphClean = m_Graph->GetSubBehaviorCount() == 0 &&
            m_Graph->GetSubBehaviorLinkCount() == 0 &&
            (!entryLinkId || m_Context->GetObject(entryLinkId) == nullptr) &&
            (!immediateLinkId || m_Context->GetObject(immediateLinkId) == nullptr) &&
            (!delayedLinkId || m_Context->GetObject(delayedLinkId) == nullptr) &&
            (!exitLinkId || m_Context->GetObject(exitLinkId) == nullptr) &&
            (!sourceId || m_Context->GetObject(sourceId) == nullptr) &&
            (!immediateDestinationId ||
             m_Context->GetObject(immediateDestinationId) == nullptr) &&
            (!delayedDestinationId ||
             m_Context->GetObject(delayedDestinationId) == nullptr);
        if (!graphClean)
            Fail("graph-link-cleanup");
        m_GraphEntryLink = nullptr;
        m_GraphImmediateLink = nullptr;
        m_GraphDelayedLink = nullptr;
        m_GraphExitLink = nullptr;
        m_GraphImmediateSource.Behavior = nullptr;
        m_GraphImmediateDestination.Behavior = nullptr;
        m_GraphDelayedDestination.Behavior = nullptr;
        if (CKScene *scene = m_Context->GetCurrentScene())
            scene->DeActivate(m_Graph);
        (void) m_Owner->RemoveScript(m_Graph->GetID());
        m_Context->DestroyObject(m_Graph);
        m_Graph = nullptr;
        m_State = State::GraphOwnership;
    }

    void CheckGraphOwnership() {
        auto *graph = static_cast<CKBehavior *>(m_Context->CreateObject(
            CKCID_BEHAVIOR, nullptr, CK_OBJECTCREATION_DYNAMIC));
        if (!graph) {
            Fail("graph-create");
            m_State = State::LifecycleFixture;
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
        m_State = State::SpliceStart;
    }

    Spec LifecycleSpec() const {
        const int setting = 42;
        const int source = 9;
        Spec spec(BML_LIFECYCLE_FIXTURE_GUID);
        spec.Setting(Slot::Named(SlotKind::Setting, "Value", CKPGUID_INT),
                     Value::From(CKPGUID_INT, setting))
            .Input(Slot::Named(SlotKind::InputParameter, "Source", CKPGUID_INT),
                   Value::From(CKPGUID_INT, source));
        return spec;
    }

    void StartSplice() {
        m_SpliceGraph = static_cast<CKBehavior *>(m_Context->CreateObject(
            CKCID_BEHAVIOR, const_cast<CKSTRING>("__BML_Exact_Splice"),
            CK_OBJECTCREATION_DYNAMIC));
        CKScene *scene = m_Context->GetCurrentScene();
        if (!m_SpliceGraph || !scene) {
            Fail("splice-graph");
            m_State = State::SpliceClose;
            return;
        }
        m_SpliceGraph->UseGraph();
        m_SpliceGraph->SetType(CKBEHAVIORTYPE_SCRIPT);
        if (m_SpliceGraph->SetOwner(m_Owner, FALSE) != CK_OK ||
            !m_SpliceGraph->CreateInput("Start") ||
            !m_SpliceGraph->CreateOutput("Done") ||
            m_Owner->AddScript(m_SpliceGraph) != CK_OK) {
            Fail("splice-graph-layout");
            m_State = State::SpliceClose;
            return;
        }
        AttachResult source = m_Runtime.AddToGraph(
            m_SpliceGraph, LifecycleSpec());
        AttachResult sink = m_Runtime.AddToGraph(
            m_SpliceGraph, LifecycleSpec());
        m_SpliceSource = source.Block;
        m_SpliceSink = sink.Block;
        if (!source || !sink || !m_SpliceSource || !m_SpliceSink) {
            Fail("splice-nodes");
            m_State = State::SpliceClose;
            return;
        }

        const auto link = [&](CKBehaviorIO *from, CKBehaviorIO *to,
                              int delay) -> CKBehaviorLink * {
            auto *value = static_cast<CKBehaviorLink *>(m_Context->CreateObject(
                CKCID_BEHAVIORLINK, nullptr, CK_OBJECTCREATION_DYNAMIC));
            if (!value || value->SetInBehaviorIO(from) != CK_OK ||
                value->SetOutBehaviorIO(to) != CK_OK) {
                if (value)
                    m_Context->DestroyObject(value);
                return nullptr;
            }
            value->SetInitialActivationDelay(delay);
            value->SetActivationDelay(delay);
            if (m_SpliceGraph->AddSubBehaviorLink(value) != CK_OK) {
                m_Context->DestroyObject(value);
                return nullptr;
            }
            return value;
        };
        m_SpliceEntry = link(m_SpliceGraph->GetInput(0),
                             m_SpliceSource->GetInput(0), 0);
        m_SpliceAnchor = link(m_SpliceSource->GetOutput(0),
                              m_SpliceSink->GetInput(0), 6);
        m_SpliceExit = link(m_SpliceSink->GetOutput(0),
                            m_SpliceGraph->GetOutput(0), 0);
        if (!m_SpliceEntry || !m_SpliceAnchor || !m_SpliceExit) {
            Fail("splice-base-links");
            m_State = State::SpliceClose;
            return;
        }
        m_SpliceAnchorId = m_SpliceAnchor->GetID();
        scene->Activate(m_SpliceGraph, TRUE);
        m_SpliceStartFrame = m_LastPlayerFrame;
        m_State = State::SplicePending;
    }

    void ApplySplice() {
        if (!m_SpliceAnchor || !m_SpliceGraph) {
            m_State = State::SpliceClose;
            return;
        }
        const int remaining = m_SpliceAnchor->GetActivationDelay();
        if (remaining >= m_SpliceAnchor->GetInitialActivationDelay() &&
            m_LastPlayerFrame - m_SpliceStartFrame <= 4)
            return;
        if (remaining <= 0 ||
            m_SpliceGraph->GetOutput(0)->IsActive()) {
            Fail("splice-pending-anchor");
            m_State = State::SpliceClose;
            return;
        }
        const Layout layout = m_Runtime.Describe(m_SpliceSource);

        Edit beta;
        Link betaLink;
        if (!m_Editor->Begin(m_SpliceGraph, {"player", "beta"}, beta) ||
            !m_Editor->Use(beta, m_SpliceAnchor, betaLink)) {
            Fail("splice-beta-plan");
            m_State = State::SpliceClose;
            return;
        }
        const Node betaNode = beta.Add(LifecycleSpec(), layout);
        beta.Splice(betaLink, betaNode);
        Status status = m_Editor->Apply(beta, m_SpliceBeta);
        if (!status || !m_SpliceBeta ||
            m_SpliceAnchor->GetActivationDelay() != remaining) {
            Fail("splice-beta-apply");
            m_State = State::SpliceClose;
            return;
        }

        Edit alpha;
        Link alphaLink;
        if (!m_Editor->Begin(m_SpliceGraph, {"player", "alpha"}, alpha) ||
            !m_Editor->Use(alpha, m_SpliceAnchor, alphaLink)) {
            Fail("splice-alpha-plan");
            m_State = State::SpliceClose;
            return;
        }
        const Node alphaNode = alpha.Add(LifecycleSpec(), layout);
        alpha.Splice(alphaLink, alphaNode,
                     {{OrderKind::Before, {"player", "beta"}}});
        status = m_Editor->Apply(alpha, m_SpliceAlpha);
        m_SpliceHead = m_SpliceAnchor->GetOutBehaviorIO();
        const bool ordered = m_SpliceHead &&
            m_SpliceHead->GetOwner() != m_SpliceSink &&
            m_SpliceGraph->GetSubBehaviorCount() == 4 &&
            m_SpliceGraph->GetSubBehaviorLinkCount() == 5 &&
            m_SpliceAnchor->GetID() == m_SpliceAnchorId &&
            m_SpliceAnchor->GetActivationDelay() == remaining;
        if (!status || !m_SpliceAlpha || !ordered) {
            Fail("splice-alpha-apply");
            m_State = State::SpliceClose;
            return;
        }
        m_State = State::SpliceWait;
    }

    void ObserveSplice() {
        if (m_SpliceGraph && !m_SpliceGraph->GetOutput(0)->IsActive() &&
            m_LastPlayerFrame - m_SpliceStartFrame <= 12)
            return;
        if (!m_SpliceGraph || !m_SpliceGraph->GetOutput(0)->IsActive())
            Fail("splice-execution");
        m_State = State::SpliceClose;
    }

    void CloseSplice() {
        if (!m_SpliceGraph) {
            m_State = State::AdditiveEditStart;
            return;
        }
        if (CKScene *scene = m_Context->GetCurrentScene())
            scene->DeActivate(m_SpliceGraph);
        bool conflict = false;
        bool restored = false;
        if (m_SpliceAlpha && m_SpliceAnchor && m_SpliceSink && m_SpliceHead) {
            (void) m_SpliceAnchor->SetOutBehaviorIO(m_SpliceSink->GetInput(0));
            const Status rejected = m_Editor->Close(m_SpliceAlpha);
            conflict = rejected.Code == Error::RevertConflict &&
                       static_cast<bool>(m_SpliceAlpha) &&
                       m_SpliceAnchor->GetOutBehaviorIO() ==
                           m_SpliceSink->GetInput(0);
            (void) m_SpliceAnchor->SetOutBehaviorIO(m_SpliceHead);
            const Status closed = m_Editor->Close(m_SpliceAlpha);
            restored = static_cast<bool>(closed) && !m_SpliceAlpha;
        }
        Status betaQueued;
        CKBehaviorIO *betaHead = m_SpliceAnchor
            ? m_SpliceAnchor->GetOutBehaviorIO() : nullptr;
        if (m_SpliceBeta) {
            std::thread closeThread([&] {
                betaQueued = m_Editor->Close(m_SpliceBeta);
            });
            closeThread.join();
        }
        const bool crossThreadQueued = betaQueued && m_SpliceBeta &&
            m_SpliceBeta.State() == PatchState::Closing && m_SpliceAnchor &&
            m_SpliceAnchor->GetOutBehaviorIO() == betaHead;
        m_Editor->ProcessFrame();
        const bool base = crossThreadQueued && !m_SpliceBeta && m_SpliceAnchor &&
            m_SpliceBeta.State() == PatchState::Closed &&
            m_SpliceAnchor->GetID() == m_SpliceAnchorId &&
            m_SpliceAnchor->GetOutBehaviorIO() == m_SpliceSink->GetInput(0) &&
            m_SpliceGraph->GetSubBehaviorCount() == 2 &&
            m_SpliceGraph->GetSubBehaviorLinkCount() == 3;
        m_SplicePassed = conflict && restored && base;
        if (!m_SplicePassed)
            Fail("splice-close");

        for (CKBehaviorLink *link :
             {m_SpliceEntry, m_SpliceAnchor, m_SpliceExit}) {
            if (!link)
                continue;
            m_SpliceGraph->RemoveSubBehaviorLink(link);
            m_Context->DestroyObject(link);
        }
        if (m_SpliceSource)
            (void) m_Runtime.Close(m_SpliceSource);
        if (m_SpliceSink)
            (void) m_Runtime.Close(m_SpliceSink);
        m_Runtime.ProcessFrame();
        (void) m_Owner->RemoveScript(m_SpliceGraph->GetID());
        m_Context->DestroyObject(m_SpliceGraph);
        m_SpliceGraph = nullptr;
        m_SpliceSource = nullptr;
        m_SpliceSink = nullptr;
        m_SpliceEntry = nullptr;
        m_SpliceAnchor = nullptr;
        m_SpliceExit = nullptr;
        m_State = State::AdditiveEditStart;
    }

    void StartAdditiveEdit() {
        if (!m_Editor) {
            Fail("additive-edit-adapter");
            m_State = State::LifecycleFixture;
            return;
        }
        m_EditFixture = static_cast<CKBehavior *>(m_Context->CreateObject(
            CKCID_BEHAVIOR, const_cast<CKSTRING>("__BML_Additive_Edit"),
            CK_OBJECTCREATION_DYNAMIC));
        CKScene *scene = m_Context->GetCurrentScene();
        if (!m_EditFixture) {
            Fail("additive-edit-graph");
            m_State = State::LifecycleFixture;
            return;
        }
        m_EditFixture->UseGraph();
        m_EditFixture->SetType(CKBEHAVIORTYPE_SCRIPT);
        if (m_EditFixture->SetOwner(m_Owner, FALSE) != CK_OK ||
            !m_EditFixture->CreateInput("Start") ||
            !m_EditFixture->CreateOutput("Done") || !scene ||
            m_Owner->AddScript(m_EditFixture) != CK_OK) {
            Fail("additive-edit-graph-layout");
            m_Context->DestroyObject(m_EditFixture);
            m_EditFixture = nullptr;
            m_State = State::LifecycleFixture;
            return;
        }

        AttachResult source = m_Runtime.AddToGraph(
            m_EditFixture, LifecycleSpec());
        m_EditSource = source.Block;
        if (!source || !m_EditSource) {
            Fail("additive-edit-source");
            m_State = State::AdditiveEditClose;
            return;
        }
        const Layout fixtureLayout = m_Runtime.Describe(m_EditSource);

        AttachResult peer = m_Runtime.AddToGraph(
            m_EditFixture, LifecycleSpec());
        CKBehavior *peerBlock = peer.Block;
        CKParameterIn *existingPin = m_EditSource->GetInputParameter(0);
        CKParameterIn *peerPin = peerBlock
            ? peerBlock->GetInputParameter(0) : nullptr;
        CKParameter *previousDirect = existingPin
            ? existingPin->GetDirectSource() : nullptr;
        CKParameterIn *previousShared = existingPin
            ? existingPin->GetSharedSource() : nullptr;
        const auto closePeer = [&] {
            if (existingPin) {
                if (previousShared)
                    (void) existingPin->ShareSourceWith(previousShared);
                else
                    (void) existingPin->SetDirectSource(previousDirect);
            }
            if (peerBlock)
                (void) m_Runtime.Close(peerBlock);
            m_Runtime.ProcessFrame();
        };
        if (!peer || !peerBlock || !existingPin || !peerPin ||
            existingPin->ShareSourceWith(peerPin) != CK_OK) {
            closePeer();
            Fail("additive-edit-shared-baseline");
            m_State = State::AdditiveEditClose;
            return;
        }
        Edit sharedCycle;
        Node sharedSource;
        Node sharedPeer;
        if (!m_Editor->Begin(m_EditFixture,
                             {"player", "shared-cycle-rejected"},
                             sharedCycle) ||
            !m_Editor->Use(sharedCycle, m_EditSource, sharedSource) ||
            !m_Editor->Use(sharedCycle, peerBlock, sharedPeer)) {
            closePeer();
            Fail("additive-edit-shared-plan");
            m_State = State::AdditiveEditClose;
            return;
        }
        sharedCycle.Share(sharedPeer.Pin("Source"),
                          sharedSource.Pin("Source"));
        Patch sharedPatch;
        const int sharedNodesBefore = m_EditFixture->GetSubBehaviorCount();
        const int sharedLinksBefore = m_EditFixture->GetSubBehaviorLinkCount();
        const Status sharedStatus = m_Editor->Apply(sharedCycle, sharedPatch);
        const bool sharedRejected =
            sharedStatus.Code == Error::SharedSourceCycle && !sharedPatch &&
            m_EditFixture->GetSubBehaviorCount() == sharedNodesBefore &&
            m_EditFixture->GetSubBehaviorLinkCount() == sharedLinksBefore;
        closePeer();
        if (!sharedRejected || m_EditFixture->GetSubBehaviorCount() != 1) {
            Fail("additive-edit-shared-cycle-guard");
            m_State = State::AdditiveEditClose;
            return;
        }

        Edit rejected;
        Node rejectedSource;
        if (!m_Editor->Begin(m_EditFixture, {"player", "cycle-rejected"},
                             rejected) ||
            !m_Editor->Use(rejected, m_EditSource, rejectedSource)) {
            Fail("additive-edit-rejected-plan");
            m_State = State::AdditiveEditClose;
            return;
        }
        const Node rejectedNode = rejected.Add(LifecycleSpec(), fixtureLayout);
        rejected.Flow(rejectedSource.Out(), rejectedNode.In(), 0,
                      Cycle::Confirmed);
        rejected.Flow(rejectedNode.Out(), rejectedSource.In());
        Patch rejectedPatch;
        const int nodesBefore = m_EditFixture->GetSubBehaviorCount();
        const int linksBefore = m_EditFixture->GetSubBehaviorLinkCount();
        const Status rejectedStatus = m_Editor->Apply(rejected, rejectedPatch);
        if (rejectedStatus.Code != Error::UnconfirmedSameFrameCycle ||
            rejectedPatch ||
            m_EditFixture->GetSubBehaviorCount() != nodesBefore ||
            m_EditFixture->GetSubBehaviorLinkCount() != linksBefore) {
            Fail("additive-edit-cycle-guard");
            m_State = State::AdditiveEditClose;
            return;
        }

        Edit edit;
        Node sourceNode;
        if (!m_Editor->Begin(m_EditFixture, {"player", "additive"}, edit) ||
            !m_Editor->Use(edit, m_EditSource, sourceNode)) {
            Fail("additive-edit-plan");
            m_State = State::AdditiveEditClose;
            return;
        }
        const Node added = edit.Add(LifecycleSpec(), fixtureLayout);
        const Port sourceCycleIn = edit.AppendIn(sourceNode, "Cycle In");
        const Port sourceCycleOut = edit.AppendOut(sourceNode, "Cycle Out");
        const Port sourcePin = edit.AppendPin(
            sourceNode, "Extra Pin", CKPGUID_INT);
        const Port sourcePout = edit.AppendPout(
            sourceNode, "Extra Pout", CKPGUID_INT);
        const Port addedCycleIn = edit.AppendIn(added, "Cycle In");
        const Port addedCycleOut = edit.AppendOut(added, "Cycle Out");
        const Port addedPin = edit.AppendPin(
            added, "Extra Pin", CKPGUID_INT);
        const Port addedPout = edit.AppendPout(
            added, "Extra Pout", CKPGUID_INT);

        edit.Flow(edit.Entry("Start"), sourceNode.In("In"));
        edit.Flow(sourceNode.Out("Out"), added.In("In"), 0,
                  Cycle::Confirmed);
        edit.Flow(added.Out("Out"), edit.Exit("Done"));
        edit.Flow(sourceCycleOut, addedCycleIn, 0, Cycle::Confirmed);
        edit.Flow(addedCycleOut, sourceCycleIn, 0, Cycle::Confirmed);
        const int value = 12;
        edit.Bind(sourcePin, Value::From(CKPGUID_INT, value));
        edit.Share(addedPin, sourceNode.Pin("Source"));
        edit.Push(sourcePout, addedPout);
        m_EditTap.Editor = m_Editor.get();
        m_EditTap.Target = &m_EditPatch;
        edit.Tap(sourceNode.Out("Out"),
                 HookBlock::Bind(ProbePatchClose, &m_EditTap));

        const Status applied = m_Editor->Apply(edit, m_EditPatch);
        std::string applyFailure;
        if (!applied) {
            applyFailure = "additive-edit-apply-status-" +
                std::to_string(static_cast<int>(applied.Code));
        } else if (!m_EditPatch) {
            applyFailure = "additive-edit-apply-patch";
        } else if (m_EditFixture->GetSubBehaviorCount() != 3) {
            applyFailure = "additive-edit-apply-nodes-" +
                std::to_string(m_EditFixture->GetSubBehaviorCount());
        } else if (m_EditFixture->GetSubBehaviorLinkCount() != 6) {
            applyFailure = "additive-edit-apply-links-" +
                std::to_string(m_EditFixture->GetSubBehaviorLinkCount());
        } else if (m_EditSource->GetInputCount() != 2) {
            applyFailure = "additive-edit-apply-ins-" +
                std::to_string(m_EditSource->GetInputCount());
        } else if (m_EditSource->GetOutputCount() != 2) {
            applyFailure = "additive-edit-apply-outs-" +
                std::to_string(m_EditSource->GetOutputCount());
        } else if (m_EditSource->GetInputParameterCount() != 2) {
            applyFailure = "additive-edit-apply-pins-" +
                std::to_string(m_EditSource->GetInputParameterCount());
        } else if (m_EditSource->GetOutputParameterCount() != 1) {
            applyFailure = "additive-edit-apply-pouts-" +
                std::to_string(m_EditSource->GetOutputParameterCount());
        } else if (m_Editor->TopologyFingerprint(m_EditFixture) == 0) {
            applyFailure = "additive-edit-apply-topology";
        }
        if (!applyFailure.empty()) {
            Fail(applyFailure.c_str());
            m_State = State::AdditiveEditClose;
            return;
        }

        if (!m_Editor->Begin(m_EditFixture, {"player", "queued"},
                             m_QueuedEdit)) {
            Fail("additive-edit-queued-plan");
            m_State = State::AdditiveEditClose;
            return;
        }
        (void) m_QueuedEdit.Add(LifecycleSpec(), fixtureLayout);
        m_EditTap.Candidate = &m_QueuedEdit;
        m_EditTap.CandidatePatch = &m_QueuedPatch;
        m_EditTap.Graph = m_EditFixture;
        m_EditTap.NodesBefore = m_EditFixture->GetSubBehaviorCount();

        scene->Activate(m_EditFixture, TRUE);
        m_EditStartFrame = m_LastPlayerFrame;
        m_State = State::AdditiveEditWait;
    }

    void ObserveAdditiveEdit() {
        if (m_EditTap.Calls == 0 &&
            m_LastPlayerFrame - m_EditStartFrame <= 3)
            return;
        m_Runtime.ProcessFrame();
        if (m_EditTap.Calls != 1)
            Fail("additive-edit-safe-point-calls");
        if (!m_EditTap.InvocationObserved)
            Fail("additive-edit-safe-point-invocation");
        if (!m_EditTap.Queued)
            Fail("additive-edit-safe-point-admission");
        if (m_EditPatch.State() != PatchState::Closed)
            Fail("additive-edit-safe-point-close");
        if (!m_EditTap.ApplyQueued || !m_EditTap.MutationDeferred)
            Fail("additive-edit-safe-point-apply-admission");
        const bool queuedApplied =
            m_QueuedPatch.State() == PatchState::Active && m_EditFixture &&
            m_EditFixture->GetSubBehaviorCount() == 2;
        if (!queuedApplied)
            Fail("additive-edit-safe-point-apply");
        const Status queuedClosed = m_QueuedPatch
            ? m_Editor->Close(m_QueuedPatch) : Status{};
        if (!queuedClosed || m_QueuedPatch || !m_EditFixture ||
            m_EditFixture->GetSubBehaviorCount() != 1) {
            Fail("additive-edit-safe-point-apply-close");
        }
        m_State = State::AdditiveEditClose;
    }

    void CloseAdditiveEdit() {
        if (!m_EditFixture) {
            m_State = State::LifecycleFixture;
            return;
        }
        if (CKScene *scene = m_Context->GetCurrentScene())
            scene->DeActivate(m_EditFixture);
        const Status closed = m_EditPatch
            ? m_Editor->Close(m_EditPatch) : Status{};
        m_Runtime.ProcessFrame();
        std::string closeFailure;
        if (!closed) {
            closeFailure = "additive-edit-close-status-" +
                std::to_string(static_cast<int>(closed.Code));
        } else if (m_EditPatch) {
            closeFailure = "additive-edit-close-patch";
        } else if (!m_EditSource) {
            closeFailure = "additive-edit-close-source";
        } else if (m_EditFixture->GetSubBehaviorCount() != 1) {
            closeFailure = "additive-edit-close-nodes-" +
                std::to_string(m_EditFixture->GetSubBehaviorCount());
        } else if (m_EditFixture->GetSubBehaviorLinkCount() != 0) {
            closeFailure = "additive-edit-close-links-" +
                std::to_string(m_EditFixture->GetSubBehaviorLinkCount());
        } else if (m_EditSource->GetInputCount() != 1) {
            closeFailure = "additive-edit-close-ins-" +
                std::to_string(m_EditSource->GetInputCount());
        } else if (m_EditSource->GetOutputCount() != 1) {
            closeFailure = "additive-edit-close-outs-" +
                std::to_string(m_EditSource->GetOutputCount());
        } else if (m_EditSource->GetInputParameterCount() != 1) {
            closeFailure = "additive-edit-close-pins-" +
                std::to_string(m_EditSource->GetInputParameterCount());
        } else if (m_EditSource->GetOutputParameterCount() != 0) {
            closeFailure = "additive-edit-close-pouts-" +
                std::to_string(m_EditSource->GetOutputParameterCount());
        } else if (m_Editor->TopologyFingerprint(m_EditFixture) != 0) {
            closeFailure = "additive-edit-close-topology";
        }
        const bool reverted = closeFailure.empty();
        if (!reverted)
            Fail(closeFailure.c_str());
        m_AdditiveEditPassed = m_SplicePassed && reverted &&
                               m_EditTap.Calls == 1 &&
                               m_EditTap.InvocationObserved &&
                               m_EditTap.Queued &&
                               m_EditTap.ApplyQueued &&
                               m_EditTap.MutationDeferred;

        if (m_EditSource)
            (void) m_Runtime.Close(m_EditSource);
        m_Runtime.ProcessFrame();
        (void) m_Owner->RemoveScript(m_EditFixture->GetID());
        m_Context->DestroyObject(m_EditFixture);
        m_EditFixture = nullptr;
        m_EditSource = nullptr;
        m_State = State::LifecycleFixture;
    }

    void CheckLifecycleFixture() {
        HMODULE module = ::GetModuleHandleA("BehaviorLifecycleFixture.dll");
        auto resetTrace = module ? reinterpret_cast<BMLLifecycleFixtureResetTraceFn>(
            ::GetProcAddress(module, "BMLLifecycleFixtureResetTrace")) : nullptr;
        auto setMode = module ? reinterpret_cast<BMLLifecycleFixtureSetModeFn>(
            ::GetProcAddress(module, "BMLLifecycleFixtureSetMode")) : nullptr;
        auto setCloseHook = module
            ? reinterpret_cast<BMLLifecycleFixtureSetCloseHookFn>(
                  ::GetProcAddress(module, "BMLLifecycleFixtureSetCloseHook"))
            : nullptr;
        auto readTrace = module ? reinterpret_cast<BMLLifecycleFixtureReadTraceFn>(
            ::GetProcAddress(module, "BMLLifecycleFixtureReadTrace")) : nullptr;
        if (!resetTrace || !setMode || !setCloseHook || !readTrace) {
            Fail("lifecycle-fixture-exports");
            Finish();
            return;
        }

        auto makeGraph = [&]() -> CKBehavior * {
            auto *graph = static_cast<CKBehavior *>(m_Context->CreateObject(
                CKCID_BEHAVIOR, nullptr, CK_OBJECTCREATION_DYNAMIC));
            if (!graph)
                return nullptr;
            graph->UseGraph();
            if (graph->SetOwner(m_Owner, FALSE) != CK_OK ||
                !graph->CreateInput("In") || !graph->CreateOutput("Out")) {
                m_Context->DestroyObject(graph);
                return nullptr;
            }
            return graph;
        };
        auto makeSpec = [&]() {
            const int setting = 42;
            const int source = 9;
            Spec spec(BML_LIFECYCLE_FIXTURE_GUID);
            spec.Setting(Slot::Named(SlotKind::Setting, "Value", CKPGUID_INT),
                         Value::From(CKPGUID_INT, setting))
                .Input(Slot::Named(SlotKind::InputParameter, "Source", CKPGUID_INT),
                       Value::From(CKPGUID_INT, source));
            return spec;
        };
        auto addLinks = [&](CKBehavior *graph, CKBehavior *block) {
            if (!graph || !block || !graph->GetInput(0) || !graph->GetOutput(0) ||
                !block->GetInput(0) || !block->GetOutput(0)) {
                return false;
            }
            CKBehaviorLink *entry = CreateBehaviorLink(
                m_Context, graph->GetInput(0), block->GetInput(0), 0);
            CKBehaviorLink *exit = CreateBehaviorLink(
                m_Context, block->GetOutput(0), graph->GetOutput(0), 0);
            bool entryAdded = false;
            bool exitAdded = false;
            if (entry)
                entryAdded = graph->AddSubBehaviorLink(entry) == CK_OK;
            if (exit)
                exitAdded = graph->AddSubBehaviorLink(exit) == CK_OK;
            if (!entryAdded || !exitAdded) {
                if (entryAdded)
                    entry = graph->RemoveSubBehaviorLink(entry);
                if (exitAdded)
                    exit = graph->RemoveSubBehaviorLink(exit);
                if (entry)
                    m_Context->DestroyObject(entry);
                if (exit)
                    m_Context->DestroyObject(exit);
                return false;
            }
            return true;
        };
        auto read = [&]() {
            BMLLifecycleFixtureTrace trace;
            trace.Size = sizeof(trace);
            if (!readTrace(&trace))
                trace.EventCount = 0;
            return trace;
        };
        auto messagesAre = [](const BMLLifecycleFixtureTrace &trace,
                              std::initializer_list<CKDWORD> messages) {
            if (trace.EventCount != messages.size())
                return false;
            std::size_t index = 0;
            for (CKDWORD message : messages) {
                if (trace.Events[index++].Message != message)
                    return false;
            }
            return true;
        };
        auto teardownVisible = [](const BMLLifecycleFixtureTrace &trace,
                                  CKDWORD message) {
            for (std::uint32_t index = 0; index < trace.EventCount; ++index) {
                const BMLLifecycleFixtureEvent &event = trace.Events[index];
                if (event.Message == message) {
                    return event.OwnerVisible && event.ParentVisible &&
                           event.LinkVisible && event.SourceVisible;
                }
            }
            return false;
        };

        bool normalPassed = false;
        resetTrace();
        setMode(BMLLifecycleFixtureMode::Normal);
        setCloseHook(nullptr, nullptr);
        CKBehavior *normalGraph = makeGraph();
        AttachResult normal = normalGraph
            ? m_Runtime.AddToGraph(normalGraph, makeSpec()) : AttachResult{};
        int normalized = 0;
        if (normal && normal.Block)
            normal.Block->GetLocalParameterValue(0, &normalized);
        const bool normalLinks = normal && addLinks(normalGraph, normal.Block);
        const Status normalClose = normal
            ? m_Runtime.Close(normal.Block) : Status{};
        const BMLLifecycleFixtureTrace normalTrace = read();
        normalPassed = normal && normalLinks && normalClose && normalized == 77 &&
            normalTrace.SettingsEditedObserved == 42 &&
            normalTrace.FinalNormalizedValue == 77 &&
            messagesAre(normalTrace,
                        {CKM_BEHAVIORCREATE, CKM_BEHAVIORATTACH,
                         CKM_BEHAVIORSETTINGSEDITED, CKM_BEHAVIOREDITED,
                         CKM_BEHAVIORDETACH, CKM_BEHAVIORDELETE}) &&
            teardownVisible(normalTrace, CKM_BEHAVIORDETACH) &&
            teardownVisible(normalTrace, CKM_BEHAVIORDELETE) &&
            normalGraph && normalGraph->GetSubBehaviorCount() == 0 &&
            normalGraph->GetSubBehaviorLinkCount() == 0;
        if (normalGraph)
            m_Context->DestroyObject(normalGraph);

        bool resetPassed = false;
        resetTrace();
        CKBehavior *resetGraph = makeGraph();
        AttachResult resetBlock = resetGraph
            ? m_Runtime.AddToGraph(resetGraph, makeSpec()) : AttachResult{};
        const bool resetLinks = resetBlock &&
            addLinks(resetGraph, resetBlock.Block);
        m_Runtime.ResetWorld();
        const BMLLifecycleFixtureTrace resetResult = read();
        resetPassed = resetBlock && resetLinks &&
            messagesAre(resetResult,
                        {CKM_BEHAVIORCREATE, CKM_BEHAVIORATTACH,
                         CKM_BEHAVIORSETTINGSEDITED, CKM_BEHAVIOREDITED,
                         CKM_BEHAVIORRESET, CKM_BEHAVIORDETACH,
                         CKM_BEHAVIORDELETE}) &&
            teardownVisible(resetResult, CKM_BEHAVIORRESET) &&
            teardownVisible(resetResult, CKM_BEHAVIORDETACH) &&
            teardownVisible(resetResult, CKM_BEHAVIORDELETE) &&
            resetGraph && resetGraph->GetSubBehaviorCount() == 0 &&
            resetGraph->GetSubBehaviorLinkCount() == 0;
        if (resetGraph)
            m_Context->DestroyObject(resetGraph);

        resetTrace();
        setMode(BMLLifecycleFixtureMode::CloseOnEdited);
        setCloseHook(CloseLifecycleFixture, &m_Runtime);
        CKBehavior *selfCloseGraph = makeGraph();
        AttachResult selfClosed = selfCloseGraph
            ? m_Runtime.AddToGraph(selfCloseGraph, makeSpec()) : AttachResult{};
        m_Runtime.ProcessFrame();
        const BMLLifecycleFixtureTrace selfCloseTrace = read();
        const bool selfClosePassed = !selfClosed &&
            selfCloseTrace.CloseHookCalls == 1 &&
            selfCloseTrace.CloseHookAccepted == 1 &&
            messagesAre(selfCloseTrace,
                        {CKM_BEHAVIORCREATE, CKM_BEHAVIORATTACH,
                         CKM_BEHAVIORSETTINGSEDITED, CKM_BEHAVIOREDITED,
                         CKM_BEHAVIORDETACH, CKM_BEHAVIORDELETE}) &&
            selfCloseGraph && selfCloseGraph->GetSubBehaviorCount() == 0;
        setCloseHook(nullptr, nullptr);
        setMode(BMLLifecycleFixtureMode::Normal);
        if (selfCloseGraph)
            m_Context->DestroyObject(selfCloseGraph);

        m_LifecyclePassed = normalPassed && resetPassed && selfClosePassed;
        if (!m_LifecyclePassed) {
            if (!normalPassed)
                Fail("lifecycle-normal");
            if (!resetPassed)
                Fail("lifecycle-reset");
            if (!selfClosePassed)
                Fail("lifecycle-self-close");
        }
        Finish();
    }

    void Finish() { m_State = State::Complete; }

    CKContext *m_Context = nullptr;
    CK3dObject *m_Owner = nullptr;
    Runtime m_Runtime;
    Runtime m_ConsumerRuntime;
    std::unique_ptr<GraphSource> m_EditGraph;
    std::unique_ptr<CKEdit> m_Editor;
    State m_State = State::StaticChecks;
    int m_LastPlayerFrame = -1;
    std::ostringstream m_Failures;
    ExecutionProbe m_Retry;
    ExecutionProbe m_Breakpoint;
    ExecutionProbe m_TerminalPulse;
    ReentrantPulseProbe m_ReentrantPulse;
    CountedExecutionProbe m_LatestFrames;
    CountedExecutionProbe m_FullFrames;
    RecursivePumpProbe m_RecursivePump;
    ReentrantReleaseProbe m_ReentrantRelease;
    SelfDeleteProbe m_SelfDelete;
    ExecutionProbe m_GraphImmediateSource;
    ExecutionProbe m_GraphImmediateDestination;
    ExecutionProbe m_GraphDelayedDestination;
    ExecutionProbe m_DetachedOuter;
    ExecutionProbe m_DetachedSource;
    ExecutionProbe m_DetachedDestination;
    Instance m_RetryInstance;
    Instance m_BreakInstance;
    Instance m_WaitAllInstance;
    Instance m_TerminalPulseInstance;
    Instance m_ReentrantPulseInstance;
    Instance m_LatestFrameInstance;
    Instance m_FullFrameInstance;
    Instance m_DetachedGraphInstance;
    Instance m_RecursivePumpInstance;
    Instance m_ReentrantReleaseInstance;
    Instance m_SelfDeleteInstance;
    Instance m_OperationInstance;
    Instance m_ConsumerInstance;
    std::unique_ptr<Runtime> m_ClosingRuntime;
    Instance m_ClosingProducerInstance;
    Instance m_ClosingConsumerInstance;
    CK_ID m_ReentrantReleaseId = 0;
    CKGUID m_Addition = CKGUID();
    CKParameter *m_OperationOutput = nullptr;
    CKParameterIn *m_ConsumerMagnitude = nullptr;
    CK_ID m_OperationId = 0;
    CK_ID m_OperationProducerId = 0;
    CKParameter *m_ClosingOutput = nullptr;
    CKParameterIn *m_ClosingConsumerMagnitude = nullptr;
    CK_ID m_ClosingOperationId = 0;
    CK_ID m_ClosingProducerId = 0;
    CKBehavior *m_Graph = nullptr;
    CKBehaviorLink *m_GraphEntryLink = nullptr;
    CKBehaviorLink *m_GraphImmediateLink = nullptr;
    CKBehaviorLink *m_GraphDelayedLink = nullptr;
    CKBehaviorLink *m_GraphExitLink = nullptr;
    CKBehavior *m_DetachedGraph = nullptr;
    CKBehaviorLink *m_DetachedEntryLink = nullptr;
    CKBehaviorLink *m_DetachedDelayLink = nullptr;
    CKBehaviorLink *m_DetachedExitLink = nullptr;
    int m_DetachedGraphFrames = 0;
    int m_GraphStartFrame = -1;
    CKBehavior *m_EditFixture = nullptr;
    CKBehavior *m_EditSource = nullptr;
    Edit m_QueuedEdit;
    Patch m_EditPatch;
    Patch m_QueuedPatch;
    PatchCloseProbe m_EditTap;
    int m_EditStartFrame = -1;
    bool m_AdditiveEditPassed = false;
    CKBehavior *m_SpliceGraph = nullptr;
    CKBehavior *m_SpliceSource = nullptr;
    CKBehavior *m_SpliceSink = nullptr;
    CKBehaviorLink *m_SpliceEntry = nullptr;
    CKBehaviorLink *m_SpliceAnchor = nullptr;
    CKBehaviorLink *m_SpliceExit = nullptr;
    CKBehaviorIO *m_SpliceHead = nullptr;
    CK_ID m_SpliceAnchorId = 0;
    Patch m_SpliceAlpha;
    Patch m_SpliceBeta;
    int m_SpliceStartFrame = -1;
    bool m_SplicePassed = false;
    bool m_LifecyclePassed = false;
};

BehaviorRuntimeSemantics::BehaviorRuntimeSemantics(CKContext *context, CK3dObject *owner)
    : m_Impl(std::make_unique<Impl>(context, owner)) {}

BehaviorRuntimeSemantics::~BehaviorRuntimeSemantics() = default;

void BehaviorRuntimeSemantics::Advance(int playerFrame) {
    m_Impl->Advance(playerFrame);
}

bool BehaviorRuntimeSemantics::Done() const {
    return m_Impl->Done();
}

BehaviorRuntimeSemanticsResult BehaviorRuntimeSemantics::Result() const {
    return m_Impl->Result();
}

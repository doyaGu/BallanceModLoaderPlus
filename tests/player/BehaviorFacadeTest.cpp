// Exercises the published Script, Patch, Plan, and Hook surface the way a Mod
// author would: only BML/Behavior.hpp, with no private Behavior API.
#include "BehaviorLifecycleFixtureApi.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "BML/Behavior.hpp"
#include "BML/IMod.h"
#include "CKAll.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <thread>
#include <variant>

#include "PlayerProbe.h"

namespace {

using BML::Behavior::Hook;
using BML::Behavior::HookEvent;
using BML::Behavior::HookResult;
using BML::Behavior::PatchState;
using BML::Behavior::PlanState;

// An author's own enumeration. A Value deduces Int from it, so nothing here
// spells out the narrowing the Virtools parameter is going to do anyway.
enum class FacadeMode : std::uint8_t {
    Off = 0,
    On = 3,
};

constexpr const char *kScriptName = "__BML_Public_Plan";
constexpr const char *kSourceName = "Public Plan Source";
constexpr const char *kSinkName = "Public Plan Sink";

std::uint32_t g_RemovalSourceRuns = 0;
std::uint32_t g_RemovalPeerRuns = 0;

int RunPlanNode(const CKBehaviorContext &context) {
    CKBehavior *behavior = context.Behavior;
    if (!behavior)
        return CKBR_BEHAVIORERROR;
    if (behavior->GetName() &&
        std::strcmp(behavior->GetName(), kSourceName) == 0)
        ++g_RemovalSourceRuns;
    for (int index = 0; index < behavior->GetInputCount(); ++index)
        behavior->ActivateInput(index, FALSE);
    for (int index = 0; index < behavior->GetOutputCount(); ++index)
        behavior->ActivateOutput(index);
    return CKBR_OK;
}

int RunActivePeer(const CKBehaviorContext &context) {
    CKBehavior *behavior = context.Behavior;
    if (!behavior)
        return CKBR_BEHAVIORERROR;
    ++g_RemovalPeerRuns;
    behavior->ActivateOutput(0);
    return CKBR_ACTIVATENEXTFRAME;
}

// What the two author callbacks record. The probe holds one reference and each
// callback holds another, so the count returning to one proves the Loader
// released the callback state the Plan owned.
struct Counters {
    std::uint32_t Taps = 0;
    std::uint32_t Afters = 0;
    std::uint32_t Blocks = 0;
    std::uint32_t Scripts = 0;
    std::uint32_t Owners = 0;
    std::uint32_t Frames = 0;
};

class BehaviorFacadeTest final : public IMod {
public:
    explicit BehaviorFacadeTest(IBML *bml) : IMod(bml) {
        AddDependency("BML");
    }

    const char *GetID() override { return "BehaviorFacadeTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "Behavior Facade Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override {
        return "Validates the published Behavior Patch, Plan, and Hook facade";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        BML::PlayerTest::ProbeReport::Reset();
        auto session = BML::Behavior::Session::Open();
        if (!session) {
            Finish(false, "session");
            return;
        }
        m_Session = std::move(session).Value();
    }

    void OnStartLevel() override { m_LevelStarted = true; }

    void OnProcess() override {
        if (m_Done || !m_LevelStarted ||
            (!m_Session && m_State != State::WaitPatchConflict &&
             m_State != State::WaitPatchRetired &&
             m_State != State::WaitScriptClosed))
            return;
        ++m_Frame;
        switch (m_State) {
        case State::Submit: SubmitPlan(); break;
        case State::WaitActive: WaitActive(); break;
        case State::WaitHooks: WaitHooks(); break;
        case State::Close: ClosePlan(); break;
        case State::WaitPlanRetired: WaitPlanRetired(); break;
        case State::WaitReleased: WaitReleased(); break;
        case State::SubmitSelfClose: SubmitSelfClose(); break;
        case State::WaitSelfActive: WaitSelfActive(); break;
        case State::WaitSelfRetired: WaitSelfRetired(); break;
        case State::AttachBlock: AttachBlock(); break;
        case State::WaitAttachRemoved: WaitAttachRemoved(); break;
        case State::AttachContinuing: AttachContinuing(); break;
        case State::WaitContinuationDone: WaitContinuationDone(); break;
        case State::WaitIdentityHook: WaitIdentityHook(); break;
        case State::WaitIdentityRestored: WaitIdentityRestored(); break;
        case State::InstallReplacement: InstallReplacement(); break;
        case State::CloseReplacement: CloseReplacement(); break;
        case State::WaitReplacementRestored: WaitReplacementRestored(); break;
        case State::WaitReplacementRemoved: WaitReplacementRemoved(); break;
        case State::ArmPendingRemoval: ArmPendingRemoval(); break;
        case State::RejectPendingRemoval: RejectPendingRemoval(); break;
        case State::InstallRemoval: InstallRemoval(); break;
        case State::WaitRemovalInactive: WaitRemovalInactive(); break;
        case State::ObserveRemovalIsolation: ObserveRemovalIsolation(); break;
        case State::CloseRemoval: CloseRemoval(); break;
        case State::WaitRemovalRestored: WaitRemovalRestored(); break;
        case State::ClosePatch: ClosePatch(); break;
        case State::WaitPatchConflict: WaitPatchConflict(); break;
        case State::WaitPatchRetired: WaitPatchRetired(); break;
        case State::WaitScriptClosed: WaitScriptClosed(); break;
        }
    }

    void OnUnload() override {
        (void) m_Plan.Close();
        (void) m_SelfPlan.Close();
        (void) m_Patch.Close();
        (void) m_ReplacementPatch.Close();
        (void) m_RemovalPatch.Close();
        if (m_ReplacementOriginal)
            (void) m_ReplacementOriginal->Close();
        DestroyGraph();
        m_Session.Close();
    }

private:
    enum class State {
        Submit,
        WaitActive,
        WaitHooks,
        Close,
        WaitPlanRetired,
        WaitReleased,
        SubmitSelfClose,
        WaitSelfActive,
        WaitSelfRetired,
        AttachBlock,
        WaitAttachRemoved,
        AttachContinuing,
        WaitContinuationDone,
        WaitIdentityHook,
        WaitIdentityRestored,
        InstallReplacement,
        CloseReplacement,
        WaitReplacementRestored,
        WaitReplacementRemoved,
        ArmPendingRemoval,
        RejectPendingRemoval,
        InstallRemoval,
        WaitRemovalInactive,
        ObserveRemovalIsolation,
        CloseRemoval,
        WaitRemovalRestored,
        ClosePatch,
        WaitPatchConflict,
        WaitPatchRetired,
        WaitScriptClosed,
    };

    CKBehaviorLink *AddLink(CKBehaviorIO *source, CKBehaviorIO *sink) {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        auto *link = context ? static_cast<CKBehaviorLink *>(
            context->CreateObject(CKCID_BEHAVIORLINK, nullptr,
                                  CK_OBJECTCREATION_DYNAMIC)) : nullptr;
        if (!link || link->SetInBehaviorIO(source) != CK_OK ||
            link->SetOutBehaviorIO(sink) != CK_OK ||
            m_Graph->AddSubBehaviorLink(link) != CK_OK) {
            if (context && link)
                context->DestroyObject(link);
            return nullptr;
        }
        link->SetInitialActivationDelay(0);
        link->SetActivationDelay(0);
        return link;
    }

    bool AddNode(const char *name, CKBehavior *&out) {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        out = context ? CKBehavior::Cast(context->CreateObject(
            CKCID_BEHAVIOR, const_cast<CKSTRING>(name),
            CK_OBJECTCREATION_DYNAMIC)) : nullptr;
        if (!out)
            return false;
        out->UseFunction();
        out->SetType(CKBEHAVIORTYPE_BASE);
        out->SetFunction(RunPlanNode);
        return out->CreateInput("In") && out->CreateOutput("Out") &&
            m_Graph->AddSubBehavior(out) == CK_OK;
    }

    bool BuildGraph() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        CKLevel *level = context ? context->GetCurrentLevel() : nullptr;
        CKScene *scene = context ? context->GetCurrentScene() : nullptr;
        if (!context || !level || !scene)
            return false;

        m_Owner = CK3dObject::Cast(context->CreateObject(
            CKCID_3DOBJECT, const_cast<CKSTRING>("__BML_Public_Plan_Owner"),
            static_cast<CK_OBJECTCREATION_OPTIONS>(
                CK_OBJECTCREATION_DYNAMIC | CK_OBJECTCREATION_ACTIVATE)));
        if (!m_Owner || level->AddObject(m_Owner) != CK_OK)
            return false;
        if (scene != level->GetLevelScene())
            (void) scene->AddObject(m_Owner);
        scene->Activate(m_Owner, TRUE);

        const int scriptsBefore = m_Owner->GetScriptCount();
        BML::Behavior::Edit rejectedBody;
        (void) rejectedBody.Require("__BML_Missing_Initial_Node");
        auto rejected = m_Session.CreateScript(
            m_Owner, "__BML_Rejected_Script", rejectedBody);
        if (rejected || m_Owner->GetScriptCount() != scriptsBefore)
            return false;
        m_AtomicScriptPassed = true;

        BML::Behavior::Edit shape;
        const auto root = shape.Graph();
        (void) shape.AppendIn(root, "Start");
        (void) shape.AppendOut(root, "Done");
        CKParameterManager *parameters = context->GetParameterManager();
        const CKGUID addition = parameters
            ? parameters->OperationNameToGuid(
                  const_cast<CKSTRING>("Addition"))
            : CKGUID();
        if (!addition.IsValid())
            return false;
        const auto left = shape.AppendLocal(
            root, "Left", CKPGUID_FLOAT);
        const auto replacementSource = shape.AppendLocal(
            root, "Replacement Source", CKPGUID_INT);
        const auto sum = shape.AddOperation(
            addition, CKPGUID_FLOAT, CKPGUID_FLOAT, CKPGUID_FLOAT);
        shape.Bind(left, 2.0f)
            .Bind(replacementSource, 23)
            .Bind(sum.Input(0), left)
            .Bind(sum.Input(1), 3.0f);
        auto created = m_Session.CreateScript(
            m_Owner, kScriptName, shape);
        if (!created)
            return false;
        m_AuthoredScript = std::move(created).Value();
        const auto createdInfo = m_AuthoredScript.Info();
        auto authored = m_AuthoredScript.Inspect();
        if (!createdInfo || createdInfo->State !=
                BML::Behavior::ScriptState::Ready ||
            createdInfo->Active || createdInfo->RequestedActive || !authored ||
            !authored->Root().In("Start") || !authored->Root().Out("Done") ||
            !authored->Root().Local("Left") ||
            authored->Operations().size() != 1 ||
            authored->Operations()[0].Function() != addition ||
            authored->Operations()[0].Result() != CKPGUID_FLOAT ||
            authored->Operations()[0].Input1() != CKPGUID_FLOAT ||
            authored->Operations()[0].Input2() != CKPGUID_FLOAT)
            return false;
        m_ScriptDefined = true;
        m_AuthoredRootId = static_cast<CK_ID>(authored->Root().Id());
        m_Graph = CKBehavior::Cast(context->GetObject(m_AuthoredRootId));
        m_ReplacementSource = m_Graph
            ? m_Graph->GetLocalParameter(1) : nullptr;
        if (!m_Graph || m_Graph->GetOwner() != m_Owner ||
            !m_ReplacementSource ||
            m_Graph->GetType() != CKBEHAVIORTYPE_SCRIPT ||
            !m_Graph->IsInScene(scene) || scene->IsObjectActive(m_Graph))
            return false;
        CKParameterOperation *nativeOperation =
            m_Graph->GetParameterOperationCount() == 1
                ? m_Graph->GetParameterOperation(0) : nullptr;
        float operationResult = 0.0f;
        const CKERROR operationError =
            nativeOperation && nativeOperation->GetOutParameter()
                ? nativeOperation->GetOutParameter()->GetValue(
                      &operationResult, TRUE)
                : CKERR_INVALIDOBJECT;
        if (!nativeOperation ||
            nativeOperation->GetOperationGuid() != addition ||
            !nativeOperation->GetOutParameter() ||
            operationError != CK_OK || operationResult != 5.0f)
            return false;
        m_AuthoredOperationId = nativeOperation->GetID();
        m_OperationPassed = true;

        if (!AddNode(kSourceName, m_Source) || !AddNode(kSinkName, m_Sink))
            return false;
        m_Entry = AddLink(m_Graph->GetInput(0), m_Source->GetInput(0));
        m_Anchor = AddLink(m_Source->GetOutput(0), m_Sink->GetInput(0));
        m_Exit = AddLink(m_Sink->GetOutput(0), m_Graph->GetOutput(0));
        if (!m_Entry || !m_Anchor || !m_Exit)
            return false;
        m_AnchorId = m_Anchor->GetID();
        return m_Graph->GetSubBehaviorCount() == 2 &&
            m_Graph->GetSubBehaviorLinkCount() == 3;
    }

    void DestroyGraph() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        if (!context)
            return;
        if (m_AuthoredScript)
            (void) m_AuthoredScript.Close();
        const bool rootLive = m_AuthoredRootId != 0 &&
            context->GetObject(m_AuthoredRootId) != nullptr;
        m_Graph = nullptr;
        m_Source = nullptr;
        m_Sink = nullptr;
        m_RemovalPeer = nullptr;
        m_RemovalPeerLink = nullptr;
        m_Entry = nullptr;
        m_Anchor = nullptr;
        m_Exit = nullptr;
        m_ReplacementSource = nullptr;
        m_ReplacementOriginalNode = nullptr;
        m_ReplacementInstalledNode = nullptr;
        m_ReplacementEntry = nullptr;
        m_ReplacementExit = nullptr;
        m_AnchorId = 0;
        m_ReplacementOriginalId = 0;
        m_ReplacementInstalledId = 0;
        m_ReplacementEntryId = 0;
        m_ReplacementExitId = 0;
        if (m_Owner && !rootLive)
            context->DestroyObject(m_Owner);
        if (!rootLive) {
            m_Owner = nullptr;
            m_AuthoredRootId = 0;
            m_AuthoredOperationId = 0;
        }
    }

    bool RunGraph() {
        if (!m_Graph || !m_AuthoredScript)
            return false;
        m_Graph->ActivateInput(0, FALSE);
        m_Graph->ActivateOutput(0, FALSE);
        const auto activation = m_AuthoredScript.Activate(false);
        if (!activation || !activation->RequestedActive)
            return false;
        m_Graph->ActivateInput(0, TRUE);
        m_WaitUntil = m_Frame + 30;
        return true;
    }

    bool Installed() const {
        return m_Graph && m_Source && m_Sink && m_Anchor &&
            m_Anchor->GetID() == m_AnchorId &&
            m_Anchor->GetOutBehaviorIO() != m_Sink->GetInput(0) &&
            m_Graph->GetSubBehaviorCount() == 5 &&
            m_Graph->GetSubBehaviorLinkCount() == 6;
    }

    bool Restored() const {
        return m_Graph && m_Source && m_Sink && m_Anchor &&
            m_Anchor->GetID() == m_AnchorId &&
            m_Anchor->GetInBehaviorIO() == m_Source->GetOutput(0) &&
            m_Anchor->GetOutBehaviorIO() == m_Sink->GetInput(0) &&
            m_Graph->GetSubBehaviorCount() == 2 &&
            m_Graph->GetSubBehaviorLinkCount() == 3;
    }

    bool PatchInstalled() const {
        return m_Graph && m_Source && m_Sink && m_Anchor &&
            m_Anchor->GetID() == m_AnchorId &&
            m_Anchor->GetOutBehaviorIO() != m_Sink->GetInput(0) &&
            m_Graph->GetSubBehaviorCount() == 3 &&
            m_Graph->GetSubBehaviorLinkCount() == 4;
    }

    bool PublicViews() {
        auto logical = m_Session.Inspect(m_Graph);
        if (!logical)
            return false;
        auto live = logical->Live();
        if (!live)
            return false;
        const auto logicalAnchor = std::find_if(
            logical->Links().begin(), logical->Links().end(),
            [&](const BML::Behavior::Link &link) {
                return link.Id() == static_cast<std::uint32_t>(m_AnchorId);
            });
        const auto liveAnchor = std::find_if(
            live->Links().begin(), live->Links().end(),
            [&](const BML::Behavior::Link &link) {
                return link.Id() == static_cast<std::uint32_t>(m_AnchorId);
            });
        return logical->Mode() == BML::Behavior::View::Logical &&
            live->Mode() == BML::Behavior::View::Live &&
            // The explicit fixture remains author-visible. Tap and the
            // exit-path After HookBlock do not add Logical nodes or Links.
            logical->Nodes().size() == 4 && logical->Links().size() == 3 &&
            live->Nodes().size() == 6 && live->Links().size() == 6 &&
            logicalAnchor != logical->Links().end() &&
            liveAnchor != live->Links().end() &&
            logicalAnchor->Source().Node() ==
                static_cast<std::uint32_t>(m_Source->GetID()) &&
            logicalAnchor->Target().Node() ==
                static_cast<std::uint32_t>(m_Sink->GetID()) &&
            liveAnchor->Target().Node() != logicalAnchor->Target().Node() &&
            logical->Fingerprint() != live->Fingerprint();
    }

    // The whole authoring program, written the way an author would write it.
    // Nothing here reaches the live graph until Submit accepts all of it.
    void SubmitPlan() {
        if (!BuildGraph()) {
            Finish(false, "graph-create");
            DestroyGraph();
            return;
        }
        auto counters = m_Counters;
        Hook tap([counters]() {
            ++counters->Taps;
            // Ask for one more frame exactly once, which proves the return
            // value reaches Virtools without stalling the chain.
            return counters->Taps == 1 ? HookResult::AgainNextFrame
                                       : HookResult::Ok;
        });
        Hook afterHook([counters](const HookEvent &event) {
            ++counters->Afters;
            if (event.Block.Domain != 0)
                ++counters->Blocks;
            if (event.Script.Domain != 0)
                ++counters->Scripts;
            if (event.Owner.Domain != 0)
                ++counters->Owners;
            if (event.DeltaTime > 0.0f)
                ++counters->Frames;
        });

        const CKGUID fixture(BML_LIFECYCLE_FIXTURE_GUID);
        BML::Behavior::Edit edit;
        const auto source = edit.Require(kSourceName);
        const auto sink = edit.Require(kSinkName);
        const auto link = edit.Between(source.Out(0), sink.In(0));
        edit.Tap(source.Out(0), tap);
        edit.After(source.Out(0), afterHook);
        const auto block = edit.Add(m_Session.Use(fixture));
        (void) edit.AppendIn(block, "Again");
        (void) edit.AppendOut(block, "Finished");
        const auto literal = edit.AppendPin(block, "Literal", CKPGUID_INT);
        edit.Bind(literal, 41);
        edit.Splice(link, block);

        auto submitted = m_Session.Plan(
            "player-public-plan", BML::Behavior::Scripts::One(kScriptName),
            edit);
        if (!submitted) {
            GetLogger()->Error(
                "Behavior plan submit failed: code=%d error=%u phase=%u detail=%s",
                submitted.Code(),
                static_cast<unsigned>(submitted.GetStatus().Error),
                static_cast<unsigned>(submitted.GetStatus().Phase),
                submitted.GetStatus().Message.c_str());
            Finish(false, "submit");
            return;
        }
        m_Plan = std::move(submitted).Value();

        // A Plan the Loader accepted reconciles on the next frame, so it is
        // not installed yet and reads Reconciling with no matches.
        const auto pending = m_Plan.Info();
        m_SubmitPassed = pending && pending->State == PlanState::Reconciling &&
            pending->Matches == 0 && pending->Installations == 0 &&
            !pending->Installed();
        if (!m_SubmitPassed) {
            Finish(false, "submit-state");
            return;
        }
        m_WaitUntil = m_Frame + 30;
        m_State = State::WaitActive;
    }

    void WaitActive() {
        const auto info = m_Plan.Info();
        if (info && info->Installed() && info->Matches == 1 &&
            info->Installations == 1 && info->World != 0) {
            if (!Installed()) {
                Finish(false, "install-shape");
                return;
            }
            if (!PublicViews()) {
                Finish(false, "graph-views");
                return;
            }
            m_InstallPassed = true;
            if (!RunGraph()) {
                Finish(false, "script-activate");
                return;
            }
            m_State = State::WaitHooks;
            return;
        }
        if (m_Frame > m_WaitUntil) {
            GetLogger()->Error(
                "Behavior plan install timed out: read=%d state=%u matches=%u installations=%u",
                info.Code(),
                info ? static_cast<unsigned>(info->State) : 0u,
                info ? info->Matches : 0u, info ? info->Installations : 0u);
            Finish(false, "install");
        }
    }

    void WaitHooks() {
        if (m_Counters->Taps >= 2 && m_Counters->Afters >= 1) {
            m_HookPassed = m_Counters->Blocks == m_Counters->Afters &&
                m_Counters->Scripts == m_Counters->Afters &&
                m_Counters->Owners == m_Counters->Afters &&
                m_Counters->Frames == m_Counters->Afters;
            if (!m_HookPassed) {
                GetLogger()->Error(
                    "Behavior plan hook context incomplete: afters=%u blocks=%u scripts=%u owners=%u frames=%u",
                    m_Counters->Afters, m_Counters->Blocks,
                    m_Counters->Scripts, m_Counters->Owners,
                    m_Counters->Frames);
                Finish(false, "hook-context");
                return;
            }
            m_State = State::Close;
            return;
        }
        if (m_Frame > m_WaitUntil) {
            GetLogger()->Error(
                "Behavior plan hooks timed out: taps=%u afters=%u",
                m_Counters->Taps, m_Counters->Afters);
            Finish(false, "hooks");
        }
    }

    void ClosePlan() {
        BML::Behavior::Result<BML::Behavior::CloseState> closed;
        std::thread closer([&] { closed = m_Plan.Close(); });
        closer.join();
        const auto closing = m_Plan.Info();
        if (!closed || closed.Value() != BML::Behavior::CloseState::Closing ||
            !m_Plan || !closing ||
            closing->State != PlanState::Retiring) {
            Finish(false, "plan-thread-close");
            return;
        }
        m_WaitUntil = m_Frame + 30;
        m_State = State::WaitPlanRetired;
    }

    void WaitPlanRetired() {
        if (!Restored()) {
            if (m_Frame > m_WaitUntil)
                Finish(false, "restore");
            return;
        }
        const auto closed = m_Plan.Close();
        if (!closed || closed.Value() != BML::Behavior::CloseState::Closed ||
            m_Plan) {
            Finish(false, "close-handle");
            return;
        }
        m_ClosePassed = true;
        m_WaitUntil = m_Frame + 30;
        m_State = State::WaitReleased;
    }

    void WaitReleased() {
        if (m_Counters.use_count() == 1) {
            m_ReleasePassed = true;
            m_State = State::SubmitSelfClose;
            return;
        }
        if (m_Frame > m_WaitUntil) {
            GetLogger()->Error(
                "Behavior plan hook state still held: references=%ld",
                static_cast<long>(m_Counters.use_count()));
            Finish(false, "release");
        }
    }

    void SubmitSelfClose() {
        auto state = m_SelfClose;
        Hook hook([this, state]() {
            ++state->Calls;
            const auto closed = m_SelfPlan.Close();
            state->Closing = closed &&
                closed.Value() == BML::Behavior::CloseState::Closing;
        });
        BML::Behavior::Edit edit;
        const auto source = edit.Require(kSourceName);
        edit.Tap(source.Out(0), hook);
        auto submitted = m_Session.Plan(
            "player-public-self-close",
            BML::Behavior::Scripts::One(kScriptName), edit);
        if (!submitted) {
            Finish(false, "self-close-submit");
            return;
        }
        m_SelfPlan = std::move(submitted).Value();
        m_WaitUntil = m_Frame + 30;
        m_State = State::WaitSelfActive;
    }

    void WaitSelfActive() {
        const auto info = m_SelfPlan.Info();
        if (info && info->Installed() && info->Matches == 1 &&
            info->Installations == 1) {
            if (!RunGraph()) {
                Finish(false, "script-reset");
                return;
            }
            m_State = State::WaitSelfRetired;
            return;
        }
        if (m_Frame > m_WaitUntil)
            Finish(false, "self-close-install");
    }

    void WaitSelfRetired() {
        if (m_SelfClose->Calls != 0 && Restored()) {
            const auto closed = m_SelfPlan.Close();
            if (!m_SelfClose->Closing || m_SelfClose->Calls != 1 ||
                !closed || closed.Value() != BML::Behavior::CloseState::Closed ||
                m_SelfPlan || m_SelfClose.use_count() != 1) {
                Finish(false, "self-close-contract");
                return;
            }
            m_SelfClosePassed = true;
            m_State = State::AttachBlock;
            return;
        }
        if (m_Frame > m_WaitUntil)
            Finish(false, "self-close-retirement");
    }

    // A parked Block lives in the graph without being linked to it. The graph
    // never activates it, so the Instance the Mod holds is still what writes
    // its settings and pulses it.
    void AttachBlock() {
        const auto graphRef = m_Session.Reference(m_Graph);
        if (!graphRef || !m_Graph) {
            Finish(false, "attach-reference");
            return;
        }
        m_AttachBlocks = m_Graph->GetSubBehaviorCount();
        m_AttachLinks = m_Graph->GetSubBehaviorLinkCount();
        auto block = m_Session.Use(CKGUID(BML_LIFECYCLE_FIXTURE_GUID));
        block.Settings({{"Value", 9}});
        auto parked = block.SpawnIn(graphRef.Value());
        if (!parked) {
            GetLogger()->Error(
                "Behavior attach failed: code=%d error=%u phase=%u detail=%s",
                parked.Code(),
                static_cast<unsigned>(parked.GetStatus().Error),
                static_cast<unsigned>(parked.GetStatus().Phase),
                parked.GetStatus().Message.c_str());
            Finish(false, "attach");
            return;
        }
        m_Parked = std::move(parked).Value();
        if (!ExistingBlockEdits() || !FailedExistingBlockEdit()) {
            (void) m_Parked->Close();
            m_Parked.reset();
            Finish(false, "existing-block-edits");
            return;
        }
        const bool resident =
            m_Graph->GetSubBehaviorCount() == m_AttachBlocks + 1 &&
            m_Graph->GetSubBehaviorLinkCount() == m_AttachLinks;
        const auto pulsed = m_Parked->Pulse("In");
        const bool ran = pulsed &&
            pulsed.Value() == BML::Behavior::PulseResult::Ran;
        if (!resident || !ran) {
            GetLogger()->Error(
                "Behavior attach shape wrong: resident=%s ran=%s blocks=%d links=%d",
                resident ? "true" : "false", ran ? "true" : "false",
                m_Graph->GetSubBehaviorCount(),
                m_Graph->GetSubBehaviorLinkCount());
            (void) m_Parked->Close();
            m_Parked.reset();
            Finish(false, "attach-shape");
            return;
        }
        const auto closed = m_Parked->Close();
        if (!closed) {
            Finish(false, "attach-close");
            return;
        }
        m_WaitUntil = m_Frame + 30;
        m_State = State::WaitAttachRemoved;
    }

    void WaitAttachRemoved() {
        if (m_Graph &&
            m_Graph->GetSubBehaviorCount() == m_AttachBlocks &&
            m_Graph->GetSubBehaviorLinkCount() == m_AttachLinks) {
            m_Parked.reset();
            m_AttachPassed = true;
            if (m_ContinuationPassed)
                IdentityEdits();
            else
                m_State = State::AttachContinuing;
            return;
        }
        if (m_Frame > m_WaitUntil) {
            GetLogger()->Error(
                "Behavior attach removal timed out: blocks=%d links=%d",
                m_Graph ? m_Graph->GetSubBehaviorCount() : -1,
                m_Graph ? m_Graph->GetSubBehaviorLinkCount() : -1);
            Finish(false, "attach-removal");
        }
    }

    struct LifecycleFixtureExports {
        BMLLifecycleFixtureResetTraceFn ResetTrace = nullptr;
        BMLLifecycleFixtureSetModeFn SetMode = nullptr;
        BMLLifecycleFixtureSetContinuationFn SetContinuation = nullptr;
        BMLLifecycleFixtureReadTraceFn ReadTrace = nullptr;

        explicit operator bool() const {
            return ResetTrace && SetMode && SetContinuation && ReadTrace;
        }
    };

    static LifecycleFixtureExports ResolveLifecycleFixture() {
        LifecycleFixtureExports exports;
        HMODULE module = ::GetModuleHandleA("BehaviorLifecycleFixture.dll");
        if (!module)
            return exports;
        exports.ResetTrace = reinterpret_cast<BMLLifecycleFixtureResetTraceFn>(
            ::GetProcAddress(module, "BMLLifecycleFixtureResetTrace"));
        exports.SetMode = reinterpret_cast<BMLLifecycleFixtureSetModeFn>(
            ::GetProcAddress(module, "BMLLifecycleFixtureSetMode"));
        exports.SetContinuation =
            reinterpret_cast<BMLLifecycleFixtureSetContinuationFn>(
                ::GetProcAddress(module, "BMLLifecycleFixtureSetContinuation"));
        exports.ReadTrace = reinterpret_cast<BMLLifecycleFixtureReadTraceFn>(
            ::GetProcAddress(module, "BMLLifecycleFixtureReadTrace"));
        return exports;
    }

    static const BMLLifecycleFixtureEvent *FindFixtureEvent(
        const BMLLifecycleFixtureTrace &trace, std::uint32_t behavior,
        CKDWORD message, std::uint32_t occurrence) {
        const std::uint32_t capacity = static_cast<std::uint32_t>(
            std::size(trace.Events));
        const std::uint32_t count = (std::min)(trace.EventCount, capacity);
        for (std::uint32_t index = 0; index < count; ++index) {
            const BMLLifecycleFixtureEvent &event = trace.Events[index];
            if (event.BehaviorId != behavior || event.Message != message)
                continue;
            if (occurrence-- == 0)
                return &event;
        }
        return nullptr;
    }

    bool ExistingBlockEdits() {
        const LifecycleFixtureExports fixture = ResolveLifecycleFixture();
        auto graph = m_Session.Inspect(m_Graph);
        if (!fixture || !graph)
            return false;
        const auto found = std::find_if(
            graph->Nodes().begin(), graph->Nodes().end(),
            [](const BML::Behavior::Node &node) {
                return node.Prototype() ==
                    CKGUID(BML_LIFECYCLE_FIXTURE_GUID);
            });
        if (found == graph->Nodes().end())
            return false;
        const std::uint32_t behavior = static_cast<std::uint32_t>(found->Id());

        struct RestoreMode {
            BMLLifecycleFixtureSetModeFn Set = nullptr;
            ~RestoreMode() {
                if (Set)
                    Set(BMLLifecycleFixtureMode::Normal);
            }
        } restoreMode{fixture.SetMode};
        fixture.ResetTrace();
        fixture.SetMode(BMLLifecycleFixtureMode::NormalizeOnEdited);
        BML::Behavior::Edit data;
        const auto existing = data.Use(*found);
        const auto value = data.AppendPin(existing, "Patch Value", CKPGUID_INT);
        data.Bind(value, 83);
        data.Bind(existing.Local("State"), 83);
        auto applied = graph->Apply("player-existing-data", data);
        if (!applied)
            return false;
        BMLLifecycleFixtureTrace trace;
        if (!fixture.ReadTrace(&trace))
            return false;
        const BMLLifecycleFixtureEvent *installed = FindFixtureEvent(
            trace, behavior, CKM_BEHAVIOREDITED, 0);
        const bool installVisible = trace.EditedCount == 1 && installed &&
            installed->OwnerVisible && installed->ParentVisible &&
            !installed->LinkVisible && installed->InputCount == 1 &&
            installed->OutputCount == 1 && installed->PinCount == 2 &&
            installed->PoutCount == 0 && installed->LocalCount == 2 &&
            installed->LocalValue == 83 && installed->BoundSourceCount == 2;
        const auto closed = applied->Close();
        if (!installVisible || !closed || !fixture.ReadTrace(&trace))
            return false;
        const BMLLifecycleFixtureEvent *restored = FindFixtureEvent(
            trace, behavior, CKM_BEHAVIOREDITED, 1);
        const bool restoreVisible = trace.EditedCount == 2 && restored &&
            restored->OwnerVisible && restored->ParentVisible &&
            !restored->LinkVisible && restored->InputCount == 1 &&
            restored->OutputCount == 1 && restored->PinCount == 1 &&
            restored->PoutCount == 0 && restored->LocalCount == 2 &&
            restored->LocalValue == 5 && restored->BoundSourceCount == 1;
        if (!restoreVisible)
            return false;

        fixture.SetMode(BMLLifecycleFixtureMode::Normal);

        graph = m_Session.Inspect(m_Graph);
        if (!graph)
            return false;
        const auto current = std::find_if(
            graph->Nodes().begin(), graph->Nodes().end(),
            [behavior](const BML::Behavior::Node &node) {
                return node.Id() == behavior;
            });
        if (current == graph->Nodes().end())
            return false;

        fixture.ResetTrace();
        BML::Behavior::Edit flow;
        const auto flowed = flow.Use(*current);
        flow.Flow(flow.Graph().In(0), flowed.In(0));
        auto linked = graph->Apply("player-existing-flow", flow);
        if (!linked || !fixture.ReadTrace(&trace) || trace.EditedCount != 0)
            return false;
        const auto unlinked = linked->Close();
        return unlinked && fixture.ReadTrace(&trace) && trace.EditedCount == 0;
    }

    bool FailedExistingBlockEdit() {
        const LifecycleFixtureExports fixture = ResolveLifecycleFixture();
        auto graph = m_Session.Inspect(m_Graph);
        if (!fixture || !graph)
            return false;
        const auto found = std::find_if(
            graph->Nodes().begin(), graph->Nodes().end(),
            [](const BML::Behavior::Node &node) {
                return node.Prototype() ==
                    CKGUID(BML_LIFECYCLE_FIXTURE_GUID);
            });
        if (found == graph->Nodes().end())
            return false;
        const std::uint32_t behavior = static_cast<std::uint32_t>(found->Id());
        struct RestoreMode {
            BMLLifecycleFixtureSetModeFn Set = nullptr;
            ~RestoreMode() {
                if (Set)
                    Set(BMLLifecycleFixtureMode::Normal);
            }
        } restoreMode{fixture.SetMode};

        fixture.ResetTrace();
        fixture.SetMode(BMLLifecycleFixtureMode::FailFirstEdited);
        BML::Behavior::Edit data;
        const auto existing = data.Use(*found);
        data.Bind(existing.Local("State"), 83);
        const auto applied = graph->Apply("player-failed-existing-data", data);
        if (applied)
            return false;

        BMLLifecycleFixtureTrace trace;
        if (!fixture.ReadTrace(&trace) || trace.EditedCount != 2)
            return false;
        const BMLLifecycleFixtureEvent *candidate = FindFixtureEvent(
            trace, behavior, CKM_BEHAVIOREDITED, 0);
        const BMLLifecycleFixtureEvent *restored = FindFixtureEvent(
            trace, behavior, CKM_BEHAVIOREDITED, 1);
        return candidate && candidate->LocalCount == 2 &&
            candidate->LocalValue == 83 && restored &&
            restored->LocalCount == 2 && restored->LocalValue == 5;
    }

    // Keeps the parent script executing every frame, the worst case for a
    // parked Block: Ballanced schedules every ACTIVE sub-behavior, linked or
    // not, so a continuation left flagged would also be run by the graph.
    void KickScript() {
        CKScene *scene = m_BML && m_BML->GetCKContext()
            ? m_BML->GetCKContext()->GetCurrentScene() : nullptr;
        if (!scene || !m_Graph || !m_Owner)
            return;
        scene->Activate(m_Owner, TRUE);
        m_Graph->Activate(TRUE, FALSE);
        m_Graph->ActivateInput(0, TRUE);
    }

    // A multi-frame parked Block keeps itself running with
    // CKBR_ACTIVATENEXTFRAME. The Instance must stay the only driver: one
    // execution per engine frame, so no two recorded RunTimes may coincide
    // even while the parent script runs every frame.
    void AttachContinuing() {
        const LifecycleFixtureExports fixture = ResolveLifecycleFixture();
        if (!fixture) {
            Finish(false, "continuation-fixture-exports");
            return;
        }
        fixture.ResetTrace();
        fixture.SetContinuation(3);
        const auto graphRef = m_Session.Reference(m_Graph);
        if (!graphRef || !m_Graph) {
            Finish(false, "continuation-reference");
            return;
        }
        m_AttachBlocks = m_Graph->GetSubBehaviorCount();
        m_AttachLinks = m_Graph->GetSubBehaviorLinkCount();
        auto parked = m_Session.Use(CKGUID(BML_LIFECYCLE_FIXTURE_GUID))
            .SpawnIn(graphRef.Value());
        if (!parked) {
            GetLogger()->Error(
                "Behavior continuation attach failed: code=%d error=%u detail=%s",
                parked.Code(),
                static_cast<unsigned>(parked.GetStatus().Error),
                parked.GetStatus().Message.c_str());
            Finish(false, "continuation-attach");
            return;
        }
        m_Parked = std::move(parked).Value();
        const auto pulsed = m_Parked->Pulse("In");
        if (!pulsed || pulsed.Value() != BML::Behavior::PulseResult::Ran) {
            Finish(false, "continuation-pulse");
            return;
        }
        m_WaitUntil = m_Frame + 30;
        m_State = State::WaitContinuationDone;
    }

    void WaitContinuationDone() {
        KickScript();
        BMLLifecycleFixtureTrace trace;
        const LifecycleFixtureExports fixture = ResolveLifecycleFixture();
        if (!fixture || !fixture.ReadTrace(&trace)) {
            Finish(false, "continuation-trace");
            return;
        }
        if (trace.RunCount >= 4) {
            fixture.SetContinuation(0);
            bool sameFrame = false;
            for (std::uint32_t i = 0; i < 4 && !sameFrame; ++i)
                for (std::uint32_t j = i + 1; j < 4; ++j)
                    if (trace.RunTimes[i] == trace.RunTimes[j]) {
                        sameFrame = true;
                        break;
                    }
            if (trace.RunCount != 4 || sameFrame) {
                GetLogger()->Error(
                    "Behavior continuation drove the Block %u times, "
                    "same-frame executions=%s",
                    trace.RunCount, sameFrame ? "true" : "false");
                (void) m_Parked->Close();
                m_Parked.reset();
                Finish(false, "continuation-drive");
                return;
            }
            const auto closed = m_Parked->Close();
            if (!closed) {
                Finish(false, "continuation-close");
                return;
            }
            m_ContinuationPassed = true;
            m_WaitUntil = m_Frame + 30;
            m_State = State::WaitAttachRemoved;
            return;
        }
        if (m_Frame > m_WaitUntil) {
            unsigned state = 999u;
            int code = 0;
            std::string detail = "<unreadable>";
            if (m_Parked) {
                const auto info = m_Parked->Info();
                code = info.Code();
                if (info)
                    state = static_cast<unsigned>(info.Value().State);
                detail = info.GetStatus().Message.empty()
                    ? "<empty>" : info.GetStatus().Message;
            }
            GetLogger()->Error(
                "Behavior continuation timed out: runs=%u state=%u code=%d "
                "detail=%s",
                trace.RunCount, state, code, detail.c_str());
            (void) m_Parked->Close();
            m_Parked.reset();
            Finish(false, "continuation-timeout");
        }
    }

    // Everything the identity surface added, written the way an author would:
    // public references instead of a test affordance, post-Apply resolution, a
    // Before Hook, a Redirect, ports appended with no variable-interface flag,
    // and literals that deduce their own Virtools type.
    void IdentityEdits() {
        const auto graphRef = m_Session.Reference(m_Graph);
        if (!graphRef) {
            Finish(false, "identity-reference");
            return;
        }
        // Inspect takes the Behavior itself, so a Mod editing the graph it
        // built needs no name lookup at all.
        const auto opened = m_Session.Inspect(m_Graph);
        if (!opened || opened->Nodes().size() != 3 ||
            opened->Links().size() != 3) {
            Finish(false, "identity-inspect");
            return;
        }

        // A durable Plan installs into scripts that do not exist yet, so it
        // refuses a reference issued against this one live world.
        const auto sourceNode = std::find_if(
            opened->Nodes().begin(), opened->Nodes().end(),
            [](const BML::Behavior::Node &node) {
                return node.Name() == kSourceName;
            });
        const auto sinkNode = std::find_if(
            opened->Nodes().begin(), opened->Nodes().end(),
            [](const BML::Behavior::Node &node) {
                return node.Name() == kSinkName;
            });
        const auto anchorLink = std::find_if(
            opened->Links().begin(), opened->Links().end(),
            [&](const BML::Behavior::Link &link) {
                return link.Id() == static_cast<std::uint32_t>(m_AnchorId);
            });
        if (sourceNode == opened->Nodes().end() ||
            sinkNode == opened->Nodes().end() ||
            anchorLink == opened->Links().end()) {
            Finish(false, "identity-snapshot");
            return;
        }

        BML::Behavior::Edit durable;
        const auto anchored = durable.Use(*sourceNode);
        durable.Tap(anchored.Out(0), Hook([] { return HookResult::Ok; }));
        const auto refused = m_Session.Plan(
            "player-public-identity-plan",
            BML::Behavior::Scripts::One(kScriptName), durable);
        if (refused ||
            refused.GetStatus().Error !=
                BML::Behavior::Error::WorldBoundValue) {
            GetLogger()->Error(
                "Behavior identity plan was not refused: code=%d error=%u",
                refused.Code(),
                static_cast<unsigned>(refused.GetStatus().Error));
            Finish(false, "plan-identity");
            return;
        }

        auto identity = m_Identity;
        Hook interposed([identity](const HookEvent &event) {
            ++identity->Befores;
            if (event.Block.Domain != 0)
                ++identity->Blocks;
        });

        const CKGUID fixture(BML_LIFECYCLE_FIXTURE_GUID);
        auto configured = m_Session.Use(fixture);
        configured.Settings({{"Value", std::int32_t{41}}});
        BML::Behavior::Edit edit;
        const auto sink = edit.Use(*sinkNode);
        const auto anchor = edit.Use(*anchorLink);
        const auto block = edit.Add(configured);
        const auto amount = edit.AppendPin(block, "Amount", CKPGUID_FLOAT);
        const auto mode = edit.AppendPin(block, "Mode", CKPGUID_INT);
        (void) edit.AppendPout(block, "Report", CKPGUID_INT);
        const auto scratch = edit.AppendLocal(block, "Scratch", CKPGUID_INT);
        edit.Bind(amount, 2.5f);
        edit.Bind(mode, FacadeMode::On);
        edit.Bind(scratch, 17);
        edit.Before(anchor, interposed);
        edit.Redirect(anchor, block.In());
        edit.Flow(block.Out(), sink.In());

        const LifecycleFixtureExports fixtureApi = ResolveLifecycleFixture();
        if (!fixtureApi) {
            Finish(false, "identity-fixture-exports");
            return;
        }
        fixtureApi.ResetTrace();
        auto applied = opened->Apply("player-public-identity", edit);
        if (!applied) {
            GetLogger()->Error(
                "Behavior identity apply failed: code=%d error=%u phase=%u detail=%s",
                applied.Code(),
                static_cast<unsigned>(applied.GetStatus().Error),
                static_cast<unsigned>(applied.GetStatus().Phase),
                applied.GetStatus().Message.c_str());
            Finish(false, "identity-apply");
            return;
        }
        m_IdentityPatch = std::move(applied).Value();

        const auto blockRef = m_IdentityPatch.Resolve(block);
        if (!blockRef || !blockRef->Domain) {
            Finish(false, "identity-resolve");
            return;
        }
        const auto installed = m_Session.Inspect(m_Graph);
        if (!installed) {
            Finish(false, "identity-lifecycle-view");
            return;
        }
        const auto installedBlock = std::find_if(
            installed->Nodes().begin(), installed->Nodes().end(),
            [&](const BML::Behavior::Node &node) {
                const BML_ObjectRef object = node.Object();
                return object.Domain == blockRef->Domain &&
                    object.Slot == blockRef->Slot &&
                    object.Generation == blockRef->Generation;
            });
        BMLLifecycleFixtureTrace trace;
        const bool traced = installedBlock != installed->Nodes().end() &&
            fixtureApi.ReadTrace(&trace);
        const BMLLifecycleFixtureEvent *edited = traced
            ? FindFixtureEvent(trace,
                               static_cast<std::uint32_t>(installedBlock->Id()),
                               CKM_BEHAVIOREDITED, 0)
            : nullptr;
        if (!edited || trace.CreateCount != 1 || trace.AttachCount != 1 ||
            trace.SettingsEditedCount != 1 || trace.EditedCount != 1 ||
            !edited->OwnerVisible || !edited->ParentVisible ||
            edited->LinkVisible || edited->InputCount != 1 ||
            edited->OutputCount != 1 || edited->PinCount != 3 ||
            edited->PoutCount != 1 || edited->LocalCount != 3 ||
            edited->BoundSourceCount != 3) {
            Finish(false, "identity-lifecycle-order");
            return;
        }
        if (!IdentityViews(blockRef.Value())) {
            Finish(false, "identity-views");
            return;
        }
        if (!RunGraph()) {
            Finish(false, "script-restart");
            return;
        }
        m_State = State::WaitIdentityHook;
    }

    bool IdentityViews(BML_ObjectRef block) {
        const auto view = m_Session.Inspect(m_Graph);
        if (!view)
            return false;
        // The Before HookBlock and its continuation Link stay infrastructure,
        // so Logical keeps the four Nodes and four Links the author wrote. The
        // Redirect is the exception: it changes where control goes, so the
        // anchor Link now reports the added Block as its destination.
        const auto added = std::find_if(
            view->Nodes().begin(), view->Nodes().end(),
            [&](const BML::Behavior::Node &node) {
                const BML_ObjectRef object = node.Object();
                return object.Domain == block.Domain &&
                    object.Slot == block.Slot &&
                    object.Generation == block.Generation;
            });
        const auto anchor = std::find_if(
            view->Links().begin(), view->Links().end(),
            [&](const BML::Behavior::Link &link) {
                return link.Id() == static_cast<std::uint32_t>(m_AnchorId);
            });
        if (view->Nodes().size() != 4 || view->Links().size() != 4 ||
            added == view->Nodes().end() ||
            anchor == view->Links().end() ||
            anchor->Source().Node() !=
                static_cast<std::uint32_t>(m_Source->GetID()) ||
            anchor->Target().Node() != added->Id() ||
            !(added->Prototype() ==
                CKGUID(BML_LIFECYCLE_FIXTURE_GUID))) {
            GetLogger()->Error(
                "Behavior identity view wrong: nodes=%u links=%u block=%s anchor=%s",
                static_cast<unsigned>(view->Nodes().size()),
                static_cast<unsigned>(view->Links().size()),
                added == view->Nodes().end() ? "missing" : "found",
                anchor == view->Links().end() ? "missing" : "found");
            return false;
        }
        // Each literal reached CK2 as the type its slot holds: float remained
        // Float and the small enumerator became Int. The Setting travelled with
        // creation, so the fixture normalized 41 to its own 77.
        const auto amount = view->Read(added->Pin("Amount"));
        const auto mode = view->Read(added->Pin("Mode"));
        const auto value = view->Read(added->Setting("Value"));
        const auto scratch = view->Read(added->Local("Scratch"));
        const bool deduced = amount && mode && value && scratch &&
            std::holds_alternative<float>(amount->Data) &&
            std::get<float>(amount->Data) == 2.5f &&
            std::holds_alternative<std::int32_t>(mode->Data) &&
            std::get<std::int32_t>(mode->Data) == 3 &&
            std::holds_alternative<std::int32_t>(value->Data) &&
            std::get<std::int32_t>(value->Data) == 77 &&
            std::holds_alternative<std::int32_t>(scratch->Data) &&
            std::get<std::int32_t>(scratch->Data) == 17;
        if (!deduced) {
            GetLogger()->Error(
                "Behavior identity literals wrong: amount=%d mode=%d value=%d",
                amount.Code(), mode.Code(), value.Code());
        }
        return deduced;
    }

    void WaitIdentityHook() {
        if (m_Identity->Befores >= 1) {
            if (m_Identity->Blocks != m_Identity->Befores) {
                GetLogger()->Error(
                    "Behavior Before context incomplete: befores=%u blocks=%u",
                    m_Identity->Befores, m_Identity->Blocks);
                Finish(false, "identity-hook-context");
                return;
            }
            const auto closed = m_IdentityPatch.Close();
            if (!closed) {
                Finish(false, "identity-close-request");
                return;
            }
            m_WaitUntil = m_Frame + 30;
            m_State = State::WaitIdentityRestored;
            return;
        }
        if (m_Frame > m_WaitUntil) {
            GetLogger()->Error("Behavior Before hook timed out: befores=%u",
                               m_Identity->Befores);
            Finish(false, "identity-hook");
        }
    }

    void WaitIdentityRestored() {
        if (!Restored()) {
            if (m_Frame > m_WaitUntil)
                Finish(false, "identity-restore");
            return;
        }
        const auto closed = m_IdentityPatch.Close();
        if (!closed ||
            closed.Value() != BML::Behavior::CloseState::Closed ||
            m_IdentityPatch || m_Identity.use_count() != 1) {
            Finish(false, "identity-close");
            return;
        }
        m_IdentityPassed = true;
        m_State = State::InstallReplacement;
    }

    static bool HasDestination(CKParameterOut *source,
                               CKParameter *destination) {
        if (!source || !destination)
            return false;
        for (int index = 0; index < source->GetDestinationCount(); ++index)
            if (source->GetDestination(index) == destination)
                return true;
        return false;
    }

    static bool HasNode(CKBehavior *graph, CKBehavior *node) {
        if (!graph || !node)
            return false;
        for (int index = 0; index < graph->GetSubBehaviorCount(); ++index)
            if (graph->GetSubBehavior(index) == node)
                return true;
        return false;
    }

    static bool HasLink(CKBehavior *graph, CKBehaviorLink *link) {
        if (!graph || !link)
            return false;
        for (int index = 0; index < graph->GetSubBehaviorLinkCount(); ++index)
            if (graph->GetSubBehaviorLink(index) == link)
                return true;
        return false;
    }

    void InstallReplacement() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        const auto graphRef = m_Session.Reference(m_Graph);
        if (!context || !m_Graph || !m_ReplacementSource || !graphRef) {
            Finish(false, "replacement-context");
            return;
        }

        auto original = m_Session.Use(CKGUID(BML_LIFECYCLE_FIXTURE_GUID))
            .Settings({{"Value", 19}})
            .SpawnIn(graphRef.Value());
        if (!original) {
            Finish(false, "replacement-original");
            return;
        }
        m_ReplacementOriginal = std::move(original).Value();
        const auto originalView = m_ReplacementOriginal->Inspect(
            BML::Behavior::View::Live);
        m_ReplacementOriginalNode = originalView
            ? CKBehavior::Cast(context->GetObject(
                  static_cast<CK_ID>(originalView->Root().Id())))
            : nullptr;
        if (!m_ReplacementOriginalNode ||
            m_ReplacementOriginalNode->GetParent() != m_Graph) {
            Finish(false, "replacement-original-view");
            return;
        }
        m_ReplacementOriginalId = m_ReplacementOriginalNode->GetID();
        m_ReplacementOriginalNode->SetName("Public Replacement Original");
        m_ReplacementOriginalNode->SetPriority(271);
        CKParameterIn *originalPin =
            m_ReplacementOriginalNode->GetInputParameter(0);
        CKParameterOut *originalPout =
            m_ReplacementOriginalNode->CreateOutputParameter(
                const_cast<CKSTRING>("Result"), CKPGUID_INT);
        m_ReplacementEntry = AddLink(
            m_Source->GetOutput(0),
            m_ReplacementOriginalNode->GetInput(0));
        m_ReplacementExit = AddLink(
            m_ReplacementOriginalNode->GetOutput(0),
            m_Sink->GetInput(0));
        if (!originalPin || !originalPout || !m_ReplacementEntry ||
            !m_ReplacementExit ||
            originalPin->SetDirectSource(m_ReplacementSource) != CK_OK ||
            originalPout->AddDestination(m_ReplacementSource, TRUE) != CK_OK) {
            Finish(false, "replacement-relations");
            return;
        }
        m_ReplacementEntryId = m_ReplacementEntry->GetID();
        m_ReplacementExitId = m_ReplacementExit->GetID();

        const auto graph = m_Session.Inspect(m_Graph);
        if (!graph) {
            Finish(false, "replacement-inspect");
            return;
        }
        const auto target = std::find_if(
            graph->Nodes().begin(), graph->Nodes().end(),
            [&](const BML::Behavior::Node &node) {
                return node.Id() ==
                    static_cast<std::uint32_t>(m_ReplacementOriginalId);
            });
        if (target == graph->Nodes().end()) {
            Finish(false, "replacement-target");
            return;
        }

        BML::Behavior::Edit edit;
        const auto existing = edit.Use(*target);
        auto block = m_Session.Use(CKGUID(BML_LIFECYCLE_FIXTURE_GUID))
            .Settings({{"Value", 31}});
        const auto replacement = edit.Replace(existing, block);
        (void) edit.AppendPout(replacement, "Result", CKPGUID_INT);
        auto applied = graph->Apply("player-node-replacement", edit);
        if (!applied) {
            GetLogger()->Error(
                "Behavior replacement failed: code=%d error=%u phase=%u detail=%s",
                applied.Code(),
                static_cast<unsigned>(applied.GetStatus().Error),
                static_cast<unsigned>(applied.GetStatus().Phase),
                applied.GetStatus().Message.c_str());
            Finish(false, "replacement-apply");
            return;
        }
        m_ReplacementPatch = std::move(applied).Value();
        const auto parked = m_ReplacementPatch.Resolve(existing);
        const auto installed = m_ReplacementPatch.Resolve(replacement);
        const auto live = m_Session.Inspect(m_Graph);
        if (parked || !installed || !live) {
            Finish(false, "replacement-resolve");
            return;
        }
        const BML_ObjectRef installedRef = installed.Value();
        const auto installedNode = std::find_if(
            live->Nodes().begin(), live->Nodes().end(),
            [&](const BML::Behavior::Node &node) {
                const BML_ObjectRef object = node.Object();
                return object.Domain == installedRef.Domain &&
                    object.Slot == installedRef.Slot &&
                    object.Generation == installedRef.Generation;
            });
        m_ReplacementInstalledNode = installedNode != live->Nodes().end()
            ? CKBehavior::Cast(context->GetObject(
                  static_cast<CK_ID>(installedNode->Id())))
            : nullptr;
        m_ReplacementInstalledId = m_ReplacementInstalledNode
            ? m_ReplacementInstalledNode->GetID() : 0;
        CKParameterIn *installedPin = m_ReplacementInstalledNode
            ? m_ReplacementInstalledNode->GetInputParameter(0) : nullptr;
        CKParameterOut *installedPout = m_ReplacementInstalledNode
            ? m_ReplacementInstalledNode->GetOutputParameter(0) : nullptr;
        const bool installedCorrectly = m_ReplacementInstalledNode &&
            !HasNode(m_Graph, m_ReplacementOriginalNode) &&
            m_ReplacementOriginalNode->GetOwner() == m_Graph->GetOwner() &&
            m_ReplacementInstalledNode->GetParent() == m_Graph &&
            m_ReplacementInstalledNode->GetOwner() == m_Graph->GetOwner() &&
            std::string_view(m_ReplacementInstalledNode->GetName()) ==
                "Public Replacement Original" &&
            m_ReplacementInstalledNode->GetPriority() == 271 &&
            m_ReplacementEntry->GetID() == m_ReplacementEntryId &&
            m_ReplacementExit->GetID() == m_ReplacementExitId &&
            m_ReplacementEntry->GetOutBehaviorIO() ==
                m_ReplacementInstalledNode->GetInput(0) &&
            m_ReplacementExit->GetInBehaviorIO() ==
                m_ReplacementInstalledNode->GetOutput(0) &&
            installedPin &&
            installedPin->GetDirectSource() == m_ReplacementSource &&
            !HasDestination(originalPout, m_ReplacementSource) &&
            HasDestination(installedPout, m_ReplacementSource);
        if (!installedCorrectly) {
            GetLogger()->Error(
                "Behavior replacement shape: node=%s old_parked=%s old_owner=%s "
                "new_parent=%s new_owner=%s name=%s priority=%d entry=%s "
                "exit=%s pin=%s old_pout=%s new_pout=%s",
                m_ReplacementInstalledNode ? "true" : "false",
                !HasNode(m_Graph, m_ReplacementOriginalNode)
                    ? "true" : "false",
                m_ReplacementOriginalNode->GetOwner() == m_Graph->GetOwner()
                    ? "true" : "false",
                m_ReplacementInstalledNode &&
                        m_ReplacementInstalledNode->GetParent() == m_Graph
                    ? "true" : "false",
                m_ReplacementInstalledNode &&
                        m_ReplacementInstalledNode->GetOwner() ==
                            m_Graph->GetOwner()
                    ? "true" : "false",
                m_ReplacementInstalledNode &&
                        m_ReplacementInstalledNode->GetName()
                    ? m_ReplacementInstalledNode->GetName() : "<null>",
                m_ReplacementInstalledNode
                    ? m_ReplacementInstalledNode->GetPriority() : -1,
                m_ReplacementInstalledNode &&
                        m_ReplacementEntry->GetOutBehaviorIO() ==
                            m_ReplacementInstalledNode->GetInput(0)
                    ? "true" : "false",
                m_ReplacementInstalledNode &&
                        m_ReplacementExit->GetInBehaviorIO() ==
                            m_ReplacementInstalledNode->GetOutput(0)
                    ? "true" : "false",
                installedPin &&
                        installedPin->GetDirectSource() == m_ReplacementSource
                    ? "true" : "false",
                HasDestination(originalPout, m_ReplacementSource)
                    ? "true" : "false",
                HasDestination(installedPout, m_ReplacementSource)
                    ? "true" : "false");
            Finish(false, "replacement-installed-shape");
            return;
        }
        m_State = State::CloseReplacement;
    }

    void CloseReplacement() {
        const auto closed = m_ReplacementPatch.Close();
        if (!closed) {
            Finish(false, "replacement-close-request");
            return;
        }
        m_WaitUntil = m_Frame + 30;
        m_State = State::WaitReplacementRestored;
    }

    void WaitReplacementRestored() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        CKParameterIn *originalPin = m_ReplacementOriginalNode
            ? m_ReplacementOriginalNode->GetInputParameter(0) : nullptr;
        CKParameterOut *originalPout = m_ReplacementOriginalNode
            ? m_ReplacementOriginalNode->GetOutputParameter(0) : nullptr;
        const bool restored = context && m_Graph &&
            m_ReplacementOriginalNode &&
            HasNode(m_Graph, m_ReplacementOriginalNode) &&
            (!m_ReplacementInstalledId ||
             !context->GetObject(m_ReplacementInstalledId)) &&
            m_ReplacementEntry &&
            m_ReplacementEntry->GetID() == m_ReplacementEntryId &&
            m_ReplacementEntry->GetOutBehaviorIO() ==
                m_ReplacementOriginalNode->GetInput(0) &&
            m_ReplacementExit &&
            m_ReplacementExit->GetID() == m_ReplacementExitId &&
            m_ReplacementExit->GetInBehaviorIO() ==
                m_ReplacementOriginalNode->GetOutput(0) &&
            originalPin &&
            originalPin->GetDirectSource() == m_ReplacementSource &&
            HasDestination(originalPout, m_ReplacementSource);
        if (!restored) {
            if (m_Frame > m_WaitUntil)
                Finish(false, "replacement-restore");
            return;
        }
        const auto closed = m_ReplacementPatch.Close();
        if (!closed || closed.Value() != BML::Behavior::CloseState::Closed ||
            m_ReplacementPatch) {
            Finish(false, "replacement-close");
            return;
        }

        m_Graph->RemoveSubBehaviorLink(m_ReplacementEntry);
        m_Graph->RemoveSubBehaviorLink(m_ReplacementExit);
        context->DestroyObject(m_ReplacementEntry);
        context->DestroyObject(m_ReplacementExit);
        m_ReplacementEntry = nullptr;
        m_ReplacementExit = nullptr;
        if (originalPout)
            originalPout->RemoveDestination(m_ReplacementSource);
        const auto originalClosed = m_ReplacementOriginal->Close();
        if (!originalClosed) {
            Finish(false, "replacement-original-close");
            return;
        }
        m_WaitUntil = m_Frame + 30;
        m_State = State::WaitReplacementRemoved;
    }

    void WaitReplacementRemoved() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        if (context &&
            (!m_ReplacementOriginalId ||
             !context->GetObject(m_ReplacementOriginalId)) && Restored()) {
            const auto closed = m_ReplacementOriginal->Close();
            if (!closed ||
                closed.Value() != BML::Behavior::CloseState::Closed) {
                Finish(false, "replacement-original-retired");
                return;
            }
            m_ReplacementOriginal.reset();
            m_ReplacementOriginalNode = nullptr;
            m_ReplacementPassed = true;
            m_WaitUntil = m_Frame + 30;
            m_State = State::ArmPendingRemoval;
            return;
        }
        if (m_Frame > m_WaitUntil)
            Finish(false, "replacement-original-removal");
    }

    void ArmPendingRemoval() {
        if (!m_Graph || !m_Source || !m_Entry) {
            Finish(false, "removal-pending-context");
            return;
        }
        if (m_Graph->IsActive()) {
            if (m_Frame > m_WaitUntil)
                Finish(false, "removal-pending-arm");
            return;
        }
        m_Entry->SetInitialActivationDelay(4);
        m_Entry->SetActivationDelay(4);
        KickScript();
        m_WaitUntil = m_Frame + 3;
        m_State = State::RejectPendingRemoval;
    }

    void RejectPendingRemoval() {
        if (!m_Graph || !m_Source || !m_Entry) {
            Finish(false, "removal-pending-context");
            return;
        }
        if (!m_Graph->IsActive() || m_Entry->GetActivationDelay() <= 0 ||
            m_Entry->GetActivationDelay() >=
                m_Entry->GetInitialActivationDelay()) {
            if (m_Frame > m_WaitUntil)
                Finish(false, "removal-pending-not-observed");
            return;
        }
        // CKBehavior::Activate(FALSE, FALSE) preserves the delayed list. The
        // edit must reject the in-flight Link from its delay state rather than
        // trusting the graph's active flag.
        m_Graph->Activate(FALSE, FALSE);
        if (m_Graph->IsActive()) {
            Finish(false, "removal-pending-deactivate");
            return;
        }
        const auto graph = m_Session.Inspect(m_Graph);
        if (!graph) {
            Finish(false, "removal-pending-inspect");
            return;
        }
        const auto target = std::find_if(
            graph->Nodes().begin(), graph->Nodes().end(),
            [&](const BML::Behavior::Node &node) {
                return node.Id() ==
                    static_cast<std::uint32_t>(m_Source->GetID());
            });
        if (target == graph->Nodes().end()) {
            Finish(false, "removal-pending-target");
            return;
        }
        BML::Behavior::Edit edit;
        const auto removed = edit.Use(*target);
        edit.Remove(removed);
        const auto applied = graph->Apply("player-pending-node-removal", edit);
        if (applied || applied.Code() != BML_ERROR_BUSY ||
            applied.GetStatus().Error != BML::Behavior::Error::Busy) {
            Finish(false, "removal-pending-admission");
            return;
        }
        m_PendingRemovalRejected = true;
        m_Graph->Activate(FALSE, TRUE);
        m_Entry->SetInitialActivationDelay(0);
        m_Entry->SetActivationDelay(0);
        m_WaitUntil = m_Frame + 30;
        m_State = State::InstallRemoval;
    }

    void InstallRemoval() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        if (!context || !m_Graph || !m_Source || !m_Entry || !m_Anchor) {
            Finish(false, "removal-context");
            return;
        }
        // Keep one disconnected peer scheduled through the Apply safe point.
        // It proves that Remove depends on the target and its incident Links,
        // not on an unrelated graph-wide active flag.
        if (!m_RemovalPeer &&
            !AddNode("Public Removal Active Peer", m_RemovalPeer)) {
            Finish(false, "removal-active-peer-create");
            return;
        }
        m_RemovalPeer->SetFunction(RunActivePeer);
        if (!m_RemovalPeerLink) {
            m_RemovalPeerLink = AddLink(
                m_RemovalPeer->GetOutput(0), m_Source->GetInput(0));
            if (!m_RemovalPeerLink) {
                Finish(false, "removal-active-peer-link");
                return;
            }
        }
        // Give the existing sink the same public interface as the lifecycle
        // fixture. Replacing that sink in the same Edit makes the removed
        // anchor Link pass through Replace and then Remove, exercising their
        // exact inverse order on Close.
        if (m_Sink->GetInputParameterCount() == 0 &&
            !m_Sink->CreateInputParameter(
                const_cast<CKSTRING>("Source"), CKPGUID_INT)) {
            Finish(false, "removal-adjacent-replacement-pin");
            return;
        }
        m_Graph->ActivateInput(0, FALSE);
        m_RemovalPeer->Activate(TRUE, FALSE);
        m_Graph->Activate(TRUE, FALSE);
        m_RemovalWithActivePeer = m_Graph->IsActive() &&
            m_RemovalPeer->IsActive() &&
            !m_Source->IsActive() && !m_Source->GetInput(0)->IsActive() &&
            !m_Source->GetOutput(0)->IsActive();
        if (!m_RemovalWithActivePeer) {
            Finish(false, "removal-active-peer");
            return;
        }
        const auto graph = m_Session.Inspect(m_Graph);
        if (!graph) {
            Finish(false, "removal-inspect");
            return;
        }
        const auto target = std::find_if(
            graph->Nodes().begin(), graph->Nodes().end(),
            [&](const BML::Behavior::Node &node) {
                return node.Id() ==
                    static_cast<std::uint32_t>(m_Source->GetID());
            });
        if (target == graph->Nodes().end()) {
            Finish(false, "removal-target");
            return;
        }
        const auto replaced = std::find_if(
            graph->Nodes().begin(), graph->Nodes().end(),
            [&](const BML::Behavior::Node &node) {
                return node.Id() ==
                    static_cast<std::uint32_t>(m_Sink->GetID());
            });
        if (replaced == graph->Nodes().end()) {
            Finish(false, "removal-adjacent-replacement-target");
            return;
        }

        m_RemovalNodeId = m_Source->GetID();
        m_RemovalEntryId = m_Entry->GetID();
        m_RemovalAnchorId = m_Anchor->GetID();
        m_RemovalPeerLinkId = m_RemovalPeerLink->GetID();
        m_RemovalEntrySource = m_Entry->GetInBehaviorIO();
        m_RemovalEntrySink = m_Entry->GetOutBehaviorIO();
        m_RemovalAnchorSource = m_Anchor->GetInBehaviorIO();
        m_RemovalAnchorSink = m_Anchor->GetOutBehaviorIO();
        m_RemovalPeerLinkSource = m_RemovalPeerLink->GetInBehaviorIO();
        m_RemovalPeerLinkSink = m_RemovalPeerLink->GetOutBehaviorIO();
        m_RemovalEntryDelay = m_Entry->GetInitialActivationDelay();
        m_RemovalAnchorDelay = m_Anchor->GetInitialActivationDelay();
        m_RemovalPeerLinkDelay =
            m_RemovalPeerLink->GetInitialActivationDelay();
        m_RemovalNodesBefore = graph->Nodes().size();
        m_RemovalLinksBefore = graph->Links().size();

        BML::Behavior::Edit edit;
        const auto removed = edit.Use(*target);
        const auto originalSink = edit.Use(*replaced);
        const auto replacement = edit.Replace(
            originalSink,
            m_Session.Use(CKGUID(BML_LIFECYCLE_FIXTURE_GUID)));
        (void) replacement;
        edit.Remove(removed);
        auto applied = graph->Apply("player-node-removal", edit);
        if (!applied) {
            GetLogger()->Error(
                "Behavior removal failed: code=%d error=%u phase=%u detail=%s entry=%u/%s anchor=%u/%s",
                applied.Code(),
                static_cast<unsigned>(applied.GetStatus().Error),
                static_cast<unsigned>(applied.GetStatus().Phase),
                applied.GetStatus().Message.c_str(),
                static_cast<unsigned>(m_RemovalEntryId),
                m_RemovalEntrySource && m_RemovalEntrySource->IsActive()
                    ? "active" : "idle",
                static_cast<unsigned>(m_RemovalAnchorId),
                m_RemovalAnchorSource && m_RemovalAnchorSource->IsActive()
                    ? "active" : "idle");
            Finish(false, "removal-apply");
            return;
        }
        m_RemovalPatch = std::move(applied).Value();
        const auto parked = m_RemovalPatch.Resolve(removed);
        const auto visible = m_Session.Inspect(m_Graph);
        const bool removedCorrectly = !parked && visible &&
            m_Graph->IsActive() && m_RemovalPeer->IsActive() &&
            context->GetObject(m_RemovalNodeId) == m_Source &&
            context->GetObject(m_RemovalEntryId) == m_Entry &&
            context->GetObject(m_RemovalAnchorId) == m_Anchor &&
            context->GetObject(m_RemovalPeerLinkId) == m_RemovalPeerLink &&
            !HasNode(m_Graph, m_Source) &&
            m_Source->GetOwner() == m_Graph->GetOwner() &&
            !HasLink(m_Graph, m_Entry) && !HasLink(m_Graph, m_Anchor) &&
            !HasLink(m_Graph, m_RemovalPeerLink) &&
            m_Entry->GetInBehaviorIO() && m_Entry->GetOutBehaviorIO() &&
            m_Entry->GetInBehaviorIO() != m_RemovalEntrySource &&
            m_Entry->GetOutBehaviorIO() != m_RemovalEntrySink &&
            m_Anchor->GetInBehaviorIO() == m_Entry->GetInBehaviorIO() &&
            m_Anchor->GetOutBehaviorIO() == m_Entry->GetOutBehaviorIO() &&
            m_RemovalPeerLink->GetInBehaviorIO() ==
                m_Entry->GetInBehaviorIO() &&
            m_RemovalPeerLink->GetOutBehaviorIO() ==
                m_Entry->GetOutBehaviorIO() &&
            !m_Entry->GetInBehaviorIO()->GetOwner() &&
            !m_Entry->GetOutBehaviorIO()->GetOwner() &&
            m_Entry->GetInBehaviorIO()->GetType() == CK_BEHAVIORIO_OUT &&
            m_Entry->GetOutBehaviorIO()->GetType() == CK_BEHAVIORIO_IN &&
            !m_Entry->GetInBehaviorIO()->IsActive() &&
            !m_Entry->GetOutBehaviorIO()->IsActive() &&
            m_Entry->GetInitialActivationDelay() == m_RemovalEntryDelay &&
            m_Anchor->GetInitialActivationDelay() == m_RemovalAnchorDelay &&
            m_RemovalPeerLink->GetInitialActivationDelay() ==
                m_RemovalPeerLinkDelay &&
            visible->Nodes().size() + 1 == m_RemovalNodesBefore &&
            visible->Links().size() + 3 == m_RemovalLinksBefore;
        if (!removedCorrectly) {
            Finish(false, "removal-shape");
            return;
        }
        // Establish a real inactive -> active Scene transition without reset.
        // A reset would erase the active peer and would no longer test native
        // continuation scheduling through its output port.
        const auto deactivation = m_AuthoredScript.Deactivate();
        if (!deactivation || deactivation->RequestedActive) {
            Finish(false, "removal-isolation-deactivation");
            return;
        }
        m_WaitUntil = m_Frame + 30;
        m_State = State::WaitRemovalInactive;
    }

    void WaitRemovalInactive() {
        const auto info = m_AuthoredScript.Info();
        if (!info) {
            Finish(false, "removal-isolation-script");
            return;
        }
        if (info->Active || info->RequestedActive) {
            if (m_Frame > m_WaitUntil)
                Finish(false, "removal-isolation-deactivation-timeout");
            return;
        }
        m_RemovalPeer->Activate(TRUE, FALSE);
        m_Graph->Activate(TRUE, FALSE);
        m_RemovalSourceRunsBefore = g_RemovalSourceRuns;
        m_RemovalPeerRunsBefore = g_RemovalPeerRuns;
        const auto activation = m_AuthoredScript.Activate(false);
        if (!activation || !activation->RequestedActive) {
            Finish(false, "removal-isolation-activation");
            return;
        }
        m_WaitUntil = m_Frame + 30;
        m_State = State::ObserveRemovalIsolation;
    }

    void ObserveRemovalIsolation() {
        if (g_RemovalSourceRuns != m_RemovalSourceRunsBefore) {
            Finish(false, "removal-source-ran");
            return;
        }
        if (g_RemovalPeerRuns == m_RemovalPeerRunsBefore &&
            m_Frame <= m_WaitUntil)
            return;
        m_RemovalIsolated = m_Graph && m_RemovalPeer &&
            m_RemovalPeerLink && g_RemovalPeerRuns > m_RemovalPeerRunsBefore &&
            m_Graph->IsActive() && m_RemovalPeer->IsActive() &&
            !m_Source->IsActive() && !m_Source->GetInput(0)->IsActive() &&
            !m_Source->GetOutput(0)->IsActive() &&
            !HasLink(m_Graph, m_RemovalPeerLink);
        if (!m_RemovalIsolated) {
            GetLogger()->Error(
                "Behavior removal isolation failed: source=%u/%u peer=%u/%u "
                "graph=%s peer_active=%s node=%s in=%s out=%s link=%s",
                g_RemovalSourceRuns, m_RemovalSourceRunsBefore,
                g_RemovalPeerRuns, m_RemovalPeerRunsBefore,
                m_Graph && m_Graph->IsActive() ? "active" : "idle",
                m_RemovalPeer && m_RemovalPeer->IsActive()
                    ? "active" : "idle",
                m_Source && m_Source->IsActive() ? "active" : "idle",
                m_Source && m_Source->GetInput(0)->IsActive()
                    ? "active" : "idle",
                m_Source && m_Source->GetOutput(0)->IsActive()
                    ? "active" : "idle",
                m_Graph && m_RemovalPeerLink &&
                        HasLink(m_Graph, m_RemovalPeerLink)
                    ? "resident" : "parked");
            Finish(false, "removal-isolation");
            return;
        }
        m_State = State::CloseRemoval;
    }

    void CloseRemoval() {
        const auto closed = m_RemovalPatch.Close();
        if (!closed) {
            Finish(false, "removal-close-request");
            return;
        }
        m_WaitUntil = m_Frame + 30;
        m_State = State::WaitRemovalRestored;
    }

    void WaitRemovalRestored() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        const auto visible = context && m_Graph
            ? m_Session.Inspect(m_Graph)
            : BML::Behavior::Result<BML::Behavior::Graph>{};
        const bool restored = context && visible &&
            context->GetObject(m_RemovalNodeId) == m_Source &&
            context->GetObject(m_RemovalEntryId) == m_Entry &&
            context->GetObject(m_RemovalAnchorId) == m_Anchor &&
            context->GetObject(m_RemovalPeerLinkId) == m_RemovalPeerLink &&
            HasNode(m_Graph, m_Source) && m_Source->GetParent() == m_Graph &&
            HasNode(m_Graph, m_Sink) && m_Sink->GetParent() == m_Graph &&
            HasLink(m_Graph, m_Entry) && HasLink(m_Graph, m_Anchor) &&
            HasLink(m_Graph, m_RemovalPeerLink) &&
            m_Entry->GetInBehaviorIO() == m_RemovalEntrySource &&
            m_Entry->GetOutBehaviorIO() == m_RemovalEntrySink &&
            m_Anchor->GetInBehaviorIO() == m_RemovalAnchorSource &&
            m_Anchor->GetOutBehaviorIO() == m_RemovalAnchorSink &&
            m_RemovalPeerLink->GetInBehaviorIO() ==
                m_RemovalPeerLinkSource &&
            m_RemovalPeerLink->GetOutBehaviorIO() ==
                m_RemovalPeerLinkSink &&
            m_Entry->GetInitialActivationDelay() == m_RemovalEntryDelay &&
            m_Anchor->GetInitialActivationDelay() == m_RemovalAnchorDelay &&
            m_RemovalPeerLink->GetInitialActivationDelay() ==
                m_RemovalPeerLinkDelay &&
            visible->Nodes().size() == m_RemovalNodesBefore &&
            visible->Links().size() == m_RemovalLinksBefore;
        if (!restored) {
            if (m_Frame > m_WaitUntil)
                Finish(false, "removal-restore");
            return;
        }
        const auto closed = m_RemovalPatch.Close();
        if (!closed || closed.Value() != BML::Behavior::CloseState::Closed ||
            m_RemovalPatch) {
            Finish(false, "removal-close");
            return;
        }
        m_Graph->Activate(FALSE, TRUE);
        if (!m_RemovalPeerLink ||
            m_Graph->RemoveSubBehaviorLink(m_RemovalPeerLink) !=
                m_RemovalPeerLink) {
            Finish(false, "removal-active-peer-link-remove");
            return;
        }
        context->DestroyObject(m_RemovalPeerLink);
        m_RemovalPeerLink = nullptr;
        if (!m_RemovalPeer ||
            m_Graph->RemoveSubBehavior(m_RemovalPeer) != m_RemovalPeer) {
            Finish(false, "removal-active-peer-remove");
            return;
        }
        context->DestroyObject(m_RemovalPeer);
        m_RemovalPeer = nullptr;
        m_RemovalPassed = true;
        ApplyPatch();
    }

    bool RejectsMalformedPort(BML_ObjectRef graph) {
        const BML_BehaviorInterface *api = m_Session.Api();
        if (!api)
            return false;

        constexpr char appendedName[] = "Malformed In";
        constexpr char patchName[] = "player-malformed-port";
        BML_BehaviorEditStep steps[2]{};
        steps[0].StructSize = sizeof(steps[0]);
        steps[0].Kind = BML_BEHAVIOR_EDIT_APPEND_SLOT;
        steps[0].Result = 2;
        steps[0].Target = BML_BEHAVIOR_EDIT_GRAPH;
        steps[0].SlotKind = BML_BEHAVIOR_SLOT_IN;
        steps[0].Name = {appendedName,
                         static_cast<std::uint32_t>(sizeof(appendedName) - 1)};

        steps[1].StructSize = sizeof(steps[1]);
        steps[1].Kind = BML_BEHAVIOR_EDIT_FLOW;
        steps[1].Source.Handle = 2;
        steps[1].Source.Kind = 0;
        // StructSize deliberately remains zero. Appended-slot references are
        // still complete public DTOs and must not bypass wire validation.
        steps[1].Sink.StructSize = sizeof(steps[1].Sink);
        steps[1].Sink.Handle = BML_BEHAVIOR_EDIT_GRAPH;
        steps[1].Sink.Kind = BML_BEHAVIOR_SLOT_OUT;
        steps[1].Sink.Slot.StructSize = sizeof(steps[1].Sink.Slot);
        steps[1].Sink.Slot.Kind = BML_BEHAVIOR_SELECTOR_INDEX;
        steps[1].Sink.Slot.Index = 0;

        BML_BehaviorPatchSpec spec{};
        spec.StructSize = sizeof(spec);
        spec.Name = {patchName,
                     static_cast<std::uint32_t>(sizeof(patchName) - 1)};
        spec.Graph = graph;
        spec.Steps = steps;
        spec.StepCount = 2;
        BML_BehaviorPatch patch = nullptr;
        BML_BehaviorStatus status{};
        status.StructSize = sizeof(status);
        const int code = api->ApplyPatch(m_Session.Handle(), &spec, &patch,
                                         nullptr, &status);
        if (patch)
            (void) api->ClosePatch(m_Session.Handle(), patch);
        return code == BML_ERROR_INVALID_PARAMETER && patch == nullptr;
    }

    void ApplyPatch() {
        const auto reference = m_Session.Reference(m_Graph);
        if (!reference) {
            Finish(false, "patch-reference");
            return;
        }
        if (!RejectsMalformedPort(reference.Value())) {
            Finish(false, "malformed-port");
            return;
        }
        auto inspected = m_Session.Inspect(reference.Value());
        if (!inspected) {
            Finish(false, "patch-inspect");
            return;
        }
        BML::Behavior::Edit edit;
        const auto source = edit.Require(kSourceName);
        const auto sink = edit.Require(kSinkName);
        const auto link = edit.Between(source.Out(), sink.In());
        const auto block = edit.Add(m_Session.Use(
            CKGUID(BML_LIFECYCLE_FIXTURE_GUID)));
        edit.Splice(link, block);

        auto applied = inspected->Apply("player-public-patch", edit);
        if (!applied) {
            GetLogger()->Error(
                "Behavior patch apply failed: code=%d error=%u phase=%u detail=%s",
                applied.Code(),
                static_cast<unsigned>(applied.GetStatus().Error),
                static_cast<unsigned>(applied.GetStatus().Phase),
                applied.GetStatus().Message.c_str());
            Finish(false, "patch-apply");
            return;
        }
        m_Patch = std::move(applied).Value();
        const auto info = m_Patch.Info();
        m_PatchPassed = info && info->State == PatchState::Active &&
            info->Installed() && info->Conflicts == 0 && PatchInstalled();
        if (!m_PatchPassed) {
            Finish(false, "patch-state");
            return;
        }
        m_State = State::ClosePatch;
    }

    void ClosePatch() {
        // The Patch owns the native Session it still needs. Releasing the
        // original facade value must not make the Patch stale before restore.
        m_Session.Close();
        m_PatchSink = m_Anchor ? m_Anchor->GetOutBehaviorIO() : nullptr;
        if (!m_Anchor || !m_Graph || !m_PatchSink ||
            m_Anchor->SetOutBehaviorIO(m_Graph->GetOutput(0)) != CK_OK) {
            Finish(false, "patch-conflict-setup");
            return;
        }
        BML::Behavior::Result<BML::Behavior::CloseState> closed;
        std::thread closer([&] { closed = m_Patch.Close(); });
        closer.join();
        const auto closing = m_Patch.Info();
        if (!closed || closed.Value() != BML::Behavior::CloseState::Closing ||
            !m_Patch || !closing ||
            closing->State != PatchState::Closing) {
            Finish(false, "patch-thread-close");
            return;
        }
        m_WaitUntil = m_Frame + 30;
        m_State = State::WaitPatchConflict;
    }

    void WaitPatchConflict() {
        const auto conflicted = m_Patch.Info();
        if (!conflicted || conflicted->State != PatchState::Conflicted) {
            if (m_Frame > m_WaitUntil)
                Finish(false, "patch-conflict");
            return;
        }
        // Destroy the only facade handle while the inverse is still blocked.
        // The Loader now owns completion of the requested retirement.
        m_Patch = {};
        if (m_Patch ||
            m_Anchor->SetOutBehaviorIO(m_PatchSink) != CK_OK) {
            Finish(false, "patch-conflict-release");
            return;
        }
        m_WaitUntil = m_Frame + 30;
        m_State = State::WaitPatchRetired;
    }

    void WaitPatchRetired() {
        if (Restored()) {
            m_PatchClosePassed = true;
            const auto closed = m_AuthoredScript.Close();
            if (!closed || closed.Value() !=
                    BML::Behavior::CloseState::Closing) {
                Finish(false, "script-close-request");
                return;
            }
            m_WaitUntil = m_Frame + 30;
            m_State = State::WaitScriptClosed;
            return;
        }
        if (m_Frame > m_WaitUntil)
            Finish(false, "patch-retirement");
    }

    void WaitScriptClosed() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        if (!context) {
            Finish(false, "script-close-context");
            return;
        }
        if ((m_AuthoredRootId && context->GetObject(m_AuthoredRootId)) ||
            (m_AuthoredOperationId &&
             context->GetObject(m_AuthoredOperationId))) {
            if (m_Frame > m_WaitUntil)
                Finish(false, "script-close");
            return;
        }
        const auto closed = m_AuthoredScript.Close();
        if (!closed || closed.Value() != BML::Behavior::CloseState::Closed ||
            m_AuthoredScript) {
            Finish(false, "script-close-handle");
            return;
        }
        m_ScriptPassed = true;
        DestroyGraph();
        Finish(true, "done");
    }

    void Finish(bool passed, const char *reason) {
        if (m_Done)
            return;
        m_Done = true;
        if (!passed) {
            (void) m_Patch.Close();
            (void) m_IdentityPatch.Close();
            (void) m_ReplacementPatch.Close();
            (void) m_RemovalPatch.Close();
            if (m_ReplacementOriginal)
                (void) m_ReplacementOriginal->Close();
            if (m_Parked)
                (void) m_Parked->Close();
            (void) m_Plan.Close();
            (void) m_SelfPlan.Close();
            DestroyGraph();
        }
        GetLogger()->Info(
            "Behavior plan: status=%s reason=%s submit=%s install=%s hooks=%s close=%s release=%s taps=%u afters=%u frames=%d",
            passed ? "pass" : "fail", reason,
            m_SubmitPassed ? "true" : "false",
            m_InstallPassed ? "true" : "false",
            m_HookPassed ? "true" : "false",
            m_ClosePassed ? "true" : "false",
            m_ReleasePassed ? "true" : "false",
            m_Counters->Taps, m_Counters->Afters, m_Frame);
        GetLogger()->Info(
            "Behavior self-close: status=%s calls=%u closing=%s",
            m_SelfClosePassed ? "pass" : "fail", m_SelfClose->Calls,
            m_SelfClose->Closing ? "true" : "false");
        GetLogger()->Info(
            "Behavior identity: status=%s attach=%s continuation=%s identity=%s befores=%u",
            (m_AttachPassed && m_ContinuationPassed && m_IdentityPassed)
                ? "pass" : "fail",
            m_AttachPassed ? "true" : "false",
            m_ContinuationPassed ? "true" : "false",
            m_IdentityPassed ? "true" : "false", m_Identity->Befores);
        GetLogger()->Info(
            "Behavior graph patch: status=%s reason=%s apply=%s close=%s",
            passed ? "pass" : "fail", reason,
            m_PatchPassed ? "true" : "false",
            m_PatchClosePassed ? "true" : "false");
        GetLogger()->Info(
            "Behavior node replacement: status=%s",
            m_ReplacementPassed ? "pass" : "fail");
        GetLogger()->Info(
            "Behavior node removal: status=%s lifecycle=%s restore=%s pending=%s active_peer=%s isolation=%s",
            m_RemovalPassed ? "pass" : "fail",
            m_RemovalPassed ? "true" : "false",
            m_RemovalPassed ? "true" : "false",
            m_PendingRemovalRejected ? "true" : "false",
            m_RemovalWithActivePeer ? "true" : "false",
            m_RemovalIsolated ? "true" : "false");
        GetLogger()->Info(
            "Behavior authored script: status=%s atomic=%s create=%s edit=%s operation=%s activity=%s close=%s",
            m_ScriptPassed ? "pass" : "fail",
            m_AtomicScriptPassed ? "true" : "false",
            m_AuthoredRootId || m_ScriptPassed ? "true" : "false",
            m_ScriptDefined ? "true" : "false",
            m_OperationPassed ? "true" : "false",
            m_InstallPassed ? "true" : "false",
            m_ScriptPassed ? "true" : "false");
        if (passed)
            BML::PlayerTest::ProbeReport::Pass(reason);
        else
            BML::PlayerTest::ProbeReport::Fail(reason);
    }

    BML::Behavior::Session m_Session;
    BML::Behavior::Script m_AuthoredScript;
    BML::Behavior::Plan m_Plan;
    BML::Behavior::Plan m_SelfPlan;
    BML::Behavior::Patch m_Patch;
    BML::Behavior::Patch m_IdentityPatch;
    BML::Behavior::Patch m_ReplacementPatch;
    BML::Behavior::Patch m_RemovalPatch;
    std::optional<BML::Behavior::Instance> m_Parked;
    std::optional<BML::Behavior::Instance> m_ReplacementOriginal;
    // What the Before callback recorded. The count returning to one proves the
    // Loader released the callback state the Patch owned.
    struct IdentityState {
        std::uint32_t Befores = 0;
        std::uint32_t Blocks = 0;
    };
    std::shared_ptr<IdentityState> m_Identity =
        std::make_shared<IdentityState>();
    std::shared_ptr<Counters> m_Counters = std::make_shared<Counters>();
    struct SelfCloseState {
        std::uint32_t Calls = 0;
        bool Closing = false;
    };
    std::shared_ptr<SelfCloseState> m_SelfClose =
        std::make_shared<SelfCloseState>();
    CK3dObject *m_Owner = nullptr;
    CKBehavior *m_Graph = nullptr;
    CKBehavior *m_Source = nullptr;
    CKBehavior *m_Sink = nullptr;
    CKBehavior *m_RemovalPeer = nullptr;
    CKBehaviorLink *m_RemovalPeerLink = nullptr;
    CKBehaviorLink *m_Entry = nullptr;
    CKBehaviorLink *m_Anchor = nullptr;
    CKBehaviorLink *m_Exit = nullptr;
    CKParameterLocal *m_ReplacementSource = nullptr;
    CKBehavior *m_ReplacementOriginalNode = nullptr;
    CKBehavior *m_ReplacementInstalledNode = nullptr;
    CKBehaviorLink *m_ReplacementEntry = nullptr;
    CKBehaviorLink *m_ReplacementExit = nullptr;
    CKBehaviorIO *m_RemovalEntrySource = nullptr;
    CKBehaviorIO *m_RemovalEntrySink = nullptr;
    CKBehaviorIO *m_RemovalAnchorSource = nullptr;
    CKBehaviorIO *m_RemovalAnchorSink = nullptr;
    CKBehaviorIO *m_RemovalPeerLinkSource = nullptr;
    CKBehaviorIO *m_RemovalPeerLinkSink = nullptr;
    CKBehaviorIO *m_PatchSink = nullptr;
    CK_ID m_AnchorId = 0;
    CK_ID m_AuthoredRootId = 0;
    CK_ID m_AuthoredOperationId = 0;
    CK_ID m_ReplacementOriginalId = 0;
    CK_ID m_ReplacementInstalledId = 0;
    CK_ID m_ReplacementEntryId = 0;
    CK_ID m_ReplacementExitId = 0;
    CK_ID m_RemovalNodeId = 0;
    CK_ID m_RemovalEntryId = 0;
    CK_ID m_RemovalAnchorId = 0;
    CK_ID m_RemovalPeerLinkId = 0;
    int m_RemovalEntryDelay = 0;
    int m_RemovalAnchorDelay = 0;
    int m_RemovalPeerLinkDelay = 0;
    std::uint32_t m_RemovalSourceRunsBefore = 0;
    std::uint32_t m_RemovalPeerRunsBefore = 0;
    std::size_t m_RemovalNodesBefore = 0;
    std::size_t m_RemovalLinksBefore = 0;
    int m_AttachBlocks = 0;
    int m_AttachLinks = 0;
    bool m_ContinuationPassed = false;
    State m_State = State::Submit;
    int m_Frame = 0;
    int m_WaitUntil = 0;
    bool m_LevelStarted = false;
    bool m_SubmitPassed = false;
    bool m_InstallPassed = false;
    bool m_HookPassed = false;
    bool m_ClosePassed = false;
    bool m_ReleasePassed = false;
    bool m_SelfClosePassed = false;
    bool m_AttachPassed = false;
    bool m_IdentityPassed = false;
    bool m_PatchPassed = false;
    bool m_PatchClosePassed = false;
    bool m_ReplacementPassed = false;
    bool m_RemovalPassed = false;
    bool m_PendingRemovalRejected = false;
    bool m_RemovalWithActivePeer = false;
    bool m_RemovalIsolated = false;
    bool m_AtomicScriptPassed = false;
    bool m_ScriptDefined = false;
    bool m_OperationPassed = false;
    bool m_ScriptPassed = false;
    bool m_Done = false;
};

} // namespace

BML_PLAYER_PROBE_READ_EXPORT()

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new BehaviorFacadeTest(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}

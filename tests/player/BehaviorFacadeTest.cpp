// Exercises the published Patch, Plan, and Hook surface the way a Mod author would:
// only BML/Behavior.hpp, no private Behavior headers. The one test affordance
// is ObserveScript, which puts a script this probe created itself into the
// Plans world; the game feeds real scripts in the same way when it loads them.
#include "Api/BehaviorTestApi.h"
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

int RunPlanNode(const CKBehaviorContext &context) {
    CKBehavior *behavior = context.Behavior;
    if (!behavior)
        return CKBR_BEHAVIORERROR;
    for (int index = 0; index < behavior->GetInputCount(); ++index)
        behavior->ActivateInput(index, FALSE);
    for (int index = 0; index < behavior->GetOutputCount(); ++index)
        behavior->ActivateOutput(index);
    return CKBR_OK;
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
        const void *found = nullptr;
        if (BML_GetInterface(BML_BEHAVIOR_TEST_INTERFACE_ID,
                             BML_BEHAVIOR_TEST_INTERFACE_MAJOR,
                             &found) != BML_OK) {
            Finish(false, "test-interface");
            return;
        }
        m_Test = static_cast<const BML_BehaviorTestInterface *>(found);

        auto session = BML::Behavior::Session::Open();
        if (!session) {
            Finish(false, "session");
            return;
        }
        m_Session = std::move(session).Value();
    }

    void OnStartLevel() override { m_LevelStarted = true; }

    void OnProcess() override {
        if (m_Done || !m_LevelStarted || !m_Test ||
            (!m_Session && m_State != State::WaitPatchConflict &&
             m_State != State::WaitPatchRetired))
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
        case State::ClosePatch: ClosePatch(); break;
        case State::WaitPatchConflict: WaitPatchConflict(); break;
        case State::WaitPatchRetired: WaitPatchRetired(); break;
        }
    }

    void OnUnload() override {
        (void) m_Plan.Close();
        (void) m_SelfPlan.Close();
        (void) m_Patch.Close();
        m_Session.Close();
        DestroyGraph();
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
        ClosePatch,
        WaitPatchConflict,
        WaitPatchRetired,
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
        m_Graph = CKBehavior::Cast(context->CreateObject(
            CKCID_BEHAVIOR, const_cast<CKSTRING>(kScriptName),
            CK_OBJECTCREATION_DYNAMIC));
        if (!m_Owner || !m_Graph || level->AddObject(m_Owner) != CK_OK)
            return false;
        if (scene != level->GetLevelScene())
            (void) scene->AddObject(m_Owner);
        scene->Activate(m_Owner, TRUE);

        m_Graph->UseGraph();
        m_Graph->SetType(CKBEHAVIORTYPE_SCRIPT);
        if (m_Graph->SetOwner(m_Owner, FALSE) != CK_OK ||
            !m_Graph->CreateInput("Start") ||
            !m_Graph->CreateOutput("Done") ||
            m_Owner->AddScript(m_Graph) != CK_OK) {
            return false;
        }
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
        if (m_Graph && m_Owner)
            (void) m_Owner->RemoveScript(m_Graph->GetID());
        if (m_Graph)
            context->DestroyObject(m_Graph);
        m_Graph = nullptr;
        m_Source = nullptr;
        m_Sink = nullptr;
        m_Entry = nullptr;
        m_Anchor = nullptr;
        m_Exit = nullptr;
        m_AnchorId = 0;
        if (m_Owner)
            context->DestroyObject(m_Owner);
        m_Owner = nullptr;
    }

    void RunGraph() {
        CKScene *scene = m_BML && m_BML->GetCKContext()
            ? m_BML->GetCKContext()->GetCurrentScene() : nullptr;
        if (!scene || !m_Graph)
            return;
        m_Graph->ActivateInput(0, FALSE);
        m_Graph->ActivateOutput(0, FALSE);
        scene->Activate(m_Graph, TRUE);
        m_Graph->ActivateInput(0, TRUE);
        m_WaitUntil = m_Frame + 30;
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
        if (m_Test->ObserveScript(m_Session.Handle(), m_Graph) != BML_OK) {
            Finish(false, "observe");
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
            RunGraph();
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
            RunGraph();
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
        BMLLifecycleFixtureSetContinuationFn SetContinuation = nullptr;
        BMLLifecycleFixtureReadTraceFn ReadTrace = nullptr;

        explicit operator bool() const {
            return ResetTrace && SetContinuation && ReadTrace;
        }
    };

    static LifecycleFixtureExports ResolveLifecycleFixture() {
        LifecycleFixtureExports exports;
        HMODULE module = ::GetModuleHandleA("BehaviorLifecycleFixture.dll");
        if (!module)
            return exports;
        exports.ResetTrace = reinterpret_cast<BMLLifecycleFixtureResetTraceFn>(
            ::GetProcAddress(module, "BMLLifecycleFixtureResetTrace"));
        exports.SetContinuation =
            reinterpret_cast<BMLLifecycleFixtureSetContinuationFn>(
                ::GetProcAddress(module, "BMLLifecycleFixtureSetContinuation"));
        exports.ReadTrace = reinterpret_cast<BMLLifecycleFixtureReadTraceFn>(
            ::GetProcAddress(module, "BMLLifecycleFixtureReadTrace"));
        return exports;
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
        edit.Bind(amount, 2.5f);
        edit.Bind(mode, FacadeMode::On);
        edit.Before(anchor, interposed);
        edit.Redirect(anchor, block.In());
        edit.Flow(block.Out(), sink.In());

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
        if (!IdentityViews(blockRef.Value())) {
            Finish(false, "identity-views");
            return;
        }
        RunGraph();
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
        const bool deduced = amount && mode && value &&
            std::holds_alternative<float>(amount->Data) &&
            std::get<float>(amount->Data) == 2.5f &&
            std::holds_alternative<std::int32_t>(mode->Data) &&
            std::get<std::int32_t>(mode->Data) == 3 &&
            std::holds_alternative<std::int32_t>(value->Data) &&
            std::get<std::int32_t>(value->Data) == 77;
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
            DestroyGraph();
            Finish(true, "done");
            return;
        }
        if (m_Frame > m_WaitUntil)
            Finish(false, "patch-retirement");
    }

    void Finish(bool passed, const char *reason) {
        if (m_Done)
            return;
        m_Done = true;
        if (!passed) {
            (void) m_Patch.Close();
            (void) m_IdentityPatch.Close();
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
        if (passed)
            BML::PlayerTest::ProbeReport::Pass(reason);
        else
            BML::PlayerTest::ProbeReport::Fail(reason);
    }

    const BML_BehaviorTestInterface *m_Test = nullptr;
    BML::Behavior::Session m_Session;
    BML::Behavior::Plan m_Plan;
    BML::Behavior::Plan m_SelfPlan;
    BML::Behavior::Patch m_Patch;
    BML::Behavior::Patch m_IdentityPatch;
    std::optional<BML::Behavior::Instance> m_Parked;
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
    CKBehaviorLink *m_Entry = nullptr;
    CKBehaviorLink *m_Anchor = nullptr;
    CKBehaviorLink *m_Exit = nullptr;
    CKBehaviorIO *m_PatchSink = nullptr;
    CK_ID m_AnchorId = 0;
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

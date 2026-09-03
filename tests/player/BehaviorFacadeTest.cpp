// Exercises the published Patch, Plan, and Hook surface the way a Mod author would:
// only BML/Behavior.hpp, no private Behavior headers. The one test affordance
// is ObserveScript, which puts a script this probe created itself into the
// Plans world; the game feeds real scripts in the same way when it loads them.
#include "Api/BehaviorTestApi.h"
#include "BehaviorLifecycleFixtureApi.h"

#include "BML/Behavior.hpp"
#include "BML/IMod.h"
#include "CKAll.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <thread>

namespace {

using BML::Behavior::Hook;
using BML::Behavior::HookEvent;
using BML::Behavior::HookResult;
using BML::Behavior::GraphPatchState;
using BML::Behavior::PlanState;

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
        BML_ObjectRef reference{};
        if (m_Test->ReferenceObject(
                m_Session.Handle(), m_Graph, &reference) != BML_OK) {
            return false;
        }
        auto logical = m_Session.Inspect(reference);
        if (!logical)
            return false;
        auto live = logical->Live();
        if (!live)
            return false;
        const auto logicalAnchor = std::find_if(
            logical->Links().begin(), logical->Links().end(),
            [&](const BML::Behavior::Link &link) {
                return link.Id == static_cast<std::uint32_t>(m_AnchorId);
            });
        const auto liveAnchor = std::find_if(
            live->Links().begin(), live->Links().end(),
            [&](const BML::Behavior::Link &link) {
                return link.Id == static_cast<std::uint32_t>(m_AnchorId);
            });
        return logical->Mode() == BML::Behavior::View::Logical &&
            live->Mode() == BML::Behavior::View::Live &&
            // The explicit fixture remains author-visible. Tap and the
            // exit-path After HookBlock do not add Logical nodes or Links.
            logical->Nodes().size() == 4 && logical->Links().size() == 3 &&
            live->Nodes().size() == 6 && live->Links().size() == 6 &&
            logicalAnchor != logical->Links().end() &&
            liveAnchor != live->Links().end() &&
            logicalAnchor->Source.Node ==
                static_cast<std::uint32_t>(m_Source->GetID()) &&
            logicalAnchor->Target.Node ==
                static_cast<std::uint32_t>(m_Sink->GetID()) &&
            liveAnchor->Target.Node != logicalAnchor->Target.Node &&
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
        Hook after([counters](const HookEvent &event) {
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

        const BML::Behavior::Guid fixture(BML_LIFECYCLE_FIXTURE_GUID);
        auto draft = m_Session.Plan("player-public-plan");
        draft.OnSingle(kScriptName);
        const auto source = draft.Require(kSourceName);
        const auto sink = draft.Require(kSinkName);
        const auto link = draft.Between(source.Out(0), sink.In(0));
        draft.Tap(source.Out(0), tap);
        draft.After(source.Out(0), after);
        const auto block = draft.Add(fixture);
        (void) draft.AppendIn(block, "Again");
        (void) draft.AppendOut(block, "Finished");
        const auto literal = draft.AppendPin(block, "Literal", CKPGUID_INT);
        draft.Bind(literal, 41);
        draft.Splice(link, block);

        auto submitted = draft.Submit();
        if (!submitted) {
            GetLogger()->Error(
                "Behavior plan submit failed: code=%d error=%u phase=%u detail=%s",
                submitted.Code(),
                static_cast<unsigned>(submitted.Detail().Error),
                static_cast<unsigned>(submitted.Detail().Phase),
                submitted.Detail().Message.c_str());
            Finish(false, "submit");
            return;
        }
        m_Plan = std::move(submitted).Value();

        // A Plan the Loader accepted reconciles on the next frame, so it is
        // not installed yet and reads Reconciling with no matches.
        const auto pending = m_Plan.Read();
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
        const auto info = m_Plan.Read();
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
        const auto closing = m_Plan.Read();
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
        auto draft = m_Session.Plan("player-public-self-close");
        draft.OnSingle(kScriptName);
        const auto source = draft.Require(kSourceName);
        draft.Tap(source.Out(0), hook);
        auto submitted = draft.Submit();
        if (!submitted) {
            Finish(false, "self-close-submit");
            return;
        }
        m_SelfPlan = std::move(submitted).Value();
        m_WaitUntil = m_Frame + 30;
        m_State = State::WaitSelfActive;
    }

    void WaitSelfActive() {
        const auto info = m_SelfPlan.Read();
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
            ApplyPatch();
            return;
        }
        if (m_Frame > m_WaitUntil)
            Finish(false, "self-close-retirement");
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
        BML_ObjectRef reference{};
        if (m_Test->ReferenceObject(
                m_Session.Handle(), m_Graph, &reference) != BML_OK) {
            Finish(false, "patch-reference");
            return;
        }
        if (!RejectsMalformedPort(reference)) {
            Finish(false, "malformed-port");
            return;
        }
        auto inspected = m_Session.Inspect(reference);
        if (!inspected) {
            Finish(false, "patch-inspect");
            return;
        }
        auto edit = inspected->Patch("player-public-patch");
        const auto source = edit.Require(kSourceName);
        const auto sink = edit.Require(kSinkName);
        const auto link = edit.Between(source.Out(), sink.In());
        const auto block = edit.Add(BML::Behavior::Guid(
            BML_LIFECYCLE_FIXTURE_GUID));
        edit.Splice(link, block);

        auto applied = edit.Apply();
        if (!applied) {
            GetLogger()->Error(
                "Behavior patch apply failed: code=%d error=%u phase=%u detail=%s",
                applied.Code(),
                static_cast<unsigned>(applied.Detail().Error),
                static_cast<unsigned>(applied.Detail().Phase),
                applied.Detail().Message.c_str());
            Finish(false, "patch-apply");
            return;
        }
        m_Patch = std::move(applied).Value();
        const auto info = m_Patch.Read();
        m_PatchPassed = info && info->State == GraphPatchState::Active &&
            info->Installed() && info->Conflicts == 0 && PatchInstalled();
        if (!m_PatchPassed) {
            Finish(false, "patch-state");
            return;
        }
        m_State = State::ClosePatch;
    }

    void ClosePatch() {
        // The GraphPatch owns the native Session it still needs. Releasing the
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
        const auto closing = m_Patch.Read();
        if (!closed || closed.Value() != BML::Behavior::CloseState::Closing ||
            !m_Patch || !closing ||
            closing->State != GraphPatchState::Closing) {
            Finish(false, "patch-thread-close");
            return;
        }
        m_WaitUntil = m_Frame + 30;
        m_State = State::WaitPatchConflict;
    }

    void WaitPatchConflict() {
        const auto conflicted = m_Patch.Read();
        if (!conflicted || conflicted->State != GraphPatchState::Conflicted) {
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
            "Behavior graph patch: status=%s reason=%s apply=%s close=%s",
            passed ? "pass" : "fail", reason,
            m_PatchPassed ? "true" : "false",
            m_PatchClosePassed ? "true" : "false");
    }

    const BML_BehaviorTestInterface *m_Test = nullptr;
    BML::Behavior::Session m_Session;
    BML::Behavior::Plan m_Plan;
    BML::Behavior::Plan m_SelfPlan;
    BML::Behavior::GraphPatch m_Patch;
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
    bool m_PatchPassed = false;
    bool m_PatchClosePassed = false;
    bool m_Done = false;
};

} // namespace

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new BehaviorFacadeTest(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}

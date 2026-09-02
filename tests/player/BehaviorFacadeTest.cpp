// Exercises the published Patch, Plan, and Hook surface the way a Mod author would:
// only BML/Behavior.hpp, no private Behavior headers. The one test affordance
// is ObserveScript, which puts a script this probe created itself into the
// Plans world; the game feeds real scripts in the same way when it loads them.
#include "Api/BehaviorTestApi.h"
#include "BehaviorLifecycleFixtureApi.h"

#include "BML/Behavior.hpp"
#include "BML/IMod.h"
#include "CKAll.h"

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
        case State::ClosePatch: ClosePatch(); break;
        case State::WaitPatchConflict: WaitPatchConflict(); break;
        case State::WaitPatchRetired: WaitPatchRetired(); break;
        }
    }

    void OnUnload() override {
        m_Plan.Close();
        m_Patch.Close();
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
        if (!m_Entry || !m_Anchor)
            return false;
        m_AnchorId = m_Anchor->GetID();
        return m_Graph->GetSubBehaviorCount() == 2 &&
            m_Graph->GetSubBehaviorLinkCount() == 2;
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
            m_Graph->GetSubBehaviorLinkCount() == 5;
    }

    bool Restored() const {
        return m_Graph && m_Source && m_Sink && m_Anchor &&
            m_Anchor->GetID() == m_AnchorId &&
            m_Anchor->GetInBehaviorIO() == m_Source->GetOutput(0) &&
            m_Anchor->GetOutBehaviorIO() == m_Sink->GetInput(0) &&
            m_Graph->GetSubBehaviorCount() == 2 &&
            m_Graph->GetSubBehaviorLinkCount() == 2;
    }

    bool PatchInstalled() const {
        return m_Graph && m_Source && m_Sink && m_Anchor &&
            m_Anchor->GetID() == m_AnchorId &&
            m_Anchor->GetOutBehaviorIO() != m_Sink->GetInput(0) &&
            m_Graph->GetSubBehaviorCount() == 3 &&
            m_Graph->GetSubBehaviorLinkCount() == 3;
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
                submitted.Code(), submitted.Detail().Error,
                submitted.Detail().Phase, submitted.Detail().Message.c_str());
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
        int closed = BML_OK;
        std::thread closer([&] { closed = m_Plan.Close(); });
        closer.join();
        const auto closing = m_Plan.Read();
        if (closed != BML_ERROR_BUSY || !m_Plan || !closing ||
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
        const int closed = m_Plan.Close();
        if (closed != BML_OK || m_Plan) {
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
            ApplyPatch();
            return;
        }
        if (m_Frame > m_WaitUntil) {
            GetLogger()->Error(
                "Behavior plan hook state still held: references=%ld",
                static_cast<long>(m_Counters.use_count()));
            Finish(false, "release");
        }
    }

    void ApplyPatch() {
        BML_ObjectRef reference{};
        if (m_Test->ReferenceObject(
                m_Session.Handle(), m_Graph, &reference) != BML_OK) {
            Finish(false, "patch-reference");
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
                applied.Code(), applied.Detail().Error,
                applied.Detail().Phase, applied.Detail().Message.c_str());
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
        int closed = BML_OK;
        std::thread closer([&] { closed = m_Patch.Close(); });
        closer.join();
        const auto closing = m_Patch.Read();
        if (closed != BML_ERROR_BUSY || !m_Patch || !closing ||
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
            m_Patch.Close();
            m_Plan.Close();
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
            "Behavior graph patch: status=%s reason=%s apply=%s close=%s",
            passed ? "pass" : "fail", reason,
            m_PatchPassed ? "true" : "false",
            m_PatchClosePassed ? "true" : "false");
    }

    const BML_BehaviorTestInterface *m_Test = nullptr;
    BML::Behavior::Session m_Session;
    BML::Behavior::Plan m_Plan;
    BML::Behavior::GraphPatch m_Patch;
    std::shared_ptr<Counters> m_Counters = std::make_shared<Counters>();
    CK3dObject *m_Owner = nullptr;
    CKBehavior *m_Graph = nullptr;
    CKBehavior *m_Source = nullptr;
    CKBehavior *m_Sink = nullptr;
    CKBehaviorLink *m_Entry = nullptr;
    CKBehaviorLink *m_Anchor = nullptr;
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

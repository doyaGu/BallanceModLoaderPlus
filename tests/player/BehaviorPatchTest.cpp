#include "Api/BehaviorTestApi.h"
#include "BehaviorLifecycleFixtureApi.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "BML/ExecuteBB.h"
#include "BML/IMod.h"
#include "CKAll.h"

#include <chrono>
#include <cstdint>
#include <cstring>

namespace {

int RunDurableNode(const CKBehaviorContext &context) {
    CKBehavior *behavior = context.Behavior;
    if (!behavior)
        return CKBR_BEHAVIORERROR;
    for (int index = 0; index < behavior->GetInputCount(); ++index)
        behavior->ActivateInput(index, FALSE);
    for (int index = 0; index < behavior->GetOutputCount(); ++index)
        behavior->ActivateOutput(index);
    return CKBR_OK;
}

class BehaviorPatchTest final : public IMod {
public:
    explicit BehaviorPatchTest(IBML *bml) : IMod(bml) {
        AddDependency("BML");
    }

    const char *GetID() override { return "BehaviorPatchTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "Behavior Patch Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override {
        return "Validates the Loader-owned Behavior Patch implementation";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        const void *found = nullptr;
        if (BML_GetInterface(BML_BEHAVIOR_INTERFACE_ID,
                             BML_BEHAVIOR_INTERFACE_MAJOR,
                             &found) != BML_OK) {
            Finish(false, "behavior-interface");
            return;
        }
        m_Behavior = static_cast<const BML_BehaviorInterface *>(found);
        found = nullptr;
        if (BML_GetInterface(BML_BEHAVIOR_TEST_INTERFACE_ID,
                             BML_BEHAVIOR_TEST_INTERFACE_MAJOR,
                             &found) != BML_OK) {
            Finish(false, "test-interface");
            return;
        }
        m_Test = static_cast<const BML_BehaviorTestInterface *>(found);
        m_ModulePassed = IsLoaderAddress(
            reinterpret_cast<const void *>(
                reinterpret_cast<std::uintptr_t>(m_Test->InstallSplice)));

        BML_BehaviorStatus status{};
        status.StructSize = sizeof(status);
        if (!m_Behavior ||
            m_Behavior->OpenSession({}, &m_Session, &status) != BML_OK ||
            !m_Session) {
            Finish(false, "session");
        }
    }

    void OnStartLevel() override { m_LevelStarted = true; }

    void OnProcess() override {
        if (m_Done || !m_LevelStarted || !m_Session || !m_Test)
            return;
        ++m_Frame;
        switch (m_State) {
        case State::CreateVisual: CreateVisualGraph(); break;
        case State::ShowBaseline: ShowBaseline(); break;
        case State::ApplyVisual: ApplyVisualPatch(); break;
        case State::ShowPatched: ShowPatched(); break;
        case State::CloseVisual: CloseVisualPatch(); break;
        case State::ShowRestored: ShowRestored(); break;
        case State::CreateDurable: CreateDurableGraph(); break;
        case State::WaitDurable: WaitDurableInstall(); break;
        case State::WaitDurableExecuted: WaitDurableExecution(); break;
        case State::ResetDurable: ResetDurablePlan(); break;
        case State::ReloadDurable: WaitDurableReload(); break;
        case State::WaitDurableReloadExecuted:
            WaitDurableReloadExecution();
            break;
        case State::DeleteDurable: DeleteDurableGraph(); break;
        case State::WaitDurableDeleted: WaitDurableRemoval(); break;
        case State::RecreateDurable: RecreateDurableGraph(); break;
        case State::WaitDurableRecreated: WaitDurableRecreation(); break;
        case State::WaitDurableRecreatedExecuted:
            WaitDurableRecreatedExecution();
            break;
        case State::CloseDurable: CloseDurablePlan(); break;
        case State::WaitDurableClosed: WaitDurableClose(); break;
        case State::CreateReset:
            CreateGraph("player-patch-reset", State::Reset);
            break;
        case State::Reset: ResetPatches(); break;
        case State::CreateDeletion:
            CreateGraph("player-graph-deletion", State::DeleteGraph);
            break;
        case State::DeleteGraph: DeleteGraph(); break;
        case State::CreateRetirement:
            CreateGraph("player-owner-retirement", State::Retire);
            break;
        case State::Retire: RetirePatches(); break;
        }
    }

    void OnUnload() override {
        if (m_Test && m_Session && m_Plan)
            (void) m_Test->ClosePlan(m_Session, m_Plan);
        m_Plan = 0;
        if (m_Test && m_Session && m_Patch)
            (void) m_Test->ClosePatch(m_Session, m_Patch);
        m_Patch = 0;
        DestroyGraph();
        if (m_Behavior && m_Session)
            m_Behavior->CloseSession(m_Session);
        m_Session = nullptr;
    }

private:
    enum class State {
        CreateVisual,
        ShowBaseline,
        ApplyVisual,
        ShowPatched,
        CloseVisual,
        ShowRestored,
        CreateDurable,
        WaitDurable,
        WaitDurableExecuted,
        ResetDurable,
        ReloadDurable,
        WaitDurableReloadExecuted,
        DeleteDurable,
        WaitDurableDeleted,
        RecreateDurable,
        WaitDurableRecreated,
        WaitDurableRecreatedExecuted,
        CloseDurable,
        WaitDurableClosed,
        CreateReset,
        Reset,
        CreateDeletion,
        DeleteGraph,
        CreateRetirement,
        Retire,
    };

    bool IsLoaderAddress(const void *address) const {
        if (!address)
            return false;
        HMODULE module = nullptr;
        if (!::GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                static_cast<LPCSTR>(address), &module)) {
            return false;
        }
        char path[MAX_PATH] = {};
        if (!::GetModuleFileNameA(module, path, MAX_PATH))
            return false;
        const char *name = std::strrchr(path, '\\');
        name = name ? name + 1 : path;
        return _stricmp(name, "BMLPlus.dll") == 0;
    }

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

    bool CreateGraphObjects(const char *suffix) {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        CKLevel *level = context ? context->GetCurrentLevel() : nullptr;
        CKScene *scene = context ? context->GetCurrentScene() : nullptr;
        if (!context || !level || !scene)
            return false;

        m_Owner = CK3dObject::Cast(context->CreateObject(
            CKCID_3DOBJECT, const_cast<CKSTRING>("__BML_Patch_Owner"),
            static_cast<CK_OBJECTCREATION_OPTIONS>(
                CK_OBJECTCREATION_DYNAMIC | CK_OBJECTCREATION_ACTIVATE)));
        m_Graph = CKBehavior::Cast(context->CreateObject(
            CKCID_BEHAVIOR, const_cast<CKSTRING>(suffix),
            CK_OBJECTCREATION_DYNAMIC));
        if (!m_Owner || !m_Graph || level->AddObject(m_Owner) != CK_OK)
            return false;
        if (scene != level->GetLevelScene())
            (void) scene->AddObject(m_Owner);
        scene->Activate(m_Owner, TRUE);

        m_Graph->UseGraph();
        m_Graph->SetType(CKBEHAVIORTYPE_SCRIPT);
        return m_Graph->SetOwner(m_Owner, FALSE) == CK_OK &&
            m_Graph->CreateInput("Start") &&
            m_Graph->CreateOutput("Done") &&
            m_Owner->AddScript(m_Graph) == CK_OK;
    }

    void RunGraph() {
        CKScene *scene = m_BML && m_BML->GetCKContext()
            ? m_BML->GetCKContext()->GetCurrentScene() : nullptr;
        if (!scene || !m_Graph)
            return;
        m_Graph->ActivateInput(0, FALSE);
        m_Graph->ActivateOutput(0, FALSE);
        if (m_Baseline) {
            m_Baseline->ActivateInput(0, FALSE);
            m_Baseline->ActivateOutput(0, FALSE);
        }
        scene->Activate(m_Graph, TRUE);
        m_Graph->ActivateInput(0, TRUE);
        m_ExecuteStarted = m_Frame;
        m_StageObserved = false;
        m_StageLogged = false;
    }

    void CreateVisualGraph() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        CKLevel *level = context ? context->GetCurrentLevel() : nullptr;
        CKScene *scene = context ? context->GetCurrentScene() : nullptr;
        if (!context || !level || !scene)
            return;

        m_Display = static_cast<CK2dEntity *>(context->CreateObject(
            CKCID_2DENTITY,
            const_cast<CKSTRING>("__BML_Visible_Patch_Result"),
            CK_OBJECTCREATION_DYNAMIC));
        if (!m_Display || level->AddObject(m_Display) != CK_OK) {
            Finish(false, "visual-display-create");
            DestroyGraph();
            return;
        }
        m_Display->SetHomogeneousCoordinates();
        m_Display->EnableClipToCamera(false);
        m_Display->EnableRatioOffset(false);
        m_Display->SetPosition(Vx2DVector(0.04f, 0.05f), TRUE);
        m_Display->SetSize(Vx2DVector(0.88f, 0.42f), TRUE);
        m_Display->SetZOrder(100);
        m_Display->Show(CKSHOW);
        if (scene != level->GetLevelScene())
            scene->AddObject(m_Display);
        scene->Activate(m_Display, TRUE);

        m_PatchDisplay = static_cast<CK2dEntity *>(context->CreateObject(
            CKCID_2DENTITY,
            const_cast<CKSTRING>("__BML_Visible_Patch_Node_Result"),
            CK_OBJECTCREATION_DYNAMIC));
        if (!m_PatchDisplay || level->AddObject(m_PatchDisplay) != CK_OK) {
            Finish(false, "visual-patch-display-create");
            DestroyGraph();
            return;
        }
        m_PatchDisplay->SetHomogeneousCoordinates();
        m_PatchDisplay->EnableClipToCamera(false);
        m_PatchDisplay->EnableRatioOffset(false);
        m_PatchDisplay->SetPosition(Vx2DVector(0.04f, 0.05f), TRUE);
        m_PatchDisplay->SetSize(Vx2DVector(0.88f, 0.42f), TRUE);
        m_PatchDisplay->SetZOrder(101);
        m_PatchDisplay->Show(CKSHOW);
        if (scene != level->GetLevelScene())
            scene->AddObject(m_PatchDisplay);
        scene->Activate(m_PatchDisplay, TRUE);

        m_Graph = CKBehavior::Cast(context->CreateObject(
            CKCID_BEHAVIOR,
            const_cast<CKSTRING>("__BML_Visible_Patch_Graph"),
            CK_OBJECTCREATION_DYNAMIC));
        if (!m_Graph) {
            Finish(false, "visual-graph-create");
            DestroyGraph();
            return;
        }
        m_Graph->UseGraph();
        m_Graph->SetType(CKBEHAVIORTYPE_SCRIPT);
        if (m_Graph->SetOwner(m_Display, FALSE) != CK_OK ||
            !m_Graph->CreateInput("Start") ||
            !m_Graph->CreateOutput("Done") ||
            m_Display->AddScript(m_Graph) != CK_OK) {
            Finish(false, "visual-graph-layout");
            DestroyGraph();
            return;
        }

        m_Baseline = ExecuteBB::Create2DText(
            m_Graph, m_Display, ExecuteBB::GAMEFONT_01,
            "BEHAVIOR PATCH\nORIGINAL GRAPH\n1 NODE  2 LINKS",
            ALIGN_TOPLEFT, VxRect(8.0f, 8.0f, 8.0f, 8.0f),
            Vx2DVector(0.0f, 0.0f), Vx2DVector(0.0f, 0.0f),
            nullptr, 0.1f, nullptr, TEXT_SCREEN | TEXT_WORDWRAP);
        if (!m_Baseline || !m_Baseline->GetInput(0) ||
            !m_Baseline->GetOutput(0)) {
            Finish(false, "visual-baseline-create");
            DestroyGraph();
            return;
        }
        m_Entry = AddLink(m_Graph->GetInput(0), m_Baseline->GetInput(0));
        m_Anchor = AddLink(m_Baseline->GetOutput(0), m_Graph->GetOutput(0));
        if (!m_Entry || !m_Anchor ||
            m_Graph->GetSubBehaviorCount() != 1 ||
            m_Graph->GetSubBehaviorLinkCount() != 2) {
            Finish(false, "visual-graph-layout");
            DestroyGraph();
            return;
        }
        m_AnchorId = m_Anchor->GetID();
        RunGraph();
        m_State = State::ShowBaseline;
    }

    bool ObserveVisualExecution(const char *stage, int nodes, int links) {
        if (!m_StageObserved && m_Graph && m_Graph->GetOutput(0) &&
            m_Graph->GetOutput(0)->IsActive()) {
            m_StageObserved = true;
        }
        if (!m_StageObserved) {
            if (m_Frame - m_ExecuteStarted > 20)
                Finish(false, stage);
            return false;
        }
        if (!m_StageLogged) {
            m_StageLogged = true;
            m_VisualStageAt = std::chrono::steady_clock::now();
            GetLogger()->Info(
                "Behavior patch visual: stage=%s anchor=%d nodes=%d links=%d",
                stage, static_cast<int>(m_AnchorId), nodes, links);
        }
        return std::chrono::steady_clock::now() - m_VisualStageAt >=
            std::chrono::milliseconds(1500);
    }

    void ShowBaseline() {
        if (ObserveVisualExecution("baseline", 1, 2))
            m_State = State::ApplyVisual;
    }

    void ApplyVisualPatch() {
        CKScene *scene = m_BML && m_BML->GetCKContext()
            ? m_BML->GetCKContext()->GetCurrentScene() : nullptr;
        if (scene && m_Graph)
            scene->DeActivate(m_Graph);
        const int installed = m_Test->InstallTextSplice(
            m_Session, m_Graph, m_Anchor, m_PatchDisplay,
            "PATCH BB EXECUTED\n2 NODES  3 LINKS",
            "player-visible-text-splice", &m_Patch);
        std::uint32_t state = 0;
        const bool applied = installed == BML_OK && m_Patch != 0 &&
            m_Test->ReadPatch(m_Session, m_Patch, &state) == BML_OK &&
            state == BML_BEHAVIOR_TEST_PATCH_ACTIVE &&
            m_Anchor->GetID() == m_AnchorId &&
            m_Anchor->GetOutBehaviorIO() != m_Graph->GetOutput(0) &&
            m_Graph->GetSubBehaviorCount() == 2 &&
            m_Graph->GetSubBehaviorLinkCount() == 3;
        m_ApplyPassed = m_ApplyPassed && applied;
        if (!applied) {
            Finish(false, "visual-patch-apply");
            return;
        }
        RunGraph();
        m_State = State::ShowPatched;
    }

    void ShowPatched() {
        if (ObserveVisualExecution("active", 2, 3)) {
            m_ExecutePassed = true;
            m_State = State::CloseVisual;
        }
    }

    bool RestoredVisualGraph() const {
        return m_Graph && m_Anchor && m_Anchor->GetID() == m_AnchorId &&
            m_Anchor->GetOutBehaviorIO() == m_Graph->GetOutput(0) &&
            m_Graph->GetSubBehaviorCount() == 1 &&
            m_Graph->GetSubBehaviorLinkCount() == 2;
    }

    void CloseVisualPatch() {
        CKScene *scene = m_BML && m_BML->GetCKContext()
            ? m_BML->GetCKContext()->GetCurrentScene() : nullptr;
        if (scene && m_Graph)
            scene->DeActivate(m_Graph);
        const int closed = m_Test->ClosePatch(m_Session, m_Patch);
        m_Patch = 0;
        m_ClosePassed = closed == BML_OK;
        m_RestorePassed = m_ClosePassed && RestoredVisualGraph();
        if (!m_RestorePassed) {
            Finish(false, "visual-patch-close");
            return;
        }
        RunGraph();
        m_State = State::ShowRestored;
    }

    void ShowRestored() {
        if (!ObserveVisualExecution("restored", 1, 2))
            return;
        m_VisualPassed = true;
        DestroyGraph();
        m_State = State::CreateDurable;
    }

    bool AddDurableNode(const char *name, CKBehavior *&out) {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        out = context ? CKBehavior::Cast(context->CreateObject(
            CKCID_BEHAVIOR, const_cast<CKSTRING>(name),
            CK_OBJECTCREATION_DYNAMIC)) : nullptr;
        if (!out)
            return false;
        out->UseFunction();
        out->SetType(CKBEHAVIORTYPE_BASE);
        out->SetFunction(RunDurableNode);
        return out->CreateInput("In") && out->CreateOutput("Out") &&
            m_Graph->AddSubBehavior(out) == CK_OK;
    }

    bool BuildDurableGraph() {
        if (!CreateGraphObjects("__BML_Durable_Patch"))
            return false;
        if (!AddDurableNode("Durable Source", m_DurableSource) ||
            !AddDurableNode("Durable Sink", m_DurableSink))
            return false;
        m_Entry = AddLink(m_Graph->GetInput(0),
                          m_DurableSource->GetInput(0));
        m_Anchor = AddLink(m_DurableSource->GetOutput(0),
                           m_DurableSink->GetInput(0));
        if (!m_Entry || !m_Anchor)
            return false;
        m_AnchorId = m_Anchor->GetID();
        return m_Graph->GetSubBehaviorCount() == 2 &&
            m_Graph->GetSubBehaviorLinkCount() == 2;
    }

    bool ReadDurable(std::uint32_t expectedState,
                     std::uint32_t expectedMatches,
                     std::uint32_t expectedInstallations,
                     std::uint64_t *world = nullptr) const {
        std::uint32_t state = 0;
        std::uint32_t matches = 0;
        std::uint32_t installations = 0;
        std::uint64_t currentWorld = 0;
        const bool read = m_Test && m_Session && m_Plan &&
            m_Test->ReadPlan(m_Session, m_Plan, &state, &matches,
                             &installations, &currentWorld) == BML_OK;
        if (world)
            *world = currentWorld;
        return read && state == expectedState &&
            matches == expectedMatches &&
            installations == expectedInstallations;
    }

    bool DurableRelationsInstalled() const {
        if (!m_Graph || !m_DurableSource || !m_DurableSink ||
            m_Graph->GetSubBehaviorCount() != 5) {
            return false;
        }
        CKBehavior *block = nullptr;
        for (int index = 0; index < m_Graph->GetSubBehaviorCount(); ++index) {
            CKBehavior *candidate = m_Graph->GetSubBehavior(index);
            if (candidate != m_DurableSource &&
                candidate != m_DurableSink &&
                candidate->GetInputParameterCount() == 4) {
                block = candidate;
                break;
            }
        }
        if (!block || block->GetInputCount() != 2 ||
            block->GetOutputCount() != 2 ||
            block->GetInputParameterCount() != 4 ||
            block->GetOutputParameterCount() != 2) {
            return false;
        }
        CKParameterIn *source = block->GetInputParameter(0);
        CKParameterIn *literal = block->GetInputParameter(1);
        CKParameterIn *direct = block->GetInputParameter(2);
        CKParameterIn *shared = block->GetInputParameter(3);
        CKParameterOut *value = block->GetOutputParameter(0);
        CKParameterOut *destination = block->GetOutputParameter(1);
        int number = 0;
        return source && literal && direct && shared && value && destination &&
            literal->GetRealSource() &&
            literal->GetRealSource()->GetValue(&number) == CK_OK &&
            number == 41 && direct->GetRealSource() == value &&
            shared->GetRealSource() == source->GetRealSource() &&
            value->GetDestinationCount() == 1 &&
            value->GetDestination(0) == destination;
    }

    bool DurableInstalled() const {
        return m_Graph && m_DurableSource && m_DurableSink && m_Anchor &&
            m_Anchor->GetID() == m_AnchorId &&
            m_Anchor->GetOutBehaviorIO() != m_DurableSink->GetInput(0) &&
            m_Graph->GetSubBehaviorCount() == 5 &&
            m_Graph->GetSubBehaviorLinkCount() == 5 &&
            DurableRelationsInstalled();
    }

    bool DurableRestored() const {
        return m_Graph && m_DurableSource && m_DurableSink && m_Anchor &&
            m_Anchor->GetID() == m_AnchorId &&
            m_Anchor->GetInBehaviorIO() == m_DurableSource->GetOutput(0) &&
            m_Anchor->GetOutBehaviorIO() == m_DurableSink->GetInput(0) &&
            m_Graph->GetSubBehaviorCount() == 2 &&
            m_Graph->GetSubBehaviorLinkCount() == 2;
    }

    bool ReadHooks(std::uint32_t retains, std::uint32_t releases,
                   std::uint32_t taps, std::uint32_t afters) const {
        std::uint32_t actualRetains = 0;
        std::uint32_t actualReleases = 0;
        std::uint32_t actualTaps = 0;
        std::uint32_t actualAfters = 0;
        return m_Test && m_Session && m_Test->ReadHooks &&
            m_Test->ReadHooks(
                m_Session, &actualRetains, &actualReleases,
                &actualTaps, &actualAfters) == BML_OK &&
            actualRetains == retains && actualReleases == releases &&
            actualTaps == taps && actualAfters == afters;
    }

    void RunDurableGraph() {
        CKScene *scene = m_BML && m_BML->GetCKContext()
            ? m_BML->GetCKContext()->GetCurrentScene() : nullptr;
        if (!scene || !m_Graph || !m_DurableSource)
            return;
        m_Graph->ActivateInput(0, FALSE);
        m_Graph->ActivateOutput(0, FALSE);
        scene->Activate(m_Graph, TRUE);
        m_Graph->ActivateInput(0, TRUE);
        m_DurableWaitUntil = m_Frame + 20;
    }

    void CreateDurableGraph() {
        if (!BuildDurableGraph()) {
            Finish(false, "durable-graph-create");
            DestroyGraph();
            return;
        }
        BML_BehaviorGuid prototype{
            static_cast<std::uint32_t>(BML_LIFECYCLE_FIXTURE_GUID.d1),
            static_cast<std::uint32_t>(BML_LIFECYCLE_FIXTURE_GUID.d2)};
        const int observed = m_Test->ObserveScript(m_Session, m_Graph);
        const int submitted = m_Test->SubmitEdit(
            m_Session, "__BML_Durable_Patch", "Durable Source",
            "Durable Sink", prototype, "player-durable-edit", &m_Plan);
        if (observed != BML_OK || submitted != BML_OK || !m_Plan) {
            Finish(false, "durable-submit");
            return;
        }
        m_State = State::WaitDurable;
    }

    void WaitDurableInstall() {
        std::uint64_t world = 0;
        m_DurableRelationsPassed = m_DurableRelationsPassed &&
            DurableRelationsInstalled();
        if (!ReadDurable(BML_BEHAVIOR_TEST_PLAN_ACTIVE, 1, 1, &world) ||
            !DurableInstalled() || !ReadHooks(2, 0, 0, 0)) {
            Finish(false, "durable-install");
            return;
        }
        m_DurableWorld = world;
        RunDurableGraph();
        m_State = State::WaitDurableExecuted;
    }

    void WaitDurableExecution() {
        if (ReadHooks(2, 0, 1, 1)) {
            m_State = State::ResetDurable;
            return;
        }
        if (m_Frame > m_DurableWaitUntil)
            Finish(false, "durable-hooks-first");
    }

    void ResetDurablePlan() {
        const int reset = m_Test->ResetPlans(m_Session);
        const bool leftWorld = reset == BML_OK &&
            ReadDurable(BML_BEHAVIOR_TEST_PLAN_UNSATISFIED, 0, 0) &&
            DurableRestored() && ReadHooks(2, 0, 1, 1);
        if (!leftWorld ||
            m_Test->ObserveScript(m_Session, m_Graph) != BML_OK) {
            Finish(false, "durable-reset");
            return;
        }
        m_State = State::ReloadDurable;
    }

    void WaitDurableReload() {
        std::uint64_t world = 0;
        m_DurableRelationsPassed = m_DurableRelationsPassed &&
            DurableRelationsInstalled();
        if (!ReadDurable(BML_BEHAVIOR_TEST_PLAN_ACTIVE, 1, 1, &world) ||
            world <= m_DurableWorld || !DurableInstalled() ||
            !ReadHooks(2, 0, 1, 1)) {
            Finish(false, "durable-reload");
            return;
        }
        m_DurableWorld = world;
        RunDurableGraph();
        m_State = State::WaitDurableReloadExecuted;
    }

    void WaitDurableReloadExecution() {
        if (ReadHooks(2, 0, 2, 2)) {
            m_State = State::DeleteDurable;
            return;
        }
        if (m_Frame > m_DurableWaitUntil)
            Finish(false, "durable-hooks-reload");
    }

    void DeleteDurableGraph() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        if (!context || !m_Graph || !m_Owner) {
            Finish(false, "durable-delete-setup");
            return;
        }
        CKBehavior *graph = m_Graph;
        CK3dObject *owner = m_Owner;
        const CK_ID graphId = graph->GetID();
        (void) owner->RemoveScript(graphId);
        m_Graph = nullptr;
        m_DurableSource = nullptr;
        m_DurableSink = nullptr;
        m_Anchor = nullptr;
        m_AnchorId = 0;
        m_Owner = nullptr;
        const CKERROR destroyed = context->DestroyObject(graph);
        context->DestroyObject(owner);
        CKObject *remaining = context->GetObject(graphId);
        if (destroyed != CK_OK ||
            (remaining && !remaining->IsToBeDeleted())) {
            Finish(false, "durable-delete");
            return;
        }
        m_DurableWaitUntil = m_Frame + 30;
        m_State = State::WaitDurableDeleted;
    }

    void WaitDurableRemoval() {
        if (ReadDurable(BML_BEHAVIOR_TEST_PLAN_UNSATISFIED, 0, 0)) {
            if (!ReadHooks(2, 0, 2, 2)) {
                Finish(false, "durable-hooks-delete");
                return;
            }
            m_State = State::RecreateDurable;
            return;
        }
        if (m_Frame < m_DurableWaitUntil)
            return;
        std::uint32_t state = 0;
        std::uint32_t matches = 0;
        std::uint32_t installations = 0;
        std::uint64_t world = 0;
        const int read = m_Test->ReadPlan(
            m_Session, m_Plan, &state, &matches, &installations, &world);
        GetLogger()->Info(
            "Behavior durable removal: read=%d state=%u matches=%u installations=%u world=%llu",
            read, state, matches, installations,
            static_cast<unsigned long long>(world));
        Finish(false, "durable-remove");
    }

    void RecreateDurableGraph() {
        if (!BuildDurableGraph() ||
            m_Test->ObserveScript(m_Session, m_Graph) != BML_OK) {
            Finish(false, "durable-recreate-graph");
            DestroyGraph();
            return;
        }
        m_State = State::WaitDurableRecreated;
    }

    void WaitDurableRecreation() {
        std::uint64_t world = 0;
        m_DurableRelationsPassed = m_DurableRelationsPassed &&
            DurableRelationsInstalled();
        if (!ReadDurable(BML_BEHAVIOR_TEST_PLAN_ACTIVE, 1, 1, &world) ||
            world != m_DurableWorld || !DurableInstalled()) {
            Finish(false, "durable-recreate");
            return;
        }
        RunDurableGraph();
        m_State = State::WaitDurableRecreatedExecuted;
    }

    void WaitDurableRecreatedExecution() {
        if (ReadHooks(2, 0, 3, 3)) {
            m_DurableHooksPassed = true;
            m_State = State::CloseDurable;
            return;
        }
        if (m_Frame > m_DurableWaitUntil)
            Finish(false, "durable-hooks-recreate");
    }

    void CloseDurablePlan() {
        if (m_Test->ClosePlan(m_Session, m_Plan) != BML_OK) {
            Finish(false, "durable-close-request");
            return;
        }
        m_State = State::WaitDurableClosed;
    }

    void WaitDurableClose() {
        std::uint32_t state = 0;
        std::uint32_t matches = 0;
        std::uint32_t installations = 0;
        std::uint64_t world = 0;
        const bool stale = m_Test->ReadPlan(
            m_Session, m_Plan, &state, &matches, &installations, &world) !=
            BML_OK;
        m_DurablePassed = stale && DurableRestored() &&
            ReadHooks(2, 2, 3, 3);
        m_Plan = 0;
        if (!m_DurablePassed) {
            Finish(false, "durable-close");
            return;
        }
        DestroyGraph();
        m_State = State::CreateReset;
    }

    void CreateGraph(const char *patchName, State next) {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        if (!context)
            return;
        if (!CreateGraphObjects("__BML_Patch_Graph")) {
            Finish(false, "graph-create");
            DestroyGraph();
            return;
        }
        m_Anchor = AddLink(m_Graph->GetInput(0), m_Graph->GetOutput(0));
        if (!m_Anchor) {
            Finish(false, "graph-link");
            DestroyGraph();
            return;
        }
        m_AnchorId = m_Anchor->GetID();
        BML_BehaviorGuid prototype{
            static_cast<std::uint32_t>(BML_LIFECYCLE_FIXTURE_GUID.d1),
            static_cast<std::uint32_t>(BML_LIFECYCLE_FIXTURE_GUID.d2)};
        const int installed = m_Test->InstallSplice(
            m_Session, m_Graph, m_Anchor, prototype,
            patchName, &m_Patch);
        std::uint32_t state = 0;
        const bool applied = installed == BML_OK && m_Patch != 0 &&
            m_Test->ReadPatch(m_Session, m_Patch, &state) == BML_OK &&
            state == BML_BEHAVIOR_TEST_PATCH_ACTIVE &&
            m_Anchor->GetID() == m_AnchorId &&
            m_Anchor->GetOutBehaviorIO() != m_Graph->GetOutput(0) &&
            m_Graph->GetSubBehaviorCount() == 1 &&
            m_Graph->GetSubBehaviorLinkCount() == 2;
        m_ApplyPassed = m_ApplyPassed && applied;
        if (!applied) {
            Finish(false, "patch-apply");
            return;
        }

        m_State = next;
    }

    bool Restored() const {
        return m_Graph && m_Anchor && m_Anchor->GetID() == m_AnchorId &&
            m_Anchor->GetOutBehaviorIO() == m_Graph->GetOutput(0) &&
            m_Graph->GetSubBehaviorCount() == 0 &&
            m_Graph->GetSubBehaviorLinkCount() == 1;
    }

    bool StalePatch() const {
        std::uint32_t state = 0;
        return m_Test && m_Session && m_Patch &&
            m_Test->ReadPatch(m_Session, m_Patch, &state) != BML_OK;
    }

    void ResetPatches() {
        const int reset = m_Test->ResetPatches(m_Session);
        m_ResetPassed = reset == BML_OK && StalePatch() && Restored();
        m_Patch = 0;
        if (!m_ResetPassed) {
            Finish(false, "patch-reset");
            return;
        }
        DestroyGraph();
        m_State = State::CreateDeletion;
    }

    void DeleteGraph() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        if (!context || !m_Graph || !m_Owner) {
            Finish(false, "patch-delete-setup");
            return;
        }

        const CK_ID graphId = m_Graph->GetID();
        CKBehavior *graph = m_Graph;
        CK3dObject *owner = m_Owner;
        (void) owner->RemoveScript(graphId);
        m_Graph = nullptr;
        m_Anchor = nullptr;
        m_Owner = nullptr;
        const CKERROR destroyed = context->DestroyObject(graph);
        CKObject *remaining = context->GetObject(graphId);
        const bool deletionObserved = destroyed == CK_OK &&
            (!remaining || remaining->IsToBeDeleted());
        m_DeletionPassed = deletionObserved && StalePatch();
        m_Patch = 0;
        context->DestroyObject(owner);
        if (!m_DeletionPassed) {
            Finish(false, "patch-delete");
            return;
        }
        m_State = State::CreateRetirement;
    }

    void RetirePatches() {
        const int retired = m_Test->RetirePatches(m_Session);
        m_RetirementPassed = retired == BML_OK && StalePatch() && Restored();
        m_Patch = 0;
        if (m_RetirementPassed)
            DestroyGraph();
        Finish(m_ModulePassed && m_VisualPassed && m_DurablePassed &&
                   m_DurableRelationsPassed &&
                   m_DurableHooksPassed &&
                   m_ApplyPassed &&
                   m_ExecutePassed &&
                   m_ClosePassed && m_RestorePassed && m_ResetPassed &&
                   m_DeletionPassed && m_RetirementPassed,
               m_RetirementPassed ? "complete" : "patch-retirement");
    }

    void DestroyGraph() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        if (!context)
            return;
        if (m_Graph && m_Entry) {
            m_Graph->RemoveSubBehaviorLink(m_Entry);
            context->DestroyObject(m_Entry);
        }
        m_Entry = nullptr;
        if (m_Graph && m_Anchor) {
            m_Graph->RemoveSubBehaviorLink(m_Anchor);
            context->DestroyObject(m_Anchor);
        }
        m_Anchor = nullptr;
        m_AnchorId = 0;
        if (m_Graph && m_Baseline)
            m_Graph->RemoveSubBehavior(m_Baseline);
        if (m_Baseline)
            context->DestroyObject(m_Baseline);
        m_Baseline = nullptr;
        if (m_Graph && m_DurableSource)
            m_Graph->RemoveSubBehavior(m_DurableSource);
        if (m_DurableSource)
            context->DestroyObject(m_DurableSource);
        m_DurableSource = nullptr;
        if (m_Graph && m_DurableSink)
            m_Graph->RemoveSubBehavior(m_DurableSink);
        if (m_DurableSink)
            context->DestroyObject(m_DurableSink);
        m_DurableSink = nullptr;
        if (m_Display && m_Graph)
            (void) m_Display->RemoveScript(m_Graph->GetID());
        else if (m_Owner && m_Graph)
            (void) m_Owner->RemoveScript(m_Graph->GetID());
        if (m_Graph)
            context->DestroyObject(m_Graph);
        m_Graph = nullptr;
        if (m_Owner)
            context->DestroyObject(m_Owner);
        m_Owner = nullptr;
        if (m_Display)
            context->DestroyObject(m_Display);
        m_Display = nullptr;
        if (m_PatchDisplay)
            context->DestroyObject(m_PatchDisplay);
        m_PatchDisplay = nullptr;
    }

    void Finish(bool passed, const char *reason) {
        if (m_Done)
            return;
        m_Done = true;
        GetLogger()->Info(
            "Behavior patch: status=%s reason=%s module=%s visual=%s durable=%s relations=%s apply=%s execute=%s close=%s restore=%s reset=%s deletion=%s retirement=%s hooks=%s",
            passed ? "pass" : "fail", reason,
            m_ModulePassed ? "true" : "false",
            m_VisualPassed ? "true" : "false",
            m_DurablePassed ? "true" : "false",
            m_DurableRelationsPassed ? "true" : "false",
            m_ApplyPassed ? "true" : "false",
            m_ExecutePassed ? "true" : "false",
            m_ClosePassed ? "true" : "false",
            m_RestorePassed ? "true" : "false",
            m_ResetPassed ? "true" : "false",
            m_DeletionPassed ? "true" : "false",
            m_RetirementPassed ? "true" : "false",
            m_DurableHooksPassed ? "true" : "false");
    }

    const BML_BehaviorInterface *m_Behavior = nullptr;
    const BML_BehaviorTestInterface *m_Test = nullptr;
    BML_BehaviorSession m_Session = nullptr;
    std::uintptr_t m_Patch = 0;
    std::uintptr_t m_Plan = 0;
    CK3dObject *m_Owner = nullptr;
    CKBehavior *m_Graph = nullptr;
    CKBehavior *m_Baseline = nullptr;
    CKBehavior *m_DurableSource = nullptr;
    CKBehavior *m_DurableSink = nullptr;
    CK2dEntity *m_Display = nullptr;
    CK2dEntity *m_PatchDisplay = nullptr;
    CKBehaviorLink *m_Entry = nullptr;
    CKBehaviorLink *m_Anchor = nullptr;
    CK_ID m_AnchorId = 0;
    std::uint64_t m_DurableWorld = 0;
    int m_DurableWaitUntil = 0;
    State m_State = State::CreateVisual;
    int m_Frame = 0;
    int m_ExecuteStarted = 0;
    std::chrono::steady_clock::time_point m_VisualStageAt{};
    bool m_StageObserved = false;
    bool m_StageLogged = false;
    bool m_LevelStarted = false;
    bool m_ModulePassed = false;
    bool m_VisualPassed = false;
    bool m_DurablePassed = false;
    bool m_DurableRelationsPassed = true;
    bool m_DurableHooksPassed = false;
    bool m_ApplyPassed = true;
    bool m_ExecutePassed = false;
    bool m_ClosePassed = false;
    bool m_RestorePassed = false;
    bool m_ResetPassed = false;
    bool m_DeletionPassed = false;
    bool m_RetirementPassed = false;
    bool m_Done = false;
};

} // namespace

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new BehaviorPatchTest(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}

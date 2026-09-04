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

#include "PlayerProbe.h"

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

// Records what one Behavior callback observes while it closes the Patch that
// owns the running node.
struct PatchCloseProbe {
    const BML_BehaviorInterface *Behavior = nullptr;
    const BML_BehaviorTestInterface *Test = nullptr;
    BML_BehaviorSession Session = nullptr;
    std::uintptr_t Patch = 0;
    CKBehavior *Graph = nullptr;
    CKBehaviorLink *Anchor = nullptr;
    std::uint32_t Calls = 0;
    int FirstClose = BML_OK;
    int SecondClose = BML_OK;
    int Read = BML_ERROR_FAIL;
    std::uint32_t State = 0;
    int Nodes = -1;
    int Links = -1;
    bool Routed = false;
};

PatchCloseProbe g_PatchClose;

// Runs inside CK dispatch. Closing the owning Patch from here must defer the
// inverse to the next safe point instead of tearing the graph down under the
// running node, and a second close from the same callback must not queue a
// second teardown.
int RunPatchCloseNode(const CKBehaviorContext &context) {
    CKBehavior *behavior = context.Behavior;
    if (!behavior)
        return CKBR_BEHAVIORERROR;
    for (int index = 0; index < behavior->GetInputCount(); ++index)
        behavior->ActivateInput(index, FALSE);
    for (int index = 0; index < behavior->GetOutputCount(); ++index)
        behavior->ActivateOutput(index);

    PatchCloseProbe &probe = g_PatchClose;
    ++probe.Calls;
    if (probe.Calls != 1 || !probe.Behavior || !probe.Test || !probe.Session ||
        !probe.Patch)
        return CKBR_OK;
    const auto handle = reinterpret_cast<BML_BehaviorPatch>(probe.Patch);
    probe.FirstClose = probe.Behavior->ClosePatch(probe.Session, handle);
    probe.SecondClose = probe.Behavior->ClosePatch(probe.Session, handle);
    probe.Read = probe.Test->ReadPatch(probe.Session, probe.Patch,
                                       &probe.State);
    if (probe.Graph) {
        probe.Nodes = probe.Graph->GetSubBehaviorCount();
        probe.Links = probe.Graph->GetSubBehaviorLinkCount();
        probe.Routed = probe.Anchor &&
            probe.Anchor->GetOutBehaviorIO() != probe.Graph->GetOutput(0);
    }
    return CKBR_OK;
}

// Records what the native teardown of one Patch observes when it closes Patches
// from inside the Loader's own inverse.
struct TeardownReentryProbe {
    const BML_BehaviorInterface *Behavior = nullptr;
    const BML_BehaviorTestInterface *Test = nullptr;
    BML_BehaviorSession Session = nullptr;
    std::uintptr_t Patch = 0;
    std::uintptr_t Sibling = 0;
    CKBehavior *Graph = nullptr;
    std::uint32_t Calls = 0;
    int SelfClose = BML_OK;
    int SiblingClose = BML_OK;
    int Read = BML_ERROR_FAIL;
    std::uint32_t State = 0;
    int SiblingRead = BML_ERROR_FAIL;
    std::uint32_t SiblingState = 0;
    int Nodes = -1;
    int Links = -1;
};

TeardownReentryProbe g_Teardown;

// Runs inside the DETACH and DELETE callbacks the Loader drives while it
// closes the Patch that owns the fixture node. Closing that Patch again must
// answer Busy, and closing a sibling Patch on the same graph must wait for the
// next safe point instead of nesting a second inverse under the running one.
int CloseFromTeardown(CKBehavior *, void *) {
    TeardownReentryProbe &probe = g_Teardown;
    ++probe.Calls;
    if (probe.Calls != 1 || !probe.Behavior || !probe.Test || !probe.Session ||
        !probe.Patch || !probe.Sibling)
        return 0;
    probe.SelfClose = probe.Behavior->ClosePatch(
        probe.Session, reinterpret_cast<BML_BehaviorPatch>(probe.Patch));
    probe.SiblingClose = probe.Behavior->ClosePatch(
        probe.Session, reinterpret_cast<BML_BehaviorPatch>(probe.Sibling));
    probe.Read = probe.Test->ReadPatch(probe.Session, probe.Patch,
                                       &probe.State);
    probe.SiblingRead = probe.Test->ReadPatch(probe.Session, probe.Sibling,
                                              &probe.SiblingState);
    if (probe.Graph) {
        probe.Nodes = probe.Graph->GetSubBehaviorCount();
        probe.Links = probe.Graph->GetSubBehaviorLinkCount();
    }
    return 1;
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
        BML::PlayerTest::ProbeReport::Reset();
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
            CreateGraph("player-patch-reset", State::GraphChanged);
            break;
        case State::GraphChanged: ProbeGraphChanged(); break;
        case State::Reset: ResetPatches(); break;
        case State::CreateCallbackClose: CreateCallbackGraph(); break;
        case State::CallbackClose: ObserveCallbackClose(); break;
        case State::WaitCallbackClose: WaitCallbackRestored(); break;
        case State::CreateTeardownReentry: CreateTeardownGraph(); break;
        case State::TeardownReentry: ObserveTeardownReentry(); break;
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
        if (m_Test && m_Session && m_SiblingPatch)
            (void) m_Test->ClosePatch(m_Session, m_SiblingPatch);
        m_SiblingPatch = 0;
        ResetLifecycleFixture();
        g_PatchClose = {};
        g_Teardown = {};
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
        GraphChanged,
        Reset,
        CreateCallbackClose,
        CallbackClose,
        WaitCallbackClose,
        CreateTeardownReentry,
        TeardownReentry,
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

    bool AddFunctionNode(const char *name, CKBEHAVIORFCT function,
                         CKBehavior *&out) {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        out = context ? CKBehavior::Cast(context->CreateObject(
            CKCID_BEHAVIOR, const_cast<CKSTRING>(name),
            CK_OBJECTCREATION_DYNAMIC)) : nullptr;
        if (!out)
            return false;
        out->UseFunction();
        out->SetType(CKBEHAVIORTYPE_BASE);
        out->SetFunction(function);
        return out->CreateInput("In") && out->CreateOutput("Out") &&
            m_Graph->AddSubBehavior(out) == CK_OK;
    }

    bool AddDurableNode(const char *name, CKBehavior *&out) {
        return AddFunctionNode(name, RunDurableNode, out);
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

    // Mirrors the Splice the test seam installs, written as a published edit
    // program: create the fixture Block, name the anchor Link by its logical
    // endpoints, then route that Link through the Block.
    int ApplySplice(BML_ObjectRef graph, const char *name,
                    BML_BehaviorPatch *out, BML_BehaviorStatus *status) {
        BML_BehaviorEditStep steps[3]{};
        steps[0].StructSize = sizeof(steps[0]);
        steps[0].Kind = BML_BEHAVIOR_EDIT_ADD_BLOCK;
        steps[0].Result = 2;
        steps[0].Prototype.StructSize = sizeof(steps[0].Prototype);
        steps[0].Prototype.Prototype = {
            static_cast<std::uint32_t>(BML_LIFECYCLE_FIXTURE_GUID.d1),
            static_cast<std::uint32_t>(BML_LIFECYCLE_FIXTURE_GUID.d2)};

        steps[1].StructSize = sizeof(steps[1]);
        steps[1].Kind = BML_BEHAVIOR_EDIT_REQUIRE_LINK;
        steps[1].Result = 3;
        steps[1].Source.StructSize = sizeof(steps[1].Source);
        steps[1].Source.Handle = BML_BEHAVIOR_EDIT_GRAPH;
        steps[1].Source.Kind = BML_BEHAVIOR_SLOT_IN;
        steps[1].Source.Slot.StructSize = sizeof(steps[1].Source.Slot);
        steps[1].Source.Slot.Kind = BML_BEHAVIOR_SELECTOR_INDEX;
        steps[1].Source.Slot.Index = 0;
        steps[1].Sink.StructSize = sizeof(steps[1].Sink);
        steps[1].Sink.Handle = BML_BEHAVIOR_EDIT_GRAPH;
        steps[1].Sink.Kind = BML_BEHAVIOR_SLOT_OUT;
        steps[1].Sink.Slot.StructSize = sizeof(steps[1].Sink.Slot);
        steps[1].Sink.Slot.Kind = BML_BEHAVIOR_SELECTOR_INDEX;
        steps[1].Sink.Slot.Index = 0;

        steps[2].StructSize = sizeof(steps[2]);
        steps[2].Kind = BML_BEHAVIOR_EDIT_SPLICE;
        steps[2].Target = 3;
        steps[2].Node = 2;

        BML_BehaviorPatchSpec spec{};
        spec.StructSize = sizeof(spec);
        spec.Name = {name, static_cast<std::uint32_t>(std::strlen(name))};
        spec.Graph = graph;
        spec.Steps = steps;
        spec.StepCount = 3;
        return m_Behavior->ApplyPatch(m_Session, &spec, out, nullptr, status);
    }

    // The live Splice anchor belongs to the Patch that rerouted it, and the
    // Loader records its live shape, endpoints and initial delay included.
    // Changing that shape from outside the Patch has to make the next edit
    // compiled against this graph fail as GraphChanged, and restoring it has to
    // let the very same program through again.
    void ProbeGraphChanged() {
        BML_ObjectRef graph{};
        if (!m_Graph || !m_Anchor || !m_Behavior ||
            m_Test->ReferenceObject(m_Session, m_Graph, &graph) != BML_OK) {
            Finish(false, "graph-changed-reference");
            return;
        }

        const int delay = m_Anchor->GetInitialActivationDelay();
        m_Anchor->SetInitialActivationDelay(delay + 1);
        BML_BehaviorPatch rejectedPatch = nullptr;
        BML_BehaviorStatus rejectedStatus{};
        rejectedStatus.StructSize = sizeof(rejectedStatus);
        const int rejected = ApplySplice(graph, "player-graph-changed",
                                        &rejectedPatch, &rejectedStatus);
        m_Anchor->SetInitialActivationDelay(delay);

        BML_BehaviorPatch acceptedPatch = nullptr;
        BML_BehaviorStatus acceptedStatus{};
        acceptedStatus.StructSize = sizeof(acceptedStatus);
        const int readmitted = ApplySplice(graph, "player-graph-restored",
                                           &acceptedPatch, &acceptedStatus);
        std::uint32_t state = 0;
        const bool active = readmitted == BML_OK && acceptedPatch &&
            m_Test->ReadPatch(
                m_Session, reinterpret_cast<std::uintptr_t>(acceptedPatch),
                &state) == BML_OK;
        m_GraphChangedPassed = rejected != BML_OK && !rejectedPatch &&
            rejectedStatus.Error == BML_BEHAVIOR_ERROR_GRAPH_CHANGED &&
            active && state == BML_BEHAVIOR_TEST_PATCH_ACTIVE;
        GetLogger()->Info(
            "Behavior patch graph changed: status=%s rejected=%d error=%u readmitted=%d state=%u",
            m_GraphChangedPassed ? "pass" : "fail", rejected,
            static_cast<unsigned>(rejectedStatus.Error), readmitted,
            static_cast<unsigned>(state));
        if (acceptedPatch)
            (void) m_Behavior->ClosePatch(m_Session, acceptedPatch);
        if (rejectedPatch)
            (void) m_Behavior->ClosePatch(m_Session, rejectedPatch);
        if (!m_GraphChangedPassed) {
            Finish(false, "graph-changed");
            return;
        }
        m_State = State::Reset;
    }

    // Drives one graph whose own node closes the Patch spliced behind it.
    void CreateCallbackGraph() {
        if (!CreateGraphObjects("__BML_Patch_Callback") ||
            !AddFunctionNode("Callback Close", RunPatchCloseNode,
                             m_CallbackNode)) {
            Finish(false, "callback-graph-create");
            DestroyGraph();
            return;
        }
        m_Entry = AddLink(m_Graph->GetInput(0), m_CallbackNode->GetInput(0));
        m_Anchor = AddLink(m_CallbackNode->GetOutput(0), m_Graph->GetOutput(0));
        if (!m_Entry || !m_Anchor) {
            Finish(false, "callback-graph-link");
            DestroyGraph();
            return;
        }
        m_AnchorId = m_Anchor->GetID();

        BML_BehaviorGuid prototype{
            static_cast<std::uint32_t>(BML_LIFECYCLE_FIXTURE_GUID.d1),
            static_cast<std::uint32_t>(BML_LIFECYCLE_FIXTURE_GUID.d2)};
        const int installed = m_Test->InstallSplice(
            m_Session, m_Graph, m_Anchor, prototype,
            "player-callback-close", &m_Patch);
        std::uint32_t state = 0;
        if (installed != BML_OK || m_Patch == 0 ||
            m_Test->ReadPatch(m_Session, m_Patch, &state) != BML_OK ||
            state != BML_BEHAVIOR_TEST_PATCH_ACTIVE) {
            GetLogger()->Error(
                "Behavior patch callback install failed: code=%d state=%u",
                installed, static_cast<unsigned>(state));
            Finish(false, "callback-patch-apply");
            return;
        }

        g_PatchClose = {};
        g_PatchClose.Behavior = m_Behavior;
        g_PatchClose.Test = m_Test;
        g_PatchClose.Session = m_Session;
        g_PatchClose.Patch = m_Patch;
        g_PatchClose.Graph = m_Graph;
        g_PatchClose.Anchor = m_Anchor;

        CKScene *scene = m_BML && m_BML->GetCKContext()
            ? m_BML->GetCKContext()->GetCurrentScene() : nullptr;
        if (!scene) {
            Finish(false, "callback-graph-scene");
            return;
        }
        m_Graph->ActivateInput(0, FALSE);
        m_Graph->ActivateOutput(0, FALSE);
        scene->Activate(m_Graph, TRUE);
        m_Graph->ActivateInput(0, TRUE);
        m_CallbackWaitUntil = m_Frame + 20;
        m_State = State::CallbackClose;
    }

    void ObserveCallbackClose() {
        if (g_PatchClose.Calls == 0) {
            if (m_Frame > m_CallbackWaitUntil)
                Finish(false, "callback-close-run");
            return;
        }
        CKScene *scene = m_BML && m_BML->GetCKContext()
            ? m_BML->GetCKContext()->GetCurrentScene() : nullptr;
        if (scene && m_Graph)
            scene->DeActivate(m_Graph);

        m_CallbackClosePassed =
            g_PatchClose.FirstClose == BML_ERROR_BUSY &&
            g_PatchClose.SecondClose == BML_ERROR_BUSY &&
            g_PatchClose.Read == BML_OK &&
            g_PatchClose.State == BML_BEHAVIOR_TEST_PATCH_CLOSING &&
            g_PatchClose.Nodes == 2 && g_PatchClose.Links == 3 &&
            g_PatchClose.Routed;
        GetLogger()->Info(
            "Behavior patch callback close: status=%s first=%d second=%d state=%u nodes=%d links=%d routed=%s calls=%u",
            m_CallbackClosePassed ? "pass" : "fail",
            g_PatchClose.FirstClose, g_PatchClose.SecondClose,
            static_cast<unsigned>(g_PatchClose.State),
            g_PatchClose.Nodes, g_PatchClose.Links,
            g_PatchClose.Routed ? "true" : "false",
            static_cast<unsigned>(g_PatchClose.Calls));
        if (!m_CallbackClosePassed) {
            Finish(false, "callback-close");
            return;
        }
        m_CallbackWaitUntil = m_Frame + 20;
        m_State = State::WaitCallbackClose;
    }

    void WaitCallbackRestored() {
        const int nodes = m_Graph ? m_Graph->GetSubBehaviorCount() : -1;
        const int links = m_Graph ? m_Graph->GetSubBehaviorLinkCount() : -1;
        const bool stale = StalePatch();
        const bool restored = m_Graph && m_Anchor && m_CallbackNode &&
            m_Anchor->GetID() == m_AnchorId &&
            m_Anchor->GetOutBehaviorIO() == m_Graph->GetOutput(0) &&
            nodes == 1 && links == 2;
        if (!stale || !restored) {
            if (m_Frame <= m_CallbackWaitUntil)
                return;
            m_CallbackClosePassed = false;
            GetLogger()->Info(
                "Behavior patch callback restore: status=fail stale=%s nodes=%d links=%d",
                stale ? "true" : "false", nodes, links);
            Finish(false, "callback-close-restore");
            return;
        }
        GetLogger()->Info(
            "Behavior patch callback restore: status=pass stale=true nodes=%d links=%d",
            nodes, links);
        m_Patch = 0;
        g_PatchClose = {};
        DestroyGraph();
        m_State = State::CreateTeardownReentry;
    }

    struct LifecycleFixtureExports {
        BMLLifecycleFixtureResetTraceFn ResetTrace = nullptr;
        BMLLifecycleFixtureSetModeFn SetMode = nullptr;
        BMLLifecycleFixtureSetCloseHookFn SetCloseHook = nullptr;
        BMLLifecycleFixtureReadTraceFn ReadTrace = nullptr;

        explicit operator bool() const {
            return ResetTrace && SetMode && SetCloseHook && ReadTrace;
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
        exports.SetCloseHook =
            reinterpret_cast<BMLLifecycleFixtureSetCloseHookFn>(
                ::GetProcAddress(module, "BMLLifecycleFixtureSetCloseHook"));
        exports.ReadTrace = reinterpret_cast<BMLLifecycleFixtureReadTraceFn>(
            ::GetProcAddress(module, "BMLLifecycleFixtureReadTrace"));
        return exports;
    }

    static void ResetLifecycleFixture() {
        const LifecycleFixtureExports fixture = ResolveLifecycleFixture();
        if (!fixture)
            return;
        fixture.SetCloseHook(nullptr, nullptr);
        fixture.SetMode(BMLLifecycleFixtureMode::Normal);
    }

    // Two fixture Patches on one graph. Closing the first one on the game
    // thread runs its inverse synchronously; the fixture's teardown callbacks
    // then close both Patches again from inside that inverse.
    void CreateTeardownGraph() {
        const LifecycleFixtureExports fixture = ResolveLifecycleFixture();
        if (!fixture) {
            Finish(false, "teardown-fixture-exports");
            return;
        }
        if (!CreateGraphObjects("__BML_Patch_Teardown") ||
            !m_Graph->CreateOutput("Done2")) {
            Finish(false, "teardown-graph-create");
            DestroyGraph();
            return;
        }
        m_Anchor = AddLink(m_Graph->GetInput(0), m_Graph->GetOutput(0));
        m_SiblingAnchor = AddLink(m_Graph->GetInput(0), m_Graph->GetOutput(1));
        if (!m_Anchor || !m_SiblingAnchor) {
            Finish(false, "teardown-graph-link");
            DestroyGraph();
            return;
        }
        m_AnchorId = m_Anchor->GetID();
        m_SiblingAnchorId = m_SiblingAnchor->GetID();

        BML_BehaviorGuid prototype{
            static_cast<std::uint32_t>(BML_LIFECYCLE_FIXTURE_GUID.d1),
            static_cast<std::uint32_t>(BML_LIFECYCLE_FIXTURE_GUID.d2)};
        fixture.ResetTrace();
        const int installed = m_Test->InstallSplice(
            m_Session, m_Graph, m_Anchor, prototype,
            "player-teardown-reentry", &m_Patch);
        const int siblingInstalled = m_Test->InstallSplice(
            m_Session, m_Graph, m_SiblingAnchor, prototype,
            "player-teardown-sibling", &m_SiblingPatch);
        std::uint32_t state = 0;
        std::uint32_t siblingState = 0;
        const bool applied = installed == BML_OK && m_Patch != 0 &&
            siblingInstalled == BML_OK && m_SiblingPatch != 0 &&
            m_Test->ReadPatch(m_Session, m_Patch, &state) == BML_OK &&
            state == BML_BEHAVIOR_TEST_PATCH_ACTIVE &&
            m_Test->ReadPatch(m_Session, m_SiblingPatch, &siblingState) ==
                BML_OK &&
            siblingState == BML_BEHAVIOR_TEST_PATCH_ACTIVE &&
            m_Graph->GetSubBehaviorCount() == 2 &&
            m_Graph->GetSubBehaviorLinkCount() == 4;
        if (!applied) {
            GetLogger()->Error(
                "Behavior patch teardown install failed: code=%d sibling_code=%d state=%u sibling_state=%u nodes=%d links=%d",
                installed, siblingInstalled, static_cast<unsigned>(state),
                static_cast<unsigned>(siblingState),
                m_Graph->GetSubBehaviorCount(),
                m_Graph->GetSubBehaviorLinkCount());
            Finish(false, "teardown-patch-apply");
            return;
        }

        g_Teardown = {};
        g_Teardown.Behavior = m_Behavior;
        g_Teardown.Test = m_Test;
        g_Teardown.Session = m_Session;
        g_Teardown.Patch = m_Patch;
        g_Teardown.Sibling = m_SiblingPatch;
        g_Teardown.Graph = m_Graph;
        fixture.SetCloseHook(CloseFromTeardown, nullptr);
        fixture.SetMode(BMLLifecycleFixtureMode::CloseOnTeardown);

        m_TeardownOuterClose = m_Behavior->ClosePatch(
            m_Session, reinterpret_cast<BML_BehaviorPatch>(m_Patch));

        // The sibling's own teardown must not re-enter anything.
        fixture.SetCloseHook(nullptr, nullptr);
        fixture.SetMode(BMLLifecycleFixtureMode::Normal);

        m_TeardownOuterStale = StalePatch();
        m_TeardownSiblingQueued =
            m_Test->ReadPatch(m_Session, m_SiblingPatch, &siblingState) ==
                BML_OK &&
            siblingState == BML_BEHAVIOR_TEST_PATCH_CLOSING;
        m_TeardownWaitUntil = m_Frame + 20;
        m_State = State::TeardownReentry;
    }

    bool SiblingStale() const {
        std::uint32_t state = 0;
        return m_Test && m_Session && m_SiblingPatch &&
            m_Test->ReadPatch(m_Session, m_SiblingPatch, &state) != BML_OK;
    }

    void ObserveTeardownReentry() {
        const bool siblingStale = SiblingStale();
        if (!siblingStale && m_Frame <= m_TeardownWaitUntil)
            return;
        const LifecycleFixtureExports fixture = ResolveLifecycleFixture();
        BMLLifecycleFixtureTrace trace;
        const bool traced = fixture && fixture.ReadTrace(&trace) != 0;
        const int nodes = m_Graph ? m_Graph->GetSubBehaviorCount() : -1;
        const int links = m_Graph ? m_Graph->GetSubBehaviorLinkCount() : -1;
        const bool restored = m_Graph && m_Anchor && m_SiblingAnchor &&
            m_Anchor->GetID() == m_AnchorId &&
            m_SiblingAnchor->GetID() == m_SiblingAnchorId &&
            m_Anchor->GetOutBehaviorIO() == m_Graph->GetOutput(0) &&
            m_SiblingAnchor->GetOutBehaviorIO() == m_Graph->GetOutput(1) &&
            nodes == 0 && links == 2;
        m_TeardownReentryPassed =
            m_TeardownOuterClose == BML_OK &&
            m_TeardownOuterStale && m_TeardownSiblingQueued &&
            siblingStale && restored && traced &&
            g_Teardown.Calls == 2 &&
            g_Teardown.SelfClose == BML_ERROR_BUSY &&
            g_Teardown.SiblingClose == BML_ERROR_BUSY &&
            g_Teardown.Read == BML_OK &&
            g_Teardown.State == BML_BEHAVIOR_TEST_PATCH_CLOSING &&
            g_Teardown.SiblingRead == BML_OK &&
            g_Teardown.SiblingState == BML_BEHAVIOR_TEST_PATCH_CLOSING &&
            // The inverse restores the anchor and removes the Patch's own
            // Link before it tears the node down, so the callback sees the
            // node still present and one Link already gone.
            g_Teardown.Nodes == 2 && g_Teardown.Links == 3 &&
            trace.CloseHookCalls == 2 && trace.CloseHookAccepted == 1 &&
            trace.CreateCount == 2 && trace.DetachCount == 2 &&
            trace.DeleteCount == 2;
        GetLogger()->Info(
            "Behavior patch teardown reentry: status=%s outer=%d stale=%s queued=%s self=%d sibling=%d state=%u sibling_state=%u calls=%u nodes=%d links=%d detach=%u delete=%u restored=%s",
            m_TeardownReentryPassed ? "pass" : "fail",
            m_TeardownOuterClose,
            m_TeardownOuterStale ? "true" : "false",
            m_TeardownSiblingQueued ? "true" : "false",
            g_Teardown.SelfClose, g_Teardown.SiblingClose,
            static_cast<unsigned>(g_Teardown.State),
            static_cast<unsigned>(g_Teardown.SiblingState),
            static_cast<unsigned>(g_Teardown.Calls),
            g_Teardown.Nodes, g_Teardown.Links,
            static_cast<unsigned>(traced ? trace.DetachCount : 0),
            static_cast<unsigned>(traced ? trace.DeleteCount : 0),
            restored ? "true" : "false");
        m_Patch = 0;
        m_SiblingPatch = 0;
        g_Teardown = {};
        if (!m_TeardownReentryPassed) {
            Finish(false, "teardown-reentry");
            return;
        }
        DestroyGraph();
        m_State = State::CreateDeletion;
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
        m_State = State::CreateCallbackClose;
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
                   m_GraphChangedPassed && m_CallbackClosePassed &&
                   m_TeardownReentryPassed &&
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
        if (m_Graph && m_SiblingAnchor) {
            m_Graph->RemoveSubBehaviorLink(m_SiblingAnchor);
            context->DestroyObject(m_SiblingAnchor);
        }
        m_SiblingAnchor = nullptr;
        m_SiblingAnchorId = 0;
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
        if (m_Graph && m_CallbackNode)
            m_Graph->RemoveSubBehavior(m_CallbackNode);
        if (m_CallbackNode)
            context->DestroyObject(m_CallbackNode);
        m_CallbackNode = nullptr;
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
            "Behavior patch: status=%s reason=%s module=%s visual=%s durable=%s relations=%s apply=%s execute=%s close=%s restore=%s reset=%s deletion=%s retirement=%s hooks=%s graph_changed=%s callback_close=%s teardown_reentry=%s",
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
            m_DurableHooksPassed ? "true" : "false",
            m_GraphChangedPassed ? "true" : "false",
            m_CallbackClosePassed ? "true" : "false",
            m_TeardownReentryPassed ? "true" : "false");
        if (passed)
            BML::PlayerTest::ProbeReport::Pass(reason);
        else
            BML::PlayerTest::ProbeReport::Fail(reason);
    }

    const BML_BehaviorInterface *m_Behavior = nullptr;
    const BML_BehaviorTestInterface *m_Test = nullptr;
    BML_BehaviorSession m_Session = nullptr;
    std::uintptr_t m_Patch = 0;
    std::uintptr_t m_SiblingPatch = 0;
    std::uintptr_t m_Plan = 0;
    CK3dObject *m_Owner = nullptr;
    CKBehavior *m_Graph = nullptr;
    CKBehavior *m_Baseline = nullptr;
    CKBehavior *m_DurableSource = nullptr;
    CKBehavior *m_DurableSink = nullptr;
    CKBehavior *m_CallbackNode = nullptr;
    CK2dEntity *m_Display = nullptr;
    CK2dEntity *m_PatchDisplay = nullptr;
    CKBehaviorLink *m_Entry = nullptr;
    CKBehaviorLink *m_Anchor = nullptr;
    CK_ID m_AnchorId = 0;
    CKBehaviorLink *m_SiblingAnchor = nullptr;
    CK_ID m_SiblingAnchorId = 0;
    std::uint64_t m_DurableWorld = 0;
    int m_DurableWaitUntil = 0;
    int m_CallbackWaitUntil = 0;
    int m_TeardownWaitUntil = 0;
    int m_TeardownOuterClose = BML_ERROR_FAIL;
    bool m_TeardownOuterStale = false;
    bool m_TeardownSiblingQueued = false;
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
    bool m_GraphChangedPassed = false;
    bool m_CallbackClosePassed = false;
    bool m_TeardownReentryPassed = false;
    bool m_DeletionPassed = false;
    bool m_RetirementPassed = false;
    bool m_Done = false;
};

} // namespace

BML_PLAYER_PROBE_READ_EXPORT()

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new BehaviorPatchTest(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}

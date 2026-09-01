#include "Api/BehaviorTestApi.h"
#include "BehaviorLifecycleFixtureApi.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "BML/IMod.h"
#include "CKAll.h"

#include <cstdint>
#include <cstring>

namespace {

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
        case State::CreateExplicit:
            CreateGraph("player-explicit-close", State::ObserveExplicit);
            break;
        case State::ObserveExplicit: ObserveExecution(); break;
        case State::CloseExplicit: CloseExplicit(); break;
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
        CreateExplicit,
        ObserveExplicit,
        CloseExplicit,
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

    void CreateGraph(const char *patchName, State next) {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        CKLevel *level = context ? context->GetCurrentLevel() : nullptr;
        CKScene *scene = context ? context->GetCurrentScene() : nullptr;
        if (!context || !level || !scene)
            return;

        m_Owner = CK3dObject::Cast(context->CreateObject(
            CKCID_3DOBJECT, const_cast<CKSTRING>("__BML_Patch_Owner"),
            static_cast<CK_OBJECTCREATION_OPTIONS>(
                CK_OBJECTCREATION_DYNAMIC | CK_OBJECTCREATION_ACTIVATE)));
        m_Graph = CKBehavior::Cast(context->CreateObject(
            CKCID_BEHAVIOR, const_cast<CKSTRING>("__BML_Patch_Graph"),
            CK_OBJECTCREATION_DYNAMIC));
        if (!m_Owner || !m_Graph || level->AddObject(m_Owner) != CK_OK) {
            Finish(false, "graph-create");
            DestroyGraph();
            return;
        }
        if (scene != level->GetLevelScene())
            (void) scene->AddObject(m_Owner);
        scene->Activate(m_Owner, TRUE);

        m_Graph->UseGraph();
        m_Graph->SetType(CKBEHAVIORTYPE_SCRIPT);
        if (m_Graph->SetOwner(m_Owner, FALSE) != CK_OK ||
            !m_Graph->CreateInput("Start") ||
            !m_Graph->CreateOutput("Done") ||
            m_Owner->AddScript(m_Graph) != CK_OK) {
            Finish(false, "graph-layout");
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

        if (next == State::ObserveExplicit) {
            scene->Activate(m_Graph, TRUE);
            m_Graph->ActivateInput(0, TRUE);
            m_ExecuteStarted = m_Frame;
        }
        m_State = next;
    }

    void ObserveExecution() {
        if (m_Graph && m_Graph->GetOutput(0) &&
            m_Graph->GetOutput(0)->IsActive()) {
            m_ExecutePassed = true;
            m_State = State::CloseExplicit;
            return;
        }
        if (m_Frame - m_ExecuteStarted > 12)
            Finish(false, "patch-execute");
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

    void CloseExplicit() {
        CKScene *scene = m_BML && m_BML->GetCKContext()
            ? m_BML->GetCKContext()->GetCurrentScene() : nullptr;
        if (scene && m_Graph)
            scene->DeActivate(m_Graph);
        const int closed = m_Test->ClosePatch(m_Session, m_Patch);
        m_Patch = 0;
        m_ClosePassed = closed == BML_OK;
        m_RestorePassed = m_ClosePassed && Restored();
        if (m_RestorePassed)
            DestroyGraph();
        if (!m_RestorePassed) {
            Finish(false, "patch-close");
            return;
        }
        m_State = State::CreateReset;
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
        Finish(m_ModulePassed && m_ApplyPassed && m_ExecutePassed &&
                   m_ClosePassed && m_RestorePassed && m_ResetPassed &&
                   m_DeletionPassed && m_RetirementPassed,
               m_RetirementPassed ? "complete" : "patch-retirement");
    }

    void DestroyGraph() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        if (!context)
            return;
        if (m_Graph && m_Anchor) {
            m_Graph->RemoveSubBehaviorLink(m_Anchor);
            context->DestroyObject(m_Anchor);
        }
        m_Anchor = nullptr;
        m_AnchorId = 0;
        if (m_Owner && m_Graph)
            (void) m_Owner->RemoveScript(m_Graph->GetID());
        if (m_Graph)
            context->DestroyObject(m_Graph);
        m_Graph = nullptr;
        if (m_Owner)
            context->DestroyObject(m_Owner);
        m_Owner = nullptr;
    }

    void Finish(bool passed, const char *reason) {
        if (m_Done)
            return;
        m_Done = true;
        GetLogger()->Info(
            "Behavior patch: status=%s reason=%s module=%s apply=%s execute=%s close=%s restore=%s reset=%s deletion=%s retirement=%s",
            passed ? "pass" : "fail", reason,
            m_ModulePassed ? "true" : "false",
            m_ApplyPassed ? "true" : "false",
            m_ExecutePassed ? "true" : "false",
            m_ClosePassed ? "true" : "false",
            m_RestorePassed ? "true" : "false",
            m_ResetPassed ? "true" : "false",
            m_DeletionPassed ? "true" : "false",
            m_RetirementPassed ? "true" : "false");
    }

    const BML_BehaviorInterface *m_Behavior = nullptr;
    const BML_BehaviorTestInterface *m_Test = nullptr;
    BML_BehaviorSession m_Session = nullptr;
    std::uintptr_t m_Patch = 0;
    CK3dObject *m_Owner = nullptr;
    CKBehavior *m_Graph = nullptr;
    CKBehaviorLink *m_Anchor = nullptr;
    CK_ID m_AnchorId = 0;
    State m_State = State::CreateExplicit;
    int m_Frame = 0;
    int m_ExecuteStarted = 0;
    bool m_LevelStarted = false;
    bool m_ModulePassed = false;
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

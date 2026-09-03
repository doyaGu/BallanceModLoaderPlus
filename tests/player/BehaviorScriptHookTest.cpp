#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>

#include "PlayerProbe.h"

#include <cstdlib>
#include <cstring>

namespace {

int RunScriptHookSource(const CKBehaviorContext &context) {
    CKBehavior *behavior = context.Behavior;
    if (!behavior)
        return CKBR_BEHAVIORERROR;
    for (int index = 0; index < behavior->GetInputCount(); ++index)
        behavior->ActivateInput(index, FALSE);
    for (int index = 0; index < behavior->GetOutputCount(); ++index)
        behavior->ActivateOutput(index);
    return CKBR_OK;
}

// Offers a real script graph for a script Mod to hook, then checks that the
// inserted block is retired again once the script Mod goes away. This needs no
// gameplay state, so the probe starts itself instead of waiting for the driver.
class BehaviorScriptHookTest final : public IMod {
public:
    explicit BehaviorScriptHookTest(IBML *bml) : IMod(bml) {
        AddDependency("BML");
    }

    const char *GetID() override { return "BehaviorScriptHookTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "Behavior Script Hook Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override {
        return "Checks script hook insertion and retirement on a real graph";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        BML::PlayerTest::ProbeReport::Reset();
        const char *disableAngelScript =
            std::getenv("BML_PLAYER_DISABLE_ANGELSCRIPT");
        if (disableAngelScript && std::strcmp(disableAngelScript, "1") == 0) {
            GetLogger()->Info(
                "ScriptHook retirement: skipped=true reason=angelscript-disabled");
            BML::PlayerTest::ProbeReport::Skip("angelscript-disabled");
            return;
        }
        if (!CreateScriptHookGraph()) {
            DestroyScriptHookGraph();
            Report(false, "script-hook-graph-create-failed");
        }
    }

    void OnProcess() override {
        if (BML::PlayerTest::ProbeReport::Reported())
            return;
        AdvanceScriptHook();
    }

    void OnUnload() override { DestroyScriptHookGraph(); }

private:
    static constexpr int kScriptHookTimeoutFrames = 1800;

    void Report(bool passed, const char *detail) {
        GetLogger()->Info(
            "Behavior script hook: status=%s reason=%s installed=%s "
            "frames=%d",
            passed ? "pass" : "fail", detail,
            m_ScriptHookInstalled ? "true" : "false", m_ScriptHookFrames);
        if (passed)
            BML::PlayerTest::ProbeReport::Pass(detail);
        else
            BML::PlayerTest::ProbeReport::Fail(detail);
    }

    bool CreateScriptHookGraph() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        if (!context)
            return false;

        m_ScriptHookGraph = static_cast<CKBehavior *>(context->CreateObject(
            CKCID_BEHAVIOR,
            const_cast<char *>("__BML_ScriptHook_Fixture"),
            CK_OBJECTCREATION_DYNAMIC));
        if (!m_ScriptHookGraph)
            return false;
        m_ScriptHookGraph->UseGraph();
        m_ScriptHookGraph->SetType(CKBEHAVIORTYPE_SCRIPT);
        CKBehaviorIO *input = m_ScriptHookGraph->CreateInput("In");
        CKBehaviorIO *output = m_ScriptHookGraph->CreateOutput("Out");
        if (!input || !output)
            return false;

        m_ScriptHookSourceBlock = CKBehavior::Cast(context->CreateObject(
            CKCID_BEHAVIOR,
            const_cast<char *>("__BML_ScriptHook_Source"),
            CK_OBJECTCREATION_DYNAMIC));
        if (!m_ScriptHookSourceBlock)
            return false;
        m_ScriptHookSourceBlock->UseFunction();
        m_ScriptHookSourceBlock->SetType(CKBEHAVIORTYPE_BASE);
        m_ScriptHookSourceBlock->SetFunction(RunScriptHookSource);
        m_ScriptHookSourceBlock->CreateInput("In");
        m_ScriptHookSourceBlock->CreateOutput("Out");
        if (m_ScriptHookGraph->AddSubBehavior(m_ScriptHookSourceBlock) != CK_OK)
            return false;

        auto createLink = [&](CKBehaviorIO *from, CKBehaviorIO *to) {
            auto *link = static_cast<CKBehaviorLink *>(context->CreateObject(
                CKCID_BEHAVIORLINK, nullptr, CK_OBJECTCREATION_DYNAMIC));
            if (!link || link->SetInBehaviorIO(from) != CK_OK ||
                link->SetOutBehaviorIO(to) != CK_OK ||
                m_ScriptHookGraph->AddSubBehaviorLink(link) != CK_OK) {
                if (link)
                    context->DestroyObject(link);
                return false;
            }
            return true;
        };
        if (!createLink(input, m_ScriptHookSourceBlock->GetInput(0)) ||
            !createLink(m_ScriptHookSourceBlock->GetOutput(0), output)) {
            return false;
        }
        bool outgoing = false;
        for (int index = 0;
             index < m_ScriptHookGraph->GetSubBehaviorLinkCount(); ++index) {
            CKBehaviorLink *link = m_ScriptHookGraph->GetSubBehaviorLink(index);
            if (link && link->GetInBehaviorIO() ==
                            m_ScriptHookSourceBlock->GetOutput(0)) {
                outgoing = true;
            }
        }
        GetLogger()->Info(
            "ScriptHook fixture: lookup=%s name=%s children=%d links=%d "
            "source_outputs=%d outgoing=%s",
            m_BML->GetScriptByName("__BML_ScriptHook_Fixture") ==
                    m_ScriptHookGraph
                ? "true"
                : "false",
            m_ScriptHookGraph->GetName(),
            m_ScriptHookGraph->GetSubBehaviorCount(),
            m_ScriptHookGraph->GetSubBehaviorLinkCount(),
            m_ScriptHookSourceBlock->GetOutputCount(),
            outgoing ? "true" : "false");
        return true;
    }

    void AdvanceScriptHook() {
        if (!m_ScriptHookGraph) {
            Report(false, "script-hook-graph-missing");
            return;
        }
        if (++m_ScriptHookFrames > kScriptHookTimeoutFrames) {
            Report(false, "script-hook-timeout");
            return;
        }
        const int children = m_ScriptHookGraph->GetSubBehaviorCount();
        if (!m_ScriptHookInstalled) {
            if (children < 2)
                return;
            m_ScriptHookInstalled = true;
            CKBehavior *inserted = nullptr;
            for (int index = 0; index < children; ++index) {
                CKBehavior *candidate = m_ScriptHookGraph->GetSubBehavior(index);
                if (candidate && candidate->GetName() &&
                    std::strcmp(candidate->GetName(),
                                "__BML_ScriptHook_Inserted") == 0) {
                    inserted = candidate;
                    break;
                }
            }
            if (!inserted || !inserted->GetInput(0)) {
                Report(false, "script-hook-inserted-block-missing");
                return;
            }
            // Two executions: the first proves the inserted block runs inside
            // the graph, the second proves it survives its own activation.
            const float delta =
                m_BML->GetCKContext()->m_BehaviorContext.DeltaTime;
            inserted->ActivateInput(0, TRUE);
            inserted->Activate(TRUE, FALSE);
            (void) inserted->Execute(delta);
            inserted->ActivateInput(0, TRUE);
            inserted->Activate(TRUE, FALSE);
            (void) inserted->Execute(delta);
            return;
        }
        if (children != 1)
            return;

        Report(true, "completed");
        DestroyScriptHookGraph();
    }

    void DestroyScriptHookGraph() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        if (context && m_ScriptHookGraph) {
            if (CKScene *scene = context->GetCurrentScene())
                scene->DeActivate(m_ScriptHookGraph);
            while (m_ScriptHookGraph->GetSubBehaviorLinkCount() > 0) {
                CKBehaviorLink *link = m_ScriptHookGraph->RemoveSubBehaviorLink(
                    m_ScriptHookGraph->GetSubBehaviorLinkCount() - 1);
                if (link)
                    context->DestroyObject(link);
            }
            if (m_ScriptHookSourceBlock) {
                m_ScriptHookGraph->RemoveSubBehavior(m_ScriptHookSourceBlock);
                context->DestroyObject(m_ScriptHookSourceBlock);
            }
            context->DestroyObject(m_ScriptHookGraph);
        }
        m_ScriptHookSourceBlock = nullptr;
        m_ScriptHookGraph = nullptr;
    }

    CKBehavior *m_ScriptHookGraph = nullptr;
    CKBehavior *m_ScriptHookSourceBlock = nullptr;
    int m_ScriptHookFrames = 0;
    bool m_ScriptHookInstalled = false;
};

} // namespace

BML_PLAYER_PROBE_READ_EXPORT()

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new BehaviorScriptHookTest(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}

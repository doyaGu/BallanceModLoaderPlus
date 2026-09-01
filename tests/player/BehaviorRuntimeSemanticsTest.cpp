#include "BehaviorRuntimeSemantics.h"
#include "BehaviorRuntimeSemanticsApi.h"

#include "BML/IMod.h"
#include "CKAll.h"

#include <algorithm>
#include <cstring>
#include <memory>

namespace {

BMLBehaviorRuntimeSemanticsResult g_Result;

class BehaviorRuntimeSemanticsTest final : public IMod {
public:
    explicit BehaviorRuntimeSemanticsTest(IBML *bml) : IMod(bml) {
        AddDependency("BML");
    }

    const char *GetID() override { return "BehaviorRuntimeSemanticsTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "Behavior Runtime Semantics Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override {
        return "Exercises private Behavior Runtime semantics against CK2";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override { g_Result = {}; }

    void OnStartLevel() override { m_LevelStarted = true; }

    void OnProcess() override {
        if (!m_LevelStarted || g_Result.State !=
                BML_BEHAVIOR_RUNTIME_SEMANTICS_PENDING)
            return;
        if (!m_Semantics && !CreateOwner())
            return;

        m_Semantics->Advance(++m_Frame);
        if (!m_Semantics->Done())
            return;

        const BehaviorRuntimeSemanticsResult result = m_Semantics->Result();
        g_Result.State = result.Passed
            ? BML_BEHAVIOR_RUNTIME_SEMANTICS_PASSED
            : BML_BEHAVIOR_RUNTIME_SEMANTICS_FAILED;
        g_Result.LifecyclePassed = result.LifecyclePassed ? 1u : 0u;
        g_Result.AdditiveEditPassed = result.AdditiveEditPassed ? 1u : 0u;
        const std::size_t length = (std::min)(
            result.Detail.size(), sizeof(g_Result.Detail) - 1);
        std::memcpy(g_Result.Detail, result.Detail.data(), length);
        g_Result.Detail[length] = '\0';

        m_Semantics.reset();
        DestroyOwner();
        GetLogger()->Info(
            "Behavior runtime semantics: status=%s lifecycle=%s additive_edit=%s detail=%s",
            result.Passed ? "pass" : "fail",
            result.LifecyclePassed ? "true" : "false",
            result.AdditiveEditPassed ? "true" : "false",
            g_Result.Detail);
    }

    void OnUnload() override {
        m_Semantics.reset();
        DestroyOwner();
    }

private:
    bool CreateOwner() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        CKLevel *level = context ? context->GetCurrentLevel() : nullptr;
        CKScene *scene = context ? context->GetCurrentScene() : nullptr;
        if (!context || !level || !scene)
            return false;

        m_Owner = CK3dObject::Cast(context->CreateObject(
            CKCID_3DOBJECT,
            const_cast<CKSTRING>("__BML_Behavior_Runtime_Semantics"),
            static_cast<CK_OBJECTCREATION_OPTIONS>(
                CK_OBJECTCREATION_DYNAMIC | CK_OBJECTCREATION_ACTIVATE)));
        if (!m_Owner || level->AddObject(m_Owner) != CK_OK) {
            DestroyOwner();
            return false;
        }
        if (scene != level->GetLevelScene())
            (void) scene->AddObject(m_Owner);
        scene->Activate(m_Owner, TRUE);

        const VxVector origin(0.0f, 0.0f, 5100.0f);
        m_Owner->SetPosition(&origin);
        m_Semantics = std::make_unique<BehaviorRuntimeSemantics>(
            context, m_Owner);
        return true;
    }

    void DestroyOwner() {
        if (!m_Owner)
            return;
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        if (context)
            context->DestroyObject(m_Owner);
        m_Owner = nullptr;
    }

    std::unique_ptr<BehaviorRuntimeSemantics> m_Semantics;
    CK3dObject *m_Owner = nullptr;
    int m_Frame = 0;
    bool m_LevelStarted = false;
};

} // namespace

extern "C" __declspec(dllexport) int __cdecl
BMLBehaviorRuntimeSemanticsRead(BMLBehaviorRuntimeSemanticsResult *result) {
    if (!result || result->Size != sizeof(BMLBehaviorRuntimeSemanticsResult))
        return 0;
    *result = g_Result;
    return 1;
}

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new BehaviorRuntimeSemanticsTest(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}

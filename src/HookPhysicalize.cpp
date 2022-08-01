#include "Defines.h"
#include "ModLoader.h"
#include "ModManager.h"

namespace {
    CKBEHAVIORFCT g_Physicalize = nullptr;
}

int Physicalize(const CKBehaviorContext &behcontext) {
    CKBehavior *beh = behcontext.Behavior;
    bool physicalize = beh->IsInputActive(0);
    auto *target = (CK3dEntity *) beh->GetTarget();

    if (physicalize) {
        CKBOOL fixed;
        float friction, elasticity, mass;
        beh->GetInputParameterValue(0, &fixed);
        beh->GetInputParameterValue(1, &friction);
        beh->GetInputParameterValue(2, &elasticity);
        beh->GetInputParameterValue(3, &mass);
        CKSTRING collGroup = (CKSTRING) beh->GetInputParameterReadDataPtr(4);
        CKBOOL startFrozen, enableColl, calcMassCenter;
        float linearDamp, rotDamp;
        beh->GetInputParameterValue(5, &startFrozen);
        beh->GetInputParameterValue(6, &enableColl);
        beh->GetInputParameterValue(7, &calcMassCenter);
        beh->GetInputParameterValue(8, &linearDamp);
        beh->GetInputParameterValue(9, &rotDamp);
        CKSTRING collSurface = (CKSTRING) beh->GetInputParameterReadDataPtr(10);
        VxVector massCenter;
        beh->GetLocalParameterValue(3, &massCenter);

        int convexCnt, ballCnt, concaveCnt;
        beh->GetLocalParameterValue(0, &convexCnt);
        beh->GetLocalParameterValue(1, &ballCnt);
        beh->GetLocalParameterValue(2, &concaveCnt);

        CKMesh **convexMesh = (convexCnt > 0) ? new CKMesh *[convexCnt] : nullptr;
        VxVector *ballCenter = (ballCnt > 0) ? new VxVector[ballCnt] : nullptr;;
        float *ballRadius = (ballCnt > 0) ? new float[ballCnt] : nullptr;
        CKMesh **concaveMesh = (concaveCnt > 0) ? new CKMesh *[concaveCnt] : nullptr;

        int paramPos = 11;
        if (convexCnt > 0) {
            for (int i = 0; i < convexCnt; i++)
                convexMesh[i] = (CKMesh *) beh->GetInputParameterObject(paramPos + i);
            paramPos += convexCnt;
        }

        if (ballCnt > 0) {
            for (int i = 0; i < ballCnt; i++) {
                beh->GetInputParameterValue(paramPos + 2 * i, ballCenter + i);
                beh->GetInputParameterValue(paramPos + 2 * i + 1, ballRadius + i);
            }
            paramPos += ballCnt * 2;
        }

        if (concaveCnt > 0) {
            for (int i = 0; i < concaveCnt; i++)
                concaveMesh[i] = (CKMesh *) beh->GetInputParameterObject(paramPos + i);
            paramPos += concaveCnt;
        }

        ModLoader::GetInstance().GetModManager()->BroadcastCallback(&IMod::OnPhysicalize, target,
                                                                              fixed, friction, elasticity, mass,
                                                                              collGroup, startFrozen, enableColl,
                                                                              calcMassCenter, linearDamp, rotDamp,
                                                                              collSurface, massCenter, convexCnt,
                                                                              convexMesh, ballCnt, ballCenter,
                                                                              ballRadius, concaveCnt, concaveMesh);
        if (convexMesh) delete[] convexMesh;
        if (ballCenter) delete[] ballCenter;
        if (ballRadius) delete[] ballRadius;
        if (concaveMesh) delete[] concaveMesh;
    } else {
        ModLoader::GetInstance().GetModManager()->BroadcastCallback(&IMod::OnUnphysicalize, target);
    }

    return g_Physicalize(behcontext);
}

bool HookPhysicalize() {
    CKBehaviorPrototype *physicalizeProto = CKGetPrototypeFromGuid(TT_PHYSICALIZE);
    if (!physicalizeProto) return false;
    if (!g_Physicalize) g_Physicalize = physicalizeProto->GetFunction();
    physicalizeProto->SetFunction(&Physicalize);
    return true;
}

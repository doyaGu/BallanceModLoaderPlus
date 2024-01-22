#include "DualBallControl.h"

IMod *BMLEntry(IBML *bml) {
    return new DualBallControl(bml);
}

void BMLExit(IMod *mod) {
    delete mod;
}

void DualBallControl::OnLoad() {
    GetConfig()->SetCategoryComment("Misc", "Miscellaneous");

    m_SwitchKey = GetConfig()->GetProperty("Misc", "SwitchKey");
    m_SwitchKey->SetComment("Switch between two balls");
    m_SwitchKey->SetDefaultKey(CKKEY_X);
}

void DualBallControl::OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                                   CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                                   XObjectArray *objArray, CKObject *masterObj) {
    if (isMap) {
        m_DualActive = false;
        CK3dObject *ps = m_BML->Get3dObjectByName("PS_FourFlames_01_Dual");
        CKGroup *pcs = m_BML->GetGroupByName("PC_Checkpoints");
        CKGroup *prs = m_BML->GetGroupByName("PR_Resetpoints");
        if (ps != nullptr && pcs != nullptr && prs != nullptr) {
            m_DualFlames.push_back(ps);
            for (int i = 1; i <= pcs->GetObjectCount(); i++) {
                std::string pcname = "PC_TwoFlames_0" + std::to_string(i) + "_Dual";
                CK3dObject *pc = m_BML->Get3dObjectByName(pcname.c_str());
                if (pc != nullptr)
                    m_DualFlames.push_back(pc);
            }

            for (int i = 1; i <= prs->GetObjectCount(); i++) {
                std::string prname = "PR_Resetpoint_0" + std::to_string(i) + "_Dual";
                CK3dObject *pr = m_BML->Get3dObjectByName(prname.c_str());
                if (pr != nullptr)
                    m_DualResets.push_back(pr);
            }

            if (m_DualFlames.size() == pcs->GetObjectCount() + 1 && m_DualResets.size() == prs->GetObjectCount()) {
                m_DualActive = true;
                for (CK3dObject *obj: m_DualFlames)
                    obj->Show(CKHIDE);
                for (CK3dObject *obj: m_DualResets)
                    obj->Show(CKHIDE);
                m_DualBallType = 2;
                m_DepthTestCubes = m_BML->GetGroupByName("DepthTestCubes");
            }
        }
    }

    if (!strcmp(filename, "3D Entities\\Balls.nmo")) {
        CKDataArray *physBall = m_BML->GetArrayByName("Physicalize_GameBall");
        for (int i = 0; i < physBall->GetRowCount(); i++) {
            std::string ballName(physBall->GetElementStringValue(i, 0, nullptr), '\0');
            physBall->GetElementStringValue(i, 0, (CKSTRING) ballName.c_str());
            CK3dObject *ballObj = m_BML->Get3dObjectByName(ballName.c_str());
            m_Balls.push_back(ballObj);

            CKDependencies dep;
            dep.Resize(40);
            dep.Fill(0);
            dep.m_Flags = CK_DEPENDENCIES_CUSTOM;
            dep[CKCID_OBJECT] = CK_DEPENDENCIES_COPY_OBJECT_NAME | CK_DEPENDENCIES_COPY_OBJECT_UNIQUENAME;
            dep[CKCID_SCENEOBJECT] = CK_DEPENDENCIES_COPY_SCENEOBJECT_SCENES;
            ballObj = (CK3dObject *) m_BML->GetCKContext()->CopyObject(ballObj, &dep, "_Dual");
            m_DualBalls.push_back(ballObj);

            PhysicsBall phys;
            phys.surface = ballName + "_Dual_Mesh";
            physBall->GetElementValue(i, 1, &phys.friction);
            physBall->GetElementValue(i, 2, &phys.elasticity);
            physBall->GetElementValue(i, 3, &phys.mass);
            physBall->GetElementValue(i, 5, &phys.linearDamp);
            physBall->GetElementValue(i, 6, &phys.rotDamp);
            m_PhysicsBalls.push_back(phys);

            std::string trafoName = ballName.substr(5);
            std::transform(trafoName.begin(), trafoName.end(), trafoName.begin(), tolower);
            m_TrafoTypes.push_back(trafoName);
        }

        CKAttributeManager *am = m_BML->GetAttributeManager();
        CKAttributeType collId = am->GetAttributeTypeByName("Coll Detection ID");
        m_DualBalls[1]->SetAttribute(collId);
        m_DualBalls[2]->SetAttribute(collId);
        ScriptHelper::SetParamValue(m_DualBalls[1]->GetAttributeParameter(collId), 1);
        ScriptHelper::SetParamValue(m_DualBalls[2]->GetAttributeParameter(collId), 2);

        GetLogger()->Info("Created Dual Balls");
    }

    if (!strcmp(filename, "3D Entities\\Gameplay.nmo")) {
        m_Energy = m_BML->GetArrayByName("Energy");
        m_CurLevel = m_BML->GetArrayByName("CurrentLevel");
        m_IngameParam = m_BML->GetArrayByName("IngameParameter");
    }

    if (!strcmp(filename, "3D Entities\\Camera.nmo")) {
        m_CamMF = m_BML->Get3dEntityByName("Cam_MF");
        m_CamTarget = m_BML->Get3dEntityByName("Cam_Target");
        m_CamPos = m_BML->Get3dEntityByName("Cam_Pos");
        m_InGameCam = m_BML->GetTargetCameraByName("InGameCam");
    }
}

void DualBallControl::OnLoadScript(const char *filename, CKBehavior *script) {
    if (!strcmp(script->GetName(), "Gameplay_Ingame"))
        OnEditScript_Gameplay_Ingame(script);
}

void DualBallControl::OnEditScript_Gameplay_Ingame(CKBehavior *script) {
    CKBehavior *ballMgr = ScriptHelper::FindFirstBB(script, "BallManager");
    m_DynamicPos = ScriptHelper::FindNextBB(script, ballMgr, "TT Set Dynamic Position");
    m_DeactivateBall = ScriptHelper::FindFirstBB(ballMgr, "Deactivate Ball");

    CKBehavior *trafoMgr = ScriptHelper::FindFirstBB(script, "Trafo Manager");
    m_SetNewBall = ScriptHelper::FindFirstBB(trafoMgr, "set new Ball");
    CKBehavior *sop = ScriptHelper::FindFirstBB(m_SetNewBall, "Switch On Parameter");
    m_CurTrafo = sop->GetInputParameter(0)->GetDirectSource();
    CKBehavior *gnig = ScriptHelper::FindFirstBB(trafoMgr, "Get Nearest In Group");
    m_NearestTrafo = gnig->GetOutputParameter(1);
}

void DualBallControl::OnProcess() {
    if (m_BML->IsPlaying() && m_DualActive) {
        if (m_BML->GetInputManager()->IsKeyPressed(m_SwitchKey->GetKey()) && m_SwitchProgress <= 0
            && m_BallNavActive && ScriptHelper::GetParamValue<float>(m_NearestTrafo) > 4.3) {
            CKMessageManager *mm = m_BML->GetMessageManager();
            CKMessageType ballDeact = mm->AddMessageType("BallNav deactivate");

            mm->SendMessageSingle(ballDeact, m_BML->GetGroupByName("All_Gameplay"));
            mm->SendMessageSingle(ballDeact, m_BML->GetGroupByName("All_Sound"));

            m_BML->AddTimer(2ul, [this]() {
                auto *curBall = (CK3dEntity *) m_CurLevel->GetElementObject(0, 1);
                ExecuteBB::Unphysicalize(curBall);

                m_DynamicPos->ActivateInput(1);
                m_DynamicPos->Activate();

                m_BML->AddTimer(1ul, [this, curBall]() {
                    ReleaseDualBall();

                    CK3dEntity *dualBall = m_DualBalls[m_DualBallType];
                    VxMatrix dualMatrix = curBall->GetWorldMatrix();
                    VxMatrix matrix = dualBall->GetWorldMatrix();

                    VxVector position, dualPosition, camPosition;
                    curBall->GetPosition(&dualPosition);
                    dualBall->GetPosition(&position);
                    m_InGameCam->GetPosition(&camPosition, m_CamPos);

                    curBall->SetWorldMatrix(matrix);
                    ScriptHelper::SetParamString(m_CurTrafo, m_TrafoTypes[m_DualBallType].c_str());
                    m_SetNewBall->ActivateInput(0);
                    m_SetNewBall->Activate();

                    m_DualBallType = GetCurrentBallType();
                    CK3dEntity *newDualBall = m_DualBalls[GetCurrentBallType()];
                    newDualBall->SetWorldMatrix(dualMatrix);
                    PhysicalizeDualBall();
                    m_SwitchProgress = 1.0f;

                    m_BML->AddTimerLoop(1ul, [this, position, dualPosition, camPosition]() {
                        if (m_SwitchProgress < 0) {
                            m_CamTarget->SetPosition(&position);
                            m_InGameCam->SetPosition(&camPosition, m_CamPos);

                            m_DynamicPos->ActivateInput(0);
                            m_DynamicPos->Activate();
                            return false;
                        } else {
                            float progress = m_SwitchProgress * m_SwitchProgress * m_SwitchProgress;
                            VxVector midPos = (dualPosition - position) * progress + position;
                            m_CamTarget->SetPosition(&midPos);
                            m_InGameCam->SetPosition(&camPosition, m_CamPos);
                        }

                        m_SwitchProgress -= m_BML->GetTimeManager()->GetLastDeltaTime() / 200;
                        return true;
                    });
                });
            });
        }

        if (!m_DeactivateBall->IsActive()) {
            CKCollisionManager *cm = m_BML->GetCollisionManager();
            for (int i = 0; i < m_DepthTestCubes->GetObjectCount(); i++) {
                if (cm->BoxBoxIntersection((CK3dEntity *) m_DepthTestCubes->GetObject(i), false, true,
                                           m_DualBalls[m_DualBallType], false, true)) {
                    m_DeactivateBall->ActivateInput(0);
                    m_DeactivateBall->Activate();
                }
            }
        }
    }
}

int DualBallControl::GetCurrentSector() {
    int sector;
    m_IngameParam->GetElementValue(0, 1, &sector);
    return sector;
}

int DualBallControl::GetCurrentBallType() {
    auto *ball = (CK3dObject *) m_CurLevel->GetElementObject(0, 1);
    for (auto i = 0u; i < m_Balls.size(); i++)
        if (m_Balls[i] == ball)
            return i;
    return -1;
}

void DualBallControl::ReleaseDualBall() {
    CK3dObject *ball = m_DualBalls[m_DualBallType];
    ball->Show(CKHIDE);
    ExecuteBB::Unphysicalize(ball);
}

void DualBallControl::PhysicalizeDualBall() {
    CK3dObject *ball = m_DualBalls[m_DualBallType];
    ball->Show();
    PhysicsBall &phys = m_PhysicsBalls[m_DualBallType];
    ExecuteBB::PhysicalizeBall(ball, false, phys.friction, phys.elasticity, phys.mass, "",
                               false, true, false, phys.linearDamp, phys.rotDamp, phys.surface.c_str());
}

void DualBallControl::ResetDualBallMatrix() {
    CK3dObject *pr = m_DualResets[GetCurrentSector() - 1];
    CK3dObject *ball = m_DualBalls[m_DualBallType];
    ball->SetWorldMatrix(pr->GetWorldMatrix());
}

void DualBallControl::ReleaseLevel() {
    m_DualResets.clear();
    m_DualFlames.clear();
}

void DualBallControl::OnPostLoadLevel() {
    if (m_DualActive) {
        CKDependencies dep;
        dep.Resize(40);
        dep.Fill(0);
        dep.m_Flags = CK_DEPENDENCIES_CUSTOM;
        dep[CKCID_OBJECT] = CK_DEPENDENCIES_COPY_OBJECT_NAME | CK_DEPENDENCIES_COPY_OBJECT_UNIQUENAME;
        dep[CKCID_SCENEOBJECT] = CK_DEPENDENCIES_COPY_SCENEOBJECT_SCENES;
        dep[CKCID_BEOBJECT] = CK_DEPENDENCIES_COPY_BEOBJECT_SCRIPTS;

        CK3dEntity *flame = m_BML->Get3dEntityByName("PS_FourFlames_Flame_A");
        CKGroup *allLevel = m_BML->GetGroupByName("All_Level");
        CKScene *scene = m_BML->GetCKContext()->GetCurrentScene();

        int counter = 0;
        auto createFlame = [this, &counter, flame, &dep, allLevel, scene]() {
            std::string suffix = "_Dual_" + std::to_string(counter++);
            auto *newFlame = (CK3dEntity *) m_BML->GetCKContext()->CopyObject(flame, &dep, (CKSTRING) suffix.c_str());
            scene->Activate(newFlame, false);
            allLevel->AddObject(newFlame);
            for (int i = 0; i < newFlame->GetScriptCount(); i++)
                scene->Activate(newFlame->GetScript(i), true);
            return newFlame;
        };

        VxVector vec(7.3338f, 2.0526f, 6.1448f);
        createFlame()->SetPosition(&vec, m_DualFlames[0]);
        vec.Set(-7.2214f, 2.0526f, 6.1448f);
        createFlame()->SetPosition(&vec, m_DualFlames[0]);
        vec.Set(-7.2214f, 2.0526f, -6.1318f);
        createFlame()->SetPosition(&vec, m_DualFlames[0]);
        vec.Set(7.3338f, 2.0526f, -6.1318f);
        createFlame()->SetPosition(&vec, m_DualFlames[0]);

        VxVector vec1(0.0400f, 2.0526f, -6.9423f);
        VxVector vec2(0.0392f, 2.0526f, 7.0605f);
        for (auto i = 1u; i < m_DualFlames.size(); i++) {
            createFlame()->SetPosition(&vec1, m_DualFlames[i]);
            createFlame()->SetPosition(&vec2, m_DualFlames[i]);
        }
    }
}
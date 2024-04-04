#include "DebugUtilities.h"
#include "DebugCommands.h"

using namespace ScriptHelper;

IMod *BMLEntry(IBML *bml) {
    return new DebugUtilities(bml);
}

void BMLExit(IMod *mod) {
    delete mod;
}

void DebugUtilities::OnLoad() {
    GetConfig()->SetCategoryComment("Debug", "Debug Utilities");

    m_EnableSuicideKey = GetConfig()->GetProperty("Debug", "EnableSuicideKey");
    m_EnableSuicideKey->SetComment("Enable the Suicide Hotkey");
    m_EnableSuicideKey->SetDefaultBoolean(true);

    m_Suicide = GetConfig()->GetProperty("Debug", "Suicide");
    m_Suicide->SetComment("Suicide");
    m_Suicide->SetDefaultKey(CKKEY_R);

    m_BallCheat[0] = GetConfig()->GetProperty("Debug", "BallUp");
    m_BallCheat[0]->SetComment("Apply an upward force to the ball");
    m_BallCheat[0]->SetDefaultKey(CKKEY_F1);

    m_BallCheat[1] = GetConfig()->GetProperty("Debug", "BallDown");
    m_BallCheat[1]->SetComment("Apply a downward force to the ball");
    m_BallCheat[1]->SetDefaultKey(CKKEY_F2);

    m_ChangeBall[0] = GetConfig()->GetProperty("Debug", "TurnPaper");
    m_ChangeBall[0]->SetComment("Turn into paper ball");
    m_ChangeBall[0]->SetDefaultKey(CKKEY_I);

    m_ChangeBall[1] = GetConfig()->GetProperty("Debug", "TurnWood");
    m_ChangeBall[1]->SetComment("Turn into wood ball");
    m_ChangeBall[1]->SetDefaultKey(CKKEY_O);

    m_ChangeBall[2] = GetConfig()->GetProperty("Debug", "TurnStone");
    m_ChangeBall[2]->SetComment("Turn into stone ball");
    m_ChangeBall[2]->SetDefaultKey(CKKEY_P);

    m_ResetBall = GetConfig()->GetProperty("Debug", "ResetBall");
    m_ResetBall->SetComment("Reset ball and all moduls");
    m_ResetBall->SetDefaultKey(CKKEY_BACK);

    m_AddLife = GetConfig()->GetProperty("Debug", "AddLife");
    m_AddLife->SetComment("Add one extra Life");
    m_AddLife->SetDefaultKey(CKKEY_L);

    m_SpeedupBall = GetConfig()->GetProperty("Debug", "BallSpeedUp");
    m_SpeedupBall->SetComment("Change to 3 times ball speed");
    m_SpeedupBall->SetDefaultKey(CKKEY_LCONTROL);

    m_EnableSuicideKey = GetConfig()->GetProperty("Debug", "EnableSuicideKey");
    m_EnableSuicideKey->SetComment("Enable the Suicide Hotkey");
    m_EnableSuicideKey->SetDefaultBoolean(true);
    m_Suicide = GetConfig()->GetProperty("Debug", "Suicide");
    m_Suicide->SetComment("Suicide");
    m_Suicide->SetDefaultKey(CKKEY_R);

    m_BallCheat[0] = GetConfig()->GetProperty("Debug", "BallUp");
    m_BallCheat[0]->SetComment("Apply an upward force to the ball");
    m_BallCheat[0]->SetDefaultKey(CKKEY_F1);

    m_BallCheat[1] = GetConfig()->GetProperty("Debug", "BallDown");
    m_BallCheat[1]->SetComment("Apply a downward force to the ball");
    m_BallCheat[1]->SetDefaultKey(CKKEY_F2);

    m_ChangeBall[0] = GetConfig()->GetProperty("Debug", "TurnPaper");
    m_ChangeBall[0]->SetComment("Turn into paper ball");
    m_ChangeBall[0]->SetDefaultKey(CKKEY_I);

    m_ChangeBall[1] = GetConfig()->GetProperty("Debug", "TurnWood");
    m_ChangeBall[1]->SetComment("Turn into wood ball");
    m_ChangeBall[1]->SetDefaultKey(CKKEY_O);

    m_ChangeBall[2] = GetConfig()->GetProperty("Debug", "TurnStone");
    m_ChangeBall[2]->SetComment("Turn into stone ball");
    m_ChangeBall[2]->SetDefaultKey(CKKEY_P);

    m_ResetBall = GetConfig()->GetProperty("Debug", "ResetBall");
    m_ResetBall->SetComment("Reset ball and all moduls");
    m_ResetBall->SetDefaultKey(CKKEY_BACK);

    m_AddLife = GetConfig()->GetProperty("Debug", "AddLife");
    m_AddLife->SetComment("Add one extra Life");
    m_AddLife->SetDefaultKey(CKKEY_L);

    m_SpeedupBall = GetConfig()->GetProperty("Debug", "BallSpeedUp");
    m_SpeedupBall->SetComment("Change to 3 times ball speed");
    m_SpeedupBall->SetDefaultKey(CKKEY_LCONTROL);

    m_SpeedNotification = GetConfig()->GetProperty("Debug", "SpeedNotification");
    m_SpeedNotification->SetComment("Notify the player when speed of the ball changes.");
    m_SpeedNotification->SetDefaultBoolean(true);

    m_SkipRenderKey = GetConfig()->GetProperty("Debug", "SkipRender");
    m_SkipRenderKey->SetComment("Skip rendering of current frames while holding.");
    m_SkipRenderKey->SetDefaultKey(CKKEY_F);

    GetConfig()->SetCategoryComment("Auxiliaries", "Temporal Auxiliary Moduls");

    m_AddBall[0] = GetConfig()->GetProperty("Auxiliaries", "PaperBall");
    m_AddBall[0]->SetComment("Add a Paper Ball");
    m_AddBall[0]->SetDefaultKey(CKKEY_J);

    m_AddBall[1] = GetConfig()->GetProperty("Auxiliaries", "WoodBall");
    m_AddBall[1]->SetComment("Add a Wood Ball");
    m_AddBall[1]->SetDefaultKey(CKKEY_K);

    m_AddBall[2] = GetConfig()->GetProperty("Auxiliaries", "StoneBall");
    m_AddBall[2]->SetComment("Add a Stone Ball");
    m_AddBall[2]->SetDefaultKey(CKKEY_N);

    m_AddBall[3] = GetConfig()->GetProperty("Auxiliaries", "Box");
    m_AddBall[3]->SetComment("Add a Box");
    m_AddBall[3]->SetDefaultKey(CKKEY_M);

    m_MoveKeys[0] = GetConfig()->GetProperty("Auxiliaries", "MoveFront");
    m_MoveKeys[0]->SetComment("Move Front");
    m_MoveKeys[0]->SetDefaultKey(CKKEY_UP);

    m_MoveKeys[1] = GetConfig()->GetProperty("Auxiliaries", "MoveBack");
    m_MoveKeys[1]->SetComment("Move Back");
    m_MoveKeys[1]->SetDefaultKey(CKKEY_DOWN);

    m_MoveKeys[2] = GetConfig()->GetProperty("Auxiliaries", "MoveLeft");
    m_MoveKeys[2]->SetComment("Move Left");
    m_MoveKeys[2]->SetDefaultKey(CKKEY_LEFT);

    m_MoveKeys[3] = GetConfig()->GetProperty("Auxiliaries", "MoveRight");
    m_MoveKeys[3]->SetComment("Move Right");
    m_MoveKeys[3]->SetDefaultKey(CKKEY_RIGHT);

    m_MoveKeys[4] = GetConfig()->GetProperty("Auxiliaries", "MoveUp");
    m_MoveKeys[4]->SetComment("Move Up");
    m_MoveKeys[4]->SetDefaultKey(CKKEY_RSHIFT);

    m_MoveKeys[5] = GetConfig()->GetProperty("Auxiliaries", "MoveDown");
    m_MoveKeys[5]->SetComment("Move Down");
    m_MoveKeys[5]->SetDefaultKey(CKKEY_RCONTROL);

    m_Balls[0] = (CK3dEntity *) ExecuteBB::ObjectLoad("3D Entities\\PH\\P_Ball_Paper.nmo", true, "P_Ball_Paper_MF").second;
    m_Balls[1] = (CK3dEntity *) ExecuteBB::ObjectLoad("3D Entities\\PH\\P_Ball_Wood.nmo", true, "P_Ball_Wood_MF").second;
    m_Balls[2] = (CK3dEntity *) ExecuteBB::ObjectLoad("3D Entities\\PH\\P_Ball_Stone.nmo", true, "P_Ball_Stone_MF").second;
    m_Balls[3] = (CK3dEntity *) ExecuteBB::ObjectLoad("3D Entities\\PH\\P_Box.nmo", true, "P_Box_MF").second;

    m_BML->RegisterCommand(new CommandScore());
    m_BML->RegisterCommand(new CommandKill(this));
    m_BML->RegisterCommand(new CommandSetSpawn());
    m_BML->RegisterCommand(new CommandSector(this));
    m_BML->RegisterCommand(new CommandWin());
    m_BML->RegisterCommand(new CommandSpeed(this));

    m_CKContext = m_BML->GetCKContext();
    m_RenderContext = m_BML->GetRenderContext();
    m_InputHook = m_BML->GetInputManager();
}

void DebugUtilities::OnModifyConfig(const char *category, const char *key, IProperty *prop) {
    if (m_BML->IsCheatEnabled()) {
        if (prop == m_BallCheat[0])
            SetParamValue(m_BallForce[0], m_BallCheat[0]->GetKey());
        if (prop == m_BallCheat[1])
            SetParamValue(m_BallForce[1], m_BallCheat[1]->GetKey());
    }
}

void DebugUtilities::OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass, CKBOOL addToScene, CKBOOL reuseMeshes,
                            CKBOOL reuseMaterials, CKBOOL dynamic, XObjectArray *objArray, CKObject *masterObj) {
    if (!strcmp(filename, "3D Entities\\Camera.nmo")) {
        m_CamOrientRef = m_BML->Get3dEntityByName("Cam_OrientRef");
        m_CamTarget = m_BML->Get3dEntityByName("Cam_Target");
    }
}

void DebugUtilities::OnLoadScript(const char *filename, CKBehavior *script) {
    if (!strcmp(script->GetName(), "Gameplay_Ingame"))
        OnEditScript_Gameplay_Ingame(script);

    if (!strcmp(script->GetName(), "Gameplay_Events"))
        OnEditScript_Gameplay_Events(script);
}

void DebugUtilities::OnProcess() {
    m_DeltaTime = m_BML->GetTimeManager()->GetLastDeltaTime() / 10;

    OnProcess_SkipRender();

    if (m_BML->IsPlaying()) {
        OnProcess_Suicide();

        if (m_BML->IsCheatEnabled()) {
            OnProcess_ChangeBall();
            OnProcess_ResetBall();
            OnProcess_ChangeSpeed();
            OnProcess_AddLife();
            OnProcess_Summon();
        }
    }
}

void DebugUtilities::OnCheatEnabled(bool enable) {
    if (enable) {
        SetParamValue(m_BallForce[0], m_BallCheat[0]->GetKey());
        SetParamValue(m_BallForce[1], m_BallCheat[1]->GetKey());
    } else {
        SetParamValue(m_BallForce[0], CKKEYBOARD(0));
        SetParamValue(m_BallForce[1], CKKEYBOARD(0));
    }
}

void DebugUtilities::OnPreCommandExecute(ICommand *command, const std::vector<std::string> &args) {
    if (args[0] == "cheat" && m_BML->IsCheatEnabled() && (args.size() == 1 || !ICommand::ParseBoolean(args[1]))) {
        ChangeBallSpeed(1);
    }
}

void DebugUtilities::OnStartLevel() {
    m_InLevel = true;
    m_Paused = false;
}

void DebugUtilities::OnPreResetLevel() {
    m_InLevel = false;
}

void DebugUtilities::OnPostResetLevel() {
    CKDataArray *ph = m_BML->GetArrayByName("PH");
    for (auto iter = m_TempBalls.rbegin(); iter != m_TempBalls.rend(); iter++) {
        ph->RemoveRow(iter->first);
        m_BML->GetCKContext()->DestroyObject(iter->second);
    }
    m_TempBalls.clear();
}

void DebugUtilities::OnPauseLevel() {
    m_Paused = true;
}

void DebugUtilities::OnUnpauseLevel() {
    m_Paused = false;
}

void DebugUtilities::OnPostExitLevel() {
    m_InLevel = false;
}

void DebugUtilities::OnPostNextLevel() {
    m_InLevel = false;
}

void DebugUtilities::OnDead() {
    m_InLevel = false;
}

void DebugUtilities::OnPostEndLevel() {
    m_InLevel = false;
}

void DebugUtilities::OnLevelFinish() {
    m_InLevel = false;
}

void DebugUtilities::ChangeBallSpeed(float times) {
    if (m_BML->IsIngame()) {
        bool notify = true;

        if (!m_PhysicsBall) {
            m_PhysicsBall = m_BML->GetArrayByName("Physicalize_GameBall");
            CKBehavior *ingame = m_BML->GetScriptByName("Gameplay_Ingame");
            m_Force = ScriptHelper::FindFirstBB(ingame, "Ball Navigation")->GetInputParameter(0)->GetRealSource();

            for (int i = 0; i < m_PhysicsBall->GetRowCount(); i++) {
                std::string ballName(m_PhysicsBall->GetElementStringValue(i, 0, nullptr), '\0');
                m_PhysicsBall->GetElementStringValue(i, 0, &ballName[0]);
                ballName.pop_back();
                float force;
                m_PhysicsBall->GetElementValue(i, 7, &force);
                m_Forces[ballName] = force;
            }
        }

        if (m_PhysicsBall) {
            CKObject *curBall = m_CurLevel->GetElementObject(0, 1);
            if (curBall) {
                auto iter = m_Forces.find(curBall->GetName());
                if (iter != m_Forces.end()) {
                    float force = iter->second;
                    force *= times;
                    if (force == ScriptHelper::GetParamValue<float>(m_Force))
                        notify = false;
                    ScriptHelper::SetParamValue(m_Force, force);
                }
            }

            for (int i = 0; i < m_PhysicsBall->GetRowCount(); i++) {
                std::string ballName(m_PhysicsBall->GetElementStringValue(i, 0, nullptr), '\0');
                m_PhysicsBall->GetElementStringValue(i, 0, &ballName[0]);
                ballName.pop_back();
                auto iter = m_Forces.find(ballName);
                if (iter != m_Forces.end()) {
                    float force = iter->second;
                    force *= times;
                    m_PhysicsBall->SetElementValue(i, 7, &force);
                }
            }

            if (notify && m_SpeedNotification->GetBoolean())
                m_BML->SendIngameMessage(("Current Ball Speed Changed to " + std::to_string(times) + " times").c_str());
        }
    }
}

void DebugUtilities::ResetBall() {
    CKMessageManager *mm = m_BML->GetMessageManager();
    CKMessageType ballDeactivate = mm->AddMessageType("BallNav deactivate");

    mm->SendMessageSingle(ballDeactivate, m_BML->GetGroupByName("All_Gameplay"));
    mm->SendMessageSingle(ballDeactivate, m_BML->GetGroupByName("All_Sound"));

    m_BML->AddTimer(2ul, [this]() {
        auto *curBall = (CK3dEntity *) m_CurLevel->GetElementObject(0, 1);
        if (curBall) {
            ExecuteBB::Unphysicalize(curBall);

            m_DynamicPos->ActivateInput(1);
            m_DynamicPos->Activate();

            m_BML->AddTimer(1ul, [this, curBall]() {
                VxMatrix matrix;
                m_CurLevel->GetElementValue(0, 3, &matrix);
                curBall->SetWorldMatrix(matrix);

                CK3dEntity *camMF = m_BML->Get3dEntityByName("Cam_MF");
                m_BML->RestoreIC(camMF, true);
                camMF->SetWorldMatrix(matrix);

                m_BML->AddTimer(1ul, [this]() {
                    m_DynamicPos->ActivateInput(0);
                    m_DynamicPos->Activate();
                    m_PhysicsNewBall->ActivateInput(0);
                    m_PhysicsNewBall->Activate();
                    m_PhysicsNewBall->GetParent()->Activate();
                });
            });
        }
    });
}


int DebugUtilities::GetSectorCount() {
    CKDataArray *checkPoints = m_BML->GetArrayByName("Checkpoints");
    if (!checkPoints)
        return 0;
    return checkPoints->GetRowCount();
}

void DebugUtilities::SetSector(int sector) {
    if (m_BML->IsPlaying()) {
        CKDataArray *checkPoints = m_BML->GetArrayByName("Checkpoints");
        CKDataArray *resetPoints = m_BML->GetArrayByName("ResetPoints");

        if (sector < 1 || sector > checkPoints->GetRowCount() + 1)
            return;

        int curSector = ScriptHelper::GetParamValue<int>(m_CurSector);
        if (curSector != sector) {
            VxMatrix matrix;
            resetPoints->GetElementValue(sector - 1, 0, &matrix);
            m_CurLevel->SetElementValue(0, 3, &matrix);

            m_IngameParam->SetElementValue(0, 1, &sector);
            m_IngameParam->SetElementValue(0, 2, &curSector);
            ScriptHelper::SetParamValue(m_CurSector, sector);

            m_BML->SendIngameMessage(("Changed to Sector " + std::to_string(sector)).c_str());

            CKBehavior *sectorMgr = m_BML->GetScriptByName("Gameplay_SectorManager");
            m_CKContext->GetCurrentScene()->Activate(sectorMgr, true);

            m_BML->AddTimerLoop(1ul, [this, sector, checkPoints, sectorMgr]() {
                if (sectorMgr->IsActive())
                    return true;

                m_BML->AddTimer(2ul, [this, checkPoints, sector]() {
                    CKBOOL active = false;
                    m_CurLevel->SetElementValue(0, 4, &active);

                    CK_ID flameId;
                    checkPoints->GetElementValue(sector % 2, 1, &flameId);
                    auto *flame = (CK3dEntity *) m_CKContext->GetObject(flameId);
                    m_CKContext->GetCurrentScene()->Activate(flame->GetScript(0), true);

                    checkPoints->GetElementValue(sector - 1, 1, &flameId);
                    flame = (CK3dEntity *) m_CKContext->GetObject(flameId);
                    m_CKContext->GetCurrentScene()->Activate(flame->GetScript(0), true);

                    if (sector > checkPoints->GetRowCount()) {
                        CKMessageManager *mm = m_BML->GetMessageManager();
                        CKMessageType msg = mm->AddMessageType("last Checkpoint reached");
                        mm->SendMessageSingle(msg, m_BML->GetGroupByName("All_Sound"));

                        ResetBall();
                    } else {
                        m_BML->AddTimer(2ul, [this, sector, checkPoints, flame]() {
                            VxMatrix matrix;
                            checkPoints->GetElementValue(sector - 1, 0, &matrix);
                            flame->SetWorldMatrix(matrix);
                            CKBOOL active = true;
                            m_CurLevel->SetElementValue(0, 4, &active);
                            m_CKContext->GetCurrentScene()->Activate(flame->GetScript(0), true);
                            m_BML->Show(flame, CKSHOW, true);

                            ResetBall();
                        });
                    }
                });
                return false;
            });
        }
    }
}

void DebugUtilities::OnEditScript_Gameplay_Ingame(CKBehavior *script) {
    GetLogger()->Info("Debug Ball Force");
    CKBehavior *ballNav = FindFirstBB(script, "Ball Navigation");
    CKBehavior *nop[2] = {nullptr, nullptr};
    FindBB(ballNav, [&nop](CKBehavior *beh) {
        if (nop[0]) nop[1] = beh;
        else nop[0] = beh;
        return !nop[1];
    }, "Nop");
    CKBehavior *keyevent[2] = {CreateBB(ballNav, VT_CONTROLLERS_KEYEVENT), CreateBB(ballNav, VT_CONTROLLERS_KEYEVENT)};
    m_BallForce[0] = CreateParamValue(ballNav, "Up", CKPGUID_KEY, CKKEYBOARD(0));
    m_BallForce[1] = CreateParamValue(ballNav, "Down", CKPGUID_KEY, CKKEYBOARD(0));
    CKBehavior *phyforce[2] = {CreateBB(ballNav, PHYSICS_RT_PHYSICSFORCE, true),
                               CreateBB(ballNav, PHYSICS_RT_PHYSICSFORCE, true)};
    CKBehavior *op = FindFirstBB(ballNav, "Op");
    CKParameter *mass = op->GetInputParameter(0)->GetDirectSource();
    CKBehavior *spf = FindFirstBB(ballNav, "SetPhysicsForce");
    CKParameter *dir[2] = {CreateParamValue(ballNav, "Up", CKPGUID_VECTOR, VxVector(0, 1, 0)),
                           CreateParamValue(ballNav, "Down", CKPGUID_VECTOR, VxVector(0, -1, 0))};
    CKBehavior *wake = FindFirstBB(ballNav, "Physics WakeUp");

    for (int i = 0; i < 2; i++) {
        keyevent[i]->GetInputParameter(0)->SetDirectSource(m_BallForce[i]);
        CreateLink(ballNav, nop[0], keyevent[i], 0, 0);
        CreateLink(ballNav, nop[1], keyevent[i], 0, 1);
        phyforce[i]->GetTargetParameter()->ShareSourceWith(spf->GetTargetParameter());
        phyforce[i]->GetInputParameter(0)->ShareSourceWith(spf->GetInputParameter(0));
        phyforce[i]->GetInputParameter(1)->ShareSourceWith(spf->GetInputParameter(1));
        phyforce[i]->GetInputParameter(2)->SetDirectSource(dir[i]);
        phyforce[i]->GetInputParameter(3)->ShareSourceWith(spf->GetInputParameter(3));
        phyforce[i]->GetInputParameter(4)->SetDirectSource(mass);
        CreateLink(ballNav, keyevent[i], phyforce[i], 0, 0);
        CreateLink(ballNav, keyevent[i], phyforce[i], 1, 1);
        CreateLink(ballNav, nop[1], phyforce[i], 0, 1);
        CreateLink(ballNav, phyforce[i], wake, 0, 0);
        CreateLink(ballNav, phyforce[i], wake, 1, 0);
    }

    CKBehavior *ballMgr = FindFirstBB(script, "BallManager");
    m_DynamicPos = FindNextBB(script, ballMgr, "TT Set Dynamic Position");

    CKBehavior *newBall = FindFirstBB(ballMgr, "New Ball");
    m_PhysicsNewBall = FindFirstBB(newBall, "physicalize new Ball");

    CKBehavior *trafoMgr = FindFirstBB(script, "Trafo Manager");
    m_SetNewBall = FindFirstBB(trafoMgr, "set new Ball");
    CKBehavior *sop = FindFirstBB(m_SetNewBall, "Switch On Parameter");
    m_CurTrafo = sop->GetInputParameter(0)->GetDirectSource();
    m_CurLevel = m_BML->GetArrayByName("CurrentLevel");
    m_IngameParam = m_BML->GetArrayByName("IngameParameter");
}

void DebugUtilities::OnEditScript_Gameplay_Events(CKBehavior *script) {
    CKBehavior *id = FindNextBB(script, script->GetInput(0));
    m_CurSector = id->GetOutputParameter(0)->GetDestination(0);
}

void DebugUtilities::OnProcess_Suicide() {
    if (m_EnableSuicideKey->GetBoolean() && !m_SuicideCd && m_InputHook->IsKeyPressed(m_Suicide->GetKey())) {
        m_BML->ExecuteCommand("kill");
        m_BML->AddTimer(1000.0f, [this]() { m_SuicideCd = false; });
        m_SuicideCd = true;
    }
}

void DebugUtilities::OnProcess_ChangeBall() {
    if (m_ChangeBallCd != 0) {
        return;
    }

    static char trafoTypes[3][6] = {"paper", "wood", "stone"};

    for (int i = 0; i < 3; i++) {
        if (m_InputHook->IsKeyPressed(m_ChangeBall[i]->GetKey()) &&
            strcmp(GetParamString(m_CurTrafo), trafoTypes[i]) != 0) {
            CKMessageManager *mm = m_BML->GetMessageManager();
            CKMessageType ballDeactivate = mm->AddMessageType("BallNav deactivate");

            mm->SendMessageSingle(ballDeactivate, m_BML->GetGroupByName("All_Gameplay"));
            mm->SendMessageSingle(ballDeactivate, m_BML->GetGroupByName("All_Sound"));
            m_InputHook->Block(CK_INPUT_DEVICE_KEYBOARD);
            m_ChangeBallCd = 2;

            m_BML->AddTimer(0.01f, [this, i]() {
                auto *curBall = (CK3dEntity *) m_CurLevel->GetElementObject(0, 1);
                ExecuteBB::Unphysicalize(curBall);

                SetParamString(m_CurTrafo, trafoTypes[i]);
                m_SetNewBall->ActivateInput(0);
                m_SetNewBall->Activate();

                m_InputHook->Unblock(CK_INPUT_DEVICE_KEYBOARD);
                m_ChangeBallCd = 0;
                GetLogger()->Info("Set to %s Ball", i == 0 ? "Paper" : i == 1 ? "Wood" : "Stone");
            });
        }
    }
}

void DebugUtilities::OnProcess_ResetBall() {
    if (m_InputHook->IsKeyPressed(m_ResetBall->GetKey())) {
        CKMessageManager *mm = m_BML->GetMessageManager();
        CKMessageType ballDeactivate = mm->AddMessageType("BallNav deactivate");

        mm->SendMessageSingle(ballDeactivate, m_BML->GetGroupByName("All_Gameplay"));
        mm->SendMessageSingle(ballDeactivate, m_BML->GetGroupByName("All_Sound"));

        m_BML->AddTimer(2ul, [this]() {
            auto *curBall = (CK3dEntity *) m_CurLevel->GetElementObject(0, 1);
            if (curBall) {
                ExecuteBB::Unphysicalize(curBall);

                CKDataArray *ph = m_BML->GetArrayByName("PH");
                for (int i = 0; i < ph->GetRowCount(); i++) {
                    CKBOOL set = true;
                    char name[100];
                    ph->GetElementStringValue(i, 1, name);
                    if (!strcmp(name, "P_Extra_Point"))
                        ph->SetElementValue(i, 4, &set);
                }

                m_IngameParam->SetElementValueFromParameter(0, 1, m_CurSector);
                m_IngameParam->SetElementValueFromParameter(0, 2, m_CurSector);
                CKBehavior *sectorMgr = m_BML->GetScriptByName("Gameplay_SectorManager");
                m_BML->GetCKContext()->GetCurrentScene()->Activate(sectorMgr, true);

                m_BML->AddTimerLoop(1ul, [this, curBall, sectorMgr]() {
                    if (sectorMgr->IsActive())
                        return true;

                    m_DynamicPos->ActivateInput(1);
                    m_DynamicPos->Activate();

                    m_BML->AddTimer(1ul, [this, curBall]() {
                        VxMatrix matrix;
                        m_CurLevel->GetElementValue(0, 3, &matrix);
                        curBall->SetWorldMatrix(matrix);

                        CK3dEntity *camMF = m_BML->Get3dEntityByName("Cam_MF");
                        m_BML->RestoreIC(camMF, true);
                        camMF->SetWorldMatrix(matrix);

                        m_BML->AddTimer(1ul, [this]() {
                            m_DynamicPos->ActivateInput(0);
                            m_DynamicPos->Activate();

                            m_PhysicsNewBall->ActivateInput(0);
                            m_PhysicsNewBall->Activate();
                            m_PhysicsNewBall->GetParent()->Activate();

                            GetLogger()->Info("Sector Reset");
                        });
                    });

                    return false;
                });
            }
        });
    }
}

void DebugUtilities::OnProcess_ChangeSpeed() {
    bool speedup = m_InputHook->IsKeyDown(m_SpeedupBall->GetKey());
    if (speedup && !m_Speedup)
        m_BML->ExecuteCommand("speed 3");
    if (!speedup && m_Speedup)
        m_BML->ExecuteCommand("speed 1");
    m_Speedup = speedup;
}

void DebugUtilities::OnProcess_AddLife() {
    if (!m_AddLifeCd && m_InputHook->IsKeyPressed(m_AddLife->GetKey())) {
        CKMessageManager *mm = m_BML->GetMessageManager();
        CKMessageType addLife = mm->AddMessageType("Life_Up");

        mm->SendMessageSingle(addLife, m_BML->GetGroupByName("All_Gameplay"));
        mm->SendMessageSingle(addLife, m_BML->GetGroupByName("All_Sound"));
        m_AddLifeCd = true;
        m_BML->AddTimer(1000.0f, [this]() { m_AddLifeCd = false; });
    }
}

void DebugUtilities::OnProcess_SkipRender() {
    m_SkipRender = (m_BML->IsCheatEnabled() && m_InputHook->IsKeyDown(m_SkipRenderKey->GetKey()));
    if (m_SkipRender)
        m_RenderContext->ChangeCurrentRenderOptions(0, CK_RENDER_DEFAULTSETTINGS);
    else
        m_RenderContext->ChangeCurrentRenderOptions(CK_RENDER_DEFAULTSETTINGS, 0);
}

void DebugUtilities::OnProcess_Summon() {
    VxVector vector;

    if (m_CurSel < 0) {
        for (int i = 0; i < 4; i++) {
            if (m_InputHook->IsKeyDown(m_AddBall[i]->GetKey())) {
                m_CurSel = i;
                m_InputHook->SetBlock(true);
            }
        }

        if (m_CurSel >= 0) {
            m_CurObj = (CK3dEntity *) m_BML->GetCKContext()->CopyObject(m_Balls[m_CurSel]);
            vector = VxVector(0, 5, 0);
            m_CurObj->SetPosition(&vector, m_CamTarget);
            m_CurObj->Show();
        }
    } else if (m_InputHook->oIsKeyDown(m_AddBall[m_CurSel]->GetKey())) {
        if (m_InputHook->oIsKeyDown(m_MoveKeys[0]->GetKey())) {
            vector = VxVector(0, 0, 0.1f * m_DeltaTime);
            m_CurObj->Translate(&vector, m_CamOrientRef);
        }
        if (m_InputHook->oIsKeyDown(m_MoveKeys[1]->GetKey())) {
            vector = VxVector(0, 0, -0.1f * m_DeltaTime);
            m_CurObj->Translate(&vector, m_CamOrientRef);
        }
        if (m_InputHook->oIsKeyDown(m_MoveKeys[2]->GetKey())) {
            vector = VxVector(-0.1f * m_DeltaTime, 0, 0);
            m_CurObj->Translate(&vector, m_CamOrientRef);
        }
        if (m_InputHook->oIsKeyDown(m_MoveKeys[3]->GetKey())) {
            vector = VxVector(0.1f * m_DeltaTime, 0, 0);
            m_CurObj->Translate(&vector, m_CamOrientRef);
        }
        if (m_InputHook->oIsKeyDown(m_MoveKeys[4]->GetKey())) {
            vector = VxVector(0, 0.1f * m_DeltaTime, 0);
            m_CurObj->Translate(&vector, m_CamOrientRef);
        }
        if (m_InputHook->oIsKeyDown(m_MoveKeys[5]->GetKey())) {
            vector = VxVector(0, -0.1f * m_DeltaTime, 0);
            m_CurObj->Translate(&vector, m_CamOrientRef);
        }
    } else {
        CKMesh *mesh = m_CurObj->GetMesh(0);
        switch (m_CurSel) {
            case 0:
                ExecuteBB::PhysicalizeConvex(m_CurObj, false, 0.5f, 0.4f, 0.2f, "", false, true, false, 1.5f, 0.1f, mesh->GetName(), VxVector(0, 0, 0), mesh);
                break;
            case 1:
                ExecuteBB::PhysicalizeBall(m_CurObj, false, 0.6f, 0.2f, 2.0f, "", false, true, false, 0.6f, 0.1f, mesh->GetName());
                break;
            case 2:
                ExecuteBB::PhysicalizeBall(m_CurObj, false, 0.7f, 0.1f, 10.0f, "", false, true, false, 0.2f, 0.1f, mesh->GetName());
                break;
            default:
                ExecuteBB::PhysicalizeConvex(m_CurObj, false, 0.7f, 0.3f, 1.0f, "", false, true, false, 0.1f, 0.1f, mesh->GetName(), VxVector(0, 0, 0), mesh);
                break;
        }

        CKDataArray *ph = m_BML->GetArrayByName("PH");
        ph->AddRow();
        int index = ph->GetRowCount() - 1;
        ph->SetElementValueFromParameter(index, 0, m_CurSector);
        static char P_BALL_NAMES[4][13] = {"P_Ball_Paper", "P_Ball_Wood", "P_Ball_Stone", "P_Box"};
        ph->SetElementStringValue(index, 1, P_BALL_NAMES[m_CurSel]);
        VxMatrix matrix = m_CurObj->GetWorldMatrix();
        ph->SetElementValue(index, 2, &matrix);
        ph->SetElementObject(index, 3, m_CurObj);
        CKBOOL set = false;
        ph->SetElementValue(index, 4, &set);

        CKGroup *depth = m_BML->GetGroupByName("DepthTest");
        depth->AddObject(m_CurObj);
        m_TempBalls.emplace_back(index, m_CurObj);

        m_CurSel = -1;
        m_CurObj = nullptr;
        m_InputHook->SetBlock(false);

        GetLogger()->Info("Summoned a %s", m_CurSel < 2 ? m_CurSel == 0 ? "Paper Ball" : "Wood Ball" : m_CurSel == 2 ? "Stone Ball" : "Box");
    }
}

#include "DebugUtilities.h"
#include "DebugCommands.h"

using namespace ScriptHelper;

BML_DEFINE_MOD(DebugUtilities, {
    .Id = "DebugUtilities",
    .Name = "Debug Utilities",
    .Version = "0.3.2",
    .Author = "Gamepiaynmo & Kakuty",
    .Description = "Ballance Debug Utilities.",
    .MinimumLoaderVersion = BML_VERSION,
})

void DebugUtilities::OnLoad(const BML::Mod::Context &context) {
    m_Context = context;
    m_Timers = ModSupport::Timers(context);
    m_Context.Config().SetCategoryComment("Debug", "Debug Utilities").Take();

    m_EnableSuicideKey =
        m_Context.Config()
            .DeclareProperty({"Debug", "EnableSuicideKey", bool(true), "Enable the Suicide Hotkey"})
            .Take();

    m_Suicide = m_Context.Config()
                    .DeclareProperty({"Debug", "Suicide", uint32_t(CKKEY_R), "Suicide"})
                    .Take();

    m_BallCheat[0] = m_Context.Config()
                         .DeclareProperty({"Debug", "BallUp", uint32_t(CKKEY_F1),
                                           "Apply an upward force to the ball"})
                         .Take();

    m_BallCheat[1] = m_Context.Config()
                         .DeclareProperty({"Debug", "BallDown", uint32_t(CKKEY_F2),
                                           "Apply a downward force to the ball"})
                         .Take();

    m_ChangeBall[0] =
        m_Context.Config()
            .DeclareProperty({"Debug", "TurnPaper", uint32_t(CKKEY_I), "Turn into paper ball"})
            .Take();

    m_ChangeBall[1] =
        m_Context.Config()
            .DeclareProperty({"Debug", "TurnWood", uint32_t(CKKEY_O), "Turn into wood ball"})
            .Take();

    m_ChangeBall[2] =
        m_Context.Config()
            .DeclareProperty({"Debug", "TurnStone", uint32_t(CKKEY_P), "Turn into stone ball"})
            .Take();

    m_ResetBall = m_Context.Config()
                      .DeclareProperty(
                          {"Debug", "ResetBall", uint32_t(CKKEY_BACK), "Reset ball and all moduls"})
                      .Take();

    m_AddLife = m_Context.Config()
                    .DeclareProperty({"Debug", "AddLife", uint32_t(CKKEY_L), "Add one extra Life"})
                    .Take();

    m_SpeedupBall = m_Context.Config()
                        .DeclareProperty({"Debug", "BallSpeedUp", uint32_t(CKKEY_LCONTROL),
                                          "Change to 3 times ball speed"})
                        .Take();

    m_EnableSuicideKey = m_Context.Config().FindProperty("Debug", "EnableSuicideKey").Value();
    m_EnableSuicideKey.SetComment("Enable the Suicide Hotkey").Take();
    m_EnableSuicideKey.SetDefault(bool(true)).Take();
    m_Suicide = m_Context.Config().FindProperty("Debug", "Suicide").Value();
    m_Suicide.SetComment("Suicide").Take();
    m_Suicide.SetDefault(uint32_t(CKKEY_R)).Take();

    m_BallCheat[0] = m_Context.Config().FindProperty("Debug", "BallUp").Value();
    m_BallCheat[0].SetComment("Apply an upward force to the ball").Take();
    m_BallCheat[0].SetDefault(uint32_t(CKKEY_F1)).Take();

    m_BallCheat[1] = m_Context.Config().FindProperty("Debug", "BallDown").Value();
    m_BallCheat[1].SetComment("Apply a downward force to the ball").Take();
    m_BallCheat[1].SetDefault(uint32_t(CKKEY_F2)).Take();

    m_ChangeBall[0] = m_Context.Config().FindProperty("Debug", "TurnPaper").Value();
    m_ChangeBall[0].SetComment("Turn into paper ball").Take();
    m_ChangeBall[0].SetDefault(uint32_t(CKKEY_I)).Take();

    m_ChangeBall[1] = m_Context.Config().FindProperty("Debug", "TurnWood").Value();
    m_ChangeBall[1].SetComment("Turn into wood ball").Take();
    m_ChangeBall[1].SetDefault(uint32_t(CKKEY_O)).Take();

    m_ChangeBall[2] = m_Context.Config().FindProperty("Debug", "TurnStone").Value();
    m_ChangeBall[2].SetComment("Turn into stone ball").Take();
    m_ChangeBall[2].SetDefault(uint32_t(CKKEY_P)).Take();

    m_ResetBall = m_Context.Config().FindProperty("Debug", "ResetBall").Value();
    m_ResetBall.SetComment("Reset ball and all moduls").Take();
    m_ResetBall.SetDefault(uint32_t(CKKEY_BACK)).Take();

    m_AddLife = m_Context.Config().FindProperty("Debug", "AddLife").Value();
    m_AddLife.SetComment("Add one extra Life").Take();
    m_AddLife.SetDefault(uint32_t(CKKEY_L)).Take();

    m_SpeedupBall = m_Context.Config().FindProperty("Debug", "BallSpeedUp").Value();
    m_SpeedupBall.SetComment("Change to 3 times ball speed").Take();
    m_SpeedupBall.SetDefault(uint32_t(CKKEY_LCONTROL)).Take();

    m_SpeedNotification =
        m_Context.Config()
            .DeclareProperty({"Debug", "SpeedNotification", bool(true),
                              "Notify the player when speed of the ball changes."})
            .Take();

    m_SkipRenderKey = m_Context.Config()
                          .DeclareProperty({"Debug", "SkipRender", uint32_t(CKKEY_F),
                                            "Skip rendering of current frames while holding."})
                          .Take();

    m_Context.Config().SetCategoryComment("Auxiliaries", "Temporal Auxiliary Moduls").Take();

    m_AddBall[0] =
        m_Context.Config()
            .DeclareProperty({"Auxiliaries", "PaperBall", uint32_t(CKKEY_J), "Add a Paper Ball"})
            .Take();

    m_AddBall[1] =
        m_Context.Config()
            .DeclareProperty({"Auxiliaries", "WoodBall", uint32_t(CKKEY_K), "Add a Wood Ball"})
            .Take();

    m_AddBall[2] =
        m_Context.Config()
            .DeclareProperty({"Auxiliaries", "StoneBall", uint32_t(CKKEY_N), "Add a Stone Ball"})
            .Take();

    m_AddBall[3] = m_Context.Config()
                       .DeclareProperty({"Auxiliaries", "Box", uint32_t(CKKEY_M), "Add a Box"})
                       .Take();

    m_MoveKeys[0] =
        m_Context.Config()
            .DeclareProperty({"Auxiliaries", "MoveFront", uint32_t(CKKEY_UP), "Move Front"})
            .Take();

    m_MoveKeys[1] =
        m_Context.Config()
            .DeclareProperty({"Auxiliaries", "MoveBack", uint32_t(CKKEY_DOWN), "Move Back"})
            .Take();

    m_MoveKeys[2] =
        m_Context.Config()
            .DeclareProperty({"Auxiliaries", "MoveLeft", uint32_t(CKKEY_LEFT), "Move Left"})
            .Take();

    m_MoveKeys[3] =
        m_Context.Config()
            .DeclareProperty({"Auxiliaries", "MoveRight", uint32_t(CKKEY_RIGHT), "Move Right"})
            .Take();

    m_MoveKeys[4] =
        m_Context.Config()
            .DeclareProperty({"Auxiliaries", "MoveUp", uint32_t(CKKEY_RSHIFT), "Move Up"})
            .Take();

    m_MoveKeys[5] =
        m_Context.Config()
            .DeclareProperty({"Auxiliaries", "MoveDown", uint32_t(CKKEY_RCONTROL), "Move Down"})
            .Take();

    m_Balls[0] = (CK3dEntity *)ModSupport::LoadObject(
        m_Behaviors, m_Context, "3D Entities\\PH\\P_Ball_Paper.nmo", true, "P_Ball_Paper_MF");
    m_Balls[1] = (CK3dEntity *)ModSupport::LoadObject(
        m_Behaviors, m_Context, "3D Entities\\PH\\P_Ball_Wood.nmo", true, "P_Ball_Wood_MF");
    m_Balls[2] = (CK3dEntity *)ModSupport::LoadObject(
        m_Behaviors, m_Context, "3D Entities\\PH\\P_Ball_Stone.nmo", true, "P_Ball_Stone_MF");
    m_Balls[3] = (CK3dEntity *)ModSupport::LoadObject(
        m_Behaviors, m_Context, "3D Entities\\PH\\P_Box.nmo", true, "P_Box_MF");

    ModSupport::RegisterCommand(m_Context, m_Commands, std::make_shared<CommandScore>());
    ModSupport::RegisterCommand(m_Context, m_Commands, std::make_shared<CommandKill>(this));
    ModSupport::RegisterCommand(m_Context, m_Commands, std::make_shared<CommandSetSpawn>());
    ModSupport::RegisterCommand(m_Context, m_Commands, std::make_shared<CommandSector>(this));
    ModSupport::RegisterCommand(m_Context, m_Commands, std::make_shared<CommandWin>());
    ModSupport::RegisterCommand(m_Context, m_Commands, std::make_shared<CommandSpeed>(this));

    m_CKContext = ModSupport::Engine(m_Context);
    m_RenderContext = ModSupport::Render(m_Context);
}

void DebugUtilities::OnModifyConfig(const BML::Mod::Context &, const BML::Mod::ConfigEvent &event) {
    const auto *category = event.Category.data();
    const auto *key = event.Key.data();
    auto found = m_Context.Config().FindProperty(event.Category, event.Key);
    if (!found)
        return;
    auto prop = found.Value();

    if (ModSupport::CheatEnabled()) {
        if ((event.Category == "Debug" && event.Key == "BallUp"))
            SetParamValue(m_BallForce[0], m_BallCheat[0].Get<uint32_t>());
        if ((event.Category == "Debug" && event.Key == "BallDown"))
            SetParamValue(m_BallForce[1], m_BallCheat[1].Get<uint32_t>());
    }
}

void DebugUtilities::OnLoadObject(const BML::Mod::Context &,
                                  const BML::Mod::LoadObjectEvent &event) {
    const char *filename = event.Filename.data();
    const bool isMap = event.IsMap;
    const char *masterName = event.MasterName.data();
    const auto filterClass = static_cast<CK_CLASSID>(event.FilterClass);
    const bool addToScene = event.AddToScene, addtoscene = event.AddToScene,
               reuseMeshes = event.ReuseMeshes, reuseMaterials = event.ReuseMaterials,
               dynamic = event.Dynamic;
    XObjectArray objects;
    for (auto ref : event.Objects) {
        auto p = BML::Engine::BorrowObject<CKObject>(m_Context, ref);
        if (p)
            objects.PushBack(p.Value()->GetID());
    }
    auto *objArray = &objects;
    auto master = BML::Engine::BorrowObject<CKObject>(m_Context, event.MasterObject);
    CKObject *masterObj = master ? master.Value() : nullptr;

    if (ModSupport::ResourceIs(filename, "3D Entities\\Camera.nmo")) {
        m_CamOrientRef = ModSupport::Find<CK3dEntity>(m_Context, "Cam_OrientRef");
        m_CamTarget = ModSupport::Find<CK3dEntity>(m_Context, "Cam_Target");
    }
}

void DebugUtilities::OnLoadScript(const BML::Mod::Context &,
                                  const BML::Mod::LoadScriptEvent &event) {
    const char *filename = event.Filename.data();
    auto borrowed = BML::Engine::BorrowObject<CKBehavior>(m_Context, event.Script);
    if (!borrowed)
        return;
    auto *script = borrowed.Value();

    if (!strcmp(script->GetName(), "Gameplay_Ingame"))
        OnEditScript_Gameplay_Ingame(script);

    if (!strcmp(script->GetName(), "Gameplay_Events"))
        OnEditScript_Gameplay_Events(script);
}

void DebugUtilities::OnProcess(const BML::Mod::Context &) {
    m_DeltaTime = ModSupport::Engine(m_Context)->GetTimeManager()->GetLastDeltaTime() / 10;

    OnProcess_SkipRender();

    if (ModSupport::GameState().Playing) {
        OnProcess_Suicide();

        if (ModSupport::CheatEnabled()) {
            OnProcess_ChangeBall();
            OnProcess_ResetBall();
            OnProcess_ChangeSpeed();
            OnProcess_AddLife();
            OnProcess_Summon();
        }
    }
}

void DebugUtilities::OnCheatEnabled(const BML::Mod::Context &, const BML::Mod::CheatEvent &event) {
    const bool enable = event.Enabled;

    if (enable) {
        SetParamValue(m_BallForce[0], m_BallCheat[0].Get<uint32_t>());
        SetParamValue(m_BallForce[1], m_BallCheat[1].Get<uint32_t>());
    } else {
        if (abs(m_SpeedTimes - 1.0f) > EPSILON)
            ChangeBallSpeed(1.0f);

        SetParamValue(m_BallForce[0], CKKEYBOARD(0));
        SetParamValue(m_BallForce[1], CKKEYBOARD(0));
    }
}

void DebugUtilities::OnStartLevel(const BML::Mod::Context &) {
    m_InLevel = true;
    m_Paused = false;
}

void DebugUtilities::OnPreResetLevel(const BML::Mod::Context &) { m_InLevel = false; }

void DebugUtilities::OnPostResetLevel(const BML::Mod::Context &) {
    CKDataArray *ph = ModSupport::Find<CKDataArray>(m_Context, "PH");
    for (auto iter = m_TempBalls.rbegin(); iter != m_TempBalls.rend(); iter++) {
        ph->RemoveRow(iter->first);
        ModSupport::Engine(m_Context)->DestroyObject(iter->second);
    }
    m_TempBalls.clear();
}

void DebugUtilities::OnPauseLevel(const BML::Mod::Context &) { m_Paused = true; }

void DebugUtilities::OnUnpauseLevel(const BML::Mod::Context &) { m_Paused = false; }

void DebugUtilities::OnPostExitLevel(const BML::Mod::Context &) { m_InLevel = false; }

void DebugUtilities::OnPostNextLevel(const BML::Mod::Context &) { m_InLevel = false; }

void DebugUtilities::OnDead(const BML::Mod::Context &) { m_InLevel = false; }

void DebugUtilities::OnPostEndLevel(const BML::Mod::Context &) { m_InLevel = false; }

void DebugUtilities::OnLevelFinish(const BML::Mod::Context &) { m_InLevel = false; }

void DebugUtilities::ChangeBallSpeed(float times) {
    if (times < 0.1f) {
        BML::UI::AddMessage("Invalid Ball Speed Change");
        return;
    }

    if (ModSupport::GameState().Ingame) {
        bool notify = true;

        if (!m_PhysicsBall) {
            m_PhysicsBall = ModSupport::Find<CKDataArray>(m_Context, "Physicalize_GameBall");
            CKBehavior *ingame = ModSupport::Find<CKBehavior>(m_Context, "Gameplay_Ingame");
            m_Force = ScriptHelper::FindFirstBB(ingame, "Ball Navigation")
                          ->GetInputParameter(0)
                          ->GetRealSource();

            for (int i = 0; i < m_PhysicsBall->GetRowCount(); i++) {
                const int nameSize = m_PhysicsBall->GetElementStringValue(i, 0, nullptr, 0);
                if (nameSize <= 1)
                    continue;
                std::string ballName(nameSize, '\0');
                m_PhysicsBall->GetElementStringValue(i, 0, ballName.data(), nameSize);
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
                const int nameSize = m_PhysicsBall->GetElementStringValue(i, 0, nullptr, 0);
                if (nameSize <= 1)
                    continue;
                std::string ballName(nameSize, '\0');
                m_PhysicsBall->GetElementStringValue(i, 0, ballName.data(), nameSize);
                ballName.pop_back();
                auto iter = m_Forces.find(ballName);
                if (iter != m_Forces.end()) {
                    float force = iter->second;
                    force *= times;
                    m_PhysicsBall->SetElementValue(i, 7, &force);
                }
            }

            if (notify && m_SpeedNotification.Get<bool>())
                BML::UI::AddMessage(
                    ("Current Ball Speed Changed to " + std::to_string(times) + " times").c_str());

            m_SpeedTimes = times;
        }
    }
}

void DebugUtilities::ResetBall() {
    CKMessageManager *mm = ModSupport::Engine(m_Context)->GetMessageManager();
    CKMessageType ballDeactivate = mm->AddMessageType("BallNav deactivate");

    mm->SendMessageSingle(ballDeactivate, ModSupport::Find<CKGroup>(m_Context, "All_Gameplay"));
    mm->SendMessageSingle(ballDeactivate, ModSupport::Find<CKGroup>(m_Context, "All_Sound"));

    m_Timers.AfterTicks(2, [this]() {
        auto *curBall = (CK3dEntity *)m_CurLevel->GetElementObject(0, 1);
        if (curBall) {
            ModSupport::Unphysicalize(m_Behaviors, m_Context, curBall);

            m_DynamicPos->ActivateInput(1);
            m_DynamicPos->Activate();

            m_Timers.AfterTicks(1, [this, curBall = ModSupport::Object(curBall)]() {
                VxMatrix matrix;
                m_CurLevel->GetElementValue(0, 3, &matrix);
                curBall->SetWorldMatrix(matrix);

                CK3dEntity *camMF = ModSupport::Find<CK3dEntity>(m_Context, "Cam_MF");
                ModSupport::RestoreIC(m_Context, camMF, true);
                camMF->SetWorldMatrix(matrix);

                m_Timers.AfterTicks(1, [this]() {
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
    CKDataArray *checkPoints = ModSupport::Find<CKDataArray>(m_Context, "Checkpoints");
    if (!checkPoints)
        return 0;
    return checkPoints->GetRowCount();
}

void DebugUtilities::SetSector(int sector) {
    if (ModSupport::GameState().Playing) {
        CKDataArray *checkPoints = ModSupport::Find<CKDataArray>(m_Context, "Checkpoints");
        CKDataArray *resetPoints = ModSupport::Find<CKDataArray>(m_Context, "ResetPoints");

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

            BML::UI::AddMessage(("Changed to Sector " + std::to_string(sector)).c_str());

            CKBehavior *sectorMgr =
                ModSupport::Find<CKBehavior>(m_Context, "Gameplay_SectorManager");
            m_CKContext->GetCurrentScene()->Activate(sectorMgr, true);

            m_Timers.EveryTicks(1, [this, sector, checkPoints,
                                    sectorMgr = ModSupport::Object(sectorMgr)]() {
                if (sectorMgr->IsActive())
                    return true;

                m_Timers.AfterTicks(2, [this, checkPoints, sector]() {
                    CKBOOL active = false;
                    m_CurLevel->SetElementValue(0, 4, &active);

                    CK_ID flameId;
                    checkPoints->GetElementValue(sector % 2, 1, &flameId);
                    auto *flame = (CK3dEntity *)m_CKContext->GetObject(flameId);
                    m_CKContext->GetCurrentScene()->Activate(flame->GetScript(0), true);

                    checkPoints->GetElementValue(sector - 1, 1, &flameId);
                    flame = (CK3dEntity *)m_CKContext->GetObject(flameId);
                    m_CKContext->GetCurrentScene()->Activate(flame->GetScript(0), true);

                    if (sector > checkPoints->GetRowCount()) {
                        CKMessageManager *mm = ModSupport::Engine(m_Context)->GetMessageManager();
                        CKMessageType msg = mm->AddMessageType("last Checkpoint reached");
                        mm->SendMessageSingle(msg,
                                              ModSupport::Find<CKGroup>(m_Context, "All_Sound"));

                        ResetBall();
                    } else {
                        m_Timers.AfterTicks(
                            2, [this, sector, checkPoints, flame = ModSupport::Object(flame)]() {
                                VxMatrix matrix;
                                checkPoints->GetElementValue(sector - 1, 0, &matrix);
                                flame->SetWorldMatrix(matrix);
                                CKBOOL active = true;
                                m_CurLevel->SetElementValue(0, 4, &active);
                                m_CKContext->GetCurrentScene()->Activate(flame->GetScript(0), true);
                                ModSupport::Show(m_Context, flame, CKSHOW, true);

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
    ModSupport::Log(m_Context, BML_LOG_INFO, "Debug Ball Force");
    CKBehavior *ballNav = FindFirstBB(script, "Ball Navigation");
    CKBehavior *nop[2] = {nullptr, nullptr};
    FindBB(
        ballNav,
        [&nop](CKBehavior *beh) {
            if (nop[0])
                nop[1] = beh;
            else
                nop[0] = beh;
            return !nop[1];
        },
        "Nop");
    CKBehavior *keyevent[2] = {CreateBB(ballNav, VT_CONTROLLERS_KEYEVENT),
                               CreateBB(ballNav, VT_CONTROLLERS_KEYEVENT)};
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
    m_CurLevel = ModSupport::Find<CKDataArray>(m_Context, "CurrentLevel");
    m_IngameParam = ModSupport::Find<CKDataArray>(m_Context, "IngameParameter");
}

void DebugUtilities::OnEditScript_Gameplay_Events(CKBehavior *script) {
    CKBehavior *id = FindNextBB(script, script->GetInput(0));
    m_CurSector = id->GetOutputParameter(0)->GetDestination(0);
}

void DebugUtilities::OnProcess_Suicide() {
    if (m_EnableSuicideKey.Get<bool>() && !m_SuicideCd &&
        ModSupport::Keyboard(m_Context).IsKeyPressed(m_Suicide.Get<uint32_t>())) {
        ModSupport::ExecuteCommand("kill");
        m_Timers.After(1000.0f, [this]() { m_SuicideCd = false; });
        m_SuicideCd = true;
    }
}

void DebugUtilities::OnProcess_ChangeBall() {
    if (m_ChangeBallCd != 0) {
        return;
    }

    static char trafoTypes[3][6] = {"paper", "wood", "stone"};

    for (int i = 0; i < 3; i++) {
        if (ModSupport::Keyboard(m_Context).IsKeyPressed(m_ChangeBall[i].Get<uint32_t>())) {
            CKMessageManager *mm = ModSupport::Engine(m_Context)->GetMessageManager();
            CKMessageType ballDeactivate = mm->AddMessageType("BallNav deactivate");

            mm->SendMessageSingle(ballDeactivate,
                                  ModSupport::Find<CKGroup>(m_Context, "All_Gameplay"));
            mm->SendMessageSingle(ballDeactivate,
                                  ModSupport::Find<CKGroup>(m_Context, "All_Sound"));
            m_ChangeBallBlock = m_Context.Input().AcquireBlock(BML_INPUT_BLOCK_KEYBOARD).Take();
            m_ChangeBallCd = 2;

            m_Timers.After(0.01f, [this, i]() {
                auto *curBall = (CK3dEntity *)m_CurLevel->GetElementObject(0, 1);
                ModSupport::Unphysicalize(m_Behaviors, m_Context, curBall);

                SetParamString(m_CurTrafo, trafoTypes[i]);
                m_SetNewBall->ActivateInput(0);
                m_SetNewBall->Activate();

                ModSupport::Check(m_ChangeBallBlock.Release());
                m_ChangeBallCd = 0;
                ModSupport::Log(m_Context, BML_LOG_INFO, "Set to %s Ball",
                                i == 0   ? "Paper"
                                : i == 1 ? "Wood"
                                         : "Stone");
            });
        }
    }
}

void DebugUtilities::OnProcess_ResetBall() {
    if (ModSupport::Keyboard(m_Context).IsKeyPressed(m_ResetBall.Get<uint32_t>())) {
        CKMessageManager *mm = ModSupport::Engine(m_Context)->GetMessageManager();
        CKMessageType ballDeactivate = mm->AddMessageType("BallNav deactivate");

        mm->SendMessageSingle(ballDeactivate, ModSupport::Find<CKGroup>(m_Context, "All_Gameplay"));
        mm->SendMessageSingle(ballDeactivate, ModSupport::Find<CKGroup>(m_Context, "All_Sound"));

        m_Timers.AfterTicks(2, [this]() {
            auto *curBall = (CK3dEntity *)m_CurLevel->GetElementObject(0, 1);
            if (curBall) {
                ModSupport::Unphysicalize(m_Behaviors, m_Context, curBall);

                CKDataArray *ph = ModSupport::Find<CKDataArray>(m_Context, "PH");
                for (int i = 0; i < ph->GetRowCount(); i++) {
                    CKBOOL set = true;
                    char name[100] = {};
                    ph->GetElementStringValue(i, 1, name, sizeof(name));
                    if (!strcmp(name, "P_Extra_Point"))
                        ph->SetElementValue(i, 4, &set);
                }

                m_IngameParam->SetElementValueFromParameter(0, 1, m_CurSector);
                m_IngameParam->SetElementValueFromParameter(0, 2, m_CurSector);
                CKBehavior *sectorMgr =
                    ModSupport::Find<CKBehavior>(m_Context, "Gameplay_SectorManager");
                ModSupport::Engine(m_Context)->GetCurrentScene()->Activate(sectorMgr, true);

                m_Timers.EveryTicks(1, [this, curBall = ModSupport::Object(curBall),
                                        sectorMgr = ModSupport::Object(sectorMgr)]() {
                    if (sectorMgr->IsActive())
                        return true;

                    m_DynamicPos->ActivateInput(1);
                    m_DynamicPos->Activate();

                    m_Timers.AfterTicks(1, [this, curBall = ModSupport::Object(curBall)]() {
                        VxMatrix matrix;
                        m_CurLevel->GetElementValue(0, 3, &matrix);
                        curBall->SetWorldMatrix(matrix);

                        CK3dEntity *camMF = ModSupport::Find<CK3dEntity>(m_Context, "Cam_MF");
                        ModSupport::RestoreIC(m_Context, camMF, true);
                        camMF->SetWorldMatrix(matrix);

                        m_Timers.AfterTicks(1, [this]() {
                            m_DynamicPos->ActivateInput(0);
                            m_DynamicPos->Activate();

                            m_PhysicsNewBall->ActivateInput(0);
                            m_PhysicsNewBall->Activate();
                            m_PhysicsNewBall->GetParent()->Activate();

                            ModSupport::Log(m_Context, BML_LOG_INFO, "Sector Reset");
                        });
                    });

                    return false;
                });
            }
        });
    }
}

void DebugUtilities::OnProcess_ChangeSpeed() {
    bool speedup = ModSupport::Keyboard(m_Context).IsKeyDown(m_SpeedupBall.Get<uint32_t>());
    if (speedup && !m_Speedup)
        ModSupport::ExecuteCommand("speed 3");
    if (!speedup && m_Speedup)
        ModSupport::ExecuteCommand("speed 1");
    m_Speedup = speedup;
}

void DebugUtilities::OnProcess_AddLife() {
    if (!m_AddLifeCd && ModSupport::Keyboard(m_Context).IsKeyPressed(m_AddLife.Get<uint32_t>())) {
        CKMessageManager *mm = ModSupport::Engine(m_Context)->GetMessageManager();
        CKMessageType addLife = mm->AddMessageType("Life_Up");

        mm->SendMessageSingle(addLife, ModSupport::Find<CKGroup>(m_Context, "All_Gameplay"));
        mm->SendMessageSingle(addLife, ModSupport::Find<CKGroup>(m_Context, "All_Sound"));
        m_AddLifeCd = true;
        m_Timers.After(1000.0f, [this]() { m_AddLifeCd = false; });
    }
}

void DebugUtilities::OnProcess_SkipRender() {
    m_SkipRender = (ModSupport::CheatEnabled() &&
                    ModSupport::Keyboard(m_Context).IsKeyDown(m_SkipRenderKey.Get<uint32_t>()));
    if (m_SkipRender)
        m_RenderContext->ChangeCurrentRenderOptions(0, CK_RENDER_DEFAULTSETTINGS);
    else
        m_RenderContext->ChangeCurrentRenderOptions(CK_RENDER_DEFAULTSETTINGS, 0);
}

void DebugUtilities::OnProcess_Summon() {
    VxVector vector;

    if (m_CurSel < 0) {
        for (int i = 0; i < 4; i++) {
            if (ModSupport::Keyboard(m_Context).IsKeyDown(m_AddBall[i].Get<uint32_t>())) {
                m_CurSel = i;
                m_SummonBlock = m_Context.Input().AcquireBlock(BML_INPUT_BLOCK_ALL).Take();
            }
        }

        if (m_CurSel >= 0) {
            m_CurObj = (CK3dEntity *)ModSupport::Engine(m_Context)->CopyObject(m_Balls[m_CurSel]);
            vector = VxVector(0, 5, 0);
            m_CurObj->SetPosition(&vector, m_CamTarget);
            m_CurObj->Show();
        }
    } else if (ModSupport::Keyboard(m_Context, BML_INPUT_RAW)
                   .IsKeyDown(m_AddBall[m_CurSel].Get<uint32_t>())) {
        if (ModSupport::Keyboard(m_Context, BML_INPUT_RAW)
                .IsKeyDown(m_MoveKeys[0].Get<uint32_t>())) {
            vector = VxVector(0, 0, 0.1f * m_DeltaTime);
            m_CurObj->Translate(&vector, m_CamOrientRef);
        }
        if (ModSupport::Keyboard(m_Context, BML_INPUT_RAW)
                .IsKeyDown(m_MoveKeys[1].Get<uint32_t>())) {
            vector = VxVector(0, 0, -0.1f * m_DeltaTime);
            m_CurObj->Translate(&vector, m_CamOrientRef);
        }
        if (ModSupport::Keyboard(m_Context, BML_INPUT_RAW)
                .IsKeyDown(m_MoveKeys[2].Get<uint32_t>())) {
            vector = VxVector(-0.1f * m_DeltaTime, 0, 0);
            m_CurObj->Translate(&vector, m_CamOrientRef);
        }
        if (ModSupport::Keyboard(m_Context, BML_INPUT_RAW)
                .IsKeyDown(m_MoveKeys[3].Get<uint32_t>())) {
            vector = VxVector(0.1f * m_DeltaTime, 0, 0);
            m_CurObj->Translate(&vector, m_CamOrientRef);
        }
        if (ModSupport::Keyboard(m_Context, BML_INPUT_RAW)
                .IsKeyDown(m_MoveKeys[4].Get<uint32_t>())) {
            vector = VxVector(0, 0.1f * m_DeltaTime, 0);
            m_CurObj->Translate(&vector, m_CamOrientRef);
        }
        if (ModSupport::Keyboard(m_Context, BML_INPUT_RAW)
                .IsKeyDown(m_MoveKeys[5].Get<uint32_t>())) {
            vector = VxVector(0, -0.1f * m_DeltaTime, 0);
            m_CurObj->Translate(&vector, m_CamOrientRef);
        }
    } else {
        CKMesh *mesh = m_CurObj->GetMesh(0);
        switch (m_CurSel) {
        case 0:
            ModSupport::PhysicalizeConvex(m_Behaviors, m_Context, m_CurObj, false, 0.5f, 0.4f, 0.2f,
                                          "", false, true, false, 1.5f, 0.1f, mesh->GetName(),
                                          VxVector(0, 0, 0), mesh);
            break;
        case 1:
            ModSupport::PhysicalizeBall(m_Behaviors, m_Context, m_CurObj, false, 0.6f, 0.2f, 2.0f,
                                        "", false, true, false, 0.6f, 0.1f, mesh->GetName());
            break;
        case 2:
            ModSupport::PhysicalizeBall(m_Behaviors, m_Context, m_CurObj, false, 0.7f, 0.1f, 10.0f,
                                        "", false, true, false, 0.2f, 0.1f, mesh->GetName());
            break;
        default:
            ModSupport::PhysicalizeConvex(m_Behaviors, m_Context, m_CurObj, false, 0.7f, 0.3f, 1.0f,
                                          "", false, true, false, 0.1f, 0.1f, mesh->GetName(),
                                          VxVector(0, 0, 0), mesh);
            break;
        }

        CKDataArray *ph = ModSupport::Find<CKDataArray>(m_Context, "PH");
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

        CKGroup *depth = ModSupport::Find<CKGroup>(m_Context, "DepthTest");
        depth->AddObject(m_CurObj);
        m_TempBalls.emplace_back(index, m_CurObj);

        m_CurSel = -1;
        m_CurObj = nullptr;
        ModSupport::Check(m_SummonBlock.Release());

        ModSupport::Log(m_Context, BML_LOG_INFO, "Summoned a %s",
                        m_CurSel < 2    ? m_CurSel == 0 ? "Paper Ball" : "Wood Ball"
                        : m_CurSel == 2 ? "Stone Ball"
                                        : "Box");
    }
}

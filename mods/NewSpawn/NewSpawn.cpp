#include "NewSpawn.h"

IMod *BMLEntry(IBML *bml) {
    return new NewSpawn(bml);
}

void BMLExit(IMod *mod) {
    delete mod;
}

void NewSpawn::OnLoad() {
    cmdspawnp_ptr = new CommandNewSpawn(this);
    m_BML->RegisterCommand(cmdspawnp_ptr);
    GetConfig()->SetCategoryComment("Main", "Main settings.");

    prop_enabled = GetConfig()->GetProperty("Main", "Enabled");
    prop_enabled->SetDefaultBoolean(true);
    prop_enabled->SetComment("Enabled to use this mod");
    enabled = prop_enabled->GetBoolean();

    reset_prop_enabled = GetConfig()->GetProperty("Main", "Prop_Reset");
    reset_prop_enabled->SetDefaultBoolean(false);
    reset_prop_enabled->SetComment("Enable to reset prop when press Transport shortcut key.");
    reset_enabled = reset_prop_enabled->GetBoolean();

    reset_spirit_enabled = GetConfig()->GetProperty("Main", "Spirit_Record");
    reset_spirit_enabled->SetDefaultBoolean(true);
    reset_spirit_enabled->SetComment("Enable to record and play spirit ball when press Record shortcut key.");
    spirit_enabled = reset_spirit_enabled->GetBoolean();

    prop_key = GetConfig()->GetProperty("Main", "Transport");
    prop_key->SetDefaultKey(CKKEY_C);
    prop_key->SetComment("Set your shortcut key for transporting your ball");
    key_prop = prop_key->GetKey();

    prop_key_record = GetConfig()->GetProperty("Main", "Record");
    prop_key_record->SetDefaultKey(CKKEY_V);
    prop_key_record->SetComment("Set your shortcut key for saving the spirit record");
    key_record = prop_key_record->GetKey();

    input_manager = m_BML->GetInputManager();
}

void NewSpawn::OnModifyConfig(const char *category, const char *key, IProperty *prop) {
    enabled = prop_enabled->GetBoolean();
    reset_enabled = reset_prop_enabled->GetBoolean();
    spirit_enabled = reset_spirit_enabled->GetBoolean();
    if (prop == prop_key) {
        key_prop = prop->GetKey();
    }
    if (prop == prop_key_record) {
        key_record = prop->GetKey();
    }
}

void NewSpawn::OnLoadScript(const char *filename, CKBehavior *script) {
    if (strcmp(script->GetName(), "Gameplay_Ingame") == 0) {
        CKBehavior *ballMgr = ScriptHelper::FindFirstBB(script, "BallManager");
        CKBehavior *newBall = ScriptHelper::FindFirstBB(ballMgr, "New Ball");
        m_dynamicPos = ScriptHelper::FindNextBB(script, ballMgr, "TT Set Dynamic Position");
        m_phyNewBall = ScriptHelper::FindFirstBB(newBall, "physicalize new Ball");
    }
}

void NewSpawn::OnBallNavActive() {
    Ball_Active = true;
}

void NewSpawn::OnBallNavInactive() {
    Ball_Active = false;
}

void NewSpawn::OnPostStartMenu() {
    if (init)
        return;

    if (!m_curLevel) {
        m_curLevel = m_BML->GetArrayByName("CurrentLevel");
        m_checkpoints = m_BML->GetArrayByName("Checkpoints");
        m_resetpoints = m_BML->GetArrayByName("ResetPoints");
        m_ingameParam = m_BML->GetArrayByName("IngameParameter");
        CKBehavior *events = m_BML->GetScriptByName("Gameplay_Events");
        CKBehavior *id = ScriptHelper::FindNextBB(events, events->GetInput(0));
        m_curSector = id->GetOutputParameter(0)->GetDestination(0);
    }

    auto *script = m_BML->GetScriptByName("Gameplay_Ingame");
    CKBehavior *trafoMgr = ScriptHelper::FindFirstBB(script, "Trafo Manager");
    m_setNewBall = ScriptHelper::FindFirstBB(trafoMgr, "set new Ball");
    CKBehavior *sop = ScriptHelper::FindFirstBB(m_setNewBall, "Switch On Parameter");
    m_curTrafo = sop->GetInputParameter(0)->GetDirectSource();

    init = true;
}

void NewSpawn::OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                            CKBOOL addtoscene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                            XObjectArray *objArray, CKObject *masterObj) {
    if (!strcmp(filename, "3D Entities\\Balls.nmo")) {
        CKDataArray *physBall = m_BML->GetArrayByName("Physicalize_GameBall");
        for (int i = 0; i < physBall->GetRowCount(); i++) {
            SpiritBall ball;
            ball.name.resize(physBall->GetElementStringValue(i, 0, nullptr), '\0');
            physBall->GetElementStringValue(i, 0, &ball.name[0]);
            ball.name.pop_back();
            ball.obj = m_BML->Get3dObjectByName(ball.name.c_str());

            CKDependencies dep;
            dep.Resize(40);
            dep.Fill(0);
            dep.m_Flags = CK_DEPENDENCIES_CUSTOM;
            dep[CKCID_OBJECT] = CK_DEPENDENCIES_COPY_OBJECT_NAME | CK_DEPENDENCIES_COPY_OBJECT_UNIQUENAME;
            dep[CKCID_MESH] = CK_DEPENDENCIES_COPY_MESH_MATERIAL;
            dep[CKCID_3DENTITY] = CK_DEPENDENCIES_COPY_3DENTITY_MESH;
            ball.obj = static_cast<CK3dObject *>(m_BML->GetCKContext()->CopyObject(ball.obj, &dep, "_Spirit"));
            for (int j = 0; j < ball.obj->GetMeshCount(); j++) {
                CKMesh *mesh = ball.obj->GetMesh(j);
                for (int k = 0; k < mesh->GetMaterialCount(); k++) {
                    CKMaterial *mat = mesh->GetMaterial(k);
                    mat->EnableAlphaBlend();
                    mat->SetSourceBlend(VXBLEND_SRCALPHA);
                    mat->SetDestBlend(VXBLEND_INVSRCALPHA);
                    VxColor color = mat->GetDiffuse();
                    color.a = 0.5f;
                    mat->SetDiffuse(color);
                    ball.materials.push_back(mat);
                    m_BML->SetIC(mat);
                }
            }
            m_dualBalls.push_back(ball);
        }

        GetLogger()->Info("Created Newspawn Spirit Balls");
    }

    if (!isMap || filename[0] == '\0')
        return;

    set_map_filename_curlevel(filename);
}

int NewSpawn::GetCurrentBall() {
    //get and return the balltype number
    CKObject *ball = m_curLevel->GetElementObject(0, 1);
    if (ball) {
        const std::string ballName = ball->GetName();
        const int count = static_cast<int>(m_dualBalls.size());
        for (int i = 0; i < count; i++) {
            if (m_dualBalls[i].name == ballName)
                return i;
        }
    }
    return 0;
}

void NewSpawn::SetCurrentBall(int curBall) {
    m_playBall = curBall;
    for (auto &ball: m_dualBalls)
        ball.obj->Show(CKHIDE);
}

void NewSpawn::StopRecording(bool save_data) {
    m_isRecording = false;
    if (save_data) {
        m_play = m_record;
        record_exist = true;
    } else {
        m_record.trafo.clear();
        m_record.trafo.shrink_to_fit();
        m_record.states.clear();
        m_record.states.shrink_to_fit();
    }
}

void NewSpawn::StopPlaying() {
    m_isPlaying = false;

    for (auto &ball: m_dualBalls)
        ball.obj->Show(CKHIDE);
}

void NewSpawn::StartRecording() {
    m_isRecording = true;
    m_recordTimer = 0;
    m_srtimer = 0;
    m_curBall = GetCurrentBall();
    m_record.trafo.emplace_back(0, m_curBall);
}

void NewSpawn::StartPlaying() {
    if (record_exist) {
        m_isPlaying = true;
        m_playTimer = -1000.0f / TICK_SPEED;
        m_playFrame = 0;
        m_playTrafo = 1;
        SetCurrentBall(m_play.trafo[0].second);
    }
}

void NewSpawn::OnProcess() {
    if (!(enabled && m_BML->IsCheatEnabled() && m_BML->IsPlaying()))
        return;
    if (input_manager->IsKeyPressed(key_prop) && Ball_Active) {
        //[transport] no nsp or no set level: return
        if (!(cmdspawnp_ptr->get_is_spawned())) {
            m_BML->SendIngameMessage("You have not set up a nsp point.");
            return;
        }
        if (cmdspawnp_ptr->get_map_filename_setlevel() != get_map_filename_curlevel()) {
            m_BML->SendIngameMessage("the nsp point you set is not in this level.");
            return;
        }

        //[spirit] Make sure you reset the ball before saving the replay
        record_save_enable = true;

        //[spirit] Press shortcut key, End last playing and recording
        StopRecording();
        StopPlaying();

        //[transport] Get the sector at the time of /nsp
        int set_sector = cmdspawnp_ptr->get_curSector();
        //[transport] Get the current sector
        CKBehavior *events = m_BML->GetScriptByName("Gameplay_Events");
        int cur_sector = ScriptHelper::GetParamValue<int>(
            ScriptHelper::FindNextBB(events, events->GetInput(0))->GetOutputParameter(0)->GetDestination(0));

        //[transport] Switch sector if (set_sector != cur_sector)
        CKContext *ctx = m_BML->GetCKContext();
        if (m_curLevel) {
            if (set_sector != cur_sector) {
                VxMatrix matrix;
                m_resetpoints->GetElementValue(set_sector - 1, 0, &matrix);
                m_curLevel->SetElementValue(0, 3, &matrix);

                m_ingameParam->SetElementValue(0, 1, &set_sector);
                m_ingameParam->SetElementValue(0, 2, &cur_sector);
                ScriptHelper::SetParamValue(m_curSector, set_sector);

                //m_BML->SendIngameMessage(("Changed to Sector " + std::to_string(set_sector)).c_str());

                CKBehavior *sectorMgr = m_BML->GetScriptByName("Gameplay_SectorManager");
                ctx->GetCurrentScene()->Activate(sectorMgr, true);

                m_BML->AddTimerLoop(1ul, [this, set_sector, sectorMgr, ctx]() {
                    if (sectorMgr->IsActive())
                        return true;

                    m_BML->AddTimer(2ul, [this, set_sector, ctx]() {
                        CKBOOL active = false;
                        m_curLevel->SetElementValue(0, 4, &active);

                        CK_ID flameId;
                        m_checkpoints->GetElementValue(set_sector % 2, 1, &flameId);
                        auto *flame = static_cast<CK3dEntity *>(ctx->GetObject(flameId));
                        ctx->GetCurrentScene()->Activate(flame->GetScript(0), true);

                        m_checkpoints->GetElementValue(set_sector - 1, 1, &flameId);
                        flame = static_cast<CK3dEntity *>(ctx->GetObject(flameId));
                        ctx->GetCurrentScene()->Activate(flame->GetScript(0), true);

                        if (set_sector > m_checkpoints->GetRowCount()) {
                            CKMessageManager *mm = m_BML->GetMessageManager();
                            CKMessageType msg = mm->AddMessageType("last Checkpoint reached");
                            mm->SendMessageSingle(msg, m_BML->GetGroupByName("All_Sound"));
                        } else {
                            m_BML->AddTimer(2ul, [this, set_sector, ctx, flame]() {
                                VxMatrix matrix;
                                m_checkpoints->GetElementValue(set_sector - 1, 0, &matrix);
                                flame->SetWorldMatrix(matrix);
                                CKBOOL active = true;
                                m_curLevel->SetElementValue(0, 4, &active);
                                ctx->GetCurrentScene()->Activate(flame->GetScript(0), true);
                                m_BML->Show(flame, CKSHOW, true);
                            });
                        }
                    });

                    return false;
                });
            }
        }

        //[transport] Reset camera
        CKDataArray *ph = m_BML->GetArrayByName("PH");
        for (int i = 0; i < ph->GetRowCount(); i++) {
            CKBOOL set = true;
            char name[100];
            ph->GetElementStringValue(i, 1, name);
            if (!strcmp(name, "P_Extra_Point"))
                ph->SetElementValue(i, 4, &set);
        }

        //[transport] Reset props
        if (reset_enabled) {
            m_ingameParam->SetElementValueFromParameter(0, 1, m_curSector);
            m_ingameParam->SetElementValueFromParameter(0, 2, m_curSector);
            CKBehavior *sectorMgr = m_BML->GetScriptByName("Gameplay_SectorManager");
            ctx->GetCurrentScene()->Activate(sectorMgr, true);
        }

        //[transport] Ball position transport
        CKMessageManager *mm = m_BML->GetMessageManager();
        CKMessageType ballDeact = mm->AddMessageType("BallNav deactivate");
        mm->SendMessageSingle(ballDeact, m_BML->GetGroupByName("All_Gameplay"));
        mm->SendMessageSingle(ballDeact, m_BML->GetGroupByName("All_Sound"));
        m_dynamicPos->ActivateInput(1);
        m_dynamicPos->Activate();
        auto current_ball = get_curBall();

        m_BML->AddTimer(2ul, [this, current_ball, mm, ballDeact]() {
            ExecuteBB::Unphysicalize(current_ball);

            auto matrix = cmdspawnp_ptr->get_matrix();

            current_ball->SetWorldMatrix(matrix);
            CK3dEntity *camMF = m_BML->Get3dEntityByName("Cam_MF");
            m_BML->RestoreIC(camMF, true);
            camMF->SetWorldMatrix(matrix);

            m_dynamicPos->ActivateInput(0);
            m_dynamicPos->Activate();
            m_phyNewBall->ActivateInput(0);
            m_phyNewBall->Activate();
            m_phyNewBall->GetParent()->Activate();
            mm->SendMessageSingle(ballDeact, m_BML->GetGroupByName("All_Gameplay"));
            mm->SendMessageSingle(ballDeact, m_BML->GetGroupByName("All_Sound"));

            //[transport] Ball type reset
            CK3dEntity *curBall = get_curBall();
            ExecuteBB::Unphysicalize(curBall);
            static char trafoTypes[3][6] = {"paper", "wood", "stone"};
            SetParamString(m_curTrafo, trafoTypes[cmdspawnp_ptr->get_ball_type()]);
            m_setNewBall->ActivateInput(0);
            m_setNewBall->Activate();
            GetLogger()->Info("NewSpawn Reset Ball");
        });

        //[spirit] Transport end, start recording and playing if enable
        if (spirit_enabled) {
            StartRecording();
            StartPlaying();
        }
    }

    if (!spirit_enabled)
        return;

    constexpr float delta = 1000.0f / TICK_SPEED;

    //[spirit] Recording
    if (m_isRecording) {
        m_srtimer += m_BML->GetTimeManager()->GetLastDeltaTime();

        m_recordTimer += m_BML->GetTimeManager()->GetLastDeltaTime();
        m_recordTimer = (std::min)(m_recordTimer, 1000.0f);

        while (m_recordTimer > 0) {
            m_recordTimer -= delta;

            int curBall = GetCurrentBall();
            if (curBall != m_curBall)
                m_record.trafo.emplace_back(m_record.states.size(), curBall);

            m_curBall = curBall;

            auto *ball = static_cast<CK3dObject *>(m_curLevel->GetElementObject(0, 1));
            if (ball) {
                Record::State state;
                ball->GetPosition(&state.pos);
                ball->GetQuaternion(&state.rot);
                m_record.states.push_back(state);
            }
        }
        if (m_srtimer > 1000 * 1800) {
            GetLogger()->Info("NewSpawn Record is longer than half hour, stop recording");
            StopPlaying();
            StopRecording(true);
        }
    }

    //[spirit] Playing
    if (m_isPlaying) {
        m_playTimer += m_BML->GetTimeManager()->GetLastDeltaTime();
        m_playTimer = (std::min)(m_playTimer, 1000.0f);

        while (m_playTimer > 0) {
            m_playTimer -= delta;
            m_playFrame++;

            auto &trafos = m_play.trafo;
            if (m_playTrafo < trafos.size()) {
                auto &trafo = trafos[m_playTrafo];
                if (m_playFrame == trafo.first) {
                    SetCurrentBall(trafo.second);
                    m_playTrafo++;
                }
            }
        }

        auto &states = m_play.states;
        if (m_playFrame < states.size() - 1) {
            if (m_playFrame >= 1) {
                if (m_playBall >= 0 && m_playBall < m_dualBalls.size()) {
                    CKObject *playerBall = m_curLevel->GetElementObject(0, 1);
                    CK3dObject *ball = m_dualBalls[m_playBall].obj;
                    ball->Show(playerBall->IsVisible() ? CKSHOW : CKHIDE);

                    const float portion = (m_playTimer / delta + 1);
                    const Record::State &cur = states[m_playFrame];
                    Record::State &next = states[m_playFrame + 1];
                    const VxVector position = (next.pos - cur.pos) * portion + cur.pos;
                    const VxQuaternion rotation = Slerp(portion, cur.rot, next.rot);
                    ball->SetPosition(&position);
                    ball->SetQuaternion(&rotation);
                }
            }
        } else StopPlaying();
    }

    //[spirit] Recording saving
    if (input_manager->IsKeyPressed(key_record)) {
        if (record_save_enable) {
            StopPlaying();
            StopRecording(true);
            m_BML->SendIngameMessage("record is updated.");
        } else {
            m_BML->SendIngameMessage("Please use transport shortcut key to reset the ball position first.");
        }
    }
}

void CommandNewSpawn::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (bml->IsIngame()) {
        //get spawn position
        CK3dEntity *camRef = bml->Get3dEntityByName("Cam_OrientRef");
        matrix = camRef->GetWorldMatrix();
        for (int i = 0; i < 4; i++) {
            std::swap(matrix[0][i], matrix[2][i]);
            matrix[0][i] = -matrix[0][i];
        }
        //get spawn sector
        CKBehavior *events = bml->GetScriptByName("Gameplay_Events");
        set_curSector(ScriptHelper::GetParamValue<int>(
            ScriptHelper::FindNextBB(events, events->GetInput(0))->GetOutputParameter(0)->GetDestination(0)));
        //get spawn ball type
        ball_type = ball_name_to_id[mod->get_curBall()->GetName()];

        bml->SendIngameMessage(("Set NSpawn Point to (sector "
                                + std::to_string(get_curSector()) + ", position "
                                + std::to_string(matrix[3][0]) + ", "
                                + std::to_string(matrix[3][1]) + ", "
                                + std::to_string(matrix[3][2]) + ")").c_str());
        //get spawn level and is spawned
        ///
        set_map_filename_setlevel(mod->get_map_filename_curlevel());
        set_is_spawned(true);
    }
}

const std::vector<std::string> CommandNewSpawn::GetTabCompletion(IBML *bml, const std::vector<std::string> &args) {
    return {};
}

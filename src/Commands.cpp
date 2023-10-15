#include "Commands.h"

#include "Version.h"
#include "ModLoader.h"
#include "BMLMod.h"
#include "ScriptHelper.h"

void CommandBML::Execute(IBML *bml, const std::vector<std::string> &args) {
    auto &loader = ModLoader::GetInstance();
    bml->SendIngameMessage("Ballance Mod Loader " BML_VERSION);
    bml->SendIngameMessage((std::to_string(loader.GetModCount()) + " Mods Installed:").data());

    int count = loader.GetModCount();
    for (int i = 0; i < count; ++i) {
        auto *mod = loader.GetMod(i);
        std::string str = std::string("  ") + mod->GetID() + ": " + mod->GetName() + " " + mod->GetVersion() + " by " + mod->GetAuthor();
        bml->SendIngameMessage(str.data());
    }
}

void CommandHelp::Execute(IBML *bml, const std::vector<std::string> &args) {
    auto &loader = ModLoader::GetInstance();
    const int cmdCount = loader.GetCommandCount();
    bml->SendIngameMessage((std::to_string(cmdCount) + " Existing Commands:").data());
    for (int i = 0; i < cmdCount; i++) {
        ICommand *cmd = loader.GetCommand(i);
        std::string str = std::string("  /") + cmd->GetName();
        if (!cmd->GetAlias().empty())
            str += "(" + cmd->GetAlias() + ")";
        if (cmd->IsCheat())
            str += "[Cheat]";
        str += ": " + cmd->GetDescription();
        bml->SendIngameMessage(str.data());
    }
}

void CommandCheat::Execute(IBML *bml, const std::vector<std::string> &args) {
    auto &loader = ModLoader::GetInstance();
    if (args.size() == 1) {
        loader.EnableCheat(!loader.IsCheatEnabled());
    } else {
        loader.EnableCheat(ParseBoolean(args[1]));
    }
    bml->SendIngameMessage(loader.IsCheatEnabled() ? "Cheat Mode On" : "Cheat Mode Off");
}

void CommandClear::Execute(IBML *bml, const std::vector<std::string> &args) {
    BMLMod *bmlmod = ModLoader::GetInstance().GetBMLMod();
    bmlmod->m_MsgCount = 0;
    for (int i = 0; i < MSG_MAXSIZE; i++) {
        bmlmod->m_Msgs[i].m_Background->SetVisible(false);
        bmlmod->m_Msgs[i].m_Text->SetVisible(false);
        bmlmod->m_Msgs[i].m_Text->SetText("");
        bmlmod->m_Msgs[i].m_Timer = 0;
    }
}

void CommandScore::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (bml->IsIngame() && args.size() > 2) {
        int num = ParseInteger(args[2], 0);
        if (!m_Energy) {
            m_Energy = bml->GetArrayByName("Energy");
        }

        if (m_Energy) {
            int score;
            m_Energy->GetElementValue(0, 0, &score);
            if (args[1] == "add")
                score += num;
            else if (args[1] == "sub")
                score = (std::max)(0, score - num);
            else if (args[1] == "set")
                score = num;
            m_Energy->SetElementValue(0, 0, &score);
            bml->SendIngameMessage(("Ingame Score Changed to " + std::to_string(score)).c_str());
        }
    }
}

void CommandKill::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (!m_DeactivateBall) {
        CKBehavior *ingame = bml->GetScriptByName("Gameplay_Ingame");
        CKBehavior *ballMgr = ScriptHelper::FindFirstBB(ingame, "BallManager");
        m_DeactivateBall = ScriptHelper::FindFirstBB(ballMgr, "Deactivate Ball");
    }

    if (bml->IsPlaying() && m_DeactivateBall) {
        m_DeactivateBall->ActivateInput(0);
        m_DeactivateBall->Activate();
        bml->SendIngameMessage("Killed Ball");
    }
}

void CommandSetSpawn::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (!m_CurLevel) {
        m_CurLevel = bml->GetArrayByName("CurrentLevel");
    }

    if (bml->IsIngame() && m_CurLevel) {
        CK3dEntity *camRef = bml->Get3dEntityByName("Cam_OrientRef");
        VxMatrix mat = camRef->GetWorldMatrix();
        for (int i = 0; i < 3; i++) {
            std::swap(mat[0][i], mat[2][i]);
            mat[0][i] = -mat[0][i];
        }
        m_CurLevel->SetElementValue(0, 3, &mat);
        bml->SendIngameMessage(
            ("Set Spawn Point to (" +
             std::to_string(mat[3][0]) + ", " +
             std::to_string(mat[3][1]) + ", " +
             std::to_string(mat[3][2]) + ")").c_str());
    }
}

void CommandSector::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (bml->IsPlaying() && args.size() > 1) {
        CKContext *ctx = bml->GetCKContext();
        if (!m_CurLevel) {
            m_CurLevel = bml->GetArrayByName("CurrentLevel");
            m_Checkpoints = bml->GetArrayByName("Checkpoints");
            m_ResetPoints = bml->GetArrayByName("ResetPoints");
            m_IngameParam = bml->GetArrayByName("IngameParameter");
            CKBehavior *events = bml->GetScriptByName("Gameplay_Events");
            CKBehavior *id = ScriptHelper::FindNextBB(events, events->GetInput(0));
            m_CurSector = id->GetOutputParameter(0)->GetDestination(0);
        }

        if (m_CurLevel) {
            int curSector = ScriptHelper::GetParamValue<int>(m_CurSector);
            int sector = ParseInteger(args[1], 1, m_Checkpoints->GetRowCount() + 1);
            if (curSector != sector) {
                VxMatrix matrix;
                m_ResetPoints->GetElementValue(sector - 1, 0, &matrix);
                m_CurLevel->SetElementValue(0, 3, &matrix);

                m_IngameParam->SetElementValue(0, 1, &sector);
                m_IngameParam->SetElementValue(0, 2, &curSector);
                ScriptHelper::SetParamValue(m_CurSector, sector);

                bml->SendIngameMessage(("Changed to Sector " + std::to_string(sector)).c_str());

                CKBehavior *sectorMgr = bml->GetScriptByName("Gameplay_SectorManager");
                ctx->GetCurrentScene()->Activate(sectorMgr, true);

                bml->AddTimerLoop(1ul, [this, bml, sector, sectorMgr, ctx]() {
                    if (sectorMgr->IsActive())
                        return true;

                    bml->AddTimer(2ul, [this, bml, sector, ctx]() {
                        CKBOOL active = false;
                        m_CurLevel->SetElementValue(0, 4, &active);

                        CK_ID flameId;
                        m_Checkpoints->GetElementValue(sector % 2, 1, &flameId);
                        auto *flame = (CK3dEntity *) ctx->GetObject(flameId);
                        ctx->GetCurrentScene()->Activate(flame->GetScript(0), true);

                        m_Checkpoints->GetElementValue(sector - 1, 1, &flameId);
                        flame = (CK3dEntity *) ctx->GetObject(flameId);
                        ctx->GetCurrentScene()->Activate(flame->GetScript(0), true);

                        if (sector > m_Checkpoints->GetRowCount()) {
                            CKMessageManager *mm = bml->GetMessageManager();
                            CKMessageType msg = mm->AddMessageType("last Checkpoint reached");
                            mm->SendMessageSingle(msg, bml->GetGroupByName("All_Sound"));

                            ResetBall(bml, ctx);
                        } else {
                            bml->AddTimer(2ul, [this, bml, sector, ctx, flame]() {
                                VxMatrix matrix;
                                m_Checkpoints->GetElementValue(sector - 1, 0, &matrix);
                                flame->SetWorldMatrix(matrix);
                                CKBOOL active = true;
                                m_CurLevel->SetElementValue(0, 4, &active);
                                ctx->GetCurrentScene()->Activate(flame->GetScript(0), true);
                                bml->Show(flame, CKSHOW, true);

                                ResetBall(bml, ctx);
                            });
                        }
                    });
                    return false;
                });
            }
        }
    }
}

void CommandSector::ResetBall(IBML *bml, CKContext *ctx) {
    CKMessageManager *mm = bml->GetMessageManager();
    CKMessageType ballDeactivate = mm->AddMessageType("BallNav deactivate");

    mm->SendMessageSingle(ballDeactivate, bml->GetGroupByName("All_Gameplay"));
    mm->SendMessageSingle(ballDeactivate, bml->GetGroupByName("All_Sound"));

    bml->AddTimer(2ul, [this, bml]() {
        auto *curBall = (CK3dEntity *) m_CurLevel->GetElementObject(0, 1);
        if (curBall) {
            ExecuteBB::Unphysicalize(curBall);

            auto *bmlMod = ModLoader::GetInstance().GetBMLMod();
            bmlMod->m_DynamicPos->ActivateInput(1);
            bmlMod->m_DynamicPos->Activate();

            bml->AddTimer(1ul, [this, bml, curBall]() {
                VxMatrix matrix;
                m_CurLevel->GetElementValue(0, 3, &matrix);
                curBall->SetWorldMatrix(matrix);

                CK3dEntity *camMF = bml->Get3dEntityByName("Cam_MF");
                bml->RestoreIC(camMF, true);
                camMF->SetWorldMatrix(matrix);

                bml->AddTimer(1ul, []() {
                    auto *bmlMod = ModLoader::GetInstance().GetBMLMod();
                    bmlMod->m_DynamicPos->ActivateInput(0);
                    bmlMod->m_DynamicPos->Activate();
                    bmlMod->m_PhysicsNewBall->ActivateInput(0);
                    bmlMod->m_PhysicsNewBall->Activate();
                    bmlMod->m_PhysicsNewBall->GetParent()->Activate();
                });
            });
        }
    });
}

void CommandWin::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (bml->IsPlaying()) {
        CKMessageManager *mm = bml->GetMessageManager();
        CKMessageType levelWin = mm->AddMessageType("Level_Finish");

        mm->SendMessageSingle(levelWin, bml->GetGroupByName("All_Gameplay"));
        bml->SendIngameMessage("Level Finished");
    }
}

void CommandSpeed::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (args.size() > 1) {
        float times = ParseFloat(args[1], 0, 1000);
        m_BMLMod->ChangeBallSpeed(times);
    }
}

void CommandTravel::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (bml->IsPlaying()) {
        if (m_BMLMod->IsInTravelCam()) {
            m_BMLMod->ExitTravelCam();
            m_BMLMod->AddIngameMessage("Exit Travel Camera");
            bml->GetGroupByName("HUD_sprites")->Show();
            bml->GetGroupByName("LifeBalls")->Show();
        } else {
            m_BMLMod->EnterTravelCam();
            m_BMLMod->AddIngameMessage("Enter Travel Camera");
            bml->GetGroupByName("HUD_sprites")->Show(CKHIDE);
            bml->GetGroupByName("LifeBalls")->Show(CKHIDE);
        }
    }
}
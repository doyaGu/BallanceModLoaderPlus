#include "DebugCommands.h"
#include "DebugUtilities.h"

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

    if (m_DeactivateBall && m_Mod->IsInLevel()) {
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
    if (args.size() == 2) {
        int sector = ParseInteger(args[1], 1, m_Mod->GetSectorCount() + 1);
        m_Mod->SetSector(sector);
    }
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
        m_Mod->ChangeBallSpeed(times);
    }
}
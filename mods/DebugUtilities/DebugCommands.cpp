#include "DebugCommands.h"
#include "DebugUtilities.h"

void CommandScore::Execute(const BML::Mod::Context &context, const std::vector<std::string> &args) {
    if (ModSupport::GameState().Ingame && args.size() > 2) {
        int num = ModSupport::ParseInteger(args[2], 0);
        if (!m_Energy) {
            m_Energy = ModSupport::Find<CKDataArray>(context, "Energy");
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
            BML::UI::AddMessage(("Ingame Score Changed to " + std::to_string(score)).c_str());
        }
    }
}

void CommandKill::Execute(const BML::Mod::Context &context, const std::vector<std::string> &args) {
    if (!m_DeactivateBall) {
        CKBehavior *ingame = ModSupport::Find<CKBehavior>(context, "Gameplay_Ingame");
        CKBehavior *ballMgr = ScriptHelper::FindFirstBB(ingame, "BallManager");
        m_DeactivateBall = ScriptHelper::FindFirstBB(ballMgr, "Deactivate Ball");
    }

    if (m_DeactivateBall && m_Mod->IsInLevel()) {
        m_DeactivateBall->ActivateInput(0);
        m_DeactivateBall->Activate();
        BML::UI::AddMessage("Killed Ball");
    }
}

void CommandSetSpawn::Execute(const BML::Mod::Context &context,
                              const std::vector<std::string> &args) {
    if (!m_CurLevel) {
        m_CurLevel = ModSupport::Find<CKDataArray>(context, "CurrentLevel");
    }

    if (ModSupport::GameState().Ingame && m_CurLevel) {
        CK3dEntity *camRef = ModSupport::Find<CK3dEntity>(context, "Cam_OrientRef");
        VxMatrix mat = camRef->GetWorldMatrix();
        for (int i = 0; i < 3; i++) {
            std::swap(mat[0][i], mat[2][i]);
            mat[0][i] = -mat[0][i];
        }
        m_CurLevel->SetElementValue(0, 3, &mat);
        BML::UI::AddMessage(("Set Spawn Point to (" + std::to_string(mat[3][0]) + ", " +
                             std::to_string(mat[3][1]) + ", " + std::to_string(mat[3][2]) + ")")
                                .c_str());
    }
}

void CommandSector::Execute(const BML::Mod::Context &context,
                            const std::vector<std::string> &args) {
    if (args.size() == 2) {
        int sector = ModSupport::ParseInteger(args[1], 1, m_Mod->GetSectorCount() + 1);
        m_Mod->SetSector(sector);
    }
}

void CommandWin::Execute(const BML::Mod::Context &context, const std::vector<std::string> &args) {
    if (ModSupport::GameState().Playing) {
        CKMessageManager *mm = ModSupport::Engine(context)->GetMessageManager();
        CKMessageType levelWin = mm->AddMessageType("Level_Finish");

        mm->SendMessageSingle(levelWin, ModSupport::Find<CKGroup>(context, "All_Gameplay"));
        BML::UI::AddMessage("Level Finished");
    }
}

void CommandSpeed::Execute(const BML::Mod::Context &context, const std::vector<std::string> &args) {
    if (args.size() > 1) {
        float times = ModSupport::ParseFloat(args[1], 0, 1000);
        m_Mod->ChangeBallSpeed(times);
    }
}
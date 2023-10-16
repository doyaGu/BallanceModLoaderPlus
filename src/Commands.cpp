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
    m_BMLMod->ClearIngameMessages();
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
    if (args.size() == 2) {
        int sector = ParseInteger(args[1], 1, m_BMLMod->GetSectorCount() + 1);
        m_BMLMod->SetSector(sector);
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
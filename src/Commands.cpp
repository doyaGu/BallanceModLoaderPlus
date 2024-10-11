#include "Commands.h"

#include "BML/IBML.h"
#include "BMLMod.h"

void CommandBML::Execute(IBML *bml, const std::vector<std::string> &args) {
    bml->SendIngameMessage("Ballance Mod Loader Plus " BML_VERSION);
    bml->SendIngameMessage((std::to_string(bml->GetModCount()) + " Mods Installed:").data());

    int count = bml->GetModCount();
    for (int i = 0; i < count; ++i) {
        auto *mod = bml->GetMod(i);
        std::string str = std::string("  ") + mod->GetID() + ": " + mod->GetName() + " " + mod->GetVersion() + " by " + mod->GetAuthor();
        bml->SendIngameMessage(str.data());
    }
}

void CommandHelp::Execute(IBML *bml, const std::vector<std::string> &args) {
    const int cmdCount = bml->GetCommandCount();
    bml->SendIngameMessage((std::to_string(cmdCount) + " Existing Commands:").data());
    for (int i = 0; i < cmdCount; i++) {
        ICommand *cmd = bml->GetCommand(i);
        std::string str = std::string("\t") + cmd->GetName();
        if (!cmd->GetAlias().empty())
            str += "(" + cmd->GetAlias() + ")";
        if (cmd->IsCheat())
            str += "[Cheat]";
        str += ": " + cmd->GetDescription();
        bml->SendIngameMessage(str.data());
    }
}

void CommandCheat::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (args.size() == 1) {
        bml->EnableCheat(!bml->IsCheatEnabled());
    } else {
        bml->EnableCheat(ParseBoolean(args[1]));
    }
    bml->SendIngameMessage(bml->IsCheatEnabled() ? "Cheat Mode On" : "Cheat Mode Off");
}

void CommandEcho::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (args.size() > 1) {
        std::string str;
        for (size_t i = 1; i < args.size(); ++i)
            str.append(args[i]).append(" ");
        str.pop_back();
        bml->SendIngameMessage(str.c_str());
    }
}

void CommandClear::Execute(IBML *bml, const std::vector<std::string> &args) {
    m_BMLMod->ClearIngameMessages();
}

void CommandHistory::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (args.size() == 1) {
        m_BMLMod->PrintHistory();
    } else if (args.size() == 2) {
        if (args[1] == "clear") {
            m_BMLMod->ClearHistory();
        } else {
            int i = ParseInteger(args[1]);
            if (i != 0)
                m_BMLMod->ExecuteHistory(i + 1);
        }
    }
}

void CommandExit::Execute(IBML *bml, const std::vector<std::string> &args) {
    bml->ExitGame();
}

CommandHUD::CommandHUD(BMLMod *mod) : m_BMLMod(mod) {
    m_State = m_BMLMod->GetHUD();
}

void CommandHUD::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (args.size() == 2) {
        if (ParseBoolean(args[1])) {
            m_BMLMod->SetHUD(m_State);
        } else {
            m_State = m_BMLMod->GetHUD();
            m_BMLMod->SetHUD(0);
        }
    } else if (args.size() == 3) {
        int state = m_BMLMod->GetHUD();
        if (args[1] == "title") {
            if (ParseBoolean(args[2])) {
                state |= HUD_TITLE;
            } else {
                state &= ~HUD_TITLE;
            }
        } else if (args[1] == "fps") {
            if (ParseBoolean(args[2])) {
                state |= HUD_FPS;
            } else {
                state &= ~HUD_FPS;
            }
        } else if (args[1] == "sr") {
            if (ParseBoolean(args[2])) {
                state |= HUD_SR;
            } else {
                state &= ~HUD_SR;
            }
        }
        m_BMLMod->SetHUD(state);
    }
}
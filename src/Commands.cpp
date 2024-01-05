#include "Commands.h"

#include "BML/Version.h"
#include "ModLoader.h"
#include "BMLMod.h"

void CommandBML::Execute(IBML *bml, const std::vector<std::string> &args) {
    auto &loader = ModLoader::GetInstance();
    bml->SendIngameMessage("Ballance Mod Loader Plus" BML_VERSION);
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
        std::string str = std::string("\t") + cmd->GetName();
        if (!cmd->GetAlias().empty())
            str += "(" + cmd->GetAlias() + ")";
        if (cmd->IsCheat())
            str += "[Cheat]";
        str += ": " + cmd->GetDescription();
        bml->SendIngameMessage(str.data());
    }
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
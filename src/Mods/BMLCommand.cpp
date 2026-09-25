#include "Mods/BMLCommand.h"

#include "BML/BML.h"
#include "BML/IBML.h"
#include "BML/IMod.h"
#include "Diagnostics/SystemReport.h"

namespace {
    void SendLine(IBML &bml, const std::string &line) {
        const std::string message = line + '\n';
        bml.SendIngameMessage(message.c_str());
    }
}

void BMLCommand::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (!bml)
        return;

    if (args.size() > 1 && args[1] == "system") {
        if (!m_Context) {
            SendLine(*bml, "System diagnostics are unavailable.");
            return;
        }
        SendLine(*bml, "BML+ system report:");
        for (const std::string &line : BML::Diagnostics::BuildSystemReport(*m_Context))
            SendLine(*bml, "  " + line);
        return;
    }

    bml->SendIngameMessage("Ballance Mod Loader Plus " BML_VERSION);
    bml->SendIngameMessage((std::to_string(bml->GetModCount()) + " Mods Installed:").data());

    int count = bml->GetModCount();
    for (int i = 0; i < count; ++i) {
        auto *mod = bml->GetMod(i);
        std::string str = std::string("  ") + mod->GetID() + ": " + mod->GetName() + " " + mod->GetVersion() + " by " + mod->GetAuthor();
        bml->SendIngameMessage(str.data());
    }
}

const std::vector<std::string> BMLCommand::GetTabCompletion(
    IBML *, const std::vector<std::string> &args) {
    if (args.size() == 2)
        return {"system"};
    return {};
}

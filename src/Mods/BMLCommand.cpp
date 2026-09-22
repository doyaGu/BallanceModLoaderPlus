#include "Mods/BMLCommand.h"

#include "BML/BML.h"
#include "BML/IBML.h"
#include "BML/IMod.h"

void BMLCommand::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (!bml)
        return;

    bml->SendIngameMessage("Ballance Mod Loader Plus " BML_VERSION);
    bml->SendIngameMessage((std::to_string(bml->GetModCount()) + " Mods Installed:").data());

    int count = bml->GetModCount();
    for (int i = 0; i < count; ++i) {
        auto *mod = bml->GetMod(i);
        std::string str = std::string("  ") + mod->GetID() + ": " + mod->GetName() + " " + mod->GetVersion() + " by " + mod->GetAuthor();
        bml->SendIngameMessage(str.data());
    }
}

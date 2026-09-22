#include "Gameplay/CheatCommand.h"

#include "BML/IBML.h"

void CheatCommand::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (args.size() == 1) {
        bml->EnableCheat(!bml->IsCheatEnabled());
    } else {
        bml->EnableCheat(ParseBoolean(args[1]));
    }
    bml->SendIngameMessage(bml->IsCheatEnabled() ? "Cheat Mode On" : "Cheat Mode Off");
}

#ifndef BML_MODS_BML_COMMAND_H
#define BML_MODS_BML_COMMAND_H

#include "BML/ICommand.h"

class BMLCommand : public ICommand {
public:
    BMLCommand() = default;

    std::string GetName() override { return "bml"; }
    std::string GetAlias() override { return ""; }
    std::string GetDescription() override { return "Show loader information and installed Mods."; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; }
};

#endif // BML_MODS_BML_COMMAND_H

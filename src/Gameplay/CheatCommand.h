#ifndef BML_GAMEPLAY_CHEAT_COMMAND_H
#define BML_GAMEPLAY_CHEAT_COMMAND_H

#include "BML/ICommand.h"

class CheatCommand : public ICommand {
public:
    CheatCommand() = default;

    std::string GetName() override { return "cheat"; }
    std::string GetAlias() override { return ""; }
    std::string GetDescription() override { return "Enable or Disable Cheat Mode."; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override {
        return args.size() == 2 ? std::vector<std::string>({"true", "false"}) : std::vector<std::string>();
    }
};

#endif // BML_GAMEPLAY_CHEAT_COMMAND_H

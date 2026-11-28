#ifndef BML_GAMEPLAY_CHEAT_COMMAND_H
#define BML_GAMEPLAY_CHEAT_COMMAND_H

#include "Console/CommandRecord.h"

class CheatCommand {
public:
    CheatCommand() = default;

    std::string GetName() { return "cheat"; }

    std::string GetAlias() { return ""; }

    std::string GetDescription() { return "Enable or Disable Cheat Mode."; }

    bool IsCheat() { return false; }

    void Execute(ModContext *bml, const std::vector<std::string> &args);

    const std::vector<std::string> GetTabCompletion(ModContext *bml,
                                                    const std::vector<std::string> &args) {
        return args.size() == 2 ? std::vector<std::string>({"true", "false"}) : std::vector<std::string>();
    }
};

#endif // BML_GAMEPLAY_CHEAT_COMMAND_H

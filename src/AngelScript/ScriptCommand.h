#ifndef BML_ANGELSCRIPT_SCRIPT_COMMAND_H
#define BML_ANGELSCRIPT_SCRIPT_COMMAND_H

#include "BML/ICommand.h"

class ScriptCommand : public ICommand {
public:
    ScriptCommand() = default;

    std::string GetName() override { return "script"; }
    std::string GetAlias() override { return ""; }
    std::string GetDescription() override { return "Manage script mods."; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override;
};

#endif // BML_ANGELSCRIPT_SCRIPT_COMMAND_H

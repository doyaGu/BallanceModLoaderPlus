#ifndef BML_ANGELSCRIPT_SCRIPT_COMMAND_H
#define BML_ANGELSCRIPT_SCRIPT_COMMAND_H

#include "Console/CommandRecord.h"

class ScriptCommand {
public:
    ScriptCommand() = default;

    std::string GetName() { return "script"; }

    std::string GetAlias() { return ""; }

    std::string GetDescription() { return "Manage script mods."; }

    bool IsCheat() { return false; }

    void Execute(ModContext *bml, const std::vector<std::string> &args);
    const std::vector<std::string> GetTabCompletion(ModContext *bml,
                                                    const std::vector<std::string> &args);
};

#endif // BML_ANGELSCRIPT_SCRIPT_COMMAND_H

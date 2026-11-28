#ifndef BML_DIAGNOSTICS_BML_COMMAND_H
#define BML_DIAGNOSTICS_BML_COMMAND_H

#include "Console/CommandRecord.h"

class ModContext;

class BMLCommand {
public:
    explicit BMLCommand(ModContext *context) : m_Context(context) {}

    std::string GetName() { return "bml"; }

    std::string GetAlias() { return ""; }

    std::string GetDescription() { return "Show loader information and installed Mods."; }

    bool IsCheat() { return false; }

    void Execute(ModContext *bml, const std::vector<std::string> &args);
    const std::vector<std::string> GetTabCompletion(ModContext *bml,
                                                    const std::vector<std::string> &args);

private:
    ModContext *m_Context = nullptr;
};

#endif // BML_DIAGNOSTICS_BML_COMMAND_H

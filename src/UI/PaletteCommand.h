#ifndef BML_UI_PALETTE_COMMAND_H
#define BML_UI_PALETTE_COMMAND_H

#include "Console/CommandRecord.h"

class PaletteCommand {
public:
    PaletteCommand() = default;

    std::string GetName() { return "palette"; }

    std::string GetAlias() { return "pal"; }

    std::string GetDescription() { return "Manage ANSI 256-color palette."; }

    bool IsCheat() { return false; }

    void Execute(ModContext *bml, const std::vector<std::string> &args);
    const std::vector<std::string> GetTabCompletion(ModContext *bml,
                                                    const std::vector<std::string> &args);
};

#endif // BML_UI_PALETTE_COMMAND_H

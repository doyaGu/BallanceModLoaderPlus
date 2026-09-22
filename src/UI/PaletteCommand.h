#ifndef BML_UI_PALETTE_COMMAND_H
#define BML_UI_PALETTE_COMMAND_H

#include "BML/ICommand.h"

class PaletteCommand : public ICommand {
public:
    PaletteCommand() = default;

    std::string GetName() override { return "palette"; }
    std::string GetAlias() override { return "pal"; }
    std::string GetDescription() override { return "Manage ANSI 256-color palette."; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override;
};

#endif // BML_UI_PALETTE_COMMAND_H

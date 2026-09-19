#ifndef BML_CUSTOMMAPS_MAPCOMMAND_H
#define BML_CUSTOMMAPS_MAPCOMMAND_H

#include "BML/ICommand.h"

class CustomMaps;

namespace CustomMap {

class MapCommand final : public ICommand {
public:
    explicit MapCommand(CustomMaps &maps) : m_Maps(maps) {}

    std::string GetName() override { return "map"; }
    std::string GetAlias() override { return ""; }
    std::string GetDescription() override { return "List and load custom maps."; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(
        IBML *bml, const std::vector<std::string> &args) override;

private:
    CustomMaps &m_Maps;
};

} // namespace CustomMap

#endif // BML_CUSTOMMAPS_MAPCOMMAND_H

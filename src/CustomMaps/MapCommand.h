#ifndef BML_CUSTOMMAPS_MAPCOMMAND_H
#define BML_CUSTOMMAPS_MAPCOMMAND_H

#include "Console/CommandRecord.h"

class CustomMaps;

namespace CustomMap {

class MapCommand final {
public:
    explicit MapCommand(CustomMaps &maps) : m_Maps(maps) {}

    std::string GetName() { return "map"; }

    std::string GetAlias() { return ""; }

    std::string GetDescription() { return "List and load custom maps."; }

    bool IsCheat() { return false; }

    void Execute(ModContext *bml, const std::vector<std::string> &args);
    const std::vector<std::string> GetTabCompletion(ModContext *bml,
                                                    const std::vector<std::string> &args);

private:
    CustomMaps &m_Maps;
};

} // namespace CustomMap

#endif // BML_CUSTOMMAPS_MAPCOMMAND_H

#include "CustomMaps/MapCommand.h"

#include <algorithm>
#include <cstddef>

#include "BML/IBML.h"
#include "CustomMaps/CustomMaps.h"

namespace CustomMap {
namespace {

void Report(IBML &bml, const std::string &message) {
    bml.SendIngameMessage(message.c_str());
}

std::string JoinArguments(const std::vector<std::string> &args, std::size_t first) {
    std::string text;
    for (std::size_t index = first; index < args.size(); ++index) {
        if (!text.empty())
            text += ' ';
        text += args[index];
    }
    return text;
}

} // namespace

void MapCommand::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (!bml)
        return;

    if (args.size() >= 2 && args[1] == "list") {
        const std::string fragment = JoinArguments(args, 2);
        std::string error;
        const std::vector<std::string> paths = m_Maps.ListMaps(fragment, error);
        if (!error.empty()) {
            Report(*bml, "map list: " + error);
        } else if (paths.empty()) {
            Report(*bml, "map list: no matching maps");
        } else {
            const std::size_t count = (std::min)(paths.size(), std::size_t(20));
            for (std::size_t index = 0; index < count; ++index)
                Report(*bml, paths[index]);
            if (paths.size() > count) {
                Report(*bml,
                       "map list: showing at most 20 maps; add a name fragment to narrow the list");
            }
        }
        return;
    }

    if (args.size() >= 3 && args[1] == "load") {
        const std::string relativePath = JoinArguments(args, 2);
        std::string error;
        if (!m_Maps.LoadFromCommand(relativePath, error))
            Report(*bml, "map load: " + error);
        else
            Report(*bml, "map load: loading " + relativePath);
        return;
    }

    Report(*bml,
           "Usage: map list [name fragment] | load \"<path relative to ModLoader/Maps>\"");
}

const std::vector<std::string> MapCommand::GetTabCompletion(
    IBML *, const std::vector<std::string> &args) {
    return args.size() == 2 ? std::vector<std::string>{"list", "load"}
                            : std::vector<std::string>{};
}

} // namespace CustomMap

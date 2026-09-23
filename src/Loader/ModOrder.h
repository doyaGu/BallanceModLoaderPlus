#ifndef BML_MODORDER_H
#define BML_MODORDER_H

#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Build the replacement index before taking the registry lock. Once complete,
// publish both containers together so readers never see a partial order.
template<typename Mod>
void PublishModOrder(std::vector<Mod *> &mods,
                     std::unordered_map<std::string, std::size_t> &index,
                     std::vector<Mod *> sorted,
                     const std::unordered_map<Mod *, std::string> &idsByMod,
                     std::shared_mutex &registryMutex) {
    std::unordered_map<std::string, std::size_t> nextIndex;
    nextIndex.reserve(sorted.size());
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        const auto inserted = nextIndex.emplace(idsByMod.at(sorted[i]), i);
        if (!inserted.second)
            throw std::logic_error("duplicate Mod id in dependency order");
    }

    std::unique_lock<std::shared_mutex> lock(registryMutex);
    mods.swap(sorted);
    index.swap(nextIndex);
}

#endif // BML_MODORDER_H

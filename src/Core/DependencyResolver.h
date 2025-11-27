#ifndef BML_CORE_DEPENDENCY_RESOLVER_H
#define BML_CORE_DEPENDENCY_RESOLVER_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "ModManifest.h"

namespace BML::Core {

struct DependencyResolutionError {
    std::string message;
    std::vector<std::string> chain;
};

struct DependencyWarning {
    std::string message;
    std::string mod_id;
    std::string dependency_id;
};

struct ResolvedNode {
    std::string id;
    const ModManifest *manifest;
};

class DependencyResolver {
public:
    void RegisterManifest(const ModManifest &manifest);
    void Clear();

    bool Resolve(std::vector<ResolvedNode> &out_order, 
                 std::vector<DependencyWarning> &out_warnings,
                 DependencyResolutionError &out_error) const;

private:
    struct Node {
        const ModManifest *manifest{nullptr};
        std::vector<const ModManifest *> duplicates;
        std::vector<std::string> dependents; // reverse edges for Kahn's algorithm
        uint32_t incoming_edges{0};
    };

    std::unordered_map<std::string, Node> m_nodes;
    std::vector<std::string> m_registrationOrder;
};

} // namespace BML::Core

#endif // BML_CORE_DEPENDENCY_RESOLVER_H

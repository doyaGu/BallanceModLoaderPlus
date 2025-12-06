#include "DependencyResolver.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "Logging.h"
#include "SemanticVersion.h"
#include "StringUtils.h"

namespace BML::Core {
    namespace {
        constexpr char kDepResolverLogCategory[] = "dependency.resolver";

        struct ReadyNode {
            size_t order{std::numeric_limits<size_t>::max()};
            std::string id;

            bool operator>(const ReadyNode &other) const {
                if (order != other.order)
                    return order > other.order;
                return id > other.id;
            }
        };

        bool MatchesRequirement(const SemanticVersionRange &range, const SemanticVersion &version) {
            if (!range.parsed)
                return true;
            return IsVersionSatisfied(range, version);
        }

        std::string DescribeManifest(const ModManifest *manifest) {
            if (!manifest)
                return std::string{"<unknown manifest>"};

            std::ostringstream oss;
            oss << manifest->package.id;
            if (!manifest->package.version.empty()) {
                oss << "@" << manifest->package.version;
            }
            if (!manifest->manifest_path.empty()) {
                oss << " (" << utils::Utf16ToUtf8(manifest->manifest_path) << ")";
            }
            return oss.str();
        }

        std::string DescribeDependencyRequirement(const ModDependency &dependency) {
            if (!dependency.requirement.parsed || dependency.requirement.raw_expression.empty()) {
                return dependency.id;
            }
            return dependency.id + " " + dependency.requirement.raw_expression;
        }

        bool ExtractCycle(const std::unordered_map<std::string, std::vector<std::string>> &adj,
                          std::vector<std::string> &out_chain) {
            enum class State { Unvisited, Visiting, Visited };
            std::unordered_map<std::string, State> state;
            for (const auto &[id, _] : adj) {
                state[id] = State::Unvisited;
            }

            std::vector<std::string> stack;
            out_chain.clear();

            std::function<bool(const std::string &)> dfs = [&](const std::string &node) {
                state[node] = State::Visiting;
                stack.push_back(node);
                auto it = adj.find(node);
                if (it != adj.end()) {
                    for (const auto &next : it->second) {
                        auto stateIt = state.find(next);
                        if (stateIt == state.end())
                            continue;
                        if (stateIt->second == State::Visiting) {
                            auto cycleStart = std::find(stack.begin(), stack.end(), next);
                            if (cycleStart != stack.end()) {
                                out_chain.assign(cycleStart, stack.end());
                                out_chain.push_back(next);
                            } else {
                                out_chain = {next};
                            }
                            return true;
                        }
                        if (stateIt->second == State::Unvisited) {
                            if (dfs(next))
                                return true;
                        }
                    }
                }
                state[node] = State::Visited;
                stack.pop_back();
                return false;
            };

            for (const auto &[id, _] : adj) {
                if (state[id] == State::Unvisited && dfs(id)) {
                    return true;
                }
            }

            return false;
        }
    } // namespace

    void DependencyResolver::RegisterManifest(const ModManifest &manifest) {
        auto [it, inserted] = m_Nodes.emplace(manifest.package.id, Node{});
        if (inserted) {
            m_RegistrationOrder.push_back(manifest.package.id);
            it->second.manifest = &manifest;
            CoreLog(BML_LOG_DEBUG, kDepResolverLogCategory,
                    "Registered manifest: %s v%s",
                    manifest.package.id.c_str(), manifest.package.version.c_str());
        } else {
            if (!it->second.manifest) {
                it->second.manifest = &manifest;
            } else {
                it->second.duplicates.push_back(&manifest);
                CoreLog(BML_LOG_WARN, kDepResolverLogCategory,
                        "Duplicate manifest detected: %s", manifest.package.id.c_str());
            }
        }
    }

    void DependencyResolver::Clear() {
        m_Nodes.clear();
        m_RegistrationOrder.clear();
    }

    bool DependencyResolver::Resolve(std::vector<ResolvedNode> &out_order,
                                     std::vector<DependencyWarning> &out_warnings,
                                     DependencyResolutionError &out_error) const {
        struct TempNode {
            const ModManifest *manifest{nullptr};
            uint32_t incoming{0};
            std::vector<std::string> outgoing;
        };

        out_order.clear();
        out_warnings.clear();
        out_error = {};

        auto appendManifestToChain = [&](const ModManifest *manifest) {
            if (!manifest)
                return;
            out_error.chain.push_back(DescribeManifest(manifest));
        };

        // Detect duplicate IDs early
        for (const auto &[id, node] : m_Nodes) {
            if (!node.duplicates.empty()) {
                std::ostringstream oss;
                oss << "Duplicate module id '" << id << "' found";
                out_error.message = oss.str();
                out_error.chain.clear();
                appendManifestToChain(node.manifest);
                for (const auto *dup : node.duplicates) {
                    appendManifestToChain(dup);
                }
                return false;
            }
        }

        std::unordered_map<std::string, TempNode> graph;
        graph.reserve(m_Nodes.size());
        for (const auto &[id, node] : m_Nodes) {
            graph.emplace(id, TempNode{node.manifest});
        }

        // Precompute registration order for deterministic traversal
        std::unordered_map<std::string, size_t> orderIndex;
        orderIndex.reserve(m_RegistrationOrder.size());
        for (size_t i = 0; i < m_RegistrationOrder.size(); ++i) {
            orderIndex[m_RegistrationOrder[i]] = i;
        }
        auto orderOf = [&](const std::string &id) {
            auto it = orderIndex.find(id);
            return it == orderIndex.end() ? std::numeric_limits<size_t>::max() : it->second;
        };

        // Conflict detection
        for (const auto &[id, node] : m_Nodes) {
            const auto *manifest = node.manifest;
            if (!manifest)
                continue;

            for (const auto &conflict : manifest->conflicts) {
                auto otherIt = m_Nodes.find(conflict.id);
                if (otherIt == m_Nodes.end() || !otherIt->second.manifest)
                    continue;

                const auto *otherManifest = otherIt->second.manifest;
                if (!MatchesRequirement(conflict.requirement, otherManifest->package.parsed_version))
                    continue;

                std::ostringstream oss;
                oss << "Conflict detected: " << DescribeManifest(manifest)
                    << " cannot load alongside " << DescribeManifest(otherManifest);
                if (conflict.requirement.parsed) {
                    oss << " (constraint " << conflict.requirement.raw_expression
                        << " matches installed version " << otherManifest->package.version << ")";
                }
                if (!conflict.reason.empty()) {
                    oss << ". Reason: " << conflict.reason;
                }

                out_error.message = oss.str();
                out_error.chain = {DescribeManifest(manifest), DescribeManifest(otherManifest)};
                return false;
            }
        }

        // Build dependency edges
        std::unordered_map<std::string, std::vector<std::string>> adjacency;
        std::unordered_set<std::string> warningDedup;

        for (const auto &[id, node] : m_Nodes) {
            const auto *manifest = node.manifest;
            if (!manifest)
                continue;

            for (const auto &dep : manifest->dependencies) {
                if (dep.id == manifest->package.id) {
                    out_error.message = "Module '" + manifest->package.id + "' cannot depend on itself";
                    out_error.chain = {DescribeManifest(manifest)};
                    return false;
                }

                auto depIt = graph.find(dep.id);
                if (depIt == graph.end()) {
                    if (dep.optional) {
                        DependencyWarning warning;
                        warning.mod_id = manifest->package.id;
                        warning.dependency_id = dep.id;
                        warning.message = "Optional dependency '" + DescribeDependencyRequirement(dep) +
                            "' not found for module '" + manifest->package.id + "'";
                        std::string key = manifest->package.id + "->" + dep.id + ":missing";
                        if (warningDedup.insert(key).second) {
                            CoreLog(BML_LOG_WARN, kDepResolverLogCategory,
                                    "%s", warning.message.c_str());
                            out_warnings.push_back(std::move(warning));
                        }
                        continue;
                    }
                    out_error.message = "Module '" + manifest->package.id + "' requires missing dependency '"
                        + DescribeDependencyRequirement(dep) + "'";
                    out_error.chain = {DescribeManifest(manifest), dep.id};
                    return false;
                }

                const auto *depManifest = depIt->second.manifest;
                if (depManifest && dep.requirement.parsed) {
                    if (!IsVersionSatisfied(dep.requirement, depManifest->package.parsed_version)) {
                        out_error.message = "Module '" + manifest->package.id + "' requires '" + dep.id +
                            "' " + dep.requirement.raw_expression + " but found " +
                            depManifest->package.version;
                        out_error.chain = {DescribeManifest(manifest), DescribeManifest(depManifest)};
                        return false;
                    }

                    std::string suggestion;
                    if (IsVersionOutdated(dep.requirement, depManifest->package.parsed_version, suggestion)) {
                        DependencyWarning warning;
                        warning.mod_id = manifest->package.id;
                        warning.dependency_id = dep.id;
                        warning.message = "Dependency '" + dep.id + "' version " + depManifest->package.version +
                            " satisfies requirement " + dep.requirement.raw_expression +
                            " but is at minimum version. " + suggestion;
                        std::string key = manifest->package.id + "->" + dep.id + ":outdated";
                        if (warningDedup.insert(key).second) {
                            CoreLog(BML_LOG_WARN, kDepResolverLogCategory,
                                    "%s", warning.message.c_str());
                            out_warnings.push_back(std::move(warning));
                        }
                    }
                }

                depIt->second.outgoing.push_back(manifest->package.id);
                adjacency[dep.id].push_back(manifest->package.id);

                auto &dependentNode = graph[manifest->package.id];
                dependentNode.incoming++;
            }
        }

        std::priority_queue<ReadyNode, std::vector<ReadyNode>, std::greater<>> ready;
        for (const auto &[id, node] : graph) {
            if (node.incoming == 0) {
                ready.push({orderOf(id), id});
            }
        }

        std::vector<std::string> resolved;
        resolved.reserve(graph.size());

        while (!ready.empty()) {
            auto current = ready.top();
            ready.pop();
            resolved.push_back(current.id);

            const auto &node = graph.at(current.id);
            for (const auto &dependent : node.outgoing) {
                auto &depNode = graph[dependent];
                if (--depNode.incoming == 0) {
                    ready.push({orderOf(dependent), dependent});
                }
            }
        }

        if (resolved.size() != graph.size()) {
            out_error.message = "Detected dependency cycle";
            if (!ExtractCycle(adjacency, out_error.chain)) {
                out_error.chain = resolved;
            }
            return false;
        }

        out_order.reserve(resolved.size());
        for (const auto &id : resolved) {
            const auto &node = m_Nodes.at(id);
            out_order.push_back({id, node.manifest});
        }

        return true;
    }
} // namespace BML::Core

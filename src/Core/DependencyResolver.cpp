#include "DependencyResolver.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "SemanticVersion.h"
#include "StringUtils.h"

namespace BML::Core {
    namespace {
        std::string Narrow(const std::wstring &wide) {
            return wide.empty() ? std::string() : utils::Utf16ToUtf8(wide);
        }

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
        auto [it, inserted] = m_nodes.emplace(manifest.package.id, Node{});
        if (inserted) {
            m_registrationOrder.push_back(manifest.package.id);
            it->second.manifest = &manifest;
        } else {
            if (!it->second.manifest) {
                it->second.manifest = &manifest;
            } else {
                it->second.duplicates.push_back(&manifest);
            }
        }
    }

    void DependencyResolver::Clear() {
        m_nodes.clear();
        m_registrationOrder.clear();
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

        // Detect duplicate IDs early
        for (const auto &[id, node] : m_nodes) {
            if (!node.duplicates.empty()) {
                std::ostringstream oss;
                oss << "Duplicate module id '" << id << "' found";
                out_error.message = oss.str();
                out_error.chain.clear();
                auto appendPath = [&](const ModManifest *manifest) {
                    if (!manifest)
                        return;
                    out_error.chain.push_back(Narrow(manifest->manifest_path));
                };
                appendPath(node.manifest);
                for (const auto *dup : node.duplicates) {
                    appendPath(dup);
                }
                return false;
            }
        }

        std::unordered_map<std::string, TempNode> graph;
        graph.reserve(m_nodes.size());
        for (const auto &[id, node] : m_nodes) {
            graph.emplace(id, TempNode{node.manifest});
        }

        // Precompute registration order for deterministic traversal
        std::unordered_map<std::string, size_t> orderIndex;
        orderIndex.reserve(m_registrationOrder.size());
        for (size_t i = 0; i < m_registrationOrder.size(); ++i) {
            orderIndex[m_registrationOrder[i]] = i;
        }
        auto orderOf = [&](const std::string &id) {
            auto it = orderIndex.find(id);
            return it == orderIndex.end() ? std::numeric_limits<size_t>::max() : it->second;
        };

        // Conflict detection
        for (const auto &[id, node] : m_nodes) {
            const auto *manifest = node.manifest;
            if (!manifest)
                continue;

            for (const auto &conflict : manifest->conflicts) {
                auto otherIt = m_nodes.find(conflict.id);
                if (otherIt == m_nodes.end() || !otherIt->second.manifest)
                    continue;

                const auto *otherManifest = otherIt->second.manifest;
                if (!MatchesRequirement(conflict.requirement, otherManifest->package.parsed_version))
                    continue;

                std::ostringstream oss;
                oss << "Conflict detected: " << manifest->package.id << " cannot load alongside "
                    << otherManifest->package.id;
                if (conflict.requirement.parsed) {
                    oss << " (constraint " << conflict.requirement.raw_expression
                        << " matches installed version " << otherManifest->package.version << ")";
                }
                if (!conflict.reason.empty()) {
                    oss << ". Reason: " << conflict.reason;
                }

                out_error.message = oss.str();
                out_error.chain = {manifest->package.id, otherManifest->package.id};
                return false;
            }
        }

        // Build dependency edges
        std::unordered_map<std::string, std::vector<std::string>> adjacency;

        for (const auto &[id, node] : m_nodes) {
            const auto *manifest = node.manifest;
            if (!manifest)
                continue;

            for (const auto &dep : manifest->dependencies) {
                auto depIt = graph.find(dep.id);
                if (depIt == graph.end()) {
                    if (dep.optional) {
                        DependencyWarning warning;
                        warning.mod_id = manifest->package.id;
                        warning.dependency_id = dep.id;
                        warning.message = "Optional dependency '" + dep.id + "' not found";
                        out_warnings.push_back(std::move(warning));
                        continue;
                    }
                    out_error.message = "Missing required dependency: " + dep.id;
                    out_error.chain = {manifest->package.id, dep.id};
                    return false;
                }

                const auto *depManifest = depIt->second.manifest;
                if (depManifest && dep.requirement.parsed) {
                    if (!IsVersionSatisfied(dep.requirement, depManifest->package.parsed_version)) {
                        out_error.message = "Version constraint not satisfied for dependency: " + dep.id;
                        out_error.chain = {
                            manifest->package.id,
                            dep.id + " (requires " + dep.requirement.raw_expression +
                            " but found " + depManifest->package.version + ")"
                        };
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
                        out_warnings.push_back(std::move(warning));
                    }
                }

                depIt->second.outgoing.push_back(manifest->package.id);
                adjacency[dep.id].push_back(manifest->package.id);

                auto &dependentNode = graph[manifest->package.id];
                dependentNode.incoming++;
            }
        }

        std::priority_queue<ReadyNode, std::vector<ReadyNode>, std::greater<ReadyNode>> ready;
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
            const auto &node = m_nodes.at(id);
            out_order.push_back({id, node.manifest});
        }

        return true;
    }
} // namespace BML::Core

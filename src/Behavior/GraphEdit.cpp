#include "Behavior/GraphEdit.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include <type_traits>
#include <utility>

namespace BML::Behavior {
namespace {

Status Failure(Error error, std::string message) {
    Status status{error, CKERR_INVALIDPARAMETER, CKBR_PARAMETERERROR,
                  std::move(message)};
    status.Details.Stage = Phase::Edit;
    return status;
}

Status ResolvePort(const GraphNode &node, const Slot &selector,
                   GraphEndpoint &out) {
    if (selector.Kind != SlotKind::Input &&
        selector.Kind != SlotKind::Output) {
        return Failure(Error::TypeMismatch,
                       "A Link selector requires an In or Out port.");
    }

    std::vector<const GraphPort *> matches;
    for (const GraphPort &port : node.Ports) {
        if (port.Kind != selector.Kind)
            continue;
        if (selector.UsesName()) {
            if (port.Name == selector.Name)
                matches.push_back(&port);
        } else if (selector.RequireOnly || port.Index == selector.Index) {
            matches.push_back(&port);
        }
    }
    if (matches.empty())
        return Failure(Error::SlotNotFound,
                       "A Link port did not match the current graph.");
    if ((selector.RequireOnly ||
         (selector.UsesName() && selector.RequireUnique)) &&
        matches.size() != 1) {
        return Failure(Error::AmbiguousSlot,
                       "A Link port is ambiguous in the current graph.");
    }
    const int occurrence = selector.UsesName() ? selector.Occurrence : 0;
    if (occurrence < 0 ||
        occurrence >= static_cast<int>(matches.size())) {
        return Failure(Error::SlotNotFound,
                       "A Link port occurrence does not exist.");
    }
    out = {node.Id, matches[static_cast<std::size_t>(occurrence)]->Kind,
           matches[static_cast<std::size_t>(occurrence)]->Index};
    return {};
}

} // namespace

Port GraphEdit::Entry(int index) const { return Graph().In(index); }

Port GraphEdit::Entry(std::string name) const {
    return Graph().In(std::move(name));
}

Port GraphEdit::Exit(int index) const { return Graph().Out(index); }

Port GraphEdit::Exit(std::string name) const {
    return Graph().Out(std::move(name));
}

Node GraphEdit::RequireOne(NodeQuery query) {
    const Node node{NextNode()};
    m_Nodes.push_back({node, std::move(query), CKGUID(), {}, false, {}});
    return node;
}

Node GraphEdit::UseNode(const ObjectRef &node) {
    EditNode entry;
    entry.Handle = Node{NextNode()};
    entry.Anchor = node;
    m_Nodes.push_back(entry);
    return entry.Handle;
}

Link GraphEdit::UseLink(const ObjectRef &link) {
    EditLink entry;
    entry.Handle = Link{NextLink()};
    entry.Anchor = link;
    m_Links.push_back(entry);
    return entry.Handle;
}

Link GraphEdit::RequireOne(Port source, Port sink,
                           std::optional<int> delay) {
    const Link link{NextLink()};
    m_Links.push_back(
        {link, std::move(source), std::move(sink), delay});
    return link;
}

PathRef GraphEdit::Follow(Port start) {
    const PathRef path{NextPath()};
    m_Paths.push_back({path, std::move(start)});
    return path;
}

Node GraphEdit::Add(CKGUID prototype) {
    const Node node{NextNode()};
    m_Nodes.push_back({node, {}, prototype, {}, true});
    return node;
}

Status GraphEdit::Setting(Node node, Slot slot, Value value,
                          bool nextStage) {
    const auto owner = std::find_if(
        m_Nodes.begin(), m_Nodes.end(), [&](const EditNode &candidate) {
            return candidate.Handle == node;
        });
    if (owner == m_Nodes.end())
        return Failure(Error::InvalidState,
                       "A Setting names an unknown Block.");
    if (!owner->Added)
        return Failure(Error::InterfaceUnsupported,
                       "Only an added Block can declare a Setting.");
    if (nextStage && owner->Settings.empty())
        return Failure(Error::InvalidState,
                       "A later Setting stage requires an earlier stage.");
    if (owner->Settings.empty() || nextStage)
        owner->Settings.emplace_back();
    owner->Settings.back().emplace_back(std::move(slot), std::move(value));
    return {};
}

void GraphEdit::Flow(Port source, Port sink, int delay, Cycle cycle) {
    m_Actions.emplace_back(EditFlow{
        std::move(source), std::move(sink), delay, cycle, m_NextAction++});
}

void GraphEdit::Bind(Port target, Value value) {
    EditBind bind;
    bind.Target = std::move(target);
    bind.Kind = BindKind::Literal;
    bind.Literal = std::move(value);
    bind.Ordinal = m_NextAction++;
    m_Actions.emplace_back(std::move(bind));
}

void GraphEdit::Bind(Port target, Port source) {
    EditBind bind;
    bind.Target = std::move(target);
    bind.Kind = BindKind::Direct;
    bind.Source = std::move(source);
    bind.Ordinal = m_NextAction++;
    m_Actions.emplace_back(std::move(bind));
}

void GraphEdit::Share(Port target, Port source) {
    EditBind bind;
    bind.Target = std::move(target);
    bind.Kind = BindKind::Shared;
    bind.Source = std::move(source);
    bind.Ordinal = m_NextAction++;
    m_Actions.emplace_back(std::move(bind));
}

void GraphEdit::Push(Port source, Port destination) {
    m_Actions.emplace_back(EditPush{
        std::move(source), std::move(destination), m_NextAction++});
}

void GraphEdit::Tap(Port source, HookBlock::Hook hook) {
    m_Actions.emplace_back(EditTap{
        std::move(source), std::move(hook), m_NextAction++});
}

void GraphEdit::After(PathRef path, HookBlock::Hook hook) {
    m_Actions.emplace_back(EditAfter{
        path, std::move(hook), m_NextAction++});
}

void GraphEdit::Before(Link target, HookBlock::Hook hook) {
    m_Actions.emplace_back(EditBefore{
        target, std::move(hook), m_NextAction++});
}

void GraphEdit::Splice(Link target, Node block,
                       std::vector<Order> ordering) {
    Splice(target, block.In(), block.Out(), std::move(ordering));
}

void GraphEdit::Splice(Link target, Port input, Port output,
                       std::vector<Order> ordering) {
    m_Actions.emplace_back(EditSplice{
        target, {input.Owner}, std::move(input), std::move(output),
        std::move(ordering), m_NextAction++});
}

void GraphEdit::Redirect(Link target, Port sink,
                         std::vector<Order> ordering) {
    m_Actions.emplace_back(EditRedirect{
        target, std::move(sink), std::move(ordering), m_NextAction++});
}

Port GraphEdit::AppendIn(Node node, std::string name) {
    return Append(node, SlotKind::Input, std::move(name), CKGUID());
}

Port GraphEdit::AppendOut(Node node, std::string name) {
    return Append(node, SlotKind::Output, std::move(name), CKGUID());
}

Port GraphEdit::AppendPin(Node node, std::string name, CKGUID type) {
    return Append(node, SlotKind::InputParameter, std::move(name), type);
}

Port GraphEdit::AppendPout(Node node, std::string name, CKGUID type) {
    return Append(node, SlotKind::OutputParameter, std::move(name), type);
}

Port GraphEdit::AppendLocal(Node node, std::string name, CKGUID type) {
    return Append(node, SlotKind::Local, std::move(name), type);
}

Port GraphEdit::Append(Node node, SlotKind kind, std::string name,
                       CKGUID type) {
    const int identity = -1 - static_cast<int>(m_NextInterface++);
    Port handle{node.Value, Slot::At(kind, identity, type)};
    m_Actions.emplace_back(EditInterface{
        handle, node, kind, std::move(name), type, m_NextAction++});
    return handle;
}

Status GraphEdit::Validate() const {
    const auto knownNode = [&](std::uint32_t value) {
        return value == Graph().Value ||
            std::any_of(m_Nodes.begin(), m_Nodes.end(),
                        [&](const EditNode &node) {
                            return node.Handle.Value == value;
                        });
    };
    const auto existingNode = [&](std::uint32_t value) {
        if (value == Graph().Value)
            return true;
        const auto found = std::find_if(
            m_Nodes.begin(), m_Nodes.end(), [&](const EditNode &node) {
                return node.Handle.Value == value;
            });
        return found != m_Nodes.end() && !found->Added;
    };
    const auto addedNode = [&](std::uint32_t value) {
        const auto found = std::find_if(
            m_Nodes.begin(), m_Nodes.end(), [&](const EditNode &node) {
                return node.Handle.Value == value;
            });
        return found != m_Nodes.end() && found->Added;
    };

    for (const EditNode &node : m_Nodes) {
        if (node.Added && !node.Prototype.IsValid())
            return Failure(Error::PrototypeNotFound,
                           "An added Block requires a Prototype GUID.");
        if (node.Added && !node.Anchor.IsNull())
            return Failure(Error::InvalidState,
                           "An added Block cannot also name an existing Node.");
        if (!node.Added && !node.Query && node.Anchor.IsNull())
            return Failure(Error::QueryNotFound,
                           "A Node query has no semantic identity.");
        if (!node.Added && !node.Settings.empty())
            return Failure(Error::InterfaceUnsupported,
                           "Only an added Block can declare a Setting.");
        for (const Settings &stage : node.Settings) {
            if (stage.empty())
                return Failure(Error::InvalidState,
                               "A declared Setting stage cannot be empty.");
            for (const auto &[slot, value] : stage) {
                if (slot.Kind != SlotKind::Setting)
                    return Failure(
                        Error::TypeMismatch,
                        "A declared Setting must name a Setting slot.");
                if (value.IsNull() && !value.Type().IsValid())
                    return Failure(
                        Error::TypeMismatch,
                        "A null Value requires a Virtools type GUID.");
            }
        }
    }
    for (const EditLink &link : m_Links) {
        if (!link.Anchor.IsNull()) {
            if (link.Source || link.Sink || link.Delay)
                return Failure(Error::InvalidState,
                               "A Link named by identity carries no query.");
            continue;
        }
        if (!link.Source || !link.Sink ||
            !existingNode(link.Source.Owner) ||
            !existingNode(link.Sink.Owner) ||
            (!link.Source.Selector.UsesName() &&
             !link.Source.Selector.RequireOnly &&
             link.Source.Selector.Index < 0) ||
            (!link.Sink.Selector.UsesName() &&
             !link.Sink.Selector.RequireOnly &&
             link.Sink.Selector.Index < 0)) {
            return Failure(Error::InvalidState,
                           "A Link query requires existing graph ports.");
        }
        if (link.Delay && (*link.Delay < 0 || *link.Delay >= 32765)) {
            return Failure(Error::InvalidDelay,
                           "A Link delay must be between 0 and 32764 frames.");
        }
    }
    for (const EditPath &path : m_Paths) {
        const bool rootEntry = path.Start.Owner == Graph().Value &&
            path.Start.Selector.Kind == SlotKind::Input;
        const bool nodeOut = path.Start.Owner != Graph().Value &&
            path.Start.Selector.Kind == SlotKind::Output;
        if (!path.Handle || !path.Start ||
            !existingNode(path.Start.Owner) ||
            (!path.Start.Selector.UsesName() &&
             !path.Start.Selector.RequireOnly &&
             path.Start.Selector.Index < 0) ||
            (!rootEntry && !nodeOut)) {
            return Failure(
                Error::InvalidState,
                "A Path must begin at a graph Entry or existing node Out.");
        }
    }

    using SymbolKey = std::tuple<std::uint32_t, SlotKind, int>;
    std::set<SymbolKey> interface;
    const auto port = [&](const Port &value) {
        if (!value || !knownNode(value.Owner))
            return false;
        return value.Selector.UsesName() || value.Selector.RequireOnly ||
            value.Selector.Index >= 0 ||
            interface.contains({value.Owner, value.Selector.Kind,
                                value.Selector.Index});
    };
    for (const Action &action : m_Actions) {
        Status status = std::visit([&](const auto &item) -> Status {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, EditFlow>) {
                if (item.Delay < 0 || item.Delay >= 32765)
                    return Failure(
                        Error::InvalidDelay,
                        "A Flow delay must be between 0 and 32764 frames.");
                if (!port(item.Source) || !port(item.Sink))
                    return Failure(Error::InvalidState,
                                   "A Flow names an unknown Node.");
            } else if constexpr (std::is_same_v<T, EditBind>) {
                if (!port(item.Target) ||
                    (item.Kind != BindKind::Literal && !port(item.Source))) {
                    return Failure(Error::InvalidState,
                                   "A Bind names an unknown Node.");
                }
                if (item.Kind == BindKind::Literal &&
                    item.Literal.IsNull() &&
                    !item.Literal.Type().IsValid()) {
                    return Failure(
                        Error::TypeMismatch,
                        "A null Value requires a Virtools type GUID.");
                }
            } else if constexpr (std::is_same_v<T, EditPush>) {
                if (!port(item.Source) || !port(item.Destination))
                    return Failure(Error::InvalidState,
                                   "A Push names an unknown Node.");
            } else if constexpr (std::is_same_v<T, EditSplice>) {
                if (!item.Target || !port(item.Input) || !port(item.Output))
                    return Failure(Error::InvalidState,
                                   "A Splice names an unknown Link or Node.");
            } else if constexpr (std::is_same_v<T, EditRedirect>) {
                if (!item.Target || !port(item.Sink))
                    return Failure(Error::InvalidState,
                                   "A Redirect names an unknown Link or Node.");
            } else if constexpr (std::is_same_v<T, EditInterface>) {
                if (!knownNode(item.Owner.Value) || item.Name.empty())
                    return Failure(Error::InvalidState,
                                   "A dynamic interface requires a Node and name.");
                // Edit::Validate never admits a dynamic Local; reject it at
                // Submit instead of at every installation.
                if (item.Kind == SlotKind::Local)
                    return Failure(Error::InterfaceUnsupported,
                                   "Dynamic Locals are not supported by Graph Edits.");
                if ((item.Kind == SlotKind::InputParameter ||
                     item.Kind == SlotKind::OutputParameter) &&
                    !item.Type.IsValid()) {
                    return Failure(Error::TypeMismatch,
                                   "A dynamic parameter requires a Virtools type GUID.");
                }
                interface.emplace(item.Handle.Owner,
                                  item.Handle.Selector.Kind,
                                  item.Handle.Selector.Index);
            } else if constexpr (std::is_same_v<T, EditTap>) {
                const bool rootEntry = item.Source.Owner == Graph().Value &&
                    item.Source.Selector.Kind == SlotKind::Input;
                const bool nodeOut = item.Source.Owner != Graph().Value &&
                    item.Source.Selector.Kind == SlotKind::Output;
                if (!port(item.Source) || !item.Hook ||
                    (!rootEntry && !nodeOut)) {
                    return Failure(
                        Error::InvalidState,
                        "Tap requires a graph Entry or node Out and a callback.");
                }
            } else if constexpr (std::is_same_v<T, EditAfter>) {
                const bool known = std::any_of(
                    m_Paths.begin(), m_Paths.end(),
                    [&](const EditPath &path) {
                        return path.Handle == item.Target;
                    });
                if (!known || !item.Hook) {
                    return Failure(
                        Error::InvalidState,
                        "After requires a Path from this Graph Edit and a callback.");
                }
            } else if constexpr (std::is_same_v<T, EditBefore>) {
                if (!item.Target || !item.Hook) {
                    return Failure(
                        Error::InvalidState,
                        "Before requires a Link from this Graph Edit and a callback.");
                }
            }
            return {};
        }, action);
        if (!status)
            return status;
    }
    return {};
}

bool GraphEdit::UsesIdentity() const noexcept {
    return std::any_of(m_Nodes.begin(), m_Nodes.end(),
                       [](const EditNode &node) {
                           return !node.Anchor.IsNull();
                       }) ||
        std::any_of(m_Links.begin(), m_Links.end(),
                    [](const EditLink &link) {
                        return !link.Anchor.IsNull();
                    });
}

Status GraphEdit::Compile(const PatchKey &patch, const ObjectRef &graph,
                          Compiler &compiler, Edit &out,
                          std::map<std::uint32_t, Node> *nodeHandles) const {
    if (nodeHandles)
        nodeHandles->clear();
    out = {};
    if (patch.Owner.empty() || patch.Name.empty() || graph.IsNull())
        return Failure(Error::InvalidState,
                       "A Graph Edit requires a patch key and live graph.");

    Status status = Validate();
    if (!status)
        return status;

    GraphModel base;
    Edit resolved;
    status = compiler.Begin(patch, graph, resolved, base);
    if (!status)
        return status;
    if (base.Root != graph)
        return Failure(Error::GraphChanged,
                       "The compiled Graph Edit inspected another graph.");

    const auto root = std::find_if(
        base.Nodes.begin(), base.Nodes.end(),
        [](const GraphNode &node) { return node.Parent == 0; });
    if (root == base.Nodes.end() || root->Object != graph)
        return Failure(Error::InvalidGraphLocality,
                       "The Graph Edit target is not the logical root graph.");

    std::map<std::uint32_t, const GraphNode *> nodes;
    std::map<std::uint32_t, Node> liveNodes;
    nodes.emplace(Graph().Value, &*root);
    liveNodes.emplace(Graph().Value, resolved.Graph());

    for (const EditNode &item : m_Nodes) {
        if (item.Added)
            continue;
        std::vector<const GraphNode *> matches;
        for (const GraphNode &candidate : base.Nodes) {
            if (candidate.Parent != root->Id)
                continue;
            if (!item.Anchor.IsNull()) {
                if (candidate.Object == item.Anchor)
                    matches.push_back(&candidate);
                continue;
            }
            if (!item.Query.Name.empty() &&
                candidate.Name != item.Query.Name)
                continue;
            if (item.Query.Prototype.IsValid() &&
                candidate.Prototype != item.Query.Prototype)
                continue;
            matches.push_back(&candidate);
        }
        if (matches.empty()) {
            if (!item.Anchor.IsNull())
                return Failure(
                    Error::InvalidGraphLocality,
                    "A Node named by identity is not in the target graph.");
            return Failure(Error::QueryNotFound,
                           "A Node query matched no Node.");
        }
        if (matches.size() != 1) {
            std::ostringstream message;
            message << "A Node query matched " << matches.size()
                    << " Nodes; RequireOne cannot choose between them.";
            return Failure(Error::QueryAmbiguous, message.str());
        }

        Node live;
        status = compiler.UseNode(resolved, matches.front()->Object, live);
        if (!status)
            return status;
        nodes.emplace(item.Handle.Value, matches.front());
        liveNodes.emplace(item.Handle.Value, live);
    }

    std::map<std::uint32_t, Link> liveLinks;
    for (const EditLink &item : m_Links) {
        if (!item.Anchor.IsNull()) {
            const auto match = std::find_if(
                base.Links.begin(), base.Links.end(),
                [&](const GraphLink &candidate) {
                    return candidate.Object == item.Anchor;
                });
            if (match == base.Links.end())
                return Failure(
                    Error::LinkNotFound,
                    "A Link named by identity is not in the target graph.");
            Link live;
            status = compiler.UseLink(resolved, match->Object, live);
            if (!status)
                return status;
            liveLinks.emplace(item.Handle.Value, live);
            continue;
        }
        const auto sourceNode = nodes.find(item.Source.Owner);
        const auto sinkNode = nodes.find(item.Sink.Owner);
        if (sourceNode == nodes.end() || sinkNode == nodes.end())
            return Failure(Error::InvalidState,
                           "A Link query names an added or unknown Node.");
        GraphEndpoint source;
        GraphEndpoint sink;
        status = ResolvePort(*sourceNode->second, item.Source.Selector, source);
        if (status)
            status = ResolvePort(*sinkNode->second, item.Sink.Selector, sink);
        if (!status)
            return status;

        std::vector<const GraphLink *> matches;
        for (const GraphLink &candidate : base.Links) {
            if (candidate.Source != source || candidate.Target != sink)
                continue;
            if (item.Delay && candidate.InitialDelay != *item.Delay)
                continue;
            matches.push_back(&candidate);
        }
        if (matches.empty())
            return Failure(Error::LinkNotFound,
                           "An exact Link query matched no Link.");
        if (matches.size() != 1) {
            std::ostringstream message;
            message << "An exact Link query matched " << matches.size()
                    << " parallel Links; add a delay or stronger endpoint selector.";
            return Failure(Error::QueryAmbiguous, message.str());
        }
        Link live;
        status = compiler.UseLink(resolved, matches.front()->Object, live);
        if (!status)
            return status;
        liveLinks.emplace(item.Handle.Value, live);
    }

    struct LivePath {
        Port End;
        std::vector<Link> Links;
        bool EndsAtExit = false;
    };
    std::map<std::uint32_t, LivePath> livePaths;
    for (const EditPath &item : m_Paths) {
        const auto owner = nodes.find(item.Start.Owner);
        const auto liveOwner = liveNodes.find(item.Start.Owner);
        if (owner == nodes.end() || liveOwner == liveNodes.end()) {
            return Failure(Error::InvalidState,
                           "A Path names an unknown Node.");
        }

        GraphEndpoint start;
        status = ResolvePort(*owner->second, item.Start.Selector, start);
        if (!status)
            return status;

        Path path;
        status = CompletePath(base, start, path);
        if (!status)
            return status;

        LivePath live;
        for (const LinkBase &link : path.Links) {
            Link resolvedLink;
            status = compiler.UseLink(
                resolved, link.Anchor, resolvedLink);
            if (!status)
                return status;
            live.Links.push_back(resolvedLink);
        }
        live.EndsAtExit = path.End.Node == root->Id &&
            path.End.Kind == SlotKind::Output;
        if (!live.EndsAtExit) {
            if (path.End == start) {
                live.End = {
                    liveOwner->second.Value, item.Start.Selector};
            } else {
                const auto end = std::find_if(
                    base.Nodes.begin(), base.Nodes.end(),
                    [&](const GraphNode &node) {
                        return node.Id == path.End.Node;
                    });
                if (end == base.Nodes.end()) {
                    return Failure(Error::GraphChanged,
                                   "A Path endpoint disappeared.");
                }
                Node liveEnd;
                const auto existing = std::find_if(
                    nodes.begin(), nodes.end(),
                    [&](const auto &entry) {
                        return entry.second->Id == path.End.Node;
                    });
                if (existing != nodes.end()) {
                    liveEnd = liveNodes.at(existing->first);
                } else {
                    status = compiler.UseNode(
                        resolved, end->Object, liveEnd);
                    if (!status)
                        return status;
                }
                live.End = {
                    liveEnd.Value,
                    Slot::At(path.End.Kind, path.End.Index)};
            }
        }
        livePaths.emplace(item.Handle.Value, std::move(live));
    }

    for (const EditNode &item : m_Nodes) {
        if (!item.Added)
            continue;
        Node live;
        status = compiler.Add(resolved, item.Prototype, item.Settings, live);
        if (!status)
            return status;
        liveNodes.emplace(item.Handle.Value, live);
    }

    using PortKey = std::tuple<std::uint32_t, SlotKind, int>;
    std::map<PortKey, Port> liveInterface;
    const auto port = [&](const Port &symbolic, Port &live) -> Status {
        // Slot::Only is also nameless with a negative index; only an
        // intent-local dynamic Port handle is looked up here.
        if (!symbolic.Selector.UsesName() && !symbolic.Selector.RequireOnly &&
            symbolic.Selector.Index < 0) {
            const auto dynamic = liveInterface.find({
                symbolic.Owner, symbolic.Selector.Kind,
                symbolic.Selector.Index});
            if (dynamic == liveInterface.end())
                return Failure(Error::InvalidState,
                               "A dynamic Port was used before it was declared.");
            live = dynamic->second;
            return {};
        }
        const auto owner = liveNodes.find(symbolic.Owner);
        if (owner == liveNodes.end())
            return Failure(Error::InvalidState,
                           "A Graph Edit action names an unknown Node.");
        live = {owner->second.Value, symbolic.Selector};
        return {};
    };

    for (const Action &action : m_Actions) {
        status = std::visit([&](const auto &item) -> Status {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, EditFlow>) {
                Port source;
                Port sink;
                Status current = port(item.Source, source);
                if (current)
                    current = port(item.Sink, sink);
                if (!current)
                    return current;
                resolved.Flow(std::move(source), std::move(sink), item.Delay,
                              item.SameFrameCycle);
            } else if constexpr (std::is_same_v<T, EditBind>) {
                Port target;
                Status current = port(item.Target, target);
                if (!current)
                    return current;
                if (item.Kind == BindKind::Literal) {
                    resolved.Bind(std::move(target), item.Literal);
                } else {
                    Port source;
                    current = port(item.Source, source);
                    if (!current)
                        return current;
                    if (item.Kind == BindKind::Direct)
                        resolved.Bind(std::move(target), std::move(source));
                    else
                        resolved.Share(std::move(target), std::move(source));
                }
            } else if constexpr (std::is_same_v<T, EditPush>) {
                Port source;
                Port destination;
                Status current = port(item.Source, source);
                if (current)
                    current = port(item.Destination, destination);
                if (!current)
                    return current;
                resolved.Push(std::move(source), std::move(destination));
            } else if constexpr (std::is_same_v<T, EditSplice>) {
                const auto link = liveLinks.find(item.Target.Value);
                if (link == liveLinks.end())
                    return Failure(Error::InvalidState,
                                   "A Graph Edit Splice names an unknown Link.");
                Port input;
                Port output;
                Status current = port(item.Input, input);
                if (current)
                    current = port(item.Output, output);
                if (!current)
                    return current;
                resolved.Splice(link->second, std::move(input),
                                std::move(output), item.Ordering);
            } else if constexpr (std::is_same_v<T, EditRedirect>) {
                const auto link = liveLinks.find(item.Target.Value);
                if (link == liveLinks.end())
                    return Failure(Error::InvalidState,
                                   "A Graph Edit Redirect names an unknown Link.");
                Port sink;
                Status current = port(item.Sink, sink);
                if (!current)
                    return current;
                resolved.Redirect(link->second, std::move(sink), item.Ordering);
            } else if constexpr (std::is_same_v<T, EditInterface>) {
                const auto owner = liveNodes.find(item.Owner.Value);
                if (owner == liveNodes.end())
                    return Failure(Error::InvalidState,
                                   "A dynamic interface names an unknown Node.");
                Port live;
                switch (item.Kind) {
                case SlotKind::Input:
                    live = resolved.AppendIn(owner->second, item.Name);
                    break;
                case SlotKind::Output:
                    live = resolved.AppendOut(owner->second, item.Name);
                    break;
                case SlotKind::InputParameter:
                    live = resolved.AppendPin(owner->second, item.Name,
                                              item.Type);
                    break;
                case SlotKind::OutputParameter:
                    live = resolved.AppendPout(owner->second, item.Name,
                                               item.Type);
                    break;
                case SlotKind::Local:
                    live = resolved.AppendLocal(owner->second, item.Name,
                                                item.Type);
                    break;
                default:
                    return Failure(Error::InterfaceUnsupported,
                                   "This Slot kind is not a dynamic interface.");
                }
                liveInterface.emplace(
                    PortKey{item.Handle.Owner, item.Handle.Selector.Kind,
                            item.Handle.Selector.Index},
                    std::move(live));
            } else if constexpr (std::is_same_v<T, EditTap>) {
                Port source;
                Status current = port(item.Source, source);
                if (!current)
                    return current;
                return compiler.Tap(
                    resolved, std::move(source), item.Hook);
            } else if constexpr (std::is_same_v<T, EditAfter>) {
                const auto path = livePaths.find(item.Target.Value);
                if (path == livePaths.end()) {
                    return Failure(Error::InvalidState,
                                   "After names an unknown Path.");
                }
                if (!path->second.EndsAtExit) {
                    return compiler.Tap(
                        resolved, path->second.End, item.Hook);
                }
                if (path->second.Links.empty())
                    return Failure(Error::GraphChanged,
                                   "A Path reached an Exit without a Link.");
                return compiler.Interpose(
                    resolved, path->second.Links.back(), item.Hook);
            } else if constexpr (std::is_same_v<T, EditBefore>) {
                const auto link = liveLinks.find(item.Target.Value);
                if (link == liveLinks.end()) {
                    return Failure(Error::InvalidState,
                                   "Before names an unknown Link.");
                }
                return compiler.Interpose(
                    resolved, link->second, item.Hook);
            }
            return {};
        }, action);
        if (!status)
            return status;
    }

    if (nodeHandles)
        *nodeHandles = liveNodes;

    resolved.m_ExpectedFingerprint = base.Fingerprint;
    out = std::move(resolved);
    return {};
}

} // namespace BML::Behavior

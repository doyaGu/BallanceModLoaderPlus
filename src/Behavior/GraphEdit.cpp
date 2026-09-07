#include "Behavior/GraphEdit.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include <type_traits>
#include <utility>

namespace BML::Behavior::Internal {
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

std::uint64_t PortShape(const GraphNode &node) {
    constexpr std::uint64_t offset = 1469598103934665603ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t hash = offset;
    const auto append = [&](const void *data, std::size_t size) {
        const auto *bytes = static_cast<const unsigned char *>(data);
        for (std::size_t index = 0; index < size; ++index) {
            hash ^= bytes[index];
            hash *= prime;
        }
    };
    for (const GraphPort &port : node.Ports) {
        const auto kind = static_cast<std::uint32_t>(port.Kind) + 1u;
        append(&kind, sizeof(kind));
        append(&port.Index, sizeof(port.Index));
        append(&port.Occurrence, sizeof(port.Occurrence));
        append(&port.Type.d1, sizeof(port.Type.d1));
        append(&port.Type.d2, sizeof(port.Type.d2));
        append(port.Name.data(), port.Name.size());
    }
    return hash;
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
    m_Nodes.push_back(
        {node, std::move(query), std::nullopt, std::nullopt, {}});
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
    return Add(BlockSpec(prototype));
}

Node GraphEdit::Add(PrototypeRef prototype) {
    BlockSpec block(prototype.Guid);
    block.PrototypeGeneration(prototype.Generation);
    return Add(std::move(block));
}

Node GraphEdit::Add(BlockSpec block) {
    const Node node{NextNode()};
    m_Nodes.push_back(
        {node, {}, std::move(block), std::nullopt, {}});
    return node;
}

Node GraphEdit::AddGraph(std::string name, int priority) {
    const Node node{NextNode()};
    m_Nodes.push_back(
        {node, {}, std::nullopt,
         GraphNodeSpec{std::move(name), priority}, {}});
    return node;
}

GraphEdit &GraphEdit::Enter(Node node, std::uint32_t scope) {
    const auto found = std::find_if(
        m_Nested.begin(), m_Nested.end(),
        [&](const Nested &candidate) { return candidate.Scope == scope; });
    if (found != m_Nested.end())
        return *found->Body;
    Nested nested;
    nested.Parent = node;
    nested.Scope = scope;
    nested.Body = std::make_unique<GraphEdit>();
    m_Nested.push_back(std::move(nested));
    return *m_Nested.back().Body;
}

Node GraphEdit::Replace(Node target, BlockSpec block) {
    const Node replacement = Add(std::move(block));
    m_Replacements.push_back({target, replacement, m_NextAction++});
    return replacement;
}

void GraphEdit::Remove(Node target) {
    m_Removals.push_back({target, m_NextAction++});
}

ParameterOperation GraphEdit::AddOperation(
    CKGUID operation, CKGUID result, CKGUID input1, CKGUID input2) {
    const ParameterOperation handle{NextNode()};
    m_Operations.push_back({handle, operation, result, input1, input2});
    return handle;
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
        return found != m_Nodes.end() && !found->Authored();
    };
    const auto addedNode = [&](std::uint32_t value) {
        const auto found = std::find_if(
            m_Nodes.begin(), m_Nodes.end(), [&](const EditNode &node) {
                return node.Handle.Value == value;
            });
        return found != m_Nodes.end() && found->Authored();
    };
    const auto operation = [&](std::uint32_t value) -> const Operation * {
        const auto found = std::find_if(
            m_Operations.begin(), m_Operations.end(),
            [&](const Operation &candidate) {
                return candidate.Handle.Value == value;
            });
        return found == m_Operations.end() ? nullptr : &*found;
    };
    for (const EditNode &node : m_Nodes) {
        if (node.Block && !node.Block->Prototype().IsValid())
            return Failure(Error::PrototypeNotFound,
                           "An added Block requires a Prototype GUID.");
        if (node.Authored() && !node.Anchor.IsNull())
            return Failure(Error::InvalidState,
                           "An added Node cannot also name an existing Node.");
        if (node.Subgraph && node.Subgraph->Name.empty())
            return Failure(Error::InvalidState,
                           "An added graph Node requires a name.");
        if (!node.Authored() && !node.Query && node.Anchor.IsNull())
            return Failure(Error::QueryNotFound,
                           "A Node query has no semantic identity.");
    }
    std::set<std::uint32_t> parked;
    for (const EditReplace &item : m_Replacements) {
        if (!existingNode(item.Target.Value) || item.Target == Graph() ||
            !addedNode(item.Replacement.Value)) {
            return Failure(
                Error::InvalidState,
                "Replace requires an existing child Node and one replacement Block.");
        }
        if (!parked.insert(item.Target.Value).second) {
            return Failure(Error::InvalidState,
                           "One Edit cannot replace the same Node twice.");
        }
    }
    for (const EditRemove &item : m_Removals) {
        if (!existingNode(item.Target.Value) || item.Target == Graph()) {
            return Failure(Error::InvalidState,
                           "Remove requires an existing child Node.");
        }
        if (!parked.insert(item.Target.Value).second) {
            return Failure(
                Error::InvalidState,
                "One Edit cannot replace and remove the same Node, or remove it twice.");
        }
    }
    if (!parked.empty()) {
        const auto usesParked = [&](const Port &port) {
            return parked.contains(port.Owner);
        };
        for (const Action &action : m_Actions) {
            const Status compatible = std::visit(
                [&](const auto &item) -> Status {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T, EditFlow>) {
                        if (usesParked(item.Source) || usesParked(item.Sink))
                            return Failure(Error::InvalidState,
                                           "A parked Node cannot participate in Flow.");
                    } else if constexpr (std::is_same_v<T, EditBind>) {
                        if (usesParked(item.Target) ||
                            (item.Kind != BindKind::Literal &&
                             usesParked(item.Source)))
                            return Failure(Error::InvalidState,
                                           "A parked Node cannot participate in Bind.");
                    } else if constexpr (std::is_same_v<T, EditPush>) {
                        if (usesParked(item.Source) ||
                            usesParked(item.Destination))
                            return Failure(Error::InvalidState,
                                           "A parked Node cannot participate in Push.");
                    } else if constexpr (std::is_same_v<T, EditInterface>) {
                        if (parked.contains(item.Owner.Value))
                            return Failure(
                                Error::InvalidState,
                                "A parked Node cannot receive an interface edit.");
                    } else if constexpr (std::is_same_v<T, EditTap>) {
                        if (usesParked(item.Source))
                            return Failure(Error::InvalidState,
                                           "A parked Node cannot participate in Tap.");
                    } else if constexpr (
                        std::is_same_v<T, EditSplice> ||
                        std::is_same_v<T, EditRedirect> ||
                        std::is_same_v<T, EditAfter> ||
                        std::is_same_v<T, EditBefore>) {
                        return Failure(
                            Error::InvalidState,
                            "A Node edit cannot share one Patch with Link overlays.");
                    }
                    return {};
                }, action);
            if (!compatible)
                return compatible;
        }
    }
    for (const Operation &item : m_Operations) {
        if (!item.Guid.IsValid() || !item.Result.IsValid() ||
            item.Result == CKPGUID_NONE) {
            return Failure(
                Error::OperationInvalid,
                "A Parameter Operation requires an operation GUID and result type.");
        }
        if (item.Input2.IsValid() && item.Input2 != CKPGUID_NONE &&
            (!item.Input1.IsValid() || item.Input1 == CKPGUID_NONE)) {
            return Failure(
                Error::OperationInvalid,
                "A Parameter Operation cannot have a second input without its first input.");
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
        if (!value)
            return false;
        if (const Operation *owner = operation(value.Owner)) {
            if (value.Selector.UsesName() || value.Selector.RequireOnly)
                return false;
            if (value.Selector.Kind == SlotKind::OutputParameter)
                return value.Selector.Index == 0;
            if (value.Selector.Kind != SlotKind::InputParameter ||
                value.Selector.Index < 0 || value.Selector.Index > 1)
                return false;
            const CKGUID type = value.Selector.Index == 0
                ? owner->Input1 : owner->Input2;
            return type.IsValid() && type != CKPGUID_NONE;
        }
        if (!knownNode(value.Owner))
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
                if (item.Kind == SlotKind::Local &&
                    item.Owner != Graph() && !addedNode(item.Owner.Value))
                    return Failure(Error::InterfaceUnsupported,
                                   "A Local can be appended only to the graph root or a Node added by this Edit.");
                if ((item.Kind == SlotKind::InputParameter ||
                     item.Kind == SlotKind::OutputParameter ||
                     item.Kind == SlotKind::Local) &&
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
    for (const Nested &nested : m_Nested) {
        if (!nested.Scope || nested.Scope == Graph().Value ||
            !knownNode(nested.Parent.Value) || !nested.Body) {
            return Failure(Error::InvalidState,
                           "A nested graph scope has no graph Node.");
        }
        const Status nestedStatus = nested.Body->Validate();
        if (!nestedStatus)
            return nestedStatus;
    }
    return {};
}

bool GraphEdit::UsesIdentity() const noexcept {
    return std::any_of(m_Nodes.begin(), m_Nodes.end(),
                       [](const EditNode &node) {
                           return !node.Anchor.IsNull() ||
                               (node.Block && node.Block->WorldBound());
                       }) ||
        std::any_of(m_Links.begin(), m_Links.end(),
                    [](const EditLink &link) {
                        return !link.Anchor.IsNull();
                    }) ||
        std::any_of(m_Nested.begin(), m_Nested.end(),
                    [](const Nested &nested) {
                        return nested.Body && nested.Body->UsesIdentity();
                    });
}

bool GraphEdit::SameAs(const GraphEdit &other) const noexcept {
    if (m_Nodes != other.m_Nodes ||
        m_Operations != other.m_Operations ||
        m_Links != other.m_Links ||
        m_Paths != other.m_Paths ||
        m_Replacements != other.m_Replacements ||
        m_Removals != other.m_Removals ||
        m_Actions != other.m_Actions ||
        m_Nested.size() != other.m_Nested.size()) {
        return false;
    }
    for (std::size_t index = 0; index < m_Nested.size(); ++index) {
        const Nested &left = m_Nested[index];
        const Nested &right = other.m_Nested[index];
        if (left.Parent != right.Parent || left.Scope != right.Scope ||
            !left.Body || !right.Body || !left.Body->SameAs(*right.Body)) {
            return false;
        }
    }
    return true;
}

Status GraphEdit::Compile(const PatchKey &patch, const ObjectRef &graph,
                          Compiler &compiler, Edit &out,
                          std::map<std::uint32_t, Node> *nodeHandles,
                          bool rootInterfaceExists) const {
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
        if (item.Authored())
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
            if (item.Query.Selector == NodeQuery::Kind::Index &&
                candidate.Index != item.Query.Index)
                continue;
            if (item.Query.Selector == NodeQuery::Kind::Name &&
                candidate.Name != item.Query.Name)
                continue;
            if (item.Query.Selector != NodeQuery::Kind::Name &&
                !item.Query.Name.empty() && candidate.Name != item.Query.Name)
                continue;
            if (item.Query.Prototype.IsValid() &&
                candidate.Prototype != item.Query.Prototype)
                continue;
            if (item.Query.ExpectedKind &&
                candidate.Kind != *item.Query.ExpectedKind)
                continue;
            if (item.Query.PortShape &&
                PortShape(candidate) != item.Query.PortShape)
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
        if (item.Query.Selector == NodeQuery::Kind::Name &&
            !item.Query.Unique) {
            if (item.Query.Occurrence < 0 ||
                item.Query.Occurrence >= static_cast<int>(matches.size()))
                return Failure(Error::QueryNotFound,
                               "A Node name occurrence does not exist.");
            const GraphNode *selected = matches[
                static_cast<std::size_t>(item.Query.Occurrence)];
            matches.assign(1, selected);
        }
        if (matches.size() != 1) {
            std::ostringstream message;
            message << "A Node query matched " << matches.size()
                    << " Nodes; RequireOne cannot choose between them.";
            return Failure(Error::QueryAmbiguous, message.str());
        }

        Node live;
        const auto alias = std::find_if(
            nodes.begin(), nodes.end(), [&](const auto &entry) {
                return entry.second->Id == matches.front()->Id;
            });
        if (alias != nodes.end()) {
            live = liveNodes.at(alias->first);
        } else {
            status = compiler.UseNode(resolved, matches.front()->Object, live);
            if (!status)
                return status;
        }
        nodes.emplace(item.Handle.Value, matches.front());
        liveNodes.emplace(item.Handle.Value, live);
    }

    std::map<std::uint32_t, ParameterOperation> liveOperations;
    for (const Operation &item : m_Operations) {
        const ParameterOperation live = resolved.AddOperation(
            item.Guid, item.Result, item.Input1, item.Input2);
        liveOperations.emplace(item.Handle.Value, live);
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
        if (!item.Authored())
            continue;
        Node live;
        if (item.Block)
            status = compiler.Add(resolved, *item.Block, live);
        else
            status = compiler.AddGraph(
                resolved, item.Subgraph->Name, item.Subgraph->Priority, live);
        if (!status)
            return status;
        liveNodes.emplace(item.Handle.Value, live);
    }

    for (const EditReplace &item : m_Replacements) {
        const auto target = liveNodes.find(item.Target.Value);
        const auto replacement = liveNodes.find(item.Replacement.Value);
        if (target == liveNodes.end() || replacement == liveNodes.end()) {
            return Failure(Error::InvalidState,
                           "A replacement lost one of its Nodes during compilation.");
        }
        resolved.Replace(target->second, replacement->second);
    }
    for (const EditRemove &item : m_Removals) {
        const auto target = liveNodes.find(item.Target.Value);
        if (target == liveNodes.end()) {
            return Failure(
                Error::InvalidState,
                "A removal lost its Node during compilation.");
        }
        resolved.Remove(target->second);
    }

    using PortKey = std::tuple<std::uint32_t, SlotKind, int>;
    std::map<PortKey, Port> liveInterface;
    struct PublishedPort {
        std::uint32_t Owner = 0;
        SlotKind Kind = SlotKind::Input;
        std::string Name;
        int Index = -1;
        int Occurrence = 0;
        Port Live;
    };
    std::vector<PublishedPort> publishedPorts;

    // A graph-backed Behavior exposes the same public interface in two
    // places: as the nested Graph's Entry/Exit/parameter interface, and as
    // ports on the Node owned by its parent Graph. The parent scope creates
    // those CK objects before the nested scope is compiled. Rebind this
    // scope's symbolic appended-port handles to the newly visible ports
    // instead of appending duplicates.
    if (rootInterfaceExists) {
        using InterfaceName = std::pair<SlotKind, std::string>;
        std::map<InterfaceName, int> declared;
        for (const Action &action : m_Actions) {
            const auto *item = std::get_if<EditInterface>(&action);
            if (item && item->Owner == Graph())
                ++declared[{item->Kind, item->Name}];
        }

        std::map<InterfaceName, int> next;
        const GraphNode *rootNode = &*root;
        for (const auto &[name, count] : declared) {
            int matches = 0;
            for (const GraphPort &candidate : rootNode->Ports) {
                if (candidate.Kind == name.first &&
                    candidate.Name == name.second)
                    ++matches;
            }
            if (matches < count) {
                return Failure(
                    Error::GraphChanged,
                    "A nested Graph public port was not created by its parent scope.");
            }
            next.emplace(name, matches - count);
        }

        for (const Action &action : m_Actions) {
            const auto *item = std::get_if<EditInterface>(&action);
            if (!item || item->Owner != Graph())
                continue;
            const InterfaceName name{item->Kind, item->Name};
            const int occurrence = next.at(name)++;
            liveInterface.emplace(
                PortKey{item->Handle.Owner, item->Handle.Selector.Kind,
                        item->Handle.Selector.Index},
                Port{liveNodes.at(Graph().Value).Value,
                     Slot::OccurrenceOf(item->Kind, item->Name, occurrence,
                                        item->Type)});
        }
    }

    // Public ports declared by an immediate nested scope are CK objects on
    // the graph-backed Node in this scope. Create them in this parent Edit so
    // later parent actions can address them and so rollback removes internal
    // child links before removing the public interface.
    for (const Nested &nested : m_Nested) {
        if (!nested.Body)
            return Failure(Error::InvalidState,
                           "A nested Graph Edit has no body.");
        const auto parent = liveNodes.find(nested.Parent.Value);
        if (parent == liveNodes.end())
            return Failure(Error::InvalidState,
                           "A nested Graph Edit lost its parent Node.");
        const auto model = nodes.find(nested.Parent.Value);
        for (const Action &action : nested.Body->m_Actions) {
            const auto *item = std::get_if<EditInterface>(&action);
            if (!item || item->Owner != nested.Body->Graph())
                continue;
            Port live;
            switch (item->Kind) {
            case SlotKind::Input:
                live = resolved.AppendIn(parent->second, item->Name);
                break;
            case SlotKind::Output:
                live = resolved.AppendOut(parent->second, item->Name);
                break;
            case SlotKind::InputParameter:
                live = resolved.AppendPin(parent->second, item->Name,
                                          item->Type);
                break;
            case SlotKind::OutputParameter:
                live = resolved.AppendPout(parent->second, item->Name,
                                           item->Type);
                break;
            case SlotKind::Local:
                live = resolved.AppendLocal(parent->second, item->Name,
                                            item->Type);
                break;
            default:
                return Failure(
                    Error::InterfaceUnsupported,
                    "This nested Graph public port kind is not supported.");
            }
            const int existingKind = model == nodes.end() ? 0
                : static_cast<int>(std::count_if(
                    model->second->Ports.begin(), model->second->Ports.end(),
                    [&](const GraphPort &port) {
                        return port.Kind == item->Kind;
                    }));
            const int index = existingKind + static_cast<int>(std::count_if(
                    publishedPorts.begin(), publishedPorts.end(),
                    [&](const PublishedPort &port) {
                        return port.Owner == nested.Parent.Value &&
                               port.Kind == item->Kind;
                    }));
            const int existingName = model == nodes.end() ? 0
                : static_cast<int>(std::count_if(
                    model->second->Ports.begin(), model->second->Ports.end(),
                    [&](const GraphPort &port) {
                        return port.Kind == item->Kind &&
                               port.Name == item->Name;
                    }));
            const int occurrence = existingName + static_cast<int>(std::count_if(
                    publishedPorts.begin(), publishedPorts.end(),
                    [&](const PublishedPort &port) {
                        return port.Owner == nested.Parent.Value &&
                               port.Kind == item->Kind &&
                               port.Name == item->Name;
                    }));
            publishedPorts.push_back({nested.Parent.Value, item->Kind,
                                      item->Name, index, occurrence,
                                      std::move(live)});
        }
    }

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
        std::vector<const PublishedPort *> published;
        for (const PublishedPort &candidate : publishedPorts) {
            if (candidate.Owner != symbolic.Owner ||
                candidate.Kind != symbolic.Selector.Kind)
                continue;
            if (symbolic.Selector.UsesName() &&
                candidate.Name != symbolic.Selector.Name)
                continue;
            if (!symbolic.Selector.UsesName() &&
                !symbolic.Selector.RequireOnly &&
                candidate.Index != symbolic.Selector.Index)
                continue;
            published.push_back(&candidate);
        }
        if (!published.empty()) {
            const auto model = nodes.find(symbolic.Owner);
            if (symbolic.Selector.UsesName()) {
                const int existing = model == nodes.end() ? 0
                    : static_cast<int>(std::count_if(
                        model->second->Ports.begin(),
                        model->second->Ports.end(),
                        [&](const GraphPort &candidate) {
                            return candidate.Kind == symbolic.Selector.Kind &&
                                   candidate.Name == symbolic.Selector.Name;
                        }));
                const int total = existing + static_cast<int>(published.size());
                if (symbolic.Selector.RequireUnique && total != 1) {
                    return Failure(Error::AmbiguousSlot,
                                   "A published graph port is ambiguous.");
                }
                const int occurrence = symbolic.Selector.RequireUnique
                    ? 0 : symbolic.Selector.Occurrence;
                if (occurrence < 0 || occurrence >= total)
                    return Failure(Error::SlotNotFound,
                                   "A published graph port occurrence does not exist.");
                if (occurrence >= existing) {
                    live = published[static_cast<std::size_t>(
                        occurrence - existing)]->Live;
                    return {};
                }
            } else if (symbolic.Selector.RequireOnly) {
                const int existing = model == nodes.end() ? 0
                    : static_cast<int>(std::count_if(
                        model->second->Ports.begin(),
                        model->second->Ports.end(),
                        [&](const GraphPort &candidate) {
                            return candidate.Kind == symbolic.Selector.Kind;
                        }));
                const int total = existing + static_cast<int>(published.size());
                if (total != 1)
                    return Failure(Error::AmbiguousSlot,
                                   "A graph port selector is not unique.");
                if (!published.empty()) {
                    live = published.front()->Live;
                    return {};
                }
            } else if (!published.empty()) {
                live = published.front()->Live;
                return {};
            }
        }
        const auto owner = liveNodes.find(symbolic.Owner);
        if (owner != liveNodes.end()) {
            live = {owner->second.Value, symbolic.Selector};
            return {};
        }
        const auto operation = liveOperations.find(symbolic.Owner);
        if (operation == liveOperations.end())
            return Failure(Error::InvalidState,
                           "A Graph Edit action names an unknown graph object.");
        live = {operation->second.Value, symbolic.Selector};
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
                if (rootInterfaceExists && item.Owner == Graph())
                    return {};
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

} // namespace BML::Behavior::Internal

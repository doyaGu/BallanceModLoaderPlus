#include "Behavior/Edit/Program.h"

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

template <class Kind, std::size_t Index = 0>
constexpr std::size_t StepIndex() {
    static_assert(Index < std::variant_size_v<Program::Step>);
    if constexpr (std::is_same_v<Kind, std::variant_alternative_t<Index, Program::Step>>)
        return Index;
    else
        return StepIndex<Kind, Index + 1>();
}

// Instantiate only the requested kinds, preserving authored order and the
// first failure rather than expanding every visitor over the whole variant.
template <class Kind, class Visit>
bool VisitStep(const Program::Step &step, Visit &visit, Status &status) {
    const auto *item = std::get_if<Kind>(&step);
    if (!item)
        return false;
    status = visit(*item);
    return true;
}

template <class... Kinds, class Visit>
Status EachStep(const std::vector<Program::Step> &steps, Visit &&visit) {
    for (const Program::Step &step : steps) {
        if (step.valueless_by_exception())
            throw std::bad_variant_access();
        Status status;
        (VisitStep<Kinds>(step, visit, status) || ...);
        if (!status)
            return status;
    }
    return {};
}

} // namespace

Program::Program() noexcept = default;
Program::~Program() = default;
Program::Program(Program &&) noexcept = default;
Program &Program::operator=(Program &&) noexcept = default;

Port Program::Entry(int index) const { return Graph().In(index); }

Port Program::Entry(std::string name) const {
    return Graph().In(std::move(name));
}

Port Program::Exit(int index) const { return Graph().Out(index); }

Port Program::Exit(std::string name) const {
    return Graph().Out(std::move(name));
}

Node Program::RequireOne(NodePattern pattern) {
    const Node node{NextNode()};
    m_Steps.push_back(
        Steps::QueryNode{node, std::move(pattern), false, std::nullopt});
    return node;
}

Node Program::Each(NodePattern pattern) {
    const Node node{NextNode()};
    m_Steps.push_back(
        Steps::QueryNode{node, std::move(pattern), true, std::nullopt});
    return node;
}

Node Program::Next(Port source) {
    return Next(std::move(source), {});
}

Node Program::Next(Port source, NodePattern expected) {
    const Node node{NextNode()};
    m_Steps.push_back(Steps::QueryNode{
        node, std::move(expected), false,
        Steps::RelatedNode{Steps::NodeRelation::Next, std::move(source)}});
    return node;
}

Node Program::Previous(Port sink) {
    return Previous(std::move(sink), {});
}

Node Program::Previous(Port sink, NodePattern expected) {
    const Node node{NextNode()};
    m_Steps.push_back(Steps::QueryNode{
        node, std::move(expected), false,
        Steps::RelatedNode{Steps::NodeRelation::Previous, std::move(sink)}});
    return node;
}

Steps::QueryNode *Program::FindQuery(std::uint32_t node) noexcept {
    for (Step &step : m_Steps) {
        auto *query = std::get_if<Steps::QueryNode>(&step);
        if (query && query->Handle.Value == node)
            return query;
    }
    return nullptr;
}

Status Program::Count(Node node, SlotKind kind, int count) {
    Steps::QueryNode *query = FindQuery(node.Value);
    if (!query) {
        return Failure(Error::InvalidState,
                       "A port-count condition requires a Node Pattern.");
    }
    query->Pattern.PortCounts.push_back({kind, count});
    return {};
}

Status Program::Observe(Port port, Value expected) {
    Steps::QueryNode *query = FindQuery(port.Owner);
    if (!query) {
        return Failure(Error::InvalidState,
                       "A value condition requires a port of a Node Pattern.");
    }
    query->Pattern.PortValues.push_back(
        {std::move(port.Selector), std::move(expected)});
    return {};
}

Node Program::UseNode(const ObjectRef &node) {
    const Node handle{NextNode()};
    m_Steps.push_back(Steps::UseNode{handle, node});
    return handle;
}

Link Program::UseLink(const ObjectRef &link) {
    const Link handle{NextLink()};
    m_Steps.push_back(Steps::UseLink{handle, link});
    return handle;
}

Link Program::RequireOne(Port source, Port sink, std::optional<int> delay) {
    const Link link{NextLink()};
    m_Steps.push_back(Steps::QueryLink{
        link, Steps::LinkRelation::Between, std::move(source),
        std::move(sink), {}, delay});
    return link;
}

Link Program::Leaving(Port source) {
    const Link link{NextLink()};
    m_Steps.push_back(Steps::QueryLink{
        link, Steps::LinkRelation::Leaving, std::move(source), Port(), {},
        std::nullopt});
    return link;
}

Link Program::Entering(Port sink) {
    const Link link{NextLink()};
    m_Steps.push_back(Steps::QueryLink{
        link, Steps::LinkRelation::Entering, Port(), std::move(sink), {},
        std::nullopt});
    return link;
}

Link Program::To(Port source, Node target) {
    const Link link{NextLink()};
    m_Steps.push_back(Steps::QueryLink{
        link, Steps::LinkRelation::To, std::move(source), Port(), target,
        std::nullopt});
    return link;
}

PathRef Program::Follow(Port start) {
    const PathRef path{NextPath()};
    m_Steps.push_back(Steps::Follow{path, std::move(start)});
    return path;
}

Node Program::Add(CKGUID prototype) {
    return Add(BlockSpec(prototype));
}

Node Program::Add(PrototypeRef prototype) {
    BlockSpec block(prototype.Guid);
    block.PrototypeGeneration(prototype.Generation);
    return Add(std::move(block));
}

Node Program::Add(BlockSpec block) {
    const Node node{NextNode()};
    m_Steps.push_back(Steps::AddBlock{node, std::move(block)});
    return node;
}

Node Program::AddGraph(std::string name, int priority) {
    const Node node{NextNode()};
    m_Steps.push_back(
        Steps::AddGraph{node, GraphSpec{std::move(name), priority}});
    return node;
}

Program &Program::Enter(Node node, std::uint32_t scope) {
    const auto found = std::find_if(
        m_Nested.begin(), m_Nested.end(),
        [&](const Nested &candidate) { return candidate.Scope == scope; });
    if (found != m_Nested.end())
        return *found->Body;
    Nested nested;
    nested.Parent = node;
    nested.Scope = scope;
    nested.Body = std::make_unique<Program>();
    m_Nested.push_back(std::move(nested));
    return *m_Nested.back().Body;
}

Node Program::Replace(Node target, BlockSpec block) {
    const Node replacement = Add(std::move(block));
    m_Steps.push_back(Steps::Replace{target, replacement});
    return replacement;
}

void Program::Remove(Node target) {
    m_Steps.push_back(Steps::Remove{target});
}

ParameterOperation Program::AddOperation(
    CKGUID operation, CKGUID result, CKGUID input1, CKGUID input2) {
    const ParameterOperation handle{NextNode()};
    m_Steps.push_back(
        Steps::AddOperation{handle, operation, result, input1, input2});
    return handle;
}

void Program::Flow(Port source, Port sink, int delay, Cycle cycle) {
    m_Steps.push_back(
        Steps::Flow{std::move(source), std::move(sink), delay, cycle});
}

void Program::Flow(Port source, HookBlock::Hook hook, Port sink) {
    m_Steps.push_back(Steps::HookFlow{
        std::move(source), std::move(sink), std::move(hook)});
}

void Program::Set(Port target, Parameter::Binding value) {
    m_Steps.push_back(Steps::Set{std::move(target), std::move(value)});
}

void Program::Bind(Port target, Parameter::Binding value) {
    m_Steps.push_back(Steps::Bind{
        std::move(target), BindKind::Literal, std::move(value), Port()});
}

void Program::Bind(Port target, Port source) {
    m_Steps.push_back(Steps::Bind{
        std::move(target), BindKind::Direct, {}, std::move(source)});
}

void Program::Share(Port target, Port source) {
    m_Steps.push_back(Steps::Bind{
        std::move(target), BindKind::Shared, {}, std::move(source)});
}

void Program::Push(Port source, Port destination) {
    m_Steps.push_back(
        Steps::Push{std::move(source), std::move(destination)});
}

void Program::Tap(Port source, HookBlock::Hook hook) {
    m_Steps.push_back(Steps::Tap{std::move(source), std::move(hook)});
}

void Program::After(PathRef path, HookBlock::Hook hook) {
    m_Steps.push_back(Steps::After{path, std::move(hook)});
}

void Program::Before(Link target, HookBlock::Hook hook) {
    m_Steps.push_back(Steps::Before{target, std::move(hook)});
}

void Program::Splice(Link target, Node block, std::vector<Order> ordering) {
    Splice(target, block.In(), block.Out(), std::move(ordering));
}

void Program::Splice(Link target, Port input, Port output,
                     std::vector<Order> ordering) {
    m_Steps.push_back(Steps::Splice{
        target, std::move(input), std::move(output), std::move(ordering)});
}

void Program::Redirect(Link target, Port sink, std::vector<Order> ordering) {
    m_Steps.push_back(
        Steps::Redirect{target, std::move(sink), std::move(ordering)});
}

void Program::Redirect(Link target, Link destination,
                       std::vector<Order> ordering) {
    m_Steps.push_back(
        Steps::RedirectToLink{target, destination, std::move(ordering)});
}

void Program::Reconnect(Link target, Port source, Port sink, Cycle cycle) {
    m_Steps.push_back(Steps::Reconnect{
        target, std::move(source), std::move(sink), cycle});
}

Port Program::AppendIn(Node node, std::string name) {
    return Append(node, SlotKind::Input, std::move(name), CKGUID());
}

Port Program::AppendOut(Node node, std::string name) {
    return Append(node, SlotKind::Output, std::move(name), CKGUID());
}

Port Program::AppendPin(Node node, std::string name, CKGUID type) {
    return Append(node, SlotKind::InputParameter, std::move(name), type);
}

Port Program::AppendPout(Node node, std::string name, CKGUID type) {
    return Append(node, SlotKind::OutputParameter, std::move(name), type);
}

Port Program::AppendLocal(Node node, std::string name, CKGUID type) {
    return Append(node, SlotKind::Local, std::move(name), type);
}

Port Program::Append(Node node, SlotKind kind, std::string name,
                     CKGUID type) {
    const int identity = -1 - static_cast<int>(m_NextInterface++);
    Port handle{node.Value, Slot::At(kind, identity, type)};
    m_Steps.push_back(Steps::Append{handle, node, kind, std::move(name), type});
    return handle;
}

Status Program::Validate() const {
    struct NodeDefinition {
        bool Authored = false;
        bool Many = false;
    };
    std::map<std::uint32_t, NodeDefinition> defined;
    std::map<std::uint32_t, const Steps::AddOperation *> operations;
    std::set<std::uint32_t> paths;
    for (const Step &step : m_Steps) {
        if (const auto *query = std::get_if<Steps::QueryNode>(&step)) {
            defined.emplace(query->Handle.Value,
                            NodeDefinition{false, query->Many});
        } else if (const auto *use = std::get_if<Steps::UseNode>(&step)) {
            defined.emplace(use->Handle.Value, NodeDefinition{});
        } else if (const auto *block = std::get_if<Steps::AddBlock>(&step)) {
            defined.emplace(block->Handle.Value, NodeDefinition{true, false});
        } else if (const auto *graph = std::get_if<Steps::AddGraph>(&step)) {
            defined.emplace(graph->Handle.Value, NodeDefinition{true, false});
        } else if (const auto *operation =
                       std::get_if<Steps::AddOperation>(&step)) {
            operations.emplace(operation->Handle.Value, operation);
        } else if (const auto *follow = std::get_if<Steps::Follow>(&step)) {
            paths.insert(follow->Handle.Value);
        }
    }
    const auto find = [&](std::uint32_t value) -> const NodeDefinition * {
        const auto found = defined.find(value);
        return found == defined.end() ? nullptr : &found->second;
    };
    const auto knownNode = [&](std::uint32_t value) {
        return value == Graph().Value || find(value) != nullptr;
    };
    const auto existingNode = [&](std::uint32_t value) {
        if (value == Graph().Value)
            return true;
        const NodeDefinition *found = find(value);
        return found && !found->Authored;
    };
    const auto manyNode = [&](std::uint32_t value) {
        const NodeDefinition *found = find(value);
        return found && found->Many;
    };
    const auto singleNode = [&](std::uint32_t value) {
        return existingNode(value) && !manyNode(value);
    };
    const auto addedNode = [&](std::uint32_t value) {
        const NodeDefinition *found = find(value);
        return found && found->Authored;
    };
    const auto operation = [&](std::uint32_t value)
        -> const Steps::AddOperation * {
        const auto found = operations.find(value);
        return found == operations.end() ? nullptr : found->second;
    };

    Status status = EachStep<Steps::QueryNode, Steps::UseNode,
                             Steps::AddBlock, Steps::AddGraph>(
        m_Steps, [&](const auto &item) -> Status {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, Steps::AddBlock>) {
                if (!item.Block.Prototype().IsValid())
                    return Failure(Error::PrototypeNotFound,
                                   "An added Block requires a Prototype GUID.");
            } else if constexpr (std::is_same_v<T, Steps::AddGraph>) {
                if (item.Graph.Name.empty())
                    return Failure(Error::InvalidState,
                                   "An added graph Node requires a name.");
            } else if constexpr (std::is_same_v<T, Steps::UseNode>) {
                if (item.Anchor.IsNull())
                    return Failure(
                        Error::InvalidState,
                        "A symbolic Node must have exactly one definition.");
            } else {
                if (!item.Related && !item.Pattern)
                    return Failure(
                        Error::InvalidState,
                        "A symbolic Node must have exactly one definition.");
                if (item.Related) {
                    const Port &endpoint = item.Related->Endpoint;
                    const bool knownOwner = singleNode(endpoint.Owner);
                    const bool usableSlot = endpoint.Selector.UsesName() ||
                        endpoint.Selector.RequireOnly ||
                        endpoint.Selector.Index >= 0;
                    const bool validDirection =
                        (item.Related->Relation == Steps::NodeRelation::Next &&
                         ((endpoint.Owner == Graph().Value &&
                           endpoint.Selector.Kind == SlotKind::Input) ||
                          (endpoint.Owner != Graph().Value &&
                           endpoint.Selector.Kind == SlotKind::Output))) ||
                        (item.Related->Relation ==
                             Steps::NodeRelation::Previous &&
                         ((endpoint.Owner == Graph().Value &&
                           endpoint.Selector.Kind == SlotKind::Output) ||
                          (endpoint.Owner != Graph().Value &&
                           endpoint.Selector.Kind == SlotKind::Input)));
                    if (!endpoint || !knownOwner || !usableSlot ||
                        !validDirection) {
                        return Failure(
                            Error::InvalidState,
                            "A related Node requires an existing Out or In port.");
                    }
                    if (item.Pattern)
                        return item.Pattern.Validate();
                } else {
                    return item.Pattern.Validate();
                }
            }
            return {};
        });
    if (!status)
        return status;

    std::set<std::uint32_t> parked;
    status = EachStep<Steps::Replace>(
        m_Steps, [&](const Steps::Replace &item) -> Status {
            if (!singleNode(item.Target.Value) || item.Target == Graph() ||
                !addedNode(item.Replacement.Value)) {
                return Failure(
                    Error::InvalidState,
                    "Replace requires an existing child Node and one replacement Block.");
            }
            if (!parked.insert(item.Target.Value).second) {
                return Failure(Error::InvalidState,
                               "One Edit cannot replace the same Node twice.");
            }
            return {};
        });
    if (!status)
        return status;
    status = EachStep<Steps::Remove>(
        m_Steps, [&](const Steps::Remove &item) -> Status {
            if (!singleNode(item.Target.Value) || item.Target == Graph()) {
                return Failure(Error::InvalidState,
                               "Remove requires an existing child Node.");
            }
            if (!parked.insert(item.Target.Value).second) {
                return Failure(
                    Error::InvalidState,
                    "One Edit cannot replace and remove the same Node, or remove it twice.");
            }
            return {};
        });
    if (!status)
        return status;
    if (!parked.empty()) {
        const auto usesParked = [&](const Port &port) {
            return parked.contains(port.Owner);
        };
        for (const Step &step : m_Steps) {
            status = std::visit(
                [&](const auto &item) -> Status {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T, Steps::Flow> ||
                                  std::is_same_v<T, Steps::HookFlow>) {
                        if (usesParked(item.Source) || usesParked(item.Sink))
                            return Failure(Error::InvalidState,
                                           "A parked Node cannot participate in Flow.");
                    } else if constexpr (std::is_same_v<T, Steps::Set>) {
                        if (usesParked(item.Target))
                            return Failure(Error::InvalidState,
                                           "A parked Node cannot participate in Set.");
                    } else if constexpr (std::is_same_v<T, Steps::Bind>) {
                        if (usesParked(item.Target) ||
                            (item.Kind != BindKind::Literal &&
                             usesParked(item.Source)))
                            return Failure(Error::InvalidState,
                                           "A parked Node cannot participate in Bind.");
                    } else if constexpr (std::is_same_v<T, Steps::Push>) {
                        if (usesParked(item.Source) ||
                            usesParked(item.Destination))
                            return Failure(Error::InvalidState,
                                           "A parked Node cannot participate in Push.");
                    } else if constexpr (std::is_same_v<T, Steps::Append>) {
                        if (parked.contains(item.Owner.Value))
                            return Failure(
                                Error::InvalidState,
                                "A parked Node cannot receive an interface edit.");
                    } else if constexpr (std::is_same_v<T, Steps::Tap>) {
                        if (usesParked(item.Source))
                            return Failure(Error::InvalidState,
                                           "A parked Node cannot participate in Tap.");
                    } else if constexpr (
                        std::is_same_v<T, Steps::Splice> ||
                        std::is_same_v<T, Steps::Redirect> ||
                        std::is_same_v<T, Steps::RedirectToLink> ||
                        std::is_same_v<T, Steps::Reconnect> ||
                        std::is_same_v<T, Steps::After> ||
                        std::is_same_v<T, Steps::Before>) {
                        return Failure(
                            Error::InvalidState,
                            "A structural edit cannot share one Patch with Link overlays.");
                    }
                    return {};
                },
                step);
            if (!status)
                return status;
        }
    }
    status = EachStep<Steps::AddOperation>(
        m_Steps, [&](const Steps::AddOperation &item) -> Status {
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
            return {};
        });
    if (!status)
        return status;
    status = EachStep<Steps::QueryLink, Steps::UseLink>(
        m_Steps, [&](const auto &item) -> Status {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, Steps::UseLink>) {
                if (item.Anchor.IsNull())
                    return Failure(
                        Error::InvalidState,
                        "A Link relation requires existing graph ports and Nodes.");
                return {};
            } else {
                const auto existingPort = [&](const Port &port, bool source) {
                    const SlotKind expected = source
                        ? (port.Owner == Graph().Value
                               ? SlotKind::Input : SlotKind::Output)
                        : (port.Owner == Graph().Value
                               ? SlotKind::Output : SlotKind::Input);
                    return port && singleNode(port.Owner) &&
                        port.Selector.Kind == expected &&
                        (port.Selector.UsesName() ||
                         port.Selector.RequireOnly ||
                         port.Selector.Index >= 0);
                };
                bool valid = false;
                switch (item.Relation) {
                case Steps::LinkRelation::Between:
                    valid = existingPort(item.Source, true) &&
                        existingPort(item.Sink, false) && !item.Target;
                    break;
                case Steps::LinkRelation::Leaving:
                    valid = existingPort(item.Source, true) &&
                        !item.Sink && !item.Target && !item.Delay;
                    break;
                case Steps::LinkRelation::Entering:
                    valid = existingPort(item.Sink, false) &&
                        !item.Source && !item.Target && !item.Delay;
                    break;
                case Steps::LinkRelation::To:
                    valid = existingPort(item.Source, true) &&
                        singleNode(item.Target.Value) && !item.Sink &&
                        !item.Delay;
                    break;
                }
                if (!valid) {
                    return Failure(
                        Error::InvalidState,
                        "A Link relation requires existing graph ports and Nodes.");
                }
                if (item.Relation == Steps::LinkRelation::Between &&
                    item.Delay &&
                    (*item.Delay < 0 || *item.Delay >= 32765)) {
                    return Failure(
                        Error::InvalidDelay,
                        "A Link delay must be between 0 and 32764 frames.");
                }
                return {};
            }
        });
    if (!status)
        return status;
    status = EachStep<Steps::Follow>(
        m_Steps, [&](const Steps::Follow &item) -> Status {
            const bool rootEntry = item.Start.Owner == Graph().Value &&
                item.Start.Selector.Kind == SlotKind::Input;
            const bool nodeOut = item.Start.Owner != Graph().Value &&
                item.Start.Selector.Kind == SlotKind::Output;
            if (!item.Handle || !item.Start ||
                !singleNode(item.Start.Owner) ||
                (!item.Start.Selector.UsesName() &&
                 !item.Start.Selector.RequireOnly &&
                 item.Start.Selector.Index < 0) ||
                (!rootEntry && !nodeOut)) {
                return Failure(
                    Error::InvalidState,
                    "A Path must begin at a graph Entry or existing node Out.");
            }
            return {};
        });
    if (!status)
        return status;

    using SymbolKey = std::tuple<std::uint32_t, SlotKind, int>;
    std::set<SymbolKey> interface;
    const auto port = [&](const Port &value) {
        if (!value)
            return false;
        if (const Steps::AddOperation *owner = operation(value.Owner)) {
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
    for (const Step &step : m_Steps) {
        status = [&]() -> Status {
            switch (step.index()) {
            case StepIndex<Steps::Flow>(): {
                const auto &item = std::get<Steps::Flow>(step);
                if (item.Delay < 0 || item.Delay >= 32765)
                    return Failure(
                        Error::InvalidDelay,
                        "A Flow delay must be between 0 and 32764 frames.");
                if (!port(item.Source) || !port(item.Sink))
                    return Failure(Error::InvalidState,
                                   "A Flow names an unknown Node.");
                break;
            }
            case StepIndex<Steps::HookFlow>(): {
                const auto &item = std::get<Steps::HookFlow>(step);
                if (!port(item.Source) || !port(item.Sink) || !item.Hook) {
                    return Failure(
                        Error::InvalidState,
                        "A callback Flow requires a source, sink, and callback.");
                }
                break;
            }
            case StepIndex<Steps::Set>(): {
                const auto &item = std::get<Steps::Set>(step);
                if (!port(item.Target))
                    return Failure(Error::InvalidState,
                                   "A Set names an unknown Node.");
                if (item.Value.Kind() == Parameter::BindingKind::Value &&
                    item.Value.Literal().IsNull() &&
                    !item.Value.Type().IsValid()) {
                    return Failure(
                        Error::TypeMismatch,
                        "A null Value requires a Virtools type GUID.");
                }
                break;
            }
            case StepIndex<Steps::Bind>(): {
                const auto &item = std::get<Steps::Bind>(step);
                if (!port(item.Target) ||
                    (item.Kind != BindKind::Literal && !port(item.Source))) {
                    return Failure(Error::InvalidState,
                                   "A Bind names an unknown Node.");
                }
                if (item.Kind == BindKind::Literal &&
                    item.Value.Kind() == Parameter::BindingKind::Value &&
                    item.Value.Literal().IsNull() &&
                    !item.Value.Type().IsValid()) {
                    return Failure(
                        Error::TypeMismatch,
                        "A null Value requires a Virtools type GUID.");
                }
                break;
            }
            case StepIndex<Steps::Push>(): {
                const auto &item = std::get<Steps::Push>(step);
                if (!port(item.Source) || !port(item.Destination))
                    return Failure(Error::InvalidState,
                                   "A Push names an unknown Node.");
                break;
            }
            case StepIndex<Steps::Splice>(): {
                const auto &item = std::get<Steps::Splice>(step);
                if (!item.Target || !port(item.Input) || !port(item.Output))
                    return Failure(Error::InvalidState,
                                   "A Splice names an unknown Link or Node.");
                if (manyNode(item.Input.Owner) || manyNode(item.Output.Owner))
                    return Failure(
                        Error::InvalidState,
                        "A Splice rewires one Link and cannot use a port of an Each Node.");
                break;
            }
            case StepIndex<Steps::Redirect>(): {
                const auto &item = std::get<Steps::Redirect>(step);
                if (!item.Target || !port(item.Sink))
                    return Failure(Error::InvalidState,
                                   "A Redirect names an unknown Link or Node.");
                if (manyNode(item.Sink.Owner))
                    return Failure(
                        Error::InvalidState,
                        "A Redirect rewires one Link and cannot use a port of an Each Node.");
                break;
            }
            case StepIndex<Steps::RedirectToLink>(): {
                const auto &item = std::get<Steps::RedirectToLink>(step);
                if (!item.Target || !item.Destination)
                    return Failure(
                        Error::InvalidState,
                        "A Redirect requires source and destination Links.");
                break;
            }
            case StepIndex<Steps::Reconnect>(): {
                const auto &item = std::get<Steps::Reconnect>(step);
                if (!item.Target || !port(item.Source) || !port(item.Sink))
                    return Failure(
                        Error::InvalidState,
                        "A Reconnect requires a Link, source, and destination from this Graph Edit.");
                if (manyNode(item.Source.Owner) || manyNode(item.Sink.Owner))
                    return Failure(
                        Error::InvalidState,
                        "A Reconnect rewires one Link and cannot use a port of an Each Node.");
                break;
            }
            case StepIndex<Steps::Append>(): {
                const auto &item = std::get<Steps::Append>(step);
                if (!knownNode(item.Owner.Value) ||
                    manyNode(item.Owner.Value) || item.Name.empty())
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
                break;
            }
            case StepIndex<Steps::Tap>(): {
                const auto &item = std::get<Steps::Tap>(step);
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
                break;
            }
            case StepIndex<Steps::After>(): {
                const auto &item = std::get<Steps::After>(step);
                if (!paths.contains(item.Target.Value) || !item.Hook) {
                    return Failure(
                        Error::InvalidState,
                        "After requires a Path from this Graph Edit and a callback.");
                }
                break;
            }
            case StepIndex<Steps::Before>(): {
                const auto &item = std::get<Steps::Before>(step);
                if (!item.Target || !item.Hook) {
                    return Failure(
                        Error::InvalidState,
                        "Before requires a Link from this Graph Edit and a callback.");
                }
                break;
            }
            default:
                break;
            }
            return {};
        }();
        if (!status)
            return status;
    }
    for (const Nested &nested : m_Nested) {
        if (!nested.Scope || nested.Scope == Graph().Value ||
            !knownNode(nested.Parent.Value) || manyNode(nested.Parent.Value) ||
            !nested.Body) {
            return Failure(Error::InvalidState,
                           "A nested graph scope has no graph Node.");
        }
        const Status nestedStatus = nested.Body->Validate();
        if (!nestedStatus)
            return nestedStatus;
    }
    return {};
}

bool Program::UsesIdentity() const noexcept {
    return std::any_of(
               m_Steps.begin(), m_Steps.end(),
               [](const Step &step) {
                   if (const auto *node = std::get_if<Steps::UseNode>(&step))
                       return !node->Anchor.IsNull();
                   if (const auto *link = std::get_if<Steps::UseLink>(&step))
                       return !link->Anchor.IsNull();
                   if (const auto *block = std::get_if<Steps::AddBlock>(&step))
                       return block->Block.WorldBound();
                   if (const auto *set = std::get_if<Steps::Set>(&step))
                       return set->Value.Kind() !=
                           Parameter::BindingKind::Value;
                   const auto *bind = std::get_if<Steps::Bind>(&step);
                   return bind && bind->Kind == BindKind::Literal &&
                       bind->Value.Kind() != Parameter::BindingKind::Value;
               }) ||
        std::any_of(m_Nested.begin(), m_Nested.end(),
                    [](const Nested &nested) {
                        return nested.Body && nested.Body->UsesIdentity();
                    });
}

bool Program::SameAs(const Program &other) const noexcept {
    if (m_Steps != other.m_Steps || m_Nested.size() != other.m_Nested.size())
        return false;
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

Status Program::Resolve(const PatchKey &patch, const ObjectRef &graph,
                        Resolver &resolver, Ops &out,
                        ResolvedSymbols *symbols,
                        bool rootInterfaceExists) const {
    if (symbols)
        *symbols = {};
    out = {};
    if (patch.Owner.empty() || patch.Name.empty() || graph.IsNull())
        return Failure(Error::InvalidState,
                       "A Graph Edit requires a patch key and live graph.");

    Status status = Validate();
    if (!status)
        return status;

    GraphModel base;
    Ops resolved;
    status = resolver.Begin(patch, graph, resolved, base);
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
    std::map<std::uint32_t, std::vector<Node>> manyLiveNodes;
    std::map<std::uint64_t, Node> nodeCache;
    nodes.emplace(Graph().Value, &*root);
    liveNodes.emplace(Graph().Value, resolved.Graph());
    nodeCache.emplace(root->Id, resolved.Graph());

    const auto useNode = [&](const GraphNode &model, Node &live) -> Status {
        const auto cached = nodeCache.find(model.Id);
        if (cached != nodeCache.end()) {
            live = cached->second;
            return {};
        }
        Status current = resolver.UseNode(resolved, model.Object, live);
        if (current)
            nodeCache.emplace(model.Id, live);
        return current;
    };

    // Existing Nodes resolve against the unchanged graph first.
    status = EachStep<Steps::QueryNode, Steps::UseNode>(
        m_Steps, [&](const auto &item) -> Status {
            using T = std::decay_t<decltype(item)>;
            std::vector<const GraphNode *> matches;
            bool many = false;
            if constexpr (std::is_same_v<T, Steps::UseNode>) {
                for (const GraphNode &candidate : base.Nodes) {
                    if (candidate.Parent == root->Id &&
                        candidate.Object == item.Anchor)
                        matches.push_back(&candidate);
                }
                if (matches.empty())
                    return Failure(
                        Error::InvalidGraphLocality,
                        "A Node named by identity is not in the target graph.");
            } else {
                many = item.Many;
                if (item.Related) {
                    const auto owner = nodes.find(item.Related->Endpoint.Owner);
                    if (owner == nodes.end()) {
                        return Failure(
                            Error::InvalidState,
                            "A related Node names a Node that has not been resolved.");
                    }
                    GraphEndpoint endpoint;
                    Status current = ResolvePort(
                        *owner->second, item.Related->Endpoint.Selector,
                        endpoint);
                    if (!current)
                        return current;

                    std::vector<const GraphNode *> expected;
                    if (item.Pattern) {
                        current = ResolveAll(base, root->Id, item.Pattern,
                                             resolver, expected);
                        if (!current)
                            return current;
                    }

                    struct Relation {
                        const GraphLink *Link = nullptr;
                        const GraphNode *Node = nullptr;
                    };
                    const bool next = item.Related->Relation ==
                        Steps::NodeRelation::Next;
                    std::vector<Relation> relations;
                    for (const GraphLink &candidate : base.Links) {
                        const bool related = next
                            ? candidate.Source == endpoint
                            : candidate.Target == endpoint;
                        if (!related)
                            continue;
                        const std::uint64_t relatedNode = next
                            ? candidate.Target.Node : candidate.Source.Node;
                        const auto node = std::find_if(
                            base.Nodes.begin(), base.Nodes.end(),
                            [&](const GraphNode &value) {
                                return value.Id == relatedNode &&
                                    (value.Id == root->Id ||
                                     value.Parent == root->Id);
                            });
                        if (node == base.Nodes.end()) {
                            return Failure(
                                Error::GraphChanged,
                                "A connecting Link ends outside the current graph.");
                        }
                        if (item.Pattern && std::none_of(
                                expected.begin(), expected.end(),
                                [&](const GraphNode *value) {
                                    return value->Id == node->Id;
                                })) {
                            continue;
                        }
                        relations.push_back({&candidate, &*node});
                    }
                    if (relations.empty()) {
                        return Failure(
                            item.Pattern ? Error::QueryNotFound
                                         : Error::LinkNotFound,
                            item.Pattern
                                ? "No connecting Link reaches a Node matching the Pattern."
                                : "A related Node has no connecting Link.");
                    }
                    if (relations.size() != 1) {
                        return Failure(
                            Error::QueryAmbiguous,
                            item.Pattern
                                ? "More than one connecting Link reaches a Node matching the Pattern."
                                : "A related Node has more than one connecting Link.");
                    }
                    matches.push_back(relations.front().Node);
                } else {
                    Status current = item.Many
                        ? ResolveAll(base, root->Id, item.Pattern, resolver,
                                     matches)
                        : Internal::Resolve(base, root->Id, item.Pattern,
                                            resolver, matches);
                    if (!current)
                        return current;
                }
                if (matches.empty())
                    return Failure(Error::QueryNotFound,
                                   "A Node Pattern matched no Node.");
            }

            if (many) {
                std::vector<Node> live;
                live.reserve(matches.size());
                for (const GraphNode *match : matches) {
                    Node node;
                    Status current = useNode(*match, node);
                    if (!current)
                        return current;
                    live.push_back(node);
                }
                manyLiveNodes.emplace(item.Handle.Value, std::move(live));
                return {};
            }

            Node live;
            Status current = useNode(*matches.front(), live);
            if (!current)
                return current;
            nodes.emplace(item.Handle.Value, matches.front());
            liveNodes.emplace(item.Handle.Value, live);
            return {};
        });
    if (!status)
        return status;

    std::map<std::uint32_t, ParameterOperation> liveOperations;
    (void) EachStep<Steps::AddOperation>(
        m_Steps, [&](const Steps::AddOperation &item) -> Status {
            const ParameterOperation live = resolved.AddOperation(
                item.Guid, item.Result, item.Input1, item.Input2);
            liveOperations.emplace(item.Handle.Value, live);
            return {};
        });

    std::map<std::uint32_t, Link> liveLinks;
    std::map<std::uint32_t, const GraphLink *> modelLinks;
    status = EachStep<Steps::QueryLink, Steps::UseLink>(
        m_Steps, [&](const auto &item) -> Status {
            using T = std::decay_t<decltype(item)>;
            const GraphLink *match = nullptr;
            if constexpr (std::is_same_v<T, Steps::UseLink>) {
                const auto found = std::find_if(
                    base.Links.begin(), base.Links.end(),
                    [&](const GraphLink &candidate) {
                        return candidate.Object == item.Anchor;
                    });
                if (found == base.Links.end())
                    return Failure(
                        Error::LinkNotFound,
                        "A Link named by identity is not in the target graph.");
                match = &*found;
            } else {
                GraphEndpoint source;
                GraphEndpoint sink;
                if (item.Relation != Steps::LinkRelation::Entering) {
                    const auto sourceNode = nodes.find(item.Source.Owner);
                    if (sourceNode == nodes.end()) {
                        return Failure(
                            Error::InvalidState,
                            "A Link relation names an unresolved source Node.");
                    }
                    Status current = ResolvePort(
                        *sourceNode->second, item.Source.Selector, source);
                    if (!current)
                        return current;
                }
                if (item.Relation == Steps::LinkRelation::Between ||
                    item.Relation == Steps::LinkRelation::Entering) {
                    const auto sinkNode = nodes.find(item.Sink.Owner);
                    if (sinkNode == nodes.end()) {
                        return Failure(
                            Error::InvalidState,
                            "A Link relation names an unresolved target Node.");
                    }
                    Status current = ResolvePort(
                        *sinkNode->second, item.Sink.Selector, sink);
                    if (!current)
                        return current;
                }
                const GraphNode *target = nullptr;
                if (item.Relation == Steps::LinkRelation::To) {
                    const auto found = nodes.find(item.Target.Value);
                    if (found == nodes.end()) {
                        return Failure(
                            Error::InvalidState,
                            "A Link relation names an unresolved target Node.");
                    }
                    target = found->second;
                }

                std::vector<const GraphLink *> matches;
                for (const GraphLink &candidate : base.Links) {
                    bool matched = false;
                    switch (item.Relation) {
                    case Steps::LinkRelation::Between:
                        matched = candidate.Source == source &&
                            candidate.Target == sink &&
                            (!item.Delay ||
                             candidate.InitialDelay == *item.Delay);
                        break;
                    case Steps::LinkRelation::Leaving:
                        matched = candidate.Source == source;
                        break;
                    case Steps::LinkRelation::Entering:
                        matched = candidate.Target == sink;
                        break;
                    case Steps::LinkRelation::To:
                        matched = candidate.Source == source && target &&
                            candidate.Target.Node == target->Id;
                        break;
                    }
                    if (!matched)
                        continue;
                    matches.push_back(&candidate);
                }
                if (matches.empty()) {
                    return Failure(Error::LinkNotFound,
                                   "A Link relation matched no Link.");
                }
                if (matches.size() != 1) {
                    std::ostringstream message;
                    message << "A Link relation matched " << matches.size()
                            << " Links; use a stronger relation or endpoint selector.";
                    return Failure(Error::QueryAmbiguous, message.str());
                }
                match = matches.front();
            }
            Link live;
            Status current = resolver.UseLink(resolved, match->Object, live);
            if (!current)
                return current;
            liveLinks.emplace(item.Handle.Value, live);
            modelLinks.emplace(item.Handle.Value, match);
            return {};
        });
    if (!status)
        return status;

    struct LivePath {
        Port End;
        std::vector<Link> Links;
        bool EndsAtExit = false;
    };
    std::map<std::uint32_t, LivePath> livePaths;
    status = EachStep<Steps::Follow>(
        m_Steps, [&](const Steps::Follow &item) -> Status {
            const auto owner = nodes.find(item.Start.Owner);
            const auto liveOwner = liveNodes.find(item.Start.Owner);
            if (owner == nodes.end() || liveOwner == liveNodes.end()) {
                return Failure(Error::InvalidState,
                               "A Path names an unknown Node.");
            }

            GraphEndpoint start;
            Status current =
                ResolvePort(*owner->second, item.Start.Selector, start);
            if (!current)
                return current;

            Path path;
            current = CompletePath(base, start, path);
            if (!current)
                return current;

            LivePath live;
            for (const LinkBase &link : path.Links) {
                Link resolvedLink;
                current = resolver.UseLink(resolved, link.Anchor, resolvedLink);
                if (!current)
                    return current;
                live.Links.push_back(resolvedLink);
            }
            live.EndsAtExit = path.End.Node == root->Id &&
                path.End.Kind == SlotKind::Output;
            if (!live.EndsAtExit) {
                if (path.End == start) {
                    live.End = {liveOwner->second.Value, item.Start.Selector};
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
                        nodes.begin(), nodes.end(), [&](const auto &entry) {
                            return entry.second->Id == path.End.Node;
                        });
                    if (existing != nodes.end()) {
                        liveEnd = liveNodes.at(existing->first);
                    } else {
                        current = resolver.UseNode(resolved, end->Object,
                                                   liveEnd);
                        if (!current)
                            return current;
                    }
                    live.End = {liveEnd.Value,
                                Slot::At(path.End.Kind, path.End.Index)};
                }
            }
            livePaths.emplace(item.Handle.Value, std::move(live));
            return {};
        });
    if (!status)
        return status;

    status = EachStep<Steps::AddBlock, Steps::AddGraph>(
        m_Steps, [&](const auto &item) -> Status {
            using T = std::decay_t<decltype(item)>;
            Node live;
            Status current;
            if constexpr (std::is_same_v<T, Steps::AddBlock>)
                current = resolver.Add(resolved, item.Block, live);
            else
                current = resolver.AddGraph(resolved, item.Graph.Name,
                                            item.Graph.Priority, live);
            if (!current)
                return current;
            liveNodes.emplace(item.Handle.Value, live);
            return {};
        });
    if (!status)
        return status;

    status = EachStep<Steps::Replace>(
        m_Steps, [&](const Steps::Replace &item) -> Status {
            const auto target = liveNodes.find(item.Target.Value);
            const auto replacement = liveNodes.find(item.Replacement.Value);
            if (target == liveNodes.end() || replacement == liveNodes.end()) {
                return Failure(Error::InvalidState,
                               "A replacement lost one of its Nodes during compilation.");
            }
            resolved.Replace(target->second, replacement->second);
            return {};
        });
    if (!status)
        return status;
    status = EachStep<Steps::Remove>(
        m_Steps, [&](const Steps::Remove &item) -> Status {
            const auto target = liveNodes.find(item.Target.Value);
            if (target == liveNodes.end()) {
                return Failure(
                    Error::InvalidState,
                    "A removal lost its Node during compilation.");
            }
            resolved.Remove(target->second);
            return {};
        });
    if (!status)
        return status;

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
    const auto publicPort = [](const Program &scope, const Step &step)
        -> const Steps::Append * {
        const auto *item = std::get_if<Steps::Append>(&step);
        return item && item->Owner == scope.Graph() &&
                item->Kind != SlotKind::Local
            ? item : nullptr;
    };

    // A graph-backed Behavior exposes its public Ins, Outs, Pins, and Pouts
    // both inside the graph and on the Node owned by its parent Graph. The
    // parent scope creates those CK objects before the nested scope is
    // resolved. Locals remain private to the nested Graph and are created by
    // this scope.
    if (rootInterfaceExists) {
        using InterfaceName = std::pair<SlotKind, std::string>;
        std::map<InterfaceName, int> declared;
        for (const Step &step : m_Steps) {
            if (const Steps::Append *item = publicPort(*this, step))
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

        for (const Step &step : m_Steps) {
            const Steps::Append *item = publicPort(*this, step);
            if (!item)
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
    // child links before removing the public interface. A Local belongs only
    // to the nested Graph and must not be projected into its parent scope.
    for (const Nested &nested : m_Nested) {
        if (!nested.Body)
            return Failure(Error::InvalidState,
                           "A nested Graph Edit has no body.");
        const auto parent = liveNodes.find(nested.Parent.Value);
        if (parent == liveNodes.end())
            return Failure(Error::InvalidState,
                           "A nested Graph Edit lost its parent Node.");
        const auto model = nodes.find(nested.Parent.Value);
        for (const Step &step : nested.Body->m_Steps) {
            const Steps::Append *item = publicPort(*nested.Body, step);
            if (!item)
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
                live = published.front()->Live;
                return {};
            } else {
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

    const auto ports = [&](const Port &symbolic,
                           std::vector<Port> &live) -> Status {
        live.clear();
        const auto many = manyLiveNodes.find(symbolic.Owner);
        if (many != manyLiveNodes.end()) {
            live.reserve(many->second.size());
            for (Node owner : many->second)
                live.push_back({owner.Value, symbolic.Selector});
            return {};
        }
        Port one;
        Status current = port(symbolic, one);
        if (current)
            live.push_back(std::move(one));
        return current;
    };

    const auto paired = [&](const std::vector<Port> &left,
                            const std::vector<Port> &right,
                            const auto &apply) -> Status {
        if (left.empty() || right.empty())
            return Failure(Error::InvalidState,
                           "A repeated graph action has no ports.");
        if (left.size() != 1 && right.size() != 1 &&
            left.size() != right.size()) {
            return Failure(
                Error::QueryAmbiguous,
                "A repeated graph action has different source and target counts.");
        }
        const std::size_t count = (std::max)(left.size(), right.size());
        for (std::size_t index = 0; index < count; ++index) {
            apply(left[left.size() == 1 ? 0 : index],
                  right[right.size() == 1 ? 0 : index]);
        }
        return {};
    };

    for (const Step &step : m_Steps) {
        status = [&]() -> Status {
            switch (step.index()) {
            case StepIndex<Steps::Flow>(): {
                const auto &item = std::get<Steps::Flow>(step);
                std::vector<Port> sources;
                std::vector<Port> sinks;
                Status current = ports(item.Source, sources);
                if (current)
                    current = ports(item.Sink, sinks);
                if (!current)
                    return current;
                return paired(sources, sinks,
                    [&](Port source, Port sink) {
                        resolved.Flow(std::move(source), std::move(sink),
                                      item.Delay, item.SameFrameCycle);
                    });
            }
            case StepIndex<Steps::HookFlow>(): {
                const auto &item = std::get<Steps::HookFlow>(step);
                std::vector<Port> sources;
                std::vector<Port> sinks;
                Status current = ports(item.Source, sources);
                if (current)
                    current = ports(item.Sink, sinks);
                if (!current)
                    return current;
                Status shape = paired(sources, sinks,
                    [&](Port source, Port sink) {
                        if (current) {
                            current = resolver.Interpose(
                                resolved, std::move(source), std::move(sink),
                                item.Hook);
                        }
                    });
                return shape ? current : shape;
            }
            case StepIndex<Steps::Set>(): {
                const auto &item = std::get<Steps::Set>(step);
                std::vector<Port> targets;
                Status current = ports(item.Target, targets);
                if (!current)
                    return current;
                for (Port target : targets)
                    resolved.Set(std::move(target), item.Value);
                break;
            }
            case StepIndex<Steps::Bind>(): {
                const auto &item = std::get<Steps::Bind>(step);
                std::vector<Port> targets;
                Status current = ports(item.Target, targets);
                if (!current)
                    return current;
                if (item.Kind == BindKind::Literal) {
                    for (Port target : targets)
                        resolved.Bind(std::move(target), item.Value);
                } else {
                    std::vector<Port> sources;
                    current = ports(item.Source, sources);
                    if (!current)
                        return current;
                    return paired(targets, sources,
                        [&](Port target, Port source) {
                            if (item.Kind == BindKind::Direct) {
                                resolved.Bind(std::move(target),
                                              std::move(source));
                            } else {
                                resolved.Share(std::move(target),
                                               std::move(source));
                            }
                        });
                }
                break;
            }
            case StepIndex<Steps::Push>(): {
                const auto &item = std::get<Steps::Push>(step);
                std::vector<Port> sources;
                std::vector<Port> destinations;
                Status current = ports(item.Source, sources);
                if (current)
                    current = ports(item.Destination, destinations);
                if (!current)
                    return current;
                return paired(sources, destinations,
                    [&](Port source, Port destination) {
                        resolved.Push(std::move(source),
                                      std::move(destination));
                    });
            }
            case StepIndex<Steps::Splice>(): {
                const auto &item = std::get<Steps::Splice>(step);
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
                break;
            }
            case StepIndex<Steps::Redirect>(): {
                const auto &item = std::get<Steps::Redirect>(step);
                const auto link = liveLinks.find(item.Target.Value);
                if (link == liveLinks.end())
                    return Failure(Error::InvalidState,
                                   "A Graph Edit Redirect names an unknown Link.");
                Port sink;
                Status current = port(item.Sink, sink);
                if (!current)
                    return current;
                resolved.Redirect(link->second, std::move(sink), item.Ordering);
                break;
            }
            case StepIndex<Steps::RedirectToLink>(): {
                const auto &item = std::get<Steps::RedirectToLink>(step);
                const auto link = liveLinks.find(item.Target.Value);
                const auto destination = modelLinks.find(
                    item.Destination.Value);
                if (link == liveLinks.end() || destination == modelLinks.end()) {
                    return Failure(
                        Error::InvalidState,
                        "A Graph Edit Redirect names an unknown Link.");
                }
                const GraphEndpoint endpoint = destination->second->Target;
                const auto modelNode = std::find_if(
                    base.Nodes.begin(), base.Nodes.end(),
                    [&](const GraphNode &candidate) {
                        return candidate.Id == endpoint.Node;
                    });
                if (modelNode == base.Nodes.end()) {
                    return Failure(
                        Error::GraphChanged,
                        "A Redirect destination disappeared from the graph.");
                }
                Node destinationNode;
                const auto existing = std::find_if(
                    nodes.begin(), nodes.end(), [&](const auto &entry) {
                        return entry.second->Id == endpoint.Node;
                    });
                if (existing != nodes.end()) {
                    destinationNode = liveNodes.at(existing->first);
                } else {
                    Status current = useNode(*modelNode, destinationNode);
                    if (!current)
                        return current;
                }
                resolved.Redirect(
                    link->second,
                    {destinationNode.Value,
                     Slot::At(endpoint.Kind, endpoint.Index)},
                    item.Ordering);
                break;
            }
            case StepIndex<Steps::Reconnect>(): {
                const auto &item = std::get<Steps::Reconnect>(step);
                const auto link = liveLinks.find(item.Target.Value);
                if (link == liveLinks.end())
                    return Failure(
                        Error::InvalidState,
                        "A Graph Edit Reconnect names an unknown Link.");
                Port source;
                Port sink;
                Status current = port(item.Source, source);
                if (current)
                    current = port(item.Sink, sink);
                if (!current)
                    return current;
                resolved.Reconnect(link->second, std::move(source),
                                   std::move(sink), item.SameFrameCycle);
                break;
            }
            case StepIndex<Steps::Append>(): {
                const auto &item = std::get<Steps::Append>(step);
                if (rootInterfaceExists && item.Owner == Graph() &&
                    item.Kind != SlotKind::Local)
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
                break;
            }
            case StepIndex<Steps::Tap>(): {
                const auto &item = std::get<Steps::Tap>(step);
                std::vector<Port> sources;
                Status current = ports(item.Source, sources);
                if (!current)
                    return current;
                for (Port source : sources) {
                    current = resolver.Tap(
                        resolved, std::move(source), item.Hook);
                    if (!current)
                        return current;
                }
                return {};
            }
            case StepIndex<Steps::After>(): {
                const auto &item = std::get<Steps::After>(step);
                const auto path = livePaths.find(item.Target.Value);
                if (path == livePaths.end()) {
                    return Failure(Error::InvalidState,
                                   "After names an unknown Path.");
                }
                if (!path->second.EndsAtExit) {
                    return resolver.Tap(
                        resolved, path->second.End, item.Hook);
                }
                if (path->second.Links.empty())
                    return Failure(Error::GraphChanged,
                                   "A Path reached an Exit without a Link.");
                return resolver.Interpose(
                    resolved, path->second.Links.back(), item.Hook);
            }
            case StepIndex<Steps::Before>(): {
                const auto &item = std::get<Steps::Before>(step);
                const auto link = liveLinks.find(item.Target.Value);
                if (link == liveLinks.end()) {
                    return Failure(Error::InvalidState,
                                   "Before names an unknown Link.");
                }
                return resolver.Interpose(
                    resolved, link->second, item.Hook);
            }
            default:
                break;
            }
            return {};
        }();
        if (!status)
            return status;
    }

    if (symbols) {
        symbols->Nodes = liveNodes;
        status = EachStep<Steps::Append>(
            m_Steps, [&](const Steps::Append &item) -> Status {
                Port live;
                Status current = port(item.Handle, live);
                if (!current)
                    return current;
                symbols->Ports.emplace(item.Handle.Selector.Index,
                                       std::move(live));
                return {};
            });
        if (!status)
            return status;
    }

    resolved.m_ExpectedFingerprint = base.Fingerprint;
    out = std::move(resolved);
    return {};
}

} // namespace BML::Behavior::Internal

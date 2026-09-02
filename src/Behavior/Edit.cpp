#include "Behavior/Edit.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace BML::Behavior {
namespace {

constexpr std::uint64_t kPlanNode = 1ull << 63u;
constexpr std::uint64_t kEntry = 1ull << 62u;
constexpr std::uint64_t kExit = 1ull << 61u;

Status Failure(Error error, std::string message,
               const Slot *selector = nullptr,
               CKGUID actual = CKGUID()) {
    Status status{error, CKERR_INVALIDPARAMETER, CKBR_PARAMETERERROR,
                  std::move(message)};
    status.Details.Stage = Phase::Edit;
    if (selector)
        status.Details.Selector = *selector;
    status.Details.ActualType = actual;
    return status;
}

std::uint64_t NativeNode(const GraphEndpoint &endpoint,
                         std::uint64_t root) {
    if (endpoint.Node != root)
        return endpoint.Node;
    if (endpoint.Kind == SlotKind::Input)
        return kEntry | static_cast<std::uint32_t>(endpoint.Index);
    if (endpoint.Kind == SlotKind::Output)
        return kExit | static_cast<std::uint32_t>(endpoint.Index);
    return 0;
}

bool IsControlSource(bool root, SlotKind kind) {
    return root ? kind == SlotKind::Input : kind == SlotKind::Output;
}

bool IsControlSink(bool root, SlotKind kind) {
    return root ? kind == SlotKind::Output : kind == SlotKind::Input;
}

bool IsBindTarget(SlotKind kind) {
    return kind == SlotKind::InputParameter || kind == SlotKind::Target;
}

bool IsDirectSource(SlotKind kind) {
    return kind == SlotKind::OutputParameter || kind == SlotKind::Local;
}

bool IsSharedSource(SlotKind kind) {
    return kind == SlotKind::InputParameter || kind == SlotKind::Target;
}

bool IsPushDestination(SlotKind kind) {
    return kind == SlotKind::OutputParameter || kind == SlotKind::Local;
}

bool OrderLess(const Order &left, const Order &right) {
    return std::tie(left.Other.Owner, left.Other.Name, left.Kind) <
           std::tie(right.Other.Owner, right.Other.Name, right.Kind);
}

CKDWORD InterfaceFlag(SlotKind kind) {
    switch (kind) {
    case SlotKind::Input: return CKBEHAVIOR_VARIABLEINPUTS;
    case SlotKind::Output: return CKBEHAVIOR_VARIABLEOUTPUTS;
    case SlotKind::InputParameter:
        return CKBEHAVIOR_VARIABLEPARAMETERINPUTS;
    case SlotKind::OutputParameter:
        return CKBEHAVIOR_VARIABLEPARAMETEROUTPUTS;
    default: return 0;
    }
}

Status Resolve(const Layout &layout, const Slot &selector, SlotInfo &out) {
    std::vector<const SlotInfo *> matches;
    for (const SlotInfo &candidate : layout.Slots) {
        if (candidate.Kind != selector.Kind)
            continue;
        if (selector.UsesName()) {
            if (candidate.Name == selector.Name)
                matches.push_back(&candidate);
        } else if (selector.RequireOnly || candidate.Index == selector.Index) {
            matches.push_back(&candidate);
        }
    }
    if (matches.empty())
        return Failure(Error::SlotNotFound,
                       "The Edit port selector did not match the candidate Layout.",
                       &selector);
    if ((selector.RequireOnly ||
         (selector.UsesName() && selector.RequireUnique)) &&
        matches.size() != 1) {
        return Failure(Error::AmbiguousSlot,
                       "The Edit port selector is ambiguous in the candidate Layout.",
                       &selector);
    }
    const int occurrence = selector.UsesName() ? selector.Occurrence : 0;
    if (occurrence < 0 ||
        occurrence >= static_cast<int>(matches.size())) {
        return Failure(Error::SlotNotFound,
                       "The requested Edit port occurrence does not exist.",
                       &selector);
    }
    out = *matches[static_cast<std::size_t>(occurrence)];
    return {};
}

class Components final {
public:
    explicit Components(
        const std::unordered_map<std::uint64_t,
                                 std::vector<std::uint64_t>> &edges)
        : m_Edges(edges) {
        for (const auto &[from, targets] : edges) {
            Add(from);
            for (std::uint64_t target : targets)
                Add(target);
        }
        for (std::uint64_t vertex : m_Vertices) {
            if (!m_Index.contains(vertex))
                Visit(vertex);
        }
    }

    [[nodiscard]] int Of(std::uint64_t vertex) const {
        const auto found = m_Component.find(vertex);
        return found == m_Component.end() ? -1 : found->second;
    }

    [[nodiscard]] int Count() const noexcept { return m_Count; }
    [[nodiscard]] int Size(int component) const {
        const auto found = m_Size.find(component);
        return found == m_Size.end() ? 0 : found->second;
    }

private:
    void Add(std::uint64_t vertex) {
        if (m_Seen.insert(vertex).second)
            m_Vertices.push_back(vertex);
    }

    void Visit(std::uint64_t vertex) {
        const int index = m_Next++;
        m_Index[vertex] = index;
        m_Low[vertex] = index;
        m_Stack.push_back(vertex);
        m_OnStack.insert(vertex);

        const auto edges = m_Edges.find(vertex);
        if (edges != m_Edges.end()) {
            for (std::uint64_t target : edges->second) {
                if (!m_Index.contains(target)) {
                    Visit(target);
                    m_Low[vertex] = (std::min)(m_Low[vertex], m_Low[target]);
                } else if (m_OnStack.contains(target)) {
                    m_Low[vertex] =
                        (std::min)(m_Low[vertex], m_Index[target]);
                }
            }
        }

        if (m_Low[vertex] != m_Index[vertex])
            return;
        const int component = m_Count++;
        int size = 0;
        for (;;) {
            const std::uint64_t current = m_Stack.back();
            m_Stack.pop_back();
            m_OnStack.erase(current);
            m_Component[current] = component;
            ++size;
            if (current == vertex)
                break;
        }
        m_Size[component] = size;
    }

    const std::unordered_map<std::uint64_t,
                             std::vector<std::uint64_t>> &m_Edges;
    std::vector<std::uint64_t> m_Vertices;
    std::unordered_set<std::uint64_t> m_Seen;
    std::unordered_map<std::uint64_t, int> m_Index;
    std::unordered_map<std::uint64_t, int> m_Low;
    std::unordered_map<std::uint64_t, int> m_Component;
    std::unordered_map<int, int> m_Size;
    std::vector<std::uint64_t> m_Stack;
    std::unordered_set<std::uint64_t> m_OnStack;
    int m_Next = 0;
    int m_Count = 0;
};

bool HasSelfLoop(
    const std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> &edges,
    std::uint64_t vertex) {
    const auto found = edges.find(vertex);
    return found != edges.end() &&
           std::find(found->second.begin(), found->second.end(), vertex) !=
               found->second.end();
}

std::uint64_t DataKey(const ResolvedPort &port) {
    return (static_cast<std::uint64_t>(port.Owner.Value) << 32u) |
           (static_cast<std::uint64_t>(port.Slot.Kind) << 24u) |
           static_cast<std::uint32_t>(port.Slot.NativeIndex);
}

bool HasCycle(const std::vector<std::pair<std::uint64_t, std::uint64_t>> &items) {
    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> edges;
    for (const auto &[from, to] : items)
        edges[from].push_back(to);
    Components components(edges);
    for (const auto &[from, to] : items) {
        const int component = components.Of(from);
        if (component >= 0 && component == components.Of(to) &&
            (from == to || components.Size(component) > 1))
            return true;
    }
    return false;
}

} // namespace

Port Node::In(int index) const {
    return {Value, Slot::At(SlotKind::Input, index)};
}

Port Node::In(std::string name) const {
    return {Value, Slot::Named(SlotKind::Input, std::move(name))};
}

Port Node::Out(int index) const {
    return {Value, Slot::At(SlotKind::Output, index)};
}

Port Node::Out(std::string name) const {
    return {Value, Slot::Named(SlotKind::Output, std::move(name))};
}

Port Node::Pin(int index) const {
    return {Value, Slot::At(SlotKind::InputParameter, index)};
}

Port Node::Pin(std::string name) const {
    return {Value, Slot::Named(SlotKind::InputParameter, std::move(name))};
}

Port Node::Pout(int index) const {
    return {Value, Slot::At(SlotKind::OutputParameter, index)};
}

Port Node::Pout(std::string name) const {
    return {Value, Slot::Named(SlotKind::OutputParameter, std::move(name))};
}

Port Node::Local(int index) const {
    return {Value, Slot::At(SlotKind::Local, index)};
}

Port Node::Local(std::string name) const {
    return {Value, Slot::Named(SlotKind::Local, std::move(name))};
}

Port Node::Target() const {
    return {Value, Slot::At(SlotKind::Target, 0)};
}

Edit::Edit(PatchKey key, NativeRef graph, Layout layout)
    : m_Key(std::move(key)) {
    m_Nodes.push_back({{1}, graph, std::move(layout), std::nullopt});
}

Port Edit::Entry(int index) const { return Graph().In(index); }

Port Edit::Entry(std::string name) const {
    return Graph().In(std::move(name));
}

Port Edit::Exit(int index) const { return Graph().Out(index); }

Port Edit::Exit(std::string name) const {
    return Graph().Out(std::move(name));
}

Node Edit::Use(NativeRef native, Layout layout) {
    const Node node{++m_NextNode};
    m_Nodes.push_back({node, native, std::move(layout), std::nullopt});
    return node;
}

Link Edit::Use(ObjectRef anchor) {
    const Link link{++m_NextLink};
    m_Links.push_back({link, anchor});
    return link;
}

Node Edit::Add(Spec block, Layout declared) {
    const Node node{++m_NextNode};
    m_Nodes.push_back({node, {}, std::move(declared), std::move(block)});
    EditNode &added = m_Nodes.back();
    for (const std::string &name : added.Block->m_AddedInputs)
        (void) Append(node, SlotKind::Input, name, CKGUID(), true);
    for (const std::string &name : added.Block->m_AddedOutputs)
        (void) Append(node, SlotKind::Output, name, CKGUID(), true);
    return node;
}

void Edit::Flow(Port source, Port sink, int delay, Cycle cycle) {
    m_Flows.push_back(
        {std::move(source), std::move(sink), delay, cycle, NextOrdinal()});
}

void Edit::Bind(Port target, Value value) {
    EditBind bind;
    bind.Target = std::move(target);
    bind.Kind = BindKind::Literal;
    bind.Literal = std::move(value);
    bind.Ordinal = NextOrdinal();
    m_Binds.push_back(std::move(bind));
}

void Edit::Bind(Port target, Port source) {
    EditBind bind;
    bind.Target = std::move(target);
    bind.Kind = BindKind::Direct;
    bind.Source = std::move(source);
    bind.Ordinal = NextOrdinal();
    m_Binds.push_back(std::move(bind));
}

void Edit::Share(Port target, Port source) {
    EditBind bind;
    bind.Target = std::move(target);
    bind.Kind = BindKind::Shared;
    bind.Source = std::move(source);
    bind.Ordinal = NextOrdinal();
    m_Binds.push_back(std::move(bind));
}

void Edit::Push(Port source, Port destination) {
    m_Pushes.push_back(
        {std::move(source), std::move(destination), NextOrdinal()});
}

void Edit::Tap(Port source,
               std::shared_ptr<HookBlock::Binding> callback) {
    m_Taps.push_back(
        {std::move(source), std::move(callback), NextOrdinal()});
}

void Edit::Splice(Link target, Node block, std::vector<Order> ordering) {
    Splice(target, block.In(), block.Out(), std::move(ordering));
}

void Edit::Splice(Link target, Port input, Port output,
                  std::vector<Order> ordering) {
    m_Splices.push_back({target, {input.Owner}, std::move(input),
                         std::move(output), std::move(ordering), NextOrdinal()});
}

Port Edit::AppendIn(Node node, std::string name) {
    return Append(node, SlotKind::Input, std::move(name), CKGUID());
}

Port Edit::AppendOut(Node node, std::string name) {
    return Append(node, SlotKind::Output, std::move(name), CKGUID());
}

Port Edit::AppendPin(Node node, std::string name, CKGUID type) {
    return Append(node, SlotKind::InputParameter, std::move(name), type);
}

Port Edit::AppendPout(Node node, std::string name, CKGUID type) {
    return Append(node, SlotKind::OutputParameter, std::move(name), type);
}

Port Edit::AppendLocal(Node node, std::string name, CKGUID type) {
    return Append(node, SlotKind::Local, std::move(name), type);
}

Port Edit::Append(Node node, SlotKind kind, std::string name, CKGUID type,
                  bool inBlockSpec) {
    EditNode *owner = Find(node);
    int index = 0;
    int occurrence = 0;
    if (owner) {
        for (const SlotInfo &candidate : owner->Shape.Slots) {
            if (candidate.Kind != kind)
                continue;
            index = (std::max)(index, candidate.Index + 1);
            if (candidate.Name == name)
                ++occurrence;
        }
        SlotInfo slot{kind, index, index, name, type, 0, occurrence};
        slot.Dynamic = true;
        owner->Shape.Slots.push_back(slot);
        m_Interface.push_back(
            {node, std::move(slot), NextOrdinal(), inBlockSpec});
    }
    return {node.Value, Slot::At(kind, index, type)};
}

const Edit::EditNode *Edit::Find(Node node) const noexcept {
    const auto found = std::find_if(
        m_Nodes.begin(), m_Nodes.end(),
        [&](const EditNode &candidate) { return candidate.Handle == node; });
    return found == m_Nodes.end() ? nullptr : &*found;
}

Edit::EditNode *Edit::Find(Node node) noexcept {
    return const_cast<EditNode *>(
        std::as_const(*this).Find(node));
}

std::uint32_t Edit::NextOrdinal() noexcept { return m_NextAction++; }

Status Edit::Validate(const GraphModel &base, CheckedEdit &out) const {
    out = {};
    if (m_Key.Owner.empty() || m_Key.Name.empty())
        return Failure(Error::InvalidState,
                       "An additive Edit requires an owner and patch key.");
    if (m_Nodes.empty() || !m_Nodes.front().Native)
        return Failure(Error::InvalidState,
                       "An additive Edit has no native graph.");
    if (m_ExpectedFingerprint != 0 &&
        base.Fingerprint != m_ExpectedFingerprint) {
        return Failure(Error::GraphChanged,
                       "The graph changed after its durable queries resolved.");
    }

    const std::uint64_t root = m_Nodes.front().Native.Id;
    const auto rootNode = std::find_if(
        base.Nodes.begin(), base.Nodes.end(),
        [&](const GraphNode &node) { return node.Id == root; });
    if (rootNode == base.Nodes.end() || rootNode->Parent != 0)
        return Failure(Error::InvalidGraphLocality,
                       "The Edit root is not the inspected Behavior graph.");

    std::unordered_set<std::uint64_t> borrowed;
    for (std::size_t index = 1; index < m_Nodes.size(); ++index) {
        const EditNode &node = m_Nodes[index];
        if (node.Block)
            continue;
        if (!node.Native || !borrowed.insert(node.Native.Id).second)
            return Failure(Error::InvalidGraphLocality,
                           "A borrowed Edit Node is stale or declared more than once.");
        const auto native = std::find_if(
            base.Nodes.begin(), base.Nodes.end(),
            [&](const GraphNode &candidate) {
                return candidate.Id == node.Native.Id;
            });
        if (native == base.Nodes.end() || native->Parent != root)
            return Failure(Error::InvalidGraphLocality,
                           "A borrowed Edit Node does not belong to the target graph.");
    }

    for (const InterfacePort &item : m_Interface) {
        const EditNode *node = Find(item.Owner);
        if (!node)
            return Failure(Error::InvalidState,
                           "A dynamic interface action names an unknown Node.");
        const CKDWORD flag = InterfaceFlag(item.Slot.Kind);
        if (!flag || (node->Shape.BehaviorFlags & flag) == 0) {
            return Failure(
                Error::InterfaceUnsupported,
                "The Building Block does not permit this dynamic interface kind.");
        }
        if ((item.Slot.Kind == SlotKind::InputParameter ||
             item.Slot.Kind == SlotKind::OutputParameter) &&
            !item.Slot.Type.IsValid()) {
            return Failure(Error::TypeMismatch,
                           "A dynamic parameter requires a Virtools type GUID.");
        }
    }

    const auto resolve = [&](const Port &port, ResolvedPort &resolved) {
        const EditNode *node = Find({port.Owner});
        if (!node)
            return Failure(Error::InvalidState,
                           "An Edit action names an unknown Node.",
                           &port.Selector);
        resolved.Owner = node->Handle;
        resolved.Selector = port.Selector;
        Status status = Resolve(node->Shape, port.Selector, resolved.Slot);
        if (!status)
            return status;
        resolved.Appended = std::any_of(
            m_Interface.begin(), m_Interface.end(),
            [&](const InterfacePort &item) {
                return item.Owner == resolved.Owner &&
                    item.Slot.Kind == resolved.Slot.Kind &&
                    item.Slot.NativeIndex == resolved.Slot.NativeIndex;
            });
        return Status{};
    };

    for (const EditFlow &flow : m_Flows) {
        if (flow.Delay < 0 || flow.Delay >= 32765)
            return Failure(Error::InvalidDelay,
                           "A Flow delay must be between 0 and 32764 frames.");
        CheckedFlow checked;
        Status status = resolve(flow.Source, checked.Source);
        if (status)
            status = resolve(flow.Sink, checked.Sink);
        if (!status)
            return status;
        const bool sourceRoot = checked.Source.Owner == Graph();
        const bool sinkRoot = checked.Sink.Owner == Graph();
        if (!IsControlSource(sourceRoot, checked.Source.Slot.Kind) ||
            !IsControlSink(sinkRoot, checked.Sink.Slot.Kind)) {
            return Failure(Error::TypeMismatch,
                           "Flow requires Entry/node Out as source and node In/Exit as sink.");
        }
        checked.Delay = flow.Delay;
        checked.SameFrameCycle = flow.SameFrameCycle;
        checked.Ordinal = flow.Ordinal;
        out.Flows.push_back(std::move(checked));
    }

    for (const EditBind &bind : m_Binds) {
        CheckedBind checked;
        Status status = resolve(bind.Target, checked.Target);
        if (!status)
            return status;
        if (!IsBindTarget(checked.Target.Slot.Kind))
            return Failure(Error::TypeMismatch,
                           "Bind requires a Pin or Target destination.");
        checked.Kind = bind.Kind;
        checked.Literal = bind.Literal;
        checked.Ordinal = bind.Ordinal;
        if (bind.Kind != BindKind::Literal) {
            status = resolve(bind.Source, checked.Source);
            if (!status)
                return status;
            if (bind.Kind == BindKind::Direct &&
                !IsDirectSource(checked.Source.Slot.Kind)) {
                return Failure(Error::TypeMismatch,
                               "A direct Bind source must be a Pout or Local.");
            }
            if (bind.Kind == BindKind::Shared &&
                !IsSharedSource(checked.Source.Slot.Kind)) {
                return Failure(Error::TypeMismatch,
                               "A shared Bind source must be a Pin or Target.");
            }
        }
        out.Binds.push_back(std::move(checked));
    }

    for (const EditPush &push : m_Pushes) {
        CheckedPush checked;
        Status status = resolve(push.Source, checked.Source);
        if (status)
            status = resolve(push.Destination, checked.Destination);
        if (!status)
            return status;
        if (checked.Source.Slot.Kind != SlotKind::OutputParameter ||
            !IsPushDestination(checked.Destination.Slot.Kind)) {
            return Failure(Error::TypeMismatch,
                           "Push requires a Pout source and parameter destination.");
        }
        checked.Ordinal = push.Ordinal;
        out.Pushes.push_back(std::move(checked));
    }

    for (const EditTap &tap : m_Taps) {
        CheckedTap checked;
        Status status = resolve(tap.Source, checked.Source);
        if (!status)
            return status;
        if (!tap.Callback ||
            !IsControlSource(checked.Source.Owner == Graph(),
                             checked.Source.Slot.Kind)) {
            return Failure(Error::TypeMismatch,
                           "Tap requires a callback and an Entry/node Out source.");
        }
        checked.Callback = tap.Callback;
        checked.Ordinal = tap.Ordinal;
        out.Taps.push_back(std::move(checked));
    }

    std::map<std::uint32_t, std::vector<Order>> spliceOrdering;
    for (const EditSplice &splice : m_Splices) {
        if (!splice.Target)
            return Failure(Error::InvalidState,
                           "A Splice requires a Link from this Edit.");
        const auto declared = std::find_if(
            m_Links.begin(), m_Links.end(), [&](const EditLink &candidate) {
                return candidate.Handle == splice.Target;
            });
        if (declared == m_Links.end() || declared->Anchor.IsNull())
            return Failure(Error::LinkNotFound,
                           "A Splice requires an exact native Link anchor.");
        const auto native = std::find_if(
            base.Links.begin(), base.Links.end(), [&](const GraphLink &candidate) {
                return candidate.Object == declared->Anchor;
            });
        if (native == base.Links.end())
            return Failure(Error::LinkNotFound,
                           "The selected Link is not present in the logical graph.");
        if (std::find_if(std::next(native), base.Links.end(),
                         [&](const GraphLink &candidate) {
                             return candidate.Object == declared->Anchor;
                         }) != base.Links.end()) {
            return Failure(Error::GraphChanged,
                           "The logical graph repeats a native Link anchor.");
        }
        if (native->InitialDelay < 0)
            return Failure(Error::InvalidDelay,
                           "A selected Link has an invalid delay.");

        CheckedSplice checked;
        Status status = resolve(splice.Input, checked.Input);
        if (status)
            status = resolve(splice.Output, checked.Output);
        if (!status)
            return status;
        if (!splice.Block || checked.Input.Owner != splice.Block ||
            checked.Output.Owner != splice.Block ||
            splice.Block == Graph() || checked.Input.Slot.Kind != SlotKind::Input ||
            checked.Output.Slot.Kind != SlotKind::Output) {
            return Failure(Error::TypeMismatch,
                           "Splice requires an In and Out on the same node.");
        }
        checked.Target = {native->Object, native->Source, native->Target,
                          native->InitialDelay};
        checked.Ordering = splice.Ordering;
        std::sort(checked.Ordering.begin(), checked.Ordering.end(), OrderLess);
        checked.Ordering.erase(
            std::unique(checked.Ordering.begin(), checked.Ordering.end()),
            checked.Ordering.end());
        if (std::any_of(
                checked.Ordering.begin(), checked.Ordering.end(),
                [&](const Order &order) { return order.Other == m_Key; })) {
            return Failure(Error::OverlayOrderCycle,
                           "A Patch cannot order a Link against itself.");
        }
        const auto [knownOrder, inserted] = spliceOrdering.emplace(
            splice.Target.Value, checked.Ordering);
        if (!inserted && knownOrder->second != checked.Ordering) {
            return Failure(
                Error::InvalidState,
                "Splices in one Patch must share the Link ordering declaration.");
        }
        checked.Ordinal = splice.Ordinal;
        out.Splices.push_back(std::move(checked));
    }

    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> baseline;
    for (const GraphLink &link : base.Links) {
        if (link.InitialDelay != 0)
            continue;
        const std::uint64_t source = NativeNode(link.Source, root);
        const std::uint64_t sink = NativeNode(link.Target, root);
        if (!source || !sink)
            return Failure(Error::GraphChanged,
                           "The inspected graph contains an invalid control Link.");
        baseline[source].push_back(sink);
    }
    Components baseComponents(baseline);

    const auto vertex = [&](const ResolvedPort &port) {
        const EditNode *node = Find(port.Owner);
        if (!node)
            return std::uint64_t{0};
        if (port.Owner == Graph()) {
            return port.Slot.Kind == SlotKind::Input
                ? kEntry | static_cast<std::uint32_t>(port.Slot.NativeIndex)
                : kExit | static_cast<std::uint32_t>(port.Slot.NativeIndex);
        }
        return node->Native ? node->Native.Id
                            : kPlanNode | port.Owner.Value;
    };

    // Include isolated plan vertices before collapsing the baseline.
    std::unordered_map<std::uint64_t, int> components;
    int nextComponent = baseComponents.Count();
    auto component = [&](std::uint64_t value) {
        int result = baseComponents.Of(value);
        if (result >= 0)
            return result;
        const auto [found, inserted] =
            components.emplace(value, nextComponent);
        if (inserted)
            ++nextComponent;
        return found->second;
    };

    struct Delta {
        int Source = -1;
        int Sink = -1;
        const CheckedFlow *Flow = nullptr;
        bool BaselineCyclic = false;
    };
    std::vector<Delta> delta;
    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> candidate;
    for (const CheckedFlow &flow : out.Flows) {
        if (flow.Delay != 0)
            continue;
        const std::uint64_t sourceVertex = vertex(flow.Source);
        const std::uint64_t sinkVertex = vertex(flow.Sink);
        const int source = component(sourceVertex);
        const int sink = component(sinkVertex);
        const bool baselineCyclic = source == sink &&
            (baseComponents.Size(source) > 1 ||
             HasSelfLoop(baseline, sourceVertex));
        delta.push_back({source, sink, &flow, baselineCyclic});
        if (source != sink || !baselineCyclic)
            candidate[static_cast<std::uint64_t>(source)].push_back(
                static_cast<std::uint64_t>(sink));
    }
    Components finalComponents(candidate);
    for (const Delta &edge : delta) {
        bool createsCycle = false;
        if (edge.Source == edge.Sink)
            createsCycle = !edge.BaselineCyclic;
        else {
            const int group = finalComponents.Of(
                static_cast<std::uint64_t>(edge.Source));
            createsCycle = group >= 0 &&
                group == finalComponents.Of(
                    static_cast<std::uint64_t>(edge.Sink)) &&
                finalComponents.Size(group) > 1;
        }
        if (createsCycle &&
            edge.Flow->SameFrameCycle != Cycle::Confirmed) {
            return Failure(
                Error::UnconfirmedSameFrameCycle,
                "Flow #" + std::to_string(edge.Flow->Ordinal) +
                    " participates in a new same-frame cycle and was not confirmed.");
        }
    }

    std::set<std::uint64_t> bound;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> shares;
    for (const CheckedBind &bind : out.Binds) {
        if (!bound.insert(DataKey(bind.Target)).second) {
            return Failure(Error::InvalidState,
                           "A Pin or Target can have only one Bind in an Edit.");
        }
        if (bind.Kind == BindKind::Shared)
            shares.emplace_back(DataKey(bind.Target), DataKey(bind.Source));
    }
    if (HasCycle(shares))
        return Failure(Error::SharedSourceCycle,
                       "The candidate shared-source graph contains a cycle.");

    std::set<std::pair<std::uint64_t, std::uint64_t>> pushed;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> pushes;
    for (const CheckedPush &push : out.Pushes) {
        const auto edge = std::make_pair(
            DataKey(push.Source), DataKey(push.Destination));
        if (!pushed.insert(edge).second) {
            return Failure(Error::InvalidState,
                           "The same Pout destination appears more than once in an Edit.");
        }
        pushes.push_back(edge);
    }
    if (HasCycle(pushes))
        return Failure(Error::PushCycle,
                       "The candidate Pout destination graph contains a cycle.");
    return {};
}

} // namespace BML::Behavior

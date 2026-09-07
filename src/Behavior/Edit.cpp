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

namespace BML::Behavior::Internal {
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

// A Pin and the Target read a source, so only they can be given one. A literal
// needs no source: CK2 keeps it in the parameter itself, which is also how a
// Local holds its value, so a written value reaches a Local too. A Setting is
// deliberately absent. Editing one can rebuild the whole layout of a block, so
// a Setting is declared when the Block is created and never written into a
// Block that already exists.
bool IsLiteralTarget(SlotKind kind) {
    return IsBindTarget(kind) || kind == SlotKind::Local;
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
    if (port.Interface != 0) {
        return (static_cast<std::uint64_t>(port.Owner.Value) << 32u) |
               0x80000000u | port.Interface;
    }
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

Port ParameterOperation::Input(int index) const {
    return {Value, Slot::At(SlotKind::InputParameter, index)};
}

Port ParameterOperation::Result() const {
    return {Value, Slot::At(SlotKind::OutputParameter, 0)};
}

Edit::Edit(PatchKey key, NativeRef graph, Layout layout)
    : m_Key(std::move(key)) {
    m_Nodes.push_back(
        {{1}, graph, std::move(layout), std::nullopt, std::nullopt,
         NodeRole::Logical});
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
    m_Nodes.push_back(
        {node, native, std::move(layout), std::nullopt, std::nullopt,
         NodeRole::Logical});
    return node;
}

Link Edit::Use(ObjectRef anchor) {
    const Link link{++m_NextLink};
    m_Links.push_back({link, anchor});
    return link;
}

Node Edit::Add(BlockSpec block, Layout declared, NodeRole role) {
    const Node node{++m_NextNode};
    m_Nodes.push_back(
        {node, {}, std::move(declared), std::move(block), std::nullopt, role});
    EditNode &added = m_Nodes.back();
    for (const std::string &name : added.Block->m_AddedInputs)
        (void) Append(node, SlotKind::Input, name, CKGUID(), true);
    for (const std::string &name : added.Block->m_AddedOutputs)
        (void) Append(node, SlotKind::Output, name, CKGUID(), true);
    return node;
}

Node Edit::AddGraph(std::string name, int priority, NodeRole role) {
    const Node node{++m_NextNode};
    Layout layout;
    layout.Origin = LayoutOrigin::Live;
    layout.Kind = BehaviorKind::Graph;
    m_Nodes.push_back(
        {node, {}, std::move(layout), std::nullopt,
         GraphSpec{std::move(name), priority}, role});
    return node;
}

ParameterOperation Edit::AddOperation(CKGUID operation, CKGUID result,
                                      CKGUID input1, CKGUID input2) {
    const ParameterOperation handle{++m_NextNode};
    m_Operations.push_back(
        {handle, operation, result, input1, input2, NextOrdinal()});
    return handle;
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

void Edit::Redirect(Link target, Port sink, std::vector<Order> ordering) {
    m_Redirects.push_back(
        {target, std::move(sink), std::move(ordering), NextOrdinal()});
}

void Edit::Replace(Node target, Node replacement) {
    m_Replacements.push_back({target, replacement, NextOrdinal()});
}

void Edit::Remove(Node target) {
    m_Removals.push_back({target, NextOrdinal()});
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
    std::uint32_t identity = 0;
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
        identity = NextOrdinal();
        m_Interface.push_back(
            {identity, node, std::move(slot), identity, inBlockSpec});
    }
    return {node.Value, Slot::At(kind, index, type), identity};
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

const EditOperation *Edit::Find(ParameterOperation operation) const noexcept {
    const auto found = std::find_if(
        m_Operations.begin(), m_Operations.end(),
        [&](const EditOperation &candidate) {
            return candidate.Handle == operation;
        });
    return found == m_Operations.end() ? nullptr : &*found;
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
        if (node.Authored())
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

    std::set<std::uint32_t> parked;
    const auto publicShape = [](const Layout &layout, SlotKind kind) {
        std::vector<const SlotInfo *> result;
        for (const SlotInfo &slot : layout.Slots) {
            if (slot.Kind == kind)
                result.push_back(&slot);
        }
        std::sort(result.begin(), result.end(),
                  [](const SlotInfo *left, const SlotInfo *right) {
                      return left->Index < right->Index;
                  });
        return result;
    };
    for (const EditReplace &item : m_Replacements) {
        const EditNode *target = Find(item.Target);
        const EditNode *replacement = Find(item.Replacement);
        if (!target || target == &m_Nodes.front() || target->Authored() ||
            !replacement || !replacement->Authored()) {
            return Failure(
                Error::InvalidState,
                "Replace requires a borrowed child Node and an authored replacement Block.");
        }
        if (!parked.insert(item.Target.Value).second) {
            return Failure(Error::InvalidState,
                           "One Edit cannot replace the same Node twice.");
        }
        const auto liveTarget = std::find_if(
            base.Nodes.begin(), base.Nodes.end(),
            [&](const GraphNode &candidate) {
                return candidate.Id == target->Native.Id;
            });
        if (liveTarget == base.Nodes.end() || liveTarget->Active ||
            std::any_of(liveTarget->Ports.begin(), liveTarget->Ports.end(),
                        [](const GraphPort &port) {
                            return (port.Kind == SlotKind::Input ||
                                    port.Kind == SlotKind::Output) &&
                                port.Active;
                        })) {
            return Failure(Error::Busy,
                           "Only an idle Behavior Node can be replaced.");
        }
        for (SlotKind kind : {SlotKind::Input, SlotKind::Output,
                              SlotKind::Target,
                              SlotKind::InputParameter,
                              SlotKind::OutputParameter}) {
            const auto before = publicShape(target->Shape, kind);
            const auto after = publicShape(replacement->Shape, kind);
            if (before.size() != after.size())
                return Failure(
                    Error::InterfaceUnsupported,
                    "A replacement Block has a different public Behavior interface.");
            for (std::size_t index = 0; index < before.size(); ++index) {
                if (before[index]->Index != after[index]->Index ||
                    before[index]->Name != after[index]->Name ||
                    before[index]->Occurrence != after[index]->Occurrence ||
                    before[index]->Type != after[index]->Type) {
                    return Failure(
                        Error::InterfaceUnsupported,
                        "A replacement Block changed a public Behavior port.");
                }
            }
        }
        out.Replacements.push_back(
            {item.Target, item.Replacement, item.Ordinal});
    }
    for (const EditRemove &item : m_Removals) {
        const EditNode *target = Find(item.Target);
        if (!target || target == &m_Nodes.front() || target->Authored()) {
            return Failure(Error::InvalidState,
                           "Remove requires a borrowed child Node.");
        }
        if (!parked.insert(item.Target.Value).second) {
            return Failure(
                Error::InvalidState,
                "One Edit cannot replace and remove the same Node, or remove it twice.");
        }
        const auto liveTarget = std::find_if(
            base.Nodes.begin(), base.Nodes.end(),
            [&](const GraphNode &candidate) {
                return candidate.Id == target->Native.Id;
            });
        if (liveTarget == base.Nodes.end() || liveTarget->Active ||
            std::any_of(liveTarget->Ports.begin(), liveTarget->Ports.end(),
                        [](const GraphPort &port) {
                            return (port.Kind == SlotKind::Input ||
                                    port.Kind == SlotKind::Output) &&
                                port.Active;
                        })) {
            return Failure(Error::Busy,
                           "Only an idle Behavior Node can be removed.");
        }
        out.Removals.push_back({item.Target, item.Ordinal});
    }
    const auto parkedPort = [&](const Port &port) {
        return parked.contains(port.Owner);
    };
    if (!parked.empty()) {
        for (const InterfacePort &item : m_Interface) {
            if (parked.contains(item.Owner.Value))
                return Failure(
                    Error::InvalidState,
                    "A parked Node cannot also receive an interface edit.");
        }
        for (const EditFlow &item : m_Flows) {
            if (parkedPort(item.Source) || parkedPort(item.Sink))
                return Failure(
                    Error::InvalidState,
                    "A parked Node cannot participate in Flow.");
        }
        for (const EditBind &item : m_Binds) {
            if (parkedPort(item.Target) ||
                (item.Kind != BindKind::Literal && parkedPort(item.Source)))
                return Failure(
                    Error::InvalidState,
                    "A parked Node cannot participate in Bind.");
        }
        for (const EditPush &item : m_Pushes) {
            if (parkedPort(item.Source) || parkedPort(item.Destination))
                return Failure(
                    Error::InvalidState,
                    "A parked Node cannot participate in Push.");
        }
        for (const EditTap &item : m_Taps) {
            if (parkedPort(item.Source))
                return Failure(
                    Error::InvalidState,
                    "A parked Node cannot participate in Tap.");
        }
        if (!m_Splices.empty() || !m_Redirects.empty()) {
            return Failure(
                Error::InvalidState,
                "A Node edit cannot share one Patch with Link overlays.");
        }
    }

    for (const InterfacePort &item : m_Interface) {
        const EditNode *node = Find(item.Owner);
        if (!node)
            return Failure(Error::InvalidState,
                           "A dynamic interface action names an unknown Node.");
        // CK2 appends Ins, Outs, and parameters to any Behavior without
        // consulting the variable-interface flags: those flags declare intent
        // to an editor, they do not gate CreateInput and friends. So an
        // appended port is allowed everywhere, and the flag only tells the
        // author whether the block's own code will read it. A Local is private
        // state, so an Edit may append one only to its graph root or to a Block
        // the same Edit owns.
        const bool ownedLocal = item.Slot.Kind == SlotKind::Local &&
            (item.Owner == Graph() || node->Authored());
        if (!InterfaceFlag(item.Slot.Kind) && !ownedLocal) {
            return Failure(
                Error::InterfaceUnsupported,
                "A Local can be appended only to the graph root or a Block added by this Edit.");
        }
        if ((item.Slot.Kind == SlotKind::InputParameter ||
             item.Slot.Kind == SlotKind::OutputParameter ||
             item.Slot.Kind == SlotKind::Local) &&
            !item.Slot.Type.IsValid()) {
            return Failure(Error::TypeMismatch,
                           "A dynamic parameter requires a Virtools type GUID.");
        }
    }

    const auto resolve = [&](const Port &port, ResolvedPort &resolved) {
        const EditNode *node = Find(Node{port.Owner});
        const EditOperation *operation = Find(ParameterOperation{port.Owner});
        if (!node && !operation)
            return Failure(Error::InvalidState,
                           "An Edit action names an unknown graph object.",
                           &port.Selector);
        resolved.Owner = {port.Owner};
        resolved.Selector = port.Selector;
        resolved.Interface = port.Interface;
        resolved.Operation = operation != nullptr;
        if (operation) {
            if (port.Interface != 0 || port.Selector.UsesName() ||
                port.Selector.RequireOnly) {
                return Failure(
                    Error::InvalidState,
                    "A Parameter Operation port must use its fixed index.",
                    &port.Selector);
            }
            CKGUID type;
            if (port.Selector.Kind == SlotKind::InputParameter &&
                port.Selector.Index >= 0 && port.Selector.Index <= 1) {
                type = port.Selector.Index == 0
                    ? operation->Input1 : operation->Input2;
            } else if (port.Selector.Kind == SlotKind::OutputParameter &&
                       port.Selector.Index == 0) {
                type = operation->Result;
            } else {
                return Failure(
                    Error::TypeMismatch,
                    "A Parameter Operation exposes Pin 0, Pin 1, and Pout 0 only.",
                    &port.Selector);
            }
            if (!type.IsValid() || type == CKPGUID_NONE) {
                return Failure(
                    Error::SlotNotFound,
                    "The selected input is absent from this Parameter Operation overload.",
                    &port.Selector);
            }
            resolved.Slot = {
                port.Selector.Kind, port.Selector.Index,
                port.Selector.Index, port.Selector.Kind == SlotKind::InputParameter
                    ? (port.Selector.Index == 0 ? "Pin 0" : "Pin 1")
                    : "Pout 0",
                type, 0, 0};
            return Status{};
        }
        resolved.Owner = node->Handle;
        if (port.Interface != 0) {
            const auto declared = std::find_if(
                m_Interface.begin(), m_Interface.end(),
                [&](const InterfacePort &item) {
                    return item.Identity == port.Interface &&
                           item.Owner == resolved.Owner;
                });
            if (declared == m_Interface.end()) {
                return Failure(
                    Error::InvalidState,
                    "An Edit Port names an unknown interface declaration.",
                    &port.Selector);
            }
            resolved.Slot = declared->Slot;
            resolved.Appended = true;
        } else {
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
        }
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
        const bool literal = bind.Kind == BindKind::Literal;
        if (literal ? !IsLiteralTarget(checked.Target.Slot.Kind)
                    : !IsBindTarget(checked.Target.Slot.Kind)) {
            return Failure(
                Error::TypeMismatch,
                literal
                    ? "A written value requires a Pin, Local, or Target destination."
                    : "Bind requires a Pin or Target destination.");
        }
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

    for (const EditOperation &operation : m_Operations) {
        if (!operation.Operation.IsValid() ||
            !operation.Result.IsValid() || operation.Result == CKPGUID_NONE) {
            Status invalid = Failure(
                Error::OperationInvalid,
                "A Parameter Operation requires an operation GUID and result type.");
            invalid.Details.OperationGuid = operation.Operation;
            return invalid;
        }
        if (operation.Input2.IsValid() && operation.Input2 != CKPGUID_NONE &&
            (!operation.Input1.IsValid() || operation.Input1 == CKPGUID_NONE)) {
            Status invalid = Failure(
                Error::OperationInvalid,
                "A Parameter Operation cannot have a second input without its first input.");
            invalid.Details.OperationGuid = operation.Operation;
            return invalid;
        }
    }

    for (const EditTap &tap : m_Taps) {
        CheckedTap checked;
        Status status = resolve(tap.Source, checked.Source);
        if (!status)
            return status;
        // Topology models Out taps only. A graph Entry has no native Out to
        // observe, so it is rejected here instead of after native mutation.
        if (!tap.Callback || checked.Source.Owner == Graph() ||
            checked.Source.Slot.Kind != SlotKind::Output) {
            return Failure(Error::TypeMismatch,
                           "Tap requires a callback and a node Out source.");
        }
        checked.Callback = tap.Callback;
        checked.Ordinal = tap.Ordinal;
        out.Taps.push_back(std::move(checked));
    }

    // Both Splice and Redirect name a Link this Edit borrowed, so both resolve
    // its anchor the same way.
    const auto anchoredLink = [&](Link handle, const char *verb,
                                  LinkBase &out) -> Status {
        if (!handle)
            return Failure(Error::InvalidState,
                           std::string("A ") + verb +
                               " requires a Link from this Edit.");
        const auto declared = std::find_if(
            m_Links.begin(), m_Links.end(), [&](const EditLink &candidate) {
                return candidate.Handle == handle;
            });
        if (declared == m_Links.end() || declared->Anchor.IsNull())
            return Failure(Error::LinkNotFound,
                           std::string("A ") + verb +
                               " requires an exact native Link anchor.");
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
        out = {native->Object, native->Source, native->Target,
               native->InitialDelay};
        return {};
    };

    // Splice and Redirect both declare the ordering of one logical Link, so a
    // Patch holding several actions on one Link must agree with itself.
    std::map<std::uint32_t, std::vector<Order>> spliceOrdering;
    const auto linkOrdering = [&](Link handle, std::vector<Order> ordering,
                                  std::vector<Order> &out) -> Status {
        std::sort(ordering.begin(), ordering.end(), OrderLess);
        ordering.erase(std::unique(ordering.begin(), ordering.end()),
                       ordering.end());
        if (std::any_of(ordering.begin(), ordering.end(),
                        [&](const Order &order) { return order.Other == m_Key; })) {
            return Failure(Error::OverlayOrderCycle,
                           "A Patch cannot order a Link against itself.");
        }
        const auto [known, inserted] =
            spliceOrdering.emplace(handle.Value, ordering);
        if (!inserted && known->second != ordering) {
            return Failure(
                Error::InvalidState,
                "Actions on one Link in one Patch must share the Link ordering "
                "declaration.");
        }
        out = std::move(ordering);
        return {};
    };

    for (const EditSplice &splice : m_Splices) {
        LinkBase anchored;
        if (Status found = anchoredLink(splice.Target, "Splice", anchored);
            !found) {
            return found;
        }

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
        checked.Target = anchored;
        if (Status ordered = linkOrdering(splice.Target, splice.Ordering,
                                          checked.Ordering);
            !ordered) {
            return ordered;
        }
        checked.Ordinal = splice.Ordinal;
        out.Splices.push_back(std::move(checked));
    }

    std::set<std::uint32_t> redirected;
    for (const EditRedirect &redirect : m_Redirects) {
        LinkBase anchored;
        if (Status found = anchoredLink(redirect.Target, "Redirect", anchored);
            !found) {
            return found;
        }
        if (!redirected.insert(redirect.Target.Value).second)
            return Failure(Error::RedirectConflict,
                           "One Patch cannot redirect a Link twice.");

        CheckedRedirect checked;
        Status status = resolve(redirect.Sink, checked.Sink);
        if (!status)
            return status;
        const bool sinkRoot = checked.Sink.Owner == Graph();
        if (!IsControlSink(sinkRoot, checked.Sink.Slot.Kind)) {
            return Failure(Error::TypeMismatch,
                           "Redirect requires a node In or a graph Exit as its "
                           "new destination.");
        }
        checked.Target = anchored;
        if (Status ordered = linkOrdering(redirect.Target, redirect.Ordering,
                                          checked.Ordering);
            !ordered) {
            return ordered;
        }
        checked.Ordinal = redirect.Ordinal;
        out.Redirects.push_back(std::move(checked));
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
    // The baseline condensation is a DAG. Its inter-component edges must take
    // part in the final search, or a new Flow that closes a cycle through
    // existing same-frame Links would pass as acyclic.
    for (const auto &[from, targets] : baseline) {
        const int source = baseComponents.Of(from);
        for (std::uint64_t target : targets) {
            const int sink = baseComponents.Of(target);
            if (source >= 0 && sink >= 0 && source != sink)
                candidate[static_cast<std::uint64_t>(source)].push_back(
                    static_cast<std::uint64_t>(sink));
        }
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
                           "A parameter can have only one Bind in an Edit.");
        }
        if (bind.Kind == BindKind::Shared)
            shares.emplace_back(DataKey(bind.Target), DataKey(bind.Source));
    }
    const auto operationInputKey = [](ParameterOperation operation,
                                      int index) {
        return (static_cast<std::uint64_t>(operation.Value) << 32u) |
               (static_cast<std::uint64_t>(SlotKind::InputParameter) << 24u) |
               static_cast<std::uint32_t>(index);
    };
    for (const EditOperation &operation : m_Operations) {
        const bool first = operation.Input1.IsValid() &&
            operation.Input1 != CKPGUID_NONE;
        const bool second = operation.Input2.IsValid() &&
            operation.Input2 != CKPGUID_NONE;
        if ((first && !bound.contains(operationInputKey(operation.Handle, 0))) ||
            (second && !bound.contains(operationInputKey(operation.Handle, 1)))) {
            Status invalid = Failure(
                Error::OperationInvalid,
                "Every input of a Parameter Operation must have one Bind.");
            invalid.Details.OperationGuid = operation.Operation;
            return invalid;
        }
    }
    if (HasCycle(shares))
        return Failure(Error::SharedSourceCycle,
                       "The candidate shared-source graph contains a cycle.");

    std::vector<std::pair<std::uint64_t, std::uint64_t>> operationEdges;
    for (const CheckedBind &bind : out.Binds) {
        if (bind.Kind != BindKind::Direct || !bind.Target.Operation ||
            !bind.Source.Operation)
            continue;
        operationEdges.emplace_back(bind.Source.Owner.Value,
                                    bind.Target.Owner.Value);
    }
    if (HasCycle(operationEdges))
        return Failure(Error::OperationInvalid,
                       "The candidate Parameter Operation graph contains a cycle.");

    std::set<std::pair<std::uint64_t, std::uint64_t>> pushed;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> pushes;
    for (const CheckedPush &push : out.Pushes) {
        const auto edge = std::make_pair(
            DataKey(push.Source), DataKey(push.Destination));
        if (!pushed.insert(edge).second) {
            return Failure(Error::InvalidState,
                           "The same Pout destination appears more than once in an Edit.");
        }
        if (bound.count(DataKey(push.Destination)) != 0) {
            return Failure(
                Error::InvalidState,
                "A parameter cannot take both a written value and a Push in an Edit.");
        }
        pushes.push_back(edge);
    }
    if (HasCycle(pushes))
        return Failure(Error::PushCycle,
                       "The candidate Pout destination graph contains a cycle.");
    return {};
}

} // namespace BML::Behavior::Internal

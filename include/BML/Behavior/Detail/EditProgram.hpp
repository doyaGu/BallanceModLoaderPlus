#ifndef BML_BEHAVIOR_DETAIL_EDIT_PROGRAM_HPP
#define BML_BEHAVIOR_DETAIL_EDIT_PROGRAM_HPP

#include "BML/Behavior/Session.hpp"

#include <algorithm>
#include <cstddef>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace BML::Behavior {

namespace Detail {

struct EditStep {
    std::uint32_t Kind = 0;
    std::uint32_t Graph = BML_BEHAVIOR_EDIT_GRAPH;
    std::uint32_t Result = 0;
    std::uint32_t Flags = 0;
    std::uint32_t Target = 0;
    std::uint32_t Node = 0;
    std::uint32_t SlotKind = 0;
    std::int32_t Delay = 0;
    std::int32_t Priority = 0;
    std::string Name;
    Behavior::Selector Selector;
    Behavior::Prototype PrototypeRef;
    Behavior::BehaviorKind ExpectedKind = Behavior::BehaviorKind::Function;
    std::uint64_t PortShape = 0;
    std::optional<NodePattern> Pattern;
    std::shared_ptr<const CompiledBlock> Block;
    CKGUID Type{0, 0};
    Edit::Port Source;
    Edit::Port Sink;
    std::optional<Behavior::Value> Value;
    std::shared_ptr<HookHolder> Hook;
    std::vector<PatchOrder> Ordering;
    BML_ObjectRef Object{};
    CKGUID Operation{0, 0};
    CKGUID OperationResult{0, 0};
    CKGUID OperationInput1{0, 0};
    CKGUID OperationInput2{0, 0};
};

struct EditWire {
    std::vector<BML_BehaviorEditOrder> Ordering;
    std::vector<BML_BehaviorEditStep> Steps;
};

struct EditProgram {
    std::shared_ptr<SessionState> Session;
    int Code = BML_OK;
    Status Failure;
    std::uint32_t NextHandle = BML_BEHAVIOR_EDIT_GRAPH + 1u;
    std::vector<EditStep> Steps;
};

// These records only connect the public On(...) syntax to wire compilation.
// Their concrete shape is deliberately kept out of the domain headers.
struct PatchSymbols {
    std::weak_ptr<EditProgram> Edit;
    std::uint32_t HandleBase = 0;
};

struct PatchTarget {
    std::shared_ptr<SessionState> Session;
    BML_ObjectRef Graph{};
    std::uint64_t Fingerprint = 0;
    std::shared_ptr<const BML::Behavior::Edit> Body;
    std::weak_ptr<EditProgram> Symbols;
    int Code = BML_OK;
    Status Failure;
};

struct PlanRule {
    std::uint32_t Targets = BML_BEHAVIOR_TARGETS_EACH;
    std::string Script;
    std::shared_ptr<const BML::Behavior::Edit> Body;
    int Code = BML_OK;
    Status Failure;
};

} // namespace Detail

inline Edit::Edit() : m_Program(std::make_shared<Detail::EditProgram>()) {}

inline Edit::Graph Edit::Root() const noexcept {
    return Graph{m_Program, BML_BEHAVIOR_EDIT_GRAPH};
}

inline Detail::EditStep &Edit::Define(
    Detail::EditProgram &program, std::uint32_t scope, std::uint32_t kind) {
    return Define(program, scope, kind, program.NextHandle++);
}

inline Detail::EditStep &Edit::Define(
    Detail::EditProgram &program, std::uint32_t scope, std::uint32_t kind,
    std::uint32_t result) {
    Detail::EditStep step;
    step.Kind = kind;
    step.Graph = scope;
    step.Result = result;
    program.Steps.push_back(std::move(step));
    return program.Steps.back();
}

inline Edit::Port Edit::Append(
    const std::shared_ptr<Detail::EditProgram> &program,
    std::uint32_t scope, Node owner, std::uint32_t slotKind,
    std::string_view name, CKGUID type) {
    if (!Require(*program, scope, owner, "Appended port owner"))
        return {};
    Detail::EditStep &step = Define(
        *program, scope, BML_BEHAVIOR_EDIT_APPEND_SLOT);
    step.Target = owner.m_Id;
    step.SlotKind = slotKind;
    step.Name.assign(name);
    step.Type = type;
    return Port{program, scope, step.Result, 0, CKGUID(0, 0), {}};
}

template <class Symbol>
inline bool Edit::Require(Detail::EditProgram &program, std::uint32_t scope,
                          const Symbol &symbol, std::string_view role) {
    const std::shared_ptr<Detail::EditProgram> owner = symbol.m_Edit.lock();
    if (owner.get() == &program && symbol.m_Id != 0 &&
        symbol.m_Scope == scope)
        return true;
    if (program.Code == BML_OK) {
        program.Code = BML_ERROR_INVALID_PARAMETER;
        program.Failure.Error = Behavior::Error::GraphLocalityInvalid;
        program.Failure.Phase = Behavior::Phase::Edit;
        program.Failure.Message = std::string(role) +
            " belongs to a different Behavior Edit.";
    }
    return false;
}

inline void Edit::RejectLiveView(
    Detail::EditProgram &program, std::string_view kind) {
    if (program.Code != BML_OK)
        return;
    program.Code = BML_ERROR_INVALID_PARAMETER;
    program.Failure.Error = Behavior::Error::GraphLocalityInvalid;
    program.Failure.Phase = Behavior::Phase::Edit;
    program.Failure.Message = "An empty Graph " + std::string(kind) +
        " cannot be used by a Behavior Edit.";
}

inline Edit::Graph Edit::EnterGraph(
    const std::shared_ptr<Detail::EditProgram> &program,
    std::uint32_t scope, const Node &node) {
    if (!Require(*program, scope, node, "Nested graph"))
        return {};
    Detail::EditStep &step = Define(
        *program, scope, BML_BEHAVIOR_EDIT_ENTER_GRAPH);
    step.Target = node.m_Id;
    return Graph{program, step.Result};
}

inline std::uint64_t Edit::Shape(const Behavior::Node &node) {
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
    for (Behavior::Port port : node.Ports()) {
        const auto kind = static_cast<std::uint32_t>(port.Kind());
        const auto index = port.Index();
        const auto occurrence = port.Occurrence();
        const CKGUID type = port.Type();
        append(&kind, sizeof(kind));
        append(&index, sizeof(index));
        append(&occurrence, sizeof(occurrence));
        append(&type.d1, sizeof(type.d1));
        append(&type.d2, sizeof(type.d2));
        const std::string_view name = port.Name();
        append(name.data(), name.size());
    }
    return hash;
}

inline std::shared_ptr<const Edit> Edit::Snapshot() const {
    if (!m_Program)
        throw std::logic_error("The Behavior Edit no longer exists.");
    auto copy = std::make_shared<Edit>();
    copy->m_Program->Session = m_Program->Session;
    copy->m_Program->Code = m_Program->Code;
    copy->m_Program->Failure = m_Program->Failure;
    copy->m_Program->NextHandle = m_Program->NextHandle;
    copy->m_Program->Steps = m_Program->Steps;
    return copy;
}

inline Edit::Graph Edit::Node::Graph() const {
    const std::shared_ptr<Detail::EditProgram> program = m_Edit.lock();
    if (!program)
        throw std::logic_error("The Behavior Edit no longer exists.");
    return Edit::EnterGraph(program, m_Scope, *this);
}

inline std::shared_ptr<Detail::EditProgram> Edit::Graph::Program() const {
    std::shared_ptr<Detail::EditProgram> program = m_Edit.lock();
    if (!program)
        throw std::logic_error("The Behavior Edit no longer exists.");
    return program;
}

inline Edit::Node Edit::Graph::Root() const {
    return Node{m_Edit, m_Scope, BML_BEHAVIOR_EDIT_GRAPH};
}

inline Edit::Node Edit::Graph::Require(Behavior::Selector selector,
                                      CKGUID prototype) const {
    NodePattern pattern(std::move(selector));
    pattern.Prototype(prototype);
    return Require(std::move(pattern));
}

inline Edit::Node Edit::Graph::Require(std::string_view name,
                                      CKGUID prototype) const {
    return Require(Behavior::Unique(name), prototype);
}

inline Edit::Node Edit::Graph::Require(CKGUID prototype) const {
    return Require(Behavior::Selector::Only(), prototype);
}

inline Edit::Node Edit::Graph::Require(NodePattern pattern) const {
    const auto edit = Program();
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_REQUIRE_NODE);
    step.Pattern.emplace(std::move(pattern));
    return Node{m_Edit, m_Scope, step.Result};
}

inline Edit::Nodes Edit::Graph::Each(NodePattern pattern) const {
    const auto edit = Program();
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_EACH_NODE);
    step.Pattern.emplace(std::move(pattern));
    return Nodes{m_Edit, m_Scope, step.Result};
}

inline Edit::Nodes Edit::Graph::Each(std::string_view name) const {
    return Each(NodePattern(name));
}

inline Edit::Node Edit::Graph::Require(const Behavior::Node &node) const {
    const auto edit = Program();
    if (!node) {
        Edit::RejectLiveView(*edit, "Node");
        return {};
    }
    if (node.Index() < 0)
        return Root();
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_REQUIRE_NODE);
    NodePattern pattern(node.Name().empty()
                            ? Behavior::Selector::Only()
                            : Behavior::Unique(node.Name()));
    pattern.m_Prototype = node.Prototype();
    pattern.m_Kind = node.Kind();
    pattern.m_PortShape = Edit::Shape(node);
    step.Pattern.emplace(std::move(pattern));
    return Node{m_Edit, m_Scope, step.Result};
}

inline Edit::Link Edit::Graph::Require(const Behavior::Link &link) const {
    const auto edit = Program();
    if (!link) {
        Edit::RejectLiveView(*edit, "Link");
        return {};
    }
    const Behavior::Port source = link.Source();
    const Behavior::Port sink = link.Target();
    const Behavior::Node sourceNode{
        source.m_Graph, source.m_Graph->Ports[source.m_Index].Node};
    const Behavior::Node sinkNode{
        sink.m_Graph, sink.m_Graph->Ports[sink.m_Index].Node};
    Node from = Require(sourceNode);
    Node to = Require(sinkNode);
    const auto slot = [](const Behavior::Port &port) {
        return port.Name().empty()
            ? Behavior::At(port.Index())
            : Behavior::Named(port.Name(), port.Occurrence());
    };
    return Between(from.Out(slot(source)), to.In(slot(sink)),
                   link.InitialDelay());
}

inline Edit::Node Edit::Graph::Use(const Behavior::Node &node) const {
    const auto edit = Program();
    if (!node) {
        Edit::RejectLiveView(*edit, "Node");
        return {};
    }
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_USE_NODE);
    step.Object = node.Object();
    return Node{m_Edit, m_Scope, step.Result};
}

inline Edit::Link Edit::Graph::Use(const Behavior::Link &link) const {
    const auto edit = Program();
    if (!link) {
        Edit::RejectLiveView(*edit, "Link");
        return {};
    }
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_USE_LINK);
    step.Object = link.Object();
    return Link{m_Edit, m_Scope, step.Result};
}

inline Edit::Link Edit::Graph::Between(Port source, Port sink) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, source, "Flow source") ||
        !Edit::Require(*edit, m_Scope, sink, "Flow sink"))
        return {};
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_REQUIRE_LINK);
    step.Source = std::move(source);
    step.Sink = std::move(sink);
    return Link{m_Edit, m_Scope, step.Result};
}

inline Edit::Link Edit::Graph::Between(Port source, Port sink,
                                      std::int32_t delay) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, source, "Flow source") ||
        !Edit::Require(*edit, m_Scope, sink, "Flow sink"))
        return {};
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_REQUIRE_LINK);
    step.Source = std::move(source);
    step.Sink = std::move(sink);
    step.Flags |= BML_BEHAVIOR_EDIT_HAS_DELAY;
    step.Delay = delay;
    return Link{m_Edit, m_Scope, step.Result};
}

inline Edit::Node Edit::Graph::Next(Port source) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, source, "Next source"))
        return {};
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_NEXT_NODE);
    step.Source = std::move(source);
    return Node{m_Edit, m_Scope, step.Result};
}

inline Edit::Node Edit::Graph::Next(Port source, NodePattern expected) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, source, "Next source"))
        return {};
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_NEXT_NODE);
    step.Source = std::move(source);
    step.Pattern.emplace(std::move(expected));
    return Node{m_Edit, m_Scope, step.Result};
}

inline Edit::Node Edit::Graph::Next(Node source) const {
    return Next(source.Out());
}

inline Edit::Node Edit::Graph::Next(
    Node source, Behavior::Selector output) const {
    return Next(source.Out(std::move(output)));
}

inline Edit::Node Edit::Graph::Next(Node source, std::int32_t output) const {
    return Next(std::move(source), Behavior::At(output));
}

inline Edit::Node Edit::Graph::Previous(Port sink) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, sink, "Previous sink"))
        return {};
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_PREVIOUS_NODE);
    step.Sink = std::move(sink);
    return Node{m_Edit, m_Scope, step.Result};
}

inline Edit::Node Edit::Graph::Previous(Port sink, NodePattern expected) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, sink, "Previous sink"))
        return {};
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_PREVIOUS_NODE);
    step.Sink = std::move(sink);
    step.Pattern.emplace(std::move(expected));
    return Node{m_Edit, m_Scope, step.Result};
}

inline Edit::Node Edit::Graph::Previous(Node sink) const {
    return Previous(sink.In());
}

inline Edit::Node Edit::Graph::Previous(
    Node sink, Behavior::Selector input) const {
    return Previous(sink.In(std::move(input)));
}

inline Edit::Node Edit::Graph::Previous(Node sink, std::int32_t input) const {
    return Previous(std::move(sink), Behavior::At(input));
}

inline Edit::Link Edit::Graph::Leaving(Port source) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, source, "Leaving source"))
        return {};
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_LEAVING_LINK);
    step.Source = std::move(source);
    return Link{m_Edit, m_Scope, step.Result};
}

inline Edit::Link Edit::Graph::Leaving(Node source) const {
    return Leaving(source.Out());
}

inline Edit::Link Edit::Graph::Leaving(
    Node source, Behavior::Selector output) const {
    return Leaving(source.Out(std::move(output)));
}

inline Edit::Link Edit::Graph::Leaving(Node source,
                                       std::int32_t output) const {
    return Leaving(std::move(source), Behavior::At(output));
}

inline Edit::Link Edit::Graph::Entering(Port sink) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, sink, "Entering sink"))
        return {};
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_ENTERING_LINK);
    step.Sink = std::move(sink);
    return Link{m_Edit, m_Scope, step.Result};
}

inline Edit::Link Edit::Graph::Entering(Node sink) const {
    return Entering(sink.In());
}

inline Edit::Link Edit::Graph::Entering(
    Node sink, Behavior::Selector input) const {
    return Entering(sink.In(std::move(input)));
}

inline Edit::Link Edit::Graph::Entering(Node sink,
                                        std::int32_t input) const {
    return Entering(std::move(sink), Behavior::At(input));
}

inline Edit::Link Edit::Graph::To(Port source, Node target) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, source, "Link source") ||
        !Edit::Require(*edit, m_Scope, target, "Link target"))
        return {};
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_LINK_TO_NODE);
    step.Source = std::move(source);
    step.Target = target.m_Id;
    return Link{m_Edit, m_Scope, step.Result};
}

inline Edit::Path Edit::Graph::Follow(Port source) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, source, "Path source"))
        return {};
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_FOLLOW);
    step.Source = std::move(source);
    return Path{m_Edit, m_Scope, step.Result};
}

inline Edit::Node Edit::Graph::Add(const Block &block) const {
    const auto edit = Program();
    const Prototype fallback = block.m_State
        ? block.m_State->Spec.PrototypeRef : Prototype{};
    auto compiled = block.Compile();
    if (!compiled) {
        if (edit->Code == BML_OK) {
            edit->Code = compiled.Code();
            edit->Failure = compiled.GetStatus();
        }
        Detail::EditStep &failed = Edit::Define(
            *edit, m_Scope, BML_BEHAVIOR_EDIT_ADD_BLOCK);
        failed.PrototypeRef = fallback;
        return Node{m_Edit, m_Scope, failed.Result};
    }
    if (edit->Session && edit->Session != block.m_Session &&
        edit->Code == BML_OK) {
        edit->Code = BML_ERROR_INVALID_PARAMETER;
        edit->Failure.Error = Behavior::Error::OwnerUnavailable;
        edit->Failure.Phase = Behavior::Phase::Edit;
        edit->Failure.Message =
            "Every Block in an Edit must belong to the same Session.";
    } else if (!edit->Session) {
        edit->Session = block.m_Session;
    }
    const std::shared_ptr<const Detail::CompiledBlock> compiledBlock =
        compiled.Value();
    if (!compiledBlock->Spec.PrototypeRef.Generation && edit->Code == BML_OK) {
        edit->Code = BML_ERROR_UNAVAILABLE;
        edit->Failure.Error = Behavior::Error::Unavailable;
        edit->Failure.Phase = Behavior::Phase::Prototype;
        edit->Failure.Prototype = compiledBlock->Spec.PrototypeRef.Id;
        edit->Failure.Message =
            "A Block must resolve its Prototype provider before it can be added to an Edit.";
    }
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_ADD_BLOCK);
    step.Block = compiledBlock;
    return Node{m_Edit, m_Scope, step.Result};
}

inline Edit::Node Edit::Graph::AddGraph(std::string_view name,
                                       std::int32_t priority) const {
    const auto edit = Program();
    if (name.empty()) {
        if (edit->Code == BML_OK) {
            edit->Code = BML_ERROR_INVALID_PARAMETER;
            edit->Failure.Error = Behavior::Error::ValueInvalid;
            edit->Failure.Phase = Behavior::Phase::Edit;
            edit->Failure.Message = "A graph-backed Node requires a name.";
        }
        return {};
    }
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_ADD_GRAPH);
    step.Name.assign(name);
    step.Priority = priority;
    return Node{m_Edit, m_Scope, step.Result};
}

inline Edit::Node Edit::Graph::Replace(Node target, const Block &block) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, target, "Replacement target"))
        return {};
    Node replacement = Add(block);
    if (!edit->Steps.empty()) {
        Detail::EditStep &step = edit->Steps.back();
        step.Kind = BML_BEHAVIOR_EDIT_REPLACE_BLOCK;
        step.Target = target.m_Id;
    }
    return replacement;
}

inline Edit::Graph Edit::Graph::Remove(Node target) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, target, "Removal target"))
        return *this;
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_REMOVE_NODE, 0);
    step.Target = target.m_Id;
    return *this;
}

inline Edit::Operation Edit::Graph::AddOperation(
    CKGUID operation, CKGUID result, CKGUID input1, CKGUID input2) const {
    const auto edit = Program();
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_ADD_OPERATION);
    step.Operation = operation;
    step.OperationResult = result;
    step.OperationInput1 = input1;
    step.OperationInput2 = input2;
    return Operation{m_Edit, m_Scope, step.Result, result, input1, input2};
}

inline Edit::Port Edit::Graph::AppendIn(std::string_view name) const {
    return AppendIn(Root(), name);
}

inline Edit::Port Edit::Graph::AppendIn(Node owner,
                                       std::string_view name) const {
    return Edit::Append(
        Program(), m_Scope, std::move(owner), BML_BEHAVIOR_SLOT_IN,
        name, CKGUID(0, 0));
}

inline Edit::Port Edit::Graph::AppendOut(std::string_view name) const {
    return AppendOut(Root(), name);
}

inline Edit::Port Edit::Graph::AppendOut(Node owner,
                                        std::string_view name) const {
    return Edit::Append(
        Program(), m_Scope, std::move(owner), BML_BEHAVIOR_SLOT_OUT,
        name, CKGUID(0, 0));
}

inline Edit::Port Edit::Graph::AppendPin(std::string_view name,
                                        CKGUID type) const {
    return AppendPin(Root(), name, type);
}

inline Edit::Port Edit::Graph::AppendPin(Node owner, std::string_view name,
                                        CKGUID type) const {
    return Edit::Append(
        Program(), m_Scope, std::move(owner), BML_BEHAVIOR_SLOT_PIN,
        name, type);
}

inline Edit::Port Edit::Graph::AppendPout(std::string_view name,
                                         CKGUID type) const {
    return AppendPout(Root(), name, type);
}

inline Edit::Port Edit::Graph::AppendPout(Node owner, std::string_view name,
                                         CKGUID type) const {
    return Edit::Append(
        Program(), m_Scope, std::move(owner), BML_BEHAVIOR_SLOT_POUT,
        name, type);
}

inline Edit::Port Edit::Graph::AppendLocal(std::string_view name,
                                          CKGUID type) const {
    return AppendLocal(Root(), name, type);
}

inline Edit::Port Edit::Graph::AppendLocal(Node owner,
                                          std::string_view name,
                                          CKGUID type) const {
    return Edit::Append(
        Program(), m_Scope, std::move(owner), BML_BEHAVIOR_SLOT_LOCAL,
        name, type);
}

inline Edit::Graph Edit::Graph::Flow(Port source, Port sink,
                                     std::int32_t delay) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, source, "Flow source") ||
        !Edit::Require(*edit, m_Scope, sink, "Flow sink"))
        return *this;
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_FLOW, 0);
    step.Source = std::move(source);
    step.Sink = std::move(sink);
    step.Delay = delay;
    return *this;
}

inline Edit::Graph Edit::Graph::Flow(Ports sources, Port sink,
                                     std::int32_t delay) const {
    return Flow(std::move(sources.m_Port), std::move(sink), delay);
}

inline Edit::Graph Edit::Graph::Flow(Port source, Ports sinks,
                                     std::int32_t delay) const {
    return Flow(std::move(source), std::move(sinks.m_Port), delay);
}

inline Edit::Graph Edit::Graph::FlowCycle(Port source, Port sink,
                                          std::int32_t delay) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, source, "Flow source") ||
        !Edit::Require(*edit, m_Scope, sink, "Flow sink"))
        return *this;
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_FLOW, 0);
    step.Source = std::move(source);
    step.Sink = std::move(sink);
    step.Delay = delay;
    step.Flags |= BML_BEHAVIOR_EDIT_CONFIRM_CYCLE;
    return *this;
}

inline Edit::Graph Edit::Graph::FlowCycle(Ports sources, Port sink,
                                          std::int32_t delay) const {
    return FlowCycle(std::move(sources.m_Port), std::move(sink), delay);
}

inline Edit::Graph Edit::Graph::FlowCycle(Port source, Ports sinks,
                                          std::int32_t delay) const {
    return FlowCycle(std::move(source), std::move(sinks.m_Port), delay);
}

inline Edit::Graph Edit::Graph::Bind(Port sink, Behavior::Value value) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, sink, "Bind sink"))
        return *this;
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_BIND_VALUE, 0);
    step.Sink = std::move(sink);
    step.Value.emplace(std::move(value));
    return *this;
}

inline Edit::Graph Edit::Graph::Bind(Ports sinks, Behavior::Value value) const {
    return Bind(std::move(sinks.m_Port), std::move(value));
}

template <class T, class>
inline Edit::Graph Edit::Graph::Bind(Ports sinks, T &&value) const {
    return Bind(std::move(sinks), Behavior::Value(std::forward<T>(value)));
}

template <class T, class>
inline Edit::Graph Edit::Graph::Bind(Port sink, T &&value) const {
    return Bind(std::move(sink), Behavior::Value(std::forward<T>(value)));
}

inline Edit::Graph Edit::Graph::Bind(Port sink, Port source) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, sink, "Bind sink") ||
        !Edit::Require(*edit, m_Scope, source, "Bind source"))
        return *this;
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_BIND_PORT, 0);
    step.Sink = std::move(sink);
    step.Source = std::move(source);
    return *this;
}

inline Edit::Graph Edit::Graph::Bind(Ports sinks, Port source) const {
    return Bind(std::move(sinks.m_Port), std::move(source));
}

inline Edit::Graph Edit::Graph::Share(Port sink, Port source) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, sink, "Share sink") ||
        !Edit::Require(*edit, m_Scope, source, "Share source"))
        return *this;
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_SHARE, 0);
    step.Sink = std::move(sink);
    step.Source = std::move(source);
    return *this;
}

inline Edit::Graph Edit::Graph::Share(Ports sinks, Port source) const {
    return Share(std::move(sinks.m_Port), std::move(source));
}

inline Edit::Graph Edit::Graph::Push(Port source, Port sink) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, source, "Push source") ||
        !Edit::Require(*edit, m_Scope, sink, "Push sink"))
        return *this;
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_PUSH, 0);
    step.Source = std::move(source);
    step.Sink = std::move(sink);
    return *this;
}

inline Edit::Graph Edit::Graph::Push(Ports sources, Port sink) const {
    return Push(std::move(sources.m_Port), std::move(sink));
}

inline Edit::Graph Edit::Graph::Push(Port source, Ports sinks) const {
    return Push(std::move(source), std::move(sinks.m_Port));
}

inline Edit::Graph Edit::Graph::Tap(Port source, Hook hook) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, source, "Tap source"))
        return *this;
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_TAP, 0);
    step.Source = std::move(source);
    step.Hook = std::move(hook.m_Record);
    return *this;
}

inline Edit::Graph Edit::Graph::Tap(Ports sources, Hook hook) const {
    return Tap(std::move(sources.m_Port), std::move(hook));
}

inline Edit::Graph Edit::Graph::Before(Link link, Hook hook) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, link, "Before Link"))
        return *this;
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_BEFORE, 0);
    step.Target = link.m_Id;
    step.Hook = std::move(hook.m_Record);
    return *this;
}

inline Edit::Graph Edit::Graph::After(Path path, Hook hook) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, path, "After Path"))
        return *this;
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_AFTER, 0);
    step.Target = path.m_Id;
    step.Hook = std::move(hook.m_Record);
    return *this;
}

inline Edit::Graph Edit::Graph::After(Port source, Hook hook) const {
    return After(Follow(std::move(source)), std::move(hook));
}

inline Edit::Graph Edit::Graph::Splice(
    Link link, Node through, std::vector<PatchOrder> ordering) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, link, "Splice Link") ||
        !Edit::Require(*edit, m_Scope, through, "Splice Block"))
        return *this;
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_SPLICE, 0);
    step.Target = link.m_Id;
    step.Node = through.m_Id;
    step.Ordering = std::move(ordering);
    return *this;
}

inline Edit::Graph Edit::Graph::Splice(
    Link link, Port sink, Port source,
    std::vector<PatchOrder> ordering) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, link, "Splice Link") ||
        !Edit::Require(*edit, m_Scope, sink, "Splice input") ||
        !Edit::Require(*edit, m_Scope, source, "Splice output"))
        return *this;
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_SPLICE, 0);
    step.Target = link.m_Id;
    step.Sink = std::move(sink);
    step.Source = std::move(source);
    step.Ordering = std::move(ordering);
    return *this;
}

inline Edit::Graph Edit::Graph::Redirect(
    Link link, Port sink, std::vector<PatchOrder> ordering) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, link, "Redirect Link") ||
        !Edit::Require(*edit, m_Scope, sink, "Redirect sink"))
        return *this;
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_REDIRECT, 0);
    step.Target = link.m_Id;
    step.Sink = std::move(sink);
    step.Ordering = std::move(ordering);
    return *this;
}

inline Edit::Graph Edit::Graph::Reconnect(
    Link link, Port source, Port sink) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, link, "Reconnect Link") ||
        !Edit::Require(*edit, m_Scope, source, "Reconnect source") ||
        !Edit::Require(*edit, m_Scope, sink, "Reconnect sink"))
        return *this;
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_RECONNECT, 0);
    step.Target = link.m_Id;
    step.Source = std::move(source);
    step.Sink = std::move(sink);
    return *this;
}

inline Edit::Graph Edit::Graph::ReconnectCycle(
    Link link, Port source, Port sink) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, link, "Reconnect Link") ||
        !Edit::Require(*edit, m_Scope, source, "Reconnect source") ||
        !Edit::Require(*edit, m_Scope, sink, "Reconnect sink"))
        return *this;
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_RECONNECT, 0);
    step.Target = link.m_Id;
    step.Source = std::move(source);
    step.Sink = std::move(sink);
    step.Flags |= BML_BEHAVIOR_EDIT_CONFIRM_CYCLE;
    return *this;
}

inline Edit::Graph Edit::Graph::Redirect(
    Link link, Link destination, std::vector<PatchOrder> ordering) const {
    const auto edit = Program();
    if (!Edit::Require(*edit, m_Scope, link, "Redirect Link") ||
        !Edit::Require(*edit, m_Scope, destination,
                       "Redirect destination Link"))
        return *this;
    Detail::EditStep &step = Edit::Define(
        *edit, m_Scope, BML_BEHAVIOR_EDIT_REDIRECT_TO_LINK, 0);
    step.Target = link.m_Id;
    step.Node = destination.m_Id;
    step.Ordering = std::move(ordering);
    return *this;
}

inline void Edit::Encode(Detail::EditWire &out) const {
    const auto encodePort = [](const Port &source) {
        BML_BehaviorPortRef port{};
        port.StructSize = sizeof(port);
        port.Graph = source.m_Scope;
        port.Handle = source.m_Id;
        port.Kind = source.m_Kind;
        port.Type = Detail::WireGuid(source.m_Type);
        port.Slot = Detail::Wire::From(source.m_Slot);
        return port;
    };
    std::size_t orderCount = 0;
    for (const Detail::EditStep &step : m_Program->Steps)
        orderCount += step.Ordering.size();
    // Both arrays are sized up front, so nothing a step points at moves.
    out.Ordering.clear();
    out.Ordering.reserve(orderCount);
    for (const Detail::EditStep &step : m_Program->Steps) {
        for (const PatchOrder &order : step.Ordering) {
            BML_BehaviorEditOrder wire{};
            wire.StructSize = sizeof(wire);
            wire.Kind = order.Kind;
            wire.Owner = Detail::Text(order.Owner);
            wire.Name = Detail::Text(order.Name);
            out.Ordering.push_back(wire);
        }
    }

    std::size_t patternSteps = 0;
    for (const Detail::EditStep &step : m_Program->Steps) {
        if (step.Pattern)
            patternSteps += step.Pattern->m_Counts.size() +
                step.Pattern->m_Values.size();
    }
    out.Steps.clear();
    out.Steps.reserve(m_Program->Steps.size() + patternSteps);
    std::size_t consumed = 0;
    for (const Detail::EditStep &step : m_Program->Steps) {
        BML_BehaviorEditStep wire{};
        wire.StructSize = sizeof(wire);
        wire.Kind = step.Kind;
        wire.Graph = step.Graph;
        wire.Result = step.Result;
        wire.Flags = step.Flags;
        wire.Target = step.Target;
        wire.Node = step.Node;
        wire.SlotKind = step.SlotKind;
        wire.Delay = step.Delay;
        wire.Priority = step.Priority;
        wire.Name = Detail::Text(step.Name);
        const NodePattern *pattern = step.Pattern ? &*step.Pattern : nullptr;
        wire.Selector = Detail::Wire::From(
            pattern ? pattern->m_Selector : step.Selector);
        wire.Prototype.StructSize = sizeof(wire.Prototype);
        wire.Prototype.Prototype = Detail::WireGuid(
            pattern ? pattern->m_Prototype : step.PrototypeRef.Id);
        wire.Prototype.Generation = pattern ? 0 : step.PrototypeRef.Generation;
        wire.ExpectedKind = pattern && pattern->m_Kind
            ? static_cast<std::uint32_t>(*pattern->m_Kind)
            : step.PortShape
                ? static_cast<std::uint32_t>(step.ExpectedKind) : 0;
        wire.PortShape = pattern ? pattern->m_PortShape : step.PortShape;
        wire.Block = step.Block ? &step.Block->Wire : nullptr;
        wire.Type = Detail::WireGuid(step.Type);
        wire.Source = encodePort(step.Source);
        wire.Sink = encodePort(step.Sink);
        if (step.Value)
            wire.Value = Detail::Wire::From(*step.Value);
        wire.Hook = step.Hook ? &step.Hook->Function : nullptr;
        wire.Object = step.Object;
        wire.Operation.StructSize = sizeof(wire.Operation);
        wire.Operation.Operation = Detail::WireGuid(step.Operation);
        wire.Operation.Result = Detail::WireGuid(step.OperationResult);
        wire.Operation.Input1 = Detail::WireGuid(step.OperationInput1);
        wire.Operation.Input2 = Detail::WireGuid(step.OperationInput2);
        wire.OrderCount = static_cast<std::uint32_t>(step.Ordering.size());
        wire.Ordering = wire.OrderCount
            ? out.Ordering.data() + consumed : nullptr;
        consumed += step.Ordering.size();
        out.Steps.push_back(wire);

        if (!pattern)
            continue;
        for (const NodePattern::PortCount &condition : pattern->m_Counts) {
            BML_BehaviorEditStep count{};
            count.StructSize = sizeof(count);
            count.Kind = BML_BEHAVIOR_EDIT_PATTERN_PORT_COUNT;
            count.Graph = step.Graph;
            count.Target = step.Result;
            count.SlotKind = static_cast<std::uint32_t>(condition.Kind);
            count.Delay = condition.Count;
            out.Steps.push_back(count);
        }
        for (const NodePattern::PortValue &condition : pattern->m_Values) {
            BML_BehaviorEditStep value{};
            value.StructSize = sizeof(value);
            value.Kind = BML_BEHAVIOR_EDIT_PATTERN_PORT_VALUE;
            value.Graph = step.Graph;
            value.Target = step.Result;
            value.Sink.StructSize = sizeof(value.Sink);
            value.Sink.Graph = step.Graph;
            value.Sink.Handle = step.Result;
            value.Sink.Kind = static_cast<std::uint32_t>(condition.Kind);
            value.Sink.Type = Detail::WireGuid(condition.Expected.Type());
            value.Sink.Slot = Detail::Wire::From(condition.Slot);
            value.Value = Detail::Wire::From(condition.Expected);
            out.Steps.push_back(value);
        }
    }
}

inline Result<void> Edit::Validate(
    const std::shared_ptr<Detail::SessionState> &session) const {
    if (!m_Program)
        return Result<void>::Failure(BML_ERROR_INVALID_PARAMETER);
    if (m_Program->Code != BML_OK)
        return Result<void>::Failure(
            m_Program->Code, m_Program->Failure);
    if (!session || !session->Api || !session->Handle)
        return Result<void>::Failure(BML_ERROR_INVALID_HANDLE);
    if (m_Program->Session && m_Program->Session != session) {
        Status status;
        status.Error = Behavior::Error::OwnerUnavailable;
        status.Phase = Behavior::Phase::Edit;
        status.Message =
            "The Edit and its destination belong to different Sessions.";
        return Result<void>::Failure(BML_ERROR_INVALID_PARAMETER,
                                     std::move(status));
    }
    return Result<void>::Success();
}

inline Result<void> Edit::ValidatePlan(
    const std::shared_ptr<Detail::SessionState> &session) const {
    Result<void> valid = Validate(session);
    if (!valid)
        return valid;
    for (const Detail::EditStep &step : m_Program->Steps) {
        bool liveReference =
            step.Kind == BML_BEHAVIOR_EDIT_USE_NODE ||
            step.Kind == BML_BEHAVIOR_EDIT_USE_LINK ||
            (step.Value && step.Value->Kind() == ValueKind::Object &&
             !step.Value->IsNull());
        if (step.Block) {
            const Detail::BlockSpec &block = step.Block->Spec;
            liveReference = liveReference ||
                (block.TargetKind == BML_BEHAVIOR_TARGET_OBJECT &&
                 Detail::ValidObjectRef(block.TargetObject));
            const auto containsObject = [](const auto &values) {
                return std::any_of(values.begin(), values.end(),
                    [](const SlotValue &value) {
                        return value.Data.Kind() == ValueKind::Object &&
                            !value.Data.IsNull();
                    });
            };
            liveReference = liveReference || containsObject(block.Pins) ||
                containsObject(block.Locals);
            for (const auto &stage : block.Settings)
                liveReference = liveReference || containsObject(stage);
        }
        if (!liveReference)
            continue;
        Status status;
        status.Error = Behavior::Error::WorldBoundValue;
        status.Phase = Behavior::Phase::Edit;
        status.Message =
            "A Plan cannot retain an object reference from one live world.";
        return Result<void>::Failure(BML_ERROR_INVALID_PARAMETER,
                                     std::move(status));
    }
    return Result<void>::Success();
}

inline Result<Behavior::Script> Session::CreateScript(
    BML_ObjectRef owner, std::string_view name, const Edit &body,
    std::int32_t priority) const {
    if (!*this)
        return Result<Behavior::Script>::Failure(BML_ERROR_INVALID_HANDLE);
    if (!BML_IFACE_HAS(m_State->Api, BML_BehaviorInterface, CreateScript) ||
        !owner.Domain)
        return Result<Behavior::Script>::Failure(
            owner.Domain ? BML_ERROR_VERSION_MISMATCH
                         : BML_ERROR_INVALID_PARAMETER);

    Result<void> valid = body.Validate(m_State);
    if (!valid)
        return Result<Behavior::Script>::Failure(
            valid.Code(), valid.GetStatus());
    try {
        Detail::EditWire program;
        body.Encode(program);
        BML_BehaviorScriptSpec spec{};
        spec.StructSize = sizeof(spec);
        spec.Owner = owner;
        spec.Name = Detail::Text(name);
        spec.Priority = priority;
        spec.StepCount = static_cast<std::uint32_t>(program.Steps.size());
        spec.Steps = program.Steps.empty() ? nullptr : program.Steps.data();
        BML_BehaviorScript handle = nullptr;
        BML_BehaviorScriptInfo info{};
        info.StructSize = sizeof(info);
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = Detail::WireCode(
            m_State->Api->CreateScript(
                m_State->Handle, &spec, &handle, &info, &status), status);
        Behavior::Script owned(m_State, handle, info.Root);
        if (code != BML_OK || !handle)
            return Result<Behavior::Script>::Failure(
                code == BML_OK ? BML_ERROR_MALFORMED_MESSAGE : code,
                Detail::ReadStatus(status));
        if (!Detail::ValidScriptInfo(info) ||
            !Detail::SameScriptObject(info.Owner, owner))
            return Result<Behavior::Script>::Failure(
                BML_ERROR_MALFORMED_MESSAGE,
                Detail::ReadStatus(status));
        return Result<Behavior::Script>::Success(
            std::move(owned), Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<Behavior::Script>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Behavior::Script>::Failure(BML_ERROR_FAIL);
    }
}

template <class Function, class>
inline Hook::Hook(Function &&callback) {
    using Holder = Detail::HookFunction<Function>;
    auto holder = std::make_unique<Holder>(std::forward<Function>(callback));
    auto record = std::make_shared<Detail::HookHolder>();
    record->Function.StructSize = sizeof(record->Function);
    record->Function.State = holder.get();
    record->Function.Retain = &Holder::Retain;
    record->Function.Release = &Holder::Release;
    record->Function.Invoke = &Holder::Invoke;
    (void) holder.release();
    m_Record = std::move(record);
}

inline Hook::operator bool() const noexcept {
    return m_Record != nullptr;
}

inline Plan::~Plan() {
    (void) Close();
}

inline Plan::Plan(Plan &&other) noexcept
    : m_Session(std::move(other.m_Session)),
      m_Handle(std::exchange(other.m_Handle, nullptr)) {}

inline Plan &Plan::operator=(Plan &&other) noexcept {
    if (this != &other) {
        Plan previous(std::move(*this));
        m_Session = std::move(other.m_Session);
        m_Handle = std::exchange(other.m_Handle, nullptr);
    }
    return *this;
}

inline Plan::operator bool() const noexcept {
    return m_Session && m_Session->Api && m_Session->Handle && m_Handle;
}

template <class First, class... More>
inline Result<PlanInfo> Plan::Replace(First &&first, More &&...more) {
    static_assert(
        std::is_same_v<std::decay_t<First>, Detail::PlanRule> &&
            (std::is_same_v<std::decay_t<More>, Detail::PlanRule> && ...),
        "Behavior Plan::Replace expects On(Scripts, Edit) entries.");
    std::vector<Detail::PlanRule> rules;
    try {
        rules.reserve(1 + sizeof...(more));
        rules.push_back(std::forward<First>(first));
        (rules.push_back(std::forward<More>(more)), ...);
    } catch (const std::bad_alloc &) {
        return Result<PlanInfo>::Failure(BML_ERROR_OUT_OF_MEMORY);
    }
    return Replace(std::move(rules));
}

inline Result<PlanInfo> Plan::Info() const {
    if (!*this)
        return Result<PlanInfo>::Failure(BML_ERROR_INVALID_HANDLE);
    BML_BehaviorPlanInfo wire{};
    wire.StructSize = sizeof(wire);
    BML_BehaviorStatus status = Detail::EmptyStatus();
    const int code = Detail::WireCode(
        m_Session->Api->ReadPlan(
            m_Session->Handle, m_Handle, &wire, &status),
        status);
    if (code != BML_OK)
        return Result<PlanInfo>::Failure(code, Detail::ReadStatus(status));
    return Detail::CompletePlanInfo(m_Session, m_Handle, wire, status);
}

inline Result<CloseState> Plan::Close() noexcept {
    if (!m_Handle) {
        m_Session.reset();
        return Result<CloseState>::Success(CloseState::Closed);
    }
    if (!m_Session || !m_Session->Api || !m_Session->Handle)
        return Result<CloseState>::Failure(BML_ERROR_INVALID_HANDLE);
    const int code = m_Session->Api->ClosePlan(m_Session->Handle, m_Handle);
    if (code == BML_OK || code == BML_ERROR_INVALID_HANDLE) {
        m_Handle = nullptr;
        m_Session.reset();
        return Result<CloseState>::Success(CloseState::Closed);
    }
    if (code == BML_ERROR_BUSY)
        return Result<CloseState>::Success(CloseState::Closing);
    try {
        const auto info = Info();
        if (info) {
            if (info->RestoreFailure.Error != Error::None)
                return Result<CloseState>::Failure(
                    code, info->RestoreFailure);
            if (info->LastStatus.Error != Error::None)
                return Result<CloseState>::Failure(code, info->LastStatus);
        }
    } catch (...) {
        // Close remains noexcept; the original ABI error is still useful when
        // the optional diagnostic read cannot allocate.
    }
    return Result<CloseState>::Failure(code);
}

inline Plan::Plan(std::shared_ptr<Detail::SessionState> session,
                  BML_BehaviorPlan handle)
    : m_Session(std::move(session)), m_Handle(handle) {}

inline Patch::~Patch() {
    (void) Close();
}

inline Patch::Patch(Patch &&other) noexcept
    : m_Session(std::move(other.m_Session)),
      m_Handle(std::exchange(other.m_Handle, nullptr)),
      m_Edits(std::move(other.m_Edits)) {}

inline Patch &Patch::operator=(Patch &&other) noexcept {
    if (this != &other) {
        Patch previous(std::move(*this));
        m_Session = std::move(other.m_Session);
        m_Handle = std::exchange(other.m_Handle, nullptr);
        m_Edits = std::move(other.m_Edits);
    }
    return *this;
}

inline Patch::operator bool() const noexcept {
    return m_Session && m_Session->Api && m_Session->Handle && m_Handle;
}

template <class First, class... More>
inline Result<PatchInfo> Patch::Replace(First &&first, More &&...more) {
    static_assert(
        std::is_same_v<std::decay_t<First>, Detail::PatchTarget> &&
            (std::is_same_v<std::decay_t<More>, Detail::PatchTarget> && ...),
        "Behavior Patch::Replace expects On(Graph, Edit) entries.");
    std::vector<Detail::PatchTarget> targets;
    try {
        targets.reserve(1 + sizeof...(more));
        targets.push_back(std::forward<First>(first));
        (targets.push_back(std::forward<More>(more)), ...);
    } catch (const std::bad_alloc &) {
        return Result<PatchInfo>::Failure(BML_ERROR_OUT_OF_MEMORY);
    }
    return Replace(std::move(targets));
}

inline Result<PatchInfo> Patch::Info() const {
    if (!*this)
        return Result<PatchInfo>::Failure(BML_ERROR_INVALID_HANDLE);
    BML_BehaviorPatchInfo wire{};
    wire.StructSize = sizeof(wire);
    BML_BehaviorStatus status = Detail::EmptyStatus();
    const int code = Detail::WireCode(
        m_Session->Api->ReadPatch(
            m_Session->Handle, m_Handle, &wire, &status),
        status);
    if (code != BML_OK)
        return Result<PatchInfo>::Failure(code, Detail::ReadStatus(status));
    return Detail::CompletePatchInfo(m_Session, m_Handle, wire, status);
}

inline Result<BML_ObjectRef> Patch::ResolveHandle(std::uint32_t node) const {
    if (!*this)
        return Result<BML_ObjectRef>::Failure(BML_ERROR_INVALID_HANDLE);
    if (!BML_IFACE_HAS(m_Session->Api, BML_BehaviorInterface,
                       ResolvePatchNode))
        return Result<BML_ObjectRef>::Failure(BML_ERROR_VERSION_MISMATCH);
    BML_ObjectRef reference{};
    BML_BehaviorStatus status = Detail::EmptyStatus();
    const int code = Detail::WireCode(
        m_Session->Api->ResolvePatchNode(
            m_Session->Handle, m_Handle, node, &reference, &status),
        status);
    if (code != BML_OK)
        return Result<BML_ObjectRef>::Failure(
            code, Detail::ReadStatus(status));
    if (!Detail::ValidObjectRef(reference) || !reference.Domain)
        return Result<BML_ObjectRef>::Failure(
            BML_ERROR_MALFORMED_MESSAGE, Detail::ReadStatus(status));
    return Result<BML_ObjectRef>::Success(
        reference, Detail::ReadStatus(status));
}

inline Result<CloseState> Patch::Close() noexcept {
    if (!m_Handle) {
        m_Session.reset();
        m_Edits.clear();
        return Result<CloseState>::Success(CloseState::Closed);
    }
    if (!m_Session || !m_Session->Api || !m_Session->Handle)
        return Result<CloseState>::Failure(BML_ERROR_INVALID_HANDLE);
    const int code = m_Session->Api->ClosePatch(m_Session->Handle, m_Handle);
    if (code == BML_OK || code == BML_ERROR_INVALID_HANDLE) {
        m_Handle = nullptr;
        m_Session.reset();
        m_Edits.clear();
        return Result<CloseState>::Success(CloseState::Closed);
    }
    if (code == BML_ERROR_BUSY)
        return Result<CloseState>::Success(CloseState::Closing);
    try {
        const auto info = Info();
        if (info) {
            if (info->RestoreFailure.Error != Error::None)
                return Result<CloseState>::Failure(
                    code, info->RestoreFailure);
            if (info->LastStatus.Error != Error::None)
                return Result<CloseState>::Failure(code, info->LastStatus);
        }
    } catch (...) {
        // Close remains noexcept; the original ABI error is still useful when
        // the optional diagnostic read cannot allocate.
    }
    return Result<CloseState>::Failure(code);
}

inline Patch::Patch(std::shared_ptr<Detail::SessionState> session,
                    BML_BehaviorPatch handle,
                    std::vector<Detail::PatchSymbols> edits)
    : m_Session(std::move(session)), m_Handle(handle),
      m_Edits(std::move(edits)) {}

inline auto On(const Graph &graph, const Edit &edit) {
    Detail::PatchTarget target;
    target.Session = graph.m_Session;
    target.Graph = graph.m_Root;
    target.Fingerprint = graph.m_Fingerprint;
    target.Symbols = edit.m_Program;
    if (graph.m_View != View::Logical || !target.Graph.Domain) {
        target.Code = BML_ERROR_INVALID_PARAMETER;
        target.Failure.Error = Error::GraphLocalityInvalid;
        target.Failure.Phase = Phase::Edit;
        target.Failure.Message =
            "A Patch target must be a logical Graph snapshot.";
        return target;
    }
    if (!edit.m_Program) {
        target.Code = BML_ERROR_INVALID_PARAMETER;
        target.Failure.Error = Error::GraphLocalityInvalid;
        target.Failure.Phase = Phase::Edit;
        target.Failure.Message = "The Behavior Edit no longer exists.";
        return target;
    }
    try {
        target.Body = edit.Snapshot();
    } catch (const std::bad_alloc &) {
        target.Code = BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        target.Code = BML_ERROR_FAIL;
    }
    return target;
}

inline auto On(const Scripts &scripts, const Edit &edit) {
    Detail::PlanRule target;
    target.Targets = scripts.m_Count;
    target.Script = scripts.m_Name;
    if (!edit.m_Program) {
        target.Code = BML_ERROR_INVALID_PARAMETER;
        target.Failure.Error = Error::GraphLocalityInvalid;
        target.Failure.Phase = Phase::Edit;
        target.Failure.Message = "The Behavior Edit no longer exists.";
        return target;
    }
    try {
        target.Body = edit.Snapshot();
    } catch (const std::bad_alloc &) {
        target.Code = BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        target.Code = BML_ERROR_FAIL;
    }
    return target;
}

namespace Detail {

struct PatchWire {
    std::vector<EditWire> Programs;
    std::vector<BML_BehaviorGraphEdit> Targets;
    std::vector<PatchSymbols> Symbols;

    [[nodiscard]] static Result<PatchWire> Compile(
        const std::shared_ptr<SessionState> &session,
        const std::vector<PatchTarget> &targets,
        bool verifySnapshots) {
        PatchWire wire;
        wire.Programs.reserve(targets.size());
        wire.Targets.reserve(targets.size());
        wire.Symbols.reserve(targets.size());

        std::uint32_t handleBase = 0;
        for (const PatchTarget &target : targets) {
            if (target.Code != BML_OK)
                return Result<PatchWire>::Failure(
                    target.Code, target.Failure);
            if (!target.Body || target.Session != session ||
                !target.Graph.Domain)
                return Result<PatchWire>::Failure(
                    BML_ERROR_INVALID_PARAMETER);

            if (verifySnapshots) {
                Result<Graph> current = Graph::Read(
                    session, target.Graph, View::Logical);
                if (!current)
                    return Result<PatchWire>::Failure(
                        current.Code(), current.GetStatus());
                if (current->Fingerprint() != target.Fingerprint) {
                    Status changed;
                    changed.Error = Error::GraphChanged;
                    changed.Phase = Phase::Edit;
                    changed.Message =
                        "The logical Graph changed after this snapshot was read.";
                    return Result<PatchWire>::Failure(
                        BML_ERROR_BUSY, std::move(changed));
                }
            }

            Result<void> valid = target.Body->Validate(session);
            if (!valid)
                return Result<PatchWire>::Failure(
                    valid.Code(), valid.GetStatus());

            wire.Programs.emplace_back();
            target.Body->Encode(wire.Programs.back());

            BML_BehaviorGraphEdit edit{};
            edit.StructSize = sizeof(edit);
            edit.Graph = target.Graph;
            edit.Fingerprint = target.Fingerprint;
            edit.Steps = wire.Programs.back().Steps.empty()
                ? nullptr : wire.Programs.back().Steps.data();
            edit.StepCount = static_cast<std::uint32_t>(
                wire.Programs.back().Steps.size());
            edit.HandleBase = handleBase;
            wire.Targets.push_back(edit);
            wire.Symbols.push_back({target.Symbols, handleBase});

            const std::uint32_t span = target.Body->m_Program->NextHandle;
            if (span > UINT32_MAX - handleBase)
                return Result<PatchWire>::Failure(
                    BML_ERROR_INVALID_PARAMETER);
            handleBase += span;
        }
        return Result<PatchWire>::Success(std::move(wire));
    }
};

struct PlanWire {
    std::vector<EditWire> Programs;
    std::vector<BML_BehaviorScriptEdit> Rules;

    [[nodiscard]] static Result<PlanWire> Compile(
        const std::shared_ptr<SessionState> &session,
        const std::vector<PlanRule> &rules) {
        PlanWire wire;
        wire.Programs.reserve(rules.size());
        wire.Rules.reserve(rules.size());
        for (const PlanRule &rule : rules) {
            if (rule.Code != BML_OK)
                return Result<PlanWire>::Failure(
                    rule.Code, rule.Failure);
            if (!rule.Body || rule.Script.empty())
                return Result<PlanWire>::Failure(
                    BML_ERROR_INVALID_PARAMETER);

            Result<void> valid = rule.Body->ValidatePlan(session);
            if (!valid)
                return Result<PlanWire>::Failure(
                    valid.Code(), valid.GetStatus());

            wire.Programs.emplace_back();
            rule.Body->Encode(wire.Programs.back());

            BML_BehaviorScriptEdit edit{};
            edit.StructSize = sizeof(edit);
            edit.Targets = rule.Targets;
            edit.Script = Detail::Text(rule.Script);
            edit.Steps = wire.Programs.back().Steps.empty()
                ? nullptr : wire.Programs.back().Steps.data();
            edit.StepCount = static_cast<std::uint32_t>(
                wire.Programs.back().Steps.size());
            wire.Rules.push_back(edit);
        }
        return Result<PlanWire>::Success(std::move(wire));
    }
};

} // namespace Detail

template <class Handle>
Result<BML_ObjectRef> Patch::Resolve(const Handle &node) const {
    static_assert(std::is_same_v<std::decay_t<Handle>, Edit::Node>,
                  "A Behavior Patch resolves only an Edit::Node.");
    if (!*this)
        return Result<BML_ObjectRef>::Failure(BML_ERROR_INVALID_HANDLE);
    const std::owner_less<std::weak_ptr<Detail::EditProgram>> before;
    const auto symbols = std::find_if(
        m_Edits.begin(), m_Edits.end(), [&](const Detail::PatchSymbols &item) {
            return !before(item.Edit, node.m_Edit) &&
                   !before(node.m_Edit, item.Edit);
        });
    if (symbols == m_Edits.end() || !node.m_Id ||
        node.m_Id > UINT32_MAX - symbols->HandleBase) {
        Status status;
        status.Error = Behavior::Error::GraphLocalityInvalid;
        status.Phase = Behavior::Phase::Edit;
        status.Message =
            "The symbolic Node belongs to a different Behavior Edit.";
        return Result<BML_ObjectRef>::Failure(BML_ERROR_INVALID_PARAMETER,
                                               std::move(status));
    }
    return ResolveHandle(node.m_Id + symbols->HandleBase);
}

template <class First, class... More>
inline Result<Behavior::Patch> Session::Apply(
    std::string_view name, First &&first, More &&...more) const {
    static_assert(
        std::is_same_v<std::decay_t<First>, Detail::PatchTarget> &&
            (std::is_same_v<std::decay_t<More>, Detail::PatchTarget> && ...),
        "Behavior Session::Apply expects On(Graph, Edit) entries.");
    std::vector<Detail::PatchTarget> targets;
    try {
        targets.reserve(1 + sizeof...(more));
        targets.push_back(std::forward<First>(first));
        (targets.push_back(std::forward<More>(more)), ...);
    } catch (const std::bad_alloc &) {
        return Result<Behavior::Patch>::Failure(BML_ERROR_OUT_OF_MEMORY);
    }
    return Apply(name, std::move(targets));
}

template <class First, class... More,
          std::enable_if_t<
              !std::is_same_v<std::decay_t<First>, Scripts>, int>>
inline Result<Behavior::Plan> Session::Plan(
    std::string_view name, First &&first, More &&...more) const {
    static_assert(
        std::is_same_v<std::decay_t<First>, Detail::PlanRule> &&
            (std::is_same_v<std::decay_t<More>, Detail::PlanRule> && ...),
        "Behavior Session::Plan expects On(Scripts, Edit) entries.");
    std::vector<Detail::PlanRule> rules;
    try {
        rules.reserve(1 + sizeof...(more));
        rules.push_back(std::forward<First>(first));
        (rules.push_back(std::forward<More>(more)), ...);
    } catch (const std::bad_alloc &) {
        return Result<Behavior::Plan>::Failure(BML_ERROR_OUT_OF_MEMORY);
    }
    return Plan(name, std::move(rules));
}

inline Result<Patch> Graph::Apply(std::string_view name,
                                  const Edit &edit) const {
    try {
        return Session::Apply(
            m_Session, name,
            std::vector<Detail::PatchTarget>{On(*this, edit)});
    } catch (const std::bad_alloc &) {
        return Result<Patch>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Patch>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Patch> Session::Apply(
    std::string_view name, std::vector<Detail::PatchTarget> targets) const {
    return Apply(m_State, name, std::move(targets));
}

inline Result<Patch> Session::Apply(
    const std::shared_ptr<Detail::SessionState> &session,
    std::string_view name, std::vector<Detail::PatchTarget> targets) {
    if (!session || !session->Api || !session->Handle)
        return Result<Patch>::Failure(BML_ERROR_INVALID_HANDLE);
    if (name.empty() || targets.empty() ||
        !BML_IFACE_HAS(session->Api, BML_BehaviorInterface, ReplacePatch))
        return Result<Patch>::Failure(
            name.empty() || targets.empty() ? BML_ERROR_INVALID_PARAMETER
                                             : BML_ERROR_VERSION_MISMATCH);
    try {
        Result<Detail::PatchWire> compiled =
            Detail::PatchWire::Compile(session, targets, true);
        if (!compiled)
            return Result<Patch>::Failure(
                compiled.Code(), compiled.GetStatus());
        Detail::PatchWire wire = compiled.Take();

        BML_BehaviorPatchSpec spec{};
        spec.StructSize = sizeof(spec);
        spec.Name = Detail::Text(name);
        spec.Edits = wire.Targets.data();
        spec.EditCount = static_cast<std::uint32_t>(wire.Targets.size());
        BML_BehaviorPatch handle = nullptr;
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = Detail::WireCode(
            session->Api->ApplyPatch(
                session->Handle, &spec, &handle, nullptr, &status), status);
        Patch owned(session, handle, std::move(wire.Symbols));
        if (code != BML_OK || !handle)
            return Result<Patch>::Failure(
                code == BML_OK ? BML_ERROR_MALFORMED_MESSAGE : code,
                Detail::ReadStatus(status));
        return Result<Patch>::Success(std::move(owned),
                                      Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<Patch>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Patch>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<Plan> Session::Plan(std::string_view name,
                                  const Scripts &scripts,
                                  const Edit &edit) const {
    return Plan(name, std::vector<Detail::PlanRule>{On(scripts, edit)});
}

inline Result<Plan> Session::Plan(
    std::string_view name, std::vector<Detail::PlanRule> rules) const {
    if (!m_State || !m_State->Api || !m_State->Handle)
        return Result<Behavior::Plan>::Failure(BML_ERROR_INVALID_HANDLE);
    if (name.empty() || rules.empty() ||
        !BML_IFACE_HAS(m_State->Api, BML_BehaviorInterface, ReplacePlan))
        return Result<Behavior::Plan>::Failure(
            name.empty() || rules.empty() ? BML_ERROR_INVALID_PARAMETER
                                          : BML_ERROR_VERSION_MISMATCH);
    try {
        Result<Detail::PlanWire> compiled =
            Detail::PlanWire::Compile(m_State, rules);
        if (!compiled)
            return Result<Behavior::Plan>::Failure(
                compiled.Code(), compiled.GetStatus());
        Detail::PlanWire wire = compiled.Take();

        BML_BehaviorPlanSpec spec{};
        spec.StructSize = sizeof(spec);
        spec.Name = Detail::Text(name);
        spec.Edits = wire.Rules.data();
        spec.EditCount = static_cast<std::uint32_t>(wire.Rules.size());

        BML_BehaviorPlan handle = nullptr;
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = Detail::WireCode(
            m_State->Api->SubmitPlan(
                m_State->Handle, &spec, &handle, nullptr, &status),
            status);
        Behavior::Plan owned(m_State, handle);
        if (code != BML_OK || !handle) {
            return Result<Behavior::Plan>::Failure(
                code == BML_OK ? BML_ERROR_MALFORMED_MESSAGE : code,
                Detail::ReadStatus(status));
        }
        return Result<Behavior::Plan>::Success(
            std::move(owned), Detail::ReadStatus(status));
    } catch (const std::bad_alloc &) {
        return Result<Behavior::Plan>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Behavior::Plan>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<PatchInfo> Patch::SetActive(bool active) {
    if (!*this || !BML_IFACE_HAS(
            m_Session->Api, BML_BehaviorInterface, SetPatchActive))
        return Result<PatchInfo>::Failure(
            *this ? BML_ERROR_VERSION_MISMATCH : BML_ERROR_INVALID_HANDLE);
    BML_BehaviorPatchInfo wire{};
    wire.StructSize = sizeof(wire);
    BML_BehaviorStatus status = Detail::EmptyStatus();
    const int code = Detail::WireCode(
        m_Session->Api->SetPatchActive(
            m_Session->Handle, m_Handle, active ? 1u : 0u,
            &wire, &status), status);
    if (code != BML_OK)
        return Result<PatchInfo>::Failure(code, Detail::ReadStatus(status));
    return Detail::CompletePatchInfo(m_Session, m_Handle, wire, status);
}

inline Result<PatchInfo> Patch::Enable() { return SetActive(true); }
inline Result<PatchInfo> Patch::Disable() { return SetActive(false); }

inline Result<PatchInfo> Patch::Replace(
    std::vector<Detail::PatchTarget> targets) {
    if (!*this || targets.empty() || !BML_IFACE_HAS(
            m_Session->Api, BML_BehaviorInterface, ReplacePatch))
        return Result<PatchInfo>::Failure(
            !*this ? BML_ERROR_INVALID_HANDLE :
            targets.empty() ? BML_ERROR_INVALID_PARAMETER
                          : BML_ERROR_VERSION_MISMATCH);
    try {
        Result<Detail::PatchWire> compiled =
            Detail::PatchWire::Compile(m_Session, targets, false);
        if (!compiled)
            return Result<PatchInfo>::Failure(
                compiled.Code(), compiled.GetStatus());
        Detail::PatchWire program = compiled.Take();

        BML_BehaviorPatchInfo info{};
        info.StructSize = sizeof(info);
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = Detail::WireCode(
            m_Session->Api->ReplacePatch(
                m_Session->Handle, m_Handle, program.Targets.data(),
                static_cast<std::uint32_t>(program.Targets.size()),
                &info, &status),
            status);
        if (code != BML_OK)
            return Result<PatchInfo>::Failure(code, Detail::ReadStatus(status));
        m_Edits = std::move(program.Symbols);
        return Detail::CompletePatchInfo(m_Session, m_Handle, info, status);
    } catch (const std::bad_alloc &) {
        return Result<PatchInfo>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<PatchInfo>::Failure(BML_ERROR_FAIL);
    }
}

inline Result<PlanInfo> Plan::SetActive(bool active) {
    if (!*this || !BML_IFACE_HAS(
            m_Session->Api, BML_BehaviorInterface, SetPlanActive))
        return Result<PlanInfo>::Failure(
            *this ? BML_ERROR_VERSION_MISMATCH : BML_ERROR_INVALID_HANDLE);
    BML_BehaviorPlanInfo wire{};
    wire.StructSize = sizeof(wire);
    BML_BehaviorStatus status = Detail::EmptyStatus();
    const int code = Detail::WireCode(
        m_Session->Api->SetPlanActive(
            m_Session->Handle, m_Handle, active ? 1u : 0u,
            &wire, &status), status);
    if (code != BML_OK)
        return Result<PlanInfo>::Failure(code, Detail::ReadStatus(status));
    return Detail::CompletePlanInfo(m_Session, m_Handle, wire, status);
}

inline Result<PlanInfo> Plan::Enable() { return SetActive(true); }
inline Result<PlanInfo> Plan::Disable() { return SetActive(false); }

inline Result<PlanInfo> Plan::Replace(
    std::vector<Detail::PlanRule> rules) {
    if (!*this || rules.empty() || !BML_IFACE_HAS(
            m_Session->Api, BML_BehaviorInterface, ReplacePlan))
        return Result<PlanInfo>::Failure(
            !*this ? BML_ERROR_INVALID_HANDLE :
            rules.empty() ? BML_ERROR_INVALID_PARAMETER
                          : BML_ERROR_VERSION_MISMATCH);
    try {
        Result<Detail::PlanWire> compiled =
            Detail::PlanWire::Compile(m_Session, rules);
        if (!compiled)
            return Result<PlanInfo>::Failure(
                compiled.Code(), compiled.GetStatus());
        Detail::PlanWire program = compiled.Take();

        BML_BehaviorPlanInfo info{};
        info.StructSize = sizeof(info);
        BML_BehaviorStatus status = Detail::EmptyStatus();
        const int code = Detail::WireCode(
            m_Session->Api->ReplacePlan(
                m_Session->Handle, m_Handle, program.Rules.data(),
                static_cast<std::uint32_t>(program.Rules.size()),
                &info, &status),
            status);
        if (code != BML_OK)
            return Result<PlanInfo>::Failure(code, Detail::ReadStatus(status));
        return Detail::CompletePlanInfo(m_Session, m_Handle, info, status);
    } catch (const std::bad_alloc &) {
        return Result<PlanInfo>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<PlanInfo>::Failure(BML_ERROR_FAIL);
    }
}

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_DETAIL_EDIT_PROGRAM_HPP

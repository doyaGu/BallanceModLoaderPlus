#include "Api/Behavior/Codec.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Behavior/Blocks/HookBlock.h"
#include "Loader/ModContext.h"

namespace BML::Api::Behavior {
namespace {

using BML::Behavior::Internal::BehaviorKind;
using BML::Behavior::Internal::Cycle;
using BML::Behavior::Internal::Link;
using BML::Behavior::Internal::Node;
using BML::Behavior::Internal::NodePattern;
using BML::Behavior::Internal::Order;
using BML::Behavior::Internal::OrderKind;
using BML::Behavior::Internal::ParameterOperation;
using BML::Behavior::Internal::PatchKey;
using BML::Behavior::Internal::PathRef;
using BML::Behavior::Internal::Phase;
using BML::Behavior::Internal::PlanCallbackState;
using BML::Behavior::Internal::Port;
using BML::Behavior::Internal::ReleaseScope;
using BML::Behavior::Internal::ScriptSelection;
using BML::Behavior::Internal::TargetSet;
namespace HookBlock = BML::Behavior::Internal::HookBlock;

// The record a Hook Block hands back to InvokeHook. The callback state owns it,
// so it outlives the Hook occurrence and every Binding taken from that
// occurrence, including Bindings a Conflicted Plan can no longer revert.
struct HookThunk {
    ~HookThunk() noexcept {
        // The last holder may be dropping this inside Behavior bookkeeping,
        // so the Release waits for the enclosing ReleaseScope to end.
        if (Retained && Function.Release)
            ReleaseScope::Release(Function.Release, Function.State);
    }

    BML_BehaviorHookFunction Function{};
    bool Retained = false;
};

int InvokeHook(const CKBehaviorContext *native, void *argument) {
    const auto *thunk = static_cast<const HookThunk *>(argument);
    if (!thunk || !thunk->Function.Invoke || !native || !native->Behavior)
        return CKBR_BEHAVIORERROR;
    ModContextLease context;
    if (!context)
        return CKBR_BEHAVIORERROR;

    CKBehavior *block = native->Behavior;
    BML_BehaviorHookContext wire{};
    wire.StructSize = sizeof(wire);
    wire.DeltaTime = native->DeltaTime;
    wire.Block = context->ObjectRefs().Issue(block);
    wire.Script = context->ObjectRefs().Issue(block->GetOwnerScript());
    wire.Owner = context->ObjectRefs().Issue(block->GetOwner());

    int result;
    {
        auto invocation = context->LockModInvocation();
        result = thunk->Function.Invoke(thunk->Function.State, &wire);
    }
    switch (result) {
    case BML_BEHAVIOR_HOOK_OK: return CKBR_OK;
    case BML_BEHAVIOR_HOOK_AGAIN_NEXT_FRAME: return CKBR_ACTIVATENEXTFRAME;
    case BML_BEHAVIOR_HOOK_FAULT: return HookBlock::CallbackFaulted;
    default: return CKBR_BEHAVIORERROR;
    }
}

enum class EditHandleKind {
    Node,
    Operation,
    Link,
    Path,
    Port,
};

struct EditHandle {
    EditHandleKind Kind = EditHandleKind::Node;
    std::uint32_t Scope = BML_BEHAVIOR_EDIT_GRAPH;
    Node NodeValue;
    ParameterOperation OperationValue;
    Link LinkValue;
    PathRef PathValue;
    Port PortValue;
};

// Translates one wire edit program into one symbolic Program. The same value
// can be applied once to a known graph or retained by a Plan.
class ProgramDecoder final {
public:
    Status Build(const BML_BehaviorEditProgram &program, ModContext &context,
                 Program &edit);
    // Reports every symbolic Node and appended Port the caller can address in
    // an installed Patch or Plan.
    [[nodiscard]] Installations::SymbolMap
    Symbols() const;

private:
    static bool Defines(std::uint32_t kind) noexcept;

    Status Step(const BML_BehaviorEditStep &step, ModContext &context,
                Program &edit);
    Status Use(std::uint32_t scope, std::uint32_t id, EditHandleKind kind,
               const EditHandle *&out) const;

    // Pool readers. Every index is one-based and checked against its pool.
    Status PortRefAt(std::uint32_t index,
                     const BML_BehaviorPortRef *&out) const;
    // A port of another graph scope is rejected: each scope numbers its
    // Nodes from one, so its Node would name an unrelated Node here.
    Status PortAt(std::uint32_t scope, std::uint32_t index,
                  Port &out) const;
    Status PatternAt(std::uint32_t index, NodePattern &out) const;
    Status ValueAt(std::uint32_t index, ModContext &context,
                   Parameter::Binding &out) const;
    Status BlockAt(std::uint32_t index, ModContext &context,
                   const char *role, BlockSpec &out) const;
    Status HookAt(std::uint32_t index, HookBlock::Hook &out) const;
    Status ObjectAt(std::uint32_t index, const char *message,
                    BML::Behavior::Internal::ObjectRef &out) const;
    Status OperationAt(std::uint32_t index,
                       const BML_BehaviorOperationSpec *&out) const;
    Status OrdersAt(const BML_BehaviorEditStep &step,
                    std::vector<Order> &out) const;

    using HandleKey = std::pair<std::uint32_t, std::uint32_t>;
    const BML_BehaviorEditProgram *m_Program = nullptr;
    std::map<HandleKey, EditHandle> m_Handles;
    std::map<std::uint32_t, Program *> m_Graphs;
};

template <typename T>
const T *Entry(const T *pool, std::uint32_t count,
               std::uint32_t index) noexcept {
    return index && index <= count ? &pool[index - 1] : nullptr;
}

bool ProgramDecoder::Defines(std::uint32_t kind) noexcept {
    switch (kind) {
    case BML_BEHAVIOR_EDIT_REQUIRE_NODE:
    case BML_BEHAVIOR_EDIT_REQUIRE_LINK:
    case BML_BEHAVIOR_EDIT_FOLLOW:
    case BML_BEHAVIOR_EDIT_ADD_BLOCK:
    case BML_BEHAVIOR_EDIT_APPEND_SLOT:
    case BML_BEHAVIOR_EDIT_USE_NODE:
    case BML_BEHAVIOR_EDIT_USE_LINK:
    case BML_BEHAVIOR_EDIT_ADD_OPERATION:
    case BML_BEHAVIOR_EDIT_REPLACE_BLOCK:
    case BML_BEHAVIOR_EDIT_ADD_GRAPH:
    case BML_BEHAVIOR_EDIT_ENTER_GRAPH:
    case BML_BEHAVIOR_EDIT_NEXT_NODE:
    case BML_BEHAVIOR_EDIT_PREVIOUS_NODE:
    case BML_BEHAVIOR_EDIT_LEAVING_LINK:
    case BML_BEHAVIOR_EDIT_ENTERING_LINK:
    case BML_BEHAVIOR_EDIT_LINK_TO_NODE:
    case BML_BEHAVIOR_EDIT_EACH_NODE:
        return true;
    default:
        return false;
    }
}

Status ProgramDecoder::Build(const BML_BehaviorEditProgram &program,
                             ModContext &context, Program &edit) {
    if (program.StructSize < sizeof(program))
        return InvalidValue(
            "A Behavior edit program has an unsupported StructSize.");
    if ((program.StepCount && !program.Steps) ||
        (program.PortCount && !program.Ports) ||
        (program.ValueCount && !program.Values) ||
        (program.PatternCount && !program.Patterns) ||
        (program.BlockCount && !program.Blocks) ||
        (program.HookCount && !program.Hooks) ||
        (program.OrderCount && !program.Orders) ||
        (program.ObjectCount && !program.Objects) ||
        (program.OperationCount && !program.Operations))
        return InvalidValue("A Behavior edit program pool is missing.");
    m_Program = &program;
    EditHandle graph;
    graph.Scope = BML_BEHAVIOR_EDIT_GRAPH;
    graph.NodeValue = edit.Graph();
    m_Handles.clear();
    m_Graphs.clear();
    m_Graphs.emplace(BML_BEHAVIOR_EDIT_GRAPH, &edit);
    m_Handles.emplace(HandleKey{BML_BEHAVIOR_EDIT_GRAPH,
                                BML_BEHAVIOR_EDIT_GRAPH}, graph);
    for (std::uint32_t index = 0; index < program.StepCount; ++index) {
        const Status status = Step(program.Steps[index], context, edit);
        if (!status)
            return status;
    }
    return {};
}

Installations::SymbolMap ProgramDecoder::Symbols() const {
    Installations::SymbolMap symbols;
    for (const auto &entry : m_Handles) {
        if (entry.second.Kind == EditHandleKind::Node) {
            Installations::Symbol symbol;
            symbol.Kind =
                Installations::SymbolKind::Node;
            symbol.NodeValue = entry.second.NodeValue;
            symbols.emplace(
                Installations::SymbolRef{
                    0, entry.first.first, entry.first.second},
                std::move(symbol));
        } else if (entry.second.Kind == EditHandleKind::Port) {
            Installations::Symbol symbol;
            symbol.Kind =
                Installations::SymbolKind::Port;
            symbol.PortValue = entry.second.PortValue;
            symbols.emplace(
                Installations::SymbolRef{
                    0, entry.first.first, entry.first.second},
                std::move(symbol));
        }
    }
    return symbols;
}

// Port-count and port-value conditions arrive as later steps, so an empty
// pattern is rejected by Program::Validate once every step is decoded.
Status ReadNodePattern(const BML_BehaviorNodePattern &from,
                       NodePattern &out) {
    out = {};
    if (from.StructSize < sizeof(from))
        return InvalidValue("A Node Pattern has an unsupported StructSize.");
    const BML_BehaviorSelector &selector = from.Selector;
    switch (selector.Kind) {
    case BML_BEHAVIOR_SELECTOR_INDEX:
        if (selector.StructSize < sizeof(selector) || selector.Index < 0)
            return InvalidValue("A Node Pattern index is invalid.");
        out.Selector = NodePattern::SelectorKind::Index;
        out.Index = selector.Index;
        break;
    case BML_BEHAVIOR_SELECTOR_NAME:
    case BML_BEHAVIOR_SELECTOR_UNIQUE_NAME:
        if (selector.StructSize < sizeof(selector) ||
            !ReadNativeString(selector.Name, out.Name) || out.Name.empty() ||
            selector.Occurrence < 0)
            return InvalidValue("A Node Pattern name is invalid.");
        out.Selector = NodePattern::SelectorKind::Name;
        out.Occurrence = selector.Occurrence;
        out.Unique = selector.Kind == BML_BEHAVIOR_SELECTOR_UNIQUE_NAME;
        break;
    case BML_BEHAVIOR_SELECTOR_ONLY:
        if (selector.StructSize < sizeof(selector))
            return InvalidValue("A Node Pattern selector is invalid.");
        out.Selector = NodePattern::SelectorKind::Only;
        break;
    default:
        return InvalidValue("A Node Pattern selector is unknown.");
    }

    std::string expectedName;
    if (!ReadNativeString(from.Name, expectedName))
        return InvalidValue("A Node Pattern expected name is invalid.");
    if (!expectedName.empty()) {
        if (out.Selector == NodePattern::SelectorKind::Name &&
            out.Name != expectedName) {
            return InvalidValue(
                "A Node Pattern names two different Behaviors.");
        }
        out.Name = std::move(expectedName);
    }

    out.Prototype = Guid(from.Prototype);
    switch (from.Kind) {
    case 0: break;
    case BML_BEHAVIOR_KIND_FUNCTION:
        out.ExpectedKind = BehaviorKind::Function;
        break;
    case BML_BEHAVIOR_KIND_CALLBACK:
        out.ExpectedKind = BehaviorKind::Callback;
        break;
    case BML_BEHAVIOR_KIND_GRAPH:
        out.ExpectedKind = BehaviorKind::Graph;
        break;
    default:
        return InvalidValue("A Node Pattern Behavior kind is unknown.");
    }
    out.PortShape = from.Shape;
    return {};
}

Status ProgramDecoder::Step(const BML_BehaviorEditStep &step,
                            ModContext &context, Program &root) {
    if (step.StructSize < sizeof(step))
        return InvalidValue("A Behavior edit step has an unsupported StructSize.");
    const auto graph = m_Graphs.find(step.Graph);
    if (graph == m_Graphs.end() || !graph->second)
        return InvalidValue("A Behavior edit step names an unknown graph scope.");
    Program &edit = *graph->second;
    std::uint32_t allowedFlags = 0;
    if (step.Kind == BML_BEHAVIOR_EDIT_REQUIRE_LINK)
        allowedFlags = BML_BEHAVIOR_EDIT_HAS_DELAY;
    else if (step.Kind == BML_BEHAVIOR_EDIT_FLOW ||
             step.Kind == BML_BEHAVIOR_EDIT_RECONNECT)
        allowedFlags = BML_BEHAVIOR_EDIT_CONFIRM_CYCLE;
    if (step.Flags & ~allowedFlags)
        return InvalidValue("A Behavior edit step contains an unsupported flag.");
    if (Defines(step.Kind)) {
        if (step.Result == 0 || step.Result == BML_BEHAVIOR_EDIT_GRAPH ||
            m_Handles.find({step.Graph, step.Result}) != m_Handles.end() ||
            m_Graphs.find(step.Result) != m_Graphs.end()) {
            return InvalidValue(
                "A Behavior edit step handle is missing or already defined.");
        }
    } else if (step.Result != 0) {
        return InvalidValue("This kind of Behavior edit step defines no handle.");
    }

    EditHandle defined;
    defined.Scope = step.Graph;
    Status status;
    Port source;
    Port sink;
    switch (step.Kind) {
    case BML_BEHAVIOR_EDIT_REQUIRE_NODE:
    case BML_BEHAVIOR_EDIT_EACH_NODE: {
        NodePattern query;
        if (status = PatternAt(step.Operand, query); !status)
            return status;
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = step.Kind == BML_BEHAVIOR_EDIT_EACH_NODE
            ? edit.Each(std::move(query))
            : edit.RequireOne(std::move(query));
        break;
    }
    case BML_BEHAVIOR_EDIT_REQUIRE_LINK: {
        if (status = PortAt(step.Graph, step.Source, source); !status)
            return status;
        if (status = PortAt(step.Graph, step.Sink, sink); !status)
            return status;
        std::optional<int> delay;
        if (step.Flags & BML_BEHAVIOR_EDIT_HAS_DELAY)
            delay = step.Number;
        defined.Kind = EditHandleKind::Link;
        defined.LinkValue = edit.RequireOne(source, sink, delay);
        break;
    }
    case BML_BEHAVIOR_EDIT_NEXT_NODE:
    case BML_BEHAVIOR_EDIT_PREVIOUS_NODE: {
        const bool next = step.Kind == BML_BEHAVIOR_EDIT_NEXT_NODE;
        Port &end = next ? source : sink;
        if (status = PortAt(step.Graph, next ? step.Source : step.Sink,
                           end); !status)
            return status;
        NodePattern expected;
        if (step.Operand) {
            if (status = PatternAt(step.Operand, expected); !status)
                return status;
        }
        defined.Kind = EditHandleKind::Node;
        if (next) {
            defined.NodeValue = expected
                ? edit.Next(source, std::move(expected)) : edit.Next(source);
        } else {
            defined.NodeValue = expected
                ? edit.Previous(sink, std::move(expected))
                : edit.Previous(sink);
        }
        break;
    }
    case BML_BEHAVIOR_EDIT_LEAVING_LINK:
        if (status = PortAt(step.Graph, step.Source, source); !status)
            return status;
        defined.Kind = EditHandleKind::Link;
        defined.LinkValue = edit.Leaving(source);
        break;
    case BML_BEHAVIOR_EDIT_ENTERING_LINK:
        if (status = PortAt(step.Graph, step.Sink, sink); !status)
            return status;
        defined.Kind = EditHandleKind::Link;
        defined.LinkValue = edit.Entering(sink);
        break;
    case BML_BEHAVIOR_EDIT_LINK_TO_NODE: {
        if (status = PortAt(step.Graph, step.Source, source); !status)
            return status;
        const EditHandle *target = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Node,
                         target); !status)
            return status;
        defined.Kind = EditHandleKind::Link;
        defined.LinkValue = edit.To(source, target->NodeValue);
        break;
    }
    case BML_BEHAVIOR_EDIT_FOLLOW:
        if (status = PortAt(step.Graph, step.Source, source); !status)
            return status;
        defined.Kind = EditHandleKind::Path;
        defined.PathValue = edit.Follow(source);
        break;
    case BML_BEHAVIOR_EDIT_USE_NODE: {
        BML::Behavior::Internal::ObjectRef object;
        if (status = ObjectAt(step.Operand,
                              "A used Behavior node needs an object reference.",
                              object); !status)
            return status;
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = edit.UseNode(object);
        break;
    }
    case BML_BEHAVIOR_EDIT_USE_LINK: {
        BML::Behavior::Internal::ObjectRef object;
        if (status = ObjectAt(step.Operand,
                              "A used Behavior link needs an object reference.",
                              object); !status)
            return status;
        defined.Kind = EditHandleKind::Link;
        defined.LinkValue = edit.UseLink(object);
        break;
    }
    case BML_BEHAVIOR_EDIT_ADD_BLOCK: {
        BlockSpec block;
        if (status = BlockAt(step.Operand, context, "An added", block);
            !status)
            return status;
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = edit.Add(std::move(block));
        break;
    }
    case BML_BEHAVIOR_EDIT_ADD_GRAPH: {
        std::string name;
        if (!ReadNativeString(step.Name, name) || name.empty())
            return InvalidValue("An added graph-backed Node needs a name.");
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = edit.AddGraph(std::move(name), step.Number);
        break;
    }
    case BML_BEHAVIOR_EDIT_ENTER_GRAPH: {
        const EditHandle *target = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Node,
                         target); !status)
            return status;
        // A second scope for the same Node would hold handles the first
        // cannot use and give the Node's graph its public ports twice.
        const std::vector<Program::Nested> &entered = edit.NestedGraphs();
        if (std::any_of(entered.begin(), entered.end(),
                        [&](const Program::Nested &nested) {
                            return nested.Parent == target->NodeValue;
                        })) {
            return InvalidValue(
                "A Behavior edit enters the same nested graph twice.");
        }
        Program &nested = edit.Enter(target->NodeValue, step.Result);
        m_Graphs.emplace(step.Result, &nested);
        EditHandle nestedRoot;
        nestedRoot.Scope = step.Result;
        nestedRoot.NodeValue = nested.Graph();
        m_Handles.emplace(
            HandleKey{step.Result, BML_BEHAVIOR_EDIT_GRAPH}, nestedRoot);
        return {};
    }
    case BML_BEHAVIOR_EDIT_REPLACE_BLOCK: {
        const EditHandle *target = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Node, target); !status)
            return status;
        BlockSpec block;
        if (status = BlockAt(step.Operand, context, "A replacement", block);
            !status)
            return status;
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = edit.Replace(target->NodeValue, std::move(block));
        break;
    }
    case BML_BEHAVIOR_EDIT_REMOVE_NODE: {
        const EditHandle *target = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Node, target); !status)
            return status;
        edit.Remove(target->NodeValue);
        break;
    }
    case BML_BEHAVIOR_EDIT_PATTERN_PORT_COUNT: {
        const EditHandle *target = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Node,
                         target); !status)
            return status;
        SlotKind kind;
        if (!ReadSlotKind(step.SlotKind, kind)) {
            return InvalidValue(
                "A Node Pattern port count names an unknown port kind.");
        }
        if (step.Number < 0)
            return InvalidValue("A Node Pattern port count cannot be negative.");
        return edit.Count(target->NodeValue, kind, step.Number);
    }
    case BML_BEHAVIOR_EDIT_PATTERN_PORT_VALUE: {
        const EditHandle *target = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Node,
                         target); !status)
            return status;
        const BML_BehaviorPortRef *reference = nullptr;
        if (status = PortRefAt(step.Sink, reference); !status)
            return status;
        if (reference->Graph != step.Graph ||
            reference->Handle != step.Target || reference->Kind == 0) {
            return InvalidValue(
                "A Node Pattern value must name a port of its target Node.");
        }
        if (status = PortAt(step.Graph, step.Sink, sink); !status)
            return status;
        if (sink.Owner != target->NodeValue.Value)
            return InvalidValue(
                "A Node Pattern value resolved to a different Node.");
        Parameter::Binding binding;
        if (status = ValueAt(step.Operand, context, binding); !status)
            return status;
        if (binding.Kind() != Parameter::BindingKind::Value) {
            return {Error::WorldBoundValue, CKERR_INVALIDPARAMETER,
                    CKBR_PARAMETERERROR,
                    "A Node Pattern in a Plan cannot retain a live object."};
        }
        return edit.Observe(std::move(sink), binding.Literal());
    }
    case BML_BEHAVIOR_EDIT_ADD_OPERATION: {
        const BML_BehaviorOperationSpec *spec = nullptr;
        if (status = OperationAt(step.Operand, spec); !status)
            return status;
        const CKGUID operation = Guid(spec->Operation);
        const CKGUID result = Guid(spec->Result);
        const CKGUID input1 = Guid(spec->Input1);
        const CKGUID input2 = Guid(spec->Input2);
        if (!operation.IsValid() || !result.IsValid() ||
            result == CKPGUID_NONE) {
            return InvalidValue(
                "A Behavior Parameter Operation needs an operation GUID and result type.");
        }
        if (input2.IsValid() && input2 != CKPGUID_NONE &&
            (!input1.IsValid() || input1 == CKPGUID_NONE)) {
            return InvalidValue(
                "A Behavior Parameter Operation cannot have a second input without its first input.");
        }
        defined.Kind = EditHandleKind::Operation;
        defined.OperationValue = edit.AddOperation(
            operation, result, input1, input2);
        break;
    }
    case BML_BEHAVIOR_EDIT_APPEND_SLOT: {
        const EditHandle *owner = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Node, owner); !status)
            return status;
        std::string name;
        if (!ReadNativeString(step.Name, name) || name.empty())
            return InvalidValue("An appended Behavior slot needs a name.");
        const CKGUID type = Guid(step.Type);
        const bool typed = step.SlotKind == BML_BEHAVIOR_SLOT_PIN ||
                           step.SlotKind == BML_BEHAVIOR_SLOT_POUT ||
                           step.SlotKind == BML_BEHAVIOR_SLOT_LOCAL;
        if (typed && !type.IsValid()) {
            return InvalidValue(
                "An appended Behavior Pin, Pout, or Local needs a parameter type.");
        }
        defined.Kind = EditHandleKind::Port;
        switch (step.SlotKind) {
        case BML_BEHAVIOR_SLOT_IN:
            defined.PortValue = edit.AppendIn(owner->NodeValue, std::move(name));
            break;
        case BML_BEHAVIOR_SLOT_OUT:
            defined.PortValue = edit.AppendOut(owner->NodeValue, std::move(name));
            break;
        case BML_BEHAVIOR_SLOT_PIN:
            defined.PortValue =
                edit.AppendPin(owner->NodeValue, std::move(name), type);
            break;
        case BML_BEHAVIOR_SLOT_POUT:
            defined.PortValue =
                edit.AppendPout(owner->NodeValue, std::move(name), type);
            break;
        case BML_BEHAVIOR_SLOT_LOCAL:
            defined.PortValue =
                edit.AppendLocal(owner->NodeValue, std::move(name), type);
            break;
        default:
            return InvalidValue(
                "A Behavior edit can only append an In, Out, Pin, Pout, or Local.");
        }
        break;
    }
    case BML_BEHAVIOR_EDIT_FLOW:
        if (status = PortAt(step.Graph, step.Source, source); !status)
            return status;
        if (status = PortAt(step.Graph, step.Sink, sink); !status)
            return status;
        edit.Flow(source, sink, step.Number,
                  (step.Flags & BML_BEHAVIOR_EDIT_CONFIRM_CYCLE)
                      ? Cycle::Confirmed : Cycle::Reject);
        break;
    case BML_BEHAVIOR_EDIT_BIND_VALUE:
    case BML_BEHAVIOR_EDIT_SET_VALUE: {
        if (status = PortAt(step.Graph, step.Sink, sink); !status)
            return status;
        Parameter::Binding binding;
        if (status = ValueAt(step.Operand, context, binding); !status)
            return status;
        if (step.Kind == BML_BEHAVIOR_EDIT_BIND_VALUE)
            edit.Bind(sink, std::move(binding));
        else
            edit.Set(sink, std::move(binding));
        break;
    }
    case BML_BEHAVIOR_EDIT_BIND_PORT:
    case BML_BEHAVIOR_EDIT_SHARE:
    case BML_BEHAVIOR_EDIT_PUSH:
        if (status = PortAt(step.Graph, step.Source, source); !status)
            return status;
        if (status = PortAt(step.Graph, step.Sink, sink); !status)
            return status;
        if (step.Kind == BML_BEHAVIOR_EDIT_BIND_PORT)
            edit.Bind(sink, source);
        else if (step.Kind == BML_BEHAVIOR_EDIT_SHARE)
            edit.Share(sink, source);
        else
            edit.Push(source, sink);
        break;
    case BML_BEHAVIOR_EDIT_TAP: {
        if (status = PortAt(step.Graph, step.Source, source); !status)
            return status;
        HookBlock::Hook hook;
        if (status = HookAt(step.Operand, hook); !status)
            return status;
        edit.Tap(source, std::move(hook));
        break;
    }
    case BML_BEHAVIOR_EDIT_FLOW_HOOK: {
        if (status = PortAt(step.Graph, step.Source, source); !status)
            return status;
        if (status = PortAt(step.Graph, step.Sink, sink); !status)
            return status;
        HookBlock::Hook hook;
        if (status = HookAt(step.Operand, hook); !status)
            return status;
        edit.Flow(source, std::move(hook), sink);
        break;
    }
    case BML_BEHAVIOR_EDIT_AFTER: {
        const EditHandle *path = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Path, path); !status)
            return status;
        HookBlock::Hook hook;
        if (status = HookAt(step.Operand, hook); !status)
            return status;
        edit.After(path->PathValue, std::move(hook));
        break;
    }
    case BML_BEHAVIOR_EDIT_BEFORE: {
        const EditHandle *link = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Link, link); !status)
            return status;
        HookBlock::Hook hook;
        if (status = HookAt(step.Operand, hook); !status)
            return status;
        edit.Before(link->LinkValue, std::move(hook));
        break;
    }
    case BML_BEHAVIOR_EDIT_SPLICE: {
        const EditHandle *link = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Link, link); !status)
            return status;
        std::vector<Order> ordering;
        if (status = OrdersAt(step, ordering); !status)
            return status;
        if (step.Node) {
            const EditHandle *block = nullptr;
            if (status = Use(step.Graph, step.Node, EditHandleKind::Node, block); !status)
                return status;
            edit.Splice(link->LinkValue, block->NodeValue, std::move(ordering));
            break;
        }
        if (status = PortAt(step.Graph, step.Sink, sink); !status)
            return status;
        if (status = PortAt(step.Graph, step.Source, source); !status)
            return status;
        edit.Splice(link->LinkValue, sink, source, std::move(ordering));
        break;
    }
    case BML_BEHAVIOR_EDIT_REDIRECT: {
        const EditHandle *link = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Link, link); !status)
            return status;
        std::vector<Order> ordering;
        if (status = OrdersAt(step, ordering); !status)
            return status;
        if (status = PortAt(step.Graph, step.Sink, sink); !status)
            return status;
        edit.Redirect(link->LinkValue, sink, std::move(ordering));
        break;
    }
    case BML_BEHAVIOR_EDIT_REDIRECT_TO_LINK: {
        const EditHandle *link = nullptr;
        const EditHandle *destination = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Link,
                         link); !status)
            return status;
        if (status = Use(step.Graph, step.Node, EditHandleKind::Link,
                         destination); !status)
            return status;
        std::vector<Order> ordering;
        if (status = OrdersAt(step, ordering); !status)
            return status;
        edit.Redirect(link->LinkValue, destination->LinkValue,
                      std::move(ordering));
        break;
    }
    case BML_BEHAVIOR_EDIT_RECONNECT: {
        const EditHandle *link = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Link,
                         link); !status)
            return status;
        if (status = PortAt(step.Graph, step.Source, source); !status)
            return status;
        if (status = PortAt(step.Graph, step.Sink, sink); !status)
            return status;
        edit.Reconnect(link->LinkValue, source, sink,
                       (step.Flags & BML_BEHAVIOR_EDIT_CONFIRM_CYCLE)
                           ? Cycle::Confirmed : Cycle::Reject);
        break;
    }
    default:
        return InvalidValue("A Behavior edit step names an unknown operation.");
    }

    if (Defines(step.Kind))
        m_Handles.emplace(HandleKey{step.Graph, step.Result}, defined);
    return {};
}

Status ProgramDecoder::Use(std::uint32_t scope, std::uint32_t id,
                           EditHandleKind kind,
                           const EditHandle *&out) const {
    const auto found = m_Handles.find({scope, id});
    if (found == m_Handles.end()) {
        return InvalidValue(
            "A Behavior edit step read a handle no earlier step defined.");
    }
    if (found->second.Kind != kind)
        return InvalidValue("A Behavior edit step read a handle of another kind.");
    out = &found->second;
    return {};
}

Status ProgramDecoder::PortRefAt(std::uint32_t index,
                                 const BML_BehaviorPortRef *&out) const {
    out = Entry(m_Program->Ports, m_Program->PortCount, index);
    if (!out)
        return InvalidValue("A Behavior edit step names no port in its program.");
    if (out->StructSize < sizeof(*out))
        return InvalidValue("A Behavior port has an unsupported StructSize.");
    return {};
}

Status ProgramDecoder::PortAt(std::uint32_t scope, std::uint32_t index,
                              Port &out) const {
    const BML_BehaviorPortRef *from = nullptr;
    Status status = PortRefAt(index, from);
    if (!status)
        return status;
    if (from->Graph != scope) {
        return {Error::InvalidGraphLocality, CKERR_INVALIDPARAMETER,
                CKBR_PARAMETERERROR,
                "A Behavior edit step names a port of another graph scope."};
    }
    const EditHandle *handle = nullptr;
    if (from->Kind == 0) {
        if (status = Use(from->Graph, from->Handle, EditHandleKind::Port,
                         handle); !status)
            return status;
        out = handle->PortValue;
        return {};
    }
    SlotKind kind;
    if (!ReadSlotKind(from->Kind, kind))
        return InvalidValue("A Behavior port names an unknown slot kind.");
    status = Use(from->Graph, from->Handle, EditHandleKind::Node, handle);
    if (!status) {
        status = Use(from->Graph, from->Handle, EditHandleKind::Operation,
                     handle);
        if (!status)
            return InvalidValue(
                "A Behavior port owner is neither a Node nor a Parameter Operation.");
        if (kind != SlotKind::InputParameter &&
            kind != SlotKind::OutputParameter) {
            return InvalidValue(
                "A Parameter Operation exposes only Pin and Pout ports.");
        }
    }
    Slot slot;
    if (!ReadSelector(from->Slot, kind, Guid(from->Type), slot, status))
        return status;
    out = Port{handle->Kind == EditHandleKind::Node
                   ? handle->NodeValue.Value
                   : handle->OperationValue.Value,
               std::move(slot)};
    return {};
}

Status ProgramDecoder::PatternAt(std::uint32_t index,
                                 NodePattern &out) const {
    const BML_BehaviorNodePattern *from =
        Entry(m_Program->Patterns, m_Program->PatternCount, index);
    if (!from) {
        return InvalidValue(
            "A Behavior edit step names no Node Pattern in its program.");
    }
    return ReadNodePattern(*from, out);
}

Status ProgramDecoder::ValueAt(std::uint32_t index, ModContext &context,
                               Parameter::Binding &out) const {
    const BML_BehaviorValue *from =
        Entry(m_Program->Values, m_Program->ValueCount, index);
    if (!from)
        return InvalidValue("A Behavior edit step names no Value in its program.");
    Status status;
    ReadValue(*from, context, out, status);
    return status;
}

Status ProgramDecoder::BlockAt(std::uint32_t index, ModContext &context,
                               const char *role, BlockSpec &out) const {
    const BML_BehaviorBlock *from =
        Entry(m_Program->Blocks, m_Program->BlockCount, index);
    if (!from)
        return InvalidValue(std::string(role) + " Behavior Block is missing.");
    Status status;
    if (!ReadBlock(*from, context, out, status))
        return status;
    if (!out.Prototype().IsValid())
        return InvalidValue(std::string(role) +
                            " Behavior Block needs a Prototype.");
    if (!out.PrototypeGeneration()) {
        return InvalidValue(
            std::string(role) +
            " Behavior Block needs a fixed Prototype provider generation.");
    }
    return {};
}

Status ProgramDecoder::HookAt(std::uint32_t index,
                              HookBlock::Hook &out) const {
    return ReadHook(Entry(m_Program->Hooks, m_Program->HookCount, index), out);
}

Status ProgramDecoder::ObjectAt(std::uint32_t index, const char *message,
                                BML::Behavior::Internal::ObjectRef &out) const {
    const BML_ObjectRef *from =
        Entry(m_Program->Objects, m_Program->ObjectCount, index);
    if (!from || !from->Domain)
        return InvalidValue(message);
    out = {from->Domain, from->Slot, from->Generation};
    return {};
}

Status ProgramDecoder::OperationAt(
    std::uint32_t index, const BML_BehaviorOperationSpec *&out) const {
    out = Entry(m_Program->Operations, m_Program->OperationCount, index);
    if (!out) {
        return InvalidValue(
            "A Behavior edit step names no Parameter Operation in its program.");
    }
    if (out->StructSize < sizeof(*out)) {
        return InvalidValue(
            "A Behavior Parameter Operation has an unsupported StructSize.");
    }
    return {};
}

Status ProgramDecoder::OrdersAt(const BML_BehaviorEditStep &step,
                                std::vector<Order> &out) const {
    // Operand names the first entry and Number the entry count.
    const std::uint32_t first = step.Operand;
    const std::uint32_t available = m_Program->OrderCount;
    if (step.Number < 0 || (step.Number == 0) != (first == 0) ||
        (first && (first - 1 > available ||
                   static_cast<std::uint32_t>(step.Number) >
                       available - (first - 1)))) {
        return InvalidValue(
            "A Behavior Patch ordering range lies outside its program.");
    }
    for (std::int32_t offset = 0; offset < step.Number; ++offset) {
        const BML_BehaviorEditOrder &order = m_Program->Orders[first - 1 + offset];
        if (order.StructSize < sizeof(order)) {
            return InvalidValue(
                "A Behavior Patch ordering entry has an unsupported StructSize.");
        }
        OrderKind kind;
        switch (order.Kind) {
        case BML_BEHAVIOR_ORDER_BEFORE: kind = OrderKind::Before; break;
        case BML_BEHAVIOR_ORDER_AFTER: kind = OrderKind::After; break;
        default:
            return InvalidValue(
                "A Behavior Patch ordering entry names an unknown order.");
        }
        std::string owner;
        std::string name;
        if (!ReadString(order.Owner, owner) || owner.empty() ||
            !ReadString(order.Name, name) || name.empty()) {
            return InvalidValue(
                "A Behavior Patch ordering entry needs an owner and name.");
        }
        out.push_back({kind, PatchKey{std::move(owner), std::move(name)}});
    }
    return {};
}

} // namespace

Status DecodeProgram(
    const BML_BehaviorEditProgram &program, ModContext &context, Program &out,
    Installations::SymbolMap *symbols, std::uint64_t binding) {
    ProgramDecoder decoder;
    const Status status = decoder.Build(program, context, out);
    if (!status || !symbols)
        return status;
    for (const auto &[reference, symbol] : decoder.Symbols()) {
        auto bound = reference;
        bound.Binding = binding;
        symbols->emplace(bound, symbol);
    }
    return {};
}

Status ReadScriptEdits(
    const BML_BehaviorScriptEdit *edits, std::uint32_t count,
    ModContext &context,
    std::vector<Installations::Rule> &out) {
    out.clear();
    if (!edits || !count)
        return InvalidValue("A Behavior Plan requires at least one Script rule.");
    try {
        out.reserve(count);
        std::set<std::uint64_t> bindings;
        for (std::uint32_t index = 0; index < count; ++index) {
            const BML_BehaviorScriptEdit &source = edits[index];
            if (!HasStructSize(&source) || !source.Binding)
                return InvalidValue("A Script Edit descriptor is malformed.");
            if (!bindings.insert(source.Binding).second) {
                return InvalidValue(
                    "Script Edits in one Behavior Plan require distinct bindings.");
            }
            TargetSet targets;
            switch (source.Targets) {
            case BML_BEHAVIOR_TARGETS_EACH: targets = TargetSet::Each; break;
            case BML_BEHAVIOR_TARGETS_ONE: targets = TargetSet::One; break;
            default:
                return InvalidValue("A Script Edit has an unknown cardinality.");
            }
            std::string script;
            if (!ReadNativeString(source.Script, script) || script.empty())
                return InvalidValue("A Script Edit requires an exact name.");
            Program edit;
            Installations::SymbolMap symbols;
            const Status status = DecodeProgram(
                source.Program, context, edit, &symbols, source.Binding);
            if (!status)
                return status;
            out.push_back({ScriptSelection{std::move(script), targets},
                           source.Binding,
                           std::make_shared<Program>(std::move(edit)),
                           std::move(symbols)});
        }
    } catch (const std::bad_alloc &) {
        throw;
    } catch (...) {
        return {Error::CreateFailed, CKERR_INVALIDPARAMETER,
                CKBR_PARAMETERERROR,
                "The Loader could not retain the Behavior Plan rules."};
    }
    return {};
}

Status ReadGraphEdits(
    const BML_BehaviorGraphEdit *edits, std::uint32_t count,
    ModContext &context,
    std::vector<Installations::Target> &out) {
    out.clear();
    if (!edits || !count)
        return InvalidValue("A Behavior Patch requires at least one Graph Edit.");
    try {
        out.reserve(count);
        std::set<std::uint64_t> bindings;
        for (std::uint32_t index = 0; index < count; ++index) {
            const BML_BehaviorGraphEdit &source = edits[index];
            if (!HasStructSize(&source) || source.Reserved != 0 ||
                !source.Binding || !source.Graph.Domain)
                return InvalidValue("A Graph Edit descriptor is malformed.");
            if (!bindings.insert(source.Binding).second) {
                return InvalidValue(
                    "Graph Edits in one Behavior Patch require distinct bindings.");
            }
            Program edit;
            Installations::SymbolMap symbols;
            const Status status = DecodeProgram(
                source.Program, context, edit, &symbols, source.Binding);
            if (!status)
                return status;
            Installations::Target target;
            target.Graph = {source.Graph.Domain, source.Graph.Slot,
                            source.Graph.Generation};
            target.Fingerprint = source.Fingerprint;
            target.Binding = source.Binding;
            target.Body = std::make_shared<Program>(std::move(edit));
            target.Symbols = std::move(symbols);
            out.push_back(std::move(target));
        }
    } catch (const std::bad_alloc &) {
        return InvalidValue("The Loader could not retain the composed Behavior Patch.");
    }
    return {};
}

Status ReadHook(const BML_BehaviorHookFunction *from,
                HookBlock::Hook &out) {
    if (!from || from->StructSize < sizeof(*from) || !from->Invoke)
        return InvalidValue("A Behavior Hook needs a callback.");
    if ((from->Retain == nullptr) != (from->Release == nullptr)) {
        return InvalidValue(
            "A Behavior Hook needs both Retain and Release, or neither.");
    }
    // The record takes the caller reference here, so the caller may drop its
    // own as soon as the edit or AttachHook call returns. The matching Release
    // runs when the last holder of the record drops it, which is later than
    // retirement for a Conflicted Patch and earlier than any lease for an edit
    // that never installs.
    auto thunk = std::make_shared<HookThunk>();
    thunk->Function = *from;
    if (from->Retain) {
        try {
            from->Retain(from->State);
            thunk->Retained = true;
        } catch (const std::exception &exception) {
            Status failure{Error::CallbackFailed, CK_OK,
                           CKBR_BEHAVIORERROR, exception.what()};
            failure.Details.Stage = Phase::LifecycleCallback;
            return failure;
        } catch (...) {
            Status failure{
                Error::CallbackFailed, CK_OK, CKBR_BEHAVIORERROR,
                "A Behavior Hook Retain callback threw an exception."};
            failure.Details.Stage = Phase::LifecycleCallback;
            return failure;
        }
    }
    out = HookBlock::Hook(
        PlanCallbackState::Retained(thunk, from->State, nullptr, nullptr),
        &InvokeHook, thunk.get(),
        HookBlock::Hook::Identity{
            reinterpret_cast<std::uintptr_t>(from->State),
            reinterpret_cast<std::uintptr_t>(from->Retain),
            reinterpret_cast<std::uintptr_t>(from->Release),
            reinterpret_cast<std::uintptr_t>(from->Invoke)});
    return {};
}

} // namespace BML::Api::Behavior

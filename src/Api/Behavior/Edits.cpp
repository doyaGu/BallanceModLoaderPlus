#include "Api/Behavior/Codec.h"

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
using BML::Behavior::Internal::ScriptSelection;
using BML::Behavior::Internal::TargetSet;
namespace HookBlock = BML::Behavior::Internal::HookBlock;

// The record a Hook Block hands back to InvokeHook. The callback state owns it,
// so it outlives the Hook occurrence and every Binding taken from that
// occurrence, including Bindings a Conflicted Plan can no longer revert.
struct HookThunk {
    ~HookThunk() noexcept {
        if (!Retained || !Function.Release)
            return;
        try {
            Function.Release(Function.State);
        } catch (...) {
            // A foreign release callback must not cross the Loader boundary.
        }
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

// Translates the shared wire edit steps into one symbolic Program. The same
// value can be applied once to a known graph or retained by a Plan.
class ProgramDecoder final {
public:
    Status Build(const BML_BehaviorEditStep *steps, std::uint32_t count,
                 ModContext &context, Program &edit);
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
    Status ReadPort(const BML_BehaviorPortRef &from, Port &out) const;
    Status ReadHook(const BML_BehaviorHookFunction *from,
                    HookBlock::Hook &out) const;
    Status ReadOrdering(const BML_BehaviorEditStep &step,
                        std::vector<Order> &out) const;

    using HandleKey = std::pair<std::uint32_t, std::uint32_t>;
    std::map<HandleKey, EditHandle> m_Handles;
    std::map<std::uint32_t, Program *> m_Graphs;
};

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

Status ProgramDecoder::Build(const BML_BehaviorEditStep *steps,
                             std::uint32_t count, ModContext &context,
                             Program &edit) {
    EditHandle graph;
    graph.Scope = BML_BEHAVIOR_EDIT_GRAPH;
    graph.NodeValue = edit.Graph();
    m_Handles.clear();
    m_Graphs.clear();
    m_Graphs.emplace(BML_BEHAVIOR_EDIT_GRAPH, &edit);
    m_Handles.emplace(HandleKey{BML_BEHAVIOR_EDIT_GRAPH,
                                BML_BEHAVIOR_EDIT_GRAPH}, graph);
    for (std::uint32_t index = 0; index < count; ++index) {
        const Status status = Step(steps[index], context, edit);
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

Status ReadNodePattern(const BML_BehaviorEditStep &step,
                       NodePattern &out, bool required) {
    out = {};
    const CKGUID prototype = Guid(step.Prototype.Prototype);
    if ((prototype.IsValid() || step.Prototype.Generation != 0) &&
        step.Prototype.StructSize < sizeof(step.Prototype)) {
        return InvalidValue(
            "A Node Pattern has an unsupported Prototype reference.");
    }
    if (step.Prototype.Generation != 0) {
        return InvalidValue(
            "A Node Pattern matches a Prototype GUID, not a provider generation.");
    }

    switch (step.Selector.Kind) {
    case BML_BEHAVIOR_SELECTOR_INDEX:
        if (step.Selector.StructSize < sizeof(step.Selector) ||
            step.Selector.Index < 0)
            return InvalidValue("A Node Pattern index is invalid.");
        out.Selector = NodePattern::SelectorKind::Index;
        out.Index = step.Selector.Index;
        break;
    case BML_BEHAVIOR_SELECTOR_NAME:
    case BML_BEHAVIOR_SELECTOR_UNIQUE_NAME:
        if (step.Selector.StructSize < sizeof(step.Selector) ||
            !ReadString(step.Selector.Name, out.Name) || out.Name.empty() ||
            step.Selector.Occurrence < 0)
            return InvalidValue("A Node Pattern name is invalid.");
        out.Selector = NodePattern::SelectorKind::Name;
        out.Occurrence = step.Selector.Occurrence;
        out.Unique = step.Selector.Kind == BML_BEHAVIOR_SELECTOR_UNIQUE_NAME;
        break;
    case BML_BEHAVIOR_SELECTOR_ONLY:
        if (step.Selector.StructSize < sizeof(step.Selector))
            return InvalidValue("A Node Pattern selector is invalid.");
        out.Selector = NodePattern::SelectorKind::Only;
        break;
    default:
        return InvalidValue("A Node Pattern selector is unknown.");
    }

    std::string expectedName;
    if (!ReadString(step.Name, expectedName))
        return InvalidValue("A Node Pattern expected name is invalid.");
    if (!expectedName.empty()) {
        if (out.Selector == NodePattern::SelectorKind::Name &&
            out.Name != expectedName) {
            return InvalidValue(
                "A Node Pattern names two different Behaviors.");
        }
        out.Name = std::move(expectedName);
    }

    out.Prototype = prototype;
    if (step.ExpectedKind) {
        switch (step.ExpectedKind) {
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
    }
    if (step.ReservedShape != 0)
        return InvalidValue("A Node Pattern has reserved shape data.");
    out.PortShape = step.PortShape;
    if (required && !out)
        return InvalidValue("A Node Pattern has no observable condition.");
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
        if (status = ReadNodePattern(step, query, true); !status)
            return status;
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = step.Kind == BML_BEHAVIOR_EDIT_EACH_NODE
            ? edit.Each(std::move(query))
            : edit.RequireOne(std::move(query));
        break;
    }
    case BML_BEHAVIOR_EDIT_REQUIRE_LINK: {
        if (status = ReadPort(step.Source, source); !status)
            return status;
        if (status = ReadPort(step.Sink, sink); !status)
            return status;
        std::optional<int> delay;
        if (step.Flags & BML_BEHAVIOR_EDIT_HAS_DELAY)
            delay = step.Delay;
        defined.Kind = EditHandleKind::Link;
        defined.LinkValue = edit.RequireOne(source, sink, delay);
        break;
    }
    case BML_BEHAVIOR_EDIT_NEXT_NODE: {
        if (status = ReadPort(step.Source, source); !status)
            return status;
        NodePattern expected;
        if (status = ReadNodePattern(step, expected, false); !status)
            return status;
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = expected
            ? edit.Next(source, std::move(expected)) : edit.Next(source);
        break;
    }
    case BML_BEHAVIOR_EDIT_PREVIOUS_NODE: {
        if (status = ReadPort(step.Sink, sink); !status)
            return status;
        NodePattern expected;
        if (status = ReadNodePattern(step, expected, false); !status)
            return status;
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = expected
            ? edit.Previous(sink, std::move(expected)) : edit.Previous(sink);
        break;
    }
    case BML_BEHAVIOR_EDIT_LEAVING_LINK:
        if (status = ReadPort(step.Source, source); !status)
            return status;
        defined.Kind = EditHandleKind::Link;
        defined.LinkValue = edit.Leaving(source);
        break;
    case BML_BEHAVIOR_EDIT_ENTERING_LINK:
        if (status = ReadPort(step.Sink, sink); !status)
            return status;
        defined.Kind = EditHandleKind::Link;
        defined.LinkValue = edit.Entering(sink);
        break;
    case BML_BEHAVIOR_EDIT_LINK_TO_NODE: {
        if (status = ReadPort(step.Source, source); !status)
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
        if (status = ReadPort(step.Source, source); !status)
            return status;
        defined.Kind = EditHandleKind::Path;
        defined.PathValue = edit.Follow(source);
        break;
    case BML_BEHAVIOR_EDIT_USE_NODE: {
        if (!step.Object.Domain) {
            return InvalidValue(
                "A used Behavior node needs an object reference.");
        }
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = edit.UseNode(
            BML::Behavior::Internal::ObjectRef{step.Object.Domain, step.Object.Slot,
                                     step.Object.Generation});
        break;
    }
    case BML_BEHAVIOR_EDIT_USE_LINK: {
        if (!step.Object.Domain) {
            return InvalidValue(
                "A used Behavior link needs an object reference.");
        }
        defined.Kind = EditHandleKind::Link;
        defined.LinkValue = edit.UseLink(
            BML::Behavior::Internal::ObjectRef{step.Object.Domain, step.Object.Slot,
                                     step.Object.Generation});
        break;
    }
    case BML_BEHAVIOR_EDIT_ADD_BLOCK: {
        if (!step.Block)
            return InvalidValue("An added Behavior Block is missing.");
        BlockSpec block;
        if (!ReadBlock(*step.Block, context, block, status))
            return status;
        if (!block.Prototype().IsValid())
            return InvalidValue("An added Behavior Block needs a Prototype.");
        if (!block.PrototypeGeneration()) {
            return InvalidValue(
                "An added Behavior Block needs a fixed Prototype provider generation.");
        }
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = edit.Add(std::move(block));
        break;
    }
    case BML_BEHAVIOR_EDIT_ADD_GRAPH: {
        std::string name;
        if (!ReadString(step.Name, name) || name.empty())
            return InvalidValue("An added graph-backed Node needs a name.");
        defined.Kind = EditHandleKind::Node;
        defined.NodeValue = edit.AddGraph(std::move(name), step.Priority);
        break;
    }
    case BML_BEHAVIOR_EDIT_ENTER_GRAPH: {
        if (!step.Result || step.Result == BML_BEHAVIOR_EDIT_GRAPH ||
            m_Graphs.find(step.Result) != m_Graphs.end())
            return InvalidValue("A nested graph scope handle is invalid.");
        const EditHandle *target = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Node,
                         target); !status)
            return status;
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
        if (!step.Block)
            return InvalidValue("A replacement Behavior Block is missing.");
        BlockSpec block;
        if (!ReadBlock(*step.Block, context, block, status))
            return status;
        if (!block.Prototype().IsValid())
            return InvalidValue("A replacement Behavior Block needs a Prototype.");
        if (!block.PrototypeGeneration()) {
            return InvalidValue(
                "A replacement Behavior Block needs a fixed Prototype provider generation.");
        }
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
        switch (step.SlotKind) {
        case BML_BEHAVIOR_SLOT_IN: kind = SlotKind::Input; break;
        case BML_BEHAVIOR_SLOT_OUT: kind = SlotKind::Output; break;
        case BML_BEHAVIOR_SLOT_PIN: kind = SlotKind::InputParameter; break;
        case BML_BEHAVIOR_SLOT_POUT: kind = SlotKind::OutputParameter; break;
        case BML_BEHAVIOR_SLOT_SETTING: kind = SlotKind::Setting; break;
        case BML_BEHAVIOR_SLOT_LOCAL: kind = SlotKind::Local; break;
        case BML_BEHAVIOR_SLOT_TARGET: kind = SlotKind::Target; break;
        default:
            return InvalidValue(
                "A Node Pattern port count names an unknown port kind.");
        }
        if (step.Delay < 0)
            return InvalidValue("A Node Pattern port count cannot be negative.");
        return edit.Count(target->NodeValue, kind, step.Delay);
    }
    case BML_BEHAVIOR_EDIT_PATTERN_PORT_VALUE: {
        const EditHandle *target = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Node,
                         target); !status)
            return status;
        if (step.Sink.Graph != step.Graph ||
            step.Sink.Handle != step.Target || step.Sink.Kind == 0) {
            return InvalidValue(
                "A Node Pattern value must name a port of its target Node.");
        }
        if (status = ReadPort(step.Sink, sink); !status)
            return status;
        if (sink.Owner != target->NodeValue.Value)
            return InvalidValue(
                "A Node Pattern value resolved to a different Node.");
        Parameter::Binding binding;
        if (!ReadValue(step.Value, context, binding, status))
            return status;
        if (binding.Kind() != Parameter::BindingKind::Value) {
            return {Error::WorldBoundValue, CKERR_INVALIDPARAMETER,
                    CKBR_PARAMETERERROR,
                    "A Node Pattern in a Plan cannot retain a live object."};
        }
        return edit.Observe(std::move(sink), binding.Literal());
    }
    case BML_BEHAVIOR_EDIT_ADD_OPERATION: {
        if (step.Operation.StructSize < sizeof(step.Operation)) {
            return InvalidValue(
                "A Behavior Parameter Operation has an unsupported StructSize.");
        }
        const CKGUID operation = Guid(step.Operation.Operation);
        const CKGUID result = Guid(step.Operation.Result);
        const CKGUID input1 = Guid(step.Operation.Input1);
        const CKGUID input2 = Guid(step.Operation.Input2);
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
        if (!ReadString(step.Name, name) || name.empty())
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
        if (status = ReadPort(step.Source, source); !status)
            return status;
        if (status = ReadPort(step.Sink, sink); !status)
            return status;
        edit.Flow(source, sink, step.Delay,
                  (step.Flags & BML_BEHAVIOR_EDIT_CONFIRM_CYCLE)
                      ? Cycle::Confirmed : Cycle::Reject);
        break;
    case BML_BEHAVIOR_EDIT_BIND_VALUE: {
        if (status = ReadPort(step.Sink, sink); !status)
            return status;
        Parameter::Binding binding;
        if (!ReadValue(step.Value, context, binding, status))
            return status;
        edit.Bind(sink, std::move(binding));
        break;
    }
    case BML_BEHAVIOR_EDIT_SET_VALUE: {
        if (status = ReadPort(step.Sink, sink); !status)
            return status;
        Parameter::Binding binding;
        if (!ReadValue(step.Value, context, binding, status))
            return status;
        edit.Set(sink, std::move(binding));
        break;
    }
    case BML_BEHAVIOR_EDIT_BIND_PORT:
    case BML_BEHAVIOR_EDIT_SHARE:
    case BML_BEHAVIOR_EDIT_PUSH:
        if (status = ReadPort(step.Source, source); !status)
            return status;
        if (status = ReadPort(step.Sink, sink); !status)
            return status;
        if (step.Kind == BML_BEHAVIOR_EDIT_BIND_PORT)
            edit.Bind(sink, source);
        else if (step.Kind == BML_BEHAVIOR_EDIT_SHARE)
            edit.Share(sink, source);
        else
            edit.Push(source, sink);
        break;
    case BML_BEHAVIOR_EDIT_TAP: {
        if (status = ReadPort(step.Source, source); !status)
            return status;
        HookBlock::Hook hook;
        if (status = ReadHook(step.Hook, hook); !status)
            return status;
        edit.Tap(source, std::move(hook));
        break;
    }
    case BML_BEHAVIOR_EDIT_FLOW_HOOK: {
        if (status = ReadPort(step.Source, source); !status)
            return status;
        if (status = ReadPort(step.Sink, sink); !status)
            return status;
        HookBlock::Hook hook;
        if (status = ReadHook(step.Hook, hook); !status)
            return status;
        edit.Flow(source, std::move(hook), sink);
        break;
    }
    case BML_BEHAVIOR_EDIT_AFTER: {
        const EditHandle *path = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Path, path); !status)
            return status;
        HookBlock::Hook hook;
        if (status = ReadHook(step.Hook, hook); !status)
            return status;
        edit.After(path->PathValue, std::move(hook));
        break;
    }
    case BML_BEHAVIOR_EDIT_BEFORE: {
        const EditHandle *link = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Link, link); !status)
            return status;
        HookBlock::Hook hook;
        if (status = ReadHook(step.Hook, hook); !status)
            return status;
        edit.Before(link->LinkValue, std::move(hook));
        break;
    }
    case BML_BEHAVIOR_EDIT_SPLICE: {
        const EditHandle *link = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Link, link); !status)
            return status;
        std::vector<Order> ordering;
        if (status = ReadOrdering(step, ordering); !status)
            return status;
        if (step.Node) {
            const EditHandle *block = nullptr;
            if (status = Use(step.Graph, step.Node, EditHandleKind::Node, block); !status)
                return status;
            edit.Splice(link->LinkValue, block->NodeValue, std::move(ordering));
            break;
        }
        if (status = ReadPort(step.Sink, sink); !status)
            return status;
        if (status = ReadPort(step.Source, source); !status)
            return status;
        edit.Splice(link->LinkValue, sink, source, std::move(ordering));
        break;
    }
    case BML_BEHAVIOR_EDIT_REDIRECT: {
        const EditHandle *link = nullptr;
        if (status = Use(step.Graph, step.Target, EditHandleKind::Link, link); !status)
            return status;
        std::vector<Order> ordering;
        if (status = ReadOrdering(step, ordering); !status)
            return status;
        if (status = ReadPort(step.Sink, sink); !status)
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
        if (status = ReadOrdering(step, ordering); !status)
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
        if (status = ReadPort(step.Source, source); !status)
            return status;
        if (status = ReadPort(step.Sink, sink); !status)
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

Status ProgramDecoder::ReadPort(const BML_BehaviorPortRef &from, Port &out) const {
    if (from.StructSize < sizeof(from))
        return InvalidValue("A Behavior port has an unsupported StructSize.");
    const EditHandle *handle = nullptr;
    if (from.Kind == 0) {
        const Status status = Use(from.Graph, from.Handle,
                                  EditHandleKind::Port, handle);
        if (!status)
            return status;
        out = handle->PortValue;
        return {};
    }
    SlotKind kind;
    if (!ReadSlotKind(from.Kind, kind))
        return InvalidValue("A Behavior port names an unknown slot kind.");
    Status status = Use(from.Graph, from.Handle, EditHandleKind::Node, handle);
    if (!status) {
        status = Use(from.Graph, from.Handle, EditHandleKind::Operation, handle);
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
    if (!ReadSelector(from.Slot, kind, Guid(from.Type), slot, status))
        return status;
    out = Port{handle->Kind == EditHandleKind::Node
                   ? handle->NodeValue.Value
                   : handle->OperationValue.Value,
               std::move(slot)};
    return {};
}

Status ProgramDecoder::ReadHook(const BML_BehaviorHookFunction *from,
                                HookBlock::Hook &out) const {
    if (!from || from->StructSize < sizeof(*from) || !from->Invoke)
        return InvalidValue("A Behavior Hook needs a callback.");
    if ((from->Retain == nullptr) != (from->Release == nullptr)) {
        return InvalidValue(
            "A Behavior Hook needs both Retain and Release, or neither.");
    }
    // The record takes the caller reference here, so the caller may drop its
    // own as soon as this edit is accepted. The matching Release runs when the
    // last holder of the record drops it, which is later than retirement for a
    // Conflicted Patch and earlier than any lease for an edit that never installs.
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

Status ProgramDecoder::ReadOrdering(const BML_BehaviorEditStep &step,
                                    std::vector<Order> &out) const {
    if (step.OrderCount && !step.Ordering)
        return InvalidValue("A Behavior Patch ordering array is missing.");
    for (std::uint32_t index = 0; index < step.OrderCount; ++index) {
        const BML_BehaviorEditOrder &order = step.Ordering[index];
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
    const BML_BehaviorEditStep *steps, std::uint32_t count,
    ModContext &context, Program &out,
    Installations::SymbolMap *symbols,
    std::uint64_t binding) {
    ProgramDecoder decoder;
    const Status status = decoder.Build(steps, count, context, out);
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
            if (!HasStructSize(&source) || !source.Binding ||
                source.Reserved != 0 ||
                (source.StepCount && !source.Steps))
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
            if (!ReadString(source.Script, script) || script.empty())
                return InvalidValue("A Script Edit requires an exact name.");
            Program edit;
            Installations::SymbolMap symbols;
            const Status status = DecodeProgram(
                source.Steps, source.StepCount, context, edit, &symbols,
                source.Binding);
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
                !source.Binding ||
                !source.Graph.Domain ||
                (source.StepCount && !source.Steps))
                return InvalidValue("A Graph Edit descriptor is malformed.");
            if (!bindings.insert(source.Binding).second) {
                return InvalidValue(
                    "Graph Edits in one Behavior Patch require distinct bindings.");
            }
            Program edit;
            Installations::SymbolMap symbols;
            const Status status = DecodeProgram(
                source.Steps, source.StepCount, context, edit, &symbols,
                source.Binding);
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

} // namespace BML::Api::Behavior

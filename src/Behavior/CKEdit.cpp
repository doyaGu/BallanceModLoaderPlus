#include "Behavior/CKEdit.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace BML::Behavior {
namespace {

Status Failure(Error error, std::string message,
               CKERROR ckError = CKERR_INVALIDPARAMETER) {
    Status status{error, ckError, CKBR_PARAMETERERROR, std::move(message)};
    status.Details.Stage = Phase::Edit;
    return status;
}

struct Stamp {
    CK_ID Id = 0;
    CKObject *Address = nullptr;
};

Stamp Capture(CKObject *object) {
    return {object ? object->GetID() : 0, object};
}

template <typename T>
T *Resolve(CKContext *context, Stamp stamp, CK_CLASSID type) {
    CKObject *object = context && stamp.Id ? context->GetObject(stamp.Id) : nullptr;
    if (object != stamp.Address || !object || object->IsToBeDeleted() ||
        !CKIsChildClassOf(object, type))
        return nullptr;
    return static_cast<T *>(object);
}

CKBehavior *ResolveBehavior(CKContext *context, NativeRef native) {
    if (!context || !native ||
        native.Id > static_cast<std::uint64_t>((std::numeric_limits<CK_ID>::max)()))
        return nullptr;
    CKObject *object = context->GetObject(static_cast<CK_ID>(native.Id));
    if (object != native.Address || !object || object->IsToBeDeleted() ||
        !CKIsChildClassOf(object, CKCID_BEHAVIOR))
        return nullptr;
    return static_cast<CKBehavior *>(object);
}

bool ContainsDestination(CKParameterOut *source, CKParameter *destination) {
    if (!source || !destination)
        return false;
    for (int index = 0; index < source->GetDestinationCount(); ++index) {
        if (source->GetDestination(index) == destination)
            return true;
    }
    return false;
}

using ParameterId = std::uint64_t;

constexpr ParameterId kPlannedParameter = 1ull << 63u;

ParameterId PlanParameter(const ResolvedPort &port) {
    return kPlannedParameter |
           (static_cast<ParameterId>(port.Owner.Value) << 32u) |
           (static_cast<ParameterId>(port.Slot.Kind) << 24u) |
           static_cast<std::uint32_t>(port.Slot.NativeIndex);
}

ParameterId NativeParameter(CKObject *parameter) {
    return parameter
        ? static_cast<std::uint32_t>(parameter->GetID()) : 0;
}

std::uint64_t HashTap(const GraphEndpoint &source, std::uint32_t ordinal) {
    std::uint64_t hash = 1469598103934665603ull;
    const auto add = [&](std::uint64_t value) {
        for (int byte = 0; byte < 8; ++byte) {
            hash ^= static_cast<unsigned char>(value >> (byte * 8));
            hash *= 1099511628211ull;
        }
    };
    add(source.Node);
    add(static_cast<std::uint64_t>(source.Kind));
    add(static_cast<std::uint32_t>(source.Index));
    add(ordinal);
    return hash;
}

} // namespace

struct Patch::Data {
    struct Link {
        Stamp Value;
    };

    struct Binding {
        Stamp Input;
        Stamp PreviousDirect;
        Stamp PreviousShared;
        Stamp InstalledDirect;
        Stamp InstalledShared;
        Stamp Literal;
    };

    struct Destination {
        Stamp Source;
        Stamp Target;
    };

    struct Interface {
        Stamp Behavior;
        Stamp Port;
        SlotKind Kind = SlotKind::Input;
    };

    CKEdit *Editor = nullptr;
    Stamp Graph;
    PatchKey Key;
    std::vector<Stamp> Nodes;
    std::vector<Link> Links;
    std::vector<Binding> Binds;
    std::vector<Destination> Pushes;
    std::vector<Interface> Ports;
    bool Active = false;
};

Patch::Patch() = default;
Patch::~Patch() = default;
Patch::Patch(Patch &&) noexcept = default;
Patch &Patch::operator=(Patch &&) noexcept = default;

Patch::operator bool() const noexcept {
    return m_Data && m_Data->Active;
}

CKEdit::CKEdit(CKContext *context, Runtime &runtime,
               PrototypeCatalog *catalog, GraphSource &graph)
    : m_Context(context), m_Runtime(runtime), m_Catalog(catalog),
      m_Graph(graph), m_Thread(std::this_thread::get_id()) {}

Status CKEdit::Ready() const {
    if (!m_Context)
        return Failure(Error::ContextExpired,
                       "Virtools context is unavailable.", CKERR_INVALIDOBJECT);
    if (m_Thread != std::this_thread::get_id())
        return Failure(Error::WrongThread,
                       "Behavior graph Edits require the game thread.");
    return {};
}

Status CKEdit::Begin(CKBehavior *graph, PatchKey key, Edit &out) {
    Status status = Ready();
    if (!status)
        return status;
    NativeRef native;
    status = m_Graph.Refer(graph, native);
    Layout layout;
    if (status)
        status = m_Graph.ReadLayout(native, layout);
    if (!status)
        return status;
    if (!graph || graph->IsUsingFunction())
        return Failure(Error::InvalidGraphLocality,
                       "Behavior Edit requires a graph-backed Behavior.");
    out = Edit(std::move(key), native, std::move(layout));
    return {};
}

Status CKEdit::Use(Edit &edit, CKBehavior *behavior, Node &out) {
    out = {};
    Status status = Ready();
    if (!status)
        return status;
    NativeRef native;
    status = m_Graph.Refer(behavior, native);
    Layout layout;
    if (status)
        status = m_Graph.ReadLayout(native, layout);
    if (!status)
        return status;
    out = edit.Use(native, std::move(layout));
    return {};
}

Status CKEdit::Add(Edit &edit, Spec block, Node &out) {
    out = {};
    Status status = Ready();
    if (!status)
        return status;
    if (!m_Catalog)
        return Failure(Error::Unavailable,
                       "Behavior Prototype discovery is unavailable.");
    Layout declared;
    status = m_Catalog->DeclaredLayout(
        {block.Prototype(), block.PrototypeGeneration()}, declared);
    if (!status)
        return status;
    if (!block.PrototypeGeneration())
        block.PrototypeGeneration(declared.ProviderGeneration);
    out = edit.Add(std::move(block), std::move(declared));
    return {};
}

Status CKEdit::Apply(const Edit &edit, Patch &out) {
    Status status = Ready();
    if (!status)
        return status;
    if (out)
        return Failure(Error::InvalidState,
                       "The destination Patch is already active.");
    if (edit.m_Nodes.empty())
        return Failure(Error::InvalidState, "The Edit has no graph.");

    CKBehavior *graph = ResolveBehavior(m_Context, edit.m_Nodes.front().Native);
    if (!graph || graph->IsUsingFunction())
        return Failure(Error::InvalidGraphLocality,
                       "The Edit graph is stale or no longer graph-backed.",
                       CKERR_INVALIDOBJECT);
    const std::uint64_t graphId = static_cast<std::uint32_t>(graph->GetID());
    if (m_Active[graphId].contains(edit.Key()))
        return Failure(Error::InvalidState,
                       "The same owner and patch key is already active on this graph.");

    GraphModel base;
    status = m_Graph.Read(edit.m_Nodes.front().Native,
                          GraphView::Logical, base);
    CheckedEdit checked;
    if (status)
        status = edit.Validate(base, checked);
    if (!status)
        return status;

    auto patch = std::make_unique<Patch::Data>();
    patch->Editor = this;
    patch->Graph = Capture(graph);
    patch->Key = edit.Key();

    std::unordered_map<std::uint32_t, CKBehavior *> nodes;
    nodes.emplace(edit.Graph().Value, graph);
    for (std::size_t index = 1; index < edit.m_Nodes.size(); ++index) {
        const Edit::EditNode &node = edit.m_Nodes[index];
        if (node.Block)
            continue;
        CKBehavior *native = ResolveBehavior(m_Context, node.Native);
        if (!native || native->GetParent() != graph) {
            status = Failure(Error::InvalidGraphLocality,
                             "A borrowed Node left the target graph before Apply.",
                             CKERR_INVALIDOBJECT);
            break;
        }
        nodes.emplace(node.Handle.Value, native);
    }

    const auto fail = [&](Status failure) {
        (void) Undo(*patch, false);
        return failure;
    };
    if (!status)
        return fail(std::move(status));

    const auto behaviorFor = [&](Node node) -> CKBehavior * {
        const auto found = nodes.find(node.Value);
        return found == nodes.end() ? nullptr : found->second;
    };

    const auto parameterBeforeApply = [&](const ResolvedPort &port,
                                          CKObject *&value) -> Status {
        value = nullptr;
        const Edit::EditNode *node = edit.Find(port.Owner);
        if (!node)
            return Failure(Error::InvalidState,
                           "An Edit data port names an unknown Node.");
        if (node->Block || port.Appended)
            return {};

        CKBehavior *behavior = behaviorFor(port.Owner);
        if (!behavior)
            return Failure(Error::InvalidGraphLocality,
                           "An Edit data port left the target graph.",
                           CKERR_INVALIDOBJECT);
        SlotInfo slot;
        Status result = m_Runtime.Resolve(behavior, port.Selector, slot);
        if (!result)
            return result;
        switch (slot.Kind) {
        case SlotKind::InputParameter:
            value = behavior->GetInputParameter(slot.NativeIndex);
            break;
        case SlotKind::OutputParameter:
            value = behavior->GetOutputParameter(slot.NativeIndex);
            break;
        case SlotKind::Setting:
        case SlotKind::Local:
            value = behavior->GetLocalParameter(slot.NativeIndex);
            break;
        case SlotKind::Target:
            value = behavior->GetTargetParameter();
            break;
        default:
            break;
        }
        return value
            ? Status{}
            : Failure(Error::GraphChanged,
                      "An Edit data port disappeared before Apply.",
                      CKERR_INVALIDOBJECT);
    };

    CKParameterManager *manager = m_Context->GetParameterManager();
    const auto compatible = [&](CKGUID destination, CKGUID source,
                                const char *relation) -> Status {
        if (!destination.IsValid() || !source.IsValid())
            return {};
        if (!manager)
            return Failure(Error::ParameterTypeUnavailable,
                           "Virtools parameter types are unavailable.");
        return Parameter::Compatible(manager, destination, source)
            ? Status{}
            : Failure(Error::TypeMismatch,
                      std::string("Virtools rejected the ") + relation +
                          " parameter types.");
    };
    const auto selectorType = [&](const ResolvedPort &port) {
        return compatible(port.Slot.Type, port.Selector.ExpectedType,
                          "selected");
    };
    for (const CheckedBind &bind : checked.Binds) {
        status = selectorType(bind.Target);
        if (status && bind.Kind != BindKind::Literal)
            status = selectorType(bind.Source);
        if (status) {
            const CKGUID source = bind.Kind == BindKind::Literal
                ? bind.Literal.Type() : bind.Source.Slot.Type;
            status = compatible(bind.Target.Slot.Type, source, "Bind");
        }
        if (!status)
            return fail(std::move(status));
    }
    for (const CheckedPush &push : checked.Pushes) {
        status = selectorType(push.Source);
        if (status)
            status = selectorType(push.Destination);
        if (status)
            status = compatible(push.Destination.Slot.Type,
                                push.Source.Slot.Type, "Push");
        if (!status)
            return fail(std::move(status));
    }

    std::unordered_map<ParameterId, CKObject *> nativeParameters;
    const auto parameterId = [&](const ResolvedPort &port,
                                 ParameterId &id) -> Status {
        CKObject *parameter = nullptr;
        Status result = parameterBeforeApply(port, parameter);
        if (!result)
            return result;
        if (!parameter) {
            id = PlanParameter(port);
            return {};
        }
        id = NativeParameter(parameter);
        nativeParameters[id] = parameter;
        return {};
    };

    std::unordered_map<ParameterId, std::optional<ParameterId>> shared;
    std::vector<std::pair<ParameterId, ParameterId>> sharedEdges;
    for (const CheckedBind &bind : checked.Binds) {
        ParameterId target = 0;
        status = parameterId(bind.Target, target);
        if (!status)
            return fail(std::move(status));
        if (bind.Kind != BindKind::Shared) {
            shared[target] = std::nullopt;
            continue;
        }
        ParameterId source = 0;
        status = parameterId(bind.Source, source);
        if (!status)
            return fail(std::move(status));
        shared[target] = source;
        sharedEdges.emplace_back(target, source);
    }
    const auto sharedSource = [&](ParameterId input,
                                  std::optional<ParameterId> &source) -> Status {
        const auto replacement = shared.find(input);
        if (replacement != shared.end()) {
            source = replacement->second;
            return {};
        }
        const auto found = nativeParameters.find(input);
        auto *native = found == nativeParameters.end()
            ? nullptr : CKParameterIn::Cast(found->second);
        CKParameterIn *next = native ? native->GetSharedSource() : nullptr;
        if (!next) {
            source.reset();
            return {};
        }
        CKObject *live = m_Context->GetObject(next->GetID());
        if (live != next || next->IsToBeDeleted())
            return Failure(Error::GraphChanged,
                           "A shared Pin source disappeared before Apply.",
                           CKERR_INVALIDOBJECT);
        const ParameterId id = NativeParameter(next);
        nativeParameters[id] = next;
        source = id;
        return {};
    };
    for (const auto &[target, source] : sharedEdges) {
        ParameterId current = source;
        std::unordered_set<ParameterId> seen;
        while (seen.insert(current).second) {
            if (current == target)
                return fail(Failure(
                    Error::SharedSourceCycle,
                    "The final shared-source graph contains a cycle."));
            std::optional<ParameterId> next;
            status = sharedSource(current, next);
            if (!status)
                return fail(std::move(status));
            if (!next)
                break;
            current = *next;
        }
    }

    std::unordered_map<ParameterId, std::vector<ParameterId>> pushes;
    std::vector<std::pair<ParameterId, ParameterId>> pushEdges;
    for (const CheckedPush &push : checked.Pushes) {
        ParameterId source = 0;
        ParameterId destination = 0;
        status = parameterId(push.Source, source);
        if (status)
            status = parameterId(push.Destination, destination);
        if (!status)
            return fail(std::move(status));
        auto *nativeSource = source & kPlannedParameter
            ? nullptr : CKParameterOut::Cast(nativeParameters[source]);
        auto *nativeDestination = destination & kPlannedParameter
            ? nullptr : CKParameter::Cast(nativeParameters[destination]);
        if (nativeSource && nativeDestination &&
            ContainsDestination(nativeSource, nativeDestination)) {
            return fail(Failure(
                Error::InvalidState,
                "The Pout already has this destination."));
        }
        pushes[source].push_back(destination);
        pushEdges.emplace_back(source, destination);
    }
    const auto pushDestinations = [&](ParameterId source,
                                      std::vector<ParameterId> &destinations)
        -> Status {
        const auto added = pushes.find(source);
        if (added != pushes.end())
            destinations.insert(destinations.end(), added->second.begin(),
                                added->second.end());
        const auto found = nativeParameters.find(source);
        auto *native = found == nativeParameters.end()
            ? nullptr : CKParameterOut::Cast(found->second);
        if (!native)
            return {};
        for (int index = 0; index < native->GetDestinationCount(); ++index) {
            CKParameter *destination = native->GetDestination(index);
            if (!destination)
                return Failure(Error::GraphChanged,
                               "A Pout destination disappeared before Apply.",
                               CKERR_INVALIDOBJECT);
            CKObject *live = m_Context->GetObject(destination->GetID());
            if (live != destination || destination->IsToBeDeleted())
                return Failure(Error::GraphChanged,
                               "A Pout destination disappeared before Apply.",
                               CKERR_INVALIDOBJECT);
            const ParameterId id = NativeParameter(destination);
            nativeParameters[id] = destination;
            destinations.push_back(id);
        }
        return {};
    };
    for (const auto &[source, destination] : pushEdges) {
        std::vector<ParameterId> pending{destination};
        std::unordered_set<ParameterId> seen;
        while (!pending.empty()) {
            const ParameterId current = pending.back();
            pending.pop_back();
            if (current == source)
                return fail(Failure(
                    Error::PushCycle,
                    "The final Pout destination graph contains a cycle."));
            if (!seen.insert(current).second)
                continue;
            std::vector<ParameterId> destinations;
            status = pushDestinations(current, destinations);
            if (!status)
                return fail(std::move(status));
            pending.insert(pending.end(), destinations.begin(),
                           destinations.end());
        }
    }

    // Added Nodes enter their complete Runtime lifecycle before any control
    // path can reach them.
    for (const Edit::EditNode &node : edit.m_Nodes) {
        if (!node.Block)
            continue;
        AttachResult added = m_Runtime.AddToGraph(graph, *node.Block);
        if (!added || !added.Block)
            return fail(added.Detail);
        nodes.emplace(node.Handle.Value, added.Block);
        patch->Nodes.push_back(Capture(added.Block));
    }

    std::unordered_map<std::uint32_t, CKBehavior *> tapNodes;
    for (const CheckedTap &tap : checked.Taps) {
        AttachResult added = m_Runtime.AddToGraph(
            graph, HookBlock::Make(tap.Callback, 1, 0));
        if (!added || !added.Block)
            return fail(added.Detail);
        tapNodes.emplace(tap.Ordinal, added.Block);
        patch->Nodes.push_back(Capture(added.Block));
    }

    std::unordered_set<CKBehavior *> changed;
    for (const InterfacePort &item : edit.m_Interface) {
        if (item.InBlockSpec)
            continue;
        const auto owner = nodes.find(item.Owner.Value);
        CKBehavior *behavior = owner == nodes.end() ? nullptr : owner->second;
        if (!behavior)
            return fail(Failure(Error::InvalidState,
                                "A dynamic interface Node is unavailable."));

        CKObject *created = nullptr;
        switch (item.Slot.Kind) {
        case SlotKind::Input:
            created = behavior->CreateInput(
                const_cast<CKSTRING>(item.Slot.Name.c_str()));
            break;
        case SlotKind::Output:
            created = behavior->CreateOutput(
                const_cast<CKSTRING>(item.Slot.Name.c_str()));
            break;
        case SlotKind::InputParameter:
            created = behavior->CreateInputParameter(
                const_cast<CKSTRING>(item.Slot.Name.c_str()), item.Slot.Type);
            break;
        case SlotKind::OutputParameter:
            created = behavior->CreateOutputParameter(
                const_cast<CKSTRING>(item.Slot.Name.c_str()), item.Slot.Type);
            break;
        default:
            return fail(Failure(Error::InterfaceUnsupported,
                                "This dynamic interface kind is not supported."));
        }
        if (!created)
            return fail(Failure(Error::CreateFailed,
                                "Virtools failed to append a Behavior port.",
                                CKERR_OUTOFMEMORY));
        patch->Ports.push_back(
            {Capture(behavior), Capture(created), item.Slot.Kind});
        changed.insert(behavior);
    }

    for (CKBehavior *behavior : changed) {
        const int result = behavior->CallCallbackFunction(CKM_BEHAVIOREDITED);
        if (result != CK_OK)
            return fail(Failure(Error::CallbackFailed,
                                "A dynamic interface EDITED callback failed.",
                                result));
    }

    const auto liveSlot = [&](const ResolvedPort &port, SlotInfo &slot) {
        CKBehavior *behavior = behaviorFor(port.Owner);
        if (!behavior)
            return Failure(Error::InvalidState,
                           "An Edit Node disappeared during Apply.",
                           CKERR_INVALIDOBJECT);
        return m_Runtime.Resolve(behavior, port.Selector, slot);
    };

    const auto control = [&](const ResolvedPort &port,
                             CKBehaviorIO *&io) -> Status {
        io = nullptr;
        CKBehavior *behavior = behaviorFor(port.Owner);
        SlotInfo slot;
        Status result = liveSlot(port, slot);
        if (!result)
            return result;
        io = slot.Kind == SlotKind::Input
            ? behavior->GetInput(slot.NativeIndex)
            : slot.Kind == SlotKind::Output
                ? behavior->GetOutput(slot.NativeIndex) : nullptr;
        return io ? Status{}
                  : Failure(Error::GraphChanged,
                            "A control port disappeared during Apply.",
                            CKERR_INVALIDOBJECT);
    };

    const auto parameter = [&](const ResolvedPort &port,
                               CKObject *&value) -> Status {
        value = nullptr;
        CKBehavior *behavior = behaviorFor(port.Owner);
        SlotInfo slot;
        Status result = liveSlot(port, slot);
        if (!result)
            return result;
        switch (slot.Kind) {
        case SlotKind::InputParameter:
            value = behavior->GetInputParameter(slot.NativeIndex);
            break;
        case SlotKind::OutputParameter:
            value = behavior->GetOutputParameter(slot.NativeIndex);
            break;
        case SlotKind::Setting:
        case SlotKind::Local:
            value = behavior->GetLocalParameter(slot.NativeIndex);
            break;
        case SlotKind::Target:
            value = behavior->GetTargetParameter();
            break;
        default:
            break;
        }
        return value ? Status{}
                     : Failure(Error::GraphChanged,
                               "A data port disappeared during Apply.",
                               CKERR_INVALIDOBJECT);
    };

    const auto addLink = [&](CKBehaviorIO *source, CKBehaviorIO *sink,
                             int delay) -> Status {
        auto *link = static_cast<CKBehaviorLink *>(m_Context->CreateObject(
            CKCID_BEHAVIORLINK, nullptr, CK_OBJECTCREATION_DYNAMIC));
        if (!link)
            return Failure(Error::CreateFailed,
                           "Virtools failed to create a Behavior Link.",
                           CKERR_OUTOFMEMORY);
        CKERROR error = link->SetInBehaviorIO(source);
        if (error == CK_OK)
            error = link->SetOutBehaviorIO(sink);
        if (error == CK_OK) {
            link->SetInitialActivationDelay(delay);
            link->SetActivationDelay(delay);
            error = graph->AddSubBehaviorLink(link);
        }
        if (error != CK_OK) {
            m_Context->DestroyObject(link);
            return Failure(Error::GraphChanged,
                           "Virtools rejected a Behavior Link.", error);
        }
        patch->Links.push_back({Capture(link)});
        return {};
    };

    for (const CheckedFlow &flow : checked.Flows) {
        CKBehaviorIO *source = nullptr;
        CKBehaviorIO *sink = nullptr;
        status = control(flow.Source, source);
        if (status)
            status = control(flow.Sink, sink);
        if (status)
            status = addLink(source, sink, flow.Delay);
        if (!status)
            return fail(std::move(status));
    }

    for (const CheckedBind &bind : checked.Binds) {
        CKObject *targetParameter = nullptr;
        status = parameter(bind.Target, targetParameter);
        auto *target = status ? CKParameterIn::Cast(targetParameter) : nullptr;
        if (!status || !target)
            return fail(status ? Failure(Error::GraphChanged,
                                         "A Bind destination is not a Pin.")
                               : std::move(status));

        Patch::Data::Binding change;
        change.Input = Capture(target);
        change.PreviousDirect = Capture(target->GetDirectSource());
        change.PreviousShared = Capture(target->GetSharedSource());

        CKERROR error = CK_OK;
        if (bind.Kind == BindKind::Direct) {
            CKObject *sourceObject = nullptr;
            status = parameter(bind.Source, sourceObject);
            CKParameter *source = status
                ? CKParameter::Cast(sourceObject) : nullptr;
            if (status && !source)
                status = Failure(Error::TypeMismatch,
                                 "A direct Bind source is not a stored parameter.");
            if (status && !Parameter::Compatible(
                    m_Context->GetParameterManager(), target->GetGUID(),
                    source->GetGUID())) {
                status = Failure(Error::TypeMismatch,
                                 "Virtools rejected the direct Bind type.");
            }
            if (status)
                error = target->SetDirectSource(source);
            change.InstalledDirect = Capture(source);
        } else if (bind.Kind == BindKind::Shared) {
            CKObject *sourceParameter = nullptr;
            status = parameter(bind.Source, sourceParameter);
            auto *source = status ? CKParameterIn::Cast(sourceParameter) : nullptr;
            if (status && !source)
                status = Failure(Error::TypeMismatch,
                                 "A shared Bind source is not a Pin.");
            if (status)
                error = target->ShareSourceWith(source);
            change.InstalledShared = Capture(source);
        } else {
            std::ostringstream name;
            name << "__BML_Edit_" << graph->GetID() << '_'
                 << bind.Ordinal;
            CKParameterLocal *literal = m_Context->CreateCKParameterLocal(
                const_cast<CKSTRING>(name.str().c_str()),
                target->GetGUID(), TRUE);
            if (!literal)
                status = Failure(Error::CreateFailed,
                                 "Virtools failed to create a Bind value.",
                                 CKERR_OUTOFMEMORY);
            if (status)
                status = Parameter::Write(m_Context, literal, bind.Literal);
            if (status)
                error = target->SetDirectSource(literal);
            if (!status || error != CK_OK) {
                if (literal)
                    m_Context->DestroyObject(literal);
                return fail(status ? Failure(Error::TypeMismatch,
                                             "Virtools rejected a Bind value.",
                                             error)
                                   : std::move(status));
            }
            change.Literal = Capture(literal);
            change.InstalledDirect = Capture(literal);
        }
        if (!status)
            return fail(std::move(status));
        if (error != CK_OK)
            return fail(Failure(Error::TypeMismatch,
                                "Virtools rejected a Bind relation.", error));
        patch->Binds.push_back(std::move(change));
    }

    for (const CheckedPush &push : checked.Pushes) {
        CKObject *sourceParameter = nullptr;
        CKObject *destinationObject = nullptr;
        status = parameter(push.Source, sourceParameter);
        if (status)
            status = parameter(push.Destination, destinationObject);
        auto *source = status ? CKParameterOut::Cast(sourceParameter) : nullptr;
        auto *destination = status
            ? CKParameter::Cast(destinationObject) : nullptr;
        if (!status || !source || !destination)
            return fail(status ? Failure(Error::GraphChanged,
                                         "Push requires a Pout and a stored parameter.")
                               : std::move(status));
        const CKERROR error = source->AddDestination(destination, TRUE);
        if (error != CK_OK)
            return fail(Failure(Error::TypeMismatch,
                                "Virtools rejected a Push relation.", error));
        patch->Pushes.push_back({Capture(source), Capture(destination)});
    }

    for (const CheckedTap &tap : checked.Taps) {
        CKBehaviorIO *source = nullptr;
        status = control(tap.Source, source);
        const auto observer = tapNodes.find(tap.Ordinal);
        CKBehavior *block = observer == tapNodes.end() ? nullptr : observer->second;
        if (status && (!block || !block->GetInput(0)))
            status = Failure(Error::GraphChanged,
                             "A Tap observer disappeared during Apply.");
        if (status)
            status = addLink(source, block->GetInput(0), 0);
        if (!status)
            return fail(std::move(status));
    }

    PatchLayer layer;
    layer.Patch = edit.Key();
    for (const CheckedTap &tap : checked.Taps) {
        CKBehavior *owner = behaviorFor(tap.Source.Owner);
        SlotInfo slot;
        status = liveSlot(tap.Source, slot);
        if (!status)
            return fail(std::move(status));
        GraphEndpoint source{
            static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(owner->GetID())),
            slot.Kind, slot.NativeIndex};
        auto found = std::find_if(
            layer.Outs.begin(), layer.Outs.end(),
            [&](const OutTaps &candidate) {
                return candidate.Out == source;
            });
        if (found == layer.Outs.end()) {
            layer.Outs.push_back({source, {}});
            found = std::prev(layer.Outs.end());
        }
        found->Taps.push_back({tap.Ordinal, HashTap(source, tap.Ordinal)});
    }
    status = m_Topology[graphId].Set(std::move(layer));
    if (!status)
        return fail(std::move(status));

    const int edited = graph->CallCallbackFunction(CKM_BEHAVIOREDITED);
    if (edited != CK_OK)
        return fail(Failure(Error::CallbackFailed,
                            "The graph EDITED callback failed.", edited));

    for (const Patch::Data::Link &item : patch->Links) {
        CKBehaviorLink *link = Resolve<CKBehaviorLink>(
            m_Context, item.Value, CKCID_BEHAVIORLINK);
        if (!link || link->GetInBehaviorIO() == nullptr ||
            link->GetOutBehaviorIO() == nullptr) {
            return fail(Failure(Error::GraphChanged,
                                "The graph changed while the Edit was published."));
        }
    }

    patch->Active = true;
    m_Active[graphId].insert(edit.Key());
    out.m_Data = std::move(patch);
    return {};
}

Status CKEdit::Undo(Patch::Data &patch, bool notify) {
    Status first;
    const auto remember = [&](Status status) {
        if (!status && first)
            first = std::move(status);
    };
    auto *graph = Resolve<CKBehavior>(m_Context, patch.Graph, CKCID_BEHAVIOR);
    const std::uint64_t graphId = patch.Graph.Id
        ? static_cast<std::uint32_t>(patch.Graph.Id) : 0;

    if (graphId) {
        const auto topology = m_Topology.find(graphId);
        if (topology != m_Topology.end())
            topology->second.Remove(patch.Key);
        const auto active = m_Active.find(graphId);
        if (active != m_Active.end())
            active->second.erase(patch.Key);
    }

    for (auto item = patch.Pushes.rbegin(); item != patch.Pushes.rend(); ++item) {
        auto *source = Resolve<CKParameterOut>(
            m_Context, item->Source, CKCID_PARAMETEROUT);
        auto *target = Resolve<CKParameter>(
            m_Context, item->Target, CKCID_PARAMETER);
        if (source && target && ContainsDestination(source, target))
            source->RemoveDestination(target);
    }

    for (auto item = patch.Binds.rbegin(); item != patch.Binds.rend(); ++item) {
        auto *input = Resolve<CKParameterIn>(
            m_Context, item->Input, CKCID_PARAMETERIN);
        if (!input)
            continue;
        CKParameter *installedDirect = Resolve<CKParameter>(
            m_Context, item->InstalledDirect, CKCID_PARAMETER);
        CKParameterIn *installedShared = Resolve<CKParameterIn>(
            m_Context, item->InstalledShared, CKCID_PARAMETERIN);
        const bool unchanged = item->InstalledShared.Id
            ? input->GetSharedSource() == installedShared
            : input->GetDirectSource() == installedDirect;
        if (!unchanged) {
            remember(Failure(Error::GraphChanged,
                             "A Bind source changed before the Patch closed."));
            continue;
        }
        CKParameterIn *previousShared = Resolve<CKParameterIn>(
            m_Context, item->PreviousShared, CKCID_PARAMETERIN);
        CKParameter *previousDirect = Resolve<CKParameter>(
            m_Context, item->PreviousDirect, CKCID_PARAMETER);
        const CKERROR error = item->PreviousShared.Id
            ? input->ShareSourceWith(previousShared)
            : input->SetDirectSource(previousDirect);
        if (error != CK_OK)
            remember(Failure(Error::GraphChanged,
                             "Virtools rejected a restored Bind source.", error));
    }

    for (auto item = patch.Links.rbegin(); item != patch.Links.rend(); ++item) {
        CKBehaviorLink *link = Resolve<CKBehaviorLink>(
            m_Context, item->Value, CKCID_BEHAVIORLINK);
        if (!link)
            continue;
        if (graph)
            graph->RemoveSubBehaviorLink(link);
        m_Context->DestroyObject(link);
    }

    for (auto item = patch.Ports.rbegin(); item != patch.Ports.rend(); ++item) {
        CKBehavior *behavior = Resolve<CKBehavior>(
            m_Context, item->Behavior, CKCID_BEHAVIOR);
        CKObject *port = item->Port.Id
            ? m_Context->GetObject(item->Port.Id) : nullptr;
        if (!behavior || port != item->Port.Address ||
            port->IsToBeDeleted())
            continue;
        CKObject *removed = nullptr;
        switch (item->Kind) {
        case SlotKind::Input: {
            auto *io = static_cast<CKBehaviorIO *>(port);
            const int index = behavior->GetInputPosition(io);
            removed = index >= 0 ? behavior->RemoveInput(index) : nullptr;
            break;
        }
        case SlotKind::Output: {
            auto *io = static_cast<CKBehaviorIO *>(port);
            const int index = behavior->GetOutputPosition(io);
            removed = index >= 0 ? behavior->RemoveOutput(index) : nullptr;
            break;
        }
        case SlotKind::InputParameter: {
            auto *parameter = static_cast<CKParameterIn *>(port);
            const int index = behavior->GetInputParameterPosition(parameter);
            removed = index >= 0 ? behavior->RemoveInputParameter(index) : nullptr;
            break;
        }
        case SlotKind::OutputParameter: {
            auto *parameter = static_cast<CKParameterOut *>(port);
            const int index = behavior->GetOutputParameterPosition(parameter);
            removed = index >= 0 ? behavior->RemoveOutputParameter(index) : nullptr;
            break;
        }
        default:
            break;
        }
        if (removed == port)
            m_Context->DestroyObject(removed);
    }

    for (const Patch::Data::Binding &item : patch.Binds) {
        CKParameterLocal *literal = Resolve<CKParameterLocal>(
            m_Context, item.Literal, CKCID_PARAMETERLOCAL);
        if (literal)
            m_Context->DestroyObject(literal);
    }

    for (auto item = patch.Nodes.rbegin(); item != patch.Nodes.rend(); ++item) {
        CKBehavior *node = Resolve<CKBehavior>(
            m_Context, *item, CKCID_BEHAVIOR);
        if (node)
            remember(m_Runtime.Close(node));
    }

    if (notify && graph) {
        const int result = graph->CallCallbackFunction(CKM_BEHAVIOREDITED);
        if (result != CK_OK)
            remember(Failure(Error::CallbackFailed,
                             "The graph EDITED callback failed while the Patch closed.",
                             result));
    }
    patch.Active = false;
    return first;
}

Status CKEdit::Close(Patch &patch) {
    Status status = Ready();
    if (!status)
        return status;
    if (!patch.m_Data || !patch.m_Data->Active) {
        patch.m_Data.reset();
        return {};
    }
    if (patch.m_Data->Editor != this)
        return Failure(Error::InvalidState,
                       "The Patch belongs to another Behavior editor.");
    status = Undo(*patch.m_Data, true);
    patch.m_Data.reset();
    return status;
}

std::uint64_t CKEdit::TopologyFingerprint(CKBehavior *graph) const {
    if (!graph)
        return 0;
    const auto found = m_Topology.find(
        static_cast<std::uint32_t>(graph->GetID()));
    return found == m_Topology.end() ? 0 : found->second.Fingerprint();
}

} // namespace BML::Behavior

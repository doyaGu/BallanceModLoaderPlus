#include "Behavior/Graph.h"

#include <bit>
#include <cstring>
#include <limits>
#include <unordered_map>

#include "Behavior/Runtime.h"

namespace BML::Behavior {
namespace {

constexpr std::uint64_t kHashOffset = 1469598103934665603ull;
constexpr std::uint64_t kHashPrime = 1099511628211ull;

void HashBytes(std::uint64_t &hash, const void *data, std::size_t size) {
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= kHashPrime;
    }
}

template <typename T>
void Hash(std::uint64_t &hash, const T &value) {
    HashBytes(hash, &value, sizeof(value));
}

void HashText(std::uint64_t &hash, const char *text) {
    const std::size_t size = text ? std::strlen(text) : 0;
    Hash(hash, size);
    if (size)
        HashBytes(hash, text, size);
}

Status Failure(Error error, std::string message,
               Phase phase = Phase::None) {
    Status status{error, CKERR_INVALIDOBJECT, CKBR_PARAMETERERROR,
                  std::move(message)};
    status.Details.Stage = phase;
    return status;
}

int Occurrence(CKBehavior *behavior, CKBehaviorIO *io, bool input,
               int index) {
    const char *name = io && io->GetName() ? io->GetName() : "";
    int occurrence = 0;
    for (int current = 0; current < index; ++current) {
        CKBehaviorIO *candidate = input ? behavior->GetInput(current)
                                        : behavior->GetOutput(current);
        const char *candidateName = candidate && candidate->GetName()
            ? candidate->GetName() : "";
        if (std::strcmp(name, candidateName) == 0)
            ++occurrence;
    }
    return occurrence;
}

class CKGraphSource final : public GraphSource {
public:
    CKGraphSource(CKContext *context, Runtime &runtime,
                  std::function<ObjectRef(const void *)> issueObjectRef)
        : m_Context(context), m_Runtime(runtime),
          m_IssueObjectRef(std::move(issueObjectRef)) {}

    Status Refer(void *object, NativeRef &out) override {
        out = {};
        CKObject *native = static_cast<CKObject *>(object);
        if (!Valid(native))
            return Failure(Error::InvalidState,
                           "The inspected Virtools object is stale.");
        out = {static_cast<std::uint64_t>(
                   static_cast<std::uint32_t>(native->GetID())), native};
        return {};
    }

    Status Read(const NativeRef &root, GraphView view,
                GraphModel &out) override {
        out = {};
        CKBehavior *graph = ResolveBehavior(root);
        if (!graph)
            return Failure(Error::InvalidState,
                           "The inspected Behavior graph is stale.");

        std::uint64_t fingerprint = 0;
        Status status = Fingerprint(graph, fingerprint);
        if (!status)
            return status;

        ObjectRef rootObject;
        status = Issue(graph, rootObject);
        if (!status)
            return status;
        out.View = view;
        out.Root = rootObject;
        out.Fingerprint = fingerprint;
        Generation &state = m_Generations[graph->GetID()];
        if (state.Address != graph || state.Fingerprint != fingerprint) {
            state.Address = graph;
            state.Fingerprint = fingerprint;
            if (state.Value != (std::numeric_limits<std::uint64_t>::max)())
                ++state.Value;
        }
        out.Generation = state.Value;

        out.Nodes.reserve(static_cast<std::size_t>(
            graph->GetSubBehaviorCount()) + 1u);
        status = AddNode(graph, nullptr, out);
        if (!status)
            return status;
        for (int index = 0; index < graph->GetSubBehaviorCount(); ++index) {
            CKBehavior *child = graph->GetSubBehavior(index);
            if (!Valid(child))
                return Failure(Error::InvalidState,
                               "A graph node disappeared during inspection.");
            status = AddNode(child, graph, out);
            if (!status)
                return status;
        }

        out.Links.reserve(static_cast<std::size_t>(
            graph->GetSubBehaviorLinkCount()));
        for (int index = 0; index < graph->GetSubBehaviorLinkCount(); ++index) {
            CKBehaviorLink *link = graph->GetSubBehaviorLink(index);
            if (!Valid(link))
                return Failure(Error::InvalidState,
                               "A graph Link disappeared during inspection.");
            CKBehaviorIO *sourceIo = link->GetInBehaviorIO();
            CKBehaviorIO *targetIo = link->GetOutBehaviorIO();
            CKBehavior *source = sourceIo ? sourceIo->GetOwner() : nullptr;
            CKBehavior *target = targetIo ? targetIo->GetOwner() : nullptr;
            if (!Valid(source) || !Valid(target))
                return Failure(Error::InvalidState,
                               "A graph Link has a missing endpoint.");
            GraphLink record;
            record.Id = static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(link->GetID()));
            status = Issue(link, record.Object);
            if (!status)
                return status;
            record.Source.Node = static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(source->GetID()));
            record.Source.Index = source->GetOutputPosition(sourceIo);
            record.Source.Kind = SlotKind::Output;
            if (record.Source.Index < 0) {
                record.Source.Index = source->GetInputPosition(sourceIo);
                record.Source.Kind = SlotKind::Input;
            }
            record.Target.Node = static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(target->GetID()));
            record.Target.Index = target->GetInputPosition(targetIo);
            record.Target.Kind = SlotKind::Input;
            if (record.Target.Index < 0) {
                record.Target.Index = target->GetOutputPosition(targetIo);
                record.Target.Kind = SlotKind::Output;
            }
            if (record.Source.Index < 0 || record.Target.Index < 0)
                return Failure(Error::InvalidState,
                               "A graph Link endpoint is not owned by its Behavior.");
            record.InitialDelay = link->GetInitialActivationDelay();
            record.RemainingDelay = link->GetActivationDelay();
            // CK2.1 exposes the residual counter but not delayed-list
            // membership. Residual delay alone cannot prove pending state.
            record.Pending = Truth::Unknown;
            out.Links.push_back(std::move(record));
        }
        return {};
    }

    Status ReadLayout(const NativeRef &node, Layout &out) override {
        CKBehavior *behavior = ResolveBehavior(node);
        if (!behavior)
            return Failure(Error::InvalidState,
                           "The inspected Behavior node is stale.");
        out = m_Runtime.Describe(behavior);
        // A script graph created by an author has no prototype GUID, but its
        // boundary and data layout are still native CKBehavior state.  A
        // prototype identifies a reusable BB; it is not a prerequisite for
        // inspecting a live graph.
        return {};
    }

    Status ReadValue(const NativeRef &node, const Slot &slot,
                     ReadMode mode, GraphValue &out) override {
        out = {};
        if (mode != ReadMode::NonForcing)
            return Failure(Error::ObserverUnavailable,
                           "Only non-forcing Behavior value reads are portable.");
        CKBehavior *behavior = ResolveBehavior(node);
        if (!behavior)
            return Failure(Error::InvalidState,
                           "The inspected Behavior node is stale.");

        SlotInfo resolved;
        Status status = m_Runtime.Resolve(behavior, slot, resolved);
        if (!status)
            return status;

        CKParameter *parameter = nullptr;
        out.Relation = ValueRelation::Stored;
        switch (resolved.Kind) {
        case SlotKind::InputParameter:
        case SlotKind::Target: {
            CKParameterIn *input = resolved.Kind == SlotKind::Target
                ? behavior->GetTargetParameter()
                : behavior->GetInputParameter(resolved.NativeIndex);
            if (!input) {
                out.State = ValueState::Indeterminate;
                return {};
            }
            if (input->GetSharedSource())
                out.Relation = ValueRelation::Shared;
            else if (input->GetDirectSource())
                out.Relation = ValueRelation::Direct;
            parameter = input->GetRealSource();
            break;
        }
        case SlotKind::OutputParameter:
            parameter = behavior->GetOutputParameter(resolved.NativeIndex);
            break;
        case SlotKind::Setting:
        case SlotKind::Local:
            parameter = behavior->GetLocalParameter(resolved.NativeIndex);
            break;
        default:
            return Failure(Error::SlotNotFound,
                           "Only Target, Pin, Pout, Setting, or Local values can be read.");
        }

        if (!parameter) {
            out.State = ValueState::Indeterminate;
            return {};
        }
        if (CKParameterOperation::Cast(parameter->GetOwner())) {
            out.State = ValueState::Indeterminate;
            out.Relation = ValueRelation::Operation;
            out.Type = parameter->GetGUID();
            return {};
        }

        const Parameter::Type type = Parameter::Describe(
            m_Context ? m_Context->GetParameterManager() : nullptr,
            parameter->GetGUID());
        out.Type = type.Guid;
        out.Form = type.ValueForm;
        if (!type.Valid || !type.Supported()) {
            out.State = ValueState::Unsupported;
            return {};
        }
        out.State = ValueState::Available;
        return ReadParameter(parameter, type, out);
    }

    Status GraphFingerprint(const NativeRef &root, GraphView,
                            std::uint64_t &out) override {
        CKBehavior *graph = ResolveBehavior(root);
        return graph ? Fingerprint(graph, out)
                     : Failure(Error::InvalidState,
                               "The watched Behavior graph is stale.");
    }

    Status LayoutFingerprint(const NativeRef &node,
                             std::uint64_t &out) override {
        CKBehavior *behavior = ResolveBehavior(node);
        if (!behavior)
            return Failure(Error::InvalidState,
                           "The watched Behavior node is stale.");
        out = kHashOffset;
        const CKGUID prototype = Prototype(behavior);
        Hash(out, prototype.d1);
        Hash(out, prototype.d2);
        Hash(out, behavior->GetCompatibleClassID());
        Hash(out, behavior->GetInputCount());
        Hash(out, behavior->GetOutputCount());
        Hash(out, behavior->GetInputParameterCount());
        Hash(out, behavior->GetOutputParameterCount());
        Hash(out, behavior->GetLocalParameterCount());
        for (int index = 0; index < behavior->GetInputCount(); ++index) {
            CKBehaviorIO *io = behavior->GetInput(index);
            HashText(out, io ? io->GetName() : nullptr);
        }
        for (int index = 0; index < behavior->GetOutputCount(); ++index) {
            CKBehaviorIO *io = behavior->GetOutput(index);
            HashText(out, io ? io->GetName() : nullptr);
        }
        for (int index = 0; index < behavior->GetInputParameterCount(); ++index) {
            CKParameterIn *parameter = behavior->GetInputParameter(index);
            HashText(out, parameter ? parameter->GetName() : nullptr);
            const CKGUID type = parameter ? parameter->GetGUID() : CKGUID();
            Hash(out, type.d1);
            Hash(out, type.d2);
        }
        for (int index = 0; index < behavior->GetOutputParameterCount(); ++index) {
            CKParameterOut *parameter = behavior->GetOutputParameter(index);
            HashText(out, parameter ? parameter->GetName() : nullptr);
            const CKGUID type = parameter ? parameter->GetGUID() : CKGUID();
            Hash(out, type.d1);
            Hash(out, type.d2);
        }
        for (int index = 0; index < behavior->GetLocalParameterCount(); ++index) {
            CKParameterLocal *parameter = behavior->GetLocalParameter(index);
            HashText(out, parameter ? parameter->GetName() : nullptr);
            const CKGUID type = parameter ? parameter->GetGUID() : CKGUID();
            Hash(out, type.d1);
            Hash(out, type.d2);
            const CKBOOL setting = behavior->IsLocalParameterSetting(index);
            Hash(out, setting);
        }
        return {};
    }

private:
    struct Generation {
        const void *Address = nullptr;
        std::uint64_t Fingerprint = 0;
        std::uint64_t Value = 0;
    };

    bool Valid(CKObject *object) const {
        return m_Context && object && object->GetCKContext() == m_Context &&
            !object->IsToBeDeleted() &&
            m_Context->GetObject(object->GetID()) == object;
    }

    CKBehavior *ResolveBehavior(const NativeRef &reference) const {
        if (!m_Context || !reference)
            return nullptr;
        CKObject *object = m_Context->GetObject(
            static_cast<CK_ID>(reference.Id));
        if (object != reference.Address || !Valid(object) ||
            !CKIsChildClassOf(object, CKCID_BEHAVIOR))
            return nullptr;
        return static_cast<CKBehavior *>(object);
    }

    Status Issue(CKObject *object, ObjectRef &out) const {
        out = {};
        if (!object)
            return {};
        if (!m_IssueObjectRef)
            return Failure(Error::Unavailable,
                           "Behavior graph ObjectRefs are unavailable.");
        out = m_IssueObjectRef(object);
        return !out.IsNull()
            ? Status{} : Failure(Error::Unavailable,
                                 "A live Behavior graph object could not be referenced.");
    }

    CKGUID Prototype(CKBehavior *behavior) const {
        if (!behavior)
            return CKGUID();
        CKBehaviorPrototype *prototype = behavior->GetPrototype();
        return prototype ? prototype->GetGuid() : CKGUID();
    }

    Status AddNode(CKBehavior *behavior, CKBehavior *parent,
                   GraphModel &out) const {
        GraphNode node;
        node.Id = static_cast<std::uint64_t>(
            static_cast<std::uint32_t>(behavior->GetID()));
        Status status = Issue(behavior, node.Object);
        if (!status)
            return status;
        node.Parent = parent ? static_cast<std::uint64_t>(
            static_cast<std::uint32_t>(parent->GetID())) : 0;
        node.Prototype = Prototype(behavior);
        node.Name = behavior->GetName() ? behavior->GetName() : "";
        node.Priority = behavior->GetPriority();
        node.Active = behavior->IsActive() != FALSE;
        node.Ports.reserve(static_cast<std::size_t>(
            behavior->GetInputCount() + behavior->GetOutputCount()));
        for (int index = 0; index < behavior->GetInputCount(); ++index) {
            CKBehaviorIO *io = behavior->GetInput(index);
            node.Ports.push_back({SlotKind::Input, index,
                                  Occurrence(behavior, io, true, index),
                                  io && io->GetName() ? io->GetName() : "",
                                  io && io->IsActive()});
        }
        for (int index = 0; index < behavior->GetOutputCount(); ++index) {
            CKBehaviorIO *io = behavior->GetOutput(index);
            node.Ports.push_back({SlotKind::Output, index,
                                  Occurrence(behavior, io, false, index),
                                  io && io->GetName() ? io->GetName() : "",
                                  io && io->IsActive()});
        }
        out.Nodes.push_back(std::move(node));
        return {};
    }

    Status Fingerprint(CKBehavior *graph, std::uint64_t &out) const {
        out = kHashOffset;
        Hash(out, graph->GetID());
        Hash(out, graph->GetSubBehaviorCount());
        Hash(out, graph->GetSubBehaviorLinkCount());
        for (int index = 0; index < graph->GetSubBehaviorCount(); ++index) {
            CKBehavior *child = graph->GetSubBehavior(index);
            if (!Valid(child))
                return Failure(Error::InvalidState,
                               "A graph node disappeared while its structure was read.");
            Hash(out, child->GetID());
            const CKGUID prototype = Prototype(child);
            Hash(out, prototype.d1);
            Hash(out, prototype.d2);
            HashText(out, child->GetName());
            Hash(out, child->GetPriority());
        }
        for (int index = 0; index < graph->GetSubBehaviorLinkCount(); ++index) {
            CKBehaviorLink *link = graph->GetSubBehaviorLink(index);
            if (!Valid(link))
                return Failure(Error::InvalidState,
                               "A graph Link disappeared while its structure was read.");
            CKBehaviorIO *sourceIo = link->GetInBehaviorIO();
            CKBehaviorIO *targetIo = link->GetOutBehaviorIO();
            CKBehavior *source = sourceIo ? sourceIo->GetOwner() : nullptr;
            CKBehavior *target = targetIo ? targetIo->GetOwner() : nullptr;
            if (!Valid(source) || !Valid(target))
                return Failure(Error::InvalidState,
                               "A graph Link has a missing endpoint.");
            Hash(out, link->GetID());
            Hash(out, source->GetID());
            int sourceIndex = source->GetOutputPosition(sourceIo);
            SlotKind sourceKind = SlotKind::Output;
            if (sourceIndex < 0) {
                sourceIndex = source->GetInputPosition(sourceIo);
                sourceKind = SlotKind::Input;
            }
            Hash(out, target->GetID());
            int targetIndex = target->GetInputPosition(targetIo);
            SlotKind targetKind = SlotKind::Input;
            if (targetIndex < 0) {
                targetIndex = target->GetOutputPosition(targetIo);
                targetKind = SlotKind::Output;
            }
            if (sourceIndex < 0 || targetIndex < 0)
                return Failure(Error::InvalidState,
                               "A graph Link endpoint is not owned by its Behavior.");
            Hash(out, sourceKind);
            Hash(out, sourceIndex);
            Hash(out, targetKind);
            Hash(out, targetIndex);
            Hash(out, link->GetInitialActivationDelay());
        }
        return {};
    }

    Status ReadParameter(CKParameter *parameter, const Parameter::Type &type,
                         GraphValue &out) const {
        if (type.ValueForm == Parameter::Form::Utf8) {
            const int size = parameter->GetStringValue(nullptr, FALSE);
            if (size < 0)
                return Failure(Error::PoutUnavailable,
                               "The Behavior string value could not be read.");
            std::vector<char> text(static_cast<std::size_t>(size) + 1u, '\0');
            if (size && parameter->GetStringValue(text.data(), FALSE) < 0)
                return Failure(Error::PoutUnavailable,
                               "The Behavior string value changed while it was read.");
            out.Data = std::string(text.data());
            return {};
        }
        if (type.ValueForm == Parameter::Form::Object) {
            CKObject *object = parameter->GetValueObject(FALSE);
            ObjectRef reference;
            Status status = Issue(object, reference);
            if (!status)
                return status;
            out.Data = reference;
            return {};
        }

        std::array<std::byte, sizeof(VxMatrix)> bytes{};
        if (type.Size < 0 || static_cast<std::size_t>(type.Size) > bytes.size() ||
            parameter->GetDataSize() != type.Size ||
            parameter->GetValue(bytes.data(), FALSE) != CK_OK)
            return Failure(Error::PoutUnavailable,
                           "The Behavior parameter value could not be read.");
        switch (type.ValueForm) {
        case Parameter::Form::Bool: {
            CKBOOL value = FALSE;
            std::memcpy(&value, bytes.data(), sizeof(value));
            out.Data = value != FALSE;
            break;
        }
        case Parameter::Form::Int32: {
            std::int32_t value = 0;
            std::memcpy(&value, bytes.data(), sizeof(value));
            out.Data = value;
            break;
        }
        case Parameter::Form::Float32: {
            float value = 0;
            std::memcpy(&value, bytes.data(), sizeof(value));
            out.Data = value;
            break;
        }
        case Parameter::Form::Vec2: {
            std::array<float, 2> value{};
            std::memcpy(value.data(), bytes.data(), sizeof(value));
            out.Data = value;
            break;
        }
        case Parameter::Form::Vec3:
        case Parameter::Form::Euler: {
            std::array<float, 3> value{};
            std::memcpy(value.data(), bytes.data(), sizeof(value));
            out.Data = value;
            break;
        }
        case Parameter::Form::Quaternion:
        case Parameter::Form::Rect:
        case Parameter::Form::Color: {
            std::array<float, 4> value{};
            std::memcpy(value.data(), bytes.data(), sizeof(value));
            out.Data = value;
            break;
        }
        case Parameter::Form::Box: {
            std::array<float, 6> value{};
            std::memcpy(value.data(), bytes.data(), sizeof(value));
            out.Data = value;
            break;
        }
        case Parameter::Form::Mat4: {
            std::array<float, 16> value{};
            std::memcpy(value.data(), bytes.data(), sizeof(value));
            out.Data = value;
            break;
        }
        default:
            out.State = ValueState::Unsupported;
            out.Data = std::monostate{};
            break;
        }
        return {};
    }

    CKContext *m_Context = nullptr;
    Runtime &m_Runtime;
    std::function<ObjectRef(const void *)> m_IssueObjectRef;
    std::unordered_map<CK_ID, Generation> m_Generations;
};

} // namespace

std::unique_ptr<GraphSource> MakeCKGraphSource(
    CKContext *context, Runtime &runtime,
    std::function<ObjectRef(const void *)> issueObjectRef) {
    return std::make_unique<CKGraphSource>(
        context, runtime, std::move(issueObjectRef));
}

} // namespace BML::Behavior

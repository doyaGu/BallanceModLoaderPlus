#include "Behavior/Graph.h"

#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <unordered_map>

#include "Behavior/Runtime.h"

namespace BML::Behavior::Internal {
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

        Status status = ReadGraph(graph, true, out);
        if (!status)
            return status;
        out.View = view;
        if (view == GraphView::Logical) {
            status = ApplyLogical(graph, out);
            if (!status)
                return status;
        }

        ObjectRef rootObject;
        status = Issue(graph, rootObject);
        if (!status)
            return status;
        out.Root = rootObject;
        out.Fingerprint = Fingerprint(out);
        Generation &state = m_Generations[graph->GetID()][ViewIndex(view)];
        if (state.Address != graph || state.Fingerprint != out.Fingerprint) {
            state.Address = graph;
            state.Fingerprint = out.Fingerprint;
            if (state.Value != (std::numeric_limits<std::uint64_t>::max)())
                ++state.Value;
        }
        out.Generation = state.Value;
        return {};
    }

    Status ReadLayout(const NativeRef &node, Layout &out) override {
        CKBehavior *behavior = ResolveBehavior(node);
        if (!behavior)
            return Failure(Error::InvalidState,
                           "The inspected Behavior node is stale.");
        out = m_Runtime.Describe(behavior);
        out.Generation = TrackLayout(behavior);
        // A script graph created by an author has no prototype GUID, but its
        // boundary and data layout are still native CKBehavior state.  A
        // prototype identifies a reusable BB; it is not a prerequisite for
        // inspecting a live graph.
        return {};
    }

    Status ReadValue(const NativeRef &node,
                     std::uint64_t layoutGeneration,
                     const Slot &slot,
                     ReadMode mode, GraphValue &out) override {
        out = {};
        if (mode != ReadMode::NonForcing)
            return Failure(Error::ObserverUnavailable,
                           "Only non-forcing Behavior value reads are portable.");
        CKBehavior *behavior = ResolveBehavior(node);
        if (!behavior)
            return Failure(Error::InvalidState,
                           "The inspected Behavior node is stale.");
        if (layoutGeneration && TrackLayout(behavior) != layoutGeneration)
            return Failure(Error::StaleLayout,
                           "The inspected port belongs to an older Behavior Layout.");

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

    Status GraphFingerprint(const NativeRef &root, GraphView view,
                            std::uint64_t &out) override {
        CKBehavior *graph = ResolveBehavior(root);
        if (!graph)
            return Failure(Error::InvalidState,
                           "The watched Behavior graph is stale.");
        Status status = ReadGraph(graph, false, m_FingerprintModel, false);
        if (status && view == GraphView::Logical)
            status = ApplyLogical(graph, m_FingerprintModel);
        if (status)
            out = Fingerprint(m_FingerprintModel);
        return status;
    }

    void SetLogicalGraph(const NativeRef &root,
                         LogicalGraph graph) override {
        if (!root)
            return;
        LogicalState &state = m_Logical[root.Id];
        if (state.Root != root)
            state = {};
        state.Root = root;

        const auto currentNode = [&](const NativeRef &node) {
            return std::find(graph.InfrastructureNodes.begin(),
                             graph.InfrastructureNodes.end(), node) !=
                   graph.InfrastructureNodes.end();
        };
        for (const NativeRef &node : state.Graph.InfrastructureNodes) {
            if (!currentNode(node) &&
                std::find(state.RetiredNodes.begin(), state.RetiredNodes.end(),
                          node) == state.RetiredNodes.end())
                state.RetiredNodes.push_back(node);
        }
        const auto currentLink = [&](const NativeRef &link) {
            return std::any_of(graph.Links.begin(), graph.Links.end(),
                               [&](const LogicalGraphLink &candidate) {
                                   return candidate.Object == link;
                               });
        };
        for (const LogicalGraphLink &link : state.Graph.Links) {
            if (!link.Logical && !currentLink(link.Object) &&
                std::find(state.RetiredLinks.begin(), state.RetiredLinks.end(),
                          link.Object) == state.RetiredLinks.end())
                state.RetiredLinks.push_back(link.Object);
        }
        state.Graph = std::move(graph);
        std::erase_if(state.RetiredNodes, currentNode);
        std::erase_if(state.RetiredLinks, currentLink);
    }

    Status LayoutFingerprint(const NativeRef &node,
                             std::uint64_t &out) override {
        CKBehavior *behavior = ResolveBehavior(node);
        if (!behavior)
            return Failure(Error::InvalidState,
                           "The watched Behavior node is stale.");
        out = TrackLayout(behavior);
        return {};
    }

private:
    Status FingerprintLayout(CKBehavior *behavior,
                             std::uint64_t &out) const {
        out = LayoutIdentity(behavior);
        return {};
    }

    std::uint64_t TrackLayout(CKBehavior *behavior) const {
        std::uint64_t fingerprint = 0;
        if (!FingerprintLayout(behavior, fingerprint))
            return 0;
        // Runtime generations also change at lifecycle callback boundaries,
        // including a layout that changes and returns to the same shape before
        // it can be observed. The native fingerprint additionally covers graph
        // edits and provider changes outside Runtime ownership.
        Hash(fingerprint, m_Runtime.LayoutGeneration(behavior));
        Generation &state = m_LayoutGenerations[behavior->GetID()];
        if (state.Address != behavior || state.Fingerprint != fingerprint) {
            state.Address = behavior;
            state.Fingerprint = fingerprint;
            if (state.Value != (std::numeric_limits<std::uint64_t>::max)())
                ++state.Value;
        }
        return state.Value;
    }

    struct LogicalState {
        NativeRef Root;
        LogicalGraph Graph;
        std::vector<NativeRef> RetiredNodes;
        std::vector<NativeRef> RetiredLinks;
    };

    struct Generation {
        const void *Address = nullptr;
        std::uint64_t Fingerprint = 0;
        std::uint64_t Value = 0;
    };

    static std::size_t ViewIndex(GraphView view) noexcept {
        return view == GraphView::Logical ? 0u : 1u;
    }

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

    Status AddNode(CKBehavior *behavior, CKBehavior *parent, int index,
                   bool references, bool ports, GraphModel &out) const {
        GraphNode node;
        node.Id = static_cast<std::uint64_t>(
            static_cast<std::uint32_t>(behavior->GetID()));
        if (references) {
            Status status = Issue(behavior, node.Object);
            if (!status)
                return status;
        }
        node.Parent = parent ? static_cast<std::uint64_t>(
            static_cast<std::uint32_t>(parent->GetID())) : 0;
        node.Index = index;
        node.LayoutGeneration = TrackLayout(behavior);
        node.Prototype = Prototype(behavior);
        node.Name = behavior->GetName() ? behavior->GetName() : "";
        node.Occurrence = static_cast<int>(std::count_if(
            out.Nodes.begin(), out.Nodes.end(), [&](const GraphNode &existing) {
                return existing.Parent == node.Parent &&
                    existing.Name == node.Name;
            }));
        node.Priority = behavior->GetPriority();
        node.Active = behavior->IsActive() != FALSE;
        const Layout layout = m_Runtime.Describe(
            behavior, node.LayoutGeneration);
        node.Kind = layout.Kind;
        if (ports) {
            node.Ports.reserve(layout.Slots.size());
            for (const SlotInfo &slot : layout.Slots) {
                GraphPort port;
                port.Kind = slot.Kind;
                port.LayoutGeneration = node.LayoutGeneration;
                port.Index = slot.Index;
                port.Occurrence = slot.Occurrence;
                port.Type = slot.Type;
                port.Dynamic = slot.Dynamic;
                port.Name = slot.Name;
                if (slot.Kind == SlotKind::Input) {
                    CKBehaviorIO *io = behavior->GetInput(slot.Index);
                    port.Active = io && io->IsActive();
                } else if (slot.Kind == SlotKind::Output) {
                    CKBehaviorIO *io = behavior->GetOutput(slot.Index);
                    port.Active = io && io->IsActive();
                }
                node.Ports.push_back(std::move(port));
            }
        }
        out.Nodes.push_back(std::move(node));
        return {};
    }

    Status ReadGraph(CKBehavior *graph, bool references,
                     GraphModel &out, bool ports = true) const {
        out.View = GraphView::Logical;
        out.Root = {};
        out.Generation = 0;
        out.Fingerprint = 0;
        out.Nodes.clear();
        out.Links.clear();
        out.Operations.clear();
        out.Nodes.reserve(static_cast<std::size_t>(
            graph->GetSubBehaviorCount()) + 1u);
        Status status = AddNode(graph, nullptr, -1, references, ports, out);
        if (!status)
            return status;
        for (int index = 0; index < graph->GetSubBehaviorCount(); ++index) {
            CKBehavior *child = graph->GetSubBehavior(index);
            if (!Valid(child))
                return Failure(Error::InvalidState,
                               "A graph node disappeared during inspection.");
            status = AddNode(child, graph, index, references, ports, out);
            if (!status)
                return status;
        }

        out.Operations.reserve(static_cast<std::size_t>(
            graph->GetParameterOperationCount()));
        for (int index = 0; index < graph->GetParameterOperationCount(); ++index) {
            CKParameterOperation *operation = graph->GetParameterOperation(index);
            if (!Valid(operation) || operation->GetOwner() != graph)
                return Failure(Error::InvalidState,
                               "A graph Parameter Operation disappeared during inspection.");
            CKParameterOut *result = operation->GetOutParameter();
            if (!Valid(result))
                return Failure(Error::InvalidState,
                               "A graph Parameter Operation has no result parameter.");
            GraphOperation record;
            record.Id = static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(operation->GetID()));
            if (references) {
                status = Issue(operation, record.Object);
                if (!status)
                    return status;
            }
            record.Owner = static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(graph->GetID()));
            record.Function = operation->GetOperationGuid();
            record.Result = result->GetGUID();
            CKParameterIn *input1 = operation->GetInParameter1();
            CKParameterIn *input2 = operation->GetInParameter2();
            record.Input1 = Valid(input1) ? input1->GetGUID() : CKPGUID_NONE;
            record.Input2 = Valid(input2) ? input2->GetGUID() : CKPGUID_NONE;
            record.Name = operation->GetName() ? operation->GetName() : "";
            out.Operations.push_back(std::move(record));
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
            if (references) {
                status = Issue(link, record.Object);
                if (!status)
                    return status;
            }
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

    bool InGraph(CKBehavior *graph, const NativeRef &reference,
                 CK_CLASSID type) const {
        if (!graph || !reference ||
            reference.Id > static_cast<std::uint64_t>(
                (std::numeric_limits<CK_ID>::max)()))
            return false;
        CKObject *object = m_Context->GetObject(static_cast<CK_ID>(reference.Id));
        if (object != reference.Address || !Valid(object) ||
            !CKIsChildClassOf(object, type))
            return false;
        if (type == CKCID_BEHAVIOR) {
            for (int index = 0; index < graph->GetSubBehaviorCount(); ++index) {
                if (graph->GetSubBehavior(index) == object)
                    return true;
            }
            return false;
        }
        for (int index = 0; index < graph->GetSubBehaviorLinkCount(); ++index) {
            if (graph->GetSubBehaviorLink(index) == object)
                return true;
        }
        return false;
    }

    static GraphLinkShape Shape(const GraphLink &link) {
        return {link.Source, link.Target, link.InitialDelay};
    }

    Status ApplyLogical(CKBehavior *graph, GraphModel &model) {
        const auto found = m_Logical.find(static_cast<std::uint32_t>(
            graph->GetID()));
        if (found == m_Logical.end())
            return {};
        LogicalState &state = found->second;
        if (state.Root.Address != graph) {
            m_Logical.erase(found);
            return {};
        }

        std::erase_if(state.RetiredNodes, [&](const NativeRef &node) {
            return !InGraph(graph, node, CKCID_BEHAVIOR);
        });
        std::erase_if(state.RetiredLinks, [&](const NativeRef &link) {
            return !InGraph(graph, link, CKCID_BEHAVIORLINK);
        });
        for (const NativeRef &node : state.Graph.InfrastructureNodes) {
            if (!InGraph(graph, node, CKCID_BEHAVIOR))
                return Failure(Error::GraphChanged,
                               "A Patch infrastructure node disappeared.");
        }

        for (const LogicalGraphLink &logical : state.Graph.Links) {
            if (!InGraph(graph, logical.Object, CKCID_BEHAVIORLINK))
                return Failure(Error::GraphChanged,
                               "A Patch-controlled Link disappeared.");
            const auto link = std::find_if(
                model.Links.begin(), model.Links.end(),
                [&](const GraphLink &candidate) {
                    return candidate.Id == logical.Object.Id;
                });
            if (link == model.Links.end() || Shape(*link) != logical.Live)
                return Failure(Error::GraphChanged,
                               "A Patch-controlled Link changed outside its Patch.");
            if (logical.Logical) {
                link->Source = logical.Logical->Source;
                link->Target = logical.Logical->Target;
                link->InitialDelay = logical.Logical->InitialDelay;
            } else {
                model.Links.erase(link);
            }
        }
        for (const NativeRef &retired : state.RetiredLinks) {
            std::erase_if(model.Links, [&](const GraphLink &link) {
                return link.Id == retired.Id;
            });
        }

        std::vector<std::uint64_t> &hidden = m_HiddenNodes;
        hidden.clear();
        hidden.reserve(state.Graph.InfrastructureNodes.size() +
                       state.RetiredNodes.size());
        for (const NativeRef &node : state.Graph.InfrastructureNodes)
            hidden.push_back(node.Id);
        for (const NativeRef &node : state.RetiredNodes)
            hidden.push_back(node.Id);
        for (const GraphLink &link : model.Links) {
            if (std::find(hidden.begin(), hidden.end(), link.Source.Node) !=
                    hidden.end() ||
                std::find(hidden.begin(), hidden.end(), link.Target.Node) !=
                    hidden.end()) {
                return Failure(Error::GraphChanged,
                               "A Patch infrastructure node gained an unmanaged Link.");
            }
        }
        std::erase_if(model.Nodes, [&](const GraphNode &node) {
            return std::find(hidden.begin(), hidden.end(), node.Id) != hidden.end();
        });
        return {};
    }

    static std::uint64_t Fingerprint(const GraphModel &graph) {
        std::uint64_t out = kHashOffset;
        const auto root = std::find_if(
            graph.Nodes.begin(), graph.Nodes.end(),
            [](const GraphNode &node) { return node.Parent == 0; });
        Hash(out, root == graph.Nodes.end() ? std::uint64_t{0} : root->Id);
        const std::size_t childCount = static_cast<std::size_t>(std::count_if(
            graph.Nodes.begin(), graph.Nodes.end(),
            [](const GraphNode &node) { return node.Parent != 0; }));
        Hash(out, childCount);
        Hash(out, graph.Links.size());
        Hash(out, graph.Operations.size());
        for (const GraphNode &node : graph.Nodes) {
            Hash(out, node.Id);
            Hash(out, node.Parent);
            Hash(out, node.Index);
            Hash(out, node.Occurrence);
            Hash(out, node.LayoutGeneration);
            Hash(out, node.Kind);
            Hash(out, node.Prototype.d1);
            Hash(out, node.Prototype.d2);
            HashText(out, node.Name.c_str());
            Hash(out, node.Priority);
            // LayoutGeneration already represents the identity, name, type,
            // order, and dynamic character of every public port. Hashing the
            // materialized port list again would add no change sensitivity.
        }
        for (const GraphLink &link : graph.Links) {
            Hash(out, link.Id);
            Hash(out, link.Source.Node);
            Hash(out, link.Source.Kind);
            Hash(out, link.Source.Index);
            Hash(out, link.Target.Node);
            Hash(out, link.Target.Kind);
            Hash(out, link.Target.Index);
            Hash(out, link.InitialDelay);
        }
        for (const GraphOperation &operation : graph.Operations) {
            Hash(out, operation.Id);
            Hash(out, operation.Owner);
            Hash(out, operation.Function.d1);
            Hash(out, operation.Function.d2);
            Hash(out, operation.Result.d1);
            Hash(out, operation.Result.d2);
            Hash(out, operation.Input1.d1);
            Hash(out, operation.Input1.d2);
            Hash(out, operation.Input2.d1);
            Hash(out, operation.Input2.d2);
            HashText(out, operation.Name.c_str());
        }
        return out;
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
    std::unordered_map<CK_ID, std::array<Generation, 2>> m_Generations;
    mutable std::unordered_map<CK_ID, Generation> m_LayoutGenerations;
    std::unordered_map<std::uint64_t, LogicalState> m_Logical;
    GraphModel m_FingerprintModel;
    std::vector<std::uint64_t> m_HiddenNodes;
};

} // namespace

std::unique_ptr<GraphSource> MakeCKGraphSource(
    CKContext *context, Runtime &runtime,
    std::function<ObjectRef(const void *)> issueObjectRef) {
    return std::make_unique<CKGraphSource>(
        context, runtime, std::move(issueObjectRef));
}

} // namespace BML::Behavior::Internal

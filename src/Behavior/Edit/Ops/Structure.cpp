#include "Behavior/Edit/Transaction.h"

#include "Behavior/Engine/Access.h"
#include "Behavior/Engine/Graph.h"
#include "Behavior/Runtime.h"

#include <algorithm>
#include <array>
#include <unordered_set>

namespace BML::Behavior::Internal {

Status CKEdit::Transaction::PrepareBlocks() {
    Status status;
    for (const Ops::EditNode &node : m_Edit->m_Nodes) {
        if (!node.Block)
            continue;
        if (!m_AddedSpecs.emplace(node.Handle.Value, *node.Block).second) {
            return Failure(
                Error::InvalidState,
                "An added Node is declared more than once.");
        }
    }

    // A targetable BB must own its Target parameter before CK2 admits it to a
    // graph whose owner has another class. Other graph data relations belong
    // to the final EDITED stage, but the Target is part of native placement.
    for (const CheckedBind &bind : m_Checked.Binds) {
        if (bind.Target.Slot.Kind != SlotKind::Target)
            continue;
        const Ops::EditNode *targetNode = m_Edit->Find(bind.Target.Owner);
        if (!targetNode || !targetNode->Block)
            continue;
        const auto found = m_AddedSpecs.find(bind.Target.Owner.Value);
        if (found == m_AddedSpecs.end()) {
            return Failure(
                Error::InvalidState,
                "An added Block lost its Target configuration.");
        }
        BlockSpec &spec = found->second;
        const CKGUID type = bind.Target.Slot.Type;
        if (bind.Kind == BindKind::Literal) {
            spec.TargetValue(type, bind.Value);
            continue;
        }

        CKObject *sourceObject = nullptr;
        status = ParameterBeforeApply(bind.Source, sourceObject);
        if (!status)
            return status;
        if (!sourceObject) {
            return Failure(
                Error::InvalidState,
                "An added Block Target must use a parameter that exists before the Block is placed.");
        }
        if (bind.Kind == BindKind::Direct) {
            CKParameter *source = CKParameter::Cast(sourceObject);
            if (!source) {
                return Failure(
                    Error::TypeMismatch,
                    "A direct Target source is not a stored parameter.");
            }
            spec.TargetSource(type, source);
        } else {
            CKParameterIn *source = CKParameterIn::Cast(sourceObject);
            if (!source) {
                return Failure(
                    Error::TypeMismatch,
                    "A shared Target source is not a Pin.");
            }
            spec.TargetShared(type, source);
        }
    }
    return {};
}

// Materialize authored Nodes in declaration order. A Block follows the
// native BB lifecycle; a plain graph has no BB callback and is owned
// directly by this Patch journal.
Status CKEdit::Transaction::CreateNodes() {
    Status status;
    for (const Ops::EditNode &node : m_Edit->m_Nodes) {
        if (!node.Authored())
            continue;
        if (node.Subgraph) {
            CKBehavior *added = CKBehavior::Cast(m_Context->CreateObject(
                CKCID_BEHAVIOR,
                node.Subgraph->Name.empty()
                    ? nullptr
                    : const_cast<CKSTRING>(node.Subgraph->Name.c_str())));
            if (!added)
                return Failure(
                    Error::CreateFailed,
                    "Virtools could not create a graph-backed Node.",
                    CKERR_OUTOFMEMORY);
            added->UseGraph();
            added->SetPriority(node.Subgraph->Priority);
            const CKERROR attached = Engine::AddChild(m_Graph, added);
            if (attached != CK_OK) {
                m_Context->DestroyObject(added);
                return Failure(
                    Error::CreateFailed,
                    "Virtools rejected the graph-backed Node relation.",
                    attached);
            }
            const Stamp stamp = Capture(added);
            m_Handles.emplace(node.Handle.Value, stamp);
            m_Journal.GraphNodes.push_back(stamp);
            if (node.Role == NodeRole::Infrastructure)
                m_Journal.InfrastructureNodes.push_back(stamp);
            status = ValidateNodes();
            if (!status)
                return status;
            m_Graph = GraphFor();
            continue;
        }
        const auto spec = m_AddedSpecs.find(node.Handle.Value);
        if (spec == m_AddedSpecs.end()) {
            return Failure(
                Error::InvalidState,
                "An added Block lost its configuration.");
        }
        AttachResult added = m_Runtime.CreateInGraph(m_Graph, spec->second);
        if (!added || !added.Block)
            return added.Detail;
        const Stamp stamp = Capture(added.Block);
        m_Handles.emplace(node.Handle.Value, stamp);
        m_Journal.Nodes.push_back(stamp);
        if (node.Role == NodeRole::Infrastructure)
            m_Journal.InfrastructureNodes.push_back(stamp);
        status = ValidateNodes();
        if (!status)
            return status;
        m_Graph = GraphFor();
    }
    return {};
}

// A replacement preserves the public graph role of an idle Node.
// RemoveSubBehavior removes graph membership while keeping the owner and
// native lifecycle state. Retail CK2's SetParent(nullptr) is a no-op, so
// the parked Node also keeps the graph's parent identity. That lets the
// Patch park the exact original object and restore it without inventing
// DETACH/DELETE callbacks.
Status CKEdit::Transaction::Replace() {
    std::unordered_set<CKObject *> installedPorts;
    std::unordered_set<CKBehavior *> originalNodes;
    std::unordered_set<CKParameter *> privateState;
    const auto slots = [](const Layout &layout, SlotKind kind) {
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
    const std::array<SlotKind, 5> publicKinds{
        SlotKind::Input, SlotKind::Output, SlotKind::Target,
        SlotKind::InputParameter, SlotKind::OutputParameter};

    for (const EditReplace &item : m_Checked.Replacements) {
        CKBehavior *original = BehaviorFor(item.Target);
        CKBehavior *installed = BehaviorFor(item.Replacement);
        if (!original || !installed || original == installed ||
            original->GetParent() != m_Graph || installed->GetParent() != m_Graph) {
            return Failure(
                Error::InvalidGraphLocality,
                "A replacement Node left the target graph before Apply.",
                CKERR_INVALIDOBJECT);
        }
        if (original->IsActive()) {
            return Failure(
                Error::Busy,
                "A running Behavior Node cannot be replaced.");
        }
        for (int index = 0; index < original->GetInputCount(); ++index) {
            CKBehaviorIO *io = original->GetInput(index);
            if (io && io->IsActive())
                return Failure(
                    Error::Busy,
                    "A Behavior Node with an active In cannot be replaced.");
        }
        for (int index = 0; index < original->GetOutputCount(); ++index) {
            CKBehaviorIO *io = original->GetOutput(index);
            if (io && io->IsActive())
                return Failure(
                    Error::Busy,
                    "A Behavior Node with an active Out cannot be replaced.");
        }

        Patch::Journal::Replacement replacement;
        replacement.Original = Capture(original);
        replacement.Installed = Capture(installed);
        originalNodes.insert(original);
        const std::size_t ownerIndex = m_Journal.Replacements.size();
        const Layout originalLayout = m_Runtime.Describe(original);
        const Layout installedLayout = m_Runtime.Describe(installed);
        for (SlotKind kind : publicKinds) {
            const auto before = slots(originalLayout, kind);
            const auto after = slots(installedLayout, kind);
            if (before.size() != after.size()) {
                return Failure(
                    Error::InterfaceUnsupported,
                    "A replacement Block has a different public Behavior interface.");
            }
            for (std::size_t index = 0; index < before.size(); ++index) {
                const SlotInfo &oldSlot = *before[index];
                const SlotInfo &newSlot = *after[index];
                if (oldSlot.Index != newSlot.Index ||
                    oldSlot.Name != newSlot.Name ||
                    oldSlot.Occurrence != newSlot.Occurrence ||
                    oldSlot.Type != newSlot.Type) {
                    return Failure(
                        Error::InterfaceUnsupported,
                        "A replacement Block changed a public Behavior port.");
                }
                CKObject *oldPort = m_Runtime.ResolveSlotObject(
                    original, oldSlot);
                CKObject *newPort = m_Runtime.ResolveSlotObject(
                    installed, newSlot);
                if (!oldPort || !newPort) {
                    return Failure(
                        Error::GraphChanged,
                        "A replacement public port disappeared before Apply.",
                        CKERR_INVALIDOBJECT);
                }
                replacement.Ports.push_back(
                    {Capture(oldPort), Capture(newPort), kind, oldSlot.Index});
                m_ReplacementPorts.emplace(oldPort, newPort);
                m_ReplacementOwners.emplace(oldPort, ownerIndex);
                installedPorts.insert(newPort);
            }
        }
        for (const SlotInfo &slot : originalLayout.Slots) {
            if (slot.Kind != SlotKind::Setting &&
                slot.Kind != SlotKind::Local)
                continue;
            if (auto *state = CKParameter::Cast(
                    m_Runtime.ResolveSlotObject(original, slot)))
                privateState.insert(state);
        }

        if (const char *name = original->GetName())
            installed->SetName(const_cast<CKSTRING>(name));
        installed->SetPriority(original->GetPriority());
        m_Journal.Replacements.push_back(std::move(replacement));
    }

    const auto isPrivateParameter = [&](CKParameter *parameter) {
        if (!parameter)
            return false;
        if (privateState.contains(parameter))
            return true;
        CKBehavior *owner = CKBehavior::Cast(parameter->GetOwner());
        return owner && originalNodes.contains(owner) &&
            !m_ReplacementPorts.contains(parameter);
    };

    // Preserve the original Pin and Target source relations in the
    // replacement BlockSpec. Explicit Bind actions that follow this step may
    // deliberately override them before the Block's single EDITED callback.
    for (std::size_t index = 0; index < m_Checked.Replacements.size(); ++index) {
        const EditReplace &item = m_Checked.Replacements[index];
        const auto spec = m_AddedSpecs.find(item.Replacement.Value);
        if (spec == m_AddedSpecs.end())
            return Failure(Error::InvalidState,
                           "A replacement Block lost its configuration.");
        BlockSpec &block = spec->second;
        for (const Patch::Journal::Replacement::PortPair &pair :
             m_Journal.Replacements[index].Ports) {
            if (pair.Kind != SlotKind::InputParameter &&
                pair.Kind != SlotKind::Target)
                continue;
            auto *oldInput = Resolve<CKParameterIn>(
                m_Context, pair.Original, CKCID_PARAMETERIN);
            auto *newInput = Resolve<CKParameterIn>(
                m_Context, pair.Installed, CKCID_PARAMETERIN);
            if (!oldInput || !newInput)
                return Failure(
                    Error::GraphChanged,
                    "A replacement Pin changed identity before Apply.",
                    CKERR_INVALIDOBJECT);

            CKParameterIn *shared = oldInput->GetSharedSource();
            CKParameter *direct = shared ? nullptr : oldInput->GetDirectSource();
            if (shared) {
                const auto mapped = m_ReplacementPorts.find(shared);
                if (mapped != m_ReplacementPorts.end())
                    shared = CKParameterIn::Cast(mapped->second);
                if (!shared)
                    return Failure(
                        Error::TypeMismatch,
                        "A replacement could not preserve a shared Pin source.");
                if (pair.Kind == SlotKind::Target)
                    block.TargetShared(newInput->GetGUID(), shared);
                else
                    block.Input(Slot::At(pair.Kind, pair.Index,
                                         newInput->GetGUID()),
                                Parameter::Binding::Shared(shared));
            } else if (direct) {
                if (isPrivateParameter(direct))
                    return Failure(
                        Error::InterfaceUnsupported,
                        "A replacement public Pin depends on the original Block's private state.");
                const auto mapped = m_ReplacementPorts.find(direct);
                if (mapped != m_ReplacementPorts.end())
                    direct = CKParameter::Cast(mapped->second);
                if (!direct)
                    return Failure(
                        Error::TypeMismatch,
                        "A replacement could not preserve a direct Pin source.");
                if (pair.Kind == SlotKind::Target)
                    block.TargetSource(newInput->GetGUID(), direct);
                else
                    block.Input(Slot::At(pair.Kind, pair.Index,
                                         newInput->GetGUID()),
                                Parameter::Binding::Direct(direct));
            }
        }
    }

    const auto relationOwnerInGraph = [&](CKObject *parameter) {
        CKObject *owner = nullptr;
        if (auto *input = CKParameterIn::Cast(parameter))
            owner = input->GetOwner();
        else if (auto *value = CKParameter::Cast(parameter))
            owner = value->GetOwner();
        if (CKBehavior *behavior = CKBehavior::Cast(owner))
            return behavior == m_Graph || behavior->GetParent() == m_Graph;
        if (auto *operation = CKParameterOperation::Cast(owner))
            return operation->GetOwner() == m_Graph;
        return false;
    };

    // Find every graph-local Pin that reads a public parameter of a parked
    // Node. CKParameterIn does not expose reverse users, so CK2's exact
    // ParameterIn object list is the authoritative relation inventory.
    const int inputCount = m_Context->GetObjectsCountByClassID(
        CKCID_PARAMETERIN);
    CK_ID *inputIds = m_Context->GetObjectsListByClassID(CKCID_PARAMETERIN);
    for (int index = 0; index < inputCount; ++index) {
        auto *input = CKParameterIn::Cast(m_Context->GetObject(inputIds[index]));
        if (!input || input->IsToBeDeleted() ||
            m_ReplacementPorts.contains(input) || installedPorts.contains(input))
            continue;
        CKParameterIn *oldShared = input->GetSharedSource();
        CKParameter *oldDirect = oldShared ? nullptr : input->GetDirectSource();
        CKObject *oldSource = oldShared
            ? static_cast<CKObject *>(oldShared)
            : static_cast<CKObject *>(oldDirect);
        const auto mapped = m_ReplacementPorts.find(oldSource);
        if (mapped == m_ReplacementPorts.end()) {
            if (oldDirect && isPrivateParameter(oldDirect)) {
                return Failure(
                    Error::InvalidGraphLocality,
                    "A parameter outside the replaced Node reads its private state.");
            }
            continue;
        }
        if (!relationOwnerInGraph(input)) {
            return Failure(
                Error::InvalidGraphLocality,
                "A parameter outside the target graph reads the replaced Node.");
        }
        const std::size_t owner = m_ReplacementOwners.at(oldSource);
        auto &change = m_Journal.Replacements[owner].Inputs.emplace_back();
        change.Input = Capture(input);
        change.OriginalDirect = Capture(oldDirect);
        change.OriginalShared = Capture(oldShared);
        if (oldShared) {
            auto *newShared = CKParameterIn::Cast(mapped->second);
            if (!newShared)
                return Failure(
                    Error::TypeMismatch,
                    "A shared Pin source does not map to a replacement Pin.");
            change.InstalledShared = Capture(newShared);
        } else {
            auto *newDirect = CKParameter::Cast(mapped->second);
            if (!newDirect)
                return Failure(
                    Error::TypeMismatch,
                    "A direct Pin source does not map to a replacement parameter.");
            change.InstalledDirect = Capture(newDirect);
        }
    }

    // Settings, Locals and other non-public parameters stay with the parked
    // implementation. A graph relation that writes one of them cannot be
    // transferred to the replacement without pretending it is public state.
    const int outputCount = m_Context->GetObjectsCountByClassID(
        CKCID_PARAMETEROUT);
    CK_ID *outputIds = m_Context->GetObjectsListByClassID(
        CKCID_PARAMETEROUT);
    for (int index = 0; index < outputCount; ++index) {
        auto *source = CKParameterOut::Cast(
            m_Context->GetObject(outputIds[index]));
        if (!source || source->IsToBeDeleted() ||
            installedPorts.contains(source))
            continue;
        for (int destinationIndex = 0;
             destinationIndex < source->GetDestinationCount();
             ++destinationIndex) {
            if (isPrivateParameter(source->GetDestination(destinationIndex)))
                return Failure(
                    Error::InterfaceUnsupported,
                    "A graph Pout writes the original Block's private state.");
        }
    }

    for (const auto &[oldObject, newObject] : m_ReplacementPorts) {
        auto *oldOutput = CKParameterOut::Cast(oldObject);
        auto *newOutput = CKParameterOut::Cast(newObject);
        if (!oldOutput || !newOutput)
            continue;
        std::vector<CKParameter *> destinations;
        for (int index = 0; index < oldOutput->GetDestinationCount(); ++index)
            destinations.push_back(oldOutput->GetDestination(index));
        for (CKParameter *destination : destinations) {
            if (!destination || destination->IsToBeDeleted())
                return Failure(
                    Error::GraphChanged,
                    "A Pout destination disappeared before replacement.",
                    CKERR_INVALIDOBJECT);
            if (privateState.contains(destination))
                return Failure(
                    Error::InvalidGraphLocality,
                    "A Pout destination belongs to the original Block's private state.");
            CKParameter *originalDestination = destination;
            const auto mapped = m_ReplacementPorts.find(destination);
            if (mapped != m_ReplacementPorts.end())
                destination = CKParameter::Cast(mapped->second);
            if (!destination || !relationOwnerInGraph(destination))
                return Failure(
                    Error::InvalidGraphLocality,
                    "A Pout destination lies outside the replacement graph.");
            if (ContainsDestination(newOutput, destination))
                return Failure(
                    Error::SourceConflict,
                    "The replacement Pout already owns an original destination.");
            const std::size_t owner = m_ReplacementOwners.at(oldObject);
            m_Journal.Replacements[owner].Destinations.push_back(
                {Capture(oldOutput), Capture(newOutput),
                 Capture(originalDestination), Capture(destination)});
        }
    }

    for (auto &replacement : m_Journal.Replacements) {
        for (auto &change : replacement.Inputs) {
            CKParameterIn *input = Resolve<CKParameterIn>(
                m_Context, change.Input, CKCID_PARAMETERIN);
            CKERROR error = CKERR_INVALIDOBJECT;
            if (input && change.InstalledShared.Id) {
                error = input->ShareSourceWith(Resolve<CKParameterIn>(
                    m_Context, change.InstalledShared, CKCID_PARAMETERIN));
            } else if (input) {
                error = input->SetDirectSource(Resolve<CKParameter>(
                    m_Context, change.InstalledDirect, CKCID_PARAMETER));
            }
            if (error != CK_OK)
                return Failure(
                    Error::TypeMismatch,
                    "Virtools rejected a replacement Pin source.", error);
            change.Applied = true;
            if (CKBehavior *owner = CKBehavior::Cast(input->GetOwner()))
                RememberEdited(owner);
        }
        for (auto &change : replacement.Destinations) {
            CKParameterOut *oldOutput = Resolve<CKParameterOut>(
                m_Context, change.OriginalSource, CKCID_PARAMETEROUT);
            CKParameterOut *newOutput = Resolve<CKParameterOut>(
                m_Context, change.InstalledSource, CKCID_PARAMETEROUT);
            CKParameter *originalDestination = Resolve<CKParameter>(
                m_Context, change.OriginalParameter, CKCID_PARAMETER);
            CKParameter *installedDestination = Resolve<CKParameter>(
                m_Context, change.InstalledParameter, CKCID_PARAMETER);
            if (!oldOutput || !newOutput || !originalDestination ||
                !installedDestination)
                return Failure(
                    Error::GraphChanged,
                    "A replacement Pout relation changed before Apply.",
                    CKERR_INVALIDOBJECT);
            const CKERROR error = newOutput->AddDestination(
                installedDestination, TRUE);
            if (error != CK_OK)
                return Failure(
                    Error::TypeMismatch,
                    "Virtools rejected a replacement Pout destination.", error);
            oldOutput->RemoveDestination(originalDestination);
            change.Applied = true;
        }
    }
    return {};
}

// Keep each native Link object and its current delay state. Only its exact
// endpoint objects change, so a delayed activation remains the same Link
// in CK2's scheduler rather than being recreated or guessed from fields.
Status CKEdit::Transaction::Relink() {
    std::vector<CKBehaviorLink *> graphLinks;
    graphLinks.reserve(static_cast<std::size_t>(m_Graph->GetSubBehaviorLinkCount()));
    for (int index = 0; index < m_Graph->GetSubBehaviorLinkCount(); ++index)
        graphLinks.push_back(m_Graph->GetSubBehaviorLink(index));
    for (CKBehaviorLink *link : graphLinks) {
        if (!link)
            return Failure(Error::GraphChanged,
                           "A graph Link disappeared during replacement.");
        CKBehaviorIO *oldSource = link->GetInBehaviorIO();
        CKBehaviorIO *oldSink = link->GetOutBehaviorIO();
        const auto source = m_ReplacementPorts.find(oldSource);
        const auto sink = m_ReplacementPorts.find(oldSink);
        if (source == m_ReplacementPorts.end() &&
            sink == m_ReplacementPorts.end())
            continue;
        auto *newSource = source == m_ReplacementPorts.end()
            ? oldSource : static_cast<CKBehaviorIO *>(source->second);
        auto *newSink = sink == m_ReplacementPorts.end()
            ? oldSink : static_cast<CKBehaviorIO *>(sink->second);
        const std::size_t owner = source != m_ReplacementPorts.end()
            ? m_ReplacementOwners.at(oldSource)
            : m_ReplacementOwners.at(oldSink);
        auto &change = m_Journal.Replacements[owner].Links.emplace_back();
        change.Link = Capture(link);
        change.OriginalSource = Capture(oldSource);
        change.OriginalSink = Capture(oldSink);
        change.InstalledSource = Capture(newSource);
        change.InstalledSink = Capture(newSink);
        CKERROR error = CK_OK;
        if (newSource != oldSource)
            error = link->SetInBehaviorIO(newSource);
        if (error == CK_OK && newSink != oldSink)
            error = link->SetOutBehaviorIO(newSink);
        if (error != CK_OK) {
            if (newSource != oldSource)
                (void) link->SetInBehaviorIO(oldSource);
            return Failure(
                Error::GraphChanged,
                "Virtools rejected a replacement Link endpoint.", error);
        }
        change.Applied = true;
    }

    for (std::size_t index = 0; index < m_Checked.Replacements.size(); ++index) {
        const EditReplace &item = m_Checked.Replacements[index];
        auto &replacement = m_Journal.Replacements[index];
        CKBehavior *original = Resolve<CKBehavior>(
            m_Context, replacement.Original, CKCID_BEHAVIOR);
        m_Graph = GraphFor();
        if (!m_Graph || !original || m_Graph->RemoveSubBehavior(original) != original)
            return Failure(
                Error::GraphChanged,
                "Virtools could not park the original Behavior Node.",
                CKERR_INVALIDOBJECT);
        if (Engine::Contains(m_Graph, original))
            return Failure(
                Error::GraphChanged,
                "Virtools kept the parked Behavior Node in its graph.",
                CKERR_INVALIDOBJECT);
        replacement.OriginalRemoved = true;
        m_Handles.erase(item.Target.Value);
    }
    return {};
}

Status CKEdit::Transaction::Remove() {
    if (!m_Checked.Removals.empty()) {
        m_Graph = GraphFor();
        if (!m_Graph) {
            return Failure(
                Error::GraphChanged,
                "The Behavior graph disappeared before Node removal.",
                CKERR_INVALIDOBJECT);
        }

        std::unordered_set<CKBehavior *> removedNodes;
        for (const EditRemove &item : m_Checked.Removals) {
            CKBehavior *node = BehaviorFor(item.Target);
            if (!node || !Engine::Contains(m_Graph, node) || node->IsActive()) {
                return Failure(
                    Error::Busy,
                    "A removal target is no longer an idle child Node.");
            }
            for (int port = 0; port < node->GetInputCount(); ++port) {
                CKBehaviorIO *input = node->GetInput(port);
                if (input && input->IsActive()) {
                    return Failure(
                        Error::Busy,
                        "A Behavior Node with an active In cannot be removed.");
                }
            }
            for (int port = 0; port < node->GetOutputCount(); ++port) {
                CKBehaviorIO *output = node->GetOutput(port);
                if (output && output->IsActive()) {
                    return Failure(
                        Error::Busy,
                        "A Behavior Node with an active Out cannot be removed.");
                }
            }
            m_Journal.Removals.push_back({Capture(node)});
            removedNodes.insert(node);
        }

        std::vector<CKBehaviorLink *> incidentLinks;
        incidentLinks.reserve(
            static_cast<std::size_t>(m_Graph->GetSubBehaviorLinkCount()));
        for (int index = 0; index < m_Graph->GetSubBehaviorLinkCount(); ++index) {
            CKBehaviorLink *link = m_Graph->GetSubBehaviorLink(index);
            CKBehaviorIO *source = link ? link->GetInBehaviorIO() : nullptr;
            CKBehaviorIO *sink = link ? link->GetOutBehaviorIO() : nullptr;
            CKBehavior *sourceNode = source
                ? CKBehavior::Cast(source->GetOwner()) : nullptr;
            CKBehavior *sinkNode = sink
                ? CKBehavior::Cast(sink->GetOwner()) : nullptr;
            if (removedNodes.contains(sourceNode) ||
                removedNodes.contains(sinkNode)) {
                if (!source || !sink) {
                    return Failure(
                        Error::GraphChanged,
                        "A removal target has a Link without both endpoints.",
                        CKERR_INVALIDOBJECT);
                }
                if (source->IsActive()) {
                    return Failure(
                        Error::Busy,
                        "Behavior Link " +
                            std::to_string(static_cast<std::uint32_t>(
                                link->GetID())) +
                            " has an active source.");
                }
                if (Engine::IsDelayed(link)) {
                    return Failure(
                        Error::Busy,
                        "A delayed Behavior Link is still pending.");
                }
                incidentLinks.push_back(link);
                m_Journal.RemovedLinks.push_back(
                    {Capture(link), Capture(source), Capture(sink),
                     link->GetInitialActivationDelay(),
                     link->GetActivationDelay()});
            }
        }

        if (!incidentLinks.empty()) {
            auto *detachedSource = CKBehaviorIO::Cast(m_Context->CreateObject(CKCID_BEHAVIORIO));
            auto *detachedSink = CKBehaviorIO::Cast(m_Context->CreateObject(CKCID_BEHAVIORIO));
            if (!detachedSource || !detachedSink) {
                if (detachedSource)
                    m_Context->DestroyObject(detachedSource);
                if (detachedSink)
                    m_Context->DestroyObject(detachedSink);
                return Failure(
                    Error::CreateFailed,
                    "Virtools could not create detached Behavior Link endpoints.",
                    CKERR_OUTOFMEMORY);
            }
            detachedSource->SetType(CK_BEHAVIORIO_OUT);
            detachedSink->SetType(CK_BEHAVIORIO_IN);
            m_Journal.DetachedSource = Capture(detachedSource);
            m_Journal.DetachedSink = Capture(detachedSink);
        }

        for (std::size_t index = 0; index < incidentLinks.size(); ++index) {
            CKBehaviorLink *link = incidentLinks[index];
            if (m_Graph->RemoveSubBehaviorLink(link) != link ||
                Engine::Contains(m_Graph, link)) {
                return Failure(
                    Error::GraphChanged,
                    "Virtools could not remove a Behavior Link.",
                    CKERR_INVALIDOBJECT);
            }
            auto &removed = m_Journal.RemovedLinks[index];
            removed.Removed = true;
            CKBehaviorIO *detachedSource = ResolveIo(
                m_Context, m_Journal.DetachedSource);
            CKBehaviorIO *detachedSink = ResolveIo(
                m_Context, m_Journal.DetachedSink);
            if (!detachedSource || !detachedSink)
                return Failure(
                    Error::GraphChanged,
                    "A detached Behavior Link endpoint disappeared.",
                    CKERR_INVALIDOBJECT);
            CKERROR detached = link->SetInBehaviorIO(detachedSource);
            if (detached != CK_OK)
                return Failure(
                    Error::GraphChanged,
                    "Virtools could not detach a Behavior Link source.",
                    detached);
            removed.SourceDetached = true;
            detached = link->SetOutBehaviorIO(detachedSink);
            if (detached != CK_OK)
                return Failure(
                    Error::GraphChanged,
                    "Virtools could not detach a Behavior Link sink.",
                    detached);
            removed.SinkDetached = true;
        }
        for (std::size_t index = 0; index < m_Checked.Removals.size(); ++index) {
            const EditRemove &item = m_Checked.Removals[index];
            CKBehavior *node = Resolve<CKBehavior>(
                m_Context, m_Journal.Removals[index].Node, CKCID_BEHAVIOR);
            if (!node || m_Graph->RemoveSubBehavior(node) != node ||
                Engine::Contains(m_Graph, node)) {
                return Failure(
                    Error::GraphChanged,
                    "Virtools could not remove a Behavior Node.",
                    CKERR_INVALIDOBJECT);
            }
            m_Journal.Removals[index].Removed = true;
            m_Handles.erase(item.Target.Value);
        }
    }
    return {};
}

} // namespace BML::Behavior::Internal

#include "Behavior/Edit/Transaction.h"

#include <algorithm>
#include <optional>
#include <sstream>
#include <unordered_set>

namespace BML::Behavior::Internal {
namespace {

struct PreparedSet {
    const CheckedSet *Checked = nullptr;
    CKParameter *Parameter = nullptr;
    CKBehavior *Receiver = nullptr;
};

using ParameterId = std::uint64_t;

constexpr ParameterId kPlannedParameter = 1ull << 63u;

ParameterId PlanParameter(const ResolvedPort &port) {
    if (port.Interface != 0) {
        return kPlannedParameter |
               (static_cast<ParameterId>(port.Owner.Value) << 32u) |
               0x80000000u | port.Interface;
    }
    return kPlannedParameter |
           (static_cast<ParameterId>(port.Owner.Value) << 32u) |
           (static_cast<ParameterId>(port.Slot.Kind) << 24u) |
           static_cast<std::uint32_t>(port.Slot.NativeIndex);
}

ParameterId NativeParameter(CKObject *parameter) {
    return parameter
        ? static_cast<std::uint32_t>(parameter->GetID()) : 0;
}

GraphEndpoint DescribePin(CKParameterIn *input) {
    CKBehavior *owner = input
        ? CKBehavior::Cast(input->GetOwner()) : nullptr;
    if (!owner)
        return {};
    if (owner->GetTargetParameter() == input) {
        return {static_cast<std::uint32_t>(owner->GetID()),
                SlotKind::Target, 0};
    }
    const int index = owner->GetInputParameterPosition(input);
    return index < 0
        ? GraphEndpoint{}
        : GraphEndpoint{static_cast<std::uint32_t>(owner->GetID()),
                        SlotKind::InputParameter, index};
}

GraphEndpoint DescribeLocal(CKParameterLocal *local) {
    CKBehavior *owner = local ? CKBehavior::Cast(local->GetOwner()) : nullptr;
    if (!owner)
        return {};
    const int index = owner->GetLocalParameterPosition(local);
    if (index < 0)
        return {};
    return {static_cast<std::uint32_t>(owner->GetID()),
            owner->IsLocalParameterSetting(index) ? SlotKind::Setting
                                                  : SlotKind::Local,
            index};
}

} // namespace

Status CKEdit::Transaction::CheckValues() {
    Status status;
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
    for (const CheckedBind &bind : m_Checked.Binds) {
        status = selectorType(bind.Target);
        if (status && bind.Kind != BindKind::Literal)
            status = selectorType(bind.Source);
        if (status) {
            const CKGUID source = bind.Kind == BindKind::Literal
                ? bind.Value.Type() : bind.Source.Slot.Type;
            status = compatible(bind.Target.Slot.Type, source, "Bind");
        }
        if (!status)
            return status;
    }
    for (const CheckedSet &set : m_Checked.Sets) {
        status = selectorType(set.Target);
        if (status)
            status = compatible(set.Target.Slot.Type,
                                set.Value.Type(), "Set");
        if (!status)
            return status;
    }
    for (const CheckedPush &push : m_Checked.Pushes) {
        status = selectorType(push.Source);
        if (status)
            status = selectorType(push.Destination);
        if (status)
            status = compatible(push.Destination.Slot.Type,
                                push.Source.Slot.Type, "Push");
        if (!status)
            return status;
    }
    std::unordered_map<ParameterId, CKObject *> nativeParameters;
    const auto parameterId = [&](const ResolvedPort &port,
                                 ParameterId &id) -> Status {
        CKObject *parameter = nullptr;
        Status result = ParameterBeforeApply(port, parameter);
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
    for (const CheckedBind &bind : m_Checked.Binds) {
        ParameterId target = 0;
        status = parameterId(bind.Target, target);
        if (!status)
            return status;
        if (bind.Kind != BindKind::Shared) {
            shared[target] = std::nullopt;
            continue;
        }
        ParameterId source = 0;
        status = parameterId(bind.Source, source);
        if (!status)
            return status;
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
                return Failure(
                    Error::SharedSourceCycle,
                    "The final shared-source graph contains a cycle.");
            std::optional<ParameterId> next;
            status = sharedSource(current, next);
            if (!status)
                return status;
            if (!next)
                break;
            current = *next;
        }
    }

    std::unordered_map<ParameterId, std::vector<ParameterId>> pushes;
    std::vector<std::pair<ParameterId, ParameterId>> pushEdges;
    for (const CheckedPush &push : m_Checked.Pushes) {
        ParameterId source = 0;
        ParameterId destination = 0;
        status = parameterId(push.Source, source);
        if (status)
            status = parameterId(push.Destination, destination);
        if (!status)
            return status;
        auto *nativeSource = source & kPlannedParameter
            ? nullptr : CKParameterOut::Cast(nativeParameters[source]);
        auto *nativeDestination = destination & kPlannedParameter
            ? nullptr : CKParameter::Cast(nativeParameters[destination]);
        if (nativeSource && nativeDestination &&
            ContainsDestination(nativeSource, nativeDestination)) {
            return Failure(
                Error::InvalidState,
                "The Pout already has this destination.");
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
                return Failure(
                    Error::PushCycle,
                    "The final Pout destination graph contains a cycle.");
            if (!seen.insert(current).second)
                continue;
            std::vector<ParameterId> destinations;
            status = pushDestinations(current, destinations);
            if (!status)
                return status;
            pending.insert(pending.end(), destinations.begin(),
                           destinations.end());
        }
    }
    return {};
}

Status CKEdit::Transaction::AddOperations() {
    for (const EditOperation &item : m_Edit->m_Operations) {
        std::ostringstream name;
        name << "__BML_Operation_" << m_Graph->GetID() << '_'
             << item.Handle.Value;
        CKParameterOperation *operation = m_Context->CreateCKParameterOperation(
            const_cast<CKSTRING>(name.str().c_str()), item.Operation,
            item.Result, item.Input1, item.Input2);
        if (!operation) {
            Status failed = Failure(
                Error::CreateFailed,
                "Virtools failed to create a Parameter Operation.",
                CKERR_OUTOFMEMORY);
            failed.Details.OperationGuid = item.Operation;
            return failed;
        }
        if (!operation->GetOutParameter() ||
            !operation->GetOperationFunction()) {
            m_Context->DestroyObject(operation);
            Status failed = Failure(
                Error::OperationInvalid,
                "No native Parameter Operation function matches the requested type tuple.",
                CKERR_INVALIDPARAMETER);
            failed.Details.OperationGuid = item.Operation;
            return failed;
        }
        const CKERROR added = m_Graph->AddParameterOperation(operation);
        if (added != CK_OK) {
            m_Context->DestroyObject(operation);
            Status failed = Failure(
                Error::OperationInvalid,
                "Virtools rejected the Parameter Operation graph ownership.",
                added);
            failed.Details.OperationGuid = item.Operation;
            return failed;
        }
        // A copy or delete of a dynamic graph leaves static Operations and
        // their parameters out.
        if (m_Graph->IsDynamic()) {
            for (CKObject *object : {static_cast<CKObject *>(operation),
                                     static_cast<CKObject *>(operation->GetInParameter1()),
                                     static_cast<CKObject *>(operation->GetInParameter2()),
                                     static_cast<CKObject *>(operation->GetOutParameter())}) {
                if (object)
                    m_Context->ChangeObjectDynamic(object, TRUE);
            }
        }
        const Stamp stamp = Capture(operation);
        m_Journal.Operations.push_back({Capture(m_Graph), stamp});
        m_OperationObjects.emplace(item.Handle.Value, stamp);
    }
    return {};
}

Status CKEdit::Transaction::Bind() {
    Status status;
    for (const CheckedBind &bind : m_Checked.Binds) {
        CKObject *targetParameter = nullptr;
        status = DataPort(bind.Target, targetParameter);
        if (!status)
            return status;

        const Ops::EditNode *targetNode = m_Edit->Find(bind.Target.Owner);
        if (targetNode && targetNode->Block) {
            const auto found = m_AddedSpecs.find(bind.Target.Owner.Value);
            if (found == m_AddedSpecs.end())
                return Failure(Error::InvalidState,
                               "An added Block lost its configuration.");
            BlockSpec &spec = found->second;
            SlotInfo liveTarget;
            status = LiveSlot(bind.Target, liveTarget);
            if (!status)
                return status;
            Slot target = Slot::At(
                liveTarget.Kind, liveTarget.Index, liveTarget.Type);

            if (bind.Kind == BindKind::Literal) {
                if (target.Kind == SlotKind::InputParameter) {
                    spec.Input(std::move(target), bind.Value);
                } else if (target.Kind == SlotKind::Local) {
                    spec.Local(std::move(target), bind.Value);
                } else if (target.Kind == SlotKind::Target) {
                    spec.TargetValue(bind.Target.Slot.Type, bind.Value);
                } else {
                    return Failure(
                        Error::TypeMismatch,
                        "A Block value requires a Pin, Local, or Target.");
                }
                continue;
            }

            CKObject *sourceObject = nullptr;
            status = DataPort(bind.Source, sourceObject);
            if (!status)
                return status;
            if (bind.Kind == BindKind::Direct) {
                CKParameter *source = CKParameter::Cast(sourceObject);
                if (!source)
                    return Failure(
                        Error::TypeMismatch,
                        "A direct Bind source is not a stored parameter.");
                if (target.Kind == SlotKind::Target)
                    spec.TargetSource(bind.Target.Slot.Type, source);
                else
                    spec.Input(std::move(target),
                               Parameter::Binding::Direct(source));
            } else {
                CKParameterIn *source = CKParameterIn::Cast(sourceObject);
                if (!source)
                    return Failure(Error::TypeMismatch,
                                   "A shared Bind source is not a Pin.");
                if (target.Kind == SlotKind::Target)
                    spec.TargetShared(bind.Target.Slot.Type, source);
                else
                    spec.Input(std::move(target),
                               Parameter::Binding::Shared(source));
            }
            continue;
        }

        if (!bind.Target.Operation)
            RememberEdited(BehaviorFor(bind.Target.Owner));
        // A Local holds its value itself, so there is no source relation to
        // install. Keep both journal values in ordinary CK parameters so the
        // registered Virtools copy and destruction semantics remain in force.
        if (bind.Target.Slot.Kind == SlotKind::Local) {
            auto *stored = CKParameterLocal::Cast(targetParameter);
            if (!stored)
                return Failure(
                    Error::GraphChanged,
                    "A written value names a slot that is not a stored parameter.");
            Patch::Journal::Written change;
            change.Parameter = Capture(stored);
            change.Slot = DescribeLocal(stored);
            CKParameterLocal *before = nullptr;
            status = Parameter::Clone(m_Context, stored, before);
            if (!status)
                return status;
            change.Before = Capture(before);
            m_Journal.Values.push_back(std::move(change));
            status = Parameter::Write(m_Context, stored, bind.Value);
            if (!status)
                return status;
            CKParameterLocal *expected = nullptr;
            status = Parameter::Clone(m_Context, stored, expected);
            if (!status)
                return status;
            m_Journal.Values.back().Expected = Capture(expected);
            continue;
        }
        auto *target = CKParameterIn::Cast(targetParameter);
        if (!target)
            return Failure(Error::GraphChanged,
                           "A Bind destination is not a Pin.");

        Patch::Journal::Binding change;
        change.Input = Capture(target);
        change.Pin = DescribePin(target);
        change.PreviousDirect = Capture(target->GetDirectSource());
        change.PreviousShared = Capture(target->GetSharedSource());
        change.Before = DescribeSource(target);
        change.OwnedTarget = bind.Target.Operation;

        CKERROR error = CK_OK;
        if (bind.Kind == BindKind::Direct) {
            CKObject *sourceObject = nullptr;
            status = DataPort(bind.Source, sourceObject);
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
            status = DataPort(bind.Source, sourceParameter);
            auto *source = status ? CKParameterIn::Cast(sourceParameter) : nullptr;
            if (status && !source)
                status = Failure(Error::TypeMismatch,
                                 "A shared Bind source is not a Pin.");
            if (status)
                error = target->ShareSourceWith(source);
            change.InstalledShared = Capture(source);
        } else {
            std::ostringstream name;
            name << "__BML_Edit_" << m_Graph->GetID() << '_'
                 << bind.Ordinal;
            CKParameterLocal *literal = m_Context->CreateCKParameterLocal(
                const_cast<CKSTRING>(name.str().c_str()),
                target->GetGUID(), TRUE);
            if (!literal)
                status = Failure(Error::CreateFailed,
                                 "Virtools failed to create a Bind value.",
                                 CKERR_OUTOFMEMORY);
            if (status)
                status = Parameter::Write(m_Context, literal, bind.Value);
            if (status)
                error = target->SetDirectSource(literal);
            if (!status || error != CK_OK) {
                if (literal)
                    m_Context->DestroyObject(literal);
                return status ? Failure(Error::TypeMismatch,
                                        "Virtools rejected a Bind value.",
                                        error)
                                   : std::move(status);
            }
            change.Literal = Capture(literal);
            change.InstalledDirect = Capture(literal);
        }
        if (!status)
            return status;
        if (error != CK_OK)
            return Failure(Error::TypeMismatch,
                           "Virtools rejected a Bind relation.", error);
        change.Expected = DescribeSource(target);
        m_Journal.Binds.push_back(std::move(change));
    }

    m_Journal.Data = m_RelationLayer;
    if (!m_RelationLayer.Pins.empty()) {
        status = m_GraphRelations->Set(m_RelationLayer);
        if (!status)
            return status;
    }
    return {};
}

Status CKEdit::Transaction::Push() {
    Status status;
    for (const CheckedPush &push : m_Checked.Pushes) {
        CKObject *sourceParameter = nullptr;
        CKObject *destinationObject = nullptr;
        status = DataPort(push.Source, sourceParameter);
        if (status)
            status = DataPort(push.Destination, destinationObject);
        auto *source = status ? CKParameterOut::Cast(sourceParameter) : nullptr;
        auto *destination = status
            ? CKParameter::Cast(destinationObject) : nullptr;
        if (!status || !source || !destination)
            return status ? Failure(Error::GraphChanged,
                                    "Push requires a Pout and a stored parameter.")
                               : std::move(status);
        const CKERROR error = source->AddDestination(destination, TRUE);
        if (error != CK_OK)
            return Failure(Error::TypeMismatch,
                           "Virtools rejected a Push relation.", error);
        m_Journal.Pushes.push_back({Capture(source), Capture(destination)});
        if (!IsAdded(push.Source.Owner))
            RememberEdited(BehaviorFor(push.Source.Owner));
    }
    return {};
}

// Set changes the value currently read by an existing graph parameter;
// it does not edit a Block interface or a parameter relation. Apply it
// after all EDITED callbacks so those callbacks cannot normalize away the
// author's requested runtime value.
Status CKEdit::Transaction::Set() {
    Status status;
    std::vector<PreparedSet> preparedSets;
    preparedSets.reserve(m_Checked.Sets.size());
    std::vector<Stamp> writtenParameters;
    writtenParameters.reserve(m_Journal.Values.size() + m_Checked.Sets.size());
    for (const Patch::Journal::Written &change : m_Journal.Values)
        writtenParameters.push_back(change.Parameter);

    for (const CheckedSet &set : m_Checked.Sets) {
        CKObject *targetObject = nullptr;
        status = DataPort(set.Target, targetObject);
        if (!status)
            return status;
        CKParameter *stored = CKParameter::Cast(targetObject);
        if (auto *input = CKParameterIn::Cast(targetObject))
            stored = input->GetRealSource();
        if (!stored || CKParameterOperation::Cast(stored->GetOwner())) {
            return Failure(
                Error::SourceInvalid,
                "Set requires a stored parameter behind the selected graph port.");
        }

        CKBehavior *receiver = BehaviorFor(set.Target.Owner);
        if (!receiver)
            return Failure(
                Error::GraphChanged,
                "The Block receiving Set disappeared before Apply.",
                CKERR_INVALIDOBJECT);

        const Stamp identity = Capture(stored);
        if (std::find(writtenParameters.begin(), writtenParameters.end(), identity) !=
            writtenParameters.end()) {
            return Failure(
                Error::SourceConflict,
                "Two value edits resolve to the same stored parameter.");
        }
        writtenParameters.push_back(identity);
        preparedSets.push_back({&set, stored, receiver});
    }

    for (const PreparedSet &set : preparedSets) {
        Patch::Journal::Written change;
        change.Parameter = Capture(set.Parameter);
        change.Slot = {
            static_cast<std::uint32_t>(set.Receiver->GetID()),
            set.Checked->Target.Slot.Kind, set.Checked->Target.Slot.NativeIndex};
        CKParameterLocal *before = nullptr;
        status = Parameter::Clone(m_Context, set.Parameter, before);
        if (!status)
            return status;
        change.Before = Capture(before);
        m_Journal.Values.push_back(std::move(change));
        status = Parameter::Write(m_Context, set.Parameter, set.Checked->Value);
        if (!status)
            return status;
        CKParameterLocal *expected = nullptr;
        status = Parameter::Clone(m_Context, set.Parameter, expected);
        if (!status)
            return status;
        m_Journal.Values.back().Expected = Capture(expected);
    }
    return {};
}

} // namespace BML::Behavior::Internal

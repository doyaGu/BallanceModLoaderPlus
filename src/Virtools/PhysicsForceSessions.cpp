#include "Virtools/PhysicsForceSessions.h"

#include <utility>

#include "Virtools/CKIdentityRegistry.h"

namespace BML::Virtools {
namespace {

bool ContainsId(const CK_ID *ids, int count, std::uint32_t slot) {
    for (int i = 0; ids && i < count; ++i) {
        if (ids[i] == static_cast<CK_ID>(slot))
            return true;
    }
    return false;
}

} // namespace

PhysicsForceSessions::StoredOptions PhysicsForceSessions::Capture(
    const Presets::ForceOptions &options) const {
    StoredOptions stored;
    stored.Target = m_Identities.Make(options.Target);
    stored.Position = options.Position;
    stored.PositionReference = m_Identities.Make(options.PositionReference);
    stored.HasPositionReference = options.PositionReference != nullptr;
    stored.Direction = options.Direction;
    stored.DirectionReference = m_Identities.Make(options.DirectionReference);
    stored.HasDirectionReference = options.DirectionReference != nullptr;
    stored.Magnitude = options.Magnitude;
    return stored;
}

bool PhysicsForceSessions::Resolve(const StoredOptions &stored,
                                   Presets::ForceOptions &options) const {
    CKObject *target = m_Identities.Resolve(stored.Target);
    if (!target || !CKIsChildClassOf(target, CKCID_3DENTITY))
        return false;
    options.Target = static_cast<CK3dEntity *>(target);
    options.Position = stored.Position;
    if (stored.HasPositionReference) {
        CKObject *reference = m_Identities.Resolve(stored.PositionReference);
        if (!reference || !CKIsChildClassOf(reference, CKCID_3DENTITY))
            return false;
        options.PositionReference = static_cast<CK3dEntity *>(reference);
    }
    options.Direction = stored.Direction;
    if (stored.HasDirectionReference) {
        CKObject *reference = m_Identities.Resolve(stored.DirectionReference);
        if (!reference || !CKIsChildClassOf(reference, CKCID_3DENTITY))
            return false;
        options.DirectionReference = static_cast<CK3dEntity *>(reference);
    }
    options.Magnitude = stored.Magnitude;
    return true;
}

PhysicsForceSessions::ObjectKey PhysicsForceSessions::KeyOf(BML_ObjectRef reference) {
    return {reference.Slot, reference.Generation};
}

bool PhysicsForceSessions::HasNativeController(Session &session) const {
    BehaviorStatus status;
    CKParameter *local = m_Runtime.Parameter(
        session.Instance, SlotSelector::At(BehaviorSlotKind::Local, 0, CKPGUID_POINTER), &status);
    void *controller = nullptr;
    return status && local && local->GetValue(&controller) == CK_OK && controller != nullptr;
}

ExecutionResult PhysicsForceSessions::Accepted(const char *message) {
    BehaviorStatus status;
    status.Message = message ? message : "";
    return {std::move(status), ExecutionState::Suspended, CKBR_OK, {}};
}

ExecutionResult PhysicsForceSessions::Create(const StoredOptions &stored) {
    Presets::ForceOptions options;
    if (!Resolve(stored, options)) {
        return {BehaviorStatus{BehaviorError::SourceInvalid, CK_OK, CKBR_OK,
                               "Physics Force target or referential has expired."},
                ExecutionState::Failed, CKBR_OK, {}};
    }

    InstanceResult created = m_Runtime.Instantiate(options.Target, Presets::PhysicsForce(options));
    if (!created)
        return {std::move(created.Status), ExecutionState::Failed, CKBR_BEHAVIORERROR, {}};
    ExecutionResult result = m_Runtime.Pulse(
        created.Instance, SlotSelector::At(BehaviorSlotKind::Input, 0));

    Session session;
    session.Target = stored.Target;
    session.Instance = std::move(created.Instance);
    m_Sessions.insert_or_assign(KeyOf(stored.Target), std::move(session));
    return result;
}

ExecutionResult PhysicsForceSessions::Set(const Presets::ForceOptions &options) {
    const StoredOptions stored = Capture(options);
    if (stored.Target.Slot == 0) {
        return {BehaviorStatus{BehaviorError::TargetInvalid, CK_OK, CKBR_OK,
                               "Physics Force target is invalid."},
                ExecutionState::Failed, CKBR_OK, {}};
    }

    auto existing = m_Sessions.find(KeyOf(stored.Target));
    if (existing == m_Sessions.end())
        return Create(stored);

    if (!existing->second.Instance.Get()) {
        m_Sessions.erase(existing);
        return Create(stored);
    }
    if (HasNativeController(existing->second)) {
        ExecutionResult shutdown = m_Runtime.Pulse(
            existing->second.Instance, SlotSelector::At(BehaviorSlotKind::Input, 1));
        if (!shutdown || shutdown.State != ExecutionState::Completed)
            return shutdown;
        m_Sessions.erase(existing);
        return Create(stored);
    }
    Presets::ForceOptions pendingOptions;
    if (!Resolve(stored, pendingOptions)) {
        return {BehaviorStatus{BehaviorError::SourceInvalid, CK_OK, CKBR_OK,
                               "Physics Force target or referential has expired."},
                ExecutionState::Failed, CKBR_OK, {}};
    }
    BehaviorStatus reconfigured = m_Runtime.Reconfigure(
        existing->second.Instance, Presets::PhysicsForce(pendingOptions));
    if (!reconfigured)
        return {std::move(reconfigured), ExecutionState::Failed, CKBR_PARAMETERERROR, {}};
    existing->second.Closing = false;
    existing->second.CancellationArmed = false;
    return Accepted("Pending Physics Force updated before native controller creation.");
}

ExecutionResult PhysicsForceSessions::Clear(CK3dEntity *target) {
    const BML_ObjectRef targetRef = m_Identities.Make(target);
    auto it = m_Sessions.find(KeyOf(targetRef));
    if (targetRef.Slot == 0 || it == m_Sessions.end()) {
        return {BehaviorStatus{BehaviorError::InvalidState, CK_OK, CKBR_OK,
                               "No Physics Force session exists for the target."},
                ExecutionState::Failed, CKBR_OK, {}};
    }
    if (!it->second.Instance.Get()) {
        m_Sessions.erase(it);
        return {BehaviorStatus{BehaviorError::InvalidState, CK_OK, CKBR_OK,
                               "Physics Force instance has expired."},
                ExecutionState::Failed, CKBR_OK, {}};
    }
    if (HasNativeController(it->second)) {
        ExecutionResult shutdown = m_Runtime.Pulse(
            it->second.Instance, SlotSelector::At(BehaviorSlotKind::Input, 1));
        if (shutdown && shutdown.State == ExecutionState::Completed)
            m_Sessions.erase(it);
        return shutdown;
    }
    Presets::ForceOptions cancellation;
    BehaviorStatus cancelled = m_Runtime.Reconfigure(
        it->second.Instance, Presets::PhysicsForce(cancellation));
    if (!cancelled)
        return {std::move(cancelled), ExecutionState::Failed, CKBR_PARAMETERERROR, {}};
    it->second.Closing = true;
    it->second.CancellationArmed = true;
    it->second.CloseAfterEpoch = m_PhysicsEpoch + 1;
    return Accepted("Physics Force shutdown queued for the next physics epoch.");
}

void PhysicsForceSessions::ProcessFrame() {
    ++m_PhysicsEpoch;
    for (auto it = m_Sessions.begin(); it != m_Sessions.end();) {
        Session &session = it->second;
        if (!session.Instance.Get()) {
            it = m_Sessions.erase(it);
            continue;
        }
        if (!session.Closing || session.CloseAfterEpoch > m_PhysicsEpoch) {
            ++it;
            continue;
        }

        // A null local means PhysicsForceCallback is still owned by the
        // physics manager and still carries the CKBehavior pointer. Keep the
        // instance alive until that callback creates the controller or
        // observes an expired target and removes itself.
        if (!HasNativeController(session)) {
            if (session.CancellationArmed || !m_Identities.Resolve(session.Target)) {
                it = m_Sessions.erase(it);
            } else {
                session.CloseAfterEpoch = m_PhysicsEpoch + 1;
                ++it;
            }
            continue;
        }

        ExecutionResult shutdown = m_Runtime.Pulse(
            session.Instance, SlotSelector::At(BehaviorSlotKind::Input, 1));
        if (!shutdown || shutdown.State == ExecutionState::Continuing ||
            shutdown.State == ExecutionState::Suspended) {
            session.CloseAfterEpoch = m_PhysicsEpoch + 1;
            ++it;
            continue;
        }
        it = m_Sessions.erase(it);
    }
}

void PhysicsForceSessions::ObjectsToBeDeleted(const CK_ID *ids, int count) {
    if (!ids || count <= 0)
        return;
    for (auto &[key, session] : m_Sessions) {
        if (!ContainsId(ids, count, key.Slot))
            continue;
        session.Closing = true;
        if (session.Instance.Get() && !HasNativeController(session)) {
            Presets::ForceOptions cancellation;
            session.CancellationArmed = static_cast<bool>(m_Runtime.Reconfigure(
                session.Instance, Presets::PhysicsForce(cancellation)));
        }
        session.CloseAfterEpoch = m_PhysicsEpoch + 1;
    }
}

void PhysicsForceSessions::Reset() {
    for (auto &[key, session] : m_Sessions) {
        (void) key;
        if (session.Instance.Get()) {
            (void) m_Runtime.Pulse(
                session.Instance, SlotSelector::At(BehaviorSlotKind::Input, 1));
        }
    }
    m_Sessions.clear();
    m_PhysicsEpoch = 0;
}

} // namespace BML::Virtools

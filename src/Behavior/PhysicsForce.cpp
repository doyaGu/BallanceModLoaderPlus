#include "Behavior/PhysicsForce.h"

#include <utility>

#include "BML/Guids/physics_RT.h"

namespace BML::Behavior::PhysicsForce {
namespace {

bool ContainsId(const CK_ID *ids, int count, CK_ID id) {
    for (int i = 0; ids && i < count; ++i) {
        if (ids[i] == id)
            return true;
    }
    return false;
}

} // namespace

Spec Make(const Options &options) {
    Spec spec(PHYSICS_RT_PHYSICSFORCE);
    spec.Target(CKPGUID_3DENTITY, options.Target)
        .Input(Slot::At(SlotKind::InputParameter, 0, CKPGUID_VECTOR),
               Value::From(CKPGUID_VECTOR, options.Position))
        .Input(Slot::At(SlotKind::InputParameter, 1, CKPGUID_3DENTITY),
               Value::Object(CKPGUID_3DENTITY, options.PositionReference))
        .Input(Slot::At(SlotKind::InputParameter, 2, CKPGUID_VECTOR),
               Value::From(CKPGUID_VECTOR, options.Direction))
        .Input(Slot::At(SlotKind::InputParameter, 3, CKPGUID_3DENTITY),
               Value::Object(CKPGUID_3DENTITY, options.DirectionReference))
        .Input(Slot::At(SlotKind::InputParameter, 4, CKPGUID_FLOAT),
               Value::From(CKPGUID_FLOAT, options.Magnitude));
    return spec;
}

Sessions::StoredOptions Sessions::Capture(const Options &options) const {
    StoredOptions stored;
    stored.Target = Capture(options.Target);
    stored.Position = options.Position;
    stored.PositionReference = Capture(options.PositionReference);
    stored.HasPositionReference = options.PositionReference != nullptr;
    stored.Direction = options.Direction;
    stored.DirectionReference = Capture(options.DirectionReference);
    stored.HasDirectionReference = options.DirectionReference != nullptr;
    stored.Magnitude = options.Magnitude;
    return stored;
}

Sessions::EntityStamp Sessions::Capture(CK3dEntity *entity) const {
    if (!entity || !m_Context || entity->GetCKContext() != m_Context ||
        entity->IsToBeDeleted()) {
        return {};
    }
    const CK_ID id = entity->GetID();
    return id != 0 && CKGetObject(m_Context, id) == entity
        ? EntityStamp{id, entity} : EntityStamp{};
}

CK3dEntity *Sessions::Resolve(EntityStamp entity) const {
    if (!m_Context || entity.Id == 0 || !entity.Address)
        return nullptr;
    CKObject *current = CKGetObject(m_Context, entity.Id);
    return current == entity.Address && !current->IsToBeDeleted() &&
                   CKIsChildClassOf(current, CKCID_3DENTITY)
        ? static_cast<CK3dEntity *>(current) : nullptr;
}

bool Sessions::Resolve(const StoredOptions &stored, Options &options) const {
    CK3dEntity *target = Resolve(stored.Target);
    if (!target)
        return false;
    options.Target = target;
    options.Position = stored.Position;
    if (stored.HasPositionReference) {
        CK3dEntity *reference = Resolve(stored.PositionReference);
        if (!reference)
            return false;
        options.PositionReference = reference;
    }
    options.Direction = stored.Direction;
    if (stored.HasDirectionReference) {
        CK3dEntity *reference = Resolve(stored.DirectionReference);
        if (!reference)
            return false;
        options.DirectionReference = reference;
    }
    options.Magnitude = stored.Magnitude;
    return true;
}

Sessions::EntityKey Sessions::KeyOf(EntityStamp reference) {
    return {reference.Id, reference.Address};
}

bool Sessions::HasNativeController(Session &session) const {
    Status status;
    CKParameter *local = m_Runtime.Parameter(
        session.Block, Slot::At(SlotKind::Local, 0, CKPGUID_POINTER), &status);
    void *controller = nullptr;
    return status && local && local->GetValue(&controller) == CK_OK && controller != nullptr;
}

RunResult Sessions::Accepted(const char *message) {
    Status status;
    status.Message = message ? message : "";
    return {std::move(status), RunState::Pending, CKBR_OK, {}};
}

RunResult Sessions::Create(const StoredOptions &stored) {
    Options options;
    if (!Resolve(stored, options)) {
        return {Status{Error::SourceInvalid, CK_OK, CKBR_OK,
                       "Physics Force target or referential has expired."},
                RunState::Failed, CKBR_OK, {}};
    }

    CreateResult created = m_Runtime.Instantiate(options.Target, Make(options));
    if (!created)
        return {std::move(created.Detail), RunState::Failed, CKBR_BEHAVIORERROR, {}};
    RunResult result = m_Runtime.Pulse(
        created.Handle, Slot::At(SlotKind::Input, 0));

    Session session;
    session.Target = stored.Target;
    session.Block = std::move(created.Handle);
    m_Sessions.insert_or_assign(KeyOf(stored.Target), std::move(session));
    return result;
}

RunResult Sessions::Set(const Options &options) {
    const StoredOptions stored = Capture(options);
    if (stored.Target.Id == 0) {
        return {Status{Error::TargetInvalid, CK_OK, CKBR_OK,
                       "Physics Force target is invalid."},
                RunState::Failed, CKBR_OK, {}};
    }

    auto existing = m_Sessions.find(KeyOf(stored.Target));
    if (existing == m_Sessions.end())
        return Create(stored);

    if (!existing->second.Block.Get()) {
        m_Sessions.erase(existing);
        return Create(stored);
    }
    if (HasNativeController(existing->second)) {
        RunResult shutdown = m_Runtime.Pulse(
            existing->second.Block, Slot::At(SlotKind::Input, 1));
        if (!shutdown || shutdown.State != RunState::Ready)
            return shutdown;
        m_Sessions.erase(existing);
        return Create(stored);
    }
    Options pendingOptions;
    if (!Resolve(stored, pendingOptions)) {
        return {Status{Error::SourceInvalid, CK_OK, CKBR_OK,
                       "Physics Force target or referential has expired."},
                RunState::Failed, CKBR_OK, {}};
    }
    Status reconfigured = m_Runtime.Reconfigure(
        existing->second.Block, Make(pendingOptions));
    if (!reconfigured)
        return {std::move(reconfigured), RunState::Failed, CKBR_PARAMETERERROR, {}};
    existing->second.Closing = false;
    existing->second.CancellationArmed = false;
    return Accepted("Pending Physics Force updated before native controller creation.");
}

RunResult Sessions::Clear(CK3dEntity *target) {
    const EntityStamp targetRef = Capture(target);
    auto it = m_Sessions.find(KeyOf(targetRef));
    if (targetRef.Id == 0 || it == m_Sessions.end()) {
        return {Status{Error::InvalidState, CK_OK, CKBR_OK,
                       "No Physics Force session exists for the target."},
                RunState::Failed, CKBR_OK, {}};
    }
    if (!it->second.Block.Get()) {
        m_Sessions.erase(it);
        return {Status{Error::InvalidState, CK_OK, CKBR_OK,
                       "Physics Force instance has expired."},
                RunState::Failed, CKBR_OK, {}};
    }
    if (HasNativeController(it->second)) {
        RunResult shutdown = m_Runtime.Pulse(
            it->second.Block, Slot::At(SlotKind::Input, 1));
        if (shutdown && shutdown.State == RunState::Ready)
            m_Sessions.erase(it);
        return shutdown;
    }
    Options cancellation;
    Status cancelled = m_Runtime.Reconfigure(
        it->second.Block, Make(cancellation));
    if (!cancelled)
        return {std::move(cancelled), RunState::Failed, CKBR_PARAMETERERROR, {}};
    it->second.Closing = true;
    it->second.CancellationArmed = true;
    it->second.CloseAfterEpoch = m_PhysicsEpoch + 1;
    return Accepted("Physics Force shutdown queued for the next physics epoch.");
}

void Sessions::ProcessFrame() {
    ++m_PhysicsEpoch;
    auto keep = [&](Session &session) {
        if (!session.Block.Get())
            return false;
        if (!session.Closing || session.CloseAfterEpoch > m_PhysicsEpoch)
            return true;

        // A null local means the physics manager still owns a callback carrying
        // the CKBehavior pointer. Keep the instance until that callback runs.
        if (!HasNativeController(session)) {
            if (session.CancellationArmed || !Resolve(session.Target))
                return false;
            session.CloseAfterEpoch = m_PhysicsEpoch + 1;
            return true;
        }

        RunResult shutdown = m_Runtime.Pulse(
            session.Block, Slot::At(SlotKind::Input, 1));
        if (!shutdown || shutdown.State == RunState::Pending) {
            session.CloseAfterEpoch = m_PhysicsEpoch + 1;
            return true;
        }
        return false;
    };

    for (auto it = m_Sessions.begin(); it != m_Sessions.end();) {
        if (!keep(it->second))
            it = m_Sessions.erase(it);
        else
            ++it;
    }
    for (auto it = m_Retiring.begin(); it != m_Retiring.end();) {
        if (!keep(*it))
            it = m_Retiring.erase(it);
        else
            ++it;
    }
}

void Sessions::ObjectsToBeDeleted(const CK_ID *ids, int count) {
    if (!ids || count <= 0)
        return;
    for (auto it = m_Sessions.begin(); it != m_Sessions.end();) {
        if (!ContainsId(ids, count, it->first.Id)) {
            ++it;
            continue;
        }
        Session &session = it->second;
        session.Closing = true;
        if (session.Block.Get() && !HasNativeController(session)) {
            Options cancellation;
            session.CancellationArmed = static_cast<bool>(m_Runtime.Reconfigure(
                session.Block, Make(cancellation)));
        }
        session.CloseAfterEpoch = m_PhysicsEpoch + 1;
        m_Retiring.push_back(std::move(session));
        it = m_Sessions.erase(it);
    }
}

void Sessions::Reset() {
    auto shutdown = [&](Session &session) {
        if (session.Block.Get()) {
            (void) m_Runtime.Pulse(
                session.Block, Slot::At(SlotKind::Input, 1));
        }
    };
    for (auto &[key, session] : m_Sessions) {
        (void) key;
        shutdown(session);
    }
    for (Session &session : m_Retiring)
        shutdown(session);
    m_Sessions.clear();
    m_Retiring.clear();
    m_PhysicsEpoch = 0;
}

} // namespace BML::Behavior::PhysicsForce

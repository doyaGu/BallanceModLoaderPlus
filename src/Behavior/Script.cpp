#include "Behavior/Script.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "Behavior/Callback.h"
#include "Behavior/GraphEdit.h"
#include "Behavior/Patches.h"
#include "CKAll.h"

namespace BML::Behavior::Internal {
namespace {

Status Failure(Error error, std::string message, Phase phase,
               CKERROR ckError = CKERR_INVALIDOBJECT) {
    Status status{error, ckError, CKBR_PARAMETERERROR, std::move(message)};
    status.Details.Stage = phase;
    return status;
}

#ifndef BML_BEHAVIOR_SCRIPT_DOMAIN_ONLY

template <class T>
T *Resolve(CKContext *context, const ScriptObject &object,
           CK_CLASSID classId) {
    if (!context || !object.Id ||
        object.Id > static_cast<std::uint64_t>(
            (std::numeric_limits<CK_ID>::max)()))
        return nullptr;
    CKObject *live = context->GetObject(static_cast<CK_ID>(object.Id));
    if (!live || live->IsToBeDeleted() ||
        reinterpret_cast<std::uintptr_t>(live) != object.Address ||
        !CKIsChildClassOf(live, classId))
        return nullptr;
    return static_cast<T *>(live);
}

class CKScriptWorld final : public ScriptWorld {
public:
    CKScriptWorld(CKContext *context, Patches &patches,
                  std::function<ObjectRef(const void *)> issueObjectRef)
        : m_Context(context), m_Patches(patches),
          m_IssueObjectRef(std::move(issueObjectRef)) {}

    bool InDispatch() const noexcept override {
        if (CallbackInvocation::Active())
            return true;
        CKBehaviorManager *manager = m_Context
            ? m_Context->GetBehaviorManager() : nullptr;
        return manager && manager->m_CurrentBehavior != nullptr;
    }

    Status Create(void *nativeOwner, std::string_view name, int priority,
                  ScriptIdentity &out) override {
        out = {};
        if (!m_Context)
            return Failure(Error::ContextExpired,
                           "Virtools context is unavailable.",
                           Phase::Creation);
        CKScene *scene = m_Context->GetCurrentScene();
        auto *owner = static_cast<CKBeObject *>(nativeOwner);
        if (!owner || m_Context->GetObject(owner->GetID()) != owner ||
            owner->IsToBeDeleted() ||
            !CKIsChildClassOf(owner, CKCID_BEOBJECT)) {
            return Failure(Error::OwnerInvalid,
                           "A top-level Script requires a live CKBeObject owner.",
                           Phase::OwnerBinding);
        }
        if (!scene || !owner->IsInScene(scene)) {
            return Failure(
                Error::OwnerInvalid,
                "The Script owner does not belong to the current Virtools Scene.",
                Phase::OwnerBinding);
        }

        std::string nativeName(name);
        auto *root = CKBehavior::Cast(m_Context->CreateObject(
            CKCID_BEHAVIOR,
            nativeName.empty() ? nullptr
                               : const_cast<CKSTRING>(nativeName.c_str()),
            CK_OBJECTCREATION_DYNAMIC));
        if (!root)
            return Failure(Error::CreateFailed,
                           "Virtools could not create the Script root.",
                           Phase::Creation, CKERR_OUTOFMEMORY);

        const auto discard = [&] {
            CKObject *live = m_Context->GetObject(root->GetID());
            if (live != root || live->IsToBeDeleted())
                return;
            CKBeObject *currentOwner = root->GetOwner();
            if (currentOwner && !currentOwner->IsToBeDeleted())
                (void) currentOwner->RemoveScript(root->GetID());
            (void) m_Context->DestroyObject(root);
        };

        root->UseGraph();
        root->SetType(CKBEHAVIORTYPE_SCRIPT);
        root->SetPriority(priority);
        const CKERROR attached = owner->AddScript(root);
        if (attached != CK_OK) {
            discard();
            return Failure(Error::OwnerInvalid,
                           "Virtools rejected the Script owner relation.",
                           Phase::OwnerBinding, attached);
        }
        if (root->GetOwner() != owner ||
            root->GetType() != CKBEHAVIORTYPE_SCRIPT ||
            root->IsUsingFunction() || !root->IsInScene(scene)) {
            discard();
            return Failure(
                Error::GraphChanged,
                "Virtools did not preserve the top-level Script relation.",
                Phase::OwnerBinding);
        }

        out.Root = Capture(root);
        out.Owner = Capture(owner);
        out.Scene = Capture(scene);
        if (!out.Root || !out.Owner || !out.Scene) {
            discard();
            out = {};
            return Failure(Error::CreateFailed,
                           "The Loader could not issue Script object references.",
                           Phase::Creation);
        }
        return {};
    }

    Status Define(const SessionOwner &owner,
                  const ScriptIdentity &script,
                  GraphEdit body, ScriptBodyId &out) override {
        out = 0;
        PatchId patch = 0;
        Status status = m_Patches.Apply(
            owner, script.Root.Reference,
            "Script/" + std::to_string(script.Root.Id),
            std::move(body), patch);
        if (!status)
            return status;
        if (!patch)
            return Failure(Error::CreateFailed,
                           "The Loader did not retain the Script graph.",
                           Phase::Edit);
        out = patch;
        PatchInfo info;
        status = m_Patches.Read(owner, patch, info);
        if (!status)
            return status;
        if (info.State != PatchState::Active) {
            return Failure(
                Error::Busy,
                "The initial Script graph cannot be published from inside another Behavior edit.",
                Phase::Edit);
        }
        return {};
    }

    Status Read(const ScriptIdentity &script, bool &active) override {
        active = false;
        CKBehavior *root = Resolve<CKBehavior>(
            m_Context, script.Root, CKCID_BEHAVIOR);
        CKBeObject *owner = Resolve<CKBeObject>(
            m_Context, script.Owner, CKCID_BEOBJECT);
        CKScene *scene = Resolve<CKScene>(
            m_Context, script.Scene, CKCID_SCENE);
        if (!root || !owner || !scene || root->GetOwner() != owner ||
            root->GetType() != CKBEHAVIORTYPE_SCRIPT ||
            root->IsUsingFunction() || !root->IsInScene(scene) ||
            !Contains(owner, root)) {
            return Failure(Error::GraphChanged,
                           "The top-level Script identity or owner relation changed.",
                           Phase::OwnerBinding);
        }
        active = scene->IsObjectActive(root) != FALSE;
        return {};
    }

    Status SetActive(const ScriptIdentity &script, bool active,
                     bool reset) override {
        bool current = false;
        Status status = Read(script, current);
        if (!status ||
            (current == active && !(active && reset)))
            return status;
        CKBehavior *root = Resolve<CKBehavior>(
            m_Context, script.Root, CKCID_BEHAVIOR);
        CKScene *scene = Resolve<CKScene>(
            m_Context, script.Scene, CKCID_SCENE);
        if (!root || !scene)
            return Failure(Error::GraphChanged,
                           "The Script or its Scene disappeared before activation.",
                           Phase::Execution);
        if (active)
            scene->Activate(root, reset ? TRUE : FALSE);
        else
            scene->DeActivate(root);
        status = Read(script, current);
        if (!status)
            return status;
        if (current != active)
            return Failure(Error::ExecutionFailed,
                           "Virtools did not apply the requested Script activity.",
                           Phase::Execution, CKERR_INVALIDOPERATION);
        return {};
    }

    Status CloseBody(const SessionOwner &owner,
                     ScriptBodyId body) override {
        return m_Patches.Close(owner, static_cast<PatchId>(body));
    }

    Status Destroy(const ScriptIdentity &script) override {
        CKBehavior *root = Resolve<CKBehavior>(
            m_Context, script.Root, CKCID_BEHAVIOR);
        if (!root)
            return {};

        Status first;
        CKScene *scene = Resolve<CKScene>(
            m_Context, script.Scene, CKCID_SCENE);
        if (scene && root->IsInScene(scene) && scene->IsObjectActive(root))
            scene->DeActivate(root);

        root = Resolve<CKBehavior>(m_Context, script.Root, CKCID_BEHAVIOR);
        if (!root)
            return {};
        CKBeObject *owner = Resolve<CKBeObject>(
            m_Context, script.Owner, CKCID_BEOBJECT);
        if (owner && root->GetOwner() == owner &&
            !owner->RemoveScript(root->GetID())) {
            first = Failure(Error::OwnerInvalid,
                            "Virtools could not detach the Script from its owner.",
                            Phase::Teardown, CKERR_INVALIDOPERATION);
        }

        root = Resolve<CKBehavior>(m_Context, script.Root, CKCID_BEHAVIOR);
        if (root) {
            const CKERROR destroyed = m_Context->DestroyObject(root);
            if (destroyed != CK_OK && first)
                first = Failure(Error::ExecutionFailed,
                                "Virtools could not destroy the Script root.",
                                Phase::Teardown, destroyed);
        }
        if (Resolve<CKBehavior>(m_Context, script.Root, CKCID_BEHAVIOR)) {
            if (first)
                first = Failure(Error::ExecutionFailed,
                                "The Script root remains live after teardown.",
                                Phase::Teardown, CKERR_INVALIDOBJECT);
            return first;
        }
        return {};
    }

private:
    ScriptObject Capture(CKObject *object) const {
        if (!object || !m_IssueObjectRef)
            return {};
        const ObjectRef reference = m_IssueObjectRef(object);
        if (reference.IsNull())
            return {};
        return {static_cast<std::uint32_t>(object->GetID()),
                reinterpret_cast<std::uintptr_t>(object), reference};
    }

    static bool Contains(CKBeObject *owner, CKBehavior *script) {
        if (!owner || !script)
            return false;
        for (int index = 0; index < owner->GetScriptCount(); ++index) {
            if (owner->GetScript(index) == script)
                return true;
        }
        return false;
    }

    CKContext *m_Context = nullptr;
    Patches &m_Patches;
    std::function<ObjectRef(const void *)> m_IssueObjectRef;
};

#endif // BML_BEHAVIOR_SCRIPT_DOMAIN_ONLY

} // namespace

Scripts::Scripts(std::unique_ptr<ScriptWorld> world, Loaded loaded)
    : m_World(std::move(world)), m_Loaded(std::move(loaded)),
      m_Thread(std::this_thread::get_id()) {}

Scripts::~Scripts() {
    ResetWorld();
}

Status Scripts::Ready() const {
    if (std::this_thread::get_id() != m_Thread)
        return Failure(Error::WrongThread,
                       "Behavior Scripts require the game thread.",
                       Phase::Creation);
    if (!m_World)
        return Failure(Error::ContextExpired,
                       "The Virtools Script world is unavailable.",
                       Phase::Creation);
    return {};
}

ScriptId Scripts::NextId() noexcept {
    if (m_NextId == 0 ||
        m_NextId == (std::numeric_limits<ScriptId>::max)())
        return 0;
    return m_NextId++;
}

bool Scripts::OwnedBy(const Entry &entry,
                      const SessionOwner &owner) const noexcept {
    return entry.Owner.Id == owner.Id &&
           entry.Owner.Generation == owner.Generation;
}

ScriptResult Scripts::Create(const SessionOwner &owner,
                             std::uintptr_t session,
                             void *nativeOwner, std::string name,
                             int priority, GraphEdit body) {
    Status status = Ready();
    if (!status)
        return {std::move(status), 0, {}};
    if (!owner || !session)
        return {Failure(Error::OwnerInvalid,
                        "A top-level Script requires an active Mod Session.",
                        Phase::OwnerBinding), 0, {}};
    if (m_World->InDispatch())
        return {Failure(Error::Busy,
                        "A top-level Script cannot be created during Behavior dispatch.",
                        Phase::Creation), 0, {}};

    const ScriptId id = NextId();
    if (!id)
        return {Failure(Error::InvalidState,
                        "Behavior Script handles are exhausted.",
                        Phase::Creation), 0, {}};

    ScriptIdentity identity;
    status = m_World->Create(nativeOwner, name, priority, identity);
    if (!status)
        return {std::move(status), 0, {}};

    auto entry = std::make_shared<Entry>();
    entry->Id = id;
    entry->Session = session;
    entry->Owner = owner;
    entry->Info.Identity = identity;
    entry->Info.Priority = priority;

    ScriptBodyId scriptBody = 0;
    status = m_World->Define(owner, identity, std::move(body), scriptBody);
    entry->Body = scriptBody;
    if (status) {
        try {
            // CloseSession uses this same registry lock. It must either see
            // this Entry or revoke its Session before we admit it.
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            if (!owner)
                status = Failure(Error::OwnerInvalid,
                                 "The Behavior Session closed while creating the Script.",
                                 Phase::Creation);
            else
                m_Scripts.emplace(id, entry);
        } catch (...) {
            status = Failure(Error::CreateFailed,
                             "The Loader could not retain the new Script.",
                             Phase::Creation);
        }
    }
    if (!status) {
        const Status rejected = status;
        entry->Info.State = ScriptState::Closing;
        entry->Info.LastStatus = rejected;
        entry->Body = scriptBody;
        const Status retired = Retire(*entry);
        if (!retired) {
            entry->Info.LastStatus = retired;
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            m_Retiring.push_back(entry);
        }
        return {rejected, 0, {}};
    }

    try {
        if (m_Loaded)
            m_Loaded(name, identity.Root.Reference);
    } catch (...) {
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            Close(*entry);
        }
        Process(entry);
        return {Failure(Error::CreateFailed,
                        "The Loader could not publish the new Script.",
                        Phase::Creation), 0, {}};
    }
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return {{}, id, entry->Info};
}

Status Scripts::Read(const SessionOwner &owner, ScriptId script,
                     ScriptInfo &out) {
    out = {};
    Status ready = Ready();
    if (!ready)
        return ready;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Scripts.find(script);
    if (found == m_Scripts.end())
        return Failure(Error::InvalidState,
                       "The Behavior Script handle is stale.",
                       Phase::OwnerBinding);
    if (!OwnedBy(*found->second, owner))
        return Failure(Error::OwnerInvalid,
                       "The Behavior Script belongs to another Mod generation.",
                       Phase::OwnerBinding);
    Entry &entry = *found->second;
    if (entry.Info.State == ScriptState::Ready) {
        bool active = false;
        Status observed = m_World->Read(entry.Info.Identity, active);
        if (!observed) {
            entry.Info.State = ScriptState::Failed;
            entry.Info.LastStatus = std::move(observed);
            entry.Info.RequestedActive = entry.Info.Active;
            entry.ResetOnActivation = false;
        } else {
            entry.Info.Active = active;
        }
    }
    out = entry.Info;
    return {};
}

Status Scripts::SetActive(const SessionOwner &owner, ScriptId script,
                          bool active, bool reset, ScriptInfo &out) {
    out = {};
    Status ready = Ready();
    if (!ready)
        return ready;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Scripts.find(script);
    if (found == m_Scripts.end())
        return Failure(Error::InvalidState,
                       "The Behavior Script handle is stale.",
                       Phase::Execution);
    Entry &entry = *found->second;
    if (!OwnedBy(entry, owner))
        return Failure(Error::OwnerInvalid,
                       "The Behavior Script belongs to another Mod generation.",
                       Phase::OwnerBinding);
    if (entry.Info.State != ScriptState::Ready)
        return Failure(Error::InvalidState,
                       "The Behavior Script no longer accepts activity changes.",
                       Phase::Execution);
    entry.Info.RequestedActive = active;
    entry.ResetOnActivation = active && reset;
    out = entry.Info;
    return {};
}

void Scripts::Close(Entry &entry) noexcept {
    if (entry.Info.State == ScriptState::Closing)
        return;
    entry.Info.State = ScriptState::Closing;
    entry.Info.RequestedActive = false;
    entry.ResetOnActivation = false;
}

Status Scripts::Retire(Entry &entry) {
    if (entry.Body) {
        Status status = m_World->CloseBody(entry.Owner, entry.Body);
        if (!status)
            return status;
        entry.Body = 0;
    }
    return m_World->Destroy(entry.Info.Identity);
}

Status Scripts::Close(const SessionOwner &owner, ScriptId script) {
    if (!script)
        return Failure(Error::InvalidState,
                       "The Behavior Script handle is null.",
                       Phase::Teardown);
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Scripts.find(script);
    if (found == m_Scripts.end())
        return {};
    if (!OwnedBy(*found->second, owner))
        return Failure(Error::OwnerInvalid,
                       "The Behavior Script belongs to another Mod generation.",
                       Phase::OwnerBinding);
    Close(*found->second);
    return Failure(Error::Busy,
                   "The Behavior Script will close at the next safe point.",
                   Phase::Teardown, CK_OK);
}

void Scripts::CloseSession(std::uintptr_t session) {
    if (!session)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    for (auto &[id, entry] : m_Scripts) {
        if (entry->Session == session)
            Close(*entry);
    }
}

Status Scripts::RetireOwner(std::string_view owner) {
    Status ready = Ready();
    if (!ready)
        return ready;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        for (auto &[id, entry] : m_Scripts) {
            if (entry->Owner.Id == owner)
                Close(*entry);
        }
    }
    ProcessFrame();
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    for (const auto &[id, entry] : m_Scripts) {
        if (entry->Owner.Id == owner)
            return Failure(Error::Busy,
                           "A top-level Script is still Closing.",
                           Phase::Teardown);
    }
    for (const std::shared_ptr<Entry> &entry : m_Retiring) {
        if (entry->Owner.Id == owner)
            return Failure(
                Error::Busy,
                "An unpublished top-level Script is still Closing.",
                Phase::Teardown);
    }
    return {};
}

void Scripts::ObjectToBeDeleted(std::uint64_t object) {
    if (!object || std::this_thread::get_id() != m_Thread)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    for (auto script = m_Scripts.begin(); script != m_Scripts.end();) {
        const ScriptIdentity &identity = script->second->Info.Identity;
        const bool deleting = object == identity.Root.Id ||
                              object == identity.Owner.Id ||
                              object == identity.Scene.Id;
        if (deleting)
            script = m_Scripts.erase(script);
        else
            ++script;
    }
    m_Retiring.erase(
        std::remove_if(
            m_Retiring.begin(), m_Retiring.end(),
            [object](const std::shared_ptr<Entry> &entry) {
                const ScriptIdentity &identity = entry->Info.Identity;
                return object == identity.Root.Id ||
                    object == identity.Owner.Id ||
                    object == identity.Scene.Id;
            }),
        m_Retiring.end());
}

void Scripts::Process(const std::shared_ptr<Entry> &entry) {
    ScriptInfo snapshot;
    bool reset = false;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        const auto current = m_Scripts.find(entry->Id);
        if (current == m_Scripts.end() || current->second != entry)
            return;
        snapshot = entry->Info;
        reset = entry->ResetOnActivation;
    }

    if (snapshot.State == ScriptState::Closing) {
        const Status status = Retire(*entry);
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        const auto current = m_Scripts.find(entry->Id);
        if (current == m_Scripts.end() || current->second != entry)
            return;
        if (status)
            m_Scripts.erase(current);
        else
            entry->Info.LastStatus = status;
        return;
    }
    if (snapshot.State != ScriptState::Ready)
        return;

    bool active = false;
    Status observed = m_World->Read(snapshot.Identity, active);
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        const auto current = m_Scripts.find(entry->Id);
        if (current == m_Scripts.end() || current->second != entry ||
            entry->Info.State != ScriptState::Ready)
            return;
        if (!observed) {
            entry->Info.State = ScriptState::Failed;
            entry->Info.LastStatus = std::move(observed);
            entry->Info.RequestedActive = entry->Info.Active;
            entry->ResetOnActivation = false;
            return;
        }
        entry->Info.Active = active;
        snapshot = entry->Info;
        reset = entry->ResetOnActivation;
    }
    if (snapshot.Active == snapshot.RequestedActive && !reset)
        return;

    const Status status = m_World->SetActive(
        snapshot.Identity, snapshot.RequestedActive, reset);
    active = snapshot.Active;
    observed = status;
    if (observed)
        observed = m_World->Read(snapshot.Identity, active);

    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto current = m_Scripts.find(entry->Id);
    if (current == m_Scripts.end() || current->second != entry)
        return;
    if (entry->Info.State != ScriptState::Ready)
        return;
    if (!observed) {
        entry->Info.State = ScriptState::Failed;
        entry->Info.LastStatus = observed;
        entry->Info.RequestedActive = entry->Info.Active;
        entry->ResetOnActivation = false;
        return;
    }
    entry->Info.Active = active;
    entry->Info.LastStatus = {};
    entry->ResetOnActivation = false;
}

void Scripts::ProcessFrame() {
    if (!Ready())
        return;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        if (m_Processing)
            return;
        m_Processing = true;
        m_FrameEntries.clear();
        m_FrameEntries.reserve(m_Scripts.size());
        for (const auto &[id, entry] : m_Scripts)
            m_FrameEntries.push_back(entry);
    }
    try {
        for (const std::shared_ptr<Entry> &entry : m_FrameEntries)
            Process(entry);
        std::size_t index = 0;
        while (true) {
            std::shared_ptr<Entry> entry;
            {
                std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                if (index >= m_Retiring.size())
                    break;
                entry = m_Retiring[index];
            }
            const Status status = Retire(*entry);
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            const auto found = std::find(
                m_Retiring.begin(), m_Retiring.end(), entry);
            if (found == m_Retiring.end())
                continue;
            if (status)
                m_Retiring.erase(found);
            else {
                entry->Info.LastStatus = status;
                index = static_cast<std::size_t>(
                    std::distance(m_Retiring.begin(), found)) + 1;
            }
        }
    } catch (...) {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_FrameEntries.clear();
        m_Processing = false;
        throw;
    }
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_FrameEntries.clear();
        m_Processing = false;
    }
}

void Scripts::ResetWorld() {
    if (std::this_thread::get_id() != m_Thread)
        return;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        for (auto &[id, entry] : m_Scripts)
            Close(*entry);
    }
    ProcessFrame();
    // Object references are about to enter a new world. Any native teardown
    // failure is no longer a usable Script handle in that world.
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_Scripts.clear();
    m_Retiring.clear();
}

#ifndef BML_BEHAVIOR_SCRIPT_DOMAIN_ONLY

std::unique_ptr<ScriptWorld> MakeCKScriptWorld(
    CKContext *context, Patches &patches,
    std::function<ObjectRef(const void *)> issueObjectRef) {
    return std::make_unique<CKScriptWorld>(
        context, patches, std::move(issueObjectRef));
}

#endif // BML_BEHAVIOR_SCRIPT_DOMAIN_ONLY

} // namespace BML::Behavior::Internal

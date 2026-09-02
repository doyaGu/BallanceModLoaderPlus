#include "Behavior/Patches.h"

#include <limits>
#include <set>
#include <utility>

namespace BML::Behavior {
namespace {

Status Failure(Error error, std::string message,
               Phase phase = Phase::Edit) {
    Status status{error, CKERR_INVALIDPARAMETER, CKBR_PARAMETERERROR,
                  std::move(message)};
    status.Details.Stage = phase;
    return status;
}

} // namespace

Patches::Patches(CKContext *context, Runtime &runtime,
                 PrototypeCatalog *catalog, GraphSource &graph,
                 ResolveObject resolveObject)
    : m_Edit(context, runtime, catalog, graph),
      m_Graph(graph), m_ResolveObject(std::move(resolveObject)),
      m_Thread(std::this_thread::get_id()) {}

Patches::~Patches() {
    ResetWorld();
}

Status Patches::Ready() const {
    return std::this_thread::get_id() == m_Thread
        ? Status{}
        : Failure(Error::WrongThread,
                  "Behavior Patches require the game thread.");
}

PatchId Patches::NextId() {
    if (m_NextId == 0 ||
        m_NextId == (std::numeric_limits<PatchId>::max)())
        return 0;
    return m_NextId++;
}

Status Patches::Begin(const SessionOwner &owner, CKBehavior *graph,
                      std::string name, Edit &out) {
    Status status = Ready();
    if (!status)
        return status;
    if (!owner || name.empty())
        return Failure(Error::InvalidState,
                       "A Behavior Patch requires a live owner and name.");
    return m_Edit.Begin(graph, {owner.Id, std::move(name)}, out);
}

Status Patches::Use(Edit &edit, CKBehavior *behavior, Node &out) {
    return m_Edit.Use(edit, behavior, out);
}

Status Patches::Use(Edit &edit, CKBehaviorLink *link, Link &out) {
    return m_Edit.Use(edit, link, out);
}

Status Patches::Add(Edit &edit, Spec block, Node &out) {
    return m_Edit.Add(edit, std::move(block), out);
}

Status Patches::Apply(const SessionOwner &owner, const Edit &edit,
                      PatchId &out) {
    out = 0;
    Status status = Ready();
    if (!status)
        return status;
    if (!owner || edit.Key().Owner != owner.Id || edit.Key().Name.empty())
        return Failure(Error::OwnerInvalid,
                       "The Behavior Edit does not belong to this Mod generation.");

    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const PatchId id = NextId();
    if (!id)
        return Failure(Error::InvalidState,
                       "Behavior Patch ids are exhausted.");

    const NativeRef graph = edit.GraphRef();
    if (!graph || graph.Id >
            static_cast<std::uint64_t>((std::numeric_limits<CK_ID>::max)())) {
        return Failure(Error::InvalidGraphLocality,
                       "The Behavior Edit graph has no live CK identity.");
    }

    Patch patch;
    status = m_Edit.Apply(edit, patch);
    if (!patch)
        return status;

    try {
        auto [stored, inserted] = m_Patches.try_emplace(id);
        if (!inserted) {
            (void) m_Edit.Close(patch);
            return Failure(Error::InvalidState,
                           "Behavior Patch id collision.");
        }
        try {
            stored->second.Id = id;
            stored->second.Owner = owner;
            stored->second.Graph = static_cast<CK_ID>(graph.Id);
            stored->second.Value = std::move(patch);
        } catch (...) {
            m_Patches.erase(stored);
            throw;
        }
    } catch (...) {
        (void) m_Edit.Close(patch);
        return Failure(Error::CreateFailed,
                       "The Loader could not retain the Behavior Patch.");
    }
    // A failed Apply normally rolls back completely and has no Patch value.
    // RevertConflict is different: retain its conflict journal under the owner,
    // but do not hand a successful installation id to the caller.
    if (status)
        out = id;
    return status;
}

class Patches::PlanWorld final : public Plan::World {
public:
    PlanWorld(Patches &patches, SessionOwner owner, GraphEdit edit)
        : m_Patches(patches), m_Owner(std::move(owner)),
          m_Edit(std::move(edit)) {}

    Status Install(const PatchKey &key, const ObjectRef &target, Epoch,
                   Installation &out) override {
        out = 0;
        PatchId patch = 0;
        Status status = m_Patches.Install(
            m_Owner, key, target, m_Edit, patch);
        if (status)
            out = static_cast<Installation>(patch);
        return status;
    }

    Status Close(Installation installation) override {
        return m_Patches.Close(
            m_Owner, static_cast<PatchId>(installation));
    }

private:
    Patches &m_Patches;
    SessionOwner m_Owner;
    GraphEdit m_Edit;
};

Status Patches::Submit(Plans &plans, const SessionOwner &owner,
                       Script target, std::string name, GraphEdit edit,
                       PlanId &out) {
    out = 0;
    Status status = Ready();
    if (!status)
        return status;
    if (!owner || !target || name.empty())
        return Failure(Error::OwnerInvalid,
                       "A durable Behavior Edit requires an owner, script, and patch name.");
    status = edit.Validate();
    if (!status)
        return status;
    try {
        auto world = std::make_shared<PlanWorld>(
            *this, owner, std::move(edit));
        return plans.Submit(
            {owner.Id, std::move(name)}, owner.Generation, std::move(target),
            std::move(world), out);
    } catch (...) {
        return Failure(Error::CreateFailed,
                       "The Loader could not retain the durable Behavior Edit.");
    }
}

Status Patches::Install(const SessionOwner &owner, const PatchKey &patch,
                        const ObjectRef &graph, const GraphEdit &edit,
                        PatchId &out) {
    out = 0;
    Edit resolved;
    Status status = edit.Compile(patch, graph, *this, resolved);
    if (status)
        status = Apply(owner, resolved, out);
    return status;
}

Status Patches::Begin(const PatchKey &patch, const ObjectRef &graph,
                      Edit &out, GraphModel &base) {
    out = {};
    base = {};
    CKObject *object = m_ResolveObject ? m_ResolveObject(graph) : nullptr;
    CKBehavior *behavior = object ? CKBehavior::Cast(object) : nullptr;
    if (!behavior)
        return Failure(Error::TargetInvalid,
                       "A durable Behavior Edit target script is stale.");
    Status status = m_Edit.Begin(behavior, patch, out);
    if (status)
        status = m_Graph.Read(out.GraphRef(), GraphView::Logical, base);
    return status;
}

Status Patches::UseNode(Edit &edit, const ObjectRef &node, Node &out) {
    out = {};
    CKObject *object = m_ResolveObject ? m_ResolveObject(node) : nullptr;
    CKBehavior *behavior = object ? CKBehavior::Cast(object) : nullptr;
    return behavior
        ? m_Edit.Use(edit, behavior, out)
        : Failure(Error::GraphChanged,
                  "A durable Behavior Node disappeared during compilation.");
}

Status Patches::UseLink(Edit &edit, const ObjectRef &link, Link &out) {
    out = {};
    CKObject *object = m_ResolveObject ? m_ResolveObject(link) : nullptr;
    CKBehaviorLink *native = object ? CKBehaviorLink::Cast(object) : nullptr;
    return native
        ? m_Edit.Use(edit, native, out)
        : Failure(Error::GraphChanged,
                  "A durable Behavior Link disappeared during compilation.");
}

Status Patches::Add(Edit &edit, CKGUID prototype, Node &out) {
    return m_Edit.Add(edit, Spec(prototype), out);
}

Status Patches::Tap(Edit &edit, Port source,
                    const HookBlock::Hook &hook) {
    std::shared_ptr<HookBlock::Binding> binding = hook.Bind();
    if (!binding) {
        return Failure(Error::CallbackFailed,
                       "The durable Tap callback is no longer available.");
    }
    edit.Tap(std::move(source), std::move(binding));
    return {};
}

Status Patches::After(Edit &edit, Link link,
                      const HookBlock::Hook &hook) {
    std::shared_ptr<HookBlock::Binding> binding = hook.Bind();
    if (!binding) {
        return Failure(Error::CallbackFailed,
                       "The durable Path callback is no longer available.");
    }
    Node block;
    Status status = m_Edit.Add(
        edit, HookBlock::Make(std::move(binding), 1, 1), block);
    if (status)
        edit.Splice(link, block);
    return status;
}

Status Patches::Read(const SessionOwner &owner, PatchId patch,
                     PatchInfo &out) const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Patches.find(patch);
    if (found == m_Patches.end() ||
        found->second.Owner.Id != owner.Id ||
        found->second.Owner.Generation != owner.Generation)
        return Failure(Error::InvalidState,
                       "The Behavior Patch handle is stale.");
    out.State = found->second.Value.State();
    out.Diagnostic = found->second.Value.Diagnostic();
    out.Conflicts = found->second.Value.Conflicts();
    return {};
}

Status Patches::Close(OwnedPatch &patch) {
    return m_Edit.Close(patch.Value);
}

Status Patches::Close(const SessionOwner &owner, PatchId patch) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Patches.find(patch);
    if (found == m_Patches.end())
        return {};
    if (found->second.Owner.Id != owner.Id ||
        found->second.Owner.Generation != owner.Generation)
        return Failure(Error::OwnerInvalid,
                       "The Behavior Patch belongs to another Mod generation.");
    Status status = Close(found->second);
    const PatchState state = found->second.Value.State();
    if (state == PatchState::Closed || state == PatchState::Failed)
        m_Patches.erase(found);
    return status;
}

Status Patches::RetireOwner(const std::string &ownerId) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Status first;
    for (auto &[id, patch] : m_Patches) {
        if (patch.Owner.Id != ownerId)
            continue;
        Status status = Close(patch);
        if (!status && first)
            first = std::move(status);
    }
    m_Edit.ProcessFrame();
    Collect();
    return first;
}

void Patches::ObjectsToBeDeleted(const CK_ID *ids, int count) {
    if (!ids || count <= 0 || std::this_thread::get_id() != m_Thread)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const std::set<CK_ID> deleting(ids, ids + count);
    for (auto patch = m_Patches.begin(); patch != m_Patches.end();) {
        if (!deleting.contains(patch->second.Graph)) {
            ++patch;
            continue;
        }
        (void) Close(patch->second);
        // The native graph is already committed to deletion. There is no
        // revert target for a conflicting inverse, and queued CKEdit/Runtime
        // work retains the resources it still needs at the safe point.
        patch = m_Patches.erase(patch);
    }
    m_Edit.ProcessFrame();
    Collect();
}

void Patches::ResetWorld() {
    if (std::this_thread::get_id() != m_Thread)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    for (auto &[id, patch] : m_Patches)
        (void) Close(patch);
    m_Edit.ProcessFrame();
    Collect();

    // A conflicting external edit may prevent exact restoration. Admission is
    // already closed, and the CK world is about to disappear; do not retain a
    // journal whose native identities belong to the old world.
    m_Patches.clear();
}

void Patches::ProcessFrame() {
    if (std::this_thread::get_id() != m_Thread)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_Edit.ProcessFrame();
    Collect();
}

void Patches::Collect() {
    for (auto patch = m_Patches.begin(); patch != m_Patches.end();) {
        const PatchState state = patch->second.Value.State();
        if (state == PatchState::Closed || state == PatchState::Failed)
            patch = m_Patches.erase(patch);
        else
            ++patch;
    }
}

} // namespace BML::Behavior

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
                 ResolveObject resolveObject, IssueObject issueObject)
    : m_Edit(context, runtime, catalog, graph),
      m_Graph(graph), m_ResolveObject(std::move(resolveObject)),
      m_IssueObject(std::move(issueObject)),
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
                      PatchId &out,
                      const std::map<std::uint32_t, Node> *handles) {
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
            stored->second.Retiring = !status;
            if (handles)
                stored->second.Handles = *handles;
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

Status Patches::Apply(const SessionOwner &owner, const ObjectRef &graph,
                      std::string name, GraphEdit edit, PatchId &out,
                      const HandleMap *authorNodes) {
    out = 0;
    Status status = Ready();
    if (!status)
        return status;
    if (!owner || graph.IsNull() || name.empty()) {
        return Failure(
            Error::OwnerInvalid,
            "A Behavior Patch requires an owner, graph, and name.");
    }
    return Install(owner, {owner.Id, std::move(name)}, graph, edit, out,
                   authorNodes);
}

Status Patches::ResolveNode(const SessionOwner &owner, PatchId patch,
                            std::uint32_t handle, ObjectRef &out) const {
    out = {};
    Status status = Ready();
    if (!status)
        return status;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Patches.find(patch);
    if (found == m_Patches.end() ||
        found->second.Owner.Id != owner.Id ||
        found->second.Owner.Generation != owner.Generation)
        return Failure(Error::InvalidState,
                       "The Behavior Patch handle is stale.");
    const auto named = found->second.Handles.find(handle);
    if (named == found->second.Handles.end())
        return Failure(Error::QueryNotFound,
                       "The Behavior Patch names no Node under this handle.");
    CKBehavior *native = nullptr;
    status = m_Edit.ResolveNode(found->second.Value, named->second, native);
    if (!status)
        return status;
    if (!m_IssueObject)
        return Failure(Error::InvalidState,
                       "This Loader cannot issue object references.");
    out = m_IssueObject(native);
    if (out.IsNull())
        return Failure(Error::CreateFailed,
                       "The Loader could not issue a reference for the Node.");
    return {};
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
    // A durable Plan installs into scripts that do not exist yet, so it cannot
    // carry a reference issued against one live world.
    if (edit.UsesIdentity())
        return Failure(Error::InvalidState,
                       "A durable Behavior Edit cannot name a Node or Link by "
                       "reference; query it by name or Prototype instead.");
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
                        PatchId &out, const HandleMap *authorNodes) {
    out = 0;
    Edit resolved;
    std::map<std::uint32_t, Node> compiled;
    Status status = edit.Compile(patch, graph, *this, resolved, &compiled);
    if (!status)
        return status;

    std::map<std::uint32_t, Node> handles;
    try {
        if (authorNodes) {
            // Keep the author's own handles, so a Patch is read back through
            // the names its program used rather than compiler-internal ones.
            for (const auto &entry : *authorNodes) {
                const auto found = compiled.find(entry.second);
                if (found != compiled.end())
                    handles.emplace(entry.first, found->second);
            }
        } else {
            handles = compiled;
        }
    } catch (...) {
        return Failure(Error::CreateFailed,
                       "The Loader could not retain the Behavior Patch handles.");
    }
    return Apply(owner, resolved, out, &handles);
}

Status Patches::Begin(const PatchKey &patch, const ObjectRef &graph,
                      Edit &out, GraphModel &base) {
    out = {};
    base = {};
    CKObject *object = m_ResolveObject ? m_ResolveObject(graph) : nullptr;
    CKBehavior *behavior = object ? CKBehavior::Cast(object) : nullptr;
    if (!behavior)
        return Failure(Error::TargetInvalid,
                       "The Behavior Patch target graph is stale.");
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
                  "A Behavior Node disappeared during compilation.");
}

Status Patches::UseLink(Edit &edit, const ObjectRef &link, Link &out) {
    out = {};
    CKObject *object = m_ResolveObject ? m_ResolveObject(link) : nullptr;
    CKBehaviorLink *native = object ? CKBehaviorLink::Cast(object) : nullptr;
    return native
        ? m_Edit.Use(edit, native, out)
        : Failure(Error::GraphChanged,
                  "A Behavior Link disappeared during compilation.");
}

Status Patches::Add(Edit &edit, PrototypeRef prototype,
                    const GraphEdit::SettingStages &settings,
                    Node &out) {
    Spec block(prototype.Guid);
    block.PrototypeGeneration(prototype.Generation);
    bool first = true;
    for (const GraphEdit::Settings &stage : settings) {
        if (!first)
            block.RefreshLayout();
        first = false;
        for (const auto &[slot, value] : stage)
            block.Setting(slot, value);
    }
    return m_Edit.Add(edit, std::move(block), out);
}

Status Patches::Tap(Edit &edit, Port source,
                    const HookBlock::Hook &hook) {
    std::shared_ptr<HookBlock::Binding> binding = hook.Bind();
    if (!binding) {
        return Failure(Error::CallbackFailed,
                       "The Tap callback is no longer available.");
    }
    edit.Tap(std::move(source), std::move(binding));
    return {};
}

Status Patches::Interpose(Edit &edit, Link link,
                          const HookBlock::Hook &hook) {
    std::shared_ptr<HookBlock::Binding> binding = hook.Bind();
    if (!binding) {
        return Failure(Error::CallbackFailed,
                       "The Link callback is no longer available.");
    }
    Node block;
    Status status = m_Edit.Add(
        edit, HookBlock::Make(std::move(binding), 1, 1), block,
        NodeRole::Infrastructure);
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
    patch.Retiring = true;
    // A Closing Patch already has one CKEdit request queued. Let that request
    // preserve callback and journal retirement order at the safe point.
    if (patch.Value.State() == PatchState::Closing)
        return Failure(Error::Busy, "The Behavior Patch is Closing.",
                       Phase::Teardown);
    Status status = m_Edit.Close(patch.Value);
    if (status && patch.Value.State() == PatchState::Closing)
        return Failure(Error::Busy, "The Behavior Patch is Closing.",
                       Phase::Teardown);
    return status;
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
    for (auto &[id, patch] : m_Patches) {
        if (patch.Owner.Id != ownerId)
            continue;
        (void) Close(patch);
    }
    m_Edit.ProcessFrame();
    Collect();
    Status remaining;
    for (const auto &[id, patch] : m_Patches) {
        if (patch.Owner.Id != ownerId)
            continue;
        Status status = patch.Value.Diagnostic();
        if (status) {
            status = Failure(Error::Busy,
                             "A Behavior Patch is still Closing.",
                             Phase::Teardown);
        }
        if (remaining ||
            (remaining.Code == Error::Busy && status.Code != Error::Busy))
            remaining = std::move(status);
    }
    return remaining;
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
    for (auto &entry : m_Patches) {
        OwnedPatch &patch = entry.second;
        const PatchState state = patch.Value.State();
        if (patch.Retiring &&
            (state == PatchState::Active || state == PatchState::Conflicted))
            (void) Close(patch);
    }
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

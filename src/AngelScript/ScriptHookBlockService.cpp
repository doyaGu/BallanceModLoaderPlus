#include "ScriptHookBlockService.h"

#include <algorithm>
#include <memory>
#include <new>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include <angelscript.h>

#include "Api/ObjectRefs.h"
#include "BML/Behavior.hpp"
#include "ScriptBehavior.h"
#include "ScriptFunctionSupport.h"
#include "ScriptMod.h"
#include "ScriptModContextView.h"
#include "Loader/ModContext.h"

namespace BML {

namespace {

struct ScriptHookBlockEntry {
    unsigned int Id = 0;
    unsigned int Generation = 0;
    std::string Name;
    Behavior::ObjectRef OwnerScript{};
    Behavior::ObjectRef Block{};
    std::optional<Behavior::Instance> Instance;
    Behavior::Patch Placement;
    bool Enabled = true;
    bool AutoActivateOutputs = true;
    bool Retiring = false;
};

static bool IsIndexInRange(int index, int count) {
    return index >= 0 && index < count;
}

static std::string DefaultHookBlockName(unsigned int id) {
    return std::string("BML Script Hook ") + std::to_string(id);
}

static bool IsHookBlockCallbackSignature(asIScriptFunction *callback) {
    const ScriptFunctionParam params[] = {
        {"BML::ModContext", asTM_INREF | asTM_CONST},
        {"BML::HookBlockEvent", asTM_INREF | asTM_CONST},
    };
    return ScriptFunctionHasSignature(callback, asTYPEID_INT32, params, 2);
}

struct HookBlockFunctionCallArgs {
    ScriptModContextView *ContextView = nullptr;
    ScriptHookBlockEventView *Event = nullptr;
    int Result = CKBR_OK;
};

static int WriteHookBlockArgs(asIScriptContext *context, void *userdata) {
    auto *args = static_cast<HookBlockFunctionCallArgs *>(userdata);
    int code = context->SetArgObject(0, args ? args->ContextView : nullptr);
    if (code >= 0)
        code = context->SetArgObject(1, args ? args->Event : nullptr);
    return code;
}

static int ReadHookBlockResult(asIScriptContext *context, void *userdata) {
    auto *args = static_cast<HookBlockFunctionCallArgs *>(userdata);
    if (!context || !args)
        return asERROR;
    args->Result = static_cast<int>(context->GetReturnDWord());
    return asSUCCESS;
}

} // namespace

class ScriptHookBlockServiceState {
public:
    ModContext *Context = nullptr;
    ScriptMod *Owner = nullptr;
    ScriptModContextView *ContextView = nullptr;
    // Owns the Session every Hook Block of this Script Mod is placed through.
    const ScriptBehaviorService *BehaviorService = nullptr;
    bool Active = false;
    unsigned int NextId = 1;
    unsigned int NextGeneration = 1;
    std::unordered_map<unsigned int, std::unique_ptr<ScriptHookBlockEntry>> Entries;
    std::vector<std::pair<unsigned int, unsigned int>> Retirements;
};

namespace {

static void RecordHookBlockDiagnostic(const std::shared_ptr<ScriptHookBlockServiceState> &state,
                                      const std::string &message) {
    if (state && state->Owner)
        state->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, message));
}

static bool SameObject(Behavior::ObjectRef left, Behavior::ObjectRef right) noexcept {
    return left.Domain == right.Domain && left.Slot == right.Slot &&
           left.Generation == right.Generation;
}

static const Behavior::Session *GetBehaviorSession(const std::shared_ptr<ScriptHookBlockServiceState> &state) {
    if (!state || !state->Active || !state->Owner || !state->BehaviorService)
        return nullptr;
    const Behavior::Session *session = state->BehaviorService->GetSession();
    if (!session)
        RecordHookBlockDiagnostic(state, "The Script Mod's Behavior session is not open.");
    return session;
}

static CKBehavior *ResolveBehavior(const std::shared_ptr<ScriptHookBlockServiceState> &state,
                                   Behavior::ObjectRef reference) {
    if (!state || !state->Context || !reference.Domain)
        return nullptr;
    CKObject *object = state->Context->ObjectRefs().Resolve(reference);
    return object && CKIsChildClassOf(object, CKCID_BEHAVIOR)
        ? static_cast<CKBehavior *>(object) : nullptr;
}

static void ActivateAllOutputs(CKBehavior *block) {
    if (!block)
        return;
    for (int i = 0; i < block->GetOutputCount(); ++i)
        block->ActivateOutput(i);
}

static Behavior::Node FindNode(const Behavior::Graph &graph, Behavior::ObjectRef object) {
    for (Behavior::Node node : graph.Nodes()) {
        if (SameObject(node.Object(), object))
            return node;
    }
    return {};
}

static bool PlaceHookBlock(
    const std::shared_ptr<ScriptHookBlockServiceState> &state,
    ScriptHookBlockEntry &entry, CKBehavior *source, CKBehavior *target,
    int sourceOutput, int targetInput, const char *operation) {
    const Behavior::Session *session = GetBehaviorSession(state);
    if (!session || !entry.OwnerScript.Domain || !entry.Block.Domain)
        return false;

    auto graph = session->Inspect(entry.OwnerScript, Behavior::View::Logical);
    auto sourceRef = source
        ? session->Reference(source)
        : Behavior::Result<Behavior::ObjectRef>::Failure(BML_ERROR_NOT_FOUND);
    auto targetRef = target
        ? session->Reference(target)
        : Behavior::Result<Behavior::ObjectRef>::Failure(BML_ERROR_NOT_FOUND);
    if (!graph || (source && !sourceRef) || (target && !targetRef)) {
        const Behavior::Status &failure = !graph
            ? graph.GetStatus()
            : source && !sourceRef ? sourceRef.GetStatus()
                                   : targetRef.GetStatus();
        std::string message = std::string(operation) +
            " could not inspect the requested Behavior graph";
        if (!failure.Message.empty()) {
            message += ": " + failure.Message;
        } else {
            message += ".";
        }
        RecordHookBlockDiagnostic(state, message);
        return false;
    }

    const Behavior::Node blockNode = FindNode(graph.Value(), entry.Block);
    const Behavior::Node sourceNode = source
        ? FindNode(graph.Value(), sourceRef.Value()) : Behavior::Node{};
    const Behavior::Node targetNode = target
        ? FindNode(graph.Value(), targetRef.Value()) : Behavior::Node{};
    if (!blockNode || (source && !sourceNode) || (target && !targetNode)) {
        RecordHookBlockDiagnostic(
            state, std::string(operation) +
                " received a Block outside its owner Graph.");
        return false;
    }

    Behavior::Link selected;
    if (sourceNode && IsIndexInRange(sourceOutput, source->GetOutputCount())) {
        for (Behavior::Link link : graph->Outgoing(sourceNode.Out(sourceOutput))) {
            if (targetNode && link.Target().Node() != targetNode.Id())
                continue;
            if (targetInput >= 0 && link.Target().Index() != targetInput)
                continue;
            selected = link;
            break;
        }
    } else if (targetNode && IsIndexInRange(targetInput, target->GetInputCount())) {
        for (Behavior::Link link : graph->Incoming(
                 targetNode.In(targetInput))) {
            if (sourceOutput >= 0 && link.Source().Index() != sourceOutput)
                continue;
            selected = link;
            break;
        }
    }
    if (!selected) {
        RecordHookBlockDiagnostic(state, std::string(operation) + " could not find the requested Behavior Link.");
        return false;
    }

    Behavior::Edit edit;
    auto root = edit.Root();
    root.Splice(root.Use(selected), root.Use(blockNode));
    auto applied = graph->Apply(entry.Name + " placement", edit);
    if (!applied) {
        RecordHookBlockDiagnostic(
            state, applied.GetStatus().Message.empty()
                ? std::string(operation) + " could not apply its Behavior Patch."
                : applied.GetStatus().Message);
        return false;
    }
    entry.Placement = applied.Take();
    return true;
}

// Restores the graph before the Block goes away: the Placement Patch first,
// then the Instance, which removes the Block at a later safe point. Returns
// false while either one is still closing.
static bool CloseHookBlock(const std::shared_ptr<ScriptHookBlockServiceState> &state,
                           ScriptHookBlockEntry &entry) {
    if (entry.Placement) {
        auto closed = entry.Placement.Close();
        if (!closed) {
            RecordHookBlockDiagnostic(
                state, closed.GetStatus().Message.empty()
                    ? "The HookBlock Behavior Patch could not be restored."
                    : closed.GetStatus().Message);
            return false;
        }
        if (closed.Value() == Behavior::CloseState::Closing)
            return false;
    }
    if (entry.Instance) {
        auto closed = entry.Instance->Close();
        if (!closed) {
            RecordHookBlockDiagnostic(
                state, closed.GetStatus().Message.empty()
                    ? "The HookBlock Behavior Instance could not be closed."
                    : closed.GetStatus().Message);
            return false;
        }
        if (closed.Value() == Behavior::CloseState::Closing)
            return false;
        entry.Instance.reset();
    }
    return true;
}

static const Behavior::Status &PatchFailure(const Behavior::PatchInfo &info) {
    if (info.ApplyFailure.Error != Behavior::Error::None)
        return info.ApplyFailure;
    if (info.RestoreFailure.Error != Behavior::Error::None)
        return info.RestoreFailure;
    return info.LastStatus;
}

static ScriptHookBlockEntry *FindHookBlockEntry(const std::shared_ptr<ScriptHookBlockServiceState> &state,
                                               unsigned int id,
                                               unsigned int generation) {
    if (!state)
        return nullptr;
    auto it = state->Entries.find(id);
    if (it == state->Entries.end() || !it->second || it->second->Generation != generation)
        return nullptr;
    return it->second.get();
}

static bool RetireHookBlockEntry(const std::shared_ptr<ScriptHookBlockServiceState> &state,
                                 unsigned int id,
                                 unsigned int generation) {
    ScriptHookBlockEntry *entry = FindHookBlockEntry(state, id, generation);
    if (!entry)
        return false;
    if (entry->Retiring)
        return true;
    entry->Retiring = true;
    entry->Enabled = false;
    state->Retirements.emplace_back(id, generation);
    return true;
}

static void ProcessHookBlockRetirements(
    const std::shared_ptr<ScriptHookBlockServiceState> &state) {
    if (!state || state->Retirements.empty())
        return;
    std::vector<std::pair<unsigned int, unsigned int>> retirements;
    retirements.swap(state->Retirements);
    for (const auto &[id, generation] : retirements) {
        auto it = state->Entries.find(id);
        if (it == state->Entries.end() || !it->second ||
            it->second->Generation != generation) {
            continue;
        }
        if (CloseHookBlock(state, *it->second))
            state->Entries.erase(it);
        else
            state->Retirements.emplace_back(id, generation);
    }
}

static void ProcessHookBlockPlacements(const std::shared_ptr<ScriptHookBlockServiceState> &state) {
    if (!state || !state->Active)
        return;

    for (const auto &[id, owned] : state->Entries) {
        if (!owned || owned->Retiring || !owned->Placement)
            continue;

        auto info = owned->Placement.Info();
        if (info && (info->State == Behavior::PatchState::Pending ||
                     info->State == Behavior::PatchState::Active)) {
            continue;
        }

        const Behavior::Status *status = info
            ? &PatchFailure(info.Value()) : &info.GetStatus();
        RecordHookBlockDiagnostic(
            state, status->Message.empty()
                ? "A HookBlock placement failed before it became active."
                : status->Message);
        (void) RetireHookBlockEntry(state, id, owned->Generation);
    }
}

static void RetireUnregisteredHookBlock(
    const std::shared_ptr<ScriptHookBlockServiceState> &state,
    std::unique_ptr<ScriptHookBlockEntry> entry) {
    if (!state || !entry)
        return;

    const unsigned int id = entry->Id;
    const unsigned int generation = entry->Generation;
    entry->Enabled = false;
    entry->Retiring = true;
    state->Entries.emplace(id, std::move(entry));
    state->Retirements.emplace_back(id, generation);
    ProcessHookBlockRetirements(state);
}

static ScriptHookBlockEntry *ResolveHookBlockEntry(const std::shared_ptr<ScriptHookBlockServiceState> &state,
                                                  unsigned int id,
                                                  unsigned int generation) {
    if (!state || !state->Active)
        return nullptr;
    ScriptHookBlockEntry *entry = FindHookBlockEntry(state, id, generation);
    return entry && !entry->Retiring ? entry : nullptr;
}

// The script callback of one entry. The Loader holds it for as long as the
// Hook Block may run, which can outlive the entry, so it names the entry by
// id and generation instead of pointing at it.
class ScriptHookBlockFunction final {
public:
    ScriptHookBlockFunction(std::weak_ptr<ScriptHookBlockServiceState> state,
                            unsigned int id, unsigned int generation,
                            asIScriptFunction *function)
        : m_State(std::move(state)), m_Id(id), m_Generation(generation),
          m_Function(function) {
        if (m_Function)
            m_Function->AddRef();
    }
    ~ScriptHookBlockFunction() {
        if (m_Function)
            m_Function->Release();
    }
    ScriptHookBlockFunction(const ScriptHookBlockFunction &) = delete;
    ScriptHookBlockFunction &operator=(const ScriptHookBlockFunction &) = delete;

    // The Block leaves its Outs to this callback, which activates them as the
    // entry's Auto Activate Outputs setting says. A retiring or released entry
    // passes the activation through without running the script.
    Behavior::HookResult Invoke(const Behavior::HookEvent &source) {
        std::shared_ptr<ScriptHookBlockServiceState> state = m_State.lock();
        CKBehavior *block = ResolveBehavior(state, source.Block);
        if (!block)
            return Behavior::HookResult::Fault;
        ScriptHookBlockEntry *entry = ResolveHookBlockEntry(state, m_Id, m_Generation);
        if (!entry) {
            ActivateAllOutputs(block);
            return Behavior::HookResult::Ok;
        }
        bool activate = entry->AutoActivateOutputs;
        if (!entry->Enabled || !m_Function || !state->ContextView ||
            (state->Owner && !state->Owner->CanDispatchScriptServiceCallback())) {
            if (activate)
                ActivateAllOutputs(block);
            return Behavior::HookResult::Ok;
        }

        ScriptHookBlockEventView event(
            block, ResolveBehavior(state, source.Script), source.DeltaTime);
        HookBlockFunctionCallArgs args = {state->ContextView, &event, CKBR_OK};
        ScriptFunctionCall call;
        call.Function = m_Function;
        call.Owner = state->Owner;
        call.Phase = ScriptDiagnosticPhase::Callback;
        call.FailurePrefix = "HookBlock callback failed";
        call.InvalidStateMessage = "HookBlock callback has invalid runtime state.";
        call.ContextFailureMessage = "Unable to create AngelScript context for HookBlock callback.";
        call.SuspendedMessage = "script HookBlock callback suspended";
        call.WriteArgs = WriteHookBlockArgs;
        call.ReadResult = ReadHookBlockResult;
        call.UserData = &args;

        ScriptDiagnostic diagnostic;
        if (!ExecuteScriptFunction(call, diagnostic)) {
            // A faulted callback closes this Hook Block's admission, and the
            // Block passes every later activation through.
            if (ScriptHookBlockEntry *failed = FindHookBlockEntry(state, m_Id, m_Generation))
                failed->Enabled = false;
            if (state->Owner)
                state->Owner->SetLoadFailure(diagnostic);
            return Behavior::HookResult::Fault;
        }

        // The callback may have changed the setting or retired the entry.
        if (ScriptHookBlockEntry *current = FindHookBlockEntry(state, m_Id, m_Generation))
            activate = current->AutoActivateOutputs;
        // Script HookBlocks have always activated their Outs after the
        // callback, even when it returned a CKBR error code. The Block still
        // returns an error code, which CK2 discards for a sub-behavior.
        if (activate)
            ActivateAllOutputs(block);
        if ((args.Result & CKBR_GENERICERROR) == CKBR_GENERICERROR)
            return Behavior::HookResult::Error;
        return (args.Result & CKBR_ACTIVATENEXTFRAME) != 0
            ? Behavior::HookResult::AgainNextFrame
            : Behavior::HookResult::Ok;
    }

private:
    std::weak_ptr<ScriptHookBlockServiceState> m_State;
    unsigned int m_Id = 0;
    unsigned int m_Generation = 0;
    asIScriptFunction *m_Function = nullptr;
};

static ScriptHookBlockRef *RegisterHookBlockEntry(const std::shared_ptr<ScriptHookBlockServiceState> &state,
                                                  std::unique_ptr<ScriptHookBlockEntry> entry) {
    if (!state || !entry)
        return nullptr;
    const unsigned int id = entry->Id;
    const unsigned int generation = entry->Generation;
    ScriptHookBlockRef *ref = new (std::nothrow) ScriptHookBlockRef(state, id, generation);
    if (!ref) {
        RetireUnregisteredHookBlock(state, std::move(entry));
        RecordHookBlockDiagnostic(state, "Unable to create HookBlock reference.");
        return nullptr;
    }
    state->Entries.emplace(id, std::move(entry));
    return ref;
}

static std::unique_ptr<ScriptHookBlockEntry> CreateHookBlockEntry(
    const std::shared_ptr<ScriptHookBlockServiceState> &state,
    CKBehavior *ownerScript,
    asIScriptFunction *callback,
    const std::string &name,
    int inputCount,
    int outputCount) {
    if (!state || !state->Active || !state->Owner || !ownerScript || !callback)
        return nullptr;

    if (!IsHookBlockCallbackSignature(callback)) {
        RecordHookBlockDiagnostic(state, "HookBlock registration requires BML::HookBlockCallback.");
        return nullptr;
    }
    const Behavior::Session *session = GetBehaviorSession(state);
    if (!session)
        return nullptr;

    std::unique_ptr<ScriptHookBlockEntry> entry(new (std::nothrow) ScriptHookBlockEntry());
    if (!entry) {
        RecordHookBlockDiagnostic(state, "Unable to create HookBlock entry.");
        return nullptr;
    }
    entry->Id = state->NextId++;
    entry->Generation = state->NextGeneration++;
    entry->Name = name.empty() ? DefaultHookBlockName(entry->Id) : name;

    std::shared_ptr<ScriptHookBlockFunction> function;
    try {
        function = std::make_shared<ScriptHookBlockFunction>(
            state, entry->Id, entry->Generation, callback);
    } catch (const std::bad_alloc &) {
        RecordHookBlockDiagnostic(state, "HookBlock callback binding failed.");
        return nullptr;
    }
    const Behavior::Hook hook(
        [function](const Behavior::HookEvent &event) {
            return function->Invoke(event);
        });
    Behavior::HookBlock shape;
    shape.Inputs = std::max(1, inputCount);
    shape.Outputs = std::max(1, outputCount);
    shape.ActivatesOutputs = false;
    auto spawned = session->SpawnIn(ownerScript, hook, shape);
    if (!spawned) {
        RecordHookBlockDiagnostic(
            state, spawned.GetStatus().Message.empty()
                ? "HookBlock creation failed."
                : spawned.GetStatus().Message);
        return nullptr;
    }
    entry->Instance.emplace(spawned.Take());

    auto shown = entry->Instance->Inspect(Behavior::View::Live);
    auto owner = session->Reference(ownerScript);
    CKBehavior *block = shown ? ResolveBehavior(state, shown->Root().Object()) : nullptr;
    if (!block || !owner) {
        (void) CloseHookBlock(state, *entry);
        RecordHookBlockDiagnostic(state, "HookBlock creation failed.");
        return nullptr;
    }
    entry->Block = shown->Root().Object();
    entry->OwnerScript = owner.Value();
    block->SetName((CKSTRING) entry->Name.c_str());
    return entry;
}

} // namespace

ScriptHookBlockEventView::ScriptHookBlockEventView(CKBehavior *block, CKBehavior *ownerScript,
                                                   float deltaTime)
    : m_Block(block),
      m_OwnerScript(ownerScript ? ownerScript : (block ? block->GetOwnerScript() : nullptr)),
      m_DeltaTime(deltaTime) {}

int ScriptHookBlockEventView::GetBlockId() const {
    return m_Block ? static_cast<int>(m_Block->GetID()) : 0;
}

std::string ScriptHookBlockEventView::GetBlockName() const {
    CKSTRING name = m_Block ? m_Block->GetName() : nullptr;
    return name ? name : "";
}

int ScriptHookBlockEventView::GetInputCount() const {
    return m_Block ? m_Block->GetInputCount() : 0;
}

int ScriptHookBlockEventView::GetOutputCount() const {
    return m_Block ? m_Block->GetOutputCount() : 0;
}

bool ScriptHookBlockEventView::ActivateOutput(int index) const {
    if (!m_Block || !IsIndexInRange(index, m_Block->GetOutputCount()))
        return false;
    m_Block->ActivateOutput(index);
    return true;
}

void ScriptHookBlockEventView::ActivateAllOutputs() const {
    if (!m_Block)
        return;
    const int count = m_Block->GetOutputCount();
    for (int i = 0; i < count; ++i)
        m_Block->ActivateOutput(i);
}

ScriptHookBlockRef::ScriptHookBlockRef(std::weak_ptr<ScriptHookBlockServiceState> state,
                                       unsigned int id,
                                       unsigned int generation)
    : m_State(std::move(state)),
      m_Id(id),
      m_Generation(generation) {}

void ScriptHookBlockRef::AddRef() { ++m_RefCount; }
void ScriptHookBlockRef::Release() {
    if (--m_RefCount == 0)
        delete this;
}

bool ScriptHookBlockRef::IsValid() const {
    return ResolveHookBlockEntry(m_State.lock(), m_Id, m_Generation) != nullptr;
}

bool ScriptHookBlockRef::IsInstalled() const {
    ScriptHookBlockEntry *entry = ResolveHookBlockEntry(m_State.lock(), m_Id, m_Generation);
    if (!entry || !entry->Placement)
        return false;
    const auto info = entry->Placement.Info();
    return info && info->State == Behavior::PatchState::Active;
}

bool ScriptHookBlockRef::IsEnabled() const {
    ScriptHookBlockEntry *entry = ResolveHookBlockEntry(m_State.lock(), m_Id, m_Generation);
    return entry && entry->Enabled;
}

bool ScriptHookBlockRef::SetEnabled(bool enabled) {
    ScriptHookBlockEntry *entry = ResolveHookBlockEntry(m_State.lock(), m_Id, m_Generation);
    if (!entry)
        return false;
    entry->Enabled = enabled;
    return true;
}

bool ScriptHookBlockRef::GetAutoActivateOutputs() const {
    ScriptHookBlockEntry *entry = ResolveHookBlockEntry(m_State.lock(), m_Id, m_Generation);
    return entry && entry->AutoActivateOutputs;
}

bool ScriptHookBlockRef::SetAutoActivateOutputs(bool enabled) {
    ScriptHookBlockEntry *entry = ResolveHookBlockEntry(m_State.lock(), m_Id, m_Generation);
    if (!entry)
        return false;
    entry->AutoActivateOutputs = enabled;
    return true;
}

int ScriptHookBlockRef::GetBlockId() const {
    ScriptHookBlockEntry *entry = ResolveHookBlockEntry(m_State.lock(), m_Id, m_Generation);
    CKBehavior *block = entry ? ResolveBehavior(m_State.lock(), entry->Block) : nullptr;
    return block ? static_cast<int>(block->GetID()) : 0;
}

std::string ScriptHookBlockRef::GetName() const {
    ScriptHookBlockEntry *entry = ResolveHookBlockEntry(m_State.lock(), m_Id, m_Generation);
    return entry ? entry->Name : std::string();
}

CKBehavior *ScriptHookBlockRef::BorrowBlock() const {
    ScriptHookBlockEntry *entry = ResolveHookBlockEntry(m_State.lock(), m_Id, m_Generation);
    return entry ? ResolveBehavior(m_State.lock(), entry->Block) : nullptr;
}

CKBehavior *ScriptHookBlockRef::BorrowOwnerScript() const {
    ScriptHookBlockEntry *entry = ResolveHookBlockEntry(m_State.lock(), m_Id, m_Generation);
    return entry ? ResolveBehavior(m_State.lock(), entry->OwnerScript) : nullptr;
}

bool ScriptHookBlockRef::Uninstall() {
    return RetireHookBlockEntry(m_State.lock(), m_Id, m_Generation);
}

ScriptHookBlockService::ScriptHookBlockService()
    : m_State(std::make_shared<ScriptHookBlockServiceState>()) {}

ScriptHookBlockService::~ScriptHookBlockService() {
    try {
        Release(nullptr);
    } catch (...) {
    }
}

bool ScriptHookBlockService::Bind(ModContext *context, ScriptMod *owner, ScriptModContextView *contextView,
                                  const ScriptBehaviorService *behavior) {
    if (!m_State) {
        try {
            m_State = std::make_shared<ScriptHookBlockServiceState>();
        } catch (const std::bad_alloc &) {
            return false;
        }
    }
    m_State->Context = context;
    m_State->Owner = owner;
    m_State->ContextView = contextView;
    m_State->BehaviorService = behavior;
    m_State->Active = true;
    return true;
}

ScriptHookBlockRef *ScriptHookBlockService::Create(CKBehavior *ownerScript,
                                                   asIScriptFunction *callback,
                                                   const std::string &name,
                                                   int inputCount,
                                                   int outputCount) {
    if (!m_State)
        return nullptr;
    std::unique_ptr<ScriptHookBlockEntry> entry = CreateHookBlockEntry(m_State,
                                                                       ownerScript,
                                                                       callback,
                                                                       name,
                                                                       inputCount,
                                                                       outputCount);
    return RegisterHookBlockEntry(m_State, std::move(entry));
}

ScriptHookBlockRef *ScriptHookBlockService::InsertAfter(CKBehavior *ownerScript,
                                                        CKBehavior *source,
                                                        asIScriptFunction *callback,
                                                        const std::string &name,
                                                        int sourceOutput,
                                                        int targetInput) {
    if (!m_State)
        return nullptr;

    std::unique_ptr<ScriptHookBlockEntry> entry = CreateHookBlockEntry(m_State, ownerScript, callback, name, 1, 1);
    if (!entry)
        return nullptr;

    if (!PlaceHookBlock(m_State, *entry, source, nullptr,
                        sourceOutput, targetInput,
                        "InsertHookBlockAfter")) {
        RetireUnregisteredHookBlock(m_State, std::move(entry));
        return nullptr;
    }
    return RegisterHookBlockEntry(m_State, std::move(entry));
}

ScriptHookBlockRef *ScriptHookBlockService::InsertBefore(CKBehavior *ownerScript,
                                                         CKBehavior *target,
                                                         asIScriptFunction *callback,
                                                         const std::string &name,
                                                         int sourceOutput,
                                                         int targetInput) {
    if (!m_State)
        return nullptr;

    std::unique_ptr<ScriptHookBlockEntry> entry = CreateHookBlockEntry(m_State, ownerScript, callback, name, 1, 1);
    if (!entry)
        return nullptr;

    if (!PlaceHookBlock(m_State, *entry, nullptr, target,
                        sourceOutput, targetInput,
                        "InsertHookBlockBefore")) {
        RetireUnregisteredHookBlock(m_State, std::move(entry));
        return nullptr;
    }
    return RegisterHookBlockEntry(m_State, std::move(entry));
}

ScriptHookBlockRef *ScriptHookBlockService::InsertBetween(CKBehavior *ownerScript,
                                                          CKBehavior *source,
                                                          CKBehavior *target,
                                                          asIScriptFunction *callback,
                                                          const std::string &name,
                                                          int sourceOutput,
                                                          int targetInput) {
    if (!m_State)
        return nullptr;

    std::unique_ptr<ScriptHookBlockEntry> entry = CreateHookBlockEntry(m_State, ownerScript, callback, name, 1, 1);
    if (!entry)
        return nullptr;

    if (!PlaceHookBlock(m_State, *entry, source, target,
                        sourceOutput, targetInput,
                        "InsertHookBlockBetween")) {
        RetireUnregisteredHookBlock(m_State, std::move(entry));
        return nullptr;
    }
    return RegisterHookBlockEntry(m_State, std::move(entry));
}

void ScriptHookBlockService::Release(ScriptDiagnostic *) {
    if (!m_State || !m_State->Active)
        return;

    std::shared_ptr<ScriptHookBlockServiceState> releasedState = m_State;
    releasedState->Active = false;
    for (auto &[id, entry] : releasedState->Entries) {
        if (!entry)
            continue;
        entry->Enabled = false;
        entry->Retiring = true;
        releasedState->Retirements.emplace_back(id, entry->Generation);
    }
    ProcessHookBlockRetirements(releasedState);
    if (!releasedState->Entries.empty())
        m_RetiredStates.push_back(std::move(releasedState));
    m_State.reset();
}

void ScriptHookBlockService::ProcessFrame() {
    ProcessHookBlockPlacements(m_State);
    ProcessHookBlockRetirements(m_State);
    for (auto position = m_RetiredStates.begin();
         position != m_RetiredStates.end();) {
        ProcessHookBlockRetirements(*position);
        if ((*position)->Entries.empty())
            position = m_RetiredStates.erase(position);
        else
            ++position;
    }
}

std::size_t ScriptHookBlockService::GetActiveCount() const {
    if (!m_State || !m_State->Active)
        return 0;
    return static_cast<std::size_t>(std::count_if(
        m_State->Entries.begin(), m_State->Entries.end(),
        [](const auto &entry) {
            return entry.second && !entry.second->Retiring;
        }));
}

} // namespace BML

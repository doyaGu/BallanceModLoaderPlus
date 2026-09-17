#include "ScriptHookBlockService.h"

#include <algorithm>
#include <memory>
#include <new>
#include <unordered_map>
#include <utility>
#include <vector>

#include <angelscript.h>

#include "BML/Behavior.hpp"
#include "ScriptFunctionSupport.h"
#include "ScriptMod.h"
#include "ScriptModContextView.h"
#include "Loader/ModContext.h"
#include "Behavior/Blocks/HookBlock.h"

namespace BML {

namespace {

struct ScriptHookBlockEntry {
    std::weak_ptr<ScriptHookBlockServiceState> State;
    unsigned int Id = 0;
    unsigned int Generation = 0;
    std::string Name;
    CKBehavior *OwnerScript = nullptr;
    CKBehavior *Block = nullptr;
    Behavior::Patch Placement;
    asIScriptFunction *Callback = nullptr; // Borrowed from Binding's plan state.
    std::shared_ptr<Behavior::Internal::HookBlock::Binding> Binding;
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

static void SetHookBlockAutoActivateOutputs(CKBehavior *block, bool enabled) {
    if (!block || block->GetLocalParameterCount() <= 2)
        return;

    CKBOOL nativeValue = enabled ? TRUE : FALSE;
    block->SetLocalParameterValue(2, &nativeValue);
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
    Behavior::Session Authoring;
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

static Behavior::Session *GetBehaviorSession(const std::shared_ptr<ScriptHookBlockServiceState> &state) {
    if (!state || !state->Active || !state->Owner)
        return nullptr;
    if (!state->Authoring) {
        const char *owner = state->Owner->GetID();
        auto opened = Behavior::Session::Open(owner ? owner : "");
        if (!opened) {
            RecordHookBlockDiagnostic(
                state, opened.GetStatus().Message.empty()
                    ? "Unable to open the Script Mod's Behavior session."
                    : opened.GetStatus().Message);
            return nullptr;
        }
        state->Authoring = opened.Take();
    }
    return &state->Authoring;
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
    Behavior::Session *session = GetBehaviorSession(state);
    if (!session || !entry.OwnerScript || !entry.Block)
        return false;

    auto graph = session->Inspect(entry.OwnerScript, Behavior::View::Logical);
    auto blockRef = session->Reference(entry.Block);
    auto sourceRef = source
        ? session->Reference(source)
        : Behavior::Result<Behavior::ObjectRef>::Failure(BML_ERROR_NOT_FOUND);
    auto targetRef = target
        ? session->Reference(target)
        : Behavior::Result<Behavior::ObjectRef>::Failure(BML_ERROR_NOT_FOUND);
    if (!graph || !blockRef || (source && !sourceRef) || (target && !targetRef)) {
        const Behavior::Status &failure = !graph
            ? graph.GetStatus()
            : !blockRef ? blockRef.GetStatus()
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

    const Behavior::Node blockNode = FindNode(graph.Value(), blockRef.Value());
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

static void FailHookBlockCallback(const std::shared_ptr<ScriptHookBlockServiceState> &state,
                                  ScriptHookBlockEntry &entry,
                                  const ScriptDiagnostic &diagnostic) {
    entry.Enabled = false;
    if (entry.Binding)
        entry.Binding->CloseAdmission();
    if (state && state->Owner)
        state->Owner->SetLoadFailure(diagnostic);
}

static bool RestoreHookBlockGraph(const std::shared_ptr<ScriptHookBlockServiceState> &state,
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

    bool closed = true;
    if (entry.Block) {
        if (state && state->Context) {
            Behavior::Internal::Status status = state->Context->Behaviors().Close(entry.Block);
            closed = static_cast<bool>(status);
            if (!closed)
                RecordHookBlockDiagnostic(state, status.Message);
        } else {
            closed = false;
        }
    }
    if (!closed && entry.Binding)
        closed = entry.Binding->RetireAtSafePoint();

    entry.Block = nullptr;
    entry.OwnerScript = nullptr;
    entry.Callback = nullptr;
    entry.Binding.reset();
    return closed;
}

static const Behavior::Status &PatchFailure(const Behavior::PatchInfo &info) {
    if (info.ApplyFailure.Error != Behavior::Error::None)
        return info.ApplyFailure;
    if (info.RestoreFailure.Error != Behavior::Error::None)
        return info.RestoreFailure;
    return info.LastStatus;
}

static bool RetireHookBlockEntry(const std::shared_ptr<ScriptHookBlockServiceState> &state,
                                 unsigned int id,
                                 unsigned int generation) {
    if (!state)
        return false;

    auto it = state->Entries.find(id);
    if (it == state->Entries.end() || !it->second || it->second->Generation != generation)
        return false;

    ScriptHookBlockEntry &entry = *it->second;
    if (entry.Retiring)
        return true;
    entry.Retiring = true;
    entry.Enabled = false;
    if (entry.Binding)
        entry.Binding->CloseAdmission();
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
        if (RestoreHookBlockGraph(state, *it->second))
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
    if (entry->Binding)
        entry->Binding->CloseAdmission();
    state->Entries.emplace(id, std::move(entry));
    state->Retirements.emplace_back(id, generation);
    ProcessHookBlockRetirements(state);
}

static ScriptHookBlockEntry *ResolveHookBlockEntry(const std::shared_ptr<ScriptHookBlockServiceState> &state,
                                                  unsigned int id,
                                                  unsigned int generation) {
    if (!state || !state->Active)
        return nullptr;
    auto it = state->Entries.find(id);
    if (it == state->Entries.end() || !it->second ||
        it->second->Generation != generation || it->second->Retiring)
        return nullptr;
    return it->second.get();
}

static int RunScriptHookBlockCallback(const CKBehaviorContext *context, void *arg) {
    auto *entry = static_cast<ScriptHookBlockEntry *>(arg);
    if (!entry || !entry->Enabled || !entry->Callback)
        return CKBR_OK;

    std::shared_ptr<ScriptHookBlockServiceState> state = entry->State.lock();
    if (!state || !state->Active || !state->ContextView)
        return CKBR_OK;
    if (state->Owner && !state->Owner->CanDispatchScriptServiceCallback())
        return CKBR_OK;

    ScriptHookBlockEventView event(context, entry->OwnerScript);
    HookBlockFunctionCallArgs args = {state->ContextView, &event, CKBR_OK};
    ScriptFunctionCall call;
    call.Function = entry->Callback;
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
    const bool ok = ExecuteScriptFunction(call, diagnostic);
    if (!ok)
        FailHookBlockCallback(state, *entry, diagnostic);

    return ok ? args.Result : CKBR_OK;
}

static int ScriptHookBlockCallback(const CKBehaviorContext *context, void *arg) {
    try {
        return RunScriptHookBlockCallback(context, arg);
    } catch (...) {
        // Virtools invokes this through a native BB callback; exceptions must not escape.
        return CKBR_OK;
    }
}

static void RetainScriptHookBlockFunction(void *value) {
    if (value)
        static_cast<asIScriptFunction *>(value)->AddRef();
}

static void ReleaseScriptHookBlockFunction(void *value) {
    if (value)
        static_cast<asIScriptFunction *>(value)->Release();
}

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

    inputCount = std::max(1, inputCount);
    outputCount = std::max(1, outputCount);

    std::unique_ptr<ScriptHookBlockEntry> entry(new (std::nothrow) ScriptHookBlockEntry());
    if (!entry) {
        RecordHookBlockDiagnostic(state, "Unable to create HookBlock entry.");
        return nullptr;
    }
    entry->State = state;
    entry->Id = state->NextId++;
    entry->Generation = state->NextGeneration++;
    entry->Name = name.empty() ? DefaultHookBlockName(entry->Id) : name;
    entry->OwnerScript = ownerScript;
    entry->Callback = callback;
    Behavior::Internal::PlanCallbackState callbackState =
        Behavior::Internal::PlanCallbackState::Retained(
            callback, RetainScriptHookBlockFunction,
            ReleaseScriptHookBlockFunction);
    entry->Binding = Behavior::Internal::HookBlock::Bind(
        std::move(callbackState), ScriptHookBlockCallback, entry.get());
    if (!entry->Binding) {
        RecordHookBlockDiagnostic(state, "HookBlock callback binding failed.");
        return nullptr;
    }

    Behavior::Internal::AttachResult created = state->Context->Behaviors().AddToGraph(
        ownerScript, Behavior::Internal::HookBlock::Make(
            entry->Binding, inputCount, outputCount));
    entry->Block = created ? created.Block : nullptr;
    if (!entry->Block) {
        entry->Binding->CloseAdmission();
        RecordHookBlockDiagnostic(state, "HookBlock creation failed.");
        return nullptr;
    }

    entry->Block->SetName((CKSTRING) entry->Name.c_str());
    SetHookBlockAutoActivateOutputs(entry->Block, entry->AutoActivateOutputs);
    return entry;
}

} // namespace

ScriptHookBlockEventView::ScriptHookBlockEventView(const CKBehaviorContext *context, CKBehavior *ownerScript)
    : m_Block(context ? context->Behavior : nullptr),
      m_OwnerScript(ownerScript ? ownerScript : (m_Block ? m_Block->GetOwnerScript() : nullptr)),
      m_DeltaTime(context ? context->DeltaTime : 0.0f) {}

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
    SetHookBlockAutoActivateOutputs(entry->Block, enabled);
    return true;
}

int ScriptHookBlockRef::GetBlockId() const {
    ScriptHookBlockEntry *entry = ResolveHookBlockEntry(m_State.lock(), m_Id, m_Generation);
    return entry && entry->Block ? static_cast<int>(entry->Block->GetID()) : 0;
}

std::string ScriptHookBlockRef::GetName() const {
    ScriptHookBlockEntry *entry = ResolveHookBlockEntry(m_State.lock(), m_Id, m_Generation);
    return entry ? entry->Name : std::string();
}

CKBehavior *ScriptHookBlockRef::BorrowBlock() const {
    ScriptHookBlockEntry *entry = ResolveHookBlockEntry(m_State.lock(), m_Id, m_Generation);
    return entry ? entry->Block : nullptr;
}

CKBehavior *ScriptHookBlockRef::BorrowOwnerScript() const {
    ScriptHookBlockEntry *entry = ResolveHookBlockEntry(m_State.lock(), m_Id, m_Generation);
    return entry ? entry->OwnerScript : nullptr;
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

bool ScriptHookBlockService::Bind(ModContext *context, ScriptMod *owner, ScriptModContextView *contextView) {
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
        if (entry->Binding)
            entry->Binding->CloseAdmission();
        releasedState->Retirements.emplace_back(id, entry->Generation);
    }
    ProcessHookBlockRetirements(releasedState);
    if (releasedState->Entries.empty()) {
        releasedState->Authoring.Reset();
    } else {
        m_RetiredStates.push_back(std::move(releasedState));
    }
    m_State.reset();
}

void ScriptHookBlockService::ProcessFrame() {
    ProcessHookBlockPlacements(m_State);
    ProcessHookBlockRetirements(m_State);
    for (auto position = m_RetiredStates.begin();
         position != m_RetiredStates.end();) {
        ProcessHookBlockRetirements(*position);
        if ((*position)->Entries.empty()) {
            (*position)->Authoring.Reset();
            position = m_RetiredStates.erase(position);
        } else {
            ++position;
        }
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

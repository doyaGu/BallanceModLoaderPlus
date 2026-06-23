#include "ScriptHookBlockService.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <utility>

#include <angelscript.h>

#include "BML/ExecuteBB.h"
#include "BML/ScriptHelper.h"
#include "ScriptFunctionSupport.h"
#include "ScriptMod.h"
#include "ScriptModContextView.h"

namespace BML {

namespace {

enum class HookBlockPatchEndpoint {
    None,
    In,
    Out,
};

struct ScriptHookBlockEntry {
    std::weak_ptr<ScriptHookBlockServiceState> State;
    unsigned int Id = 0;
    unsigned int Generation = 0;
    std::string Name;
    CKBehavior *OwnerScript = nullptr;
    CKBehavior *Block = nullptr;
    CKBehaviorLink *PatchedLink = nullptr;
    CKBehaviorLink *CreatedLink = nullptr;
    CKBehaviorIO *OriginalEndpoint = nullptr;
    HookBlockPatchEndpoint PatchedEndpoint = HookBlockPatchEndpoint::None;
    asIScriptFunction *Callback = nullptr;
    bool Enabled = true;
    bool AutoActivateOutputs = true;
    int ActiveCalls = 0;
    bool PendingUninstall = false;
};

static bool IsIndexInRange(int index, int count) {
    return index >= 0 && index < count;
}

static std::string DefaultHookBlockName(unsigned int id) {
    return std::string("BML Script Hook ") + std::to_string(id);
}

static void SetHookBlockNativeCallback(CKBehavior *block,
                                       ExecuteBB::CKBehaviorCallback callback,
                                       void *arg) {
    if (!block || block->GetLocalParameterCount() < 2)
        return;

    block->SetLocalParameterValue(0, &callback);
    block->SetLocalParameterValue(1, &arg);
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

static CKBehaviorLink *FindNextLink(CKBehavior *ownerScript,
                                    CKBehavior *source,
                                    CKBehavior *target,
                                    int sourceOutput,
                                    int targetInput) {
    if (!ownerScript || !source)
        return nullptr;
    if (!IsIndexInRange(sourceOutput, source->GetOutputCount()))
        return nullptr;
    if (target && !IsIndexInRange(targetInput, target->GetInputCount()))
        return nullptr;

    CKBehaviorIO *sourceOutputIo = source->GetOutput(sourceOutput);
    CKBehaviorIO *targetInputIo = target ? target->GetInput(targetInput) : nullptr;
    const int count = ownerScript->GetSubBehaviorLinkCount();
    for (int i = 0; i < count; ++i) {
        CKBehaviorLink *link = ownerScript->GetSubBehaviorLink(i);
        if (!link || link->GetInBehaviorIO() != sourceOutputIo)
            continue;
        CKBehaviorIO *out = link->GetOutBehaviorIO();
        if (!out)
            continue;
        if (targetInputIo && out != targetInputIo)
            continue;
        if (!targetInputIo && targetInput >= 0) {
            CKBehavior *outOwner = out->GetOwner();
            if (!outOwner || !IsIndexInRange(targetInput, outOwner->GetInputCount()) ||
                outOwner->GetInput(targetInput) != out) {
                continue;
            }
        }
        return link;
    }

    return nullptr;
}

static CKBehaviorLink *FindPreviousLink(CKBehavior *ownerScript,
                                        CKBehavior *source,
                                        CKBehavior *target,
                                        int sourceOutput,
                                        int targetInput) {
    if (!ownerScript || !target)
        return nullptr;
    if (!IsIndexInRange(targetInput, target->GetInputCount()))
        return nullptr;
    if (source && !IsIndexInRange(sourceOutput, source->GetOutputCount()))
        return nullptr;

    CKBehaviorIO *targetInputIo = target->GetInput(targetInput);
    CKBehaviorIO *sourceOutputIo = source ? source->GetOutput(sourceOutput) : nullptr;
    const int count = ownerScript->GetSubBehaviorLinkCount();
    for (int i = 0; i < count; ++i) {
        CKBehaviorLink *link = ownerScript->GetSubBehaviorLink(i);
        if (!link || link->GetOutBehaviorIO() != targetInputIo)
            continue;
        CKBehaviorIO *in = link->GetInBehaviorIO();
        if (!in)
            continue;
        if (sourceOutputIo && in != sourceOutputIo)
            continue;
        if (!sourceOutputIo && sourceOutput >= 0) {
            CKBehavior *inOwner = in->GetOwner();
            if (!inOwner || !IsIndexInRange(sourceOutput, inOwner->GetOutputCount()) ||
                inOwner->GetOutput(sourceOutput) != in) {
                continue;
            }
        }
        return link;
    }

    return nullptr;
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

static void ReadHookBlockResult(asIScriptContext *context, void *userdata) {
    auto *args = static_cast<HookBlockFunctionCallArgs *>(userdata);
    if (args)
        args->Result = static_cast<int>(context->GetReturnDWord());
}

} // namespace

class ScriptHookBlockServiceState {
public:
    ModContext *Context = nullptr;
    ScriptMod *Owner = nullptr;
    ScriptModContextView *ContextView = nullptr;
    bool Active = false;
    unsigned int NextId = 1;
    unsigned int NextGeneration = 1;
    std::unordered_map<unsigned int, std::unique_ptr<ScriptHookBlockEntry>> Entries;
};

namespace {

static void RecordHookBlockDiagnostic(const std::shared_ptr<ScriptHookBlockServiceState> &state,
                                      const std::string &message) {
    if (state && state->Owner)
        state->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, message));
}

static void FailHookBlockCallback(const std::shared_ptr<ScriptHookBlockServiceState> &state,
                                  ScriptHookBlockEntry &entry,
                                  const ScriptDiagnostic &diagnostic) {
    entry.Enabled = false;
    if (state && state->Owner)
        state->Owner->SetLoadFailure(diagnostic);
}

static void ReleaseHookBlockCallback(ScriptHookBlockEntry &entry) {
    if (entry.Callback) {
        entry.Callback->Release();
        entry.Callback = nullptr;
    }
}

static void ClearHookBlockNativeCallback(ScriptHookBlockEntry &entry) {
    ExecuteBB::CKBehaviorCallback callback = nullptr;
    void *arg = nullptr;
    SetHookBlockNativeCallback(entry.Block, callback, arg);
}

static void RestoreHookBlockGraph(ScriptHookBlockEntry &entry) {
    ClearHookBlockNativeCallback(entry);

    if (entry.PatchedLink && entry.OriginalEndpoint) {
        if (entry.PatchedEndpoint == HookBlockPatchEndpoint::Out)
            entry.PatchedLink->SetOutBehaviorIO(entry.OriginalEndpoint);
        else if (entry.PatchedEndpoint == HookBlockPatchEndpoint::In)
            entry.PatchedLink->SetInBehaviorIO(entry.OriginalEndpoint);
    }

    if (entry.OwnerScript && entry.CreatedLink)
        ScriptHelper::DeleteLink(entry.OwnerScript, entry.CreatedLink);
    if (entry.OwnerScript && entry.Block)
        ScriptHelper::DeleteBB(entry.OwnerScript, entry.Block);

    entry.PatchedLink = nullptr;
    entry.CreatedLink = nullptr;
    entry.OriginalEndpoint = nullptr;
    entry.PatchedEndpoint = HookBlockPatchEndpoint::None;
    entry.Block = nullptr;
    entry.OwnerScript = nullptr;
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
    if (entry.ActiveCalls > 0) {
        entry.PendingUninstall = true;
        entry.Enabled = false;
        ClearHookBlockNativeCallback(entry);
        return true;
    }

    RestoreHookBlockGraph(entry);
    ReleaseHookBlockCallback(entry);
    state->Entries.erase(it);
    return true;
}

static ScriptHookBlockEntry *ResolveHookBlockEntry(const std::shared_ptr<ScriptHookBlockServiceState> &state,
                                                  unsigned int id,
                                                  unsigned int generation) {
    if (!state || !state->Active)
        return nullptr;
    auto it = state->Entries.find(id);
    if (it == state->Entries.end() || !it->second || it->second->Generation != generation)
        return nullptr;
    return it->second.get();
}

static int ScriptHookBlockCallback(const CKBehaviorContext *context, void *arg) {
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

    ++entry->ActiveCalls;
    ScriptDiagnostic diagnostic;
    const bool ok = ExecuteScriptFunction(call, diagnostic);
    if (!ok)
        FailHookBlockCallback(state, *entry, diagnostic);

    if (entry->ActiveCalls > 0)
        --entry->ActiveCalls;
    const bool pendingUninstall = entry->PendingUninstall && entry->ActiveCalls == 0;
    const unsigned int id = entry->Id;
    const unsigned int generation = entry->Generation;
    if (pendingUninstall)
        RetireHookBlockEntry(state, id, generation);

    return ok ? args.Result : CKBR_OK;
}

static ScriptHookBlockRef *RegisterHookBlockEntry(const std::shared_ptr<ScriptHookBlockServiceState> &state,
                                                  std::unique_ptr<ScriptHookBlockEntry> entry) {
    if (!state || !entry)
        return nullptr;
    const unsigned int id = entry->Id;
    const unsigned int generation = entry->Generation;
    state->Entries.emplace(id, std::move(entry));
    return new ScriptHookBlockRef(state, id, generation);
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

    std::unique_ptr<ScriptHookBlockEntry> entry(new ScriptHookBlockEntry());
    entry->State = state;
    entry->Id = state->NextId++;
    entry->Generation = state->NextGeneration++;
    entry->Name = name.empty() ? DefaultHookBlockName(entry->Id) : name;
    entry->OwnerScript = ownerScript;
    entry->Callback = callback;
    callback->AddRef();

    entry->Block = ExecuteBB::CreateHookBlock(ownerScript,
                                              ScriptHookBlockCallback,
                                              entry.get(),
                                              inputCount,
                                              outputCount);
    if (!entry->Block) {
        ReleaseHookBlockCallback(*entry);
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
    return entry && entry->PatchedLink != nullptr;
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

ScriptHookBlockService::~ScriptHookBlockService() { Release(nullptr); }

void ScriptHookBlockService::Bind(ModContext *context, ScriptMod *owner, ScriptModContextView *contextView) {
    m_State->Context = context;
    m_State->Owner = owner;
    m_State->ContextView = contextView;
    m_State->Active = true;
}

ScriptHookBlockRef *ScriptHookBlockService::Create(CKBehavior *ownerScript,
                                                   asIScriptFunction *callback,
                                                   const std::string &name,
                                                   int inputCount,
                                                   int outputCount) {
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
    CKBehaviorLink *link = FindNextLink(ownerScript, source, nullptr, sourceOutput, targetInput);
    if (!link) {
        RecordHookBlockDiagnostic(m_State, "InsertHookBlockAfter could not find a matching outgoing behavior link.");
        return nullptr;
    }

    std::unique_ptr<ScriptHookBlockEntry> entry = CreateHookBlockEntry(m_State, ownerScript, callback, name, 1, 1);
    if (!entry)
        return nullptr;

    CKBehaviorIO *originalOut = link->GetOutBehaviorIO();
    CKBehaviorLink *continuation = ScriptHelper::CreateLink(ownerScript, entry->Block, originalOut, 0);
    if (!continuation || link->SetOutBehaviorIO(entry->Block->GetInput(0)) != CK_OK) {
        if (continuation)
            ScriptHelper::DeleteLink(ownerScript, continuation);
        RestoreHookBlockGraph(*entry);
        ReleaseHookBlockCallback(*entry);
        RecordHookBlockDiagnostic(m_State, "InsertHookBlockAfter failed to rewire the behavior link.");
        return nullptr;
    }

    entry->PatchedLink = link;
    entry->CreatedLink = continuation;
    entry->OriginalEndpoint = originalOut;
    entry->PatchedEndpoint = HookBlockPatchEndpoint::Out;
    return RegisterHookBlockEntry(m_State, std::move(entry));
}

ScriptHookBlockRef *ScriptHookBlockService::InsertBefore(CKBehavior *ownerScript,
                                                         CKBehavior *target,
                                                         asIScriptFunction *callback,
                                                         const std::string &name,
                                                         int sourceOutput,
                                                         int targetInput) {
    CKBehaviorLink *link = FindPreviousLink(ownerScript, nullptr, target, sourceOutput, targetInput);
    if (!link) {
        RecordHookBlockDiagnostic(m_State, "InsertHookBlockBefore could not find a matching incoming behavior link.");
        return nullptr;
    }

    std::unique_ptr<ScriptHookBlockEntry> entry = CreateHookBlockEntry(m_State, ownerScript, callback, name, 1, 1);
    if (!entry)
        return nullptr;

    CKBehaviorIO *originalIn = link->GetInBehaviorIO();
    CKBehaviorLink *prefix = ScriptHelper::CreateLink(ownerScript, originalIn, entry->Block, 0);
    if (!prefix || link->SetInBehaviorIO(entry->Block->GetOutput(0)) != CK_OK) {
        if (prefix)
            ScriptHelper::DeleteLink(ownerScript, prefix);
        RestoreHookBlockGraph(*entry);
        ReleaseHookBlockCallback(*entry);
        RecordHookBlockDiagnostic(m_State, "InsertHookBlockBefore failed to rewire the behavior link.");
        return nullptr;
    }

    entry->PatchedLink = link;
    entry->CreatedLink = prefix;
    entry->OriginalEndpoint = originalIn;
    entry->PatchedEndpoint = HookBlockPatchEndpoint::In;
    return RegisterHookBlockEntry(m_State, std::move(entry));
}

ScriptHookBlockRef *ScriptHookBlockService::InsertBetween(CKBehavior *ownerScript,
                                                          CKBehavior *source,
                                                          CKBehavior *target,
                                                          asIScriptFunction *callback,
                                                          const std::string &name,
                                                          int sourceOutput,
                                                          int targetInput) {
    CKBehaviorLink *link = FindNextLink(ownerScript, source, target, sourceOutput, targetInput);
    if (!link) {
        RecordHookBlockDiagnostic(m_State, "InsertHookBlockBetween could not find the requested behavior link.");
        return nullptr;
    }

    std::unique_ptr<ScriptHookBlockEntry> entry = CreateHookBlockEntry(m_State, ownerScript, callback, name, 1, 1);
    if (!entry)
        return nullptr;

    CKBehaviorIO *originalOut = link->GetOutBehaviorIO();
    CKBehaviorLink *continuation = ScriptHelper::CreateLink(ownerScript, entry->Block, originalOut, 0);
    if (!continuation || link->SetOutBehaviorIO(entry->Block->GetInput(0)) != CK_OK) {
        if (continuation)
            ScriptHelper::DeleteLink(ownerScript, continuation);
        RestoreHookBlockGraph(*entry);
        ReleaseHookBlockCallback(*entry);
        RecordHookBlockDiagnostic(m_State, "InsertHookBlockBetween failed to rewire the behavior link.");
        return nullptr;
    }

    entry->PatchedLink = link;
    entry->CreatedLink = continuation;
    entry->OriginalEndpoint = originalOut;
    entry->PatchedEndpoint = HookBlockPatchEndpoint::Out;
    return RegisterHookBlockEntry(m_State, std::move(entry));
}

void ScriptHookBlockService::Release(ScriptDiagnostic *) {
    if (!m_State || !m_State->Active)
        return;

    std::shared_ptr<ScriptHookBlockServiceState> releasedState = m_State;
    releasedState->Active = false;
    while (!releasedState->Entries.empty()) {
        auto it = releasedState->Entries.begin();
        if (it->second) {
            RestoreHookBlockGraph(*it->second);
            ReleaseHookBlockCallback(*it->second);
        }
        releasedState->Entries.erase(it);
    }
    m_State = std::make_shared<ScriptHookBlockServiceState>();
}

size_t ScriptHookBlockService::GetActiveCount() const {
    return m_State && m_State->Active ? m_State->Entries.size() : 0;
}

} // namespace BML

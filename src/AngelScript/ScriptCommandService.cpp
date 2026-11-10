#include "ScriptStringInterop.h"
#include "Api/ModApi.h"
#include "BML/Detail/Api.hpp"

#include "ScriptCommandService.h"

#include <new>
#include <optional>
#include <utility>
#include <vector>

#include "Console/CommandContext.h"
#include "Console/Shell/ShellIo.h"
#include "Console/Shell/ShellTypes.h"
#include "ScriptAngelScriptHandle.h"
#include "ScriptFunctionSupport.h"
#include "Loader/ModContext.h"
#include "ScriptCallbackEvents.h"
#include "ScriptMod.h"
#include "ScriptModContextView.h"
#include "ScriptModRuntime.h"
#include "StringUtils.h"

namespace BML {

class ScriptCommand;

struct ScriptCommandEntry {
    ScriptCommandEntry() = default;
    ScriptCommandEntry(const ScriptCommandEntry &) = delete;
    ScriptCommandEntry &operator=(const ScriptCommandEntry &) = delete;
    ScriptCommandEntry(ScriptCommandEntry &&other) noexcept;
    ~ScriptCommandEntry();

    std::shared_ptr<ScriptControlTicket> Ticket;
    std::string Name;
    std::string Alias;
    std::string Description;
    std::string Usage;
    std::string Category;
    bool Cheat = false;
    bool Hidden = false;
    bool Enabled = true;
    unsigned int Generation = 0;
    asIScriptFunction *ExecuteMethod = nullptr;
    asIScriptFunction *CompleteMethod = nullptr;
    bool OwnsFunctionRefs = false;
    std::shared_ptr<ScriptCommand> Command;
#ifdef BML_TEST
    std::function<void(const std::vector<std::string> &)> TestExecute;
#endif
};

class ScriptCommandServiceState {
public:
    ModContext *Context = nullptr;
    ScriptMod *Owner = nullptr;
    ScriptModContextView *ContextView = nullptr;
    bool Active = false;
    unsigned int NextGeneration = 1;
    std::unordered_map<std::string, std::shared_ptr<ScriptCommandEntry>> Commands;
};

struct CommandCallArgs {
    ScriptMod *Owner = nullptr;
    ScriptModContextView *ContextView = nullptr;
    void *Event = nullptr;
    int *Result = nullptr;
    ScriptCommandCompletion *Completion = nullptr;
};

static bool IsCommandExecuteSignature(asIScriptFunction *callback) {
    const ScriptFunctionParam params[] = {
        {"BML::ModContext", asTM_INREF | asTM_CONST},
        {"BML::CommandInvocation", asTM_INREF | asTM_CONST},
    };
    return ScriptFunctionHasSignature(callback, asTYPEID_INT32, params, 2);
}

static bool IsCommandCompleteSignature(asIScriptFunction *callback) {
    const ScriptFunctionParam params[] = {
        {"BML::ModContext", asTM_INREF | asTM_CONST},
        {"BML::CommandCompletionRequest", asTM_INREF | asTM_CONST},
        {"BML::CommandCompletion", asTM_INREF | asTM_CONST},
    };
    return ScriptFunctionHasSignature(callback, asTYPEID_INT32, params, 3);
}

static bool ValidateCommandNames(ScriptMod *owner, const ScriptCommandEntry &entry) {
    if (entry.Name.empty()) {
        if (owner) {
            owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
                                                               "Command name cannot be empty."));
        }
        return false;
    }
    if (!CommandContext::IsValidCommandName(entry.Name)) {
        if (owner) {
            owner->RecordScriptDiagnostic(MakeScriptDiagnostic(
                ScriptDiagnosticPhase::Runtime, "Command name is invalid: " + entry.Name));
        }
        return false;
    }
    if (!entry.Alias.empty() && !CommandContext::IsValidCommandAlias(entry.Alias)) {
        if (owner) {
            owner->RecordScriptDiagnostic(MakeScriptDiagnostic(
                ScriptDiagnosticPhase::Runtime, "Command alias is invalid: " + entry.Alias));
        }
        return false;
    }
    return true;
}

static ScriptDiagnostic MakeAngelScriptDiagnostic(ScriptDiagnosticPhase phase, int code,
                                                  asIScriptContext *context,
                                                  const std::string &prefix) {
    ScriptDiagnostic diagnostic;
    diagnostic.Phase = phase;
    diagnostic.AngelScriptCode = code;
    diagnostic.Message = prefix;
    if (!diagnostic.Message.empty())
        diagnostic.Message += ": ";

    if (code == asEXECUTION_EXCEPTION && context) {
        const char *exception = context->GetExceptionString();
        diagnostic.Message += exception ? exception : "AngelScript exception";
        asIScriptFunction *function = context->GetExceptionFunction();
        if (function) {
            diagnostic.StackTrace = function->GetDeclaration(true, true, true);
            int column = 0;
            const int line = context->GetExceptionLineNumber(&column);
            if (line > 0) {
                diagnostic.StackTrace += ":";
                diagnostic.StackTrace += std::to_string(line);
                if (column > 0) {
                    diagnostic.StackTrace += ":";
                    diagnostic.StackTrace += std::to_string(column);
                }
            }
        }
    } else if (code == asEXECUTION_SUSPENDED) {
        diagnostic.Message += "script command callback suspended";
    } else {
        diagnostic.Message += "AngelScript call failed with code ";
        diagnostic.Message += std::to_string(code);
    }
    return diagnostic;
}

struct CommandFunctionCallArgs {
    const CommandCallArgs *Args = nullptr;
    bool Completion = false;
};

static int WriteCommandFunctionArgs(asIScriptContext *context, void *userdata) {
    auto *callArgs = static_cast<CommandFunctionCallArgs *>(userdata);
    const CommandCallArgs *args = callArgs ? callArgs->Args : nullptr;
    int code = context->SetArgObject(0, args ? args->ContextView : nullptr);
    if (code >= 0)
        code = context->SetArgObject(1, args ? args->Event : nullptr);
    if (code >= 0 && callArgs && callArgs->Completion)
        code = context->SetArgObject(2, args ? args->Completion : nullptr);
    return code;
}

static bool ExecutePreparedMethod(asIScriptObject *object, asIScriptFunction *method,
                                  const CommandCallArgs &args, bool completion,
                                  const char *failurePrefix, ScriptDiagnostic &diagnostic) {
    if (!method || !args.ContextView || !args.Event) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Callback,
                                          "Command callback has invalid runtime state.");
        return false;
    }

    CommandFunctionCallArgs functionArgs = {&args, completion};
    ScriptFunctionCall call;
    call.Function = method;
    call.Object = object;
    call.Owner = args.Owner;
    call.Phase = ScriptDiagnosticPhase::Callback;
    call.FailurePrefix = failurePrefix;
    call.InvalidStateMessage = "Command callback has invalid runtime state.";
    call.ContextFailureMessage = "Unable to create AngelScript context for command callback.";
    call.SuspendedMessage = "script command callback suspended";
    call.WriteArgs = WriteCommandFunctionArgs;
    call.ReadResult = [](asIScriptContext *ctx, void *data) -> int {
        const auto *a = static_cast<CommandFunctionCallArgs *>(data);
        if (a->Args->Result)
            *a->Args->Result = static_cast<int>(ctx->GetReturnDWord());
        return asSUCCESS;
    };
    call.UserData = &functionArgs;
    return ExecuteScriptFunction(call, diagnostic);
}

static void ReleaseScriptCommandObject(ScriptCommandEntry &entry) {

    if (entry.OwnsFunctionRefs) {
        if (entry.ExecuteMethod)
            entry.ExecuteMethod->Release();
        if (entry.CompleteMethod)
            entry.CompleteMethod->Release();
        entry.ExecuteMethod = nullptr;
        entry.CompleteMethod = nullptr;
        entry.OwnsFunctionRefs = false;
    }
}

ScriptCommandEntry::ScriptCommandEntry(ScriptCommandEntry &&other) noexcept
    : Ticket(std::move(other.Ticket)), Name(std::move(other.Name)), Alias(std::move(other.Alias)),
      Description(std::move(other.Description)), Usage(std::move(other.Usage)),
      Category(std::move(other.Category)), Cheat(other.Cheat), Hidden(other.Hidden),
      Enabled(other.Enabled), Generation(other.Generation),
      ExecuteMethod(std::exchange(other.ExecuteMethod, nullptr)),
      CompleteMethod(std::exchange(other.CompleteMethod, nullptr)),
      OwnsFunctionRefs(std::exchange(other.OwnsFunctionRefs, false)),
#ifdef BML_TEST
      Command(std::move(other.Command)), TestExecute(std::move(other.TestExecute))
#else
      Command(std::move(other.Command))
#endif
{
}

ScriptCommandEntry::~ScriptCommandEntry() {
    ReleaseScriptCommandObject(*this);
}

class ScriptCommand final {
public:
    ScriptCommand(std::weak_ptr<ScriptCommandServiceState> state,
                  std::weak_ptr<ScriptCommandEntry> entry, std::string name, std::string alias,
                  std::string description, bool cheat)
        : m_State(std::move(state)), m_Entry(std::move(entry)), m_Name(std::move(name)),
          m_Alias(std::move(alias)), m_Description(std::move(description)), m_Cheat(cheat) {}

    std::string GetName() { return m_Name; }

    std::string GetAlias() { return m_Alias; }

    std::string GetDescription() { return m_Description; }

    bool IsCheat() { return m_Cheat; }

    void Execute(ModContext *, const std::vector<std::string> &args) {
        std::shared_ptr<ScriptCommandServiceState> state = m_State.lock();
        if (!state || !state->Active || !state->ContextView)
            return;
        if (state->Owner && !state->Owner->CanDispatchScriptServiceCallback())
            return;

        const std::shared_ptr<ScriptCommandEntry> entry = m_Entry.lock();
        if (!entry || !entry->ExecuteMethod)
            return;

        auto frame = std::make_shared<ScriptCommandFrame>();
        frame->Name = entry->Name;
        frame->InvokedAs = args.empty() ? entry->Name : args.front();
        if (args.size() > 1)
            frame->Arguments.assign(args.begin() + 1, args.end());
        if (const auto *input = Shell::GetInput()) {
            frame->HasInput = true;
            frame->Input = *input;
        }
        frame->Context = state->Context;

        struct Retire {
            std::shared_ptr<ScriptCommandFrame> Frame;

            ~Retire() {
                Frame->Active = false;
                Frame->Context = nullptr;
            }
        } retire{frame};

        ScriptCommandInvocation invocation;
        invocation.Frame = frame;
        int status = Shell::Status::Failure;
        CommandCallArgs callArgs = {state->Owner, state->ContextView, &invocation, &status,
                                    nullptr};
        ScriptDiagnostic diagnostic;
        const bool ok = ExecutePreparedMethod(nullptr, entry->ExecuteMethod, callArgs, false,
                                              "Command callback failed", diagnostic);
        Shell::SetStatus(status);
        if (!ok) {
            Shell::SetStatus(Shell::Status::Failure);
            if (state->Owner)
                state->Owner->SetLoadFailure(diagnostic);
        }
    }

    const std::vector<std::string> GetTabCompletion(ModContext *,
                                                    const std::vector<std::string> &args) {
        std::shared_ptr<ScriptCommandServiceState> state = m_State.lock();
        if (!state || !state->Active || !state->ContextView)
            return {};
        if (state->Owner && !state->Owner->CanDispatchScriptServiceCallback())
            return {};

        const std::shared_ptr<ScriptCommandEntry> entry = m_Entry.lock();
        if (!entry || !entry->CompleteMethod)
            return {};

        std::vector<std::string> completions;
        auto frame = std::make_shared<ScriptCommandFrame>();
        frame->Name = entry->Name;
        frame->InvokedAs = args.empty() ? entry->Name : args.front();
        if (args.size() > 1)
            frame->Arguments.assign(args.begin() + 1, args.end());
        frame->ActiveArgument =
            frame->Arguments.empty() ? 0 : unsigned(frame->Arguments.size() - 1);
        frame->Prefix = frame->Arguments.empty() ? "" : frame->Arguments.back();
        frame->Items = &completions;

        struct Retire {
            std::shared_ptr<ScriptCommandFrame> Frame;

            ~Retire() {
                Frame->Active = false;
                Frame->Items = nullptr;
            }
        } retire{frame};

        ScriptCommandCompletion completion;
        completion.Frame = frame;
        ScriptCommandCompletionRequest request;
        request.Frame = frame;
        int status = BML_OK;
        CommandCallArgs callArgs = {state->Owner, state->ContextView, &request, &status,
                                    &completion};
        ScriptDiagnostic diagnostic;
        const bool ok = ExecutePreparedMethod(nullptr, entry->CompleteMethod, callArgs, true,
                                              "Command completion callback failed", diagnostic);
        if (!ok) {
            if (state->Owner)
                state->Owner->SetLoadFailure(diagnostic);
            return {};
        }
        return status == BML_OK && frame->Status == BML_OK ? completions
                                                           : std::vector<std::string>{};
    }

private:
    std::weak_ptr<ScriptCommandServiceState> m_State;
    std::weak_ptr<ScriptCommandEntry> m_Entry;
    std::string m_Name;
    std::string m_Alias;
    std::string m_Description;
    bool m_Cheat = false;
};

static std::shared_ptr<ScriptCommand>
CreateScriptCommand(const std::shared_ptr<ScriptCommandServiceState> &state,
                    const std::shared_ptr<ScriptCommandEntry> &entry) {
    return std::make_shared<ScriptCommand>(state, entry, entry->Name, entry->Alias,
                                           entry->Description, entry->Cheat);
}

static ScriptCommandRef *
RegisterCommandEntry(const std::shared_ptr<ScriptCommandServiceState> &state,
                     const std::string &key, ScriptCommandEntry &&entry, int *status = nullptr) {
    if (status)
        *status = BML_ERROR_FAIL;
    if (state->ContextView)
        entry.Ticket =
            std::make_shared<ScriptControlTicket>(state->ContextView->RuntimeScope().lock());
    const unsigned int generation = entry.Generation;
    std::shared_ptr<ScriptCommandEntry> stored;
    try {
        stored = std::make_shared<ScriptCommandEntry>(std::move(entry));
    } catch (const std::bad_alloc &) {
        if (status)
            *status = BML_ERROR_OUT_OF_MEMORY;
        state->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(
            ScriptDiagnosticPhase::Runtime, "Unable to retain registered command."));
        return nullptr;
    }

    try {
        stored->Command = CreateScriptCommand(state, stored);
    } catch (const std::bad_alloc &) {
        if (status)
            *status = BML_ERROR_OUT_OF_MEMORY;
        state->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(
            ScriptDiagnosticPhase::Runtime, "Unable to create command wrapper."));
        return nullptr;
    }

    ScriptCommandRef *ref = new (std::nothrow) ScriptCommandRef(state, key, generation);
    if (!ref) {
        state->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(
            ScriptDiagnosticPhase::Runtime, "Unable to create command reference."));
        return nullptr;
    }

    ref->Ticket = stored->Ticket;
    try {
        auto [position, inserted] = state->Commands.emplace(key, stored);
        if (!inserted) {
            ref->Release();
            return nullptr;
        }
    } catch (const std::bad_alloc &) {
        if (status)
            *status = BML_ERROR_OUT_OF_MEMORY;
        ref->Release();
        state->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(
            ScriptDiagnosticPhase::Runtime, "Unable to retain registered command."));
        return nullptr;
    }

    // The registry retains the complete script entry for every acquired call,
    // so removal can stop new lookup without invalidating an in-progress
    // Execute or Complete callback.
    CommandContext::CommandInfo info;
    info.Name = stored->Name;
    info.Alias = stored->Alias;
    info.Description = stored->Description;
    info.Usage = stored->Usage;
    info.Category = stored->Category;
    info.Cheat = stored->Cheat;
    info.Hidden = stored->Hidden;
    info.Enabled = stored->Enabled;
    if (!state->Context->GetCommandContext().RegisterCommand(state.get(), stored->Command.get(),
                                                             std::move(info), stored)) {
        const std::string commandName = stored->Name;
        state->Commands.erase(key);
        ref->Release();
        state->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(
            ScriptDiagnosticPhase::Runtime, "Command registration failed: " + commandName));
        return nullptr;
    }

    if (status)
        *status = BML_OK;
    return ref;
}

#ifdef BML_TEST
static bool InvokeTestCommand(const std::shared_ptr<ScriptCommandServiceState> &state,
                              const std::string &key, const std::vector<std::string> &args) {
    if (!state || !state->Active)
        return false;

    auto it = state->Commands.find(key);
    if (it == state->Commands.end() || !it->second->Enabled || !it->second->TestExecute)
        return false;

    const std::shared_ptr<ScriptCommandEntry> entry = it->second;
    entry->TestExecute(args);
    return true;
}
#endif

std::string ScriptCommandInvocation::GetName() const {
    return Frame && Frame->Active ? Frame->Name : "";
}

std::string ScriptCommandInvocation::GetInvokedAs() const {
    return Frame && Frame->Active ? Frame->InvokedAs : "";
}

unsigned ScriptCommandInvocation::GetArgumentCount() const {
    return Frame && Frame->Active ? unsigned(Frame->Arguments.size()) : 0;
}

int ScriptCommandInvocation::ReadArgument(unsigned index, std::string &out) const {
    if (!Frame || !Frame->Active)
        return BML_ERROR_INVALID_HANDLE;
    if (index >= Frame->Arguments.size())
        return BML_ERROR_NOT_FOUND;
    out = Frame->Arguments[index];
    return BML_OK;
}

bool ScriptCommandInvocation::HasInput() const {
    return Frame && Frame->Active && Frame->HasInput;
}

std::string ScriptCommandInvocation::GetInput() const {
    return HasInput() ? Frame->Input : "";
}

int ScriptCommandInvocation::Write(const std::string &text) const {
    if (!Frame || !Frame->Active || !Frame->Context)
        return BML_ERROR_INVALID_HANDLE;
    if ((!BML::Detail::IsText(text) || !utils::IsValidUtf8(text)))
        return BML_ERROR_INVALID_PARAMETER;
    Frame->Context->SendIngameMessage(text.c_str());
    return BML_OK;
}

unsigned ScriptCommandCompletionRequest::GetActiveArgument() const {
    return Frame && Frame->Active ? Frame->ActiveArgument : 0;
}

std::string ScriptCommandCompletionRequest::GetPrefix() const {
    return Frame && Frame->Active ? Frame->Prefix : "";
}

ScriptCommandCompletion::ScriptCommandCompletion(std::vector<std::string> *items)
    : Frame(std::make_shared<ScriptCommandFrame>()) {
    Frame->Items = items;
}

int ScriptCommandCompletion::Add(const std::string &value) const {
    if (!Frame || !Frame->Active || !Frame->Items)
        return BML_ERROR_INVALID_HANDLE;
    if (Frame->Status != BML_OK)
        return Frame->Status;
    if (value.empty() || value.size() > Shell::Limits::MaxCompletionBytes ||
        (!BML::Detail::IsText(value) || !utils::IsValidUtf8(value)))
        return Frame->Status = BML_ERROR_INVALID_PARAMETER;
    if (Frame->Items->size() >= Shell::Limits::MaxCompletionCandidates)
        return Frame->Status = BML_ERROR_WOULD_BLOCK;
    Frame->Items->push_back(value);
    return BML_OK;
}

int ScriptCommandCompletion::ReadCount(unsigned &out) const {
    if (!Frame || !Frame->Active || !Frame->Items)
        return BML_ERROR_INVALID_HANDLE;
    out = unsigned(Frame->Items->size());
    return BML_OK;
}

int ScriptCommandCompletion::ReadCandidate(unsigned index, std::string &out) const {
    if (!Frame || !Frame->Active || !Frame->Items)
        return BML_ERROR_INVALID_HANDLE;
    if (index >= Frame->Items->size())
        return BML_ERROR_NOT_FOUND;
    out = (*Frame->Items)[index];
    return BML_OK;
}

int ScriptCommandCompletion::GetCount() const {
    unsigned n = 0;
    ReadCount(n);
    return int(n);
}

std::string ScriptCommandCompletion::At(int index) const {
    std::string out;
    if (index >= 0)
        ReadCandidate(unsigned(index), out);
    return out;
}

ScriptCommandRef::ScriptCommandRef(std::weak_ptr<ScriptCommandServiceState> state, std::string key,
                                   unsigned int generation)
    : m_State(std::move(state)), m_Key(std::move(key)), m_Generation(generation) {}

void ScriptCommandRef::AddRef() {
    ++m_RefCount;
}

void ScriptCommandRef::Release() {
    if (--m_RefCount == 0)
        delete this;
}

bool ScriptCommandRef::IsValid() const {
    std::optional<ScriptRuntimeAccess> access;
    if (Ticket) {
        access.emplace(*Ticket);
        if (access->Code() != BML_OK)
            return false;
    }
    std::shared_ptr<ScriptCommandServiceState> state = m_State.lock();
    if (!state || !state->Active)
        return false;
    if (Ticket && Ticket->Terminal)
        return false;
    auto it = state->Commands.find(m_Key);
    return it != state->Commands.end() && it->second->Generation == m_Generation;
}

std::string ScriptCommandRef::GetName() const {
    std::shared_ptr<ScriptCommandServiceState> state = m_State.lock();
    if (!state)
        return {};
    auto it = state->Commands.find(m_Key);
    return it != state->Commands.end() && it->second->Generation == m_Generation ? it->second->Name
                                                                                 : std::string();
}

std::string ScriptCommandRef::GetAlias() const {
    std::shared_ptr<ScriptCommandServiceState> state = m_State.lock();
    if (!state)
        return {};
    auto it = state->Commands.find(m_Key);
    return it != state->Commands.end() && it->second->Generation == m_Generation ? it->second->Alias
                                                                                 : std::string();
}

bool ScriptCommandRef::IsCheat() const {
    std::shared_ptr<ScriptCommandServiceState> state = m_State.lock();
    if (!state)
        return false;
    auto it = state->Commands.find(m_Key);
    return it != state->Commands.end() && it->second->Generation == m_Generation &&
           it->second->Cheat;
}

bool ScriptCommandRef::IsEnabled() const {
    std::shared_ptr<ScriptCommandServiceState> state = m_State.lock();
    if (!state)
        return false;
    auto it = state->Commands.find(m_Key);
    return it != state->Commands.end() && it->second->Generation == m_Generation &&
           it->second->Enabled;
}

int ScriptCommandRef::Read(CommandInfo &out) const {
    if (!Ticket)
        return BML_ERROR_INVALID_HANDLE;
    ScriptRuntimeAccess access(*Ticket);
    if (access.Code() != BML_OK)
        return access.Code();
    auto state = m_State.lock();
    if (!state || !state->Active)
        return BML_ERROR_INVALID_HANDLE;
    auto it = state->Commands.find(m_Key);
    if (it == state->Commands.end() || it->second->Generation != m_Generation)
        return BML_ERROR_INVALID_HANDLE;
    if (Ticket->Terminal || it->second->Ticket != Ticket)
        return BML_ERROR_INVALID_HANDLE;
    const auto &e = *it->second;
    auto *record = state->Context->FindCommand(e.Name.c_str());
    if (!record || record->UserData != e.Command.get())
        return BML_ERROR_INVALID_HANDLE;
    out = record->Info;
    return BML_OK;
}

int ScriptCommandRef::SetEnabled(bool enabled) {
    if (!Ticket)
        return BML_ERROR_INVALID_HANDLE;
    ScriptRuntimeAccess access(*Ticket);
    if (access.Code() != BML_OK)
        return access.Code();
    auto state = m_State.lock();
    if (!state || !state->Active)
        return BML_ERROR_INVALID_HANDLE;
    auto it = state->Commands.find(m_Key);
    if (it == state->Commands.end() || it->second->Generation != m_Generation)
        return BML_ERROR_INVALID_HANDLE;
    if (Ticket->Terminal || it->second->Ticket != Ticket)
        return BML_ERROR_INVALID_HANDLE;
    if (!state->Context->SetCommandEnabled(it->second->Command.get(), enabled))
        return BML_ERROR_FAIL;
    it->second->Enabled = enabled;
    return BML_OK;
}

int ScriptCommandRef::Unregister() {
    if (!Ticket)
        return BML_ERROR_INVALID_HANDLE;
    ScriptRuntimeAccess access(*Ticket, true);
    if (access.Code() != BML_OK || Ticket->Terminal)
        return access.Code();
    auto state = m_State.lock();
    if (!state || !state->Active || !state->Owner)
        return BML_ERROR_INVALID_HANDLE;
    if (!state->Owner->CommandsForAuthoring().Unregister(m_Key))
        return BML_ERROR_FAIL;
    Ticket->Terminal = true;
    return BML_OK;
}

ScriptCommandService::ScriptCommandService()
    : m_State(std::make_shared<ScriptCommandServiceState>()) {}

ScriptCommandService::~ScriptCommandService() {
    try {
        Release(nullptr);
    } catch (...) {
    }
}

bool ScriptCommandService::Bind(ModContext *context, ScriptMod *owner,
                                ScriptModContextView *contextView) {
    if (!m_State) {
        try {
            m_State = std::make_shared<ScriptCommandServiceState>();
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

int ScriptCommandService::Register(const ScriptCommandDefinition &definition,
                                   asIScriptFunction *execute, asIScriptFunction *complete,
                                   ScriptCommandRef *&out) try {
    out = nullptr;
    if (!m_State || !execute || !m_State->Active || !m_State->Context || !m_State->Owner)
        return BML_ERROR_INVALID_PARAMETER;

    if (!IsCommandExecuteSignature(execute)) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(
            ScriptDiagnosticPhase::Runtime, "RegisterCommand requires BML::CommandExecute."));
        return BML_ERROR_INVALID_PARAMETER;
    }
    if (complete && !IsCommandCompleteSignature(complete)) {
        m_State->Owner->RecordScriptDiagnostic(
            MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
                                 "RegisterCommand completion requires BML::CommandComplete."));
        return BML_ERROR_INVALID_PARAMETER;
    }

    ScriptCommandEntry entry;
    entry.Name = definition.Name;
    entry.Alias = definition.Alias;
    entry.Description = definition.Description;
    entry.Usage = definition.Usage;
    entry.Category = definition.Category;
    entry.Cheat = definition.Cheat;
    entry.Hidden = definition.Hidden;
    entry.Enabled = definition.Enabled;

    if (!ValidateCommandNames(m_State->Owner, entry))
        return BML_ERROR_INVALID_PARAMETER;

    const std::string key = CommandContext::NormalizeCommandName(entry.Name.c_str());
    if (m_State->Commands.find(key) != m_State->Commands.end()) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(
            ScriptDiagnosticPhase::Runtime,
            "Command name is already registered by this script mod: " + entry.Name));
        return BML_ERROR_INVALID_PARAMETER;
    }
    if (m_State->Context->FindCommand(entry.Name.c_str())) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(
            ScriptDiagnosticPhase::Runtime, "Command name is already registered: " + entry.Name));
        return BML_ERROR_INVALID_PARAMETER;
    }
    if (!entry.Alias.empty() && m_State->Context->FindCommand(entry.Alias.c_str())) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(
            ScriptDiagnosticPhase::Runtime, "Command alias is already registered: " + entry.Alias));
        return BML_ERROR_INVALID_PARAMETER;
    }

    execute->AddRef();
    if (complete)
        complete->AddRef();
    entry.ExecuteMethod = execute;
    entry.CompleteMethod = complete;
    entry.OwnsFunctionRefs = true;
    entry.Generation = m_State->NextGeneration++;

    int code = BML_ERROR_FAIL;
    out = RegisterCommandEntry(m_State, key, std::move(entry), &code);
    return code;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}

bool ScriptCommandService::Unregister(const std::string &name) {
    if (!m_State || !m_State->Context)
        return false;
    const std::string key = CommandContext::NormalizeCommandName(name.c_str());
    auto it = m_State->Commands.find(key);
    if (it == m_State->Commands.end())
        return false;

    const auto result = m_State->Context->GetCommandContext().UnregisterCommand(
        m_State.get(), it->second->Name.c_str());
    if (result != CommandContext::UnregisterResult::Success &&
        result != CommandContext::UnregisterResult::NotFound) {
        return false;
    }
    if (it->second->Ticket)
        it->second->Ticket->Terminal = true;
    m_State->Commands.erase(it);
    return true;
}

void ScriptCommandService::Release(ScriptDiagnostic *) {
    if (!m_State || !m_State->Active)
        return;

    std::shared_ptr<ScriptCommandServiceState> releasedState = m_State;
    releasedState->Active = false;
    if (releasedState->Context) {
        releasedState->Context->GetCommandContext().UnregisterCommands(releasedState.get());
    }
    for (auto &[key, entry] : releasedState->Commands)
        if (entry->Ticket)
            entry->Ticket->Terminal = true;
    releasedState->Commands.clear();
    m_State.reset();
}

size_t ScriptCommandService::GetActiveCount() const {
    return m_State && m_State->Active ? m_State->Commands.size() : 0;
}

#ifdef BML_TEST
ScriptCommandRef *ScriptCommandService::AddTestCommandForRelease(
    const std::string &name, const std::string &alias,
    std::function<void(const std::vector<std::string> &)> execute) {
    if (!m_State || name.empty())
        return nullptr;
    m_State->Active = true;

    ScriptCommandEntry entry;
    entry.Name = name;
    entry.Alias = alias;
    entry.Enabled = true;
    entry.Generation = m_State->NextGeneration++;
    entry.TestExecute = std::move(execute);

    const std::string key = CommandContext::NormalizeCommandName(entry.Name.c_str());
    const unsigned int generation = entry.Generation;
    ScriptCommandRef *ref = new (std::nothrow) ScriptCommandRef(m_State, key, generation);
    if (!ref)
        return nullptr;
    try {
        m_State->Commands.emplace(key, std::make_shared<ScriptCommandEntry>(std::move(entry)));
    } catch (const std::bad_alloc &) {
        ref->Release();
        return nullptr;
    }
    return ref;
}

bool ScriptCommandService::InvokeTestCommandForRelease(const std::string &name,
                                                       const std::vector<std::string> &args) {
    return InvokeTestCommand(m_State, CommandContext::NormalizeCommandName(name.c_str()), args);
}
#endif

} // namespace BML

#include "ScriptCommandService.h"

#include <algorithm>
#include <cctype>
#include <utility>
#include <vector>

#include <angelscript.h>

#include "CommandContext.h"
#include "ScriptAngelScriptHandle.h"
#include "ScriptFunctionSupport.h"
#include "ModContext.h"
#include "ScriptCallbackEvents.h"
#include "ScriptMod.h"
#include "ScriptModContextView.h"
#include "ScriptModRuntime.h"

namespace BML {

static constexpr const char *kCommandInterfaceDecl = "BML::Command";
static constexpr const char *kCommandExecuteDecl =
    "void Execute(const BML::ModContext &in, const BML::CommandEvent &in)";
static constexpr const char *kCommandCompleteDecl =
    "void Complete(const BML::ModContext &in, const BML::CommandEvent &in, BML::CommandCompletion &inout)";

struct ScriptCommandEntry {
    std::string Name;
    std::string Alias;
    std::string Description;
    std::string Usage;
    std::string Category;
    bool Cheat = false;
    bool Hidden = false;
    bool Enabled = true;
    unsigned int Generation = 0;
    asIScriptObject *Object = nullptr;
    asIScriptFunction *ExecuteMethod = nullptr;
    asIScriptFunction *CompleteMethod = nullptr;
    bool OwnsFunctionRefs = false;
    std::unique_ptr<ICommand> Command;
    int ActiveCalls = 0;
    bool PendingUnregister = false;
};

class ScriptCommandServiceState {
public:
    ModContext *Context = nullptr;
    ScriptMod *Owner = nullptr;
    ScriptModContextView *ContextView = nullptr;
    bool Active = false;
    unsigned int NextGeneration = 1;
    std::unordered_map<std::string, ScriptCommandEntry> Commands;
    std::vector<std::unique_ptr<ICommand>> RetiredCommands;
};

struct CommandCallArgs {
    ScriptMod *Owner = nullptr;
    ScriptModContextView *ContextView = nullptr;
    ScriptCommandEventView *Event = nullptr;
    ScriptCommandCompletion *Completion = nullptr;
};

static std::string NormalizeCommandName(const std::string &name) {
    std::string normalized = name;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return normalized;
}

static bool IsCommandExecuteSignature(asIScriptFunction *callback) {
    const ScriptFunctionParam params[] = {
        {"BML::ModContext", asTM_INREF | asTM_CONST},
        {"BML::CommandEvent", asTM_INREF | asTM_CONST},
    };
    return ScriptFunctionHasSignature(callback, asTYPEID_VOID, params, 2);
}

static bool IsCommandCompleteSignature(asIScriptFunction *callback) {
    const ScriptFunctionParam params[] = {
        {"BML::ModContext", asTM_INREF | asTM_CONST},
        {"BML::CommandEvent", asTM_INREF | asTM_CONST},
        {"BML::CommandCompletion", asTM_INOUTREF},
    };
    return ScriptFunctionHasSignature(callback, asTYPEID_VOID, params, 3);
}

static ScriptDiagnostic MakeAngelScriptDiagnostic(ScriptDiagnosticPhase phase,
                                                  int code,
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

static bool ExecutePreparedMethod(asIScriptObject *object,
                                  asIScriptFunction *method,
                                  const CommandCallArgs &args,
                                  bool completion,
                                  const char *failurePrefix,
                                  ScriptDiagnostic &diagnostic) {
    if (!method || !args.ContextView || !args.Event) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Callback, "Command callback has invalid runtime state.");
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
    call.UserData = &functionArgs;
    return ExecuteScriptFunction(call, diagnostic);
}

static bool ReadStringProperty(asIScriptObject *object,
                               ScriptMod *owner,
                               const char *declaration,
                               bool required,
                               std::string &value,
                               ScriptDiagnostic &diagnostic) {
    asIScriptFunction *method = object && object->GetObjectType()
                                    ? object->GetObjectType()->GetMethodByDecl(declaration)
                                    : nullptr;
    if (!method) {
        if (!required) {
            value.clear();
            return true;
        }
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
                                          std::string("Command object is missing property accessor ") + declaration);
        return false;
    }

    ScriptHostCallScope activeCall(owner);
    if (owner && !activeCall.Entered()) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Script mod reload is in progress.");
        diagnostic.Status = CKAS_INUSE;
        return false;
    }

    asIScriptContext *context = object->GetEngine()->CreateContext();
    if (!context) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Unable to create AngelScript context for command property.");
        return false;
    }

    ScriptCurrentModScope callScope(owner);
    int code = context->Prepare(method);
    if (code >= 0)
        code = context->SetObject(object);
    if (code >= 0)
        code = context->Execute();

    if (code == asEXECUTION_SUSPENDED)
        context->Abort();
    if (code != asEXECUTION_FINISHED) {
        diagnostic = MakeAngelScriptDiagnostic(ScriptDiagnosticPhase::Runtime, code, context, "Command property read failed");
        context->Release();
        return false;
    }

    auto *result = static_cast<std::string *>(context->GetReturnObject());
    value = result ? *result : std::string();
    context->Release();
    return true;
}

static bool ReadBoolProperty(asIScriptObject *object,
                             ScriptMod *owner,
                             const char *declaration,
                             bool defaultValue,
                             bool &value,
                             ScriptDiagnostic &diagnostic) {
    asIScriptFunction *method = object && object->GetObjectType()
                                    ? object->GetObjectType()->GetMethodByDecl(declaration)
                                    : nullptr;
    if (!method) {
        value = defaultValue;
        return true;
    }

    ScriptHostCallScope activeCall(owner);
    if (owner && !activeCall.Entered()) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Script mod reload is in progress.");
        diagnostic.Status = CKAS_INUSE;
        return false;
    }

    asIScriptContext *context = object->GetEngine()->CreateContext();
    if (!context) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Unable to create AngelScript context for command property.");
        return false;
    }

    ScriptCurrentModScope callScope(owner);
    int code = context->Prepare(method);
    if (code >= 0)
        code = context->SetObject(object);
    if (code >= 0)
        code = context->Execute();

    if (code == asEXECUTION_SUSPENDED)
        context->Abort();
    if (code != asEXECUTION_FINISHED) {
        diagnostic = MakeAngelScriptDiagnostic(ScriptDiagnosticPhase::Runtime, code, context, "Command property read failed");
        context->Release();
        return false;
    }

    value = context->GetReturnByte() != 0;
    context->Release();
    return true;
}

static void ReleaseScriptCommandObject(ScriptCommandEntry &entry) {
    if (entry.Object) {
        entry.Object->Release();
        entry.Object = nullptr;
    }
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

static void FinishCommandCall(const std::shared_ptr<ScriptCommandServiceState> &state, const std::string &key) {
    if (!state)
        return;
    auto it = state->Commands.find(key);
    if (it == state->Commands.end())
        return;
    if (it->second.ActiveCalls > 0)
        --it->second.ActiveCalls;
    if (it->second.PendingUnregister && it->second.ActiveCalls == 0) {
        ReleaseScriptCommandObject(it->second);
        state->RetiredCommands.push_back(std::move(it->second.Command));
        state->Commands.erase(it);
    }
}

class ScriptCommand final : public ICommand {
public:
    ScriptCommand(std::weak_ptr<ScriptCommandServiceState> state,
                  std::string key,
                  std::string name,
                  std::string alias,
                  std::string description,
                  bool cheat)
        : m_State(std::move(state)),
          m_Key(std::move(key)),
          m_Name(std::move(name)),
          m_Alias(std::move(alias)),
          m_Description(std::move(description)),
          m_Cheat(cheat) {}

    std::string GetName() override { return m_Name; }
    std::string GetAlias() override { return m_Alias; }
    std::string GetDescription() override { return m_Description; }
    bool IsCheat() override { return m_Cheat; }

    void Execute(IBML *, const std::vector<std::string> &args) override {
        std::shared_ptr<ScriptCommandServiceState> state = m_State.lock();
        if (!state || !state->Active || !state->ContextView)
            return;

        const std::string key = m_Key;
        auto it = state->Commands.find(key);
        if (it == state->Commands.end() || !it->second.ExecuteMethod || !it->second.Enabled)
            return;
        ++it->second.ActiveCalls;

        ScriptCommandEventView event(ScriptCommandEventExecute, this, &args);
        CommandCallArgs callArgs = {state->Owner, state->ContextView, &event, nullptr};
        ScriptDiagnostic diagnostic;
        const bool ok = ExecutePreparedMethod(it->second.Object,
                                              it->second.ExecuteMethod,
                                              callArgs,
                                              false,
                                              "Command callback failed",
                                              diagnostic);
        FinishCommandCall(state, key);
        if (!ok && state->Owner)
            state->Owner->SetLoadFailure(diagnostic);
    }

    const std::vector<std::string> GetTabCompletion(IBML *, const std::vector<std::string> &args) override {
        std::shared_ptr<ScriptCommandServiceState> state = m_State.lock();
        if (!state || !state->Active || !state->ContextView)
            return {};

        const std::string key = m_Key;
        auto it = state->Commands.find(key);
        if (it == state->Commands.end() || !it->second.CompleteMethod || !it->second.Enabled)
            return {};
        ++it->second.ActiveCalls;

        std::vector<std::string> completions;
        ScriptCommandCompletion completion(&completions);
        ScriptCommandEventView event(ScriptCommandEventComplete, this, &args);
        CommandCallArgs callArgs = {state->Owner, state->ContextView, &event, &completion};
        ScriptDiagnostic diagnostic;
        const bool ok = ExecutePreparedMethod(it->second.Object,
                                              it->second.CompleteMethod,
                                              callArgs,
                                              true,
                                              "Command completion callback failed",
                                              diagnostic);
        FinishCommandCall(state, key);
        if (!ok) {
            if (state->Owner)
                state->Owner->SetLoadFailure(diagnostic);
            return {};
        }
        return completions;
    }

private:
    std::weak_ptr<ScriptCommandServiceState> m_State;
    std::string m_Key;
    std::string m_Name;
    std::string m_Alias;
    std::string m_Description;
    bool m_Cheat = false;
};

void ScriptCommandCompletion::Add(const std::string &value) const {
    if (m_Items && !value.empty())
        m_Items->push_back(value);
}

int ScriptCommandCompletion::GetCount() const {
    return m_Items ? static_cast<int>(m_Items->size()) : 0;
}

std::string ScriptCommandCompletion::At(int index) const {
    if (!m_Items || index < 0 || static_cast<size_t>(index) >= m_Items->size())
        return {};
    return (*m_Items)[static_cast<size_t>(index)];
}

ScriptCommandRef::ScriptCommandRef(std::weak_ptr<ScriptCommandServiceState> state,
                                   std::string key,
                                   unsigned int generation)
    : m_State(std::move(state)),
      m_Key(std::move(key)),
      m_Generation(generation) {}

void ScriptCommandRef::AddRef() { ++m_RefCount; }
void ScriptCommandRef::Release() {
    if (--m_RefCount == 0)
        delete this;
}

bool ScriptCommandRef::IsValid() const {
    std::shared_ptr<ScriptCommandServiceState> state = m_State.lock();
    if (!state || !state->Active)
        return false;
    auto it = state->Commands.find(m_Key);
    return it != state->Commands.end() && it->second.Generation == m_Generation && !it->second.PendingUnregister;
}

std::string ScriptCommandRef::GetName() const {
    std::shared_ptr<ScriptCommandServiceState> state = m_State.lock();
    if (!state)
        return {};
    auto it = state->Commands.find(m_Key);
    return it != state->Commands.end() && it->second.Generation == m_Generation ? it->second.Name : std::string();
}

std::string ScriptCommandRef::GetAlias() const {
    std::shared_ptr<ScriptCommandServiceState> state = m_State.lock();
    if (!state)
        return {};
    auto it = state->Commands.find(m_Key);
    return it != state->Commands.end() && it->second.Generation == m_Generation ? it->second.Alias : std::string();
}

bool ScriptCommandRef::IsCheat() const {
    std::shared_ptr<ScriptCommandServiceState> state = m_State.lock();
    if (!state)
        return false;
    auto it = state->Commands.find(m_Key);
    return it != state->Commands.end() && it->second.Generation == m_Generation && it->second.Cheat;
}

bool ScriptCommandRef::IsEnabled() const {
    std::shared_ptr<ScriptCommandServiceState> state = m_State.lock();
    if (!state)
        return false;
    auto it = state->Commands.find(m_Key);
    return it != state->Commands.end() && it->second.Generation == m_Generation && it->second.Enabled;
}

bool ScriptCommandRef::SetEnabled(bool enabled) {
    std::shared_ptr<ScriptCommandServiceState> state = m_State.lock();
    if (!state || !state->Active)
        return false;
    auto it = state->Commands.find(m_Key);
    if (it == state->Commands.end() || it->second.Generation != m_Generation || it->second.PendingUnregister)
        return false;
    it->second.Enabled = enabled;
    return true;
}

bool ScriptCommandRef::Unregister() {
    std::shared_ptr<ScriptCommandServiceState> state = m_State.lock();
    if (!state || !state->Active || !state->Owner)
        return false;
    auto it = state->Commands.find(m_Key);
    if (it == state->Commands.end() || it->second.Generation != m_Generation)
        return false;
    return state->Owner->UnregisterScriptCommand(it->second.Name);
}

ScriptCommandService::ScriptCommandService() : m_State(std::make_shared<ScriptCommandServiceState>()) {}
ScriptCommandService::~ScriptCommandService() { Release(nullptr); }

void ScriptCommandService::Bind(ModContext *context,
                                ScriptMod *owner,
                                ScriptModContextView *contextView) {
    m_State->Context = context;
    m_State->Owner = owner;
    m_State->ContextView = contextView;
    m_State->Active = true;
}

ScriptCommandRef *ScriptCommandService::Register(asIScriptObject *command) {
    if (!command || !m_State->Active || !m_State->Context || !m_State->Owner || !command->GetObjectType())
        return nullptr;

    asITypeInfo *commandInterface = command->GetEngine()->GetTypeInfoByDecl(kCommandInterfaceDecl);
    if (!commandInterface || !command->GetObjectType()->Implements(commandInterface)) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "RegisterCommand requires an object implementing BML::Command."));
        return nullptr;
    }

    ScriptDiagnostic diagnostic;
    ScriptCommandEntry entry;
    if (!ReadStringProperty(command, m_State->Owner, "string get_Name() const", true, entry.Name, diagnostic) ||
        !ReadStringProperty(command, m_State->Owner, "string get_Alias() const", false, entry.Alias, diagnostic) ||
        !ReadStringProperty(command, m_State->Owner, "string get_Description() const", false, entry.Description, diagnostic) ||
        !ReadStringProperty(command, m_State->Owner, "string get_Usage() const", false, entry.Usage, diagnostic) ||
        !ReadStringProperty(command, m_State->Owner, "string get_Category() const", false, entry.Category, diagnostic) ||
        !ReadBoolProperty(command, m_State->Owner, "bool get_Cheat() const", false, entry.Cheat, diagnostic) ||
        !ReadBoolProperty(command, m_State->Owner, "bool get_Hidden() const", false, entry.Hidden, diagnostic) ||
        !ReadBoolProperty(command, m_State->Owner, "bool get_Enabled() const", true, entry.Enabled, diagnostic)) {
        m_State->Owner->RecordScriptDiagnostic(diagnostic);
        return nullptr;
    }

    const std::string key = NormalizeCommandName(entry.Name);
    if (key.empty()) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "Command name cannot be empty."));
        return nullptr;
    }
    if (m_State->Commands.find(key) != m_State->Commands.end()) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "Command name is already registered by this script mod: " + entry.Name));
        return nullptr;
    }
    if (m_State->Context->FindCommand(entry.Name.c_str())) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "Command name is already registered: " + entry.Name));
        return nullptr;
    }
    if (!entry.Alias.empty() && m_State->Context->FindCommand(entry.Alias.c_str())) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "Command alias is already registered: " + entry.Alias));
        return nullptr;
    }

    entry.ExecuteMethod = command->GetObjectType()->GetMethodByDecl(kCommandExecuteDecl);
    entry.CompleteMethod = command->GetObjectType()->GetMethodByDecl(kCommandCompleteDecl);
    if (!entry.ExecuteMethod) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "Command object is missing Execute implementation."));
        return nullptr;
    }

    entry.Generation = m_State->NextGeneration++;
    ScriptObjectHandle retained = ScriptObjectHandle::Retain(command);
    entry.Object = retained.Detach();
    entry.Command.reset(new ScriptCommand(m_State, key, entry.Name, entry.Alias, entry.Description, entry.Cheat));

    ICommand *nativeCommand = entry.Command.get();
    if (!m_State->Context->GetCommandContext().RegisterCommand(nativeCommand)) {
        ReleaseScriptCommandObject(entry);
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "Command registration failed: " + entry.Name));
        return nullptr;
    }

    const unsigned int generation = entry.Generation;
    m_State->Commands.emplace(key, std::move(entry));
    return new ScriptCommandRef(m_State, key, generation);
}

ScriptCommandRef *ScriptCommandService::Register(const ScriptCommandDefinition &definition,
                                                 asIScriptFunction *execute,
                                                 asIScriptFunction *complete) {
    if (!execute || !m_State->Active || !m_State->Context || !m_State->Owner)
        return nullptr;

    if (!IsCommandExecuteSignature(execute)) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "RegisterCommand requires BML::CommandCallback."));
        return nullptr;
    }
    if (complete && !IsCommandCompleteSignature(complete)) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "RegisterCommand completion requires BML::CommandCompletionCallback."));
        return nullptr;
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

    const std::string key = NormalizeCommandName(entry.Name);
    if (key.empty()) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "Command name cannot be empty."));
        return nullptr;
    }
    if (m_State->Commands.find(key) != m_State->Commands.end()) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "Command name is already registered by this script mod: " + entry.Name));
        return nullptr;
    }
    if (m_State->Context->FindCommand(entry.Name.c_str())) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "Command name is already registered: " + entry.Name));
        return nullptr;
    }
    if (!entry.Alias.empty() && m_State->Context->FindCommand(entry.Alias.c_str())) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "Command alias is already registered: " + entry.Alias));
        return nullptr;
    }

    execute->AddRef();
    if (complete)
        complete->AddRef();
    entry.ExecuteMethod = execute;
    entry.CompleteMethod = complete;
    entry.OwnsFunctionRefs = true;
    entry.Generation = m_State->NextGeneration++;
    entry.Command.reset(new ScriptCommand(m_State, key, entry.Name, entry.Alias, entry.Description, entry.Cheat));

    ICommand *nativeCommand = entry.Command.get();
    if (!m_State->Context->GetCommandContext().RegisterCommand(nativeCommand)) {
        ReleaseScriptCommandObject(entry);
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "Command registration failed: " + entry.Name));
        return nullptr;
    }

    const unsigned int generation = entry.Generation;
    m_State->Commands.emplace(key, std::move(entry));
    return new ScriptCommandRef(m_State, key, generation);
}

bool ScriptCommandService::Unregister(const std::string &name) {
    if (!m_State->Context)
        return false;
    const std::string key = NormalizeCommandName(name);
    auto it = m_State->Commands.find(key);
    if (it == m_State->Commands.end())
        return false;

    m_State->Context->GetCommandContext().UnregisterCommand(it->second.Name.c_str());
    if (it->second.ActiveCalls > 0) {
        it->second.PendingUnregister = true;
        return true;
    }
    ReleaseScriptCommandObject(it->second);
    m_State->Commands.erase(it);
    return true;
}

void ScriptCommandService::Release(ScriptDiagnostic *) {
    if (!m_State || !m_State->Active)
        return;

    std::shared_ptr<ScriptCommandServiceState> releasedState = m_State;
    releasedState->Active = false;
    for (auto &entry : releasedState->Commands) {
        if (releasedState->Context)
            releasedState->Context->GetCommandContext().UnregisterCommand(entry.second.Name.c_str());
        ReleaseScriptCommandObject(entry.second);
    }
    releasedState->Commands.clear();
    releasedState->RetiredCommands.clear();
    m_State = std::make_shared<ScriptCommandServiceState>();
}

size_t ScriptCommandService::GetActiveCount() const {
    return m_State && m_State->Active ? m_State->Commands.size() : 0;
}

#ifdef BML_TEST
ScriptCommandRef *ScriptCommandService::AddTestCommandForRelease(const std::string &name,
                                                                 const std::string &alias) {
    if (!m_State || name.empty())
        return nullptr;
    m_State->Active = true;

    ScriptCommandEntry entry;
    entry.Name = name;
    entry.Alias = alias;
    entry.Enabled = true;
    entry.Generation = m_State->NextGeneration++;

    const std::string key = NormalizeCommandName(entry.Name);
    const unsigned int generation = entry.Generation;
    m_State->Commands.emplace(key, std::move(entry));
    return new ScriptCommandRef(m_State, key, generation);
}
#endif

} // namespace BML

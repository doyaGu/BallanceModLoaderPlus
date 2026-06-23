#include "ScriptDataShareService.h"

#include <cstring>
#include <utility>
#include <unordered_map>

#include <angelscript.h>

#include "ModContext.h"
#include "ScriptAngelScriptHandle.h"
#include "ScriptFunctionSupport.h"
#include "ScriptMod.h"
#include "ScriptModContextView.h"
#include "ScriptModRuntime.h"

namespace BML {

static constexpr const char *kDataShareRequestInterfaceDecl = "BML::DataShareRequest";
static constexpr const char *kDataShareReceiveDecl =
    "void Receive(const BML::ModContext &in, const BML::DataShareEvent &in)";

struct ScriptDataShareRequestEntry {
    unsigned int Generation = 0;
    asIScriptObject *Object = nullptr;
    asIScriptFunction *ReceiveMethod = nullptr;
    bool OwnsFunctionRef = false;
    ScriptDataShareRequestType Type = ScriptDataShareRequestType::String;
    std::string Key;
    std::string Name;
    int ActiveCalls = 0;
    bool PendingCancel = false;
};

class ScriptDataShareServiceState {
public:
    ModContext *Context = nullptr;
    ScriptMod *Owner = nullptr;
    ScriptModRuntime *Runtime = nullptr;
    ScriptModContextView *ContextView = nullptr;
    bool Active = false;
    int NextRequestId = 1;
    unsigned int NextGeneration = 1;
    std::unordered_map<int, ScriptDataShareRequestEntry> Requests;
};

struct ScriptDataShareRequestCookie {
    std::shared_ptr<ScriptDataShareServiceState> State;
    int Id = 0;
};

static bool IsValidDataShareKey(const std::string &key) {
    return !key.empty() && key.size() <= 255;
}

static std::string StringFromData(const void *data, size_t size) {
    if (!data || size == 0)
        return {};
    const char *text = static_cast<const char *>(data);
    return std::string(text, strnlen(text, size));
}

template <typename T>
static bool ValueFromData(const void *data, size_t size, T &value) {
    if (!data || size != sizeof(T))
        return false;
    std::memcpy(&value, data, sizeof(T));
    return true;
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
        diagnostic.Message += "script DataShare request callback suspended";
    } else {
        diagnostic.Message += "AngelScript call failed with code ";
        diagnostic.Message += std::to_string(code);
    }
    return diagnostic;
}

static bool ReadStringProperty(asIScriptObject *object,
                               ScriptMod *owner,
                               const char *declaration,
                               bool required,
                               std::string &value,
                               ScriptDiagnostic &diagnostic) {
    value.clear();
    if (!object || !object->GetObjectType())
        return !required;

    asIScriptFunction *method = object->GetObjectType()->GetMethodByDecl(declaration);
    if (!method) {
        if (required) {
            diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
                std::string("DataShare request object is missing property: ") + declaration);
        }
        return !required;
    }

    ScriptHostCallScope activeCall(owner);
    if (owner && !activeCall.Entered()) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Script mod reload is in progress.");
        diagnostic.Status = CKAS_INUSE;
        return false;
    }

    asIScriptContext *context = object->GetEngine()->CreateContext();
    if (!context) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Unable to create AngelScript context for DataShare request property.");
        return false;
    }

    ScriptCurrentModScope callScope(owner);
    int code = context->Prepare(method);
    if (code >= 0)
        code = context->SetObject(object);
    if (code < 0) {
        diagnostic = MakeAngelScriptDiagnostic(ScriptDiagnosticPhase::Runtime, code, context, "DataShare request property read failed");
        context->Release();
        return false;
    }

    code = context->Execute();
    if (code == asEXECUTION_SUSPENDED)
        context->Abort();
    if (code != asEXECUTION_FINISHED) {
        diagnostic = MakeAngelScriptDiagnostic(ScriptDiagnosticPhase::Runtime, code, context, "DataShare request property read failed");
        context->Release();
        return false;
    }

    auto *result = static_cast<std::string *>(context->GetReturnObject());
    if (result)
        value = *result;
    context->Release();
    return true;
}

static bool ReadIntProperty(asIScriptObject *object,
                            ScriptMod *owner,
                            const char *declaration,
                            int &value,
                            ScriptDiagnostic &diagnostic) {
    value = 0;
    if (!object || !object->GetObjectType()) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Invalid DataShare request object.");
        return false;
    }

    asIScriptFunction *method = object->GetObjectType()->GetMethodByDecl(declaration);
    if (!method) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            std::string("DataShare request object is missing property: ") + declaration);
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
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Unable to create AngelScript context for DataShare request property.");
        return false;
    }

    ScriptCurrentModScope callScope(owner);
    int code = context->Prepare(method);
    if (code >= 0)
        code = context->SetObject(object);
    if (code < 0) {
        diagnostic = MakeAngelScriptDiagnostic(ScriptDiagnosticPhase::Runtime, code, context, "DataShare request property read failed");
        context->Release();
        return false;
    }

    code = context->Execute();
    if (code == asEXECUTION_SUSPENDED)
        context->Abort();
    if (code != asEXECUTION_FINISHED) {
        diagnostic = MakeAngelScriptDiagnostic(ScriptDiagnosticPhase::Runtime, code, context, "DataShare request property read failed");
        context->Release();
        return false;
    }

    value = static_cast<int>(context->GetReturnDWord());
    context->Release();
    return true;
}

static bool NormalizeType(int value, ScriptDataShareRequestType &type) {
    switch (value) {
    case 0:
        type = ScriptDataShareRequestType::String;
        return true;
    case 1:
        type = ScriptDataShareRequestType::Bool;
        return true;
    case 2:
        type = ScriptDataShareRequestType::Int;
        return true;
    case 3:
        type = ScriptDataShareRequestType::Float;
        return true;
    default:
        return false;
    }
}

static bool IsDataShareCallbackSignature(asIScriptFunction *callback) {
    const ScriptFunctionParam params[] = {
        {"BML::ModContext", asTM_INREF | asTM_CONST},
        {"BML::DataShareEvent", asTM_INREF | asTM_CONST},
    };
    return ScriptFunctionHasSignature(callback, asTYPEID_VOID, params, 2);
}

static void ReleaseScriptDataShareRequestObject(ScriptDataShareRequestEntry &entry) {
    if (entry.Object) {
        entry.Object->Release();
        entry.Object = nullptr;
    }
    if (entry.OwnsFunctionRef && entry.ReceiveMethod) {
        entry.ReceiveMethod->Release();
        entry.OwnsFunctionRef = false;
    }
    entry.ReceiveMethod = nullptr;
}

struct DataShareFunctionCallArgs {
    ScriptModContextView *ContextView = nullptr;
    ScriptDataShareEventView *Event = nullptr;
};

static int WriteDataShareFunctionArgs(asIScriptContext *context, void *userdata) {
    auto *args = static_cast<DataShareFunctionCallArgs *>(userdata);
    int code = context->SetArgObject(0, args ? args->ContextView : nullptr);
    if (code >= 0)
        code = context->SetArgObject(1, args ? args->Event : nullptr);
    return code;
}

static void RetireRequestEntry(const std::shared_ptr<ScriptDataShareServiceState> &state, int id) {
    if (!state)
        return;
    auto it = state->Requests.find(id);
    if (it == state->Requests.end())
        return;
    if (it->second.ActiveCalls > 0) {
        it->second.PendingCancel = true;
        return;
    }
    ReleaseScriptDataShareRequestObject(it->second);
    state->Requests.erase(it);
}

static bool ExecuteDataShareReceive(asIScriptObject *object,
                                    asIScriptFunction *method,
                                    ScriptMod *owner,
                                    ScriptModContextView *contextView,
                                    ScriptDataShareEventView *event,
                                    ScriptDiagnostic &diagnostic) {
    if (!method || !contextView || !event) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Callback, "DataShare request callback has invalid runtime state.");
        return false;
    }

    DataShareFunctionCallArgs args = {contextView, event};
    ScriptFunctionCall call;
    call.Function = method;
    call.Object = object;
    call.Owner = owner;
    call.Phase = ScriptDiagnosticPhase::Callback;
    call.FailurePrefix = "DataShare request callback failed";
    call.InvalidStateMessage = "DataShare request callback has invalid runtime state.";
    call.ContextFailureMessage = "Unable to create AngelScript context for DataShare request callback.";
    call.SuspendedMessage = "script DataShare request callback suspended";
    call.WriteArgs = WriteDataShareFunctionArgs;
    call.UserData = &args;
    return ExecuteScriptFunction(call, diagnostic);
}

static bool CallDataShareRequest(const std::shared_ptr<ScriptDataShareServiceState> &state,
                                 int id,
                                 const void *data,
                                 size_t size) {
    if (!state || !state->Active || !state->ContextView)
        return true;

    auto it = state->Requests.find(id);
    if (it == state->Requests.end())
        return true;

    ScriptDataShareRequestEntry &entry = it->second;
    if (!entry.ReceiveMethod)
        return true;

    ++entry.ActiveCalls;
    ScriptDataShareEventView event(&entry.Key, entry.Type, data != nullptr && size > 0, data, size);
    ScriptDiagnostic diagnostic;
    const bool ok = ExecuteDataShareReceive(entry.Object,
                                            entry.ReceiveMethod,
                                            state->Owner,
                                            state->ContextView,
                                            &event,
                                            diagnostic);
    auto afterCall = state->Requests.find(id);
    if (afterCall != state->Requests.end() && afterCall->second.ActiveCalls > 0)
        --afterCall->second.ActiveCalls;
    if (!ok && state->Owner)
        state->Owner->SetLoadFailure(diagnostic);
    afterCall = state->Requests.find(id);
    if (afterCall != state->Requests.end() && afterCall->second.PendingCancel)
        RetireRequestEntry(state, id);
    return ok;
}

static void __cdecl OnDataShareRequest(const char *, const void *data, size_t size, void *userdata) {
    auto *cookie = static_cast<ScriptDataShareRequestCookie *>(userdata);
    if (!cookie)
        return;
    CallDataShareRequest(cookie->State, cookie->Id, data, size);
}

static void __cdecl CleanupDataShareRequest(const char *, void *userdata) {
    auto *cookie = static_cast<ScriptDataShareRequestCookie *>(userdata);
    if (!cookie)
        return;
    RetireRequestEntry(cookie->State, cookie->Id);
    delete cookie;
}

ScriptDataShareEventView::ScriptDataShareEventView(const std::string *key,
                                                   ScriptDataShareRequestType type,
                                                   bool exists,
                                                   const void *data,
                                                   size_t size)
    : m_Key(key ? *key : std::string()), m_Type(type), m_Exists(exists) {
    if (!m_Exists)
        return;

    switch (m_Type) {
    case ScriptDataShareRequestType::String:
        m_StringValue = StringFromData(data, size);
        break;
    case ScriptDataShareRequestType::Bool:
        ValueFromData(data, size, m_BoolValue);
        break;
    case ScriptDataShareRequestType::Int:
        ValueFromData(data, size, m_IntValue);
        break;
    case ScriptDataShareRequestType::Float:
        ValueFromData(data, size, m_FloatValue);
        break;
    }
}

std::string ScriptDataShareEventView::GetKey() const {
    return m_Key;
}

int ScriptDataShareEventView::GetType() const {
    return static_cast<int>(m_Type);
}

std::string ScriptDataShareEventView::GetString() const {
    return m_Exists && m_Type == ScriptDataShareRequestType::String ? m_StringValue : std::string();
}

bool ScriptDataShareEventView::GetBool() const {
    return m_Exists && m_Type == ScriptDataShareRequestType::Bool ? m_BoolValue : false;
}

int ScriptDataShareEventView::GetInt() const {
    return m_Exists && m_Type == ScriptDataShareRequestType::Int ? m_IntValue : 0;
}

float ScriptDataShareEventView::GetFloat() const {
    return m_Exists && m_Type == ScriptDataShareRequestType::Float ? m_FloatValue : 0.0f;
}

ScriptDataShareRequestRef::ScriptDataShareRequestRef(std::weak_ptr<ScriptDataShareServiceState> state,
                                                     int id,
                                                     unsigned int generation)
    : m_State(std::move(state)), m_Id(id), m_Generation(generation) {}

void ScriptDataShareRequestRef::AddRef() { ++m_RefCount; }

void ScriptDataShareRequestRef::Release() {
    if (--m_RefCount <= 0)
        delete this;
}

bool ScriptDataShareRequestRef::IsValid() const {
    std::shared_ptr<ScriptDataShareServiceState> state = m_State.lock();
    if (!state || !state->Active)
        return false;
    auto it = state->Requests.find(m_Id);
    return it != state->Requests.end() && it->second.Generation == m_Generation;
}

std::string ScriptDataShareRequestRef::GetKey() const {
    std::shared_ptr<ScriptDataShareServiceState> state = m_State.lock();
    if (!state || !state->Active)
        return {};
    auto it = state->Requests.find(m_Id);
    return it != state->Requests.end() && it->second.Generation == m_Generation ? it->second.Key : std::string();
}

int ScriptDataShareRequestRef::GetType() const {
    std::shared_ptr<ScriptDataShareServiceState> state = m_State.lock();
    if (!state || !state->Active)
        return 0;
    auto it = state->Requests.find(m_Id);
    return it != state->Requests.end() && it->second.Generation == m_Generation ? static_cast<int>(it->second.Type) : 0;
}

bool ScriptDataShareRequestRef::Cancel() {
    std::shared_ptr<ScriptDataShareServiceState> state = m_State.lock();
    if (!state || !state->Active)
        return false;
    auto it = state->Requests.find(m_Id);
    if (it == state->Requests.end() || it->second.Generation != m_Generation)
        return false;
    RetireRequestEntry(state, m_Id);
    return true;
}

ScriptDataShareService::ScriptDataShareService() : m_State(std::make_shared<ScriptDataShareServiceState>()) {}
ScriptDataShareService::~ScriptDataShareService() { Release(nullptr); }

void ScriptDataShareService::Bind(ModContext *context,
                                  ScriptMod *owner,
                                  ScriptModRuntime *runtime,
                                  ScriptModContextView *contextView) {
    m_State->Context = context;
    m_State->Owner = owner;
    m_State->Runtime = runtime;
    m_State->ContextView = contextView;
    m_State->Active = true;
}

ScriptDataShareRequestRef *ScriptDataShareService::Request(asIScriptObject *request) {
    if (!request || !m_State->Active || !m_State->Context || !m_State->Owner || !request->GetObjectType())
        return nullptr;

    asITypeInfo *requestInterface = request->GetEngine()->GetTypeInfoByDecl(kDataShareRequestInterfaceDecl);
    if (!requestInterface || !request->GetObjectType()->Implements(requestInterface)) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "RequestDataShare requires an object implementing BML::DataShareRequest."));
        return nullptr;
    }

    ScriptDiagnostic diagnostic;
    ScriptDataShareRequestEntry entry;
    int type = 0;
    if (!ReadStringProperty(request, m_State->Owner, "string get_Key() const", true, entry.Key, diagnostic) ||
        !ReadStringProperty(request, m_State->Owner, "string get_Name() const", false, entry.Name, diagnostic) ||
        !ReadIntProperty(request, m_State->Owner, "int get_Type() const", type, diagnostic)) {
        m_State->Owner->RecordScriptDiagnostic(diagnostic);
        return nullptr;
    }
    if (!NormalizeType(type, entry.Type)) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "DataShare request type is invalid."));
        return nullptr;
    }
    if (!IsValidDataShareKey(entry.Key)) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "DataShare request key is invalid."));
        return nullptr;
    }

    entry.ReceiveMethod = request->GetObjectType()->GetMethodByDecl(kDataShareReceiveDecl);
    if (!entry.ReceiveMethod) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "DataShare request object is missing Receive implementation."));
        return nullptr;
    }

    BML_DataShare *share = BML_GetDataShare(entry.Name.empty() ? nullptr : entry.Name.c_str());
    if (!share)
        return nullptr;

    const int requestId = m_State->NextRequestId++;
    entry.Generation = m_State->NextGeneration++;
    ScriptObjectHandle retained = ScriptObjectHandle::Retain(request);
    entry.Object = retained.Detach();
    const unsigned int generation = entry.Generation;
    m_State->Requests.emplace(requestId, std::move(entry));

    auto *cookie = new ScriptDataShareRequestCookie;
    cookie->State = m_State;
    cookie->Id = requestId;

    auto registered = m_State->Requests.find(requestId);
    BML_DataShare_Request(share, registered != m_State->Requests.end() ? registered->second.Key.c_str() : "",
                          OnDataShareRequest, cookie, CleanupDataShareRequest);
    BML_DataShare_Release(share);
    return new ScriptDataShareRequestRef(m_State, requestId, generation);
}

ScriptDataShareRequestRef *ScriptDataShareService::Request(const std::string &key,
                                                           int typeValue,
                                                           asIScriptFunction *callback,
                                                           const std::string &name) {
    if (!callback || !m_State->Active || !m_State->Context || !m_State->Owner)
        return nullptr;

    if (!IsDataShareCallbackSignature(callback)) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "RequestDataShare requires BML::DataShareCallback."));
        return nullptr;
    }

    ScriptDataShareRequestEntry entry;
    entry.Key = key;
    entry.Name = name;
    if (!NormalizeType(typeValue, entry.Type)) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "DataShare request type is invalid."));
        return nullptr;
    }
    if (!IsValidDataShareKey(entry.Key)) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "DataShare request key is invalid."));
        return nullptr;
    }

    BML_DataShare *share = BML_GetDataShare(entry.Name.empty() ? nullptr : entry.Name.c_str());
    if (!share)
        return nullptr;

    callback->AddRef();
    entry.ReceiveMethod = callback;
    entry.OwnsFunctionRef = true;

    const int requestId = m_State->NextRequestId++;
    entry.Generation = m_State->NextGeneration++;
    const unsigned int generation = entry.Generation;
    m_State->Requests.emplace(requestId, std::move(entry));

    auto *cookie = new ScriptDataShareRequestCookie;
    cookie->State = m_State;
    cookie->Id = requestId;

    auto registered = m_State->Requests.find(requestId);
    BML_DataShare_Request(share, registered != m_State->Requests.end() ? registered->second.Key.c_str() : "",
                          OnDataShareRequest, cookie, CleanupDataShareRequest);
    BML_DataShare_Release(share);
    return new ScriptDataShareRequestRef(m_State, requestId, generation);
}

void ScriptDataShareService::Release(ScriptDiagnostic *) {
    if (!m_State || !m_State->Active)
        return;

    std::shared_ptr<ScriptDataShareServiceState> releasedState = m_State;
    releasedState->Active = false;
    for (auto &entry : releasedState->Requests)
        ReleaseScriptDataShareRequestObject(entry.second);
    releasedState->Requests.clear();
    m_State = std::make_shared<ScriptDataShareServiceState>();
}

size_t ScriptDataShareService::GetActiveCount() const {
    return m_State && m_State->Active ? m_State->Requests.size() : 0;
}

} // namespace BML

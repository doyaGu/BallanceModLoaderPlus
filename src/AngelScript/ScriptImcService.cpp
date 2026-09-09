#include "ScriptImcService.h"

#include <deque>
#include <mutex>
#include <new>
#include <unordered_map>
#include <utility>
#include <vector>

#include <angelscript.h>

#include "BML/Imc.h"
#include "Imc/ImcRuntime.h"
#include "Loader/ModContext.h"
#include "ScriptFunctionSupport.h"
#include "ScriptImcRecord.h"
#include "ScriptMod.h"

namespace BML {

struct ScriptImcRequestRef::Control {
    int Id = 0;
    unsigned int Generation = 0;
    std::atomic<bool> Active{true};
    std::atomic<int> Status{BML_ERROR_BUSY};
};

struct ScriptImcSubscriptionRef::Control {
    int Id = 0;
    unsigned int Generation = 0;
    std::atomic<bool> Active{true};
    std::atomic<int> Status{BML_OK};
};

struct ScriptImcProviderRef::Control {
    int Id = 0;
    unsigned int Generation = 0;
    std::atomic<bool> Active{true};
    std::atomic<int> Status{BML_OK};
};

namespace {

struct PendingTopic {
    int SubscriptionId = 0;
    BML_ImcPayloadTypeId PayloadType = BML_IMC_INVALID_ID;
    int Status = BML_OK;
    std::vector<std::uint8_t> Bytes;
};

struct RequestEntry {
    unsigned int Generation = 0;
    BML_ImcFuture Future = nullptr;
    BML_ImcPayloadTypeId ExpectedPayload = BML_IMC_INVALID_ID;
    int ImmediateStatus = BML_ERROR_BUSY;
    asIScriptFunction *Callback = nullptr;
    std::shared_ptr<ScriptImcRequestRef::Control> Control;
};

struct TopicCookie {
    std::weak_ptr<ScriptImcServiceState> State;
    int SubscriptionId = 0;
};

struct SubscriptionEntry {
    unsigned int Generation = 0;
    BML_ImcSubscription Subscription = nullptr;
    BML_ImcPayloadTypeId ExpectedPayload = BML_IMC_INVALID_ID;
    asIScriptFunction *Callback = nullptr;
    std::shared_ptr<ScriptImcSubscriptionRef::Control> Control;
    std::unique_ptr<TopicCookie> Cookie;
};

struct RpcCookie {
    ScriptMod *Owner = nullptr;
    asIScriptFunction *Handler = nullptr;
    BML_ImcPayloadTypeId RequestPayload = BML_IMC_INVALID_ID;
    BML_ImcPayloadTypeId ResponsePayload = BML_IMC_INVALID_ID;

    ~RpcCookie() {
        if (Handler)
            Handler->Release();
    }
};

struct RpcRegistrationEntry {
    BML_ImcRpcId RpcId = BML_IMC_INVALID_ID;
    std::unique_ptr<RpcCookie> Cookie;
};

struct ProviderEntry {
    unsigned int Generation = 0;
    std::shared_ptr<ScriptImcProviderRef::Control> Control;
    std::unordered_map<BML_ImcRpcId, RpcRegistrationEntry> Rpcs;
};

struct CallbackInvocation {
    ScriptMod *Owner = nullptr;
    asIScriptFunction *Callback = nullptr;
    int Status = BML_OK;
    ScriptImcRecord *Record = nullptr;
};

struct CallbackArgs {
    int Status = BML_OK;
    ScriptImcRecord *Record = nullptr;
};

bool HasBridgeCallbackSignature(asIScriptFunction *callback) {
    const ScriptFunctionParam params[] = {
        {"int", 0},
        {"BML::Detail::ImcRecord", asTM_INREF | asTM_CONST},
    };
    return ScriptFunctionHasSignature(callback, asTYPEID_VOID, params, 2);
}

bool HasRpcHandlerSignature(asIScriptFunction *handler) {
    const ScriptFunctionParam params[] = {
        {"BML::Detail::ImcRecord", asTM_INREF | asTM_CONST},
        {"BML::Detail::ImcReply", asTM_INOUTREF},
    };
    return ScriptFunctionHasSignature(handler, asTYPEID_VOID, params, 2);
}

int WriteCallbackArgs(asIScriptContext *context, void *userdata) {
    const auto *args = static_cast<const CallbackArgs *>(userdata);
    int code = context->SetArgDWord(0, static_cast<asDWORD>(args ? args->Status : BML_ERROR_FAIL));
    if (code >= 0)
        code = context->SetArgObject(1, args ? args->Record : nullptr);
    return code;
}

bool ExecuteCallback(const CallbackInvocation &invocation, ScriptDiagnostic &diagnostic) {
    if (!invocation.Callback || !invocation.Record) {
        diagnostic = MakeScriptDiagnostic(
            ScriptDiagnosticPhase::Callback,
            "IMC callback has invalid runtime state.");
        return false;
    }
    CallbackArgs args{invocation.Status, invocation.Record};
    ScriptFunctionCall call;
    call.Function = invocation.Callback;
    call.Owner = invocation.Owner;
    call.Phase = ScriptDiagnosticPhase::Callback;
    call.FailurePrefix = "IMC callback failed";
    call.InvalidStateMessage = "IMC callback has invalid runtime state.";
    call.ContextFailureMessage = "Unable to create AngelScript context for IMC callback.";
    call.SuspendedMessage = "script IMC callback suspended";
    call.WriteArgs = WriteCallbackArgs;
    call.UserData = &args;
    return ExecuteScriptFunction(call, diagnostic);
}

struct RpcCallbackArgs {
    ScriptImcRecord *Request = nullptr;
    ScriptImcReply *Reply = nullptr;
};

int WriteRpcCallbackArgs(asIScriptContext *context, void *userdata) {
    const auto *args = static_cast<const RpcCallbackArgs *>(userdata);
    int code = context->SetArgObject(0, args ? args->Request : nullptr);
    if (code >= 0)
        code = context->SetArgObject(1, args ? args->Reply : nullptr);
    return code;
}

bool ExecuteRpcCallback(RpcCookie &cookie, ScriptImcRecord &request,
                        ScriptImcReply &reply, ScriptDiagnostic &diagnostic) {
    RpcCallbackArgs args{&request, &reply};
    ScriptFunctionCall call;
    call.Function = cookie.Handler;
    call.Owner = cookie.Owner;
    call.Phase = ScriptDiagnosticPhase::Callback;
    call.FailurePrefix = "IMC provider callback failed";
    call.InvalidStateMessage = "IMC provider callback has invalid runtime state.";
    call.ContextFailureMessage = "Unable to create AngelScript context for IMC provider callback.";
    call.SuspendedMessage = "script IMC provider callback suspended";
    call.WriteArgs = WriteRpcCallbackArgs;
    call.UserData = &args;
    return ExecuteScriptFunction(call, diagnostic);
}

int OnRpc(BML_ImcRpcId, const BML_ImcMessage *message,
          BML_ImcResponse *response, void *userdata) {
    auto *cookie = static_cast<RpcCookie *>(userdata);
    if (!cookie || !cookie->Owner || !cookie->Handler)
        return BML_ERROR_INVALID_PARAMETER;

    ScriptImcRecord *request = nullptr;
    if (cookie->RequestPayload == BML_IMC_INVALID_ID) {
        if (message && (message->Size < sizeof(BML_ImcMessage) || message->DataSize != 0))
            return BML_ERROR_MALFORMED_MESSAGE;
        request = CreateScriptImcRecord();
    } else {
        if (!message || message->Size < sizeof(BML_ImcMessage))
            return BML_ERROR_MALFORMED_MESSAGE;
        if (message->PayloadType != cookie->RequestPayload)
            return BML_ERROR_TYPE_MISMATCH;
        request = ScriptImcRecord::Decode(*message);
    }
    if (!request)
        return BML_ERROR_OUT_OF_MEMORY;
    if (request->GetStatus() != BML_OK) {
        const int status = request->GetStatus();
        request->Release();
        return status;
    }

    ScriptImcReply reply(cookie->ResponsePayload != BML_IMC_INVALID_ID);
    ScriptDiagnostic diagnostic;
    const bool executed = ExecuteRpcCallback(*cookie, *request, reply, diagnostic);
    request->Release();
    if (!executed) {
        cookie->Owner->RecordScriptDiagnostic(diagnostic);
        return BML_ERROR_IMC_TARGET_EXECUTION_FAILED;
    }
    return reply.Commit(response, cookie->ResponsePayload);
}

int FutureCompletionStatus(ImcRuntime &runtime, BML_ImcFuture future,
                           BML_ImcMessage &message) {
    BML_ImcFutureState state = BML_IMC_FUTURE_PENDING;
    int status = runtime.FutureGetState(future, &state);
    if (status != BML_OK)
        return status;
    switch (state) {
    case BML_IMC_FUTURE_PENDING:
        return BML_ERROR_BUSY;
    case BML_IMC_FUTURE_READY:
        return runtime.FutureGetResult(future, &message);
    case BML_IMC_FUTURE_FAILED: {
        int error = BML_ERROR_FAIL;
        status = runtime.FutureGetError(future, &error);
        return status == BML_OK ? error : status;
    }
    case BML_IMC_FUTURE_CANCELLED:
        return BML_ERROR_CANCELLED;
    case BML_IMC_FUTURE_TIMED_OUT:
        return BML_ERROR_TIMEOUT;
    default:
        return BML_ERROR_FAIL;
    }
}

} // namespace

class ScriptImcServiceState final {
public:
    ModContext *Context = nullptr;
    ScriptMod *Owner = nullptr;
    BML_ImcClient Client = nullptr;
    bool Active = false;
    int NextId = 1;
    unsigned int NextGeneration = 1;
    mutable std::mutex Mutex;
    std::unordered_map<int, RequestEntry> Requests;
    std::unordered_map<int, SubscriptionEntry> Subscriptions;
    std::unordered_map<int, ProviderEntry> Providers;
    std::deque<PendingTopic> PendingTopics;

    int EnsureClient() {
        if (Client)
            return BML_OK;
        if (!Active || !Context || !Owner)
            return BML_ERROR_FROZEN;
        return Context->GetImcRuntime().OpenClient(Owner->GetID(), &Client);
    }

    int CancelRequest(int id, unsigned int generation) {
        std::lock_guard<std::mutex> guard(Mutex);
        auto found = Requests.find(id);
        if (found == Requests.end() || found->second.Generation != generation)
            return BML_ERROR_INVALID_HANDLE;
        if (found->second.Future && Context) {
            const int status = Context->GetImcRuntime().FutureCancel(
                found->second.Future);
            if (status != BML_OK)
                return status;
        }
        RequestEntry entry = std::move(found->second);
        Requests.erase(found);
        if (entry.Future && Context) {
            Context->GetImcRuntime().FutureRelease(entry.Future);
        }
        if (entry.Callback)
            entry.Callback->Release();
        entry.Control->Status.store(BML_ERROR_CANCELLED, std::memory_order_release);
        entry.Control->Active.store(false, std::memory_order_release);
        return BML_OK;
    }

    int CancelSubscription(int id, unsigned int generation) {
        std::lock_guard<std::mutex> guard(Mutex);
        auto found = Subscriptions.find(id);
        if (found == Subscriptions.end() || found->second.Generation != generation)
            return BML_ERROR_INVALID_HANDLE;

        if (found->second.Subscription && Client && Context) {
            const int status = Context->GetImcRuntime().Unsubscribe(
                Client, found->second.Subscription);
            if (status != BML_OK && status != BML_ERROR_INVALID_HANDLE)
                return status;
        }
        SubscriptionEntry entry = std::move(found->second);
        Subscriptions.erase(found);
        if (entry.Callback)
            entry.Callback->Release();
        entry.Control->Status.store(BML_ERROR_CANCELLED, std::memory_order_release);
        entry.Control->Active.store(false, std::memory_order_release);
        return BML_OK;
    }

    int DroppedCount(int id, unsigned int generation, std::uint64_t &count) const {
        count = 0;
        std::lock_guard<std::mutex> guard(Mutex);
        const auto found = Subscriptions.find(id);
        if (found == Subscriptions.end() || found->second.Generation != generation ||
            !found->second.Subscription || !Client || !Context)
            return BML_ERROR_INVALID_HANDLE;
        return Context->GetImcRuntime().GetSubscriptionDroppedCount(
            Client, found->second.Subscription, &count);
    }

    int Publish(const std::string &topic, const std::string &payload,
                const ScriptImcRecord &message, std::uint64_t &delivered) {
        delivered = 0;
        if (topic.empty() || payload.empty())
            return BML_ERROR_INVALID_PARAMETER;
        int status = EnsureClient();
        if (status != BML_OK)
            return status;

        ImcRuntime &imc = Context->GetImcRuntime();
        BML_ImcTopicId topicId = BML_IMC_INVALID_ID;
        BML_ImcPayloadTypeId payloadType = BML_IMC_INVALID_ID;
        std::vector<std::uint8_t> bytes;
        status = message.Encode(bytes);
        if (status == BML_OK)
            status = imc.GetTopicId(Client, topic.c_str(), &topicId);
        if (status == BML_OK)
            status = imc.GetPayloadTypeId(Client, payload.c_str(), &payloadType);
        if (status != BML_OK)
            return status;

        BML_ImcMessage encoded = BML_IMC_MESSAGE_INIT;
        encoded.Data = bytes.empty() ? nullptr : bytes.data();
        encoded.DataSize = bytes.size();
        encoded.PayloadType = payloadType;
        std::size_t count = 0;
        status = imc.Publish(Client, topicId, &encoded, &count);
        if (status == BML_OK)
            delivered = static_cast<std::uint64_t>(count);
        return status;
    }

    int GetSubscriberCount(const std::string &topic, std::uint64_t &count) {
        count = 0;
        if (topic.empty())
            return BML_ERROR_INVALID_PARAMETER;
        int status = EnsureClient();
        if (status != BML_OK)
            return status;
        ImcRuntime &imc = Context->GetImcRuntime();
        BML_ImcTopicId topicId = BML_IMC_INVALID_ID;
        status = imc.GetTopicId(Client, topic.c_str(), &topicId);
        std::size_t subscribers = 0;
        if (status == BML_OK)
            status = imc.GetTopicSubscriberCount(Client, topicId, &subscribers);
        if (status == BML_OK)
            count = static_cast<std::uint64_t>(subscribers);
        return status;
    }

    int RegisterProviderRpc(int providerId, unsigned int generation,
                            const std::string &route,
                            const std::string &requestPayload,
                            const std::string &responsePayload,
                            asIScriptFunction *handler) {
        if (route.empty() || !handler || !HasRpcHandlerSignature(handler))
            return BML_ERROR_INVALID_PARAMETER;
        int status = EnsureClient();
        if (status != BML_OK)
            return status;

        ImcRuntime &imc = Context->GetImcRuntime();
        BML_ImcRpcId rpcId = BML_IMC_INVALID_ID;
        BML_ImcPayloadTypeId requestType = BML_IMC_INVALID_ID;
        BML_ImcPayloadTypeId responseType = BML_IMC_INVALID_ID;
        status = imc.GetRpcId(Client, route.c_str(), &rpcId);
        if (status == BML_OK && !requestPayload.empty())
            status = imc.GetPayloadTypeId(Client, requestPayload.c_str(), &requestType);
        if (status == BML_OK && !responsePayload.empty())
            status = imc.GetPayloadTypeId(Client, responsePayload.c_str(), &responseType);
        if (status != BML_OK)
            return status;

        {
            std::lock_guard<std::mutex> guard(Mutex);
            const auto provider = Providers.find(providerId);
            if (!Active || provider == Providers.end() ||
                provider->second.Generation != generation)
                return BML_ERROR_INVALID_HANDLE;
            if (provider->second.Rpcs.find(rpcId) != provider->second.Rpcs.end())
                return BML_ERROR_ALREADY_EXISTS;
        }

        std::unique_ptr<RpcCookie> cookie;
        try {
            cookie = std::make_unique<RpcCookie>();
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        }
        cookie->Owner = Owner;
        cookie->Handler = handler;
        cookie->Handler->AddRef();
        cookie->RequestPayload = requestType;
        cookie->ResponsePayload = responseType;

        BML_ImcRpcRegistrationOptions options = BML_IMC_RPC_REGISTRATION_OPTIONS_INIT;
        options.Execution = BML_IMC_EXECUTION_GAME_THREAD;
        status = imc.RegisterRpc(Client, rpcId, &options, OnRpc, cookie.get());
        if (status != BML_OK)
            return status;

        try {
            std::lock_guard<std::mutex> guard(Mutex);
            const auto provider = Providers.find(providerId);
            if (!Active || provider == Providers.end() ||
                provider->second.Generation != generation) {
                imc.UnregisterRpc(Client, rpcId);
                return BML_ERROR_INVALID_HANDLE;
            }
            RpcRegistrationEntry registration;
            registration.RpcId = rpcId;
            registration.Cookie = std::move(cookie);
            provider->second.Rpcs.emplace(rpcId, std::move(registration));
        } catch (const std::bad_alloc &) {
            imc.UnregisterRpc(Client, rpcId);
            return BML_ERROR_OUT_OF_MEMORY;
        }
        return BML_OK;
    }

    int CloseProvider(int providerId, unsigned int generation) {
        std::vector<BML_ImcRpcId> rpcIds;
        {
            std::lock_guard<std::mutex> guard(Mutex);
            const auto provider = Providers.find(providerId);
            if (provider == Providers.end() || provider->second.Generation != generation)
                return BML_ERROR_INVALID_HANDLE;
            rpcIds.reserve(provider->second.Rpcs.size());
            for (const auto &[rpcId, registration] : provider->second.Rpcs)
                rpcIds.push_back(rpcId);
        }

        int result = BML_OK;
        for (BML_ImcRpcId rpcId : rpcIds) {
            const int status = Context && Client
                                   ? Context->GetImcRuntime().UnregisterRpc(Client, rpcId)
                                   : BML_ERROR_INVALID_HANDLE;
            if (status == BML_OK || status == BML_ERROR_NOT_FOUND) {
                std::lock_guard<std::mutex> guard(Mutex);
                const auto provider = Providers.find(providerId);
                if (provider != Providers.end() && provider->second.Generation == generation)
                    provider->second.Rpcs.erase(rpcId);
            } else if (result == BML_OK) {
                result = status;
            }
        }

        std::lock_guard<std::mutex> guard(Mutex);
        const auto provider = Providers.find(providerId);
        if (provider == Providers.end() || provider->second.Generation != generation)
            return result == BML_OK ? BML_ERROR_INVALID_HANDLE : result;
        if (!provider->second.Rpcs.empty())
            return result == BML_OK ? BML_ERROR_BUSY : result;
        provider->second.Control->Status.store(result, std::memory_order_release);
        provider->second.Control->Active.store(false, std::memory_order_release);
        Providers.erase(provider);
        return result;
    }
};

namespace {

void OnTopic(BML_ImcTopicId, const BML_ImcMessage *message, void *userdata) {
    auto *cookie = static_cast<TopicCookie *>(userdata);
    if (!cookie || !message)
        return;
    std::shared_ptr<ScriptImcServiceState> state = cookie->State.lock();
    if (!state)
        return;
    try {
        PendingTopic pending;
        pending.SubscriptionId = cookie->SubscriptionId;
        pending.PayloadType = message->PayloadType;
        if (message->DataSize) {
            const auto *bytes = static_cast<const std::uint8_t *>(message->Data);
            if (!bytes)
                pending.Status = BML_ERROR_MALFORMED_MESSAGE;
            else
                pending.Bytes.assign(bytes, bytes + message->DataSize);
        }
        std::lock_guard<std::mutex> guard(state->Mutex);
        if (state->Active && state->Subscriptions.find(cookie->SubscriptionId) != state->Subscriptions.end())
            state->PendingTopics.push_back(std::move(pending));
    } catch (const std::bad_alloc &) {
        try {
            PendingTopic pending;
            pending.SubscriptionId = cookie->SubscriptionId;
            pending.Status = BML_ERROR_OUT_OF_MEMORY;
            std::lock_guard<std::mutex> guard(state->Mutex);
            if (state->Active)
                state->PendingTopics.push_back(std::move(pending));
        } catch (...) {
        }
    } catch (...) {
    }
}

} // namespace

ScriptImcReply::ScriptImcReply(bool expectsResponse)
    : m_ExpectsResponse(expectsResponse) {}

void ScriptImcReply::Complete(int status) {
    if (m_Completed)
        return;
    m_Completed = true;
    if (status == BML_OK && m_ExpectsResponse)
        m_Status = BML_ERROR_IMC_SCHEMA_MISMATCH;
    else
        m_Status = status;
}

void ScriptImcReply::Complete(int status, const ScriptImcRecord &record) {
    if (m_Completed)
        return;
    m_Completed = true;
    if (status != BML_OK) {
        m_Status = status;
        return;
    }
    if (!m_ExpectsResponse) {
        m_Status = BML_ERROR_IMC_SCHEMA_MISMATCH;
        return;
    }
    m_Status = record.Encode(m_Bytes);
}

int ScriptImcReply::Commit(BML_ImcResponse *response,
                           BML_ImcPayloadTypeId responsePayload) const {
    if (!m_Completed)
        return BML_ERROR_IMC_TARGET_EXECUTION_FAILED;
    if (m_Status != BML_OK)
        return m_Status;
    if (!m_ExpectsResponse)
        return responsePayload == BML_IMC_INVALID_ID
                   ? BML_OK
                   : BML_ERROR_IMC_SCHEMA_MISMATCH;
    if (responsePayload == BML_IMC_INVALID_ID)
        return BML_ERROR_IMC_SCHEMA_MISMATCH;
    return ImcRuntime::ResponseWrite(response,
                                     m_Bytes.empty() ? nullptr : m_Bytes.data(),
                                     m_Bytes.size(), responsePayload);
}

ScriptImcRequestRef::ScriptImcRequestRef(
    std::weak_ptr<ScriptImcServiceState> state, std::shared_ptr<Control> control)
    : m_State(std::move(state)), m_Control(std::move(control)) {}

void ScriptImcRequestRef::AddRef() { ++m_RefCount; }

void ScriptImcRequestRef::Release() {
    if (--m_RefCount == 0)
        delete this;
}

bool ScriptImcRequestRef::IsValid() const {
    return m_Control && m_Control->Active.load(std::memory_order_acquire);
}

bool ScriptImcRequestRef::IsComplete() const {
    return !m_Control || !m_Control->Active.load(std::memory_order_acquire);
}

int ScriptImcRequestRef::GetStatus() const {
    return m_Control ? m_Control->Status.load(std::memory_order_acquire)
                     : BML_ERROR_INVALID_HANDLE;
}

int ScriptImcRequestRef::Cancel() {
    std::shared_ptr<ScriptImcServiceState> state = m_State.lock();
    return state && m_Control
               ? state->CancelRequest(m_Control->Id, m_Control->Generation)
               : BML_ERROR_INVALID_HANDLE;
}

ScriptImcSubscriptionRef::ScriptImcSubscriptionRef(
    std::weak_ptr<ScriptImcServiceState> state, std::shared_ptr<Control> control)
    : m_State(std::move(state)), m_Control(std::move(control)) {}

void ScriptImcSubscriptionRef::AddRef() { ++m_RefCount; }

void ScriptImcSubscriptionRef::Release() {
    if (--m_RefCount == 0)
        delete this;
}

bool ScriptImcSubscriptionRef::IsValid() const {
    return m_Control && m_Control->Active.load(std::memory_order_acquire);
}

int ScriptImcSubscriptionRef::GetStatus() const {
    return m_Control ? m_Control->Status.load(std::memory_order_acquire)
                     : BML_ERROR_INVALID_HANDLE;
}

int ScriptImcSubscriptionRef::GetDroppedCount(std::uint64_t &count) const {
    count = 0;
    std::shared_ptr<ScriptImcServiceState> state = m_State.lock();
    return state && m_Control
               ? state->DroppedCount(m_Control->Id, m_Control->Generation, count)
               : BML_ERROR_INVALID_HANDLE;
}

int ScriptImcSubscriptionRef::Cancel() {
    std::shared_ptr<ScriptImcServiceState> state = m_State.lock();
    return state && m_Control
               ? state->CancelSubscription(m_Control->Id, m_Control->Generation)
               : BML_ERROR_INVALID_HANDLE;
}

ScriptImcProviderRef::ScriptImcProviderRef(
    std::weak_ptr<ScriptImcServiceState> state, std::shared_ptr<Control> control)
    : m_State(std::move(state)), m_Control(std::move(control)) {}

ScriptImcProviderRef::~ScriptImcProviderRef() {
    Close();
}

void ScriptImcProviderRef::AddRef() { ++m_RefCount; }

void ScriptImcProviderRef::Release() {
    if (--m_RefCount == 0)
        delete this;
}

bool ScriptImcProviderRef::IsOpen() const {
    return m_Control && m_Control->Active.load(std::memory_order_acquire);
}

int ScriptImcProviderRef::GetStatus() const {
    return m_Control ? m_Control->Status.load(std::memory_order_acquire)
                     : BML_ERROR_INVALID_HANDLE;
}

int ScriptImcProviderRef::RegisterRpc(
    const std::string &route, const std::string &requestPayload,
    const std::string &responsePayload, asIScriptFunction *handler) {
    std::shared_ptr<ScriptImcServiceState> state = m_State.lock();
    if (!state || !m_Control || !IsOpen())
        return BML_ERROR_INVALID_HANDLE;
    const int status = state->RegisterProviderRpc(
        m_Control->Id, m_Control->Generation, route, requestPayload,
        responsePayload, handler);
    m_Control->Status.store(status, std::memory_order_release);
    return status;
}

int ScriptImcProviderRef::Publish(const std::string &topic,
                                  const std::string &payload,
                                  const ScriptImcRecord &message,
                                  std::uint64_t &delivered) {
    std::shared_ptr<ScriptImcServiceState> state = m_State.lock();
    if (!state || !m_Control || !IsOpen()) {
        delivered = 0;
        return BML_ERROR_INVALID_HANDLE;
    }
    const int status = state->Publish(topic, payload, message, delivered);
    m_Control->Status.store(status, std::memory_order_release);
    return status;
}

int ScriptImcProviderRef::GetSubscriberCount(const std::string &topic,
                                             std::uint64_t &count) const {
    std::shared_ptr<ScriptImcServiceState> state = m_State.lock();
    if (!state || !m_Control || !IsOpen()) {
        count = 0;
        return BML_ERROR_INVALID_HANDLE;
    }
    return state->GetSubscriberCount(topic, count);
}

int ScriptImcProviderRef::Close() {
    if (!m_Control || !m_Control->Active.load(std::memory_order_acquire))
        return BML_OK;
    std::shared_ptr<ScriptImcServiceState> state = m_State.lock();
    if (!state) {
        m_Control->Status.store(BML_ERROR_INVALID_HANDLE, std::memory_order_release);
        m_Control->Active.store(false, std::memory_order_release);
        return BML_ERROR_INVALID_HANDLE;
    }
    const int status = state->CloseProvider(m_Control->Id, m_Control->Generation);
    m_Control->Status.store(status, std::memory_order_release);
    return status;
}

ScriptImcService::ScriptImcService() = default;

ScriptImcService::~ScriptImcService() {
    try {
        Release(nullptr);
    } catch (...) {
    }
}

bool ScriptImcService::Bind(ModContext *context, ScriptMod *owner) {
    try {
        Release(nullptr);
        auto state = std::make_shared<ScriptImcServiceState>();
        state->Context = context;
        state->Owner = owner;
        state->Active = context && owner;
        m_State = std::move(state);
        return m_State->Active;
    } catch (const std::bad_alloc &) {
        return false;
    }
}

int ScriptImcService::IsRpcAvailable(const std::string &route, bool &available) {
    available = false;
    std::shared_ptr<ScriptImcServiceState> state = m_State;
    if (!state)
        return BML_ERROR_INVALID_HANDLE;
    if (route.empty())
        return BML_ERROR_INVALID_PARAMETER;
    int status = state->EnsureClient();
    if (status != BML_OK)
        return status;

    BML_ImcRpcId rpc = BML_IMC_INVALID_ID;
    int rawAvailable = 0;
    ImcRuntime &imc = state->Context->GetImcRuntime();
    status = imc.GetRpcId(state->Client, route.c_str(), &rpc);
    if (status == BML_OK)
        status = imc.IsRpcAvailable(state->Client, rpc, &rawAvailable);
    if (status == BML_OK)
        available = rawAvailable != 0;
    return status;
}

ScriptImcRequestRef *ScriptImcService::Call(
    const std::string &route, const std::string &requestPayload,
    const std::string &responsePayload, const ScriptImcRecord &request,
    asIScriptFunction *callback, unsigned int timeoutMs) {
    std::shared_ptr<ScriptImcServiceState> state = m_State;
    if (!state || !callback || !HasBridgeCallbackSignature(callback)) {
        if (state && state->Owner) {
            state->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(
                ScriptDiagnosticPhase::Runtime,
                "Generated IMC call requires BML::Detail::ImcCompletion."));
        }
        return nullptr;
    }

    std::shared_ptr<ScriptImcRequestRef::Control> control;
    ScriptImcRequestRef *ref = nullptr;
    try {
        control = std::make_shared<ScriptImcRequestRef::Control>();
        ref = new (std::nothrow) ScriptImcRequestRef(state, control);
        if (!ref)
            return nullptr;
        RequestEntry entry;
        entry.Generation = state->NextGeneration++;
        entry.Callback = callback;
        entry.Callback->AddRef();
        entry.Control = control;
        const int id = state->NextId++;
        control->Id = id;
        control->Generation = entry.Generation;
        std::lock_guard<std::mutex> guard(state->Mutex);
        if (!state->Active) {
            entry.Callback->Release();
            control->Status.store(BML_ERROR_FROZEN, std::memory_order_release);
            control->Active.store(false, std::memory_order_release);
            return ref;
        }
        state->Requests.emplace(id, std::move(entry));
    } catch (const std::bad_alloc &) {
        if (ref) ref->Release();
        return nullptr;
    }

    int status = route.empty() ? BML_ERROR_INVALID_PARAMETER : state->EnsureClient();
    BML_ImcFuture future = nullptr;
    BML_ImcPayloadTypeId expected = BML_IMC_INVALID_ID;
    std::vector<std::uint8_t> bytes;
    ImcRuntime &imc = state->Context->GetImcRuntime();
    BML_ImcRpcId rpc = BML_IMC_INVALID_ID;
    BML_ImcPayloadTypeId requestType = BML_IMC_INVALID_ID;
    if (status == BML_OK && !requestPayload.empty())
        status = request.Encode(bytes);
    if (status == BML_OK)
        status = imc.GetRpcId(state->Client, route.c_str(), &rpc);
    if (status == BML_OK && !requestPayload.empty())
        status = imc.GetPayloadTypeId(state->Client, requestPayload.c_str(), &requestType);
    if (status == BML_OK && !responsePayload.empty())
        status = imc.GetPayloadTypeId(state->Client, responsePayload.c_str(), &expected);
    if (status == BML_OK) {
        BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
        message.Data = bytes.empty() ? nullptr : bytes.data();
        message.DataSize = bytes.size();
        message.PayloadType = requestType;
        BML_ImcCallOptions options = BML_IMC_CALL_OPTIONS_INIT;
        options.TimeoutMs = timeoutMs;
        status = imc.CallRpc(state->Client, rpc,
                             requestPayload.empty() ? nullptr : &message,
                             &options, &future);
    }

    std::lock_guard<std::mutex> guard(state->Mutex);
    auto found = state->Requests.find(control->Id);
    if (found != state->Requests.end()) {
        found->second.Future = future;
        found->second.ExpectedPayload = expected;
        found->second.ImmediateStatus = status;
        if (status != BML_OK)
            control->Status.store(status, std::memory_order_release);
    } else if (future) {
        imc.FutureCancel(future);
        imc.FutureRelease(future);
    }
    return ref;
}

ScriptImcSubscriptionRef *ScriptImcService::Subscribe(
    const std::string &topic, const std::string &payload,
    asIScriptFunction *callback, unsigned int capacity) {
    std::shared_ptr<ScriptImcServiceState> state = m_State;
    if (!state || !callback || !HasBridgeCallbackSignature(callback) || !capacity) {
        if (state && state->Owner) {
            state->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(
                ScriptDiagnosticPhase::Runtime,
                "Generated IMC subscription requires BML::Detail::ImcTopicCallback and non-zero capacity."));
        }
        return nullptr;
    }

    std::shared_ptr<ScriptImcSubscriptionRef::Control> control;
    ScriptImcSubscriptionRef *ref = nullptr;
    try {
        control = std::make_shared<ScriptImcSubscriptionRef::Control>();
        ref = new (std::nothrow) ScriptImcSubscriptionRef(state, control);
        if (!ref)
            return nullptr;
        SubscriptionEntry entry;
        entry.Generation = state->NextGeneration++;
        entry.Callback = callback;
        entry.Callback->AddRef();
        entry.Control = control;
        entry.Cookie = std::make_unique<TopicCookie>();
        const int id = state->NextId++;
        control->Id = id;
        control->Generation = entry.Generation;
        entry.Cookie->State = state;
        entry.Cookie->SubscriptionId = id;
        std::lock_guard<std::mutex> guard(state->Mutex);
        if (!state->Active) {
            entry.Callback->Release();
            control->Status.store(BML_ERROR_FROZEN, std::memory_order_release);
            control->Active.store(false, std::memory_order_release);
            return ref;
        }
        state->Subscriptions.emplace(id, std::move(entry));
    } catch (const std::bad_alloc &) {
        if (ref) ref->Release();
        return nullptr;
    }

    int status = topic.empty() || payload.empty() ? BML_ERROR_INVALID_PARAMETER
                                                   : state->EnsureClient();
    BML_ImcTopicId topicId = BML_IMC_INVALID_ID;
    BML_ImcPayloadTypeId payloadType = BML_IMC_INVALID_ID;
    BML_ImcSubscription subscription = nullptr;
    ImcRuntime &imc = state->Context->GetImcRuntime();
    if (status == BML_OK)
        status = imc.GetTopicId(state->Client, topic.c_str(), &topicId);
    if (status == BML_OK)
        status = imc.GetPayloadTypeId(state->Client, payload.c_str(), &payloadType);
    TopicCookie *cookie = nullptr;
    {
        std::lock_guard<std::mutex> guard(state->Mutex);
        const auto found = state->Subscriptions.find(control->Id);
        if (found != state->Subscriptions.end())
            cookie = found->second.Cookie.get();
    }
    if (status == BML_OK) {
        BML_ImcSubscribeOptions options = BML_IMC_SUBSCRIBE_OPTIONS_INIT;
        options.Execution = BML_IMC_EXECUTION_GAME_THREAD;
        options.Capacity = capacity;
        options.ExpectedPayloadType = payloadType;
        status = imc.Subscribe(state->Client, topicId, &options, OnTopic, cookie,
                               &subscription);
    }

    std::lock_guard<std::mutex> guard(state->Mutex);
    auto found = state->Subscriptions.find(control->Id);
    if (found != state->Subscriptions.end() && status == BML_OK) {
        found->second.Subscription = subscription;
        found->second.ExpectedPayload = payloadType;
    } else {
        if (subscription)
            imc.Unsubscribe(state->Client, subscription);
        if (found != state->Subscriptions.end()) {
            if (found->second.Callback)
                found->second.Callback->Release();
            state->Subscriptions.erase(found);
        }
        control->Status.store(status, std::memory_order_release);
        control->Active.store(false, std::memory_order_release);
    }
    return ref;
}

int ScriptImcService::Publish(const std::string &topic,
                              const std::string &payload,
                              const ScriptImcRecord &message,
                              std::uint64_t &delivered) {
    const std::shared_ptr<ScriptImcServiceState> state = m_State;
    if (!state) {
        delivered = 0;
        return BML_ERROR_INVALID_HANDLE;
    }
    return state->Publish(topic, payload, message, delivered);
}

int ScriptImcService::GetSubscriberCount(const std::string &topic,
                                         std::uint64_t &count) {
    const std::shared_ptr<ScriptImcServiceState> state = m_State;
    if (!state) {
        count = 0;
        return BML_ERROR_INVALID_HANDLE;
    }
    return state->GetSubscriberCount(topic, count);
}

ScriptImcProviderRef *ScriptImcService::OpenProvider() {
    const std::shared_ptr<ScriptImcServiceState> state = m_State;
    if (!state)
        return nullptr;

    std::shared_ptr<ScriptImcProviderRef::Control> control;
    ScriptImcProviderRef *ref = nullptr;
    try {
        control = std::make_shared<ScriptImcProviderRef::Control>();
        ref = new (std::nothrow) ScriptImcProviderRef(state, control);
        if (!ref)
            return nullptr;

        const int status = state->EnsureClient();
        control->Status.store(status, std::memory_order_release);
        if (status != BML_OK) {
            control->Active.store(false, std::memory_order_release);
            return ref;
        }

        ProviderEntry provider;
        provider.Generation = state->NextGeneration++;
        provider.Control = control;
        const int id = state->NextId++;
        control->Id = id;
        control->Generation = provider.Generation;
        std::lock_guard<std::mutex> guard(state->Mutex);
        if (!state->Active) {
            control->Status.store(BML_ERROR_FROZEN, std::memory_order_release);
            control->Active.store(false, std::memory_order_release);
            return ref;
        }
        state->Providers.emplace(id, std::move(provider));
        return ref;
    } catch (const std::bad_alloc &) {
        if (ref)
            ref->Release();
        return nullptr;
    }
}

void ScriptImcService::ProcessQueuedCallbacks() {
    std::shared_ptr<ScriptImcServiceState> state = m_State;
    if (!state || !state->Active || !state->Context)
        return;
    ImcRuntime &imc = state->Context->GetImcRuntime();

    std::vector<int> requestIds;
    {
        std::lock_guard<std::mutex> guard(state->Mutex);
        requestIds.reserve(state->Requests.size());
        for (const auto &[id, entry] : state->Requests)
            requestIds.push_back(id);
    }
    for (int id : requestIds) {
        RequestEntry entry;
        BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
        int status = BML_ERROR_BUSY;
        {
            std::lock_guard<std::mutex> guard(state->Mutex);
            const auto found = state->Requests.find(id);
            if (found == state->Requests.end())
                continue;
            status = found->second.ImmediateStatus;
            if (status == BML_OK && found->second.Future)
                status = FutureCompletionStatus(imc, found->second.Future, message);
            if (status == BML_ERROR_BUSY)
                continue;
            if (status == BML_OK && message.PayloadType != found->second.ExpectedPayload)
                status = BML_ERROR_IMC_SCHEMA_MISMATCH;
            entry = std::move(found->second);
            state->Requests.erase(found);
        }

        if (entry.Future)
            imc.FutureRelease(entry.Future);
        ScriptImcRecord *record = status == BML_OK ? ScriptImcRecord::Decode(message)
                                                   : CreateScriptImcRecord();
        if (!record) {
            record = CreateScriptImcRecord();
            status = BML_ERROR_OUT_OF_MEMORY;
        }
        entry.Control->Status.store(status, std::memory_order_release);
        entry.Control->Active.store(false, std::memory_order_release);
        if (record && state->Owner && state->Owner->CanDispatchScriptServiceCallback()) {
            CallbackInvocation invocation{state->Owner, entry.Callback, status, record};
            ScriptDiagnostic diagnostic;
            if (!ExecuteCallback(invocation, diagnostic))
                state->Owner->RecordScriptDiagnostic(diagnostic);
        }
        if (record)
            record->Release();
        if (entry.Callback)
            entry.Callback->Release();
    }

    for (;;) {
        PendingTopic pending;
        asIScriptFunction *callback = nullptr;
        BML_ImcPayloadTypeId expected = BML_IMC_INVALID_ID;
        {
            std::lock_guard<std::mutex> guard(state->Mutex);
            if (state->PendingTopics.empty())
                break;
            pending = std::move(state->PendingTopics.front());
            state->PendingTopics.pop_front();
            auto found = state->Subscriptions.find(pending.SubscriptionId);
            if (found == state->Subscriptions.end())
                continue;
            callback = found->second.Callback;
            expected = found->second.ExpectedPayload;
            if (callback)
                callback->AddRef();
        }
        int status = pending.Status;
        if (status == BML_OK && pending.PayloadType != expected)
            status = BML_ERROR_IMC_SCHEMA_MISMATCH;
        BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
        message.Data = pending.Bytes.empty() ? nullptr : pending.Bytes.data();
        message.DataSize = pending.Bytes.size();
        message.PayloadType = pending.PayloadType;
        ScriptImcRecord *record = status == BML_OK ? ScriptImcRecord::Decode(message)
                                                   : CreateScriptImcRecord();
        if (!record) {
            record = CreateScriptImcRecord();
            status = BML_ERROR_OUT_OF_MEMORY;
        }
        if (record && callback && state->Owner &&
            state->Owner->CanDispatchScriptServiceCallback()) {
            CallbackInvocation invocation{state->Owner, callback, status, record};
            ScriptDiagnostic diagnostic;
            if (!ExecuteCallback(invocation, diagnostic))
                state->Owner->RecordScriptDiagnostic(diagnostic);
        }
        if (record)
            record->Release();
        if (callback)
            callback->Release();
    }
}

void ScriptImcService::Release(ScriptDiagnostic *diagnostic) {
    std::shared_ptr<ScriptImcServiceState> state = std::move(m_State);
    if (!state)
        return;
    std::unordered_map<int, RequestEntry> requests;
    std::unordered_map<int, SubscriptionEntry> subscriptions;
    std::unordered_map<int, ProviderEntry> providers;
    ModContext *context = nullptr;
    BML_ImcClient client = nullptr;
    {
        std::lock_guard<std::mutex> guard(state->Mutex);
        state->Active = false;
        context = state->Context;
        client = state->Client;
        requests.swap(state->Requests);
        subscriptions.swap(state->Subscriptions);
        providers.swap(state->Providers);
        state->PendingTopics.clear();
    }
    int releaseStatus = BML_OK;
    for (auto &[id, provider] : providers) {
        for (auto &[rpcId, registration] : provider.Rpcs) {
            if (context && client) {
                const int status = context->GetImcRuntime().UnregisterRpc(client, rpcId);
                if (releaseStatus == BML_OK && status != BML_OK &&
                    status != BML_ERROR_NOT_FOUND && status != BML_ERROR_INVALID_HANDLE)
                    releaseStatus = status;
            }
        }
        provider.Control->Status.store(BML_ERROR_CANCELLED, std::memory_order_release);
        provider.Control->Active.store(false, std::memory_order_release);
    }
    for (auto &[id, entry] : subscriptions) {
        if (context && entry.Subscription && client) {
            const int status = context->GetImcRuntime().Unsubscribe(client, entry.Subscription);
            if (releaseStatus == BML_OK && status != BML_OK && status != BML_ERROR_INVALID_HANDLE)
                releaseStatus = status;
        }
        if (entry.Callback)
            entry.Callback->Release();
        entry.Control->Status.store(BML_ERROR_CANCELLED, std::memory_order_release);
        entry.Control->Active.store(false, std::memory_order_release);
    }
    for (auto &[id, entry] : requests) {
        if (context && entry.Future) {
            ImcRuntime &imc = context->GetImcRuntime();
            imc.FutureCancel(entry.Future);
            const int status = imc.FutureRelease(entry.Future);
            if (releaseStatus == BML_OK && status != BML_OK && status != BML_ERROR_INVALID_HANDLE)
                releaseStatus = status;
        }
        if (entry.Callback)
            entry.Callback->Release();
        entry.Control->Status.store(BML_ERROR_CANCELLED, std::memory_order_release);
        entry.Control->Active.store(false, std::memory_order_release);
    }
    if (context && client) {
        const int status = context->GetImcRuntime().CloseClient(client);
        if (releaseStatus == BML_OK && status != BML_OK && status != BML_ERROR_INVALID_HANDLE)
            releaseStatus = status;
    }
    if (diagnostic && releaseStatus != BML_OK) {
        *diagnostic = MakeScriptDiagnostic(
            ScriptDiagnosticPhase::Unload,
            "IMC resources could not be released cleanly (status " +
                std::to_string(releaseStatus) + ").");
    }
}

std::size_t ScriptImcService::GetActiveCount() const {
    const std::shared_ptr<ScriptImcServiceState> state = m_State;
    if (!state)
        return 0;
    std::lock_guard<std::mutex> guard(state->Mutex);
    return state->Requests.size() + state->Subscriptions.size() + state->Providers.size();
}

std::size_t ScriptImcService::GetQueuedCallbackCount() const {
    const std::shared_ptr<ScriptImcServiceState> state = m_State;
    if (!state)
        return 0;
    std::lock_guard<std::mutex> guard(state->Mutex);
    return state->PendingTopics.size();
}

} // namespace BML

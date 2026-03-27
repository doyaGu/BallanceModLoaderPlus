#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"

#include "bml_network.h"
#include "bml_interface.hpp"
#include "bml_topics.h"

#include "WinHttpClient.h"
#include "SocketWorker.h"

namespace {

WinHttpClient *g_HttpClient = nullptr;
SocketWorker *g_SocketWorker = nullptr;

// -- HTTP sub-table --

BML_Result HttpRequestAsync(const BML_HttpRequest *request,
                            BML_HttpCallback callback,
                            BML_HttpProgressCallback progress,
                            void *user_data,
                            BML_HttpRequestId *out_id) {
    if (!request || !request->url || !callback) return BML_RESULT_INVALID_ARGUMENT;
    if (!g_HttpClient) return BML_RESULT_NOT_INITIALIZED;

    HttpPendingRequest pending;
    pending.id = g_HttpClient->AllocateId();
    pending.url = request->url;
    pending.method = request->method ? request->method : "GET";
    if (request->body && request->body_size > 0)
        pending.body.assign(request->body, request->body_size);
    if (request->headers) {
        for (const char *const *h = request->headers; *h; ++h)
            pending.headers.emplace_back(*h);
    }
    pending.timeout_ms = request->timeout_ms;
    pending.flags = request->flags;
    pending.max_retries = request->max_retries;
    pending.max_body_size = request->max_body_size;
    if (request->output_path) pending.output_path = request->output_path;
    pending.callback = callback;
    pending.progress = progress;
    pending.user_data = user_data;

    uint32_t id = g_HttpClient->Submit(std::move(pending));
    if (out_id) *out_id = id;
    return BML_RESULT_OK;
}

BML_Result HttpCancelRequest(BML_HttpRequestId id) {
    if (!g_HttpClient) return BML_RESULT_NOT_INITIALIZED;
    return g_HttpClient->Cancel(id) ? BML_RESULT_OK : BML_RESULT_NOT_FOUND;
}

uint32_t HttpGetPendingCount() {
    return g_HttpClient ? g_HttpClient->PendingCount() : 0;
}

const BML_HttpSubInterface g_HttpSub = {
    BML_IFACE_HEADER(BML_HttpSubInterface, "bml.network.http", 1, 0),
    HttpRequestAsync,
    HttpCancelRequest,
    HttpGetPendingCount,
};

// -- Socket sub-table --

BML_Result SocketOpen(BML_SocketType type, const char *host, uint16_t port,
                      BML_SocketCallback callback, void *user_data, BML_SocketId *out_id) {
    if (!g_SocketWorker) return BML_RESULT_NOT_INITIALIZED;
    uint32_t id = g_SocketWorker->AllocateId();
    BML_Result result = g_SocketWorker->Open(type, host, port, callback, user_data, id);
    if (result == BML_RESULT_OK && out_id) *out_id = id;
    return result;
}

BML_Result SocketSend(BML_SocketId id, const void *data, size_t size) {
    if (!g_SocketWorker) return BML_RESULT_NOT_INITIALIZED;
    return g_SocketWorker->Send(id, data, size);
}

BML_Result SocketSendTo(BML_SocketId id, const char *host, uint16_t port,
                        const void *data, size_t size) {
    if (!g_SocketWorker) return BML_RESULT_NOT_INITIALIZED;
    return g_SocketWorker->SendTo(id, host, port, data, size);
}

BML_Result SocketClose(BML_SocketId id) {
    if (!g_SocketWorker) return BML_RESULT_NOT_INITIALIZED;
    return g_SocketWorker->Close(id);
}

uint32_t SocketGetOpenCount() {
    return g_SocketWorker ? g_SocketWorker->OpenCount() : 0;
}

const BML_SocketSubInterface g_SocketSub = {
    BML_IFACE_HEADER(BML_SocketSubInterface, "bml.network.socket", 1, 0),
    SocketOpen,
    SocketSend,
    SocketSendTo,
    SocketClose,
    SocketGetOpenCount,
};

// -- Root interface --

const BML_NetworkInterface g_NetworkVtable = {
    BML_IFACE_HEADER(BML_NetworkInterface, BML_NETWORK_INTERFACE_ID, 1, 0),
    &g_HttpSub,
    &g_SocketSub,
};

} // namespace

class NetworkMod : public bml::Module {
    bml::imc::SubscriptionManager m_Subs;
    bml::PublishedInterface m_Published;
    WinHttpClient m_HttpClient;
    SocketWorker m_SocketWorker;

public:
    BML_Result OnAttach() override {
        if (!m_HttpClient.Start()) {
            Services().Log().Error("Failed to initialize WinHTTP session");
            return BML_RESULT_FAIL;
        }
        if (!m_SocketWorker.Start()) {
            Services().Log().Error("Failed to initialize Winsock");
            m_HttpClient.Stop();
            return BML_RESULT_FAIL;
        }

        g_HttpClient = &m_HttpClient;
        g_SocketWorker = &m_SocketWorker;

        m_Published = Publish(BML_NETWORK_INTERFACE_ID, &g_NetworkVtable, 1, 0);
        if (!m_Published) {
            Services().Log().Error("Failed to publish network interface");
            g_HttpClient = nullptr;
            g_SocketWorker = nullptr;
            m_HttpClient.Stop();
            m_SocketWorker.Stop();
            return BML_RESULT_FAIL;
        }

        m_Subs = Services().CreateSubscriptions();
        m_Subs.Add(BML_TOPIC_ENGINE_POST_PROCESS, [this](const bml::imc::Message &) {
            m_HttpClient.DrainCompletions();
            m_SocketWorker.DrainEvents();
        });

        if (m_Subs.Count() < 1) {
            Services().Log().Error("Failed to subscribe to PostProcess");
            m_Published.Reset();
            g_HttpClient = nullptr;
            g_SocketWorker = nullptr;
            m_HttpClient.Stop();
            m_SocketWorker.Stop();
            return BML_RESULT_FAIL;
        }

        Services().Log().Info("Network module initialized (HTTP + TCP/UDP)");
        return BML_RESULT_OK;
    }

    BML_Result OnPrepareDetach() override {
        m_Published.Reset();
        return BML_RESULT_OK;
    }

    void OnDetach() override {
        g_HttpClient = nullptr;
        g_SocketWorker = nullptr;
        m_SocketWorker.Stop();
        m_HttpClient.Stop();
    }
};

BML_DEFINE_MODULE(NetworkMod)

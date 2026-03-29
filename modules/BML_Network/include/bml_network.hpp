#ifndef BML_NETWORK_HPP
#define BML_NETWORK_HPP

#include "bml_network.h"
#include "bml_interface.hpp"

#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bml::net {

    // ========================================================================
    // Callback types
    // ========================================================================

    using HttpCallback = std::function<void(const BML_HttpResponse &)>;
    using HttpProgressFn = std::function<void(uint64_t downloaded, uint64_t total)>;
    using SocketCallback = std::function<void(const BML_SocketEventData &)>;

    // ========================================================================
    // URL encoding + query params
    // ========================================================================

    inline std::string UrlEncode(std::string_view input) {
        std::string result;
        result.reserve(input.size());
        for (unsigned char c : input) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
                result += static_cast<char>(c);
            } else {
                char hex[4];
                std::snprintf(hex, sizeof(hex), "%%%02X", c);
                result += hex;
            }
        }
        return result;
    }

    class QueryParams {
    public:
        QueryParams &Add(std::string_view key, std::string_view value) {
            m_Params.emplace_back(std::string(key), std::string(value));
            return *this;
        }
        QueryParams &Add(std::string_view key, int64_t value) {
            return Add(key, std::to_string(value));
        }
        std::string Encode() const {
            std::string result;
            for (size_t i = 0; i < m_Params.size(); ++i) {
                result += (i == 0 ? '?' : '&');
                result += UrlEncode(m_Params[i].first);
                result += '=';
                result += UrlEncode(m_Params[i].second);
            }
            return result;
        }
        std::string BuildUrl(std::string_view base) const {
            return std::string(base) + Encode();
        }
    private:
        std::vector<std::pair<std::string, std::string>> m_Params;
    };

    // ========================================================================
    // Response helpers
    // ========================================================================

    inline bool IsOk(int32_t c)           { return c == 200; }
    inline bool IsSuccess(int32_t c)      { return c >= 200 && c < 300; }
    inline bool IsRedirect(int32_t c)     { return c >= 300 && c < 400; }
    inline bool IsClientError(int32_t c)  { return c >= 400 && c < 500; }
    inline bool IsServerError(int32_t c)  { return c >= 500 && c < 600; }
    inline bool IsNetworkError(int32_t c) { return c == 0; }

    inline const char *FindHeader(const BML_HttpResponse &resp, const char *name) {
        if (!resp.headers || !name) return nullptr;
        for (const char *const *h = resp.headers; *h; h += 2) {
            if (h[1] && _stricmp(h[0], name) == 0) return h[1];
        }
        return nullptr;
    }

    inline std::string_view BodyView(const BML_HttpResponse &resp) {
        return resp.body ? std::string_view(resp.body, resp.body_size) : std::string_view{};
    }

    // ========================================================================
    // Header builder
    // ========================================================================

    class Headers {
    public:
        Headers &Add(const char *key, const char *value) {
            m_Storage.emplace_back(key); m_Storage.emplace_back(value); return *this;
        }
        Headers &Add(const char *key, const std::string &v) { return Add(key, v.c_str()); }
        Headers &ContentType(const char *v) { return Add("Content-Type", v); }
        Headers &Accept(const char *v) { return Add("Accept", v); }
        Headers &Authorization(const char *v) { return Add("Authorization", v); }
        Headers &Json() { return ContentType("application/json").Accept("application/json"); }
        Headers &FormUrlEncoded() { return ContentType("application/x-www-form-urlencoded"); }
        Headers &BearerToken(const char *t) {
            m_Storage.emplace_back("Authorization");
            m_Storage.push_back(std::string("Bearer ") + t);
            return *this;
        }
        Headers &IfNoneMatch(const char *e) { return Add("If-None-Match", e); }
        Headers &IfModifiedSince(const char *d) { return Add("If-Modified-Since", d); }
        Headers &UserAgent(const char *u) { return Add("User-Agent", u); }
        const char *const *Build() {
            m_Ptrs.clear();
            for (const auto &s : m_Storage) m_Ptrs.push_back(s.c_str());
            m_Ptrs.push_back(nullptr);
            return m_Ptrs.data();
        }
    private:
        std::vector<std::string> m_Storage;
        std::vector<const char *> m_Ptrs;
    };

    // ========================================================================
    // HTTP request handle
    // ========================================================================

    class HttpRequest {
    public:
        HttpRequest() = default;
        HttpRequest(const BML_HttpSubInterface *sub, BML_HttpRequestId id)
            : m_Sub(sub), m_Id(id) {}
        BML_HttpRequestId Id() const { return m_Id; }
        explicit operator bool() const { return m_Sub != nullptr && m_Id != 0; }
        void Cancel() {
            if (m_Sub && m_Sub->CancelRequest && m_Id != 0) {
                m_Sub->CancelRequest(m_Id); m_Sub = nullptr; m_Id = 0;
            }
        }
    private:
        const BML_HttpSubInterface *m_Sub = nullptr;
        BML_HttpRequestId m_Id = 0;
    };

    // ========================================================================
    // Socket handle
    // ========================================================================

    class Socket {
    public:
        Socket() = default;
        Socket(const BML_SocketSubInterface *sub, BML_SocketId id)
            : m_Sub(sub), m_Id(id) {}
        ~Socket() { FreeCbWrap(); }

        Socket(const Socket &) = delete;
        Socket &operator=(const Socket &) = delete;

        Socket(Socket &&o) noexcept
            : m_Sub(o.m_Sub), m_Id(o.m_Id), m_CbWrap(o.m_CbWrap) {
            o.m_Sub = nullptr; o.m_Id = 0; o.m_CbWrap = nullptr;
        }
        Socket &operator=(Socket &&o) noexcept {
            if (this != &o) {
                FreeCbWrap();
                m_Sub = o.m_Sub; m_Id = o.m_Id; m_CbWrap = o.m_CbWrap;
                o.m_Sub = nullptr; o.m_Id = 0; o.m_CbWrap = nullptr;
            }
            return *this;
        }

        BML_SocketId Id() const { return m_Id; }
        explicit operator bool() const { return m_Sub != nullptr && m_Id != 0; }
        BML_Result Send(const void *data, size_t size) {
            return (m_Sub && m_Sub->Send) ? m_Sub->Send(m_Id, data, size) : BML_RESULT_NOT_INITIALIZED;
        }
        BML_Result Send(std::string_view data) { return Send(data.data(), data.size()); }
        BML_Result SendTo(const char *host, uint16_t port, const void *data, size_t size) {
            return (m_Sub && m_Sub->SendTo) ? m_Sub->SendTo(m_Id, host, port, data, size) : BML_RESULT_NOT_INITIALIZED;
        }
        void Close() {
            if (m_Sub && m_Sub->Close && m_Id != 0) {
                m_Sub->Close(m_Id); m_Sub = nullptr; m_Id = 0;
            }
            FreeCbWrap();
        }
    private:
        friend Socket TcpConnect(const BML_NetworkInterface *, const char *, uint16_t, SocketCallback);
        friend Socket UdpOpen(const BML_NetworkInterface *, const char *, uint16_t, SocketCallback);

        void FreeCbWrap() {
            if (m_CbWrap) {
                delete static_cast<detail::SocketCbWrap *>(m_CbWrap);
                m_CbWrap = nullptr;
            }
        }

        const BML_SocketSubInterface *m_Sub = nullptr;
        BML_SocketId m_Id = 0;
        void *m_CbWrap = nullptr;
    };

    // ========================================================================
    // Trampolines
    // ========================================================================

    namespace detail {
        struct HttpCbPair { HttpCallback on_complete; HttpProgressFn on_progress; };
        inline void HttpCompletionTrampoline(const BML_HttpResponse *r, void *ud) {
            auto *p = static_cast<HttpCbPair *>(ud);
            if (p && p->on_complete) p->on_complete(*r);
            delete p;
        }
        inline void HttpProgressTrampoline(BML_HttpRequestId, uint64_t dl, uint64_t total, void *ud) {
            auto *p = static_cast<HttpCbPair *>(ud);
            if (p && p->on_progress) p->on_progress(dl, total);
        }
        struct SocketCbWrap { SocketCallback on_event; };
        inline void SocketTrampoline(const BML_SocketEventData *ev, void *ud) {
            auto *w = static_cast<SocketCbWrap *>(ud);
            if (w && w->on_event && ev) w->on_event(*ev);
        }
    } // namespace detail

    // ========================================================================
    // HTTP request builder (works with sub-table)
    // ========================================================================

    class HttpRequestBuilder {
    public:
        explicit HttpRequestBuilder(const BML_HttpSubInterface *sub) : m_Sub(sub) {}
        HttpRequestBuilder &Url(const char *u) { m_Req.url = u; return *this; }
        HttpRequestBuilder &Url(const std::string &u) { m_UrlStorage = u; m_Req.url = m_UrlStorage.c_str(); return *this; }
        HttpRequestBuilder &Method(const char *m) { m_Req.method = m; return *this; }
        HttpRequestBuilder &Body(const char *d, size_t s) { m_Req.body = d; m_Req.body_size = s; return *this; }
        HttpRequestBuilder &Body(std::string_view d) { return Body(d.data(), d.size()); }
        HttpRequestBuilder &SetHeaders(const char *const *h) { m_Req.headers = h; return *this; }
        HttpRequestBuilder &Timeout(uint32_t ms) { m_Req.timeout_ms = ms; return *this; }
        HttpRequestBuilder &Flags(uint32_t f) { m_Req.flags = f; return *this; }
        HttpRequestBuilder &MaxRetries(uint32_t n) { m_Req.max_retries = n; return *this; }
        HttpRequestBuilder &MaxBodySize(size_t n) { m_Req.max_body_size = n; return *this; }
        HttpRequestBuilder &NoRedirect() { m_Req.flags |= BML_HTTP_FLAG_NO_REDIRECT; return *this; }
        HttpRequestBuilder &Insecure() { m_Req.flags |= BML_HTTP_FLAG_INSECURE; return *this; }
        HttpRequestBuilder &DownloadTo(const char *p) { m_Req.output_path = p; m_Req.flags |= BML_HTTP_FLAG_DOWNLOAD; return *this; }
        HttpRequestBuilder &OnProgress(HttpProgressFn fn) { m_ProgressFn = std::move(fn); return *this; }

        HttpRequest Send(HttpCallback cb) {
            if (!m_Sub || !m_Sub->RequestAsync) return {};
            auto *pair = new detail::HttpCbPair{std::move(cb), std::move(m_ProgressFn)};
            BML_HttpRequestId id = 0;
            BML_Result r = m_Sub->RequestAsync(&m_Req, detail::HttpCompletionTrampoline,
                pair->on_progress ? detail::HttpProgressTrampoline : nullptr, pair, &id);
            if (r != BML_RESULT_OK) { delete pair; return {}; }
            return HttpRequest(m_Sub, id);
        }
    private:
        const BML_HttpSubInterface *m_Sub = nullptr;
        BML_HttpRequest m_Req = BML_HTTP_REQUEST_INIT;
        std::string m_UrlStorage;
        HttpProgressFn m_ProgressFn;
    };

    // ========================================================================
    // Top-level HTTP convenience (takes root interface, dereferences sub-table)
    // ========================================================================

    inline HttpRequestBuilder Http(const BML_NetworkInterface *n) {
        return HttpRequestBuilder(n ? n->Http : nullptr);
    }

    inline HttpRequest Get(const BML_NetworkInterface *n, const char *url, HttpCallback cb) {
        return Http(n).Url(url).Send(std::move(cb));
    }
    inline HttpRequest Head(const BML_NetworkInterface *n, const char *url, HttpCallback cb) {
        return Http(n).Url(url).Method("HEAD").Send(std::move(cb));
    }
    inline HttpRequest Delete(const BML_NetworkInterface *n, const char *url, HttpCallback cb) {
        return Http(n).Url(url).Method("DELETE").Send(std::move(cb));
    }
    inline HttpRequest Post(const BML_NetworkInterface *n, const char *url, std::string_view body, const char *ct, HttpCallback cb) {
        Headers h; h.ContentType(ct);
        return Http(n).Url(url).Method("POST").Body(body).SetHeaders(h.Build()).Send(std::move(cb));
    }
    inline HttpRequest Put(const BML_NetworkInterface *n, const char *url, std::string_view body, const char *ct, HttpCallback cb) {
        Headers h; h.ContentType(ct);
        return Http(n).Url(url).Method("PUT").Body(body).SetHeaders(h.Build()).Send(std::move(cb));
    }
    inline HttpRequest Patch(const BML_NetworkInterface *n, const char *url, std::string_view body, const char *ct, HttpCallback cb) {
        Headers h; h.ContentType(ct);
        return Http(n).Url(url).Method("PATCH").Body(body).SetHeaders(h.Build()).Send(std::move(cb));
    }
    inline HttpRequest PostJson(const BML_NetworkInterface *n, const char *url, std::string_view json, HttpCallback cb) {
        return Post(n, url, json, "application/json", std::move(cb));
    }
    inline HttpRequest PutJson(const BML_NetworkInterface *n, const char *url, std::string_view json, HttpCallback cb) {
        return Put(n, url, json, "application/json", std::move(cb));
    }
    inline HttpRequest Download(const BML_NetworkInterface *n, const char *url, const char *path, HttpCallback cb, HttpProgressFn progress = {}) {
        auto b = Http(n).Url(url).DownloadTo(path).MaxRetries(2);
        if (progress) b.OnProgress(std::move(progress));
        return b.Send(std::move(cb));
    }

    // ========================================================================
    // Socket convenience (takes root interface, dereferences sub-table)
    // ========================================================================

    inline Socket TcpConnect(const BML_NetworkInterface *n, const char *host, uint16_t port, SocketCallback cb) {
        if (!n || !n->Socket || !n->Socket->Open) return {};
        auto *w = new detail::SocketCbWrap{std::move(cb)};
        BML_SocketId id = 0;
        BML_Result r = n->Socket->Open(BML_SOCKET_TCP, host, port, detail::SocketTrampoline, w, &id);
        if (r != BML_RESULT_OK) { delete w; return {}; }
        Socket socket(n->Socket, id);
        socket.m_CbWrap = w;
        return socket;
    }

    inline Socket UdpOpen(const BML_NetworkInterface *n, const char *host, uint16_t port, SocketCallback cb) {
        if (!n || !n->Socket || !n->Socket->Open) return {};
        auto *w = new detail::SocketCbWrap{std::move(cb)};
        BML_SocketId id = 0;
        BML_Result r = n->Socket->Open(BML_SOCKET_UDP, host, port, detail::SocketTrampoline, w, &id);
        if (r != BML_RESULT_OK) { delete w; return {}; }
        Socket socket(n->Socket, id);
        socket.m_CbWrap = w;
        return socket;
    }

} // namespace bml::net

#endif // BML_NETWORK_HPP

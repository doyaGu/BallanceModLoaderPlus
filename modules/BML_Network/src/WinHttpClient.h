#ifndef BML_HTTP_WINHTTPCLIENT_H
#define BML_HTTP_WINHTTPCLIENT_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "bml_network.h"

// ============================================================================
// RAII wrapper for HINTERNET handles
// ============================================================================

class WinHttpHandle {
public:
    WinHttpHandle() = default;
    explicit WinHttpHandle(HINTERNET h) : m_Handle(h) {}
    ~WinHttpHandle() { Close(); }

    WinHttpHandle(const WinHttpHandle &) = delete;
    WinHttpHandle &operator=(const WinHttpHandle &) = delete;
    WinHttpHandle(WinHttpHandle &&o) noexcept : m_Handle(o.m_Handle) { o.m_Handle = nullptr; }
    WinHttpHandle &operator=(WinHttpHandle &&o) noexcept {
        if (this != &o) { Close(); m_Handle = o.m_Handle; o.m_Handle = nullptr; }
        return *this;
    }

    explicit operator bool() const { return m_Handle != nullptr; }
    HINTERNET Get() const { return m_Handle; }
    void Close() { if (m_Handle) { WinHttpCloseHandle(m_Handle); m_Handle = nullptr; } }

private:
    HINTERNET m_Handle = nullptr;
};

// ============================================================================
// Internal request/completion types
// ============================================================================

struct HttpPendingRequest {
    uint32_t id = 0;
    std::string url;
    std::string method;
    std::string body;
    std::vector<std::string> headers; // alternating key, value
    uint32_t timeout_ms = 0;
    uint32_t flags = 0;
    uint32_t max_retries = 0;
    size_t max_body_size = 0;
    std::string output_path;          // for download-to-file
    BML_HttpCallback callback = nullptr;
    BML_HttpProgressCallback progress = nullptr;
    void *user_data = nullptr;
};

struct HttpCompletedRequest {
    uint32_t id = 0;
    int32_t status_code = 0;
    std::string body;
    size_t body_size = 0;             // total bytes (for downloads, body may be empty)
    std::string content_type;
    std::vector<std::string> response_headers; // alternating key, value
    std::string error;
    std::string final_url;            // populated on redirect
    DWORD win_error = 0;              // WinHTTP error code (for retry logic)
    BML_HttpCallback callback = nullptr;
    void *user_data = nullptr;
};

struct HttpProgressUpdate {
    uint32_t id = 0;
    uint64_t downloaded = 0;
    uint64_t total = 0;
    BML_HttpProgressCallback callback = nullptr;
    void *user_data = nullptr;
};

// ============================================================================
// HTTP Client with thread pool
// ============================================================================

class WinHttpClient {
public:
    explicit WinHttpClient(uint32_t worker_count = 4);
    ~WinHttpClient();

    WinHttpClient(const WinHttpClient &) = delete;
    WinHttpClient &operator=(const WinHttpClient &) = delete;

    bool Start();
    void Stop();

    uint32_t AllocateId() { return m_NextId.fetch_add(1); }
    uint32_t Submit(HttpPendingRequest request);
    bool Cancel(uint32_t id);
    uint32_t PendingCount() const;

    void DrainCompletions();

private:
    void WorkerLoop();
    bool IsCancelled(uint32_t id);
    void ConsumeCancelledId(uint32_t id);
    void PostCompletion(HttpCompletedRequest result);
    void PostProgress(const HttpPendingRequest &request, uint64_t downloaded, uint64_t total);

    // HTTP execution pipeline (called from worker threads)
    HttpCompletedRequest Execute(const HttpPendingRequest &request);
    HttpCompletedRequest ExecuteOnce(const HttpPendingRequest &request);
    std::string ReadResponseBodyToMemory(HINTERNET hRequest, const HttpPendingRequest &request,
                                          uint64_t contentLength, size_t &out_size);
    bool ReadResponseBodyToFile(HINTERNET hRequest, const HttpPendingRequest &request,
                                uint64_t contentLength, size_t &out_size);
    void ParseResponseHeaders(HINTERNET hRequest, HttpCompletedRequest &result);
    static std::string FormatWinHttpError(const char *context, DWORD error);
    static bool IsRetryableStatus(int32_t status);
    static bool IsRetryableError(DWORD error);

    uint32_t m_WorkerCount;
    WinHttpHandle m_Session;
    std::vector<std::thread> m_Workers;
    std::atomic<bool> m_Running{false};

    mutable std::mutex m_QueueMutex;
    std::condition_variable m_QueueCv;
    std::queue<HttpPendingRequest> m_PendingQueue;

    std::mutex m_CompletionMutex;
    std::vector<HttpCompletedRequest> m_Completions;
    std::vector<HttpProgressUpdate> m_ProgressUpdates;

    std::mutex m_CancelMutex;
    std::vector<uint32_t> m_CancelledIds;

    std::atomic<uint32_t> m_NextId{1};
    std::atomic<uint32_t> m_InFlightCount{0};
};

#endif // BML_HTTP_WINHTTPCLIENT_H

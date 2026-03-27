#include "WinHttpClient.h"
#include "StringUtils.h"

#include <algorithm>
#include <cstdio>
#include <fstream>

#pragma comment(lib, "winhttp.lib")

static constexpr uint32_t kDefaultTimeoutMs = 30000;
static constexpr uint32_t kRetryBaseDelayMs = 500;
static constexpr wchar_t kUserAgent[] = L"BML/0.4.0";

// Narrow a substring (wchar_t pointer + length) to UTF-8.
static std::string NarrowSubstring(const wchar_t *ws, int len) {
    if (!ws || len == 0) return {};
    return utils::Utf16ToUtf8(std::wstring(ws, static_cast<size_t>(len)));
}

// ============================================================================
// URL Parsing
// ============================================================================

struct UrlParts {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    bool secure = true;
};

static bool ParseUrl(const std::string &url, UrlParts &out) {
    std::wstring wurl = utils::Utf8ToUtf16(url);
    if (wurl.empty()) return false;

    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.dwHostNameLength = 1;
    uc.dwUrlPathLength = 1;
    uc.dwExtraInfoLength = 1;

    if (!WinHttpCrackUrl(wurl.c_str(), static_cast<DWORD>(wurl.size()), 0, &uc))
        return false;

    out.host.assign(uc.lpszHostName, uc.dwHostNameLength);
    out.path.assign(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength > 0) {
        out.path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    }
    out.port = uc.nPort;
    out.secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    return true;
}

// ============================================================================
// WinHttpClient -- Lifecycle
// ============================================================================

WinHttpClient::WinHttpClient(uint32_t worker_count)
    : m_WorkerCount(std::max(1u, worker_count)) {}

WinHttpClient::~WinHttpClient() {
    Stop();
}

bool WinHttpClient::Start() {
    if (m_Running.load()) return true;

    m_Session = WinHttpHandle(WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!m_Session) return false;

    m_Running.store(true);
    m_Workers.reserve(m_WorkerCount);
    for (uint32_t i = 0; i < m_WorkerCount; ++i) {
        m_Workers.emplace_back(&WinHttpClient::WorkerLoop, this);
    }
    return true;
}

void WinHttpClient::Stop() {
    if (!m_Running.exchange(false)) return;

    m_QueueCv.notify_all();
    for (auto &w : m_Workers) {
        if (w.joinable()) w.join();
    }
    m_Workers.clear();

    // Deliver any completions that arrived before workers stopped,
    // so callers get their error/success callbacks instead of silence.
    DrainCompletions();

    m_Session.Close();

    {
        std::lock_guard lock(m_QueueMutex);
        m_PendingQueue = {};
    }
    {
        std::lock_guard lock(m_CompletionMutex);
        m_Completions.clear();
        m_ProgressUpdates.clear();
    }
    {
        std::lock_guard lock(m_CancelMutex);
        m_CancelledIds.clear();
    }
}

// ============================================================================
// WinHttpClient -- Submit / Cancel / Status
// ============================================================================

uint32_t WinHttpClient::Submit(HttpPendingRequest request) {
    uint32_t id = request.id;
    m_InFlightCount.fetch_add(1);
    {
        std::lock_guard lock(m_QueueMutex);
        m_PendingQueue.push(std::move(request));
    }
    m_QueueCv.notify_one();
    return id;
}

bool WinHttpClient::Cancel(uint32_t id) {
    std::lock_guard lock(m_CancelMutex);
    m_CancelledIds.push_back(id);
    return true;
}

uint32_t WinHttpClient::PendingCount() const {
    return m_InFlightCount.load();
}

bool WinHttpClient::IsCancelled(uint32_t id) {
    std::lock_guard lock(m_CancelMutex);
    return std::find(m_CancelledIds.begin(), m_CancelledIds.end(), id) != m_CancelledIds.end();
}

void WinHttpClient::ConsumeCancelledId(uint32_t id) {
    std::lock_guard lock(m_CancelMutex);
    auto it = std::find(m_CancelledIds.begin(), m_CancelledIds.end(), id);
    if (it != m_CancelledIds.end()) m_CancelledIds.erase(it);
}

void WinHttpClient::PostCompletion(HttpCompletedRequest result) {
    std::lock_guard lock(m_CompletionMutex);
    m_Completions.push_back(std::move(result));
}

void WinHttpClient::PostProgress(const HttpPendingRequest &request, uint64_t downloaded, uint64_t total) {
    if (!request.progress) return;
    HttpProgressUpdate update;
    update.id = request.id;
    update.downloaded = downloaded;
    update.total = total;
    update.callback = request.progress;
    update.user_data = request.user_data;

    std::lock_guard lock(m_CompletionMutex);
    m_ProgressUpdates.push_back(update);
}

// ============================================================================
// WinHttpClient -- Main-Thread Completion Delivery
// ============================================================================

void WinHttpClient::DrainCompletions() {
    std::vector<HttpCompletedRequest> completions;
    std::vector<HttpProgressUpdate> progress;
    {
        std::lock_guard lock(m_CompletionMutex);
        completions.swap(m_Completions);
        progress.swap(m_ProgressUpdates);
    }

    for (const auto &p : progress) {
        if (p.callback) p.callback(p.id, p.downloaded, p.total, p.user_data);
    }

    for (const auto &comp : completions) {
        if (!comp.callback) continue;

        std::vector<const char *> headerPtrs;
        for (const auto &h : comp.response_headers) {
            headerPtrs.push_back(h.c_str());
        }
        headerPtrs.push_back(nullptr);

        BML_HttpResponse response = BML_HTTP_RESPONSE_INIT;
        response.request_id = comp.id;
        response.status_code = comp.status_code;
        response.body = comp.body.empty() ? nullptr : comp.body.c_str();
        response.body_size = comp.body_size;
        response.content_type = comp.content_type.empty() ? nullptr : comp.content_type.c_str();
        response.headers = headerPtrs.data();
        response.error = comp.error.empty() ? nullptr : comp.error.c_str();
        response.final_url = comp.final_url.empty() ? nullptr : comp.final_url.c_str();

        comp.callback(&response, comp.user_data);
    }
}

// ============================================================================
// WinHttpClient -- Worker Thread
// ============================================================================

void WinHttpClient::WorkerLoop() {
    while (m_Running.load()) {
        HttpPendingRequest request;
        {
            std::unique_lock lock(m_QueueMutex);
            m_QueueCv.wait(lock, [this] {
                return !m_PendingQueue.empty() || !m_Running.load();
            });
            if (!m_Running.load()) break;
            if (m_PendingQueue.empty()) continue;

            request = std::move(m_PendingQueue.front());
            m_PendingQueue.pop();
        }

        if (IsCancelled(request.id)) {
            ConsumeCancelledId(request.id);
            m_InFlightCount.fetch_sub(1);
            continue;
        }

        auto result = Execute(request);

        if (IsCancelled(request.id)) {
            ConsumeCancelledId(request.id);
        } else {
            PostCompletion(std::move(result));
        }
        m_InFlightCount.fetch_sub(1);
    }
}

// ============================================================================
// WinHttpClient -- HTTP Execution with Retry
// ============================================================================

std::string WinHttpClient::FormatWinHttpError(const char *context, DWORD error) {
    char buf[192];
    std::snprintf(buf, sizeof(buf), "%s (WinHTTP error %lu)", context, error);
    return buf;
}

bool WinHttpClient::IsRetryableStatus(int32_t status) {
    return status == 429 || status == 502 || status == 503 || status == 504;
}

bool WinHttpClient::IsRetryableError(DWORD error) {
    switch (error) {
    case ERROR_WINHTTP_TIMEOUT:
    case ERROR_WINHTTP_CONNECTION_ERROR:
    case ERROR_WINHTTP_NAME_NOT_RESOLVED:
        return true;
    default:
        return false;
    }
}

HttpCompletedRequest WinHttpClient::Execute(const HttpPendingRequest &request) {
    uint32_t maxAttempts = 1 + request.max_retries;
    HttpCompletedRequest result;

    for (uint32_t attempt = 0; attempt < maxAttempts; ++attempt) {
        if (attempt > 0) {
            if (IsCancelled(request.id)) break;
            uint32_t delay = kRetryBaseDelayMs * (1u << std::min(attempt - 1, 4u));
            Sleep(delay);
        }

        result = ExecuteOnce(request);

        bool shouldRetry = (result.status_code == 0 && IsRetryableError(result.win_error)) ||
                           (result.status_code > 0 && IsRetryableStatus(result.status_code));
        if (!shouldRetry || attempt + 1 >= maxAttempts) break;
    }

    return result;
}

HttpCompletedRequest WinHttpClient::ExecuteOnce(const HttpPendingRequest &request) {
    HttpCompletedRequest result;
    result.id = request.id;
    result.callback = request.callback;
    result.user_data = request.user_data;

    // 1. Parse URL
    UrlParts parts;
    if (!ParseUrl(request.url, parts)) {
        result.error = "Failed to parse URL: " + request.url;
        return result;
    }

    // 2. Connect
    WinHttpHandle hConnect(WinHttpConnect(m_Session.Get(), parts.host.c_str(), parts.port, 0));
    if (!hConnect) {
        result.win_error = GetLastError();
        result.error = FormatWinHttpError("WinHttpConnect failed", result.win_error);
        return result;
    }

    // 3. Open request
    std::wstring wMethod = utils::Utf8ToUtf16(request.method.empty() ? "GET" : request.method);
    DWORD flags = parts.secure ? WINHTTP_FLAG_SECURE : 0;

    WinHttpHandle hRequest(WinHttpOpenRequest(hConnect.Get(), wMethod.c_str(), parts.path.c_str(),
                                               nullptr, WINHTTP_NO_REFERER,
                                               WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!hRequest) {
        result.win_error = GetLastError();
        result.error = FormatWinHttpError("WinHttpOpenRequest failed", result.win_error);
        return result;
    }

    // 4. Configure
    uint32_t timeout = request.timeout_ms > 0 ? request.timeout_ms : kDefaultTimeoutMs;
    int t = static_cast<int>(timeout);
    WinHttpSetTimeouts(hRequest.Get(), t, t, t, t);

    if (request.flags & BML_HTTP_FLAG_NO_REDIRECT) {
        DWORD optFlags = WINHTTP_DISABLE_REDIRECTS;
        WinHttpSetOption(hRequest.Get(), WINHTTP_OPTION_DISABLE_FEATURE, &optFlags, sizeof(optFlags));
    }
    if (request.flags & BML_HTTP_FLAG_INSECURE) {
        DWORD secFlags = SECURITY_FLAG_IGNORE_ALL_CERT_ERRORS;
        WinHttpSetOption(hRequest.Get(), WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
    }

    // 5. Headers
    for (size_t i = 0; i + 1 < request.headers.size(); i += 2) {
        std::wstring header = utils::Utf8ToUtf16(request.headers[i]) + L": " + utils::Utf8ToUtf16(request.headers[i + 1]);
        WinHttpAddRequestHeaders(hRequest.Get(), header.c_str(), static_cast<DWORD>(header.size()),
                                  WINHTTP_ADDREQ_FLAG_ADD);
    }

    // 6. Send
    LPCVOID bodyPtr = request.body.empty() ? WINHTTP_NO_REQUEST_DATA : request.body.c_str();
    DWORD bodyLen = static_cast<DWORD>(request.body.size());

    if (!WinHttpSendRequest(hRequest.Get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                             const_cast<LPVOID>(bodyPtr), bodyLen, bodyLen, 0)) {
        result.win_error = GetLastError();
        result.error = FormatWinHttpError("WinHttpSendRequest failed", result.win_error);
        return result;
    }

    // 7. Receive response headers
    if (!WinHttpReceiveResponse(hRequest.Get(), nullptr)) {
        result.win_error = GetLastError();
        result.error = FormatWinHttpError("WinHttpReceiveResponse failed", result.win_error);
        return result;
    }

    // 8. Status code
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest.Get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                         WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
                         WINHTTP_NO_HEADER_INDEX);
    result.status_code = static_cast<int32_t>(statusCode);

    // 9. Content-Length
    DWORD contentLenDw = 0;
    DWORD contentLenSize = sizeof(contentLenDw);
    uint64_t contentLength = 0;
    if (WinHttpQueryHeaders(hRequest.Get(), WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &contentLenDw, &contentLenSize,
                             WINHTTP_NO_HEADER_INDEX)) {
        contentLength = contentLenDw;
    }

    // 10. Final URL (after redirects)
    {
        DWORD urlSize = 0;
        WinHttpQueryOption(hRequest.Get(), WINHTTP_OPTION_URL, nullptr, &urlSize);
        if (urlSize > 0) {
            std::wstring finalUrl(urlSize / sizeof(wchar_t), L'\0');
            if (WinHttpQueryOption(hRequest.Get(), WINHTTP_OPTION_URL, finalUrl.data(), &urlSize)) {
                std::string narrow = utils::Utf16ToUtf8(finalUrl.c_str());
                if (narrow != request.url) {
                    result.final_url = std::move(narrow);
                }
            }
        }
    }

    // 11. Response headers
    ParseResponseHeaders(hRequest.Get(), result);

    // 11. Body -- to file or memory
    size_t totalRead = 0;
    if (request.flags & BML_HTTP_FLAG_DOWNLOAD) {
        if (!ReadResponseBodyToFile(hRequest.Get(), request, contentLength, totalRead)) {
            if (result.error.empty()) result.error = "Failed to write response to file";
        }
    } else {
        result.body = ReadResponseBodyToMemory(hRequest.Get(), request, contentLength, totalRead);
    }
    result.body_size = totalRead;

    return result;
}

// ============================================================================
// WinHttpClient -- Response Body Readers
// ============================================================================

std::string WinHttpClient::ReadResponseBodyToMemory(HINTERNET hRequest, const HttpPendingRequest &request,
                                                     uint64_t contentLength, size_t &out_size) {
    std::string body;
    if (contentLength > 0 && (request.max_body_size == 0 || contentLength <= request.max_body_size)) {
        body.reserve(static_cast<size_t>(contentLength));
    }

    out_size = 0;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(hRequest, &available) && available > 0) {
        if (request.max_body_size > 0 && out_size + available > request.max_body_size) {
            break; // body size limit exceeded
        }
        std::vector<char> chunk(available);
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, chunk.data(), available, &read)) break;
        body.append(chunk.data(), read);
        out_size += read;

        PostProgress(request, out_size, contentLength);
    }
    return body;
}

bool WinHttpClient::ReadResponseBodyToFile(HINTERNET hRequest, const HttpPendingRequest &request,
                                            uint64_t contentLength, size_t &out_size) {
    if (request.output_path.empty()) return false;

    std::ofstream file(request.output_path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;

    out_size = 0;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(hRequest, &available) && available > 0) {
        std::vector<char> chunk(available);
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, chunk.data(), available, &read)) break;
        file.write(chunk.data(), read);
        if (!file.good()) break;
        out_size += read;

        PostProgress(request, out_size, contentLength);
    }

    file.close();
    return file.good() || out_size > 0;
}

// ============================================================================
// WinHttpClient -- Response Header Parsing
// ============================================================================

void WinHttpClient::ParseResponseHeaders(HINTERNET hRequest, HttpCompletedRequest &result) {
    DWORD headersSize = 0;
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                         WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &headersSize,
                         WINHTTP_NO_HEADER_INDEX);
    if (headersSize == 0) return;

    std::wstring rawHeaders(headersSize / sizeof(wchar_t), L'\0');
    if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                              WINHTTP_HEADER_NAME_BY_INDEX, rawHeaders.data(), &headersSize,
                              WINHTTP_NO_HEADER_INDEX)) {
        return;
    }

    size_t pos = 0;
    bool firstLine = true;
    while (pos < rawHeaders.size()) {
        size_t eol = rawHeaders.find(L"\r\n", pos);
        if (eol == std::wstring::npos) break;

        if (firstLine) { firstLine = false; pos = eol + 2; continue; }
        if (eol == pos) break;

        size_t colon = rawHeaders.find(L':', pos);
        if (colon != std::wstring::npos && colon < eol) {
            std::string key = NarrowSubstring(rawHeaders.c_str() + pos, static_cast<int>(colon - pos));
            size_t valStart = colon + 1;
            while (valStart < eol && rawHeaders[valStart] == L' ') ++valStart;
            std::string val = NarrowSubstring(rawHeaders.c_str() + valStart, static_cast<int>(eol - valStart));

            if (_stricmp(key.c_str(), "content-type") == 0) {
                result.content_type = val;
            }

            result.response_headers.push_back(std::move(key));
            result.response_headers.push_back(std::move(val));
        }
        pos = eol + 2;
    }
}

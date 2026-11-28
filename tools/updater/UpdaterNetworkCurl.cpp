#include "UpdaterNetwork.h"
#include "UpdaterPaths.h"
#include "UpdaterRemote.h"
#include "UpdaterSdl.h"
#include "Platform/FileSystemNative.h"
#include <SDL3/SDL.h>
#include <curl/curl.h>
#include <atomic>
#include <limits>
#include <memory>

namespace bmlupdater {
namespace {
struct CurlRuntime {
    CURLcode result{curl_global_init(CURL_GLOBAL_DEFAULT)};
    ~CurlRuntime() { if (result == CURLE_OK) curl_global_cleanup(); }
};
struct Download {
    std::string text;
    SDL_IOStream *stream{};
    uint64_t total{};
    uint64_t limit{};
    ProgressCallback progress;
    std::string error;
    bool acceptBody{false};
};
size_t Header(char *data, size_t size, size_t count, void *opaque) {
    if (size && count > std::numeric_limits<size_t>::max() / size) return 0;
    const size_t bytes = size * count;
    std::string_view line(data, bytes);
    if (line.starts_with("HTTP/")) {
        const auto space = line.find(' ');
        static_cast<Download *>(opaque)->acceptBody = space != std::string_view::npos && line.substr(space + 1, 3) == "200";
    }
    return bytes;
}
size_t Write(void *data, size_t size, size_t count, void *opaque) {
    auto &download = *static_cast<Download *>(opaque);
    try {
        if (size && count > std::numeric_limits<size_t>::max() / size) return 0;
        const auto bytes = size * count;
        if (!download.acceptBody) return bytes;
        if (bytes > download.limit - download.total) { download.error = "Download exceeds size limit"; return 0; }
        if (download.stream) {
            if (SDL_WriteIO(download.stream, data, bytes) != bytes) { download.error = SDL_GetError(); return 0; }
        } else download.text.append(static_cast<char *>(data), bytes);
        download.total += bytes;
        if (download.progress) download.progress("downloaded " + std::to_string(download.total) + " bytes");
        return bytes;
    } catch (...) { download.error = "Download callback failed"; return 0; }
}
Result Get(const std::wstring &url, Download &download, std::chrono::milliseconds timeout) {
    auto address = PathUtf8(url);
    auto valid = ValidateRemoteHttpsUrl(address, "URL");
    if (!valid.ok) return valid;
    static CurlRuntime runtime;
    if (runtime.result != CURLE_OK) return Result::Failure("Unable to initialize libcurl");
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) return Result::Failure("Unable to create libcurl request");
    char error[CURL_ERROR_SIZE]{};
    curl_easy_setopt(curl.get(), CURLOPT_URL, address.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "BMLPlusUpdater/2.0");
    curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, error);
#if LIBCURL_VERSION_NUM >= 0x075500
    if (curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "https") != CURLE_OK ||
        curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS_STR, "https") != CURLE_OK)
#else
    if (curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS, static_cast<long>(CURLPROTO_HTTPS)) != CURLE_OK ||
        curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS, static_cast<long>(CURLPROTO_HTTPS)) != CURLE_OK)
#endif
        return Result::Failure("libcurl cannot restrict requests and redirects to HTTPS");
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT_MS, static_cast<long>(timeout.count()));
    curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, Write);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &download);
    curl_easy_setopt(curl.get(), CURLOPT_HEADERFUNCTION, Header);
    curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA, &download);
    // Trust a user-supplied CA bundle without disabling TLS verification.
    const char *ca = SDL_getenv("CURL_CA_BUNDLE");
    if (!ca || !*ca) ca = SDL_getenv("SSL_CERT_FILE");
    if (ca && *ca && curl_easy_setopt(curl.get(), CURLOPT_CAINFO, ca) != CURLE_OK)
        return Result::Failure("Unable to configure the TLS CA bundle");
    const auto result = curl_easy_perform(curl.get());
    if (result != CURLE_OK)
        return Result::Failure(!download.error.empty() ? download.error :
                               "HTTPS download failed: " + std::string(*error ? error : curl_easy_strerror(result)));
    long status{};
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
    if (status != 200) return Result::Failure("HTTP GET failed with status " + std::to_string(status));
    return Result::Success();
}
}
Result HttpGetUtf8(const std::wstring &url, std::string &body) {
    body.clear();
    Download download;
    download.limit = 16 * 1024 * 1024;
    auto result = Get(url, download, DefaultHttpTimeout);
    if (result.ok) body = std::move(download.text);
    return result;
}
Result HttpDownloadFile(const std::wstring &url, const std::wstring &destination,
                        ProgressCallback progress, std::chrono::milliseconds timeout) {
    if (timeout <= std::chrono::milliseconds::zero() || timeout > DefaultHttpTimeout)
        return Result::Failure("Invalid HTTP timeout");
    if (!CreateDirectories(ParentPath(destination)) || BML::Platform::IsLink(PathUtf8(destination)))
        return Result::Failure("Unable to prepare download destination");
    static std::atomic<uint64_t> sequence{0};
    auto temporary = destination + L".download-" + std::to_wstring(BML::Platform::ProcessId()) + L"-" +
                     std::to_wstring(SDL_GetTicksNS()) + L"-" + std::to_wstring(sequence.fetch_add(1));
    struct Cleanup { std::wstring path; ~Cleanup() { SDL_RemovePath(PathUtf8(path).c_str()); } } cleanup{temporary};
    Download download;
    download.limit = 2ULL * 1024 * 1024 * 1024;
    download.progress = std::move(progress);
    download.stream = SDL_IOFromFile(PathUtf8(temporary).c_str(), "wbx");
    if (!download.stream) return Result::Failure("Unable to create download file: " + std::string(SDL_GetError()));
    auto result = Get(url, download, timeout);
    bool synced = result.ok && BML::Platform::SyncFile(download.stream);
    if (!SDL_CloseIO(download.stream)) synced = false;
    if (!result.ok) return result;
    if (!synced || !SDL_RenamePath(PathUtf8(temporary).c_str(), PathUtf8(destination).c_str()) ||
        !SyncDirectory(ParentPath(destination)))
        return Result::Failure("Unable to publish downloaded file: " + std::string(SDL_GetError()));
    return Result::Success("download complete");
}
}

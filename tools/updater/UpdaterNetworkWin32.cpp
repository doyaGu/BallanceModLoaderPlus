#include "UpdaterNetwork.h"

#include <Windows.h>
#include <winhttp.h>

#include <array>
#include <atomic>
#include <chrono>
#include <string>

#include "UpdaterPaths.h"
#include "Platform/FileSystemNative.h"

namespace bmlupdater {
    namespace {
        struct Handle {
            HINTERNET value{};
            ~Handle() {
                if (value) {
                    WinHttpCloseHandle(value);
                }
            }
            operator HINTERNET() const { return value; }
        };

        Result LastErrorResult(const char *prefix) {
            return Result::Failure(std::string(prefix) + " (WinHTTP error " + std::to_string(GetLastError()) + ")");
        }

        Result OpenHttpsGetRequest(const std::wstring &url,
                                   Handle &session,
                                   Handle &connect,
                                   Handle &request) {
            URL_COMPONENTSW parts{};
            parts.dwStructSize = sizeof(parts);
            parts.dwSchemeLength = static_cast<DWORD>(-1);
            parts.dwHostNameLength = static_cast<DWORD>(-1);
            parts.dwUrlPathLength = static_cast<DWORD>(-1);
            parts.dwExtraInfoLength = static_cast<DWORD>(-1);
            if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &parts)) {
                return LastErrorResult("Unable to parse URL");
            }
            if (parts.nScheme != INTERNET_SCHEME_HTTPS) {
                return Result::Failure("Updater remote sources must use HTTPS");
            }

            session.value = WinHttpOpen(L"BMLPlusUpdater/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!session) {
                return LastErrorResult("Unable to open WinHTTP session");
            }

            if (!WinHttpSetTimeouts(session, 10000, 10000, 30000, 30000))
                return LastErrorResult("Unable to configure HTTP timeouts");

            std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
            connect.value = WinHttpConnect(session, host.c_str(), parts.nPort, 0);
            if (!connect) {
                return LastErrorResult("Unable to connect");
            }

            std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
            if (parts.dwExtraInfoLength > 0) {
                path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
            }
            request.value = WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
            if (!request) {
                return LastErrorResult("Unable to open HTTP request");
            }

            DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
            if (!WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect)))
                return LastErrorResult("Unable to enforce HTTPS redirects");

            if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
                !WinHttpReceiveResponse(request, nullptr)) {
                return LastErrorResult("HTTP request failed");
            }

            DWORD status = 0;
            DWORD statusSize = sizeof(status);
            if (!WinHttpQueryHeaders(request,
                                     WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                     WINHTTP_HEADER_NAME_BY_INDEX,
                                     &status,
                                     &statusSize,
                                     WINHTTP_NO_HEADER_INDEX)) {
                return LastErrorResult("Unable to read HTTP status");
            }
            if (status != 200) {
                return Result::Failure("HTTP GET failed with status " + std::to_string(status));
            }
            return Result::Success();
        }

        template <class Sink>
        Result ReadResponse(const std::wstring &url, uint64_t limit, Sink sink) {
            Handle session, connection, request;
            auto opened = OpenHttpsGetRequest(url, session, connection, request);
            if (!opened.ok)
                return opened;
            std::array<char, 64 * 1024> buffer{};
            uint64_t total = 0;
            for (;;) {
                DWORD read = 0;
                if (!WinHttpReadData(request, buffer.data(), static_cast<DWORD>(buffer.size()), &read))
                    return LastErrorResult("Unable to read response data");
                if (!read)
                    return Result::Success();
                if (read > limit - total)
                    return Result::Failure("HTTP response exceeds the download limit");
                total += read;
                auto result = sink(buffer.data(), read, total);
                if (!result.ok)
                    return result;
            }
        }

        struct DownloadFile {
            std::wstring Path;
            HANDLE File = INVALID_HANDLE_VALUE;
            ~DownloadFile() {
                if (File != INVALID_HANDLE_VALUE)
                    CloseHandle(File);
                if (!Path.empty())
                    DeleteFileW(Path.c_str());
            }
        };
    } // namespace

    Result HttpGetUtf8(const std::wstring &url, std::string &body) {
        body.clear();
        std::string response;
        auto result = ReadResponse(url, 16 * 1024 * 1024, [&](const char *data, DWORD read, uint64_t) {
            response.append(data, read);
            return Result::Success();
        });
        if (result.ok)
            body = std::move(response);
        return result;
    }

    Result HttpDownloadFile(const std::wstring &url, const std::wstring &destination,
                            ProgressCallback progress) {
        if (!CreateDirectories(ParentPath(destination)) || BML::Platform::IsLink(PathUtf8(destination)))
            return Result::Failure("Unable to prepare download destination");
        static std::atomic<uint64_t> sequence{0};
        const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto temporary = destination + L".download-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
            std::to_wstring(ticks) + L"-" + std::to_wstring(sequence.fetch_add(1));
        DownloadFile download;
        download.File = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
        if (download.File == INVALID_HANDLE_VALUE)
            return LastErrorResult("Unable to create download file");
        download.Path = temporary;
        auto result = ReadResponse(url, 2ULL * 1024 * 1024 * 1024, [&](const char *data, DWORD read, uint64_t total) {
            DWORD written = 0;
            if (!WriteFile(download.File, data, read, &written, nullptr) || written != read)
                return LastErrorResult("Unable to write download file");
            if (progress)
                progress("downloaded " + std::to_string(total) + " bytes");
            return Result::Success();
        });
        if (!result.ok)
            return result;
        if (!FlushFileBuffers(download.File))
            return LastErrorResult("Unable to flush download file");
        const bool closed = CloseHandle(download.File) != FALSE;
        download.File = INVALID_HANDLE_VALUE;
        uint32_t error{};
        if (!closed || !BML::Platform::PublishFile(PathUtf8(temporary), PathUtf8(destination), true, error))
            return Result::Failure("Unable to publish downloaded file (Win32 error " + std::to_string(error) + ")");
        download.Path.clear();
        return Result::Success("download complete");
    }
} // namespace bmlupdater

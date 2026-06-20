#include "UpdaterNetwork.h"

#include <Windows.h>
#include <winhttp.h>

#include <string>
#include <vector>

#include "UpdaterPaths.h"

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
    } // namespace

    Result HttpGetUtf8(const std::wstring &url, std::string &body) {
        body.clear();
        Handle session;
        Handle connect;
        Handle request;
        Result opened = OpenHttpsGetRequest(url, session, connect, request);
        if (!opened.ok) {
            return opened;
        }

        for (;;) {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request, &available)) {
                return LastErrorResult("Unable to query response data");
            }
            if (available == 0) {
                break;
            }
            std::vector<char> buffer(available);
            DWORD read = 0;
            if (!WinHttpReadData(request, buffer.data(), available, &read)) {
                return LastErrorResult("Unable to read response data");
            }
            body.append(buffer.data(), read);
        }
        return Result::Success();
    }

    Result HttpDownloadFile(const std::wstring &url,
                            const std::wstring &destination,
                            ProgressCallback progress) {
        if (!CreateDirectories(ParentPath(destination))) {
            return Result::Failure("Unable to create download directory: " + PathUtf8(ParentPath(destination)));
        }

        Handle session;
        Handle connect;
        Handle request;
        Result opened = OpenHttpsGetRequest(url, session, connect, request);
        if (!opened.ok) {
            return opened;
        }

        const std::wstring tempPath = destination + L".tmp";
        HANDLE file = CreateFileW(tempPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return LastErrorResult("Unable to create download file");
        }

        uint64_t total = 0;
        for (;;) {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request, &available)) {
                CloseHandle(file);
                std::string ignored;
                (void)RemoveFileIfPresent(tempPath, ignored);
                return LastErrorResult("Unable to query response data");
            }
            if (available == 0) {
                break;
            }
            std::vector<char> buffer(available);
            DWORD read = 0;
            if (!WinHttpReadData(request, buffer.data(), available, &read)) {
                CloseHandle(file);
                std::string ignored;
                (void)RemoveFileIfPresent(tempPath, ignored);
                return LastErrorResult("Unable to read response data");
            }
            DWORD written = 0;
            if (!WriteFile(file, buffer.data(), read, &written, nullptr) || written != read) {
                CloseHandle(file);
                std::string ignored;
                (void)RemoveFileIfPresent(tempPath, ignored);
                return LastErrorResult("Unable to write download file");
            }
            total += read;
            if (progress) {
                progress("downloaded " + std::to_string(total) + " bytes");
            }
        }
        CloseHandle(file);

        std::string error;
        (void)RemoveFileIfPresent(destination, error);
        if (MoveFileW(tempPath.c_str(), destination.c_str()) == FALSE) {
            (void)RemoveFileIfPresent(tempPath, error);
            return LastErrorResult("Unable to finalize download file");
        }
        return Result::Success("download complete");
    }
} // namespace bmlupdater

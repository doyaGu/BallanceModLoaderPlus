#include "UpdaterNetwork.h"

#include <Windows.h>
#include <winhttp.h>

#include <string>
#include <vector>

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
    } // namespace

    Result HttpGetUtf8(const std::wstring &url, std::string &body) {
        body.clear();
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

        Handle session{WinHttpOpen(L"BMLPlusUpdater/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
        if (!session) {
            return LastErrorResult("Unable to open WinHTTP session");
        }

        std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
        Handle connect{WinHttpConnect(session, host.c_str(), parts.nPort, 0)};
        if (!connect) {
            return LastErrorResult("Unable to connect");
        }

        std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
        if (parts.dwExtraInfoLength > 0) {
            path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
        }
        Handle request{WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)};
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
} // namespace bmlupdater

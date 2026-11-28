#include "UpdaterNetwork.h"

#include <Windows.h>
#include <winhttp.h>

#include <array>
#include <atomic>
#include <chrono>
#include <string>
#include <utility>

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

        Result LastErrorResult(const char *prefix, DWORD error = GetLastError()) {
            return Result::Failure(std::string(prefix) + " (WinHTTP error " + std::to_string(error) + ")");
        }

        using Clock = std::chrono::steady_clock;

        Result DeadlineExceeded() {
            return Result::Failure("HTTP request exceeded its total deadline");
        }

        std::chrono::milliseconds NetworkTimeout() {
#ifdef BML_UPDATER_NETWORK_TEST
            wchar_t value[2]{};
            if (GetEnvironmentVariableW(L"BML_TEST_UPDATER_DEADLINE", value, 2) == 1 &&
                value[0] == L'1')
                return std::chrono::milliseconds(750);
#endif
            return std::chrono::minutes(10);
        }

        struct AsyncRequest {
            Handle Request;
            HANDLE Completed = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            HANDLE Closing = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            std::atomic<DWORD> Error{ERROR_SUCCESS};
            std::atomic<DWORD> BytesRead{0};
            bool Armed = false;

            ~AsyncRequest() {
                // WinHTTP can issue callbacks after cancellation. Keep the state
                // and the caller's read buffer alive until the final notification.
                if (Armed && WinHttpCloseHandle(std::exchange(Request.value, nullptr)))
                    WaitForSingleObject(Closing, INFINITE);
                if (Closing)
                    CloseHandle(Closing);
                if (Completed)
                    CloseHandle(Completed);
            }

            bool RegisterCallback() {
                if (!Completed || !Closing)
                    return false;
                DWORD_PTR context = reinterpret_cast<DWORD_PTR>(this);
                if (!WinHttpSetOption(Request, WINHTTP_OPTION_CONTEXT_VALUE, &context, sizeof(context)))
                    return false;
                constexpr DWORD flags = WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS |
                                        WINHTTP_CALLBACK_FLAG_HANDLES;
                Armed = WinHttpSetStatusCallback(Request, &OnStatus, flags, 0) !=
                    WINHTTP_INVALID_STATUS_CALLBACK;
                return Armed;
            }

            template <class Submit>
            Result RunUntil(Clock::time_point deadline, const char *failure, Submit submit) {
                if (Clock::now() >= deadline)
                    return DeadlineExceeded();
                Error.store(ERROR_SUCCESS);
                BytesRead.store(0);
                if (!submit()) {
                    const DWORD error = GetLastError();
                    if (error != ERROR_IO_PENDING)
                        return LastErrorResult(failure, error);
                }
                const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - Clock::now());
                if (remaining.count() <= 0)
                    return DeadlineExceeded();
                const DWORD wait = WaitForSingleObject(Completed, static_cast<DWORD>(remaining.count()));
                if (wait == WAIT_TIMEOUT || Clock::now() >= deadline)
                    return DeadlineExceeded();
                if (wait != WAIT_OBJECT_0)
                    return LastErrorResult(failure);
                const DWORD error = Error.load();
                return error == ERROR_SUCCESS ? Result::Success() : LastErrorResult(failure, error);
            }

        private:
            static void CALLBACK OnStatus(HINTERNET, DWORD_PTR context, DWORD status,
                                          void *information, DWORD length) {
                auto *operation = reinterpret_cast<AsyncRequest *>(context);
                if (!operation)
                    return;
                switch (status) {
                case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
                    SetEvent(operation->Closing);
                    return;
                case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
                    operation->Error.store(static_cast<const WINHTTP_ASYNC_RESULT *>(information)->dwError);
                    break;
                case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
                    operation->BytesRead.store(length);
                    break;
                case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
                case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
                    break;
                default:
                    return;
                }
                SetEvent(operation->Completed);
            }
        };

        Result OpenHttpsGetRequest(const std::wstring &url,
                                   Handle &session,
                                   Handle &connect,
                                   AsyncRequest &operation, Clock::time_point deadline) {
            auto &request = operation.Request;
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

            session.value = WinHttpOpen(L"BMLPlusUpdater/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
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

            if (!operation.RegisterCallback())
                return LastErrorResult("Unable to initialize asynchronous HTTP request");
            auto sent = operation.RunUntil(deadline, "Unable to send HTTP request", [&] {
                return WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                          WINHTTP_NO_REQUEST_DATA, 0, 0,
                                          reinterpret_cast<DWORD_PTR>(&operation));
            });
            if (!sent.ok)
                return sent;
            auto received = operation.RunUntil(deadline, "Unable to receive HTTP headers", [&] {
                return WinHttpReceiveResponse(request, nullptr);
            });
            if (!received.ok)
                return received;

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
            const auto deadline = Clock::now() + NetworkTimeout();
            Handle session, connection;
            std::array<char, 64 * 1024> buffer{};
            AsyncRequest request;
            auto opened = OpenHttpsGetRequest(url, session, connection, request, deadline);
            if (!opened.ok)
                return opened;
            uint64_t total = 0;
            for (;;) {
                auto received = request.RunUntil(deadline, "Unable to read HTTP response data", [&] {
                    return WinHttpReadData(request.Request, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr);
                });
                if (!received.ok)
                    return received;
                const DWORD read = request.BytesRead.load();
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

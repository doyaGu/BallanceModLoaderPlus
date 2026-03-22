#ifndef BML_CORE_ERRORS_H
#define BML_CORE_ERRORS_H

#include <exception>
#include <string>
#include <string_view>
#include <utility>

#include "bml_errors.h"
#include "bml_types.h"
#include "DiagnosticManager.h"
#include "KernelServices.h"
#include "Logging.h"

namespace BML::Core {
    namespace detail {
        void SetLastErrorNoThrow(BML_Result code,
                                 const char *message = nullptr,
                                 const char *api_name = nullptr) noexcept;
    }

    /**
     * @brief Internal result structure with message
     */
    struct CoreResult {
        BML_Result code{BML_RESULT_OK};
        std::string message;
    };

    /**
     * @brief Translate current exception to CoreResult
     */
    CoreResult TranslateException(std::string_view subsystem);

    const char *GetErrorString(BML_Result result);

    /**
     * @brief Set the last error for the current thread
     *
     * @param code Result code
     * @param message Error message (copied internally)
     * @param api_name Name of the API that failed (optional)
     * @param source_file Source file name (optional, use __FILE__)
     * @param source_line Source line number (optional, use __LINE__)
     */
    void SetLastError(BML_Result code,
                      const char *message = nullptr,
                      const char *api_name = nullptr,
                      const char *source_file = nullptr,
                      int source_line = 0);

    inline void detail::SetLastErrorNoThrow(BML_Result code,
                                            const char *message,
                                            const char *api_name) noexcept {
        try {
            SetLastError(code, message, api_name, nullptr, 0);
        } catch (const std::exception &ex) {
            CoreLog(BML_LOG_ERROR, "core.errors",
                    "Failed to persist last error (code=%d): %s",
                    static_cast<int>(code), ex.what() ? ex.what() : "std::exception");
        } catch (...) {
            CoreLog(BML_LOG_ERROR, "core.errors",
                    "Failed to persist last error (code=%d): non-standard exception",
                    static_cast<int>(code));
        }
    }

    /**
     * @brief Persist an error and return the same result code
     *
     * @param code Result code
     * @param api_name Name of the API that failed
     * @param message Error message (copied internally)
     * @param detail_code Reserved for platform-specific detail propagation
     * @return The passed-in result code for chaining
     */
    inline BML_Result SetErrorResult(BML_Result code,
                                     const char *api_name,
                                     const char *message,
                                     int detail_code = 0) {
        (void) detail_code;
        SetLastError(code, message, api_name, nullptr, 0);
        return code;
    }

    inline BML_Result SetDefaultLastErrorIfMissing(BML_Result code,
                                                   const char *api_name = nullptr,
                                                   const char *source_file = nullptr,
                                                   int source_line = 0) {
        if (code == BML_RESULT_OK)
            return BML_RESULT_OK;

        if (!Kernel().diagnostics->HasLastError()) {
            SetLastError(code, GetErrorString(code), api_name, source_file, source_line);
        }
        return code;
    }

    /**
     * @brief Get the last error for the current thread
     *
     * @param out_info Pointer to receive error information
     * @return BML_RESULT_OK if error info available, BML_RESULT_NOT_FOUND otherwise
     */
    BML_Result GetLastErrorInfo(BML_ErrorInfo *out_info);

    /**
     * @brief Clear the last error for the current thread
     */
    void ClearLastErrorInfo();

    /**
     * @brief Convert a result code to a string
     *
     * @param result The result code
     * @return Static string describing the error
     */

    /**
     * @brief Macro to set error with file/line info
     */
#define BML_SET_ERROR(code, message) \
    BML::Core::SetLastError((code), (message), nullptr, __FILE__, __LINE__)

#define BML_SET_ERROR_API(code, message, api_name) \
    BML::Core::SetLastError((code), (message), (api_name), __FILE__, __LINE__)

#define BML_RETURN_ERROR(code, message) \
    return BML::Core::SetErrorResult((code), nullptr, (message), 0)

#define BML_RETURN_ERROR_API(code, api_name, message) \
    return BML::Core::SetErrorResult((code), (api_name), (message), 0)

    template <typename Fn>
    BML_Result GuardResult(std::string_view subsystem,
                           const char *api_name,
                           Fn &&fn) noexcept {
        try {
            ClearLastErrorInfo();
            const auto result = static_cast<BML_Result>(std::forward<Fn>(fn)());
            if (result == BML_RESULT_OK) {
                ClearLastErrorInfo();
                return result;
            }
            return SetDefaultLastErrorIfMissing(result, api_name);
        } catch (...) {
            auto result = TranslateException(subsystem);
            detail::SetLastErrorNoThrow(result.code, result.message.c_str(), api_name);
            return result.code;
        }
    }

    template <typename Fn>
    BML_Result GuardResult(std::string_view subsystem, Fn &&fn) noexcept {
        return GuardResult(subsystem, nullptr, std::forward<Fn>(fn));
    }

    template <typename Fn>
    BML_Result GuardRegisteredApiResult(std::string_view subsystem,
                                        const char *api_name,
                                        Fn &&fn) noexcept {
        try {
            ClearLastErrorInfo();
            const auto result = static_cast<BML_Result>(std::forward<Fn>(fn)());
            ClearLastErrorInfo();
            return result;
        } catch (...) {
            auto result = TranslateException(subsystem);
            detail::SetLastErrorNoThrow(result.code, result.message.c_str(), api_name);
            return result.code;
        }
    }

    template <typename Fn>
    BML_Result GuardRegisteredApiResult(std::string_view subsystem, Fn &&fn) noexcept {
        return GuardRegisteredApiResult(subsystem, nullptr, std::forward<Fn>(fn));
    }

    template <typename Fn>
    void GuardVoid(std::string_view subsystem,
                   const char *api_name,
                   Fn &&fn) noexcept {
        try {
            ClearLastErrorInfo();
            std::forward<Fn>(fn)();
            ClearLastErrorInfo();
        } catch (...) {
            auto result = TranslateException(subsystem);
            detail::SetLastErrorNoThrow(result.code, result.message.c_str(), api_name);
        }
    }

    template <typename Fn>
    void GuardVoid(std::string_view subsystem, Fn &&fn) noexcept {
        GuardVoid(subsystem, nullptr, std::forward<Fn>(fn));
    }
} // namespace BML::Core

#endif // BML_CORE_ERRORS_H

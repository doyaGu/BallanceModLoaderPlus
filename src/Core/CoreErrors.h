#ifndef BML_CORE_ERRORS_H
#define BML_CORE_ERRORS_H

#include <exception>
#include <string>
#include <string_view>
#include <utility>

#include "bml_errors.h"
#include "bml_types.h"
#include "Logging.h"

namespace BML::Core {

namespace detail {
void SetLastErrorNoThrow(BML_Result code,
                         const char* message = nullptr,
                         const char* api_name = nullptr) noexcept;
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

/**
 * @brief Set the last error for the current thread (Task 1.1)
 * 
 * @param code Result code
 * @param message Error message (copied internally)
 * @param api_name Name of the API that failed (optional)
 * @param source_file Source file name (optional, use __FILE__)
 * @param source_line Source line number (optional, use __LINE__)
 */
void SetLastError(BML_Result code, 
                  const char* message = nullptr,
                  const char* api_name = nullptr,
                  const char* source_file = nullptr,
                  int source_line = 0);

inline void detail::SetLastErrorNoThrow(BML_Result code,
                                        const char* message,
                                        const char* api_name) noexcept {
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
 * @brief Set error and return the error code (convenience for legacy code)
 * 
 * This overload maintains backward compatibility with older calling patterns
 * that passed (code, domain, api_name, message, detail_code).
 * 
 * Note: Named SetLastErrorAndReturn to avoid conflict with Windows API SetLastErrorEx.
 * 
 * @param code Result code
 * @param domain Subsystem domain (for context, not stored separately)
 * @param api_name Name of the API that failed
 * @param message Error message (copied internally)  
 * @param detail_code Additional error code (e.g., from GetLastError())
 * @return The passed-in result code for chaining
 */
inline BML_Result SetLastErrorAndReturn(BML_Result code,
                                  const char* domain,
                                  const char* api_name,
                                  const char* message,
                                  int detail_code = 0) {
    (void)domain;       // Domain is informational, included in message if needed
    (void)detail_code;  // Could be appended to message in future
    SetLastError(code, message, api_name, nullptr, 0);
    return code;
}

/**
 * @brief Get the last error for the current thread (Task 1.1)
 * 
 * @param out_info Pointer to receive error information
 * @return BML_RESULT_OK if error info available, BML_RESULT_NOT_FOUND otherwise
 */
BML_Result GetLastErrorInfo(BML_ErrorInfo* out_info);

/**
 * @brief Clear the last error for the current thread (Task 1.1)
 */
void ClearLastErrorInfo();

/**
 * @brief Convert a result code to a string (Task 1.1)
 * 
 * @param result The result code
 * @return Static string describing the error
 */
const char* GetErrorString(BML_Result result);

/**
 * @brief Macro to set error with file/line info
 */
#define BML_SET_ERROR(code, message) \
    BML::Core::SetLastError((code), (message), nullptr, __FILE__, __LINE__)

#define BML_SET_ERROR_API(code, message, api_name) \
    BML::Core::SetLastError((code), (message), (api_name), __FILE__, __LINE__)

template <typename Fn>
BML_Result GuardResult(std::string_view subsystem, Fn &&fn) noexcept {
    try {
        return static_cast<BML_Result>(std::forward<Fn>(fn)());
    } catch (...) {
        auto result = TranslateException(subsystem);
        detail::SetLastErrorNoThrow(result.code, result.message.c_str());
        return result.code;
    }
}

template <typename Fn>
void GuardVoid(std::string_view subsystem, Fn &&fn) noexcept {
    try {
        std::forward<Fn>(fn)();
    } catch (...) {
        auto result = TranslateException(subsystem);
        detail::SetLastErrorNoThrow(result.code, result.message.c_str());
    }
}

} // namespace BML::Core

#endif // BML_CORE_ERRORS_H

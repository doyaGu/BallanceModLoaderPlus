#include "CoreErrors.h"

#include <exception>
#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>
#include <cstring>
#include <typeinfo>

#include "Logging.h"

namespace BML::Core {
    namespace {
        /* ========================================================================
         * Thread-Local Error Storage (Task 1.1)
         * ======================================================================== */

        /**
         * @brief Thread-local storage for last error information
         */
        struct ThreadLocalError {
            BML_Result code = BML_RESULT_OK;
            std::string message;
            std::string api_name;
            std::string source_file;
            int source_line = 0;
            bool has_error = false;
        };

        thread_local ThreadLocalError g_lastError;

        std::string_view NormalizeSubsystem(std::string_view subsystem) {
            return subsystem.empty() ? std::string_view{"core"} : subsystem;
        }

        void LogTranslation(std::string_view subsystem, const CoreResult &result) {
            const std::string tag{subsystem};
            const char *message = result.message.empty() ? "Unhandled exception" : result.message.c_str();
            CoreLog(BML_LOG_ERROR, tag.c_str(), "%s (code=%d)", message, static_cast<int>(result.code));
        }

        std::string BuildExceptionMessage(const std::exception &ex) {
            std::string message = ex.what() ? ex.what() : typeid(ex).name();
            try {
                std::rethrow_if_nested(ex);
            } catch (const std::exception &nested) {
                message.append(" -> ").append(BuildExceptionMessage(nested));
            } catch (...) {
                message.append(" -> <non-standard nested>");
            }
            return message;
        }
    } // namespace

    /* ========================================================================
     * Error Handling Implementation (Task 1.1)
     * ======================================================================== */

    void SetLastError(BML_Result code,
                      const char *message,
                      const char *api_name,
                      const char *source_file,
                      int source_line) {
        g_lastError.code = code;
        g_lastError.message = message ? message : "";
        g_lastError.api_name = api_name ? api_name : "";
        g_lastError.source_file = source_file ? source_file : "";
        g_lastError.source_line = source_line;
        g_lastError.has_error = (code != BML_RESULT_OK);
    }

    BML_Result GetLastErrorInfo(BML_ErrorInfo *out_info) {
        if (!out_info) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        if (out_info->struct_size < sizeof(BML_ErrorInfo)) {
            return BML_RESULT_INVALID_SIZE;
        }

        if (!g_lastError.has_error) {
            return BML_RESULT_NOT_FOUND;
        }

        out_info->result_code = g_lastError.code;
        out_info->message = g_lastError.message.empty() ? nullptr : g_lastError.message.c_str();
        out_info->source_file = g_lastError.source_file.empty() ? nullptr : g_lastError.source_file.c_str();
        out_info->source_line = g_lastError.source_line;
        out_info->api_name = g_lastError.api_name.empty() ? nullptr : g_lastError.api_name.c_str();

        return BML_RESULT_OK;
    }

    void ClearLastErrorInfo() {
        g_lastError.has_error = false;
        g_lastError.code = BML_RESULT_OK;
        g_lastError.message.clear();
        g_lastError.api_name.clear();
        g_lastError.source_file.clear();
        g_lastError.source_line = 0;
    }

    const char *GetErrorString(BML_Result result) {
        switch (result) {
        // Generic errors
        case BML_RESULT_OK: return "OK";
        case BML_RESULT_FAIL: return "Generic failure";
        case BML_RESULT_INVALID_ARGUMENT: return "Invalid argument";
        case BML_RESULT_INVALID_STATE: return "Invalid state";
        case BML_RESULT_INVALID_CONTEXT: return "Invalid context";
        case BML_RESULT_NOT_FOUND: return "Not found";
        case BML_RESULT_OUT_OF_MEMORY: return "Out of memory";
        case BML_RESULT_NOT_SUPPORTED: return "Not supported";
        case BML_RESULT_TIMEOUT: return "Timeout";
        case BML_RESULT_WOULD_BLOCK: return "Would block";
        case BML_RESULT_ALREADY_EXISTS: return "Already exists";
        case BML_RESULT_VERSION_MISMATCH: return "Version mismatch";
        case BML_RESULT_PERMISSION_DENIED: return "Permission denied";
        case BML_RESULT_IO_ERROR: return "I/O error";
        case BML_RESULT_UNSUPPORTED: return "Unsupported";
        case BML_RESULT_UNKNOWN_ERROR: return "Unknown error";
        case BML_RESULT_INVALID_SIZE: return "Invalid struct_size";
        case BML_RESULT_BUFFER_TOO_SMALL: return "Buffer too small";
        case BML_RESULT_INVALID_HANDLE: return "Invalid handle";
        case BML_RESULT_NOT_INITIALIZED: return "Not initialized";
        case BML_RESULT_ALREADY_INITIALIZED: return "Already initialized";

        // Config errors
        case BML_RESULT_CONFIG_KEY_NOT_FOUND: return "Config key not found";
        case BML_RESULT_CONFIG_TYPE_MISMATCH: return "Config type mismatch";
        case BML_RESULT_CONFIG_READ_ONLY: return "Config is read-only";
        case BML_RESULT_CONFIG_INVALID_CATEGORY: return "Invalid config category";
        case BML_RESULT_CONFIG_INVALID_NAME: return "Invalid config name";
        case BML_RESULT_CONFIG_VALUE_OUT_OF_RANGE: return "Config value out of range";

        // Extension errors
        case BML_RESULT_EXTENSION_NOT_FOUND: return "Extension not found";
        case BML_RESULT_EXTENSION_VERSION_TOO_OLD: return "Extension version too old";
        case BML_RESULT_EXTENSION_VERSION_TOO_NEW: return "Extension version too new";
        case BML_RESULT_EXTENSION_INCOMPATIBLE: return "Extension incompatible";
        case BML_RESULT_EXTENSION_ALREADY_REGISTERED: return "Extension already registered";
        case BML_RESULT_EXTENSION_INVALID_NAME: return "Invalid extension name";

        // IMC errors
        case BML_RESULT_IMC_QUEUE_FULL: return "IMC queue full";
        case BML_RESULT_IMC_NO_SUBSCRIBERS: return "No subscribers";
        case BML_RESULT_IMC_INVALID_TOPIC: return "Invalid topic";
        case BML_RESULT_IMC_RPC_NOT_REGISTERED: return "RPC not registered";
        case BML_RESULT_IMC_RPC_ALREADY_REGISTERED: return "RPC already registered";
        case BML_RESULT_IMC_FUTURE_CANCELLED: return "Future cancelled";
        case BML_RESULT_IMC_FUTURE_FAILED: return "Future failed";
        case BML_RESULT_IMC_SUBSCRIPTION_CLOSED: return "Subscription closed";

        // Resource errors
        case BML_RESULT_RESOURCE_INVALID_HANDLE: return "Invalid resource handle";
        case BML_RESULT_RESOURCE_HANDLE_EXPIRED: return "Resource handle expired";
        case BML_RESULT_RESOURCE_TYPE_NOT_REGISTERED: return "Resource type not registered";
        case BML_RESULT_RESOURCE_SLOT_EXHAUSTED: return "Resource slots exhausted";

        // Logging errors
        case BML_RESULT_LOG_INVALID_SEVERITY: return "Invalid log severity";
        case BML_RESULT_LOG_SINK_UNAVAILABLE: return "Log sink unavailable";
        case BML_RESULT_LOG_FILTER_REJECTED: return "Log filter rejected";

        // Sync errors
        case BML_RESULT_SYNC_DEADLOCK: return "Deadlock detected";
        case BML_RESULT_SYNC_INVALID_HANDLE: return "Invalid sync handle";
        case BML_RESULT_SYNC_NOT_OWNER: return "Not lock owner";

        default:
            return "Unknown error code";
        }
    }

    CoreResult TranslateException(std::string_view subsystem) {
        CoreResult result{};
        result.code = BML_RESULT_FAIL;

        auto current = std::current_exception();
        if (!current) {
            result.message = "Unknown exception";
            LogTranslation(NormalizeSubsystem(subsystem), result);
            return result;
        }

        try {
            std::rethrow_exception(current);
        } catch (const std::bad_alloc &ex) {
            result.code = BML_RESULT_OUT_OF_MEMORY;
            result.message = BuildExceptionMessage(ex);
        } catch (const std::invalid_argument &ex) {
            result.code = BML_RESULT_INVALID_ARGUMENT;
            result.message = BuildExceptionMessage(ex);
        } catch (const std::out_of_range &ex) {
            result.code = BML_RESULT_INVALID_ARGUMENT;
            result.message = BuildExceptionMessage(ex);
        } catch (const std::domain_error &ex) {
            result.code = BML_RESULT_INVALID_ARGUMENT;
            result.message = BuildExceptionMessage(ex);
        } catch (const std::length_error &ex) {
            result.code = BML_RESULT_INVALID_ARGUMENT;
            result.message = BuildExceptionMessage(ex);
        } catch (const std::overflow_error &ex) {
            result.code = BML_RESULT_INVALID_ARGUMENT;
            result.message = BuildExceptionMessage(ex);
        } catch (const std::underflow_error &ex) {
            result.code = BML_RESULT_INVALID_ARGUMENT;
            result.message = BuildExceptionMessage(ex);
        } catch (const std::range_error &ex) {
            result.code = BML_RESULT_INVALID_ARGUMENT;
            result.message = BuildExceptionMessage(ex);
        } catch (const std::logic_error &ex) {
            result.code = BML_RESULT_INVALID_STATE;
            result.message = BuildExceptionMessage(ex);
        } catch (const std::filesystem::filesystem_error &ex) {
            result.code = BML_RESULT_IO_ERROR;
            std::ostringstream oss;
            oss << ex.what();
            if (!ex.path1().empty())
                oss << " [" << ex.path1().string() << ']';
            if (!ex.path2().empty())
                oss << " [" << ex.path2().string() << ']';
            result.message = oss.str();
            try {
                std::rethrow_if_nested(ex);
            } catch (const std::exception &nested) {
                result.message.append(" -> ").append(BuildExceptionMessage(nested));
            } catch (...) {
                result.message.append(" -> <non-standard nested>");
            }
        } catch (const std::system_error &ex) {
            result.code = BML_RESULT_FAIL;
            std::ostringstream oss;
            oss << ex.what() << " (errno=" << ex.code().value() << ')';
            result.message = oss.str();
            try {
                std::rethrow_if_nested(ex);
            } catch (const std::exception &nested) {
                result.message.append(" -> ").append(BuildExceptionMessage(nested));
            } catch (...) {
                result.message.append(" -> <non-standard nested>");
            }
        } catch (const std::runtime_error &ex) {
            result.code = BML_RESULT_FAIL;
            result.message = BuildExceptionMessage(ex);
        } catch (const std::exception &ex) {
            result.code = BML_RESULT_FAIL;
            result.message = BuildExceptionMessage(ex);
        } catch (...) {
            result.code = BML_RESULT_FAIL;
            result.message = "Unknown exception";
        }

        LogTranslation(NormalizeSubsystem(subsystem), result);
        return result;
    }
} // namespace BML::Core

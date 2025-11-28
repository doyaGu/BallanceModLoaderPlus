#ifndef BML_CORE_DIAGNOSTIC_MANAGER_H
#define BML_CORE_DIAGNOSTIC_MANAGER_H

#include <cstring>
#include <array>

#include "bml_types.h"
#include "bml_errors.h"

namespace BML::Core {
    /**
     * @brief Thread-local error context
     *
     * Stores detailed error information for the current thread.
     * Automatically managed via TLS.
     */
    struct ErrorContext {
        BML_ErrorInfo info{};
        std::array<char, 256> message_buffer{};
        std::array<char, 128> api_name_buffer{};
        std::array<char, 256> source_file_buffer{};
        bool has_error{false};

        void Clear() {
            has_error = false;
            info = BML_ErrorInfo{};
            info.struct_size = sizeof(BML_ErrorInfo);
            message_buffer.fill(0);
            api_name_buffer.fill(0);
            source_file_buffer.fill(0);
        }

        void SetError(BML_Result code,
                      const char *message,
                      const char *api_name = nullptr,
                      const char *source_file = nullptr,
                      int source_line = 0) {
            has_error = true;
            info.struct_size = sizeof(BML_ErrorInfo);
            info.result_code = code;
            info.source_line = source_line;

            // Copy strings to thread-local buffers
            if (message) {
                strncpy(message_buffer.data(), message, message_buffer.size() - 1);
                info.message = message_buffer.data();
            } else {
                info.message = nullptr;
            }

            if (api_name) {
                strncpy(api_name_buffer.data(), api_name, api_name_buffer.size() - 1);
                info.api_name = api_name_buffer.data();
            } else {
                info.api_name = nullptr;
            }

            if (source_file) {
                strncpy(source_file_buffer.data(), source_file, source_file_buffer.size() - 1);
                info.source_file = source_file_buffer.data();
            } else {
                info.source_file = nullptr;
            }
        }
    };

    /**
     * @brief Diagnostic manager for error tracking
     *
     * Manages thread-local error contexts and provides error query APIs.
     */
    class DiagnosticManager {
    public:
        static DiagnosticManager &Instance();

        // Error context management
        BML_Result GetLastError(BML_ErrorInfo *out_error);
        void ClearLastError();

        // Set error (internal use)
        void SetError(BML_Result code,
                      const char *message,
                      const char *api_name = nullptr,
                      const char *source_file = nullptr,
                      int source_line = 0);

    private:
        DiagnosticManager() = default;
        ~DiagnosticManager() = default;

        DiagnosticManager(const DiagnosticManager &) = delete;
        DiagnosticManager &operator=(const DiagnosticManager &) = delete;

        // Get thread-local error context
        ErrorContext &GetThreadContext();
    };

    // Helper function for setting errors (simplified signature)
    inline BML_Result SetLastErrorDiag(BML_Result code,
                                       const char *message,
                                       const char *api_name = nullptr) {
        DiagnosticManager::Instance().SetError(code, message, api_name, nullptr, 0);
        return code;
    }
} // namespace BML::Core

#endif // BML_CORE_DIAGNOSTIC_MANAGER_H

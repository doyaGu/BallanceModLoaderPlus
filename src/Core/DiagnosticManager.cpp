#include "DiagnosticManager.h"

namespace BML::Core {
    // Thread-local error context
    static thread_local ErrorContext g_ThreadErrorContext;

    DiagnosticManager &DiagnosticManager::Instance() {
        static DiagnosticManager instance;
        return instance;
    }

    ErrorContext &DiagnosticManager::GetThreadContext() {
        return g_ThreadErrorContext;
    }

    BML_Result DiagnosticManager::GetLastError(BML_ErrorInfo *out_error) {
        if (!out_error) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        if (out_error->struct_size < sizeof(BML_ErrorInfo)) {
            return BML_RESULT_INVALID_SIZE;
        }

        auto &ctx = GetThreadContext();
        if (!ctx.has_error) {
            return BML_RESULT_NOT_FOUND;
        }

        *out_error = ctx.info;
        return BML_RESULT_OK;
    }

    void DiagnosticManager::ClearLastError() {
        auto &ctx = GetThreadContext();
        ctx.Clear();
    }

    void DiagnosticManager::SetError(BML_Result code,
                                     const char *message,
                                     const char *api_name,
                                     const char *source_file,
                                     int source_line) {
        auto &ctx = GetThreadContext();
        ctx.SetError(code, message, api_name, source_file, source_line);
    }
} // namespace BML::Core

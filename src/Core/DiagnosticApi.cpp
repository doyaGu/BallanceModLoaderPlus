#include "ApiRegistration.h"
#include "ApiRegistry.h"
#include "ApiRegistrationMacros.h"
#include "CoreErrors.h"
#include "DiagnosticManager.h"

#include "bml_errors.h"
#include "bml_api_ids.h"
#include "bml_capabilities.h"

namespace BML::Core {
    BML_Result BML_API_GetLastError(BML_ErrorInfo* out_error) {
        return DiagnosticManager::Instance().GetLastError(out_error);
    }

    void BML_API_ClearLastError() {
        DiagnosticManager::Instance().ClearLastError();
    }

    void RegisterDiagnosticApis() {
        BML_BEGIN_API_REGISTRATION();

        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlGetLastError, "diagnostics", BML_API_GetLastError, BML_CAP_DIAGNOSTICS);
        BML_REGISTER_API_WITH_CAPS(bmlClearLastError, BML_API_ClearLastError, BML_CAP_DIAGNOSTICS);
        BML_REGISTER_API_WITH_CAPS(bmlGetErrorString, GetErrorString, BML_CAP_DIAGNOSTICS);
    }
} // namespace BML::Core

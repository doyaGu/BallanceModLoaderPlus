#include "ApiRegistration.h"
#include "ApiRegistry.h"
#include "ApiRegistrationMacros.h"
#include "CoreErrors.h"
#include "DiagnosticManager.h"

#include "bml_errors.h"

namespace BML::Core {
    BML_Result BML_API_GetLastError(BML_ErrorInfo* out_error) {
        return DiagnosticManager::Instance().GetLastError(out_error);
    }

    void BML_API_ClearLastError() {
        DiagnosticManager::Instance().ClearLastError();
    }

    void RegisterDiagnosticApis() {
        BML_BEGIN_API_REGISTRATION();

        BML_REGISTER_API(bmlGetLastError, BML_API_GetLastError);
        BML_REGISTER_API(bmlClearLastError, BML_API_ClearLastError);
        BML_REGISTER_API(bmlGetErrorString, GetErrorString);
    }
} // namespace BML::Core

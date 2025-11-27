#include "ApiRegistration.h"
#include "ApiRegistry.h"
#include "ApiRegistrationMacros.h"
#include "CoreErrors.h"

#include "bml_errors.h"
#include "bml_api_ids.h"
#include "bml_capabilities.h"

namespace BML::Core {

// Forward declare C API functions
extern "C" {
    BML_Result BML_API_GetLastError(BML_ErrorInfo* out_error);
    void BML_API_ClearLastError();
}

// ============================================================================
// API Registration
// ============================================================================

void RegisterDiagnosticApis() {
    BML_BEGIN_API_REGISTRATION();
    
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlGetLastError, "diagnostics", BML_API_GetLastError, BML_CAP_DIAGNOSTICS);
    BML_REGISTER_API_WITH_CAPS(bmlClearLastError, BML_API_ClearLastError, BML_CAP_DIAGNOSTICS);
}

} // namespace BML::Core

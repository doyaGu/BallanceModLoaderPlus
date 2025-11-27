#include "ApiRegistration.h"
#include "ApiRegistry.h"
#include "ApiRegistrationMacros.h"
#include "CoreErrors.h"

#include "bml_errors.h"
#include "bml_api_ids.h"

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
    auto& registry = ApiRegistry::Instance();
    
    BML_REGISTER_API_GUARDED(bmlGetLastError, "diagnostics", BML_API_GetLastError);
    BML_REGISTER_API(bmlClearLastError, BML_API_ClearLastError);
}

} // namespace BML::Core

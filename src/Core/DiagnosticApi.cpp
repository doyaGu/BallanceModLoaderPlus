#include "ApiRegistrationMacros.h"
#include "CoreErrors.h"

namespace BML::Core {
    void RegisterDiagnosticApis(ApiRegistry &apiRegistry) {
        BML_BEGIN_API_REGISTRATION(apiRegistry);

        // Error strings are value-only helpers and remain safe as raw lookup.
        BML_REGISTER_API(bmlGetErrorString, GetErrorString);
    }
} // namespace BML::Core

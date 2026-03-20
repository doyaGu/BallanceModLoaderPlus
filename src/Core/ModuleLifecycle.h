#ifndef BML_CORE_MODULE_LIFECYCLE_H
#define BML_CORE_MODULE_LIFECYCLE_H

#include <string>

#include "bml_errors.h"
#include "bml_export.h"

#include "ModHandle.h"
#include "PlatformCompat.h"

namespace BML::Core {
    using ModuleEntrypoint = int (*)(BML_ModEntrypointCommand command, void *args);

    BML_Result PrepareModuleForDetach(const std::string &module_id,
                                      BML_Mod mod,
                                      ModuleEntrypoint entrypoint,
                                      std::string *out_diagnostic);
    void CancelModuleDetachPreparation(const std::string &module_id);
    void CleanupModuleKernelState(const std::string &module_id, BML_Mod mod);
} // namespace BML::Core

#endif /* BML_CORE_MODULE_LIFECYCLE_H */

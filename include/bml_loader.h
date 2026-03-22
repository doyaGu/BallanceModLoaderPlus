#ifndef BML_LOADER_H
#define BML_LOADER_H

#include "bml_types.h"
#include "bml_export.h"
#include "bml_errors.h"
#include "bml_config.h"
#include "bml_core.h"
#include "bml_imc.h"
#include "bml_logging.h"
#include "bml_resource.h"
#include "bml_memory.h"
#include "bml_interface.h"
#include "bml_sync.h"
#include "bml_profiling.h"
#include "bml_builtin_interfaces.h"

BML_BEGIN_CDECLS

/*
 * Header-only bootstrap loader (GLAD-style).
 *
 *   In ONE .cpp file (typically your main .cpp):
 *     #define BML_LOADER_IMPLEMENTATION
 *     #include <bml_loader.h>
 *
 *   In other .cpp files that need the bootstrap globals:
 *     #include <bml_loader.h>  // Gets extern declarations
 *
 * bmlLoadAPI populates only the bootstrap minimum:
 *   - bmlGetProcAddress (validated through the callback argument)
 *   - bmlInterfaceAcquire
 *   - bmlInterfaceRelease
 *
 * Optional bootstrap-adjacent helpers may also be loaded when present,
 * but they are not required for successful bootstrap:
 *   - bmlInterfaceAcquire
 *   - bmlGetHostModule
 *
 * All other runtime APIs should be resolved through acquired builtin
 * interfaces or module-local get_proc calls.
 */

BML_Result bmlLoadAPI(PFN_BML_GetProcAddress get_proc);
void       bmlUnloadAPI(void);
BML_Bool   bmlIsApiLoaded(void);
size_t     bmlGetApiCount(void);
size_t     bmlGetRequiredApiCount(void);

#define BML_BOOTSTRAP_LOAD(get_proc) bmlLoadAPI((get_proc))

/* ========================================================================
 * Bootstrap Global Pointer Declarations
 * ======================================================================== */

#ifdef BML_LOADER_IMPLEMENTATION

/* Definitions */
PFN_BML_InterfaceAcquire bmlInterfaceAcquire = NULL;
PFN_BML_InterfaceRelease bmlInterfaceRelease = NULL;
PFN_BML_GetHostModule bmlGetHostModule = NULL;

static void bmlResetApiPointers(void) {
    bmlInterfaceAcquire = NULL;
    bmlInterfaceRelease = NULL;
    bmlGetHostModule = NULL;
}

BML_Result bmlLoadAPI(PFN_BML_GetProcAddress get_proc) {
    if (!get_proc)
        return BML_RESULT_INVALID_ARGUMENT;

    bmlUnloadAPI();

    if (!get_proc("bmlGetProcAddress"))
        return BML_RESULT_NOT_FOUND;

    bmlInterfaceAcquire = (PFN_BML_InterfaceAcquire) get_proc("bmlInterfaceAcquire");
    bmlInterfaceRelease = (PFN_BML_InterfaceRelease) get_proc("bmlInterfaceRelease");
    bmlGetHostModule = (PFN_BML_GetHostModule) get_proc("bmlGetHostModule");

    if (!bmlInterfaceAcquire || !bmlInterfaceRelease) {
        bmlUnloadAPI();
        return BML_RESULT_NOT_FOUND;
    }

    return BML_RESULT_OK;
}

void bmlUnloadAPI(void) {
    bmlResetApiPointers();
}

BML_Bool bmlIsApiLoaded(void) {
    return (bmlInterfaceAcquire != NULL) ? BML_TRUE : BML_FALSE;
}

size_t bmlGetApiCount(void) {
    return 4;
}

size_t bmlGetRequiredApiCount(void) {
    return 2;
}

#else /* !BML_LOADER_IMPLEMENTATION */

/* Extern declarations for non-implementation translation units */
extern PFN_BML_InterfaceAcquire bmlInterfaceAcquire;
extern PFN_BML_InterfaceRelease bmlInterfaceRelease;
extern PFN_BML_GetHostModule bmlGetHostModule;

#endif /* BML_LOADER_IMPLEMENTATION */

BML_END_CDECLS

#endif /* BML_LOADER_H */

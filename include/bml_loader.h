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
 * bmlLoadAPI populates only the host bootstrap minimum:
 *   - bmlGetProcAddress (validated through the callback argument)
 *   - bmlInterfaceRegister
 *   - bmlInterfaceAcquire
 *   - bmlInterfaceRelease
 *   - bmlInterfaceUnregister
 *
 * Module code must not use proc lookup for runtime behavior. Modules receive
 * `BML_Services` through attach args and can bind the interface globals
 * from that aggregate via bmlBindServices().
 */

BML_Result bmlLoadAPI(PFN_BML_GetProcAddress get_proc);
void       bmlBindServices(const BML_Services *services);
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
PFN_BML_InterfaceRegister bmlInterfaceRegister = NULL;
PFN_BML_InterfaceAcquire bmlInterfaceAcquire = NULL;
PFN_BML_InterfaceRelease bmlInterfaceRelease = NULL;
PFN_BML_InterfaceUnregister bmlInterfaceUnregister = NULL;

static void bmlResetApiPointers(void) {
    bmlInterfaceRegister = NULL;
    bmlInterfaceAcquire = NULL;
    bmlInterfaceRelease = NULL;
    bmlInterfaceUnregister = NULL;
}

BML_Result bmlLoadAPI(PFN_BML_GetProcAddress get_proc) {
    if (!get_proc)
        return BML_RESULT_INVALID_ARGUMENT;

    bmlUnloadAPI();

    if (!get_proc("bmlGetProcAddress"))
        return BML_RESULT_NOT_FOUND;

    bmlInterfaceRegister = (PFN_BML_InterfaceRegister) get_proc("bmlInterfaceRegister");
    bmlInterfaceAcquire = (PFN_BML_InterfaceAcquire) get_proc("bmlInterfaceAcquire");
    bmlInterfaceRelease = (PFN_BML_InterfaceRelease) get_proc("bmlInterfaceRelease");
    bmlInterfaceUnregister = (PFN_BML_InterfaceUnregister) get_proc("bmlInterfaceUnregister");

    if (!bmlInterfaceRegister || !bmlInterfaceAcquire ||
        !bmlInterfaceRelease || !bmlInterfaceUnregister) {
        bmlUnloadAPI();
        return BML_RESULT_NOT_FOUND;
    }

    return BML_RESULT_OK;
}

void bmlBindServices(const BML_Services *services) {
    if (!services || !services->InterfaceControl) {
        return;
    }

    bmlInterfaceRegister = services->InterfaceControl->Register;
    bmlInterfaceAcquire = services->InterfaceControl->Acquire;
    bmlInterfaceRelease = services->InterfaceControl->Release;
    bmlInterfaceUnregister = services->InterfaceControl->Unregister;
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
    return 4;
}

#else /* !BML_LOADER_IMPLEMENTATION */

/* Extern declarations for non-implementation translation units */
extern PFN_BML_InterfaceRegister bmlInterfaceRegister;
extern PFN_BML_InterfaceAcquire bmlInterfaceAcquire;
extern PFN_BML_InterfaceRelease bmlInterfaceRelease;
extern PFN_BML_InterfaceUnregister bmlInterfaceUnregister;

#endif /* BML_LOADER_IMPLEMENTATION */

BML_END_CDECLS

#endif /* BML_LOADER_H */

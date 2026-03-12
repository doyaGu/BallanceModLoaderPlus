#ifndef BML_LOADER_H
#define BML_LOADER_H

#include "bml_types.h"
#include "bml_export.h"
#include "bml_errors.h"   /* Includes diagnostics */
#include "bml_config.h"
#include "bml_core.h"
#include "bml_extension.h"
#include "bml_imc.h"
#include "bml_logging.h"
#include "bml_resource.h"
#include "bml_memory.h"
#include "bml_sync.h"
#include "bml_profiling.h"
#include "bml_api_tracing.h"
#include "bml_capabilities.h"

BML_BEGIN_CDECLS

/*
 * Usage (header-only, GLAD-style):
 *
 *   In ONE .cpp file (typically your main .cpp):
 *     #define BML_LOADER_IMPLEMENTATION
 *     #include <bml_loader.h>
 *   
 *   In other .cpp files that need to call BML APIs:
 *     #include <bml_loader.h>  // Gets extern declarations
 */

/*
 * Call bmlLoadAPI at the very beginning of your mod entry point to populate
 * the global function pointers. Name-based lookup is mandatory because mods
 * may need to query dynamically appended APIs; ID-based lookup is an optional
 * fast path for the built-in stable API surface.
 */
BML_Result bmlLoadAPI(PFN_BML_GetProcAddress get_proc,
					  PFN_BML_GetProcAddressById get_proc_by_id);

/*
 * Reset every global function pointer back to nullptr. Call this during mod
 * shutdown.
 */
void bmlUnloadAPI(void);

/* Query whether the API is currently loaded (defensive check). */
BML_Bool bmlIsApiLoaded(void);

/* Get total number of APIs in the loader. */
size_t bmlGetApiCount(void);

/* Get number of required APIs that must be present for loading to succeed. */
size_t bmlGetRequiredApiCount(void);

#include "bml_loader_autogen.h"

BML_END_CDECLS

#endif /* BML_LOADER_H */

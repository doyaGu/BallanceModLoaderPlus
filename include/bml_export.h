#ifndef BML_EXPORT_H
#define BML_EXPORT_H

#include "bml_types.h"
#include "bml_errors.h"   /* Includes diagnostics */
#include "bml_bootstrap.h"
#include "bml_version.h"

BML_BEGIN_CDECLS

#ifndef BML_API
#   ifdef _WIN32
#       ifdef BML_BUILD_DLL
#           define BML_API __declspec(dllexport)
#       else
#           define BML_API __declspec(dllimport)
#       endif
#   else
#       define BML_API __attribute__((visibility("default")))
#   endif
#endif

typedef void *(*PFN_BML_GetProcAddress)(const char *proc_name);

typedef enum BML_ModEntrypointCommand {
	BML_MOD_ENTRYPOINT_ATTACH = 1,
	BML_MOD_ENTRYPOINT_PREPARE_DETACH = 2,
	BML_MOD_ENTRYPOINT_DETACH = 3
} BML_ModEntrypointCommand;

#define BML_MOD_ENTRYPOINT_API_VERSION 6u

typedef struct BML_ModAttachArgs {
	uint32_t struct_size;
	uint32_t api_version;
	BML_Mod mod;
	PFN_BML_GetProcAddress get_proc;
} BML_ModAttachArgs;

typedef struct BML_ModDetachArgs {
	uint32_t struct_size;
	uint32_t api_version;
	BML_Mod mod;
} BML_ModDetachArgs;

typedef BML_Result (*PFN_BML_ModEntrypoint)(BML_ModEntrypointCommand command, void *command_args);

/* Runtime lifecycle entry points (Core exports) */
BML_API BML_Result bmlAttach(void);           /**< Initialize BML Core only */
BML_API BML_Result bmlDiscoverModules(void);  /**< Scan and validate mods (Phase 1) */
BML_API BML_Result bmlLoadModules(void);      /**< Load discovered mods (Phase 2) */
BML_API BML_Result bmlBootstrap(const BML_BootstrapConfig *config);
BML_API void bmlUpdate(void);                 /**< Drive deferred runtime work (main loop) */
BML_API void bmlDetach(void);
BML_API void bmlShutdown(void);
BML_API BML_BootstrapState bmlGetBootstrapState(void);

/* API lookup */
BML_API void *bmlGetProcAddress(const char *proc_name);

/* Thread-local diagnostics snapshot; valid until the next call on the same thread. */
BML_API const BML_BootstrapDiagnostics *bmlGetBootstrapDiagnostics(void);

/* 
 * Mod entry points (Implemented by modules - DO NOT import)
 * 
 * When compiling a module, these should be defined with BML_MODULE_ENTRY macro.
 * Do NOT declare or use BML_API here as it would cause import/export conflicts.
 */

BML_END_CDECLS

#endif /* BML_EXPORT_H */

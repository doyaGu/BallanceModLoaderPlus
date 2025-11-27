#ifndef BML_EXPORT_H
#define BML_EXPORT_H

#include "bml_types.h"
#include "bml_errors.h"   /* Includes diagnostics */
#include "bml_version.h"
#include "bml_api_ids.h"

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

/**
 * @brief 32-bit API identifier for fast lookup
 * 
 * CRITICAL: IDs are explicitly assigned and PERMANENT (like syscall numbers).
 * Once assigned, an ID never changes across BML versions, ensuring binary compatibility.
 * 
 * All API IDs are defined in bml_api_ids.h and must remain stable.
 * Using integer IDs instead of string lookups provides:
 * - ~3-5x faster lookup (direct integer map access)
 * - Better cache locality
 * - Zero allocation overhead
 * - Guaranteed stability across versions
 */
typedef uint32_t BML_ApiId;

typedef void *(*PFN_BML_GetProcAddress)(const char *proc_name);
typedef void *(*PFN_BML_GetProcAddressById)(BML_ApiId api_id);
typedef int (*PFN_BML_GetApiId)(const char *proc_name, BML_ApiId *out_id);

typedef enum BML_ModEntrypointCommand {
	BML_MOD_ENTRYPOINT_ATTACH = 1,
	BML_MOD_ENTRYPOINT_DETACH = 2
} BML_ModEntrypointCommand;

#define BML_MOD_ENTRYPOINT_API_VERSION 1u

typedef struct BML_ModAttachArgs {
	uint32_t struct_size;
	uint32_t api_version;
	BML_Mod mod;
	PFN_BML_GetProcAddress get_proc;
	PFN_BML_GetProcAddressById get_proc_by_id;  /* Fast path (v0.4.1+) */
	PFN_BML_GetApiId get_api_id;                /* Get ID from name (v0.4.1+) */
	void *reserved;
} BML_ModAttachArgs;

typedef struct BML_ModDetachArgs {
	uint32_t struct_size;
	uint32_t api_version;
	BML_Mod mod;
	void *reserved;
} BML_ModDetachArgs;

typedef BML_Result (*PFN_BML_ModEntrypoint)(BML_ModEntrypointCommand command, void *command_args);

/* Runtime lifecycle entry points (Core exports) */
BML_API BML_Result bmlAttach(void);
BML_API BML_Result bmlLoadModules(void);
BML_API void bmlDetach(void);

/* API lookup - String-based (legacy, compatible) */
BML_API void *bmlGetProcAddress(const char *proc_name);

/* API lookup - ID-based (fast path, v0.4.1+) */
/**
 * @brief Get API pointer using pre-computed ID (fast path)
 * 
 * Performance: ~3-5x faster than bmlGetProcAddress()
 * 
 * @param api_id Pre-computed API ID (from compile-time macro or runtime query)
 * @return API function pointer, or NULL if not registered
 * 
 * @code
 * // C: Runtime query
 * BML_ApiId id;
 * if (bmlGetApiId("bmlImcPublish", &id)) {
 *     PFN_BML_ImcPublish publish = (PFN_BML_ImcPublish)bmlGetProcAddressById(id);
 * }
 * @endcode
 */
BML_API void *bmlGetProcAddressById(BML_ApiId api_id);

/**
 * @brief Get API ID for registered API name
 * 
 * @param proc_name API function name (e.g., "bmlImcPublish")
 * @param out_id Receives the 32-bit API ID
 * @return 1 if found, 0 if not registered
 */
BML_API int bmlGetApiId(const char *proc_name, BML_ApiId *out_id);

BML_API const BML_BootstrapDiagnostics *bmlGetBootstrapDiagnostics(void);

/* 
 * Mod entry points (Implemented by modules - DO NOT import)
 * 
 * When compiling a module, these should be defined with BML_MODULE_ENTRY macro.
 * Do NOT declare or use BML_API here as it would cause import/export conflicts.
 */

BML_END_CDECLS

#endif /* BML_EXPORT_H */

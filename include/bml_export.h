/**
 * @file bml_export.h
 * @brief Module entry points, DLL export macros, and service bundle
 *
 * Defines the BML_Services struct (injected into modules at attach time),
 * the module entrypoint signature (BML_MODULE_ENTRY), and the runtime
 * lifecycle API (bmlRuntimeCreate/Destroy/Update).
 *
 * Interface struct types are forward-declared here to avoid circular
 * includes. Full definitions are in their respective subsystem headers
 * (bml_core.h, bml_imc.h, bml_config.h, etc.).
 */

#ifndef BML_EXPORT_H
#define BML_EXPORT_H

#include "bml_types.h"
#include "bml_errors.h"   /* Includes diagnostics */
#include "bml_bootstrap.h"
#include "bml_version.h"

BML_BEGIN_CDECLS

/* ======== Forward Declarations ======== */

typedef struct BML_CoreContextInterface BML_CoreContextInterface;
typedef struct BML_CoreLoggingInterface BML_CoreLoggingInterface;
typedef struct BML_CoreModuleInterface BML_CoreModuleInterface;
typedef struct BML_CoreConfigInterface BML_CoreConfigInterface;
typedef struct BML_CoreMemoryInterface BML_CoreMemoryInterface;
typedef struct BML_CoreResourceInterface BML_CoreResourceInterface;
typedef struct BML_CoreDiagnosticInterface BML_CoreDiagnosticInterface;
typedef struct BML_CoreInterfaceControlInterface BML_CoreInterfaceControlInterface;
typedef struct BML_ImcBusInterface BML_ImcBusInterface;
typedef struct BML_ImcRpcInterface BML_ImcRpcInterface;
typedef struct BML_CoreTimerInterface BML_CoreTimerInterface;
typedef struct BML_CoreLocaleInterface BML_CoreLocaleInterface;
typedef struct BML_CoreHookRegistryInterface BML_CoreHookRegistryInterface;
typedef struct BML_CoreSyncInterface BML_CoreSyncInterface;
typedef struct BML_CoreProfilingInterface BML_CoreProfilingInterface;
typedef struct BML_HostRuntimeInterface BML_HostRuntimeInterface;

/* ======== Service Bundle ======== */

/**
 * @brief Service bundle injected into modules at attach time.
 *
 * Extensibility rules:
 *   - New interface pointers MUST be appended at the end of this struct.
 *   - Consumers should check BML_ModAttachArgs.api_version to determine
 *     which fields are available.
 *   - Any pointer may be NULL if the corresponding subsystem is unavailable.
 */
typedef struct BML_Services {
    const BML_CoreContextInterface *Context;
    const BML_CoreLoggingInterface *Logging;
    const BML_CoreModuleInterface *Module;
    const BML_CoreConfigInterface *Config;
    const BML_CoreMemoryInterface *Memory;
    const BML_CoreResourceInterface *Resource;
    const BML_CoreDiagnosticInterface *Diagnostic;
    const BML_CoreInterfaceControlInterface *InterfaceControl;
    const BML_ImcBusInterface *ImcBus;
    const BML_ImcRpcInterface *ImcRpc;
    const BML_CoreTimerInterface *Timer;
    const BML_CoreLocaleInterface *Locale;
    const BML_CoreHookRegistryInterface *HookRegistry;
    const BML_CoreSyncInterface *Sync;
    const BML_CoreProfilingInterface *Profiling;
    const BML_HostRuntimeInterface *HostRuntime;
} BML_Services;

/* ======== DLL Export ======== */

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

/* ======== Module Entry Points ======== */

typedef enum BML_ModEntrypointCommand {
	BML_MOD_ENTRYPOINT_ATTACH = 1,
	BML_MOD_ENTRYPOINT_PREPARE_DETACH = 2,
	BML_MOD_ENTRYPOINT_DETACH = 3,
	/** Called instead of PREPARE_DETACH when the module is about to be reloaded.
	 *  Old modules return BML_RESULT_INVALID_ARGUMENT (unknown command),
	 *  which the runtime treats as "fall back to PREPARE_DETACH". */
	BML_MOD_ENTRYPOINT_PREPARE_RELOAD = 4
} BML_ModEntrypointCommand;

#define BML_MOD_ENTRYPOINT_API_VERSION 1u

typedef struct BML_ModAttachArgs {
	uint32_t struct_size;
	uint32_t api_version;
	BML_Context context;
	BML_Mod mod;
	const BML_Services *services;
	/* V2 extension — only accessible when struct_size is large enough.
	 * BML_TRUE when this attach follows a hot-reload (not the first load). */
	BML_Bool is_reload;
} BML_ModAttachArgs;

/** Args for BML_MOD_ENTRYPOINT_PREPARE_RELOAD. */
typedef struct BML_ModPrepareReloadArgs {
	uint32_t struct_size;
	uint32_t api_version;
	BML_Mod mod;
} BML_ModPrepareReloadArgs;

typedef struct BML_ModDetachArgs {
	uint32_t struct_size;
	uint32_t api_version;
	BML_Mod mod;
} BML_ModDetachArgs;

typedef BML_Result (*PFN_BML_ModEntrypoint)(BML_ModEntrypointCommand command, void *command_args);

/* ======== Runtime Lifecycle API ======== */

/* API lookup */
BML_API void *bmlGetProcAddress(const char *proc_name);
BML_API BML_Result bmlRuntimeCreate(const BML_RuntimeConfig *config, BML_Runtime *out_runtime);
BML_API BML_Result bmlRuntimeDiscoverModules(BML_Runtime runtime);
BML_API BML_Result bmlRuntimeLoadModules(BML_Runtime runtime);
BML_API void bmlRuntimeUpdate(BML_Runtime runtime);
BML_API const BML_Services *bmlRuntimeGetServices(BML_Runtime runtime);
BML_API const BML_BootstrapDiagnostics *bmlRuntimeGetBootstrapDiagnostics(BML_Runtime runtime);
BML_API void bmlRuntimeDestroy(BML_Runtime runtime);

/* 
 * Mod entry points (Implemented by modules - DO NOT import)
 * 
 * When compiling a module, these should be defined with BML_MODULE_ENTRY macro.
 * Do NOT declare or use BML_API here as it would cause import/export conflicts.
 */

BML_END_CDECLS

#endif /* BML_EXPORT_H */

#ifndef BML_BUILTIN_INTERFACES_H
#define BML_BUILTIN_INTERFACES_H

#include "bml_config.h"
#include "bml_core.h"
#include "bml_errors.h"
#include "bml_hook.h"
#include "bml_imc.h"
#include "bml_interface.h"
#include "bml_locale.h"
#include "bml_logging.h"
#include "bml_memory.h"
#include "bml_profiling.h"
#include "bml_resource.h"
#include "bml_sync.h"
#include "bml_timer.h"

BML_BEGIN_CDECLS

/* ========================================================================
 * Interface ID Constants
 * ======================================================================== */

#define BML_CORE_CONTEXT_INTERFACE_ID       "bml.core.context"
#define BML_CORE_MODULE_INTERFACE_ID        "bml.core.module"
#define BML_CORE_LOGGING_INTERFACE_ID       "bml.core.logging"
#define BML_CORE_CONFIG_INTERFACE_ID        "bml.core.config"
#define BML_CORE_MEMORY_INTERFACE_ID        "bml.core.memory"
#define BML_CORE_RESOURCE_INTERFACE_ID      "bml.core.resource"
#define BML_CORE_DIAGNOSTIC_INTERFACE_ID    "bml.core.diagnostic"
#define BML_IMC_BUS_INTERFACE_ID            "bml.imc.bus"
#define BML_CORE_TIMER_INTERFACE_ID         "bml.core.timer"
#define BML_CORE_HOOK_REGISTRY_INTERFACE_ID "bml.core.hook_registry"
#define BML_CORE_LOCALE_INTERFACE_ID        "bml.core.locale"
#define BML_CORE_SYNC_INTERFACE_ID          "bml.core.sync"
#define BML_CORE_PROFILING_INTERFACE_ID     "bml.core.profiling"
#define BML_CORE_HOST_RUNTIME_INTERFACE_ID  "bml.core.host_runtime"

/* ========================================================================
 * Context -- global handle, user data, version query
 * ======================================================================== */

typedef struct BML_CoreContextInterface {
    BML_InterfaceHeader header;
    PFN_BML_GetGlobalContext GetGlobalContext;
    PFN_BML_GetRuntimeVersion GetRuntimeVersion;
    PFN_BML_ContextRetain Retain;
    PFN_BML_ContextRelease Release;
    PFN_BML_ContextSetUserData SetUserData;
    PFN_BML_ContextGetUserData GetUserData;
} BML_CoreContextInterface;

/* ========================================================================
 * Module -- identity, capabilities, lifecycle, manifest
 * ======================================================================== */

typedef struct BML_CoreModuleInterface {
    BML_InterfaceHeader header;
    /* Identity */
    PFN_BML_GetModId GetModId;
    PFN_BML_GetModVersion GetModVersion;
    /* Capabilities */
    PFN_BML_RequestCapability RequestCapability;
    PFN_BML_CheckCapability CheckCapability;
    /* Thread-local binding */
    PFN_BML_SetCurrentModule SetCurrentModule;
    PFN_BML_GetCurrentModule GetCurrentModule;
    /* Lifecycle */
    PFN_BML_RegisterShutdownHook RegisterShutdownHook;
    /* Enumeration */
    PFN_BML_GetLoadedModuleCount GetLoadedModuleCount;
    PFN_BML_GetLoadedModuleAt GetLoadedModuleAt;
    /* Manifest custom fields */
    PFN_BML_GetManifestString GetManifestString;
    PFN_BML_GetManifestInt GetManifestInt;
    PFN_BML_GetManifestFloat GetManifestFloat;
    PFN_BML_GetManifestBool GetManifestBool;
    /* Extended metadata (v1.1) */
    PFN_BML_GetModDirectory GetModDirectory;
    PFN_BML_GetModName GetModName;
    PFN_BML_GetModDescription GetModDescription;
    PFN_BML_GetModAuthorCount GetModAuthorCount;
    PFN_BML_GetModAuthorAt GetModAuthorAt;
    /* Lookup (v1.2) */
    PFN_BML_FindModuleById FindModuleById;
} BML_CoreModuleInterface;

/* ========================================================================
 * Logging -- log, filter, sink override
 * ======================================================================== */

typedef struct BML_CoreLoggingInterface {
    BML_InterfaceHeader header;
    PFN_BML_Log Log;
    PFN_BML_LogVa LogVa;
    PFN_BML_SetLogFilter SetLogFilter;
    PFN_BML_RegisterLogSinkOverride RegisterSinkOverride;
    PFN_BML_ClearLogSinkOverride ClearSinkOverride;
} BML_CoreLoggingInterface;

/* ========================================================================
 * Config -- get/set, batch, migration hooks
 * ======================================================================== */

typedef struct BML_CoreConfigInterface {
    BML_InterfaceHeader header;
    /* Single-key operations */
    PFN_BML_ConfigGet Get;
    PFN_BML_ConfigSet Set;
    PFN_BML_ConfigReset Reset;
    PFN_BML_ConfigEnumerate Enumerate;
    /* Batch (atomic multi-key) */
    PFN_BML_ConfigBatchBegin BatchBegin;
    PFN_BML_ConfigBatchSet BatchSet;
    PFN_BML_ConfigBatchCommit BatchCommit;
    PFN_BML_ConfigBatchDiscard BatchDiscard;
    /* Schema migration */
    PFN_BML_RegisterConfigLoadHooks RegisterLoadHooks;
    /* Typed shortcuts (v1.1) */
    PFN_BML_ConfigGetInt GetInt;
    PFN_BML_ConfigGetFloat GetFloat;
    PFN_BML_ConfigGetBool GetBool;
    PFN_BML_ConfigGetString GetString;
} BML_CoreConfigInterface;

/* ========================================================================
 * Memory -- allocator, pools, stats
 * ======================================================================== */

typedef struct BML_CoreMemoryInterface {
    BML_InterfaceHeader header;
    /* Standard allocator */
    PFN_BML_Alloc Alloc;
    PFN_BML_Calloc Calloc;
    PFN_BML_Realloc Realloc;
    PFN_BML_Free Free;
    /* Aligned allocator */
    PFN_BML_AllocAligned AllocAligned;
    PFN_BML_FreeAligned FreeAligned;
    /* Pool allocator */
    PFN_BML_MemoryPoolCreate MemoryPoolCreate;
    PFN_BML_MemoryPoolAlloc MemoryPoolAlloc;
    PFN_BML_MemoryPoolFree MemoryPoolFree;
    PFN_BML_MemoryPoolDestroy MemoryPoolDestroy;
    /* Stats */
    PFN_BML_GetMemoryStats GetMemoryStats;
} BML_CoreMemoryInterface;

/* ========================================================================
 * Resource -- ref-counted handles, custom types
 * ======================================================================== */

typedef struct BML_CoreResourceInterface {
    BML_InterfaceHeader header;
    PFN_BML_RegisterResourceType RegisterResourceType;
    PFN_BML_HandleCreate HandleCreate;
    PFN_BML_HandleRetain HandleRetain;
    PFN_BML_HandleRelease HandleRelease;
    PFN_BML_HandleValidate HandleValidate;
    PFN_BML_HandleAttachUserData HandleAttachUserData;
    PFN_BML_HandleGetUserData HandleGetUserData;
} BML_CoreResourceInterface;

/* ========================================================================
 * Diagnostic -- errors, interface reflection
 * ======================================================================== */

typedef struct BML_CoreDiagnosticInterface {
    BML_InterfaceHeader header;
    /* Error handling */
    PFN_BML_GetLastError GetLastError;
    PFN_BML_ClearLastError ClearLastError;
    PFN_BML_GetErrorString GetErrorString;
    /* Interface reflection -- query */
    PFN_BML_DiagInterfaceExists InterfaceExists;
    PFN_BML_DiagGetInterfaceDescriptor GetInterfaceDescriptor;
    PFN_BML_DiagIsInterfaceCompatible IsInterfaceCompatible;
    PFN_BML_DiagGetInterfaceCount GetInterfaceCount;
    PFN_BML_DiagGetLeaseCount GetLeaseCount;
    /* Interface reflection -- enumeration */
    PFN_BML_DiagEnumerateInterfaces EnumerateInterfaces;
    PFN_BML_DiagEnumerateByProvider EnumerateByProvider;
    PFN_BML_DiagEnumerateByCapability EnumerateByCapability;
} BML_CoreDiagnosticInterface;

/* ========================================================================
 * IMC Bus -- topics, pub/sub, intercept, RPC, futures, state, stats
 * ======================================================================== */

typedef struct BML_ImcBusInterface {
    BML_InterfaceHeader header;
    /* Topic / RPC resolution */
    PFN_BML_ImcGetTopicId GetTopicId;
    PFN_BML_ImcGetRpcId GetRpcId;
    /* Publish */
    PFN_BML_ImcPublish Publish;
    PFN_BML_ImcPublishEx PublishEx;
    PFN_BML_ImcPublishBuffer PublishBuffer;
    PFN_BML_ImcPublishMulti PublishMulti;
    PFN_BML_ImcPublishInterceptable PublishInterceptable;
    /* Subscribe */
    PFN_BML_ImcSubscribe Subscribe;
    PFN_BML_ImcSubscribeEx SubscribeEx;
    PFN_BML_ImcSubscribeIntercept SubscribeIntercept;
    PFN_BML_ImcSubscribeInterceptEx SubscribeInterceptEx;
    PFN_BML_ImcUnsubscribe Unsubscribe;
    PFN_BML_ImcSubscriptionIsActive SubscriptionIsActive;
    /* RPC */
    PFN_BML_ImcRegisterRpc RegisterRpc;
    PFN_BML_ImcUnregisterRpc UnregisterRpc;
    PFN_BML_ImcCallRpc CallRpc;
    /* Futures */
    PFN_BML_ImcFutureAwait FutureAwait;
    PFN_BML_ImcFutureGetResult FutureGetResult;
    PFN_BML_ImcFutureGetState FutureGetState;
    PFN_BML_ImcFutureCancel FutureCancel;
    PFN_BML_ImcFutureOnComplete FutureOnComplete;
    PFN_BML_ImcFutureRelease FutureRelease;
    /* Retained state */
    PFN_BML_ImcPublishState PublishState;
    PFN_BML_ImcCopyState CopyState;
    PFN_BML_ImcClearState ClearState;
    /* Pump */
    PFN_BML_ImcPump Pump;
    /* Stats */
    PFN_BML_ImcGetSubscriptionStats GetSubscriptionStats;
    PFN_BML_ImcGetStats GetStats;
    PFN_BML_ImcResetStats ResetStats;
    PFN_BML_ImcGetTopicInfo GetTopicInfo;
    PFN_BML_ImcGetTopicName GetTopicName;
    /* RPC v1.1 -- introspection, errors, options, middleware, streaming */
    PFN_BML_ImcRegisterRpcEx RegisterRpcEx;
    PFN_BML_ImcCallRpcEx CallRpcEx;
    PFN_BML_ImcFutureGetError FutureGetError;
    PFN_BML_ImcGetRpcInfo GetRpcInfo;
    PFN_BML_ImcGetRpcName GetRpcName;
    PFN_BML_ImcEnumerateRpc EnumerateRpc;
    PFN_BML_ImcAddRpcMiddleware AddRpcMiddleware;
    PFN_BML_ImcRemoveRpcMiddleware RemoveRpcMiddleware;
    PFN_BML_ImcRegisterStreamingRpc RegisterStreamingRpc;
    PFN_BML_ImcStreamPush StreamPush;
    PFN_BML_ImcStreamComplete StreamComplete;
    PFN_BML_ImcStreamError StreamError;
    PFN_BML_ImcCallStreamingRpc CallStreamingRpc;
} BML_ImcBusInterface;

/* ========================================================================
 * Timer -- one-shot, repeating, frame-based scheduling
 * ======================================================================== */

typedef struct BML_CoreTimerInterface {
    BML_InterfaceHeader header;
    PFN_BML_TimerScheduleOnce ScheduleOnce;
    PFN_BML_TimerScheduleRepeat ScheduleRepeat;
    PFN_BML_TimerScheduleFrames ScheduleFrames;
    PFN_BML_TimerCancel Cancel;
    PFN_BML_TimerIsActive IsActive;
    PFN_BML_TimerCancelAll CancelAll;
} BML_CoreTimerInterface;

/* ========================================================================
 * Hook Registry -- advisory hook tracking, conflict detection
 * ======================================================================== */

typedef struct BML_CoreHookRegistryInterface {
    BML_InterfaceHeader header;
    PFN_BML_HookRegister Register;
    PFN_BML_HookUnregister Unregister;
    PFN_BML_HookEnumerate Enumerate;
} BML_CoreHookRegistryInterface;

/* ========================================================================
 * Locale -- per-mod localization strings
 * ======================================================================== */

typedef struct BML_CoreLocaleInterface {
    BML_InterfaceHeader header;
    PFN_BML_LocaleLoad Load;
    PFN_BML_LocaleSetLanguage SetLanguage;
    PFN_BML_LocaleGetLanguage GetLanguage;
    PFN_BML_LocaleGet Get;
    PFN_BML_LocaleBindTable BindTable;
    PFN_BML_LocaleLookup Lookup;
} BML_CoreLocaleInterface;

/* ========================================================================
 * Sync -- portable synchronization primitives
 * ======================================================================== */

typedef struct BML_CoreSyncInterface {
    BML_InterfaceHeader header;
    /* Total: 44 function pointers */
    /* Mutex (6) */
    PFN_BML_MutexCreate MutexCreate;
    PFN_BML_MutexDestroy MutexDestroy;
    PFN_BML_MutexLock MutexLock;
    PFN_BML_MutexTryLock MutexTryLock;
    PFN_BML_MutexLockTimeout MutexLockTimeout;
    PFN_BML_MutexUnlock MutexUnlock;
    /* RwLock (11) */
    PFN_BML_RwLockCreate RwLockCreate;
    PFN_BML_RwLockDestroy RwLockDestroy;
    PFN_BML_RwLockReadLock RwLockReadLock;
    PFN_BML_RwLockTryReadLock RwLockTryReadLock;
    PFN_BML_RwLockReadLockTimeout RwLockReadLockTimeout;
    PFN_BML_RwLockWriteLock RwLockWriteLock;
    PFN_BML_RwLockTryWriteLock RwLockTryWriteLock;
    PFN_BML_RwLockWriteLockTimeout RwLockWriteLockTimeout;
    PFN_BML_RwLockUnlock RwLockUnlock;
    PFN_BML_RwLockReadUnlock RwLockReadUnlock;
    PFN_BML_RwLockWriteUnlock RwLockWriteUnlock;
    /* Atomic (8) */
    PFN_BML_AtomicIncrement32 AtomicIncrement32;
    PFN_BML_AtomicDecrement32 AtomicDecrement32;
    PFN_BML_AtomicAdd32 AtomicAdd32;
    PFN_BML_AtomicCompareExchange32 AtomicCompareExchange32;
    PFN_BML_AtomicExchange32 AtomicExchange32;
    PFN_BML_AtomicLoadPtr AtomicLoadPtr;
    PFN_BML_AtomicStorePtr AtomicStorePtr;
    PFN_BML_AtomicCompareExchangePtr AtomicCompareExchangePtr;
    /* Semaphore (4) */
    PFN_BML_SemaphoreCreate SemaphoreCreate;
    PFN_BML_SemaphoreDestroy SemaphoreDestroy;
    PFN_BML_SemaphoreWait SemaphoreWait;
    PFN_BML_SemaphoreSignal SemaphoreSignal;
    /* TLS (4) */
    PFN_BML_TlsCreate TlsCreate;
    PFN_BML_TlsDestroy TlsDestroy;
    PFN_BML_TlsGet TlsGet;
    PFN_BML_TlsSet TlsSet;
    /* CondVar (6) */
    PFN_BML_CondVarCreate CondVarCreate;
    PFN_BML_CondVarDestroy CondVarDestroy;
    PFN_BML_CondVarWait CondVarWait;
    PFN_BML_CondVarWaitTimeout CondVarWaitTimeout;
    PFN_BML_CondVarSignal CondVarSignal;
    PFN_BML_CondVarBroadcast CondVarBroadcast;
    /* SpinLock (5) */
    PFN_BML_SpinLockCreate SpinLockCreate;
    PFN_BML_SpinLockDestroy SpinLockDestroy;
    PFN_BML_SpinLockLock SpinLockLock;
    PFN_BML_SpinLockTryLock SpinLockTryLock;
    PFN_BML_SpinLockUnlock SpinLockUnlock;
} BML_CoreSyncInterface;

/* ========================================================================
 * Profiling -- performance tracing and counters
 * ======================================================================== */

typedef struct BML_CoreProfilingInterface {
    BML_InterfaceHeader header;
    /* Trace (6) */
    PFN_BML_TraceBegin TraceBegin;
    PFN_BML_TraceEnd TraceEnd;
    PFN_BML_TraceInstant TraceInstant;
    PFN_BML_TraceSetThreadName TraceSetThreadName;
    PFN_BML_TraceCounter TraceCounter;
    PFN_BML_TraceFrameMark TraceFrameMark;
    /* Counters (4) */
    PFN_BML_GetApiCallCount GetApiCallCount;
    PFN_BML_GetTotalAllocBytes GetTotalAllocBytes;
    PFN_BML_GetTimestampNs GetTimestampNs;
    PFN_BML_GetCpuFrequency GetCpuFrequency;
    /* Backend (4) */
    PFN_BML_GetProfilerBackend GetProfilerBackend;
    PFN_BML_SetProfilingEnabled SetProfilingEnabled;
    PFN_BML_IsProfilingEnabled IsProfilingEnabled;
    PFN_BML_FlushProfilingData FlushProfilingData;
    /* Stats (1) */
    PFN_BML_GetProfilingStats GetProfilingStats;
} BML_CoreProfilingInterface;

/* ========================================================================
 * Host Runtime -- host-owned interface contribution
 * ======================================================================== */

typedef struct BML_HostRuntimeInterface {
    BML_InterfaceHeader header;
    PFN_BML_HostRegisterContribution RegisterContribution;
    PFN_BML_HostUnregisterContribution UnregisterContribution;
} BML_HostRuntimeInterface;

BML_END_CDECLS

#endif /* BML_BUILTIN_INTERFACES_H */

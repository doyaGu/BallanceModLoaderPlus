#ifndef BML_API_IDS_H
#define BML_API_IDS_H

/**
 * @file bml_api_ids.h
 * @brief Stable API identifiers for binary compatibility
 * 
 * ============================================================================
 * CRITICAL: THESE IDS ARE PERMANENT AND MUST NEVER CHANGE
 * ============================================================================
 * 
 * Like Linux syscall numbers, once an ID is assigned, it is frozen forever.
 * This ensures binary compatibility: mods compiled against older BML versions
 * will continue to work with newer versions without recompilation.
 * 
 * ID Allocation Rules:
 * - Range 1-999:     Core BML APIs
 * - Range 1000-1999: IMC system APIs
 * - Range 2000-2999: Configuration APIs
 * - Range 3000-3999: Logging APIs
 * - Range 4000-4999: Module management APIs (reserved)
 * - Range 5000-5999: Resource/Memory APIs
 * - Range 6000-6999: Extension system APIs
 * - Range 7000-7999: Synchronization APIs
 * - Range 8000-8999: Profiling APIs
 * - Range 9000-9999: Available for future use
 * - Range 10000+:    Reserved for major expansions
 * 
 * Version Compatibility:
 * - Adding new IDs: Always append to the end of each range
 * - Deprecated APIs: Keep ID reserved with DEPRECATED comment
 * - Never reuse IDs: Even if API is removed
 * - Binary mods compiled against older versions work with newer versions
 * 
 * Example:
 *   #define BML_API_ID_bmlImcPublish 1000u  // v0.4.0 - permanent forever
 */

#include "bml_types.h"

/* ========================================================================
 * Core BML APIs (1-999)
 * ======================================================================== */

/** @brief Core lifecycle */
#define BML_API_ID_bmlAttach               1u
#define BML_API_ID_bmlDiscoverModules      2u
#define BML_API_ID_bmlLoadModules          3u
#define BML_API_ID_bmlDetach               4u
#define BML_API_ID_bmlGetProcAddress       5u
#define BML_API_ID_bmlGetProcAddressById   6u
#define BML_API_ID_bmlGetApiId             7u

/** @brief Context management */
#define BML_API_ID_bmlContextRetain        10u
#define BML_API_ID_bmlContextRelease       11u
#define BML_API_ID_bmlGetGlobalContext     12u
#define BML_API_ID_bmlGetRuntimeVersion    13u
#define BML_API_ID_bmlContextSetUserData   14u
#define BML_API_ID_bmlContextGetUserData   15u

/** @brief Capability system */
#define BML_API_ID_bmlRequestCapability    20u
#define BML_API_ID_bmlCheckCapability      21u
#define BML_API_ID_bmlCoreGetCaps          22u

/** @brief Module metadata */
#define BML_API_ID_bmlGetModId             30u
#define BML_API_ID_bmlGetModVersion        31u
#define BML_API_ID_bmlRegisterShutdownHook 32u
#define BML_API_ID_bmlSetCurrentModule     33u
#define BML_API_ID_bmlGetCurrentModule     34u

/** @brief Diagnostics / Error Handling */
#define BML_API_ID_bmlGetLastError         40u
#define BML_API_ID_bmlClearLastError       41u
#define BML_API_ID_bmlGetErrorString       42u

/* ========================================================================
 * IMC System APIs (1000-1099)
 * 
 * v0.4.0 Robust High-Performance IMC (25 APIs total)
 * - ID Resolution: 2
 * - Pub/Sub: 7
 * - RPC: 3
 * - Futures: 6
 * - Diagnostics: 6
 * - Runtime: 1
 * ======================================================================== */

/** @brief ID Resolution */
#define BML_API_ID_bmlImcGetTopicId              1000u
#define BML_API_ID_bmlImcGetRpcId                1001u

/** @brief Pub/Sub */
#define BML_API_ID_bmlImcPublish                 1010u
#define BML_API_ID_bmlImcPublishEx               1011u
#define BML_API_ID_bmlImcPublishBuffer           1012u
#define BML_API_ID_bmlImcPublishMulti            1013u
#define BML_API_ID_bmlImcSubscribe               1014u
#define BML_API_ID_bmlImcSubscribeEx             1015u
#define BML_API_ID_bmlImcUnsubscribe             1016u
#define BML_API_ID_bmlImcSubscriptionIsActive    1017u

/** @brief RPC */
#define BML_API_ID_bmlImcRegisterRpc             1020u
#define BML_API_ID_bmlImcUnregisterRpc           1021u
#define BML_API_ID_bmlImcCallRpc                 1022u

/** @brief Futures */
#define BML_API_ID_bmlImcFutureAwait             1030u
#define BML_API_ID_bmlImcFutureGetResult         1031u
#define BML_API_ID_bmlImcFutureGetState          1032u
#define BML_API_ID_bmlImcFutureCancel            1033u
#define BML_API_ID_bmlImcFutureOnComplete        1034u
#define BML_API_ID_bmlImcFutureRelease           1035u

/** @brief Diagnostics & Statistics */
#define BML_API_ID_bmlImcGetCaps                 1040u
#define BML_API_ID_bmlImcGetStats                1041u
#define BML_API_ID_bmlImcResetStats              1042u
#define BML_API_ID_bmlImcGetSubscriptionStats    1043u
#define BML_API_ID_bmlImcGetTopicInfo            1044u
#define BML_API_ID_bmlImcGetTopicName            1045u

/** @brief Runtime */
#define BML_API_ID_bmlImcPump                    1050u

/* ========================================================================
 * Configuration APIs (2000-2999)
 * ======================================================================== */

#define BML_API_ID_bmlConfigGet          2000u
#define BML_API_ID_bmlConfigSet          2001u
#define BML_API_ID_bmlConfigReset        2002u
#define BML_API_ID_bmlConfigEnumerate    2003u
#define BML_API_ID_bmlConfigGetCaps      2004u

/** @brief Batch operations (v0.4.0) */
#define BML_API_ID_bmlConfigBatchBegin   2010u
#define BML_API_ID_bmlConfigBatchSet     2011u
#define BML_API_ID_bmlConfigBatchCommit  2012u
#define BML_API_ID_bmlConfigBatchDiscard 2013u

/* ========================================================================
 * Logging APIs (3000-3999)
 * ======================================================================== */

#define BML_API_ID_bmlLog                3000u
#define BML_API_ID_bmlLogVa              3001u
#define BML_API_ID_bmlSetLogFilter       3002u
#define BML_API_ID_bmlLoggingGetCaps     3003u

/* ========================================================================
 * Module Management APIs (4000-4999)
 * ======================================================================== */

/* Reserved for future module management APIs */

/* ========================================================================
 * Resource/Memory APIs (5000-5999)
 * ======================================================================== */

/** @brief Basic allocation */
#define BML_API_ID_bmlAlloc             5000u
#define BML_API_ID_bmlCalloc            5001u
#define BML_API_ID_bmlRealloc           5002u
#define BML_API_ID_bmlFree              5003u
#define BML_API_ID_bmlAllocAligned      5004u
#define BML_API_ID_bmlFreeAligned       5005u

/** @brief Memory pools */
#define BML_API_ID_bmlMemoryPoolCreate  5010u
#define BML_API_ID_bmlMemoryPoolAlloc   5011u
#define BML_API_ID_bmlMemoryPoolFree    5012u
#define BML_API_ID_bmlMemoryPoolDestroy 5013u

/** @brief Memory stats */
#define BML_API_ID_bmlGetMemoryStats    5020u
#define BML_API_ID_bmlMemoryGetCaps     5021u

/** @brief Handle management */
#define BML_API_ID_bmlHandleCreate         5030u
#define BML_API_ID_bmlHandleRetain         5031u
#define BML_API_ID_bmlHandleRelease        5032u
#define BML_API_ID_bmlHandleValidate       5033u
#define BML_API_ID_bmlHandleAttachUserData 5034u
#define BML_API_ID_bmlHandleGetUserData    5035u
#define BML_API_ID_bmlResourceGetCaps      5036u

/* ========================================================================
 * Extension System APIs (6000-6999)
 * ======================================================================== */

#define BML_API_ID_bmlExtensionRegister         6000u
#define BML_API_ID_bmlExtensionQuery            6001u
#define BML_API_ID_bmlExtensionLoad             6002u
#define BML_API_ID_bmlExtensionEnumerate        6003u
#define BML_API_ID_bmlExtensionUnregister       6004u
#define BML_API_ID_bmlExtensionGetCaps          6005u
#define BML_API_ID_bmlExtensionCount            6006u
#define BML_API_ID_bmlExtensionUpdateApi        6007u
#define BML_API_ID_bmlExtensionDeprecate        6008u
#define BML_API_ID_bmlExtensionAddListener      6009u
#define BML_API_ID_bmlExtensionRemoveListener   6010u
#define BML_API_ID_bmlExtensionUnload           6011u
#define BML_API_ID_bmlExtensionGetRefCount      6012u
#define BML_API_ID_bmlRegisterConfigLoadHooks   6020u
#define BML_API_ID_bmlRegisterLogSinkOverride   6021u
#define BML_API_ID_bmlClearLogSinkOverride      6022u
#define BML_API_ID_bmlRegisterResourceType      6023u

/* ========================================================================
 * Synchronization APIs (7000-7999)
 * ======================================================================== */

/** @brief Mutex operations */
#define BML_API_ID_bmlMutexCreate       7000u
#define BML_API_ID_bmlMutexDestroy      7001u
#define BML_API_ID_bmlMutexLock         7002u
#define BML_API_ID_bmlMutexTryLock      7003u
#define BML_API_ID_bmlMutexUnlock       7004u
#define BML_API_ID_bmlMutexLockTimeout  7005u

/** @brief Read-write lock operations */
#define BML_API_ID_bmlRwLockCreate           7010u
#define BML_API_ID_bmlRwLockDestroy          7011u
#define BML_API_ID_bmlRwLockReadLock         7012u
#define BML_API_ID_bmlRwLockTryReadLock      7013u
#define BML_API_ID_bmlRwLockWriteLock        7014u
#define BML_API_ID_bmlRwLockTryWriteLock     7015u
#define BML_API_ID_bmlRwLockUnlock           7016u
#define BML_API_ID_bmlRwLockReadUnlock       7017u
#define BML_API_ID_bmlRwLockWriteUnlock      7018u
#define BML_API_ID_bmlRwLockReadLockTimeout  7019u
#define BML_API_ID_bmlRwLockWriteLockTimeout 7020u

/** @brief Atomic operations */
#define BML_API_ID_bmlAtomicIncrement32        7030u
#define BML_API_ID_bmlAtomicDecrement32        7031u
#define BML_API_ID_bmlAtomicAdd32              7032u
#define BML_API_ID_bmlAtomicCompareExchange32  7033u
#define BML_API_ID_bmlAtomicExchange32         7034u
#define BML_API_ID_bmlAtomicLoadPtr            7035u
#define BML_API_ID_bmlAtomicStorePtr           7036u
#define BML_API_ID_bmlAtomicCompareExchangePtr 7037u

/** @brief Semaphore operations */
#define BML_API_ID_bmlSemaphoreCreate   7040u
#define BML_API_ID_bmlSemaphoreDestroy  7041u
#define BML_API_ID_bmlSemaphoreWait     7042u
#define BML_API_ID_bmlSemaphoreSignal   7043u

/** @brief Thread-local storage */
#define BML_API_ID_bmlTlsCreate         7050u
#define BML_API_ID_bmlTlsDestroy        7051u
#define BML_API_ID_bmlTlsGet            7052u
#define BML_API_ID_bmlTlsSet            7053u

/** @brief Condition variables */
#define BML_API_ID_bmlCondVarCreate         7060u
#define BML_API_ID_bmlCondVarDestroy        7061u
#define BML_API_ID_bmlCondVarWait           7062u
#define BML_API_ID_bmlCondVarWaitTimeout    7063u
#define BML_API_ID_bmlCondVarSignal         7064u
#define BML_API_ID_bmlCondVarBroadcast      7065u

/** @brief Spin locks */
#define BML_API_ID_bmlSpinLockCreate    7070u
#define BML_API_ID_bmlSpinLockDestroy   7071u
#define BML_API_ID_bmlSpinLockLock      7072u
#define BML_API_ID_bmlSpinLockTryLock   7073u
#define BML_API_ID_bmlSpinLockUnlock    7074u

/** @brief Sync capabilities */
#define BML_API_ID_bmlSyncGetCaps       7080u

/* ========================================================================
 * Profiling APIs (8000-8999)
 * ======================================================================== */

/** @brief Tracing */
#define BML_API_ID_bmlTraceBegin         8000u
#define BML_API_ID_bmlTraceEnd           8001u
#define BML_API_ID_bmlTraceInstant       8002u
#define BML_API_ID_bmlTraceSetThreadName 8003u
#define BML_API_ID_bmlTraceCounter       8004u
#define BML_API_ID_bmlTraceFrameMark     8005u

/** @brief Profiling stats */
#define BML_API_ID_bmlGetApiCallCount    8010u
#define BML_API_ID_bmlGetTotalAllocBytes 8011u
#define BML_API_ID_bmlGetTimestampNs     8012u
#define BML_API_ID_bmlGetCpuFrequency    8013u

/** @brief Profiler control */
#define BML_API_ID_bmlGetProfilerBackend  8020u
#define BML_API_ID_bmlSetProfilingEnabled 8021u
#define BML_API_ID_bmlIsProfilingEnabled  8022u
#define BML_API_ID_bmlFlushProfilingData  8023u
#define BML_API_ID_bmlGetProfilingStats   8024u
#define BML_API_ID_bmlProfilingGetCaps    8025u

/** @brief API Tracing (v0.5.0) */
#define BML_API_ID_bmlEnableApiTracing    8030u
#define BML_API_ID_bmlIsApiTracingEnabled 8031u
#define BML_API_ID_bmlSetTraceCallback    8032u
#define BML_API_ID_bmlGetApiStats         8033u
#define BML_API_ID_bmlEnumerateApiStats   8034u
#define BML_API_ID_bmlDumpApiStats        8035u
#define BML_API_ID_bmlResetApiStats       8036u
#define BML_API_ID_bmlValidateApiId       8037u

/* ========================================================================
 * Capability and Discovery APIs (9000-9099) - v0.5.0
 * ======================================================================== */

/** @brief Capability query */
#define BML_API_ID_bmlQueryCapabilities       9000u
#define BML_API_ID_bmlHasCapability           9001u
#define BML_API_ID_bmlCheckCompatibility      9002u

/** @brief API discovery */
#define BML_API_ID_bmlGetApiDescriptor        9003u
#define BML_API_ID_bmlGetApiDescriptorByName  9004u
#define BML_API_ID_bmlEnumerateApis           9005u
#define BML_API_ID_bmlGetApiIntroducedVersion 9006u

/* ========================================================================
 * Reserved for Future Use (10000+)
 * ======================================================================== */

/* Keep space for major feature additions */

#endif /* BML_API_IDS_H */

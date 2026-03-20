#include "BuiltinInterfaces.h"

#include "ApiRegistry.h"
#include "Context.h"
#include "InterfaceRegistry.h"
#include "LeaseManager.h"

#include "bml_builtin_interfaces.h"

namespace BML::Core {
    namespace {
        constexpr uint16_t kCoreContextInterfaceMinor = 0;
        constexpr uint16_t kCoreModuleInterfaceMinor = 2;
        constexpr uint16_t kCoreLoggingInterfaceMinor = 0;
        constexpr uint16_t kCoreConfigInterfaceMinor = 1;
        constexpr uint16_t kCoreMemoryInterfaceMinor = 0;
        constexpr uint16_t kCoreResourceInterfaceMinor = 0;
        constexpr uint16_t kCoreDiagnosticInterfaceMinor = 0;
        constexpr uint16_t kImcBusInterfaceMinor = 1;
        constexpr uint16_t kCoreTimerInterfaceMinor = 0;
        constexpr uint16_t kCoreHookRegistryInterfaceMinor = 0;
        constexpr uint16_t kCoreLocaleInterfaceMinor = 0;
        constexpr uint16_t kCoreSyncInterfaceMinor = 0;
        constexpr uint16_t kCoreProfilingInterfaceMinor = 0;
        constexpr uint16_t kHostRuntimeInterfaceMinor = 0;

        template <typename T>
        T ResolveApi(const char *name) {
            return reinterpret_cast<T>(ApiRegistry::Instance().Get(name));
        }

        BML_Bool BML_API_GetInterfaceDescriptorRuntime(const char *interface_id,
                                                       BML_InterfaceRuntimeDesc *out_desc) {
            return InterfaceRegistry::Instance().GetDescriptor(interface_id, out_desc) == BML_RESULT_OK
                ? BML_TRUE
                : BML_FALSE;
        }

        void BML_API_EnumerateInterfacesRuntime(PFN_BML_InterfaceRuntimeEnumerator callback,
                                                void *user_data,
                                                uint64_t required_flags_mask) {
            InterfaceRegistry::Instance().Enumerate(callback, user_data, required_flags_mask);
        }

        BML_Bool BML_API_InterfaceExists(const char *interface_id) {
            return InterfaceRegistry::Instance().Exists(interface_id) ? BML_TRUE : BML_FALSE;
        }

        BML_Bool BML_API_IsInterfaceCompatible(const char *interface_id,
                                                const BML_Version *required_abi) {
            return InterfaceRegistry::Instance().IsCompatible(interface_id, required_abi)
                ? BML_TRUE : BML_FALSE;
        }

        void BML_API_EnumerateByProvider(const char *provider_id,
                                          PFN_BML_InterfaceRuntimeEnumerator callback,
                                          void *user_data) {
            InterfaceRegistry::Instance().EnumerateByProvider(provider_id, callback, user_data);
        }

        void BML_API_EnumerateByCapability(uint64_t required_caps,
                                            PFN_BML_InterfaceRuntimeEnumerator callback,
                                            void *user_data) {
            InterfaceRegistry::Instance().EnumerateByCapability(required_caps, callback, user_data);
        }

        uint32_t BML_API_GetInterfaceCount() {
            return InterfaceRegistry::Instance().GetCount();
        }

        uint32_t BML_API_GetLeaseCount(const char *interface_id) {
            return InterfaceRegistry::Instance().GetLeaseCount(interface_id);
        }

        BML_Result BML_API_RegisterHostContribution(BML_Mod provider_mod,
                                                    const char *host_interface_id,
                                                    BML_InterfaceRegistration *out_registration) {
            if (!provider_mod || !host_interface_id || !out_registration) {
                return BML_RESULT_INVALID_ARGUMENT;
            }

            auto *consumer = Context::Instance().ResolveCurrentConsumer();
            auto *provider = Context::Instance().ResolveModHandle(provider_mod);
            if (!consumer || !provider) {
                return BML_RESULT_INVALID_CONTEXT;
            }

            BML_InterfaceRuntimeDesc desc = BML_INTERFACE_RUNTIME_DESC_INIT;
            BML_CHECK(InterfaceRegistry::Instance().GetDescriptor(host_interface_id, &desc));
            if ((desc.flags & BML_INTERFACE_FLAG_HOST_OWNED) == 0) {
                return BML_RESULT_PERMISSION_DENIED;
            }
            if (LeaseManager::Instance().IsProviderBlocked(provider->id)) {
                return BML_RESULT_BUSY;
            }

            return LeaseManager::Instance().CreateInterfaceRegistration(
                host_interface_id, provider->id, consumer->id, out_registration);
        }

        BML_Result BML_API_UnregisterHostContribution(BML_InterfaceRegistration registration) {
            return LeaseManager::Instance().ReleaseInterfaceRegistration(registration);
        }

        BML_CoreContextInterface g_CoreContextInterface{};
        BML_CoreModuleInterface g_CoreModuleInterface{};
        BML_CoreLoggingInterface g_CoreLoggingInterface{};
        BML_CoreConfigInterface g_CoreConfigInterface{};
        BML_CoreMemoryInterface g_CoreMemoryInterface{};
        BML_CoreResourceInterface g_CoreResourceInterface{};
        BML_CoreDiagnosticInterface g_CoreDiagnosticInterface{};
        BML_ImcBusInterface g_ImcBusInterface{};
        BML_CoreTimerInterface g_CoreTimerInterface{};
        BML_CoreLocaleInterface g_CoreLocaleInterface{};
        BML_CoreHookRegistryInterface g_CoreHookRegistryInterface{};
        BML_CoreSyncInterface g_CoreSyncInterface{};
        BML_CoreProfilingInterface g_CoreProfilingInterface{};
        BML_HostRuntimeInterface g_HostRuntimeInterface{};

        BML_Result RegisterBuiltinInterface(const char *interface_id,
                                            const void *implementation,
                                            size_t implementation_size,
                                            uint64_t flags,
                                            BML_Version abi_version) {
            BML_InterfaceDesc desc = BML_INTERFACE_DESC_INIT;
            desc.interface_id = interface_id;
            desc.abi_version = abi_version;
            desc.implementation = implementation;
            desc.implementation_size = implementation_size;
            desc.flags = flags;
            return InterfaceRegistry::Instance().Register(&desc, "BML");
        }
    } // namespace

    void RegisterBuiltinInterfaces() {
        // Context
        g_CoreContextInterface = {
            BML_IFACE_HEADER(BML_CoreContextInterface,
                             BML_CORE_CONTEXT_INTERFACE_ID,
                             1,
                             kCoreContextInterfaceMinor),
            ResolveApi<PFN_BML_GetGlobalContext>("bmlGetGlobalContext"),
            ResolveApi<PFN_BML_GetRuntimeVersion>("bmlGetRuntimeVersion"),
            ResolveApi<PFN_BML_ContextRetain>("bmlContextRetain"),
            ResolveApi<PFN_BML_ContextRelease>("bmlContextRelease"),
            ResolveApi<PFN_BML_ContextSetUserData>("bmlContextSetUserData"),
            ResolveApi<PFN_BML_ContextGetUserData>("bmlContextGetUserData"),
        };
        // Module
        g_CoreModuleInterface = {
            BML_IFACE_HEADER(BML_CoreModuleInterface,
                             BML_CORE_MODULE_INTERFACE_ID,
                             1,
                             kCoreModuleInterfaceMinor),
            ResolveApi<PFN_BML_GetModId>("bmlGetModId"),
            ResolveApi<PFN_BML_GetModVersion>("bmlGetModVersion"),
            ResolveApi<PFN_BML_RequestCapability>("bmlRequestCapability"),
            ResolveApi<PFN_BML_CheckCapability>("bmlCheckCapability"),
            ResolveApi<PFN_BML_SetCurrentModule>("bmlSetCurrentModule"),
            ResolveApi<PFN_BML_GetCurrentModule>("bmlGetCurrentModule"),
            ResolveApi<PFN_BML_RegisterShutdownHook>("bmlRegisterShutdownHook"),
            ResolveApi<PFN_BML_GetLoadedModuleCount>("bmlGetLoadedModuleCount"),
            ResolveApi<PFN_BML_GetLoadedModuleAt>("bmlGetLoadedModuleAt"),
            ResolveApi<PFN_BML_GetManifestString>("bmlGetManifestString"),
            ResolveApi<PFN_BML_GetManifestInt>("bmlGetManifestInt"),
            ResolveApi<PFN_BML_GetManifestFloat>("bmlGetManifestFloat"),
            ResolveApi<PFN_BML_GetManifestBool>("bmlGetManifestBool"),
            ResolveApi<PFN_BML_GetModDirectory>("bmlGetModDirectory"),
            ResolveApi<PFN_BML_GetModName>("bmlGetModName"),
            ResolveApi<PFN_BML_GetModDescription>("bmlGetModDescription"),
            ResolveApi<PFN_BML_GetModAuthorCount>("bmlGetModAuthorCount"),
            ResolveApi<PFN_BML_GetModAuthorAt>("bmlGetModAuthorAt"),
            ResolveApi<PFN_BML_FindModuleById>("bmlFindModuleById"),
        };
        // Logging
        g_CoreLoggingInterface = {
            BML_IFACE_HEADER(BML_CoreLoggingInterface,
                             BML_CORE_LOGGING_INTERFACE_ID,
                             1,
                             kCoreLoggingInterfaceMinor),
            ResolveApi<PFN_BML_Log>("bmlLog"),
            ResolveApi<PFN_BML_LogVa>("bmlLogVa"),
            ResolveApi<PFN_BML_SetLogFilter>("bmlSetLogFilter"),
            ResolveApi<PFN_BML_RegisterLogSinkOverride>("bmlRegisterLogSinkOverride"),
            ResolveApi<PFN_BML_ClearLogSinkOverride>("bmlClearLogSinkOverride"),
        };
        // Config
        g_CoreConfigInterface = {
            BML_IFACE_HEADER(BML_CoreConfigInterface,
                             BML_CORE_CONFIG_INTERFACE_ID,
                             1,
                             kCoreConfigInterfaceMinor),
            ResolveApi<PFN_BML_ConfigGet>("bmlConfigGet"),
            ResolveApi<PFN_BML_ConfigSet>("bmlConfigSet"),
            ResolveApi<PFN_BML_ConfigReset>("bmlConfigReset"),
            ResolveApi<PFN_BML_ConfigEnumerate>("bmlConfigEnumerate"),
            ResolveApi<PFN_BML_ConfigBatchBegin>("bmlConfigBatchBegin"),
            ResolveApi<PFN_BML_ConfigBatchSet>("bmlConfigBatchSet"),
            ResolveApi<PFN_BML_ConfigBatchCommit>("bmlConfigBatchCommit"),
            ResolveApi<PFN_BML_ConfigBatchDiscard>("bmlConfigBatchDiscard"),
            ResolveApi<PFN_BML_RegisterConfigLoadHooks>("bmlRegisterConfigLoadHooks"),
            ResolveApi<PFN_BML_ConfigGetInt>("bmlConfigGetInt"),
            ResolveApi<PFN_BML_ConfigGetFloat>("bmlConfigGetFloat"),
            ResolveApi<PFN_BML_ConfigGetBool>("bmlConfigGetBool"),
            ResolveApi<PFN_BML_ConfigGetString>("bmlConfigGetString"),
        };
        // Memory
        g_CoreMemoryInterface = {
            BML_IFACE_HEADER(BML_CoreMemoryInterface,
                             BML_CORE_MEMORY_INTERFACE_ID,
                             1,
                             kCoreMemoryInterfaceMinor),
            ResolveApi<PFN_BML_Alloc>("bmlAlloc"),
            ResolveApi<PFN_BML_Calloc>("bmlCalloc"),
            ResolveApi<PFN_BML_Realloc>("bmlRealloc"),
            ResolveApi<PFN_BML_Free>("bmlFree"),
            ResolveApi<PFN_BML_AllocAligned>("bmlAllocAligned"),
            ResolveApi<PFN_BML_FreeAligned>("bmlFreeAligned"),
            ResolveApi<PFN_BML_MemoryPoolCreate>("bmlMemoryPoolCreate"),
            ResolveApi<PFN_BML_MemoryPoolAlloc>("bmlMemoryPoolAlloc"),
            ResolveApi<PFN_BML_MemoryPoolFree>("bmlMemoryPoolFree"),
            ResolveApi<PFN_BML_MemoryPoolDestroy>("bmlMemoryPoolDestroy"),
            ResolveApi<PFN_BML_GetMemoryStats>("bmlGetMemoryStats"),
        };
        // Resource
        g_CoreResourceInterface = {
            BML_IFACE_HEADER(BML_CoreResourceInterface,
                             BML_CORE_RESOURCE_INTERFACE_ID,
                             1,
                             kCoreResourceInterfaceMinor),
            ResolveApi<PFN_BML_RegisterResourceType>("bmlRegisterResourceType"),
            ResolveApi<PFN_BML_HandleCreate>("bmlHandleCreate"),
            ResolveApi<PFN_BML_HandleRetain>("bmlHandleRetain"),
            ResolveApi<PFN_BML_HandleRelease>("bmlHandleRelease"),
            ResolveApi<PFN_BML_HandleValidate>("bmlHandleValidate"),
            ResolveApi<PFN_BML_HandleAttachUserData>("bmlHandleAttachUserData"),
            ResolveApi<PFN_BML_HandleGetUserData>("bmlHandleGetUserData"),
        };
        // Diagnostic
        g_CoreDiagnosticInterface = {
            BML_IFACE_HEADER(BML_CoreDiagnosticInterface,
                             BML_CORE_DIAGNOSTIC_INTERFACE_ID,
                             1,
                             kCoreDiagnosticInterfaceMinor),
            ResolveApi<PFN_BML_GetLastError>("bmlGetLastError"),
            ResolveApi<PFN_BML_ClearLastError>("bmlClearLastError"),
            ResolveApi<PFN_BML_GetErrorString>("bmlGetErrorString"),
            BML_API_InterfaceExists,
            BML_API_GetInterfaceDescriptorRuntime,
            BML_API_IsInterfaceCompatible,
            BML_API_GetInterfaceCount,
            BML_API_GetLeaseCount,
            BML_API_EnumerateInterfacesRuntime,
            BML_API_EnumerateByProvider,
            BML_API_EnumerateByCapability,
        };
        // IMC Bus
        g_ImcBusInterface = {
            BML_IFACE_HEADER(BML_ImcBusInterface,
                             BML_IMC_BUS_INTERFACE_ID,
                             1,
                             kImcBusInterfaceMinor),
            ResolveApi<PFN_BML_ImcGetTopicId>("bmlImcGetTopicId"),
            ResolveApi<PFN_BML_ImcGetRpcId>("bmlImcGetRpcId"),
            ResolveApi<PFN_BML_ImcPublish>("bmlImcPublish"),
            ResolveApi<PFN_BML_ImcPublishEx>("bmlImcPublishEx"),
            ResolveApi<PFN_BML_ImcPublishBuffer>("bmlImcPublishBuffer"),
            ResolveApi<PFN_BML_ImcPublishMulti>("bmlImcPublishMulti"),
            ResolveApi<PFN_BML_ImcPublishInterceptable>("bmlImcPublishInterceptable"),
            ResolveApi<PFN_BML_ImcSubscribe>("bmlImcSubscribe"),
            ResolveApi<PFN_BML_ImcSubscribeEx>("bmlImcSubscribeEx"),
            ResolveApi<PFN_BML_ImcSubscribeIntercept>("bmlImcSubscribeIntercept"),
            ResolveApi<PFN_BML_ImcSubscribeInterceptEx>("bmlImcSubscribeInterceptEx"),
            ResolveApi<PFN_BML_ImcUnsubscribe>("bmlImcUnsubscribe"),
            ResolveApi<PFN_BML_ImcSubscriptionIsActive>("bmlImcSubscriptionIsActive"),
            ResolveApi<PFN_BML_ImcRegisterRpc>("bmlImcRegisterRpc"),
            ResolveApi<PFN_BML_ImcUnregisterRpc>("bmlImcUnregisterRpc"),
            ResolveApi<PFN_BML_ImcCallRpc>("bmlImcCallRpc"),
            ResolveApi<PFN_BML_ImcFutureAwait>("bmlImcFutureAwait"),
            ResolveApi<PFN_BML_ImcFutureGetResult>("bmlImcFutureGetResult"),
            ResolveApi<PFN_BML_ImcFutureGetState>("bmlImcFutureGetState"),
            ResolveApi<PFN_BML_ImcFutureCancel>("bmlImcFutureCancel"),
            ResolveApi<PFN_BML_ImcFutureOnComplete>("bmlImcFutureOnComplete"),
            ResolveApi<PFN_BML_ImcFutureRelease>("bmlImcFutureRelease"),
            ResolveApi<PFN_BML_ImcPublishState>("bmlImcPublishState"),
            ResolveApi<PFN_BML_ImcCopyState>("bmlImcCopyState"),
            ResolveApi<PFN_BML_ImcClearState>("bmlImcClearState"),
            ResolveApi<PFN_BML_ImcPump>("bmlImcPump"),
            ResolveApi<PFN_BML_ImcGetSubscriptionStats>("bmlImcGetSubscriptionStats"),
            ResolveApi<PFN_BML_ImcGetStats>("bmlImcGetStats"),
            ResolveApi<PFN_BML_ImcResetStats>("bmlImcResetStats"),
            ResolveApi<PFN_BML_ImcGetTopicInfo>("bmlImcGetTopicInfo"),
            ResolveApi<PFN_BML_ImcGetTopicName>("bmlImcGetTopicName"),
            // RPC v1.1
            ResolveApi<PFN_BML_ImcRegisterRpcEx>("bmlImcRegisterRpcEx"),
            ResolveApi<PFN_BML_ImcCallRpcEx>("bmlImcCallRpcEx"),
            ResolveApi<PFN_BML_ImcFutureGetError>("bmlImcFutureGetError"),
            ResolveApi<PFN_BML_ImcGetRpcInfo>("bmlImcGetRpcInfo"),
            ResolveApi<PFN_BML_ImcGetRpcName>("bmlImcGetRpcName"),
            ResolveApi<PFN_BML_ImcEnumerateRpc>("bmlImcEnumerateRpc"),
            ResolveApi<PFN_BML_ImcAddRpcMiddleware>("bmlImcAddRpcMiddleware"),
            ResolveApi<PFN_BML_ImcRemoveRpcMiddleware>("bmlImcRemoveRpcMiddleware"),
            ResolveApi<PFN_BML_ImcRegisterStreamingRpc>("bmlImcRegisterStreamingRpc"),
            ResolveApi<PFN_BML_ImcStreamPush>("bmlImcStreamPush"),
            ResolveApi<PFN_BML_ImcStreamComplete>("bmlImcStreamComplete"),
            ResolveApi<PFN_BML_ImcStreamError>("bmlImcStreamError"),
            ResolveApi<PFN_BML_ImcCallStreamingRpc>("bmlImcCallStreamingRpc"),
        };
        // Timer
        g_CoreTimerInterface = {
            BML_IFACE_HEADER(BML_CoreTimerInterface,
                             BML_CORE_TIMER_INTERFACE_ID,
                             1,
                             kCoreTimerInterfaceMinor),
            ResolveApi<PFN_BML_TimerScheduleOnce>("bmlTimerScheduleOnce"),
            ResolveApi<PFN_BML_TimerScheduleRepeat>("bmlTimerScheduleRepeat"),
            ResolveApi<PFN_BML_TimerScheduleFrames>("bmlTimerScheduleFrames"),
            ResolveApi<PFN_BML_TimerCancel>("bmlTimerCancel"),
            ResolveApi<PFN_BML_TimerIsActive>("bmlTimerIsActive"),
            ResolveApi<PFN_BML_TimerCancelAll>("bmlTimerCancelAll"),
        };
        // Hook Registry
        g_CoreHookRegistryInterface = {
            BML_IFACE_HEADER(BML_CoreHookRegistryInterface,
                             BML_CORE_HOOK_REGISTRY_INTERFACE_ID,
                             1,
                             kCoreHookRegistryInterfaceMinor),
            ResolveApi<PFN_BML_HookRegister>("bmlHookRegister"),
            ResolveApi<PFN_BML_HookUnregister>("bmlHookUnregister"),
            ResolveApi<PFN_BML_HookEnumerate>("bmlHookEnumerate"),
        };
        // Locale
        g_CoreLocaleInterface = {
            BML_IFACE_HEADER(BML_CoreLocaleInterface,
                             BML_CORE_LOCALE_INTERFACE_ID,
                             1,
                             kCoreLocaleInterfaceMinor),
            ResolveApi<PFN_BML_LocaleLoad>("bmlLocaleLoad"),
            ResolveApi<PFN_BML_LocaleSetLanguage>("bmlLocaleSetLanguage"),
            ResolveApi<PFN_BML_LocaleGetLanguage>("bmlLocaleGetLanguage"),
            ResolveApi<PFN_BML_LocaleGet>("bmlLocaleGet"),
            ResolveApi<PFN_BML_LocaleBindTable>("bmlLocaleBindTable"),
            ResolveApi<PFN_BML_LocaleLookup>("bmlLocaleLookup"),
        };
        // Sync
        g_CoreSyncInterface = {
            BML_IFACE_HEADER(BML_CoreSyncInterface,
                             BML_CORE_SYNC_INTERFACE_ID,
                             1,
                             kCoreSyncInterfaceMinor),
            // Mutex
            ResolveApi<PFN_BML_MutexCreate>("bmlMutexCreate"),
            ResolveApi<PFN_BML_MutexDestroy>("bmlMutexDestroy"),
            ResolveApi<PFN_BML_MutexLock>("bmlMutexLock"),
            ResolveApi<PFN_BML_MutexTryLock>("bmlMutexTryLock"),
            ResolveApi<PFN_BML_MutexLockTimeout>("bmlMutexLockTimeout"),
            ResolveApi<PFN_BML_MutexUnlock>("bmlMutexUnlock"),
            // RwLock
            ResolveApi<PFN_BML_RwLockCreate>("bmlRwLockCreate"),
            ResolveApi<PFN_BML_RwLockDestroy>("bmlRwLockDestroy"),
            ResolveApi<PFN_BML_RwLockReadLock>("bmlRwLockReadLock"),
            ResolveApi<PFN_BML_RwLockTryReadLock>("bmlRwLockTryReadLock"),
            ResolveApi<PFN_BML_RwLockReadLockTimeout>("bmlRwLockReadLockTimeout"),
            ResolveApi<PFN_BML_RwLockWriteLock>("bmlRwLockWriteLock"),
            ResolveApi<PFN_BML_RwLockTryWriteLock>("bmlRwLockTryWriteLock"),
            ResolveApi<PFN_BML_RwLockWriteLockTimeout>("bmlRwLockWriteLockTimeout"),
            ResolveApi<PFN_BML_RwLockUnlock>("bmlRwLockUnlock"),
            ResolveApi<PFN_BML_RwLockReadUnlock>("bmlRwLockReadUnlock"),
            ResolveApi<PFN_BML_RwLockWriteUnlock>("bmlRwLockWriteUnlock"),
            // Atomic
            ResolveApi<PFN_BML_AtomicIncrement32>("bmlAtomicIncrement32"),
            ResolveApi<PFN_BML_AtomicDecrement32>("bmlAtomicDecrement32"),
            ResolveApi<PFN_BML_AtomicAdd32>("bmlAtomicAdd32"),
            ResolveApi<PFN_BML_AtomicCompareExchange32>("bmlAtomicCompareExchange32"),
            ResolveApi<PFN_BML_AtomicExchange32>("bmlAtomicExchange32"),
            ResolveApi<PFN_BML_AtomicLoadPtr>("bmlAtomicLoadPtr"),
            ResolveApi<PFN_BML_AtomicStorePtr>("bmlAtomicStorePtr"),
            ResolveApi<PFN_BML_AtomicCompareExchangePtr>("bmlAtomicCompareExchangePtr"),
            // Semaphore
            ResolveApi<PFN_BML_SemaphoreCreate>("bmlSemaphoreCreate"),
            ResolveApi<PFN_BML_SemaphoreDestroy>("bmlSemaphoreDestroy"),
            ResolveApi<PFN_BML_SemaphoreWait>("bmlSemaphoreWait"),
            ResolveApi<PFN_BML_SemaphoreSignal>("bmlSemaphoreSignal"),
            // TLS
            ResolveApi<PFN_BML_TlsCreate>("bmlTlsCreate"),
            ResolveApi<PFN_BML_TlsDestroy>("bmlTlsDestroy"),
            ResolveApi<PFN_BML_TlsGet>("bmlTlsGet"),
            ResolveApi<PFN_BML_TlsSet>("bmlTlsSet"),
            // CondVar
            ResolveApi<PFN_BML_CondVarCreate>("bmlCondVarCreate"),
            ResolveApi<PFN_BML_CondVarDestroy>("bmlCondVarDestroy"),
            ResolveApi<PFN_BML_CondVarWait>("bmlCondVarWait"),
            ResolveApi<PFN_BML_CondVarWaitTimeout>("bmlCondVarWaitTimeout"),
            ResolveApi<PFN_BML_CondVarSignal>("bmlCondVarSignal"),
            ResolveApi<PFN_BML_CondVarBroadcast>("bmlCondVarBroadcast"),
            // SpinLock
            ResolveApi<PFN_BML_SpinLockCreate>("bmlSpinLockCreate"),
            ResolveApi<PFN_BML_SpinLockDestroy>("bmlSpinLockDestroy"),
            ResolveApi<PFN_BML_SpinLockLock>("bmlSpinLockLock"),
            ResolveApi<PFN_BML_SpinLockTryLock>("bmlSpinLockTryLock"),
            ResolveApi<PFN_BML_SpinLockUnlock>("bmlSpinLockUnlock"),
        };
        // Profiling
        g_CoreProfilingInterface = {
            BML_IFACE_HEADER(BML_CoreProfilingInterface,
                             BML_CORE_PROFILING_INTERFACE_ID,
                             1,
                             kCoreProfilingInterfaceMinor),
            // Trace
            ResolveApi<PFN_BML_TraceBegin>("bmlTraceBegin"),
            ResolveApi<PFN_BML_TraceEnd>("bmlTraceEnd"),
            ResolveApi<PFN_BML_TraceInstant>("bmlTraceInstant"),
            ResolveApi<PFN_BML_TraceSetThreadName>("bmlTraceSetThreadName"),
            ResolveApi<PFN_BML_TraceCounter>("bmlTraceCounter"),
            ResolveApi<PFN_BML_TraceFrameMark>("bmlTraceFrameMark"),
            // Counters
            ResolveApi<PFN_BML_GetApiCallCount>("bmlGetApiCallCount"),
            ResolveApi<PFN_BML_GetTotalAllocBytes>("bmlGetTotalAllocBytes"),
            ResolveApi<PFN_BML_GetTimestampNs>("bmlGetTimestampNs"),
            ResolveApi<PFN_BML_GetCpuFrequency>("bmlGetCpuFrequency"),
            // Backend
            ResolveApi<PFN_BML_GetProfilerBackend>("bmlGetProfilerBackend"),
            ResolveApi<PFN_BML_SetProfilingEnabled>("bmlSetProfilingEnabled"),
            ResolveApi<PFN_BML_IsProfilingEnabled>("bmlIsProfilingEnabled"),
            ResolveApi<PFN_BML_FlushProfilingData>("bmlFlushProfilingData"),
            // Stats
            ResolveApi<PFN_BML_GetProfilingStats>("bmlGetProfilingStats"),
        };
        // Host Runtime
        g_HostRuntimeInterface = {
            BML_IFACE_HEADER(BML_HostRuntimeInterface,
                             BML_CORE_HOST_RUNTIME_INTERFACE_ID,
                             1,
                             kHostRuntimeInterfaceMinor),
            BML_API_RegisterHostContribution,
            BML_API_UnregisterHostContribution,
        };

        // Register all interfaces in the registry
        RegisterBuiltinInterface(BML_CORE_CONTEXT_INTERFACE_ID,
                                 &g_CoreContextInterface,
                                 sizeof(g_CoreContextInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreContextInterfaceMinor, 0));
        RegisterBuiltinInterface(BML_CORE_MODULE_INTERFACE_ID,
                                 &g_CoreModuleInterface,
                                 sizeof(g_CoreModuleInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreModuleInterfaceMinor, 0));
        RegisterBuiltinInterface(BML_CORE_LOGGING_INTERFACE_ID,
                                 &g_CoreLoggingInterface,
                                 sizeof(g_CoreLoggingInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreLoggingInterfaceMinor, 0));
        RegisterBuiltinInterface(BML_CORE_CONFIG_INTERFACE_ID,
                                 &g_CoreConfigInterface,
                                 sizeof(g_CoreConfigInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreConfigInterfaceMinor, 0));
        RegisterBuiltinInterface(BML_CORE_MEMORY_INTERFACE_ID,
                                 &g_CoreMemoryInterface,
                                 sizeof(g_CoreMemoryInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreMemoryInterfaceMinor, 0));
        RegisterBuiltinInterface(BML_CORE_RESOURCE_INTERFACE_ID,
                                 &g_CoreResourceInterface,
                                 sizeof(g_CoreResourceInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreResourceInterfaceMinor, 0));
        RegisterBuiltinInterface(BML_CORE_DIAGNOSTIC_INTERFACE_ID,
                                 &g_CoreDiagnosticInterface,
                                 sizeof(g_CoreDiagnosticInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreDiagnosticInterfaceMinor, 0));
        RegisterBuiltinInterface(BML_IMC_BUS_INTERFACE_ID,
                                 &g_ImcBusInterface,
                                 sizeof(g_ImcBusInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kImcBusInterfaceMinor, 0));
        RegisterBuiltinInterface(BML_CORE_TIMER_INTERFACE_ID,
                                 &g_CoreTimerInterface,
                                 sizeof(g_CoreTimerInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreTimerInterfaceMinor, 0));
        RegisterBuiltinInterface(BML_CORE_HOOK_REGISTRY_INTERFACE_ID,
                                 &g_CoreHookRegistryInterface,
                                 sizeof(g_CoreHookRegistryInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreHookRegistryInterfaceMinor, 0));
        RegisterBuiltinInterface(BML_CORE_LOCALE_INTERFACE_ID,
                                 &g_CoreLocaleInterface,
                                 sizeof(g_CoreLocaleInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreLocaleInterfaceMinor, 0));
        RegisterBuiltinInterface(BML_CORE_SYNC_INTERFACE_ID,
                                 &g_CoreSyncInterface,
                                 sizeof(g_CoreSyncInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreSyncInterfaceMinor, 0));
        RegisterBuiltinInterface(BML_CORE_PROFILING_INTERFACE_ID,
                                 &g_CoreProfilingInterface,
                                 sizeof(g_CoreProfilingInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreProfilingInterfaceMinor, 0));
        RegisterBuiltinInterface(BML_CORE_HOST_RUNTIME_INTERFACE_ID,
                                 &g_HostRuntimeInterface,
                                 sizeof(g_HostRuntimeInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE | BML_INTERFACE_FLAG_INTERNAL,
                                 bmlMakeVersion(1, kHostRuntimeInterfaceMinor, 0));
    }
    void PopulateBuiltinServices(bml::BuiltinServices &out) {
        out.Context = &g_CoreContextInterface;
        out.Logging = &g_CoreLoggingInterface;
        out.Module = &g_CoreModuleInterface;
        out.Config = &g_CoreConfigInterface;
        out.Memory = &g_CoreMemoryInterface;
        out.Resource = &g_CoreResourceInterface;
        out.Diagnostic = &g_CoreDiagnosticInterface;
        out.ImcBus = &g_ImcBusInterface;
        out.Timer = &g_CoreTimerInterface;
        out.Locale = &g_CoreLocaleInterface;
        out.HookRegistry = &g_CoreHookRegistryInterface;
        out.Sync = &g_CoreSyncInterface;
        out.Profiling = &g_CoreProfilingInterface;
        out.HostRuntime = &g_HostRuntimeInterface;
    }
} // namespace BML::Core

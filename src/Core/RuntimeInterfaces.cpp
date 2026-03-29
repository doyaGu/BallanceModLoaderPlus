#include "RuntimeInterfaces.h"

#include "ApiRegistry.h"
#include "Context.h"
#include "DiagnosticManager.h"
#include "HookRegistry.h"
#include "ImcBus.h"
#include "InterfaceRegistry.h"
#include "KernelServices.h"
#include "LeaseManager.h"
#include "LocaleManager.h"
#include "Logging.h"
#include "MemoryManager.h"
#include "ModuleLifecycle.h"
#include "ProfilingManager.h"
#include "SyncManager.h"

#include "bml_services.hpp"
namespace BML::Core {
    namespace {
        constexpr uint16_t kCoreContextInterfaceMinor = 0;
        constexpr uint16_t kCoreModuleInterfaceMinor = 1;
        constexpr uint16_t kCoreLoggingInterfaceMinor = 1;
        constexpr uint16_t kCoreConfigInterfaceMinor = 2;
        constexpr uint16_t kCoreMemoryInterfaceMinor = 0;
        constexpr uint16_t kCoreResourceInterfaceMinor = 1;
        constexpr uint16_t kCoreDiagnosticInterfaceMinor = 0;
        constexpr uint16_t kCoreInterfaceControlMinor = 0;
        constexpr uint16_t kImcBusInterfaceMinor = 3;
        constexpr uint16_t kRpcInterfaceMinor = 3;
        constexpr uint16_t kCoreTimerInterfaceMinor = 1;
        constexpr uint16_t kCoreHookRegistryInterfaceMinor = 0;
        constexpr uint16_t kCoreLocaleInterfaceMinor = 1;
        constexpr uint16_t kCoreSyncInterfaceMinor = 0;
        constexpr uint16_t kCoreProfilingInterfaceMinor = 0;
        constexpr uint16_t kHostRuntimeInterfaceMinor = 0;

        template <typename T>
        T ResolveRawApi(KernelServices &kernel, const char *name) {
            auto &apiRegistry = *kernel.api_registry;
            return reinterpret_cast<T>(apiRegistry.Get(name));
        }

        KernelServices *KernelFromContextHandle(BML_Context ctx) {
            return Context::KernelFromHandle(ctx);
        }

        Context *ContextFromContextHandle(BML_Context ctx) {
            return Context::FromHandle(ctx);
        }

        const BML_Version *BML_API_GetRuntimeVersion(BML_Context ctx) {
            thread_local BML_Version versionSnapshot{};
            auto *context = ContextFromContextHandle(ctx);
            if (!context) {
                return nullptr;
            }
            versionSnapshot = context->GetRuntimeVersionCopy();
            return &versionSnapshot;
        }

        uint32_t BML_API_GetLoadedModuleCount(BML_Context ctx) {
            auto *context = ContextFromContextHandle(ctx);
            return context ? context->GetLoadedModuleCount() : 0;
        }

        BML_Mod BML_API_GetLoadedModuleAt(BML_Context ctx, uint32_t index) {
            auto *context = ContextFromContextHandle(ctx);
            return context ? context->GetLoadedModuleAt(index) : nullptr;
        }

        BML_Mod BML_API_FindModuleById(BML_Context ctx, const char *id) {
            if (!id) {
                return nullptr;
            }
            auto *context = ContextFromContextHandle(ctx);
            return context ? context->GetModHandleById(id) : nullptr;
        }

        BML_Result BML_API_ImcGetTopicId(BML_Context ctx,
                                                const char *name,
                                                BML_TopicId *out_id) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (!kernel) {
                return BML_RESULT_INVALID_CONTEXT;
            }
            return ImcGetTopicId(*kernel, name, out_id);
        }

        void BML_API_ImcPump(BML_Context ctx, size_t max_per_sub) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel) {
                ImcPump(*kernel, max_per_sub);
            }
        }

        BML_Result BML_API_ImcGetStats(BML_Context ctx, BML_ImcStats *out_stats) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (!kernel) {
                return BML_RESULT_INVALID_CONTEXT;
            }
            return ImcGetStats(*kernel, out_stats);
        }

        BML_Result BML_API_ImcResetStats(BML_Context ctx) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (!kernel) {
                return BML_RESULT_INVALID_CONTEXT;
            }
            return ImcResetStats(*kernel);
        }

        BML_Result BML_API_ImcGetTopicInfo(BML_Context ctx,
                                                  BML_TopicId topic,
                                                  BML_TopicInfo *out_info) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (!kernel) {
                return BML_RESULT_INVALID_CONTEXT;
            }
            return ImcGetTopicInfo(*kernel, topic, out_info);
        }

        BML_Result BML_API_ImcGetTopicName(BML_Context ctx,
                                                  BML_TopicId topic,
                                                  char *buffer,
                                                  size_t buffer_size,
                                                  size_t *out_length) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (!kernel) {
                return BML_RESULT_INVALID_CONTEXT;
            }
            return ImcGetTopicName(*kernel, topic, buffer, buffer_size, out_length);
        }

        BML_Result BML_API_ImcCopyState(BML_Context ctx,
                                               BML_TopicId topic,
                                               void *dst,
                                               size_t dst_size,
                                               size_t *out_size,
                                               BML_ImcStateMeta *out_meta) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (!kernel) {
                return BML_RESULT_INVALID_CONTEXT;
            }
            return ImcCopyState(*kernel, topic, dst, dst_size, out_size, out_meta);
        }

        BML_Result BML_API_ImcClearState(BML_Context ctx,
                                                BML_TopicId topic) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (!kernel) {
                return BML_RESULT_INVALID_CONTEXT;
            }
            return ImcClearState(*kernel, topic);
        }

        BML_Result BML_API_ImcGetRpcId(BML_Context ctx,
                                              const char *name,
                                              BML_RpcId *out_id) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (!kernel) {
                return BML_RESULT_INVALID_CONTEXT;
            }
            return ImcGetRpcId(*kernel, name, out_id);
        }

        BML_Result BML_API_ImcGetRpcInfo(BML_Context ctx,
                                                BML_RpcId rpc_id,
                                                BML_RpcInfo *out_info) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (!kernel) {
                return BML_RESULT_INVALID_CONTEXT;
            }
            return ImcGetRpcInfo(*kernel, rpc_id, out_info);
        }

        BML_Result BML_API_ImcGetRpcName(BML_Context ctx,
                                                BML_RpcId rpc_id,
                                                char *buffer,
                                                size_t cap,
                                                size_t *out_len) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (!kernel) {
                return BML_RESULT_INVALID_CONTEXT;
            }
            return ImcGetRpcName(*kernel, rpc_id, buffer, cap, out_len);
        }

        void BML_API_ImcEnumerateRpc(
            BML_Context ctx,
            void (*callback)(BML_RpcId rpc_id, const char *name, BML_Bool is_streaming, void *user_data),
            void *user_data) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel) {
                ImcEnumerateRpc(*kernel, callback, user_data);
            }
        }

        BML_Result BML_API_GetModAssetMount(BML_Mod mod, const char **out_mount) {
            if (!out_mount) {
                return BML_RESULT_INVALID_ARGUMENT;
            }
            auto *context = Context::ContextFromMod(mod);
            if (!context) {
                return BML_RESULT_INVALID_CONTEXT;
            }
            auto *handle = context->ResolveModHandle(mod);
            if (!handle || !handle->manifest) {
                return BML_RESULT_INVALID_HANDLE;
            }
            *out_mount = handle->manifest->assets.mount.empty()
                ? nullptr
                : handle->manifest->assets.mount.c_str();
            return BML_RESULT_OK;
        }

        BML_Result BML_API_RegisterLogSinkOverride(BML_Mod /*owner*/,
                                                          const BML_LogSinkOverrideDesc *desc) {
            return RegisterLogSinkOverride(desc);
        }

        BML_Result BML_API_ClearLogSinkOverride(BML_Mod /*owner*/) {
            return ClearLogSinkOverride();
        }

        BML_Result BML_API_GetLastError(BML_Context ctx, BML_ErrorInfo *out_error) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->diagnostics) ? kernel->diagnostics->GetLastError(out_error)
                                                   : BML_RESULT_INVALID_CONTEXT;
        }

        void BML_API_ClearLastError(BML_Context ctx) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->diagnostics) {
                kernel->diagnostics->ClearLastError();
            }
        }

        BML_Bool BML_API_DiagInterfaceExists(BML_Context ctx, const char *interface_id) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->interface_registry &&
                    kernel->interface_registry->Exists(interface_id))
                ? BML_TRUE
                : BML_FALSE;
        }

        BML_Bool BML_API_DiagGetInterfaceDescriptor(BML_Context ctx,
                                                           const char *interface_id,
                                                           BML_InterfaceRuntimeDesc *out_desc) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (!kernel || !kernel->interface_registry) {
                return BML_FALSE;
            }
            return kernel->interface_registry->GetDescriptor(interface_id, out_desc) == BML_RESULT_OK
                ? BML_TRUE
                : BML_FALSE;
        }

        BML_Bool BML_API_DiagIsInterfaceCompatible(BML_Context ctx,
                                                          const char *interface_id,
                                                          const BML_Version *required_abi) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->interface_registry &&
                    kernel->interface_registry->IsCompatible(interface_id, required_abi))
                ? BML_TRUE
                : BML_FALSE;
        }

        uint32_t BML_API_DiagGetInterfaceCount(BML_Context ctx) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->interface_registry) ? kernel->interface_registry->GetCount() : 0;
        }

        uint32_t BML_API_DiagGetLeaseCount(BML_Context ctx, const char *interface_id) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->interface_registry)
                ? kernel->interface_registry->GetLeaseCount(interface_id)
                : 0;
        }

        void BML_API_DiagEnumerateInterfaces(BML_Context ctx,
                                                    PFN_BML_InterfaceRuntimeEnumerator callback,
                                                    void *user_data,
                                                    uint64_t required_flags_mask) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->interface_registry) {
                kernel->interface_registry->Enumerate(callback, user_data, required_flags_mask);
            }
        }

        void BML_API_DiagEnumerateByProvider(BML_Context ctx,
                                                    const char *provider_id,
                                                    PFN_BML_InterfaceRuntimeEnumerator callback,
                                                    void *user_data) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->interface_registry) {
                kernel->interface_registry->EnumerateByProvider(provider_id, callback, user_data);
            }
        }

        void BML_API_DiagEnumerateByCapability(BML_Context ctx,
                                                      uint64_t required_caps,
                                                      PFN_BML_InterfaceRuntimeEnumerator callback,
                                                      void *user_data) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->interface_registry) {
                kernel->interface_registry->EnumerateByCapability(
                    required_caps, callback, user_data);
            }
        }

        BML_Result BML_API_LocaleSetLanguage(BML_Context ctx, const char *language_code) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (!kernel || !kernel->locale) {
                return BML_RESULT_INVALID_CONTEXT;
            }
            BML_Result result = kernel->locale->SetLanguage(language_code);
            if (result == BML_RESULT_OK) {
                kernel->locale->ReloadAll();
            }
            return result;
        }

        BML_Result BML_API_LocaleGetLanguage(BML_Context ctx, const char **out_code) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->locale) ? kernel->locale->GetLanguage(out_code)
                                              : BML_RESULT_INVALID_CONTEXT;
        }

        const char *BML_API_LocaleLookup(BML_Context ctx,
                                                BML_LocaleTable table,
                                                const char *key) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->locale) ? kernel->locale->Lookup(table, key) : nullptr;
        }

        void *BML_API_Alloc(BML_Context ctx, size_t size) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->memory) ? kernel->memory->Alloc(size) : nullptr;
        }

        void *BML_API_Calloc(BML_Context ctx, size_t count, size_t size) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->memory) ? kernel->memory->Calloc(count, size) : nullptr;
        }

        void *BML_API_Realloc(BML_Context ctx, void *ptr, size_t old_size, size_t new_size) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->memory)
                ? kernel->memory->Realloc(ptr, old_size, new_size)
                : nullptr;
        }

        void BML_API_Free(BML_Context ctx, void *ptr) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->memory) {
                kernel->memory->Free(ptr);
            }
        }

        void *BML_API_AllocAligned(BML_Context ctx, size_t size, size_t alignment) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->memory) ? kernel->memory->AllocAligned(size, alignment)
                                              : nullptr;
        }

        void BML_API_FreeAligned(BML_Context ctx, void *ptr) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->memory) {
                kernel->memory->FreeAligned(ptr);
            }
        }

        BML_Result BML_API_MemoryPoolCreate(BML_Context ctx,
                                                   size_t block_size,
                                                   uint32_t initial_blocks,
                                                   BML_MemoryPool *out_pool) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->memory)
                ? kernel->memory->CreatePool(block_size, initial_blocks, out_pool)
                : BML_RESULT_INVALID_CONTEXT;
        }

        void *BML_API_MemoryPoolAlloc(BML_Context ctx, BML_MemoryPool pool) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->memory) ? kernel->memory->PoolAlloc(pool) : nullptr;
        }

        void BML_API_MemoryPoolFree(BML_Context ctx, BML_MemoryPool pool, void *ptr) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->memory) {
                kernel->memory->PoolFree(pool, ptr);
            }
        }

        void BML_API_MemoryPoolDestroy(BML_Context ctx, BML_MemoryPool pool) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->memory) {
                kernel->memory->DestroyPool(pool);
            }
        }

        BML_Result BML_API_GetMemoryStats(BML_Context ctx, BML_MemoryStats *out_stats) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->memory) ? kernel->memory->GetStats(out_stats)
                                              : BML_RESULT_INVALID_CONTEXT;
        }

        BML_Result BML_API_MutexCreate(BML_Context ctx, BML_Mod owner, BML_Mutex *out_mutex) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync) ? kernel->sync->CreateMutex(owner, out_mutex)
                                            : BML_RESULT_INVALID_CONTEXT;
        }

        void BML_API_MutexDestroy(BML_Context ctx, BML_Mutex mutex) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->sync) {
                kernel->sync->DestroyMutex(mutex);
            }
        }

        void BML_API_MutexLock(BML_Context ctx, BML_Mutex mutex) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->sync) {
                kernel->sync->LockMutex(mutex);
            }
        }

        BML_Bool BML_API_MutexTryLock(BML_Context ctx, BML_Mutex mutex) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync) ? kernel->sync->TryLockMutex(mutex) : BML_FALSE;
        }

        BML_Result BML_API_MutexLockTimeout(BML_Context ctx,
                                                   BML_Mutex mutex,
                                                   uint32_t timeout_ms) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync) ? kernel->sync->LockMutexTimeout(mutex, timeout_ms)
                                            : BML_RESULT_INVALID_CONTEXT;
        }

        void BML_API_MutexUnlock(BML_Context ctx, BML_Mutex mutex) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->sync) {
                kernel->sync->UnlockMutex(mutex);
            }
        }

        BML_Result BML_API_RwLockCreate(BML_Context ctx, BML_Mod owner, BML_RwLock *out_lock) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync) ? kernel->sync->CreateRwLock(owner, out_lock)
                                            : BML_RESULT_INVALID_CONTEXT;
        }

        void BML_API_RwLockDestroy(BML_Context ctx, BML_RwLock lock) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->sync) {
                kernel->sync->DestroyRwLock(lock);
            }
        }

        void BML_API_RwLockReadLock(BML_Context ctx, BML_RwLock lock) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->sync) {
                kernel->sync->ReadLockRwLock(lock);
            }
        }

        BML_Bool BML_API_RwLockTryReadLock(BML_Context ctx, BML_RwLock lock) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync) ? kernel->sync->TryReadLockRwLock(lock) : BML_FALSE;
        }

        BML_Result BML_API_RwLockReadLockTimeout(BML_Context ctx,
                                                        BML_RwLock lock,
                                                        uint32_t timeout_ms) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync) ? kernel->sync->ReadLockRwLockTimeout(lock, timeout_ms)
                                            : BML_RESULT_INVALID_CONTEXT;
        }

        void BML_API_RwLockWriteLock(BML_Context ctx, BML_RwLock lock) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->sync) {
                kernel->sync->WriteLockRwLock(lock);
            }
        }

        BML_Bool BML_API_RwLockTryWriteLock(BML_Context ctx, BML_RwLock lock) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync) ? kernel->sync->TryWriteLockRwLock(lock) : BML_FALSE;
        }

        BML_Result BML_API_RwLockWriteLockTimeout(BML_Context ctx,
                                                         BML_RwLock lock,
                                                         uint32_t timeout_ms) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync) ? kernel->sync->WriteLockRwLockTimeout(lock, timeout_ms)
                                            : BML_RESULT_INVALID_CONTEXT;
        }

        void BML_API_RwLockUnlock(BML_Context ctx, BML_RwLock lock) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->sync) {
                kernel->sync->UnlockRwLock(lock);
            }
        }

        void BML_API_RwLockReadUnlock(BML_Context ctx, BML_RwLock lock) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->sync) {
                kernel->sync->ReadUnlockRwLock(lock);
            }
        }

        void BML_API_RwLockWriteUnlock(BML_Context ctx, BML_RwLock lock) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->sync) {
                kernel->sync->WriteUnlockRwLock(lock);
            }
        }

        BML_Result BML_API_SemaphoreCreate(BML_Context ctx,
                                                  BML_Mod owner,
                                                  uint32_t initial_count,
                                                  uint32_t max_count,
                                                  BML_Semaphore *out_semaphore) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync)
                ? kernel->sync->CreateSemaphore(owner, initial_count, max_count, out_semaphore)
                : BML_RESULT_INVALID_CONTEXT;
        }

        void BML_API_SemaphoreDestroy(BML_Context ctx, BML_Semaphore semaphore) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->sync) {
                kernel->sync->DestroySemaphore(semaphore);
            }
        }

        BML_Result BML_API_SemaphoreWait(BML_Context ctx,
                                                BML_Semaphore semaphore,
                                                uint32_t timeout_ms) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync) ? kernel->sync->WaitSemaphore(semaphore, timeout_ms)
                                            : BML_RESULT_INVALID_CONTEXT;
        }

        BML_Result BML_API_SemaphoreSignal(BML_Context ctx,
                                                  BML_Semaphore semaphore,
                                                  uint32_t count) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync) ? kernel->sync->SignalSemaphore(semaphore, count)
                                            : BML_RESULT_INVALID_CONTEXT;
        }

        BML_Result BML_API_TlsCreate(BML_Context ctx,
                                            BML_Mod owner,
                                            BML_TlsDestructor destructor,
                                            BML_TlsKey *out_key) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync) ? kernel->sync->CreateTls(owner, destructor, out_key)
                                            : BML_RESULT_INVALID_CONTEXT;
        }

        void BML_API_TlsDestroy(BML_Context ctx, BML_TlsKey key) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->sync) {
                kernel->sync->DestroyTls(key);
            }
        }

        void *BML_API_TlsGet(BML_Context ctx, BML_TlsKey key) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync) ? kernel->sync->GetTls(key) : nullptr;
        }

        BML_Result BML_API_TlsSet(BML_Context ctx, BML_TlsKey key, void *value) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync) ? kernel->sync->SetTls(key, value)
                                            : BML_RESULT_INVALID_CONTEXT;
        }

        BML_Result BML_API_CondVarCreate(BML_Context ctx, BML_Mod owner, BML_CondVar *out_condvar) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync) ? kernel->sync->CreateCondVar(owner, out_condvar)
                                            : BML_RESULT_INVALID_CONTEXT;
        }

        void BML_API_CondVarDestroy(BML_Context ctx, BML_CondVar condvar) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->sync) {
                kernel->sync->DestroyCondVar(condvar);
            }
        }

        BML_Result BML_API_CondVarWait(BML_Context ctx, BML_CondVar condvar, BML_Mutex mutex) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync) ? kernel->sync->WaitCondVar(condvar, mutex)
                                            : BML_RESULT_INVALID_CONTEXT;
        }

        BML_Result BML_API_CondVarWaitTimeout(BML_Context ctx,
                                                     BML_CondVar condvar,
                                                     BML_Mutex mutex,
                                                     uint32_t timeout_ms) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync)
                ? kernel->sync->WaitCondVarTimeout(condvar, mutex, timeout_ms)
                : BML_RESULT_INVALID_CONTEXT;
        }

        BML_Result BML_API_CondVarSignal(BML_Context ctx, BML_CondVar condvar) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync) ? kernel->sync->SignalCondVar(condvar)
                                            : BML_RESULT_INVALID_CONTEXT;
        }

        BML_Result BML_API_CondVarBroadcast(BML_Context ctx, BML_CondVar condvar) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync) ? kernel->sync->BroadcastCondVar(condvar)
                                            : BML_RESULT_INVALID_CONTEXT;
        }

        BML_Result BML_API_SpinLockCreate(BML_Context ctx, BML_Mod owner, BML_SpinLock *out_lock) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync) ? kernel->sync->CreateSpinLock(owner, out_lock)
                                            : BML_RESULT_INVALID_CONTEXT;
        }

        void BML_API_SpinLockDestroy(BML_Context ctx, BML_SpinLock lock) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->sync) {
                kernel->sync->DestroySpinLock(lock);
            }
        }

        void BML_API_SpinLockLock(BML_Context ctx, BML_SpinLock lock) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->sync) {
                kernel->sync->LockSpinLock(lock);
            }
        }

        BML_Bool BML_API_SpinLockTryLock(BML_Context ctx, BML_SpinLock lock) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->sync) ? kernel->sync->TryLockSpinLock(lock) : BML_FALSE;
        }

        void BML_API_SpinLockUnlock(BML_Context ctx, BML_SpinLock lock) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->sync) {
                kernel->sync->UnlockSpinLock(lock);
            }
        }

        void BML_API_TraceBegin(BML_Context ctx, const char *name, const char *category) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->profiling) {
                kernel->profiling->TraceBegin(name, category);
            }
        }

        void BML_API_TraceEnd(BML_Context ctx) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->profiling) {
                kernel->profiling->TraceEnd();
            }
        }

        void BML_API_TraceInstant(BML_Context ctx, const char *name, const char *category) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->profiling) {
                kernel->profiling->TraceInstant(name, category);
            }
        }

        void BML_API_TraceSetThreadName(BML_Context ctx, const char *name) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->profiling) {
                kernel->profiling->TraceSetThreadName(name);
            }
        }

        void BML_API_TraceCounter(BML_Context ctx, const char *name, int64_t value) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->profiling) {
                kernel->profiling->TraceCounter(name, value);
            }
        }

        void BML_API_TraceFrameMark(BML_Context ctx) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (kernel && kernel->profiling) {
                kernel->profiling->TraceFrameMark();
            }
        }

        uint64_t BML_API_GetApiCallCount(BML_Context ctx, const char *api_name) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->profiling) ? kernel->profiling->GetApiCallCount(api_name) : 0;
        }

        uint64_t BML_API_GetTotalAllocBytes(BML_Context ctx) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->profiling) ? kernel->profiling->GetTotalAllocBytes() : 0;
        }

        uint64_t BML_API_GetTimestampNs(BML_Context ctx) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->profiling) ? kernel->profiling->GetTimestampNs() : 0;
        }

        uint64_t BML_API_GetCpuFrequency(BML_Context ctx) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->profiling) ? kernel->profiling->GetCpuFrequency() : 0;
        }

        BML_ProfilerBackend BML_API_GetProfilerBackend(BML_Context ctx) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->profiling) ? kernel->profiling->GetProfilerBackend()
                                                 : BML_PROFILER_NONE;
        }

        BML_Result BML_API_SetProfilingEnabled(BML_Context ctx, BML_Bool enable) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->profiling) ? kernel->profiling->SetProfilingEnabled(enable)
                                                 : BML_RESULT_INVALID_CONTEXT;
        }

        BML_Bool BML_API_IsProfilingEnabled(BML_Context ctx) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->profiling) ? kernel->profiling->IsProfilingEnabled()
                                                 : BML_FALSE;
        }

        BML_Result BML_API_FlushProfilingData(BML_Context ctx, const char *filename) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->profiling) ? kernel->profiling->FlushProfilingData(filename)
                                                 : BML_RESULT_INVALID_CONTEXT;
        }

        BML_Result BML_API_GetProfilingStats(BML_Context ctx,
                                                    BML_ProfilingStats *out_stats) {
            auto *kernel = KernelFromContextHandle(ctx);
            return (kernel && kernel->profiling) ? kernel->profiling->GetProfilingStats(out_stats)
                                                 : BML_RESULT_INVALID_CONTEXT;
        }

        BML_Result BML_API_RegisterHostContribution(BML_Mod provider_mod,
                                                    const char *host_interface_id,
                                                    BML_InterfaceRegistration *out_registration) {
            if (!provider_mod || !host_interface_id || !out_registration) {
                return BML_RESULT_INVALID_ARGUMENT;
            }

            auto *kernel = Context::KernelFromMod(provider_mod);
            auto *context = Context::ContextFromMod(provider_mod);
            if (!kernel || !context) {
                return BML_RESULT_INVALID_CONTEXT;
            }

            auto *provider = context->ResolveModHandle(provider_mod);
            if (!provider) {
                return BML_RESULT_INVALID_CONTEXT;
            }

            auto &interfaceRegistry = *kernel->interface_registry;
            auto &leases = *kernel->leases;

            BML_InterfaceRuntimeDesc desc = BML_INTERFACE_RUNTIME_DESC_INIT;
            BML_CHECK(interfaceRegistry.GetDescriptor(host_interface_id, &desc));
            if ((desc.flags & BML_INTERFACE_FLAG_HOST_OWNED) == 0) {
                return BML_RESULT_PERMISSION_DENIED;
            }
            if (leases.IsProviderBlocked(provider->id)) {
                return BML_RESULT_BUSY;
            }

            return leases.CreateInterfaceRegistration(
                kernel, host_interface_id, provider->id, provider->id, out_registration);
        }

        BML_Result BML_API_UnregisterHostContribution(BML_InterfaceRegistration registration) {
            auto *kernel = LeaseManager::KernelFromRegistration(registration);
            if (!kernel || !kernel->leases) {
                return BML_RESULT_INVALID_HANDLE;
            }
            return kernel->leases->ReleaseInterfaceRegistration(registration);
        }

        BML_Result BML_API_RegisterRuntimeProvider(
            BML_Context ctx,
            const BML_ModuleRuntimeProvider *provider,
            const char *owner_id) {
            auto *context = ContextFromContextHandle(ctx);
            return context ? context->RegisterRuntimeProvider(provider, owner_id)
                           : BML_RESULT_INVALID_CONTEXT;
        }

        BML_Result BML_API_UnregisterRuntimeProvider(
            BML_Context ctx,
            const BML_ModuleRuntimeProvider *provider) {
            auto *context = ContextFromContextHandle(ctx);
            return context ? context->UnregisterRuntimeProvider(provider)
                           : BML_RESULT_INVALID_CONTEXT;
        }

        BML_Result BML_API_CleanupModuleState(BML_Context ctx, BML_Mod mod) {
            auto *kernel = KernelFromContextHandle(ctx);
            auto *context = ContextFromContextHandle(ctx);
            if (!kernel || !context) {
                return BML_RESULT_INVALID_CONTEXT;
            }

            auto *handle = context->ResolveModHandle(mod);
            if (!handle) {
                return BML_RESULT_INVALID_HANDLE;
            }

            CleanupModuleKernelState(*kernel, handle->id, mod);
            return BML_RESULT_OK;
        }

        BML_Result BML_API_HookEnumerate(BML_Context ctx,
                                                BML_HookEnumCallback callback,
                                                void *user_data) {
            auto *kernel = KernelFromContextHandle(ctx);
            if (!kernel || !kernel->hooks) {
                return BML_RESULT_INVALID_CONTEXT;
            }
            return kernel->hooks->Enumerate(callback, user_data);
        }

        BML_Result RegisterRuntimeInterface(KernelServices &kernel,
                                            const char *interface_id,
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
            auto host = kernel.context ? kernel.context->GetSyntheticHostModule() : nullptr;
            return kernel.interface_registry->Register(&desc, kernel.context->ResolveModHandle(host));
        }
    } // namespace

    void RefreshRuntimeInterfaces(KernelServices &kernel) {
        auto &hub = *kernel.context->GetServiceHubMutable();

        hub.m_ContextInterface = {
            BML_IFACE_HEADER(BML_CoreContextInterface,
                             BML_CORE_CONTEXT_INTERFACE_ID,
                             1,
                             kCoreContextInterfaceMinor),
            kernel.context ? kernel.context->GetHandle() : nullptr,
            BML_API_GetRuntimeVersion,
            ResolveRawApi<PFN_BML_ContextRetain>(kernel, "bmlContextRetain"),
            ResolveRawApi<PFN_BML_ContextRelease>(kernel, "bmlContextRelease"),
            ResolveRawApi<PFN_BML_ContextSetUserData>(kernel, "bmlContextSetUserData"),
            ResolveRawApi<PFN_BML_ContextGetUserData>(kernel, "bmlContextGetUserData"),
        };

        hub.m_ModuleInterface = {
            BML_IFACE_HEADER(BML_CoreModuleInterface,
                             BML_CORE_MODULE_INTERFACE_ID,
                             2,
                             kCoreModuleInterfaceMinor),
            kernel.context ? kernel.context->GetHandle() : nullptr,
            ResolveRawApi<PFN_BML_GetModId>(kernel, "bmlGetModId"),
            ResolveRawApi<PFN_BML_GetModVersion>(kernel, "bmlGetModVersion"),
            ResolveRawApi<PFN_BML_RequestCapability>(kernel, "bmlRequestCapability"),
            ResolveRawApi<PFN_BML_CheckCapability>(kernel, "bmlCheckCapability"),
            ResolveRawApi<PFN_BML_RegisterShutdownHook>(kernel, "bmlRegisterShutdownHook"),
            BML_API_GetLoadedModuleCount,
            BML_API_GetLoadedModuleAt,
            ResolveRawApi<PFN_BML_GetManifestString>(kernel, "bmlGetManifestString"),
            ResolveRawApi<PFN_BML_GetManifestInt>(kernel, "bmlGetManifestInt"),
            ResolveRawApi<PFN_BML_GetManifestFloat>(kernel, "bmlGetManifestFloat"),
            ResolveRawApi<PFN_BML_GetManifestBool>(kernel, "bmlGetManifestBool"),
            ResolveRawApi<PFN_BML_GetModDirectory>(kernel, "bmlGetModDirectory"),
            ResolveRawApi<PFN_BML_GetModAssetMount>(kernel, "bmlGetModAssetMount"),
            ResolveRawApi<PFN_BML_GetModName>(kernel, "bmlGetModName"),
            ResolveRawApi<PFN_BML_GetModDescription>(kernel, "bmlGetModDescription"),
            ResolveRawApi<PFN_BML_GetModAuthorCount>(kernel, "bmlGetModAuthorCount"),
            ResolveRawApi<PFN_BML_GetModAuthorAt>(kernel, "bmlGetModAuthorAt"),
            BML_API_FindModuleById,
        };

        hub.m_LoggingInterface = {
            BML_IFACE_HEADER(BML_CoreLoggingInterface,
                             BML_CORE_LOGGING_INTERFACE_ID,
                             1,
                             kCoreLoggingInterfaceMinor),
            kernel.context ? kernel.context->GetHandle() : nullptr,
            ResolveRawApi<PFN_BML_Log>(kernel, "bmlLog"),
            ResolveRawApi<PFN_BML_LogVa>(kernel, "bmlLogVa"),
            ResolveRawApi<PFN_BML_SetLogFilter>(kernel, "bmlSetLogFilter"),
            BML_API_RegisterLogSinkOverride,
            BML_API_ClearLogSinkOverride,
        };

        hub.m_ConfigInterface = {
            BML_IFACE_HEADER(BML_CoreConfigInterface,
                             BML_CORE_CONFIG_INTERFACE_ID,
                             1,
                             kCoreConfigInterfaceMinor),
            kernel.context ? kernel.context->GetHandle() : nullptr,
            ResolveRawApi<PFN_BML_ConfigGet>(kernel, "bmlConfigGet"),
            ResolveRawApi<PFN_BML_ConfigSet>(kernel, "bmlConfigSet"),
            ResolveRawApi<PFN_BML_ConfigReset>(kernel, "bmlConfigReset"),
            ResolveRawApi<PFN_BML_ConfigEnumerate>(kernel, "bmlConfigEnumerate"),
            ResolveRawApi<PFN_BML_ConfigBatchBegin>(kernel, "bmlConfigBatchBegin"),
            ResolveRawApi<PFN_BML_ConfigBatchSet>(kernel, "bmlConfigBatchSet"),
            ResolveRawApi<PFN_BML_ConfigBatchCommit>(kernel, "bmlConfigBatchCommit"),
            ResolveRawApi<PFN_BML_ConfigBatchDiscard>(kernel, "bmlConfigBatchDiscard"),
            ResolveRawApi<PFN_BML_RegisterConfigLoadHooks>(kernel, "bmlRegisterConfigLoadHooks"),
            ResolveRawApi<PFN_BML_ConfigGetInt>(kernel, "bmlConfigGetInt"),
            ResolveRawApi<PFN_BML_ConfigGetFloat>(kernel, "bmlConfigGetFloat"),
            ResolveRawApi<PFN_BML_ConfigGetBool>(kernel, "bmlConfigGetBool"),
            ResolveRawApi<PFN_BML_ConfigGetString>(kernel, "bmlConfigGetString"),
        };

        hub.m_MemoryInterface = {
            BML_IFACE_HEADER(BML_CoreMemoryInterface,
                             BML_CORE_MEMORY_INTERFACE_ID,
                             1,
                             kCoreMemoryInterfaceMinor),
            kernel.context ? kernel.context->GetHandle() : nullptr,
            BML_API_Alloc,
            BML_API_Calloc,
            BML_API_Realloc,
            BML_API_Free,
            BML_API_AllocAligned,
            BML_API_FreeAligned,
            BML_API_MemoryPoolCreate,
            BML_API_MemoryPoolAlloc,
            BML_API_MemoryPoolFree,
            BML_API_MemoryPoolDestroy,
            BML_API_GetMemoryStats,
        };

        hub.m_ResourceInterface = {
            BML_IFACE_HEADER(BML_CoreResourceInterface,
                             BML_CORE_RESOURCE_INTERFACE_ID,
                             1,
                             kCoreResourceInterfaceMinor),
            kernel.context ? kernel.context->GetHandle() : nullptr,
            ResolveRawApi<PFN_BML_RegisterResourceType>(kernel, "bmlRegisterResourceType"),
            ResolveRawApi<PFN_BML_HandleCreate>(kernel, "bmlHandleCreate"),
            ResolveRawApi<PFN_BML_HandleRetain>(kernel, "bmlHandleRetain"),
            ResolveRawApi<PFN_BML_HandleRelease>(kernel, "bmlHandleRelease"),
            ResolveRawApi<PFN_BML_HandleValidate>(kernel, "bmlHandleValidate"),
            ResolveRawApi<PFN_BML_HandleAttachUserData>(kernel, "bmlHandleAttachUserData"),
            ResolveRawApi<PFN_BML_HandleGetUserData>(kernel, "bmlHandleGetUserData"),
        };

        hub.m_DiagnosticInterface = {
            BML_IFACE_HEADER(BML_CoreDiagnosticInterface,
                             BML_CORE_DIAGNOSTIC_INTERFACE_ID,
                             1,
                             kCoreDiagnosticInterfaceMinor),
            kernel.context ? kernel.context->GetHandle() : nullptr,
            BML_API_GetLastError,
            BML_API_ClearLastError,
            ResolveRawApi<PFN_BML_GetErrorString>(kernel, "bmlGetErrorString"),
            BML_API_DiagInterfaceExists,
            BML_API_DiagGetInterfaceDescriptor,
            BML_API_DiagIsInterfaceCompatible,
            BML_API_DiagGetInterfaceCount,
            BML_API_DiagGetLeaseCount,
            BML_API_DiagEnumerateInterfaces,
            BML_API_DiagEnumerateByProvider,
            BML_API_DiagEnumerateByCapability,
        };

        hub.m_InterfaceControlInterface = {
            BML_IFACE_HEADER(BML_CoreInterfaceControlInterface,
                             BML_CORE_INTERFACE_CONTROL_INTERFACE_ID,
                             1,
                             kCoreInterfaceControlMinor),
            kernel.context ? kernel.context->GetHandle() : nullptr,
            ResolveRawApi<PFN_BML_InterfaceRegister>(kernel, "bmlInterfaceRegister"),
            ResolveRawApi<PFN_BML_InterfaceAcquire>(kernel, "bmlInterfaceAcquire"),
            ResolveRawApi<PFN_BML_InterfaceRelease>(kernel, "bmlInterfaceRelease"),
            ResolveRawApi<PFN_BML_InterfaceAddRef>(kernel, "bmlInterfaceAddRef"),
            ResolveRawApi<PFN_BML_InterfaceUnregister>(kernel, "bmlInterfaceUnregister"),
        };

        hub.m_ImcBusInterface = {
            BML_IFACE_HEADER(BML_ImcBusInterface,
                             BML_IMC_BUS_INTERFACE_ID,
                             1,
                             kImcBusInterfaceMinor),
            kernel.context ? kernel.context->GetHandle() : nullptr,
            BML_API_ImcGetTopicId,
            ResolveRawApi<PFN_BML_ImcPublish>(kernel, "bmlImcPublish"),
            ResolveRawApi<PFN_BML_ImcPublishEx>(kernel, "bmlImcPublishEx"),
            ResolveRawApi<PFN_BML_ImcPublishBuffer>(kernel, "bmlImcPublishBuffer"),
            ResolveRawApi<PFN_BML_ImcPublishMulti>(kernel, "bmlImcPublishMulti"),
            ResolveRawApi<PFN_BML_ImcPublishInterceptable>(kernel, "bmlImcPublishInterceptable"),
            ResolveRawApi<PFN_BML_ImcSubscribe>(kernel, "bmlImcSubscribe"),
            ResolveRawApi<PFN_BML_ImcSubscribeEx>(kernel, "bmlImcSubscribeEx"),
            ResolveRawApi<PFN_BML_ImcSubscribeIntercept>(kernel, "bmlImcSubscribeIntercept"),
            ResolveRawApi<PFN_BML_ImcSubscribeInterceptEx>(kernel, "bmlImcSubscribeInterceptEx"),
            ResolveRawApi<PFN_BML_ImcUnsubscribe>(kernel, "bmlImcUnsubscribe"),
            ResolveRawApi<PFN_BML_ImcSubscriptionIsActive>(kernel, "bmlImcSubscriptionIsActive"),
            ResolveRawApi<PFN_BML_ImcPublishState>(kernel, "bmlImcPublishState"),
            BML_API_ImcCopyState,
            BML_API_ImcClearState,
            BML_API_ImcPump,
            ResolveRawApi<PFN_BML_ImcGetSubscriptionStats>(kernel, "bmlImcGetSubscriptionStats"),
            BML_API_ImcGetStats,
            BML_API_ImcResetStats,
            BML_API_ImcGetTopicInfo,
            BML_API_ImcGetTopicName,
        };

        hub.m_ImcRpcInterface = {
            BML_IFACE_HEADER(BML_ImcRpcInterface,
                             BML_IMC_RPC_INTERFACE_ID,
                             1,
                             kRpcInterfaceMinor),
            kernel.context ? kernel.context->GetHandle() : nullptr,
            BML_API_ImcGetRpcId,
            ResolveRawApi<PFN_BML_ImcRegisterRpc>(kernel, "bmlImcRegisterRpc"),
            ResolveRawApi<PFN_BML_ImcUnregisterRpc>(kernel, "bmlImcUnregisterRpc"),
            ResolveRawApi<PFN_BML_ImcCallRpc>(kernel, "bmlImcCallRpc"),
            ResolveRawApi<PFN_BML_ImcFutureAwait>(kernel, "bmlImcFutureAwait"),
            ResolveRawApi<PFN_BML_ImcFutureGetResult>(kernel, "bmlImcFutureGetResult"),
            ResolveRawApi<PFN_BML_ImcFutureGetState>(kernel, "bmlImcFutureGetState"),
            ResolveRawApi<PFN_BML_ImcFutureCancel>(kernel, "bmlImcFutureCancel"),
            ResolveRawApi<PFN_BML_ImcFutureOnComplete>(kernel, "bmlImcFutureOnComplete"),
            ResolveRawApi<PFN_BML_ImcFutureRelease>(kernel, "bmlImcFutureRelease"),
            ResolveRawApi<PFN_BML_ImcRegisterRpcEx>(kernel, "bmlImcRegisterRpcEx"),
            ResolveRawApi<PFN_BML_ImcCallRpcEx>(kernel, "bmlImcCallRpcEx"),
            ResolveRawApi<PFN_BML_ImcFutureGetError>(kernel, "bmlImcFutureGetError"),
            BML_API_ImcGetRpcInfo,
            BML_API_ImcGetRpcName,
            BML_API_ImcEnumerateRpc,
            ResolveRawApi<PFN_BML_ImcAddRpcMiddleware>(kernel, "bmlImcAddRpcMiddleware"),
            ResolveRawApi<PFN_BML_ImcRemoveRpcMiddleware>(kernel, "bmlImcRemoveRpcMiddleware"),
            ResolveRawApi<PFN_BML_ImcRegisterStreamingRpc>(kernel, "bmlImcRegisterStreamingRpc"),
            ResolveRawApi<PFN_BML_ImcStreamPush>(kernel, "bmlImcStreamPush"),
            ResolveRawApi<PFN_BML_ImcStreamComplete>(kernel, "bmlImcStreamComplete"),
            ResolveRawApi<PFN_BML_ImcStreamError>(kernel, "bmlImcStreamError"),
            ResolveRawApi<PFN_BML_ImcCallStreamingRpc>(kernel, "bmlImcCallStreamingRpc"),
        };

        hub.m_TimerInterface = {
            BML_IFACE_HEADER(BML_CoreTimerInterface,
                             BML_CORE_TIMER_INTERFACE_ID,
                             1,
                             kCoreTimerInterfaceMinor),
            kernel.context ? kernel.context->GetHandle() : nullptr,
            ResolveRawApi<PFN_BML_TimerScheduleOnce>(kernel, "bmlTimerScheduleOnce"),
            ResolveRawApi<PFN_BML_TimerScheduleRepeat>(kernel, "bmlTimerScheduleRepeat"),
            ResolveRawApi<PFN_BML_TimerScheduleFrames>(kernel, "bmlTimerScheduleFrames"),
            ResolveRawApi<PFN_BML_TimerCancel>(kernel, "bmlTimerCancel"),
            ResolveRawApi<PFN_BML_TimerIsActive>(kernel, "bmlTimerIsActive"),
            ResolveRawApi<PFN_BML_TimerCancelAll>(kernel, "bmlTimerCancelAll"),
        };

        hub.m_HookRegistryInterface = {
            BML_IFACE_HEADER(BML_CoreHookRegistryInterface,
                             BML_CORE_HOOK_REGISTRY_INTERFACE_ID,
                             2,
                             kCoreHookRegistryInterfaceMinor),
            kernel.context ? kernel.context->GetHandle() : nullptr,
            ResolveRawApi<PFN_BML_HookRegister>(kernel, "bmlHookRegister"),
            ResolveRawApi<PFN_BML_HookUnregister>(kernel, "bmlHookUnregister"),
            BML_API_HookEnumerate,
        };

        hub.m_LocaleInterface = {
            BML_IFACE_HEADER(BML_CoreLocaleInterface,
                             BML_CORE_LOCALE_INTERFACE_ID,
                             1,
                             kCoreLocaleInterfaceMinor),
            kernel.context ? kernel.context->GetHandle() : nullptr,
            ResolveRawApi<PFN_BML_LocaleLoad>(kernel, "bmlLocaleLoad"),
            BML_API_LocaleSetLanguage,
            BML_API_LocaleGetLanguage,
            ResolveRawApi<PFN_BML_LocaleGet>(kernel, "bmlLocaleGet"),
            ResolveRawApi<PFN_BML_LocaleBindTable>(kernel, "bmlLocaleBindTable"),
            BML_API_LocaleLookup,
        };

        hub.m_SyncInterface = {
            BML_IFACE_HEADER(BML_CoreSyncInterface,
                             BML_CORE_SYNC_INTERFACE_ID,
                             1,
                             kCoreSyncInterfaceMinor),
            kernel.context ? kernel.context->GetHandle() : nullptr,
            BML_API_MutexCreate,
            BML_API_MutexDestroy,
            BML_API_MutexLock,
            BML_API_MutexTryLock,
            BML_API_MutexLockTimeout,
            BML_API_MutexUnlock,
            BML_API_RwLockCreate,
            BML_API_RwLockDestroy,
            BML_API_RwLockReadLock,
            BML_API_RwLockTryReadLock,
            BML_API_RwLockReadLockTimeout,
            BML_API_RwLockWriteLock,
            BML_API_RwLockTryWriteLock,
            BML_API_RwLockWriteLockTimeout,
            BML_API_RwLockUnlock,
            BML_API_RwLockReadUnlock,
            BML_API_RwLockWriteUnlock,
            ResolveRawApi<PFN_BML_AtomicIncrement32>(kernel, "bmlAtomicIncrement32"),
            ResolveRawApi<PFN_BML_AtomicDecrement32>(kernel, "bmlAtomicDecrement32"),
            ResolveRawApi<PFN_BML_AtomicAdd32>(kernel, "bmlAtomicAdd32"),
            ResolveRawApi<PFN_BML_AtomicCompareExchange32>(kernel, "bmlAtomicCompareExchange32"),
            ResolveRawApi<PFN_BML_AtomicExchange32>(kernel, "bmlAtomicExchange32"),
            ResolveRawApi<PFN_BML_AtomicLoadPtr>(kernel, "bmlAtomicLoadPtr"),
            ResolveRawApi<PFN_BML_AtomicStorePtr>(kernel, "bmlAtomicStorePtr"),
            ResolveRawApi<PFN_BML_AtomicCompareExchangePtr>(kernel, "bmlAtomicCompareExchangePtr"),
            BML_API_SemaphoreCreate,
            BML_API_SemaphoreDestroy,
            BML_API_SemaphoreWait,
            BML_API_SemaphoreSignal,
            BML_API_TlsCreate,
            BML_API_TlsDestroy,
            BML_API_TlsGet,
            BML_API_TlsSet,
            BML_API_CondVarCreate,
            BML_API_CondVarDestroy,
            BML_API_CondVarWait,
            BML_API_CondVarWaitTimeout,
            BML_API_CondVarSignal,
            BML_API_CondVarBroadcast,
            BML_API_SpinLockCreate,
            BML_API_SpinLockDestroy,
            BML_API_SpinLockLock,
            BML_API_SpinLockTryLock,
            BML_API_SpinLockUnlock,
        };

        hub.m_ProfilingInterface = {
            BML_IFACE_HEADER(BML_CoreProfilingInterface,
                             BML_CORE_PROFILING_INTERFACE_ID,
                             1,
                             kCoreProfilingInterfaceMinor),
            kernel.context ? kernel.context->GetHandle() : nullptr,
            BML_API_TraceBegin,
            BML_API_TraceEnd,
            BML_API_TraceInstant,
            BML_API_TraceSetThreadName,
            BML_API_TraceCounter,
            BML_API_TraceFrameMark,
            BML_API_GetApiCallCount,
            BML_API_GetTotalAllocBytes,
            BML_API_GetTimestampNs,
            BML_API_GetCpuFrequency,
            BML_API_GetProfilerBackend,
            BML_API_SetProfilingEnabled,
            BML_API_IsProfilingEnabled,
            BML_API_FlushProfilingData,
            BML_API_GetProfilingStats,
        };

        hub.m_HostRuntimeInterface = {
            BML_IFACE_HEADER(BML_HostRuntimeInterface,
                             BML_CORE_HOST_RUNTIME_INTERFACE_ID,
                             2,
                             kHostRuntimeInterfaceMinor),
            kernel.context ? kernel.context->GetHandle() : nullptr,
            BML_API_RegisterHostContribution,
            BML_API_UnregisterHostContribution,
            BML_API_RegisterRuntimeProvider,
            BML_API_UnregisterRuntimeProvider,
            BML_API_CleanupModuleState,
        };
    }

    void RegisterRuntimeInterfaces(KernelServices &kernel) {
        auto &hub = *kernel.context->GetServiceHubMutable();
        RefreshRuntimeInterfaces(kernel);

        RegisterRuntimeInterface(kernel,
                                 BML_CORE_CONTEXT_INTERFACE_ID,
                                 &hub.m_ContextInterface,
                                 sizeof(hub.m_ContextInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreContextInterfaceMinor, 0));
        RegisterRuntimeInterface(kernel,
                                 BML_CORE_MODULE_INTERFACE_ID,
                                 &hub.m_ModuleInterface,
                                 sizeof(hub.m_ModuleInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(2, kCoreModuleInterfaceMinor, 0));
        RegisterRuntimeInterface(kernel,
                                 BML_CORE_LOGGING_INTERFACE_ID,
                                 &hub.m_LoggingInterface,
                                 sizeof(hub.m_LoggingInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreLoggingInterfaceMinor, 0));
        RegisterRuntimeInterface(kernel,
                                 BML_CORE_CONFIG_INTERFACE_ID,
                                 &hub.m_ConfigInterface,
                                 sizeof(hub.m_ConfigInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreConfigInterfaceMinor, 0));
        RegisterRuntimeInterface(kernel,
                                 BML_CORE_MEMORY_INTERFACE_ID,
                                 &hub.m_MemoryInterface,
                                 sizeof(hub.m_MemoryInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreMemoryInterfaceMinor, 0));
        RegisterRuntimeInterface(kernel,
                                 BML_CORE_RESOURCE_INTERFACE_ID,
                                 &hub.m_ResourceInterface,
                                 sizeof(hub.m_ResourceInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreResourceInterfaceMinor, 0));
        RegisterRuntimeInterface(kernel,
                                 BML_CORE_DIAGNOSTIC_INTERFACE_ID,
                                 &hub.m_DiagnosticInterface,
                                 sizeof(hub.m_DiagnosticInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreDiagnosticInterfaceMinor, 0));
        RegisterRuntimeInterface(kernel,
                                 BML_CORE_INTERFACE_CONTROL_INTERFACE_ID,
                                 &hub.m_InterfaceControlInterface,
                                 sizeof(hub.m_InterfaceControlInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE | BML_INTERFACE_FLAG_INTERNAL,
                                 bmlMakeVersion(1, kCoreInterfaceControlMinor, 0));
        RegisterRuntimeInterface(kernel,
                                 BML_IMC_BUS_INTERFACE_ID,
                                 &hub.m_ImcBusInterface,
                                 sizeof(hub.m_ImcBusInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kImcBusInterfaceMinor, 0));
        RegisterRuntimeInterface(kernel,
                                 BML_IMC_RPC_INTERFACE_ID,
                                 &hub.m_ImcRpcInterface,
                                 sizeof(hub.m_ImcRpcInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kRpcInterfaceMinor, 0));
        RegisterRuntimeInterface(kernel,
                                 BML_CORE_TIMER_INTERFACE_ID,
                                 &hub.m_TimerInterface,
                                 sizeof(hub.m_TimerInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreTimerInterfaceMinor, 0));
        RegisterRuntimeInterface(kernel,
                                 BML_CORE_HOOK_REGISTRY_INTERFACE_ID,
                                 &hub.m_HookRegistryInterface,
                                 sizeof(hub.m_HookRegistryInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(2, kCoreHookRegistryInterfaceMinor, 0));
        RegisterRuntimeInterface(kernel,
                                 BML_CORE_LOCALE_INTERFACE_ID,
                                 &hub.m_LocaleInterface,
                                 sizeof(hub.m_LocaleInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreLocaleInterfaceMinor, 0));
        RegisterRuntimeInterface(kernel,
                                 BML_CORE_SYNC_INTERFACE_ID,
                                 &hub.m_SyncInterface,
                                 sizeof(hub.m_SyncInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreSyncInterfaceMinor, 0));
        RegisterRuntimeInterface(kernel,
                                 BML_CORE_PROFILING_INTERFACE_ID,
                                 &hub.m_ProfilingInterface,
                                 sizeof(hub.m_ProfilingInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE,
                                 bmlMakeVersion(1, kCoreProfilingInterfaceMinor, 0));
        RegisterRuntimeInterface(kernel,
                                 BML_CORE_HOST_RUNTIME_INTERFACE_ID,
                                 &hub.m_HostRuntimeInterface,
                                 sizeof(hub.m_HostRuntimeInterface),
                                 BML_INTERFACE_FLAG_IMMUTABLE | BML_INTERFACE_FLAG_INTERNAL,
                                 bmlMakeVersion(2, kHostRuntimeInterfaceMinor, 0));
    }
} // namespace BML::Core

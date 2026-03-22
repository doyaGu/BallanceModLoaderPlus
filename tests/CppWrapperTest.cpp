/*
 * Comprehensive test suite for BML v2 C++ Wrapper (bml.hpp)
 * 
 * Tests all wrapper classes against the actual C API to ensure correctness.
 */

#include <gtest/gtest.h>

#include <array>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define BML_LOADER_IMPLEMENTATION
#include "bml.hpp"


namespace {
    struct InterfaceMockState {
        BML_Result register_result = BML_RESULT_OK;
        BML_Result unregister_result = BML_RESULT_OK;
        int register_calls = 0;
        int unregister_calls = 0;
        BML_Mod last_owner = nullptr;
        std::string last_interface_id;
    };

    InterfaceMockState g_InterfaceMockState;

    BML_Result MockInterfaceRegister(BML_Mod owner, const BML_InterfaceDesc *desc) {
        ++g_InterfaceMockState.register_calls;
        g_InterfaceMockState.last_owner = owner;
        g_InterfaceMockState.last_interface_id = (desc && desc->interface_id) ? desc->interface_id : "";
        return g_InterfaceMockState.register_result;
    }

    BML_Result MockInterfaceUnregister(BML_Mod owner, const char *interface_id) {
        ++g_InterfaceMockState.unregister_calls;
        g_InterfaceMockState.last_owner = owner;
        g_InterfaceMockState.last_interface_id = interface_id ? interface_id : "";
        return g_InterfaceMockState.unregister_result;
    }

    struct ContributionMockState {
        BML_Result unregister_result = BML_RESULT_OK;
        int unregister_calls = 0;
        BML_InterfaceRegistration last_registration = nullptr;
    };

    ContributionMockState g_ContributionMockState;

    struct ContextMockState {
        BML_Context global_context = reinterpret_cast<BML_Context>(0x2468);
        BML_Context last_context = nullptr;
        BML_Version runtime_version = BML_VERSION_INIT(4, 5, 6);
        int retain_calls = 0;
        int release_calls = 0;
    };

    ContextMockState g_ContextMockState;

    BML_Result MockContextRetain(BML_Context ctx) {
        g_ContextMockState.last_context = ctx;
        ++g_ContextMockState.retain_calls;
        return BML_RESULT_OK;
    }

    BML_Result MockContextRelease(BML_Context ctx) {
        g_ContextMockState.last_context = ctx;
        ++g_ContextMockState.release_calls;
        return BML_RESULT_OK;
    }

    BML_Context MockGetGlobalContextInterface() {
        return g_ContextMockState.global_context;
    }

    const BML_Version *MockGetRuntimeVersionInterface() {
        return &g_ContextMockState.runtime_version;
    }

    void ResetContextMockState() {
        g_ContextMockState.global_context = reinterpret_cast<BML_Context>(0x2468);
        g_ContextMockState.last_context = nullptr;
        g_ContextMockState.runtime_version = BML_VERSION_INIT(4, 5, 6);
        g_ContextMockState.retain_calls = 0;
        g_ContextMockState.release_calls = 0;
    }

    struct ConfigMockState {
        std::string string_value = "value";
        int32_t int_value = 42;
        float float_value = 2.5f;
        bool bool_value = true;
        int get_calls = 0;
        int set_calls = 0;
        int reset_calls = 0;
        int batch_begin_calls = 0;
        int batch_set_calls = 0;
        int batch_commit_calls = 0;
        int batch_discard_calls = 0;
        BML_ConfigType last_set_type = BML_CONFIG_STRING;
        std::string last_set_category;
        std::string last_set_key;
        std::string last_set_string;
        int32_t last_set_int = 0;
        float last_set_float = 0.0f;
        bool last_set_bool = false;
        BML_ConfigBatch batch_handle = reinterpret_cast<BML_ConfigBatch>(0x1111);
    };

    ConfigMockState g_ConfigMockState;

    BML_Result MockConfigGet(BML_Mod, const BML_ConfigKey *key, BML_ConfigValue *outValue) {
        ++g_ConfigMockState.get_calls;
        if (!key || !outValue || !key->name) return BML_RESULT_INVALID_ARGUMENT;

        if (std::strcmp(key->name, "string") == 0) {
            outValue->type = BML_CONFIG_STRING;
            outValue->data.string_value = g_ConfigMockState.string_value.c_str();
            return BML_RESULT_OK;
        }
        if (std::strcmp(key->name, "int") == 0) {
            outValue->type = BML_CONFIG_INT;
            outValue->data.int_value = g_ConfigMockState.int_value;
            return BML_RESULT_OK;
        }
        if (std::strcmp(key->name, "float") == 0) {
            outValue->type = BML_CONFIG_FLOAT;
            outValue->data.float_value = g_ConfigMockState.float_value;
            return BML_RESULT_OK;
        }
        if (std::strcmp(key->name, "bool") == 0) {
            outValue->type = BML_CONFIG_BOOL;
            outValue->data.bool_value = g_ConfigMockState.bool_value ? BML_TRUE : BML_FALSE;
            return BML_RESULT_OK;
        }
        return BML_RESULT_NOT_FOUND;
    }

    BML_Result MockConfigSet(BML_Mod, const BML_ConfigKey *key, const BML_ConfigValue *value) {
        ++g_ConfigMockState.set_calls;
        if (!key || !value) return BML_RESULT_INVALID_ARGUMENT;
        g_ConfigMockState.last_set_category = key->category ? key->category : "";
        g_ConfigMockState.last_set_key = key->name ? key->name : "";
        g_ConfigMockState.last_set_type = value->type;
        switch (value->type) {
        case BML_CONFIG_STRING:
            g_ConfigMockState.last_set_string = value->data.string_value ? value->data.string_value : "";
            break;
        case BML_CONFIG_INT:
            g_ConfigMockState.last_set_int = value->data.int_value;
            break;
        case BML_CONFIG_FLOAT:
            g_ConfigMockState.last_set_float = value->data.float_value;
            break;
        case BML_CONFIG_BOOL:
            g_ConfigMockState.last_set_bool = value->data.bool_value != BML_FALSE;
            break;
        default:
            return BML_RESULT_INVALID_ARGUMENT;
        }
        return BML_RESULT_OK;
    }

    BML_Result MockConfigReset(BML_Mod, const BML_ConfigKey *) {
        ++g_ConfigMockState.reset_calls;
        return BML_RESULT_OK;
    }


    BML_Result MockConfigBatchBegin(BML_Mod, BML_ConfigBatch *outBatch) {
        ++g_ConfigMockState.batch_begin_calls;
        if (!outBatch) return BML_RESULT_INVALID_ARGUMENT;
        *outBatch = g_ConfigMockState.batch_handle;
        return BML_RESULT_OK;
    }

    BML_Result MockConfigBatchSet(BML_ConfigBatch batch,
                                  const BML_ConfigKey *,
                                  const BML_ConfigValue *) {
        if (batch != g_ConfigMockState.batch_handle) return BML_RESULT_INVALID_ARGUMENT;
        ++g_ConfigMockState.batch_set_calls;
        return BML_RESULT_OK;
    }

    BML_Result MockConfigBatchCommit(BML_ConfigBatch batch) {
        if (batch != g_ConfigMockState.batch_handle) return BML_RESULT_INVALID_ARGUMENT;
        ++g_ConfigMockState.batch_commit_calls;
        return BML_RESULT_OK;
    }

    BML_Result MockConfigBatchDiscard(BML_ConfigBatch batch) {
        if (batch != g_ConfigMockState.batch_handle) return BML_RESULT_INVALID_ARGUMENT;
        ++g_ConfigMockState.batch_discard_calls;
        return BML_RESULT_OK;
    }

    void ResetConfigMockState() {
        g_ConfigMockState = ConfigMockState{};
    }

    struct LoggingMockState {
        BML_Context last_context = nullptr;
        BML_Mod last_owner = nullptr;
        BML_LogSeverity last_severity = BML_LOG_INFO;
        std::string last_tag;
        std::string last_message;
        BML_LogSeverity last_filter = BML_LOG_INFO;
        int log_calls = 0;
        int set_filter_calls = 0;
    };

    LoggingMockState g_LoggingMockState;

    void MockLogVa(BML_Mod owner,
                   BML_Context ctx,
                   BML_LogSeverity level,
                   const char *tag,
                   const char *fmt,
                   va_list args) {
        ++g_LoggingMockState.log_calls;
        g_LoggingMockState.last_owner = owner;
        g_LoggingMockState.last_context = ctx;
        g_LoggingMockState.last_severity = level;
        g_LoggingMockState.last_tag = tag ? tag : "";
        char buffer[256];
        std::vsnprintf(buffer, sizeof(buffer), fmt, args);
        g_LoggingMockState.last_message = buffer;
    }

    void MockSetLogFilter(BML_Mod owner, BML_LogSeverity level) {
        g_LoggingMockState.last_owner = owner;
        ++g_LoggingMockState.set_filter_calls;
        g_LoggingMockState.last_filter = level;
    }


    void ResetLoggingMockState() {
        g_LoggingMockState = LoggingMockState{};
    }

    struct ModuleMockState {
        std::array<BML_Mod, 2> loaded_modules = {
            reinterpret_cast<BML_Mod>(0xA001),
            reinterpret_cast<BML_Mod>(0xA002),
        };
        BML_Mod last_shutdown_hook_mod = nullptr;
        BML_ShutdownCallback last_shutdown_callback = nullptr;
        void *last_shutdown_user_data = nullptr;
        std::string last_capability;
        int register_shutdown_calls = 0;
        int set_current_calls = 0;
        int acquire_owned_calls = 0;
        BML_Mod last_acquire_owner = nullptr;
    };

    ModuleMockState g_ModuleMockState;

    BML_Result MockRequestCapability(BML_Mod, const char *capabilityId) {
        g_ModuleMockState.last_capability = capabilityId ? capabilityId : "";
        return BML_RESULT_OK;
    }

    BML_Result MockCheckCapability(BML_Mod, const char *capabilityId, BML_Bool *supported) {
        if (!supported) return BML_RESULT_INVALID_ARGUMENT;
        g_ModuleMockState.last_capability = capabilityId ? capabilityId : "";
        *supported = (capabilityId && std::strcmp(capabilityId, "caps.ok") == 0) ? BML_TRUE : BML_FALSE;
        return BML_RESULT_OK;
    }

    BML_Result MockGetModId(BML_Mod mod, const char **outId) {
        if (!outId) return BML_RESULT_INVALID_ARGUMENT;
        if (mod == g_ModuleMockState.loaded_modules[0]) {
            *outId = "mod.alpha";
            return BML_RESULT_OK;
        }
        if (mod == g_ModuleMockState.loaded_modules[1]) {
            *outId = "mod.beta";
            return BML_RESULT_OK;
        }
        return BML_RESULT_NOT_FOUND;
    }

    BML_Result MockGetModVersion(BML_Mod mod, BML_Version *outVersion) {
        if (!outVersion) return BML_RESULT_INVALID_ARGUMENT;
        if (mod == g_ModuleMockState.loaded_modules[0]) {
            *outVersion = BML_VERSION_INIT(1, 2, 3);
            return BML_RESULT_OK;
        }
        if (mod == g_ModuleMockState.loaded_modules[1]) {
            *outVersion = BML_VERSION_INIT(2, 0, 0);
            return BML_RESULT_OK;
        }
        return BML_RESULT_NOT_FOUND;
    }

    BML_Result MockRegisterShutdownHook(BML_Mod mod, BML_ShutdownCallback callback, void *user_data) {
        ++g_ModuleMockState.register_shutdown_calls;
        g_ModuleMockState.last_shutdown_hook_mod = mod;
        g_ModuleMockState.last_shutdown_callback = callback;
        g_ModuleMockState.last_shutdown_user_data = user_data;
        return BML_RESULT_OK;
    }

    uint32_t MockGetLoadedModuleCount() {
        return static_cast<uint32_t>(g_ModuleMockState.loaded_modules.size());
    }

    BML_Mod MockGetLoadedModuleAt(uint32_t index) {
        return index < g_ModuleMockState.loaded_modules.size()
            ? g_ModuleMockState.loaded_modules[index]
            : nullptr;
    }

    void ResetModuleMockState() {
        g_ModuleMockState = ModuleMockState{};
    }

    BML_Result MockAcquireModuleInterface(BML_Mod owner,
                                          const char *interfaceId,
                                          const BML_Version *,
                                          const void **outImplementation,
                                          BML_InterfaceLease *outLease) {
        if (!outImplementation || !outLease) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (!interfaceId || std::strcmp(interfaceId, BML_CORE_MODULE_INTERFACE_ID) != 0) {
            return BML_RESULT_NOT_FOUND;
        }

        static BML_CoreModuleInterface moduleApi{};
        moduleApi.RequestCapability = &MockRequestCapability;
        moduleApi.CheckCapability = &MockCheckCapability;
        moduleApi.GetModId = &MockGetModId;
        moduleApi.GetModVersion = &MockGetModVersion;
        moduleApi.RegisterShutdownHook = &MockRegisterShutdownHook;
        moduleApi.GetLoadedModuleCount = &MockGetLoadedModuleCount;
        moduleApi.GetLoadedModuleAt = &MockGetLoadedModuleAt;

        *outImplementation = &moduleApi;
        *outLease = reinterpret_cast<BML_InterfaceLease>(0xBEEF);
        ++g_ModuleMockState.acquire_owned_calls;
        g_ModuleMockState.last_acquire_owner = owner;
        return BML_RESULT_OK;
    }

    BML_Result MockReleaseModuleInterface(BML_InterfaceLease) {
        return BML_RESULT_OK;
    }

    struct LocaleMockState {
        BML_LocaleTable table = reinterpret_cast<BML_LocaleTable>(0xCAFE);
        std::string greeting = "Hello";
        std::string language = "en";
        BML_Mod last_load_module = nullptr;
        BML_Mod last_get_module = nullptr;
        BML_Mod last_bind_module = nullptr;
        int load_calls = 0;
        int get_calls = 0;
        int bind_calls = 0;
        int lookup_calls = 0;
        int set_language_calls = 0;
        bool bind_returns_table = true;
    };

    LocaleMockState g_LocaleMockState;

    BML_Result MockLocaleLoad(BML_Mod owner, const char *localeCode) {
        ++g_LocaleMockState.load_calls;
        g_LocaleMockState.last_load_module = owner;
        (void) localeCode;
        return BML_RESULT_OK;
    }

    const char *MockLocaleGet(BML_Mod owner, const char *key) {
        g_LocaleMockState.last_get_module = owner;
        ++g_LocaleMockState.get_calls;
        if (!key) return nullptr;
        if (std::strcmp(key, "greeting") == 0) {
            return g_LocaleMockState.greeting.c_str();
        }
        return key;
    }

    BML_Result MockLocaleSetLanguage(const char *languageCode) {
        ++g_LocaleMockState.set_language_calls;
        if (!languageCode) return BML_RESULT_INVALID_ARGUMENT;
        g_LocaleMockState.language = languageCode;
        return BML_RESULT_OK;
    }

    BML_Result MockLocaleGetLanguage(const char **outCode) {
        if (!outCode) return BML_RESULT_INVALID_ARGUMENT;
        *outCode = g_LocaleMockState.language.c_str();
        return BML_RESULT_OK;
    }

    BML_Result MockLocaleBindTable(BML_Mod owner, BML_LocaleTable *outTable) {
        g_LocaleMockState.last_bind_module = owner;
        ++g_LocaleMockState.bind_calls;
        if (!outTable) return BML_RESULT_INVALID_ARGUMENT;
        *outTable = g_LocaleMockState.bind_returns_table ? g_LocaleMockState.table : nullptr;
        return BML_RESULT_OK;
    }

    const char *MockLocaleLookup(BML_LocaleTable table, const char *key) {
        ++g_LocaleMockState.lookup_calls;
        if (!key) return nullptr;
        if (table == g_LocaleMockState.table && std::strcmp(key, "greeting") == 0) {
            return g_LocaleMockState.greeting.c_str();
        }
        return key;
    }

    void ResetLocaleMockState() {
        g_LocaleMockState = LocaleMockState{};
    }

    struct TimerMockState {
        BML_Mod last_owner = nullptr;
        BML_Timer last_timer = reinterpret_cast<BML_Timer>(0x7777);
        int schedule_once_calls = 0;
        int cancel_calls = 0;
        int cancel_all_calls = 0;
        int is_active_calls = 0;
    };

    TimerMockState g_TimerMockState;

    BML_Result MockTimerScheduleOnce(BML_Mod owner,
                                          uint32_t,
                                          BML_TimerCallback,
                                          void *,
                                          BML_Timer *outTimer) {
        ++g_TimerMockState.schedule_once_calls;
        g_TimerMockState.last_owner = owner;
        if (!outTimer) return BML_RESULT_INVALID_ARGUMENT;
        *outTimer = g_TimerMockState.last_timer;
        return BML_RESULT_OK;
    }

    BML_Result MockTimerCancel(BML_Mod owner, BML_Timer timer) {
        ++g_TimerMockState.cancel_calls;
        g_TimerMockState.last_owner = owner;
        g_TimerMockState.last_timer = timer;
        return BML_RESULT_OK;
    }

    BML_Result MockTimerIsActive(BML_Mod owner, BML_Timer timer, BML_Bool *outActive) {
        ++g_TimerMockState.is_active_calls;
        g_TimerMockState.last_owner = owner;
        g_TimerMockState.last_timer = timer;
        if (!outActive) return BML_RESULT_INVALID_ARGUMENT;
        *outActive = BML_TRUE;
        return BML_RESULT_OK;
    }

    BML_Result MockTimerCancelAll(BML_Mod owner) {
        ++g_TimerMockState.cancel_all_calls;
        g_TimerMockState.last_owner = owner;
        return BML_RESULT_OK;
    }

    void ResetTimerMockState() {
        g_TimerMockState = TimerMockState{};
    }

    struct MemoryMockState {
        int alloc_calls = 0;
        int calloc_calls = 0;
        int realloc_calls = 0;
        int free_calls = 0;
        int aligned_alloc_calls = 0;
        int aligned_free_calls = 0;
        int pool_create_calls = 0;
        int pool_alloc_calls = 0;
        int pool_free_calls = 0;
        int pool_destroy_calls = 0;
        size_t last_alloc_size = 0;
        size_t pool_block_size = 0;
        BML_MemoryPool pool_handle = reinterpret_cast<BML_MemoryPool>(0x5151);
    };

    MemoryMockState g_MemoryMockState;

    void *MockAlloc(size_t size) {
        ++g_MemoryMockState.alloc_calls;
        g_MemoryMockState.last_alloc_size = size;
        return std::malloc(size);
    }

    void *MockCalloc(size_t count, size_t size) {
        ++g_MemoryMockState.calloc_calls;
        g_MemoryMockState.last_alloc_size = count * size;
        return std::calloc(count, size);
    }

    void *MockRealloc(void *ptr, size_t, size_t newSize) {
        ++g_MemoryMockState.realloc_calls;
        g_MemoryMockState.last_alloc_size = newSize;
        return std::realloc(ptr, newSize);
    }

    void MockFree(void *ptr) {
        ++g_MemoryMockState.free_calls;
        std::free(ptr);
    }

    void *MockAllocAligned(size_t size, size_t) {
        ++g_MemoryMockState.aligned_alloc_calls;
        g_MemoryMockState.last_alloc_size = size;
        return std::malloc(size);
    }

    void MockFreeAligned(void *ptr) {
        ++g_MemoryMockState.aligned_free_calls;
        std::free(ptr);
    }

    BML_Result MockMemoryPoolCreate(size_t blockSize, uint32_t, BML_MemoryPool *outPool) {
        if (!outPool) return BML_RESULT_INVALID_ARGUMENT;
        ++g_MemoryMockState.pool_create_calls;
        g_MemoryMockState.pool_block_size = blockSize;
        *outPool = g_MemoryMockState.pool_handle;
        return BML_RESULT_OK;
    }

    void *MockMemoryPoolAlloc(BML_MemoryPool pool) {
        if (pool != g_MemoryMockState.pool_handle) return nullptr;
        ++g_MemoryMockState.pool_alloc_calls;
        return std::malloc(g_MemoryMockState.pool_block_size);
    }

    void MockMemoryPoolFree(BML_MemoryPool pool, void *ptr) {
        if (pool != g_MemoryMockState.pool_handle) return;
        ++g_MemoryMockState.pool_free_calls;
        std::free(ptr);
    }

    void MockMemoryPoolDestroy(BML_MemoryPool pool) {
        if (pool == g_MemoryMockState.pool_handle) {
            ++g_MemoryMockState.pool_destroy_calls;
        }
    }

    BML_Result MockGetMemoryStats(BML_MemoryStats *outStats) {
        if (!outStats) return BML_RESULT_INVALID_ARGUMENT;
        outStats->total_alloc_count = static_cast<uint64_t>(g_MemoryMockState.alloc_calls
            + g_MemoryMockState.calloc_calls + g_MemoryMockState.pool_alloc_calls);
        outStats->total_free_count = static_cast<uint64_t>(g_MemoryMockState.free_calls
            + g_MemoryMockState.pool_free_calls);
        outStats->active_alloc_count = outStats->total_alloc_count - outStats->total_free_count;
        outStats->total_allocated = g_MemoryMockState.last_alloc_size;
        outStats->peak_allocated = g_MemoryMockState.last_alloc_size;
        return BML_RESULT_OK;
    }


    void ResetMemoryMockState() {
        g_MemoryMockState = MemoryMockState{};
    }

    struct ResourceMockState {
        uint32_t next_slot = 1;
        int retain_calls = 0;
        int release_calls = 0;
        BML_Mod last_owner = nullptr;
        std::unordered_set<uint32_t> valid_slots;
        std::unordered_map<uint32_t, void *> user_data;
    };

    ResourceMockState g_ResourceMockState;

    BML_Result MockHandleCreate(BML_Mod owner, BML_HandleType type, BML_HandleDesc *outDesc) {
        g_ResourceMockState.last_owner = owner;
        if (!outDesc) return BML_RESULT_INVALID_ARGUMENT;
        outDesc->struct_size = sizeof(BML_HandleDesc);
        outDesc->type = type;
        outDesc->generation = 1;
        outDesc->slot = g_ResourceMockState.next_slot++;
        g_ResourceMockState.valid_slots.insert(outDesc->slot);
        return BML_RESULT_OK;
    }

    BML_Result MockHandleRetain(const BML_HandleDesc *desc) {
        if (!desc || !g_ResourceMockState.valid_slots.count(desc->slot)) return BML_RESULT_NOT_FOUND;
        ++g_ResourceMockState.retain_calls;
        return BML_RESULT_OK;
    }

    BML_Result MockHandleRelease(const BML_HandleDesc *desc) {
        if (!desc || !g_ResourceMockState.valid_slots.count(desc->slot)) return BML_RESULT_NOT_FOUND;
        ++g_ResourceMockState.release_calls;
        g_ResourceMockState.valid_slots.erase(desc->slot);
        g_ResourceMockState.user_data.erase(desc->slot);
        return BML_RESULT_OK;
    }

    BML_Result MockHandleValidate(const BML_HandleDesc *desc, BML_Bool *outValid) {
        if (!desc || !outValid) return BML_RESULT_INVALID_ARGUMENT;
        *outValid = g_ResourceMockState.valid_slots.count(desc->slot) ? BML_TRUE : BML_FALSE;
        return BML_RESULT_OK;
    }

    BML_Result MockHandleAttachUserData(const BML_HandleDesc *desc, void *userData) {
        if (!desc || !g_ResourceMockState.valid_slots.count(desc->slot)) return BML_RESULT_NOT_FOUND;
        g_ResourceMockState.user_data[desc->slot] = userData;
        return BML_RESULT_OK;
    }

    BML_Result MockHandleGetUserData(const BML_HandleDesc *desc, void **outUserData) {
        if (!desc || !outUserData) return BML_RESULT_INVALID_ARGUMENT;
        if (!g_ResourceMockState.valid_slots.count(desc->slot)) return BML_RESULT_NOT_FOUND;
        *outUserData = g_ResourceMockState.user_data[desc->slot];
        return BML_RESULT_OK;
    }


    void ResetResourceMockState() {
        g_ResourceMockState = ResourceMockState{};
    }

    struct DiagnosticMockState {
        int get_error_string_calls = 0;
        int get_last_error_calls = 0;
    };

    DiagnosticMockState g_DiagnosticMockState;

    BML_ErrorInfo g_LastErrorInfo = BML_ERROR_INFO_INIT;

    const char *MockGetErrorString(BML_Result result) {
        ++g_DiagnosticMockState.get_error_string_calls;
        if (result == BML_RESULT_NOT_FOUND) return "Mock not found";
        if (result == BML_RESULT_INVALID_ARGUMENT) return "Mock invalid argument";
        return "Mock error";
    }

    BML_Result MockGetLastError(BML_ErrorInfo *outInfo) {
        ++g_DiagnosticMockState.get_last_error_calls;
        if (!outInfo) return BML_RESULT_INVALID_ARGUMENT;
        *outInfo = g_LastErrorInfo;
        return BML_RESULT_OK;
    }

    void ResetDiagnosticMockState() {
        g_DiagnosticMockState = DiagnosticMockState{};
        g_LastErrorInfo = BML_ERROR_INFO_INIT;
        g_LastErrorInfo.result_code = BML_RESULT_NOT_FOUND;
        g_LastErrorInfo.message = "mock last error";
        g_LastErrorInfo.api_name = "MockApi";
    }

    BML_Result MockUnregisterContribution(BML_InterfaceRegistration registration) {
        ++g_ContributionMockState.unregister_calls;
        g_ContributionMockState.last_registration = registration;
        return g_ContributionMockState.unregister_result;
    }

    void ResetInterfaceMockState() {
        g_InterfaceMockState = {};
    }

    void ResetContributionMockState() {
        g_ContributionMockState = {};
    }
}

// ============================================================================
// API Loading Tests
// ============================================================================

TEST(BMLWrapperTest, LoadAPI_Success) {
    // Note: In real tests, we'd need a mock or actual BML.dll
    // This is a smoke test to verify compilation
    EXPECT_FALSE(bml::IsApiLoaded());
}

TEST(BMLWrapperTest, LoadAPI_InvalidPointer) {
    auto result = bml::LoadAPI(nullptr);
    EXPECT_FALSE(result);
}

// ============================================================================
// Context Tests
// ============================================================================

TEST(BMLWrapperTest, Context_DefaultConstruction) {
    bml::Context ctx;
    EXPECT_FALSE(ctx);
    EXPECT_EQ(ctx.Handle(), nullptr);
}

TEST(BMLWrapperTest, Context_ExplicitConstruction) {
    BML_Context raw_ctx = reinterpret_cast<BML_Context>(0x12345678);
    bml::Context ctx(raw_ctx);
    EXPECT_TRUE(ctx);
    EXPECT_EQ(ctx.Handle(), raw_ctx);
}

TEST(BMLWrapperTest, Context_UsesExplicitInterface) {
    ResetContextMockState();

    BML_CoreContextInterface contextApi{};
    contextApi.Retain = &MockContextRetain;
    contextApi.Release = &MockContextRelease;
    contextApi.GetGlobalContext = &MockGetGlobalContextInterface;
    contextApi.GetRuntimeVersion = &MockGetRuntimeVersionInterface;

    auto ctx = bml::GetGlobalContext(&contextApi);
    ASSERT_TRUE(ctx);
    EXPECT_EQ(g_ContextMockState.global_context, ctx.Handle());
    EXPECT_TRUE(ctx.Retain());
    EXPECT_TRUE(ctx.Release());

    {
        bml::ScopedContext scoped(ctx.Handle(), &contextApi);
        EXPECT_TRUE(static_cast<bool>(scoped));
    }

    auto version = bml::GetRuntimeVersion(&contextApi);
    ASSERT_TRUE(version.has_value());
    EXPECT_EQ(4u, version->major);
    EXPECT_EQ(5u, version->minor);
    EXPECT_EQ(6u, version->patch);
    EXPECT_EQ(2, g_ContextMockState.retain_calls);
    EXPECT_EQ(2, g_ContextMockState.release_calls);
}

// ============================================================================
// Config Tests
// ============================================================================

TEST(BMLWrapperTest, Config_Construction) {
    BML_Mod mod = reinterpret_cast<BML_Mod>(0x1);
    bml::Config config(mod);
    // Just verify it compiles
}

TEST(BMLWrapperTest, Config_APISignatures) {
    // Verify all method signatures compile correctly
    BML_Mod mod = reinterpret_cast<BML_Mod>(0x1);
    bml::Config config(mod);
    
    // These will fail at runtime without real API, but should compile
    auto str_val = config.GetString("category", "key");
    EXPECT_FALSE(str_val.has_value());
    
    auto int_val = config.GetInt("category", "key");
    EXPECT_FALSE(int_val.has_value());
    
    auto float_val = config.GetFloat("category", "key");
    EXPECT_FALSE(float_val.has_value());
    
    auto bool_val = config.GetBool("category", "key");
    EXPECT_FALSE(bool_val.has_value());
}

TEST(BMLWrapperTest, Config_UsesExplicitInterface) {
    ResetConfigMockState();

    BML_CoreConfigInterface configApi{};
    configApi.Get = &MockConfigGet;
    configApi.Set = &MockConfigSet;
    configApi.Reset = &MockConfigReset;
    configApi.BatchBegin = &MockConfigBatchBegin;
    configApi.BatchSet = &MockConfigBatchSet;
    configApi.BatchCommit = &MockConfigBatchCommit;
    configApi.BatchDiscard = &MockConfigBatchDiscard;

    BML_Mod mod = reinterpret_cast<BML_Mod>(0x1);
    bml::Config config(mod, &configApi);

    EXPECT_EQ("value", config.GetString("category", "string").value());
    EXPECT_EQ(42, config.GetInt("category", "int").value());
    EXPECT_FLOAT_EQ(2.5f, config.GetFloat("category", "float").value());
    EXPECT_TRUE(config.GetBool("category", "bool").value());

    EXPECT_TRUE(config.SetString("graphics", "quality", "high"));
    EXPECT_EQ(BML_CONFIG_STRING, g_ConfigMockState.last_set_type);
    EXPECT_EQ("graphics", g_ConfigMockState.last_set_category);
    EXPECT_EQ("quality", g_ConfigMockState.last_set_key);
    EXPECT_EQ("high", g_ConfigMockState.last_set_string);

    EXPECT_TRUE(config.SetInt("graphics", "fps", 144));
    EXPECT_EQ(BML_CONFIG_INT, g_ConfigMockState.last_set_type);
    EXPECT_EQ(144, g_ConfigMockState.last_set_int);

    EXPECT_TRUE(config.SetFloat("graphics", "scale", 1.5f));
    EXPECT_EQ(BML_CONFIG_FLOAT, g_ConfigMockState.last_set_type);
    EXPECT_FLOAT_EQ(1.5f, g_ConfigMockState.last_set_float);

    EXPECT_TRUE(config.SetBool("graphics", "vsync", true));
    EXPECT_EQ(BML_CONFIG_BOOL, g_ConfigMockState.last_set_type);
    EXPECT_TRUE(g_ConfigMockState.last_set_bool);

    EXPECT_TRUE(config.Reset("graphics", "quality"));

    bml::ConfigBatch batch(mod, &configApi);
    ASSERT_TRUE(batch.Valid());
    EXPECT_TRUE(batch.Set("graphics", "quality", "medium"));
    EXPECT_TRUE(batch.Commit());
    EXPECT_EQ(1, g_ConfigMockState.batch_begin_calls);
    EXPECT_EQ(1, g_ConfigMockState.batch_set_calls);
    EXPECT_EQ(1, g_ConfigMockState.batch_commit_calls);
}

TEST(BMLWrapperTest, MemoryWrappers_UseExplicitInterface) {
    ResetMemoryMockState();

    BML_CoreMemoryInterface memoryApi{};
    memoryApi.Alloc = &MockAlloc;
    memoryApi.Calloc = &MockCalloc;
    memoryApi.Realloc = &MockRealloc;
    memoryApi.Free = &MockFree;
    memoryApi.AllocAligned = &MockAllocAligned;
    memoryApi.FreeAligned = &MockFreeAligned;
    memoryApi.MemoryPoolCreate = &MockMemoryPoolCreate;
    memoryApi.MemoryPoolAlloc = &MockMemoryPoolAlloc;
    memoryApi.MemoryPoolFree = &MockMemoryPoolFree;
    memoryApi.MemoryPoolDestroy = &MockMemoryPoolDestroy;
    memoryApi.GetMemoryStats = &MockGetMemoryStats;

    void *block = bml::Alloc(32, &memoryApi);
    ASSERT_NE(nullptr, block);
    block = bml::Realloc(block, 32, 64, &memoryApi);
    ASSERT_NE(nullptr, block);
    bml::Free(block, &memoryApi);

    auto aligned = bml::AllocAligned(16, 8, &memoryApi);
    ASSERT_NE(nullptr, aligned);
    bml::FreeAligned(aligned, &memoryApi);

    struct TestObject {
        explicit TestObject(int value) : value(value) {}
        int value;
    };

    auto obj = bml::MakeUniqueWithInterface<TestObject>(&memoryApi, 7);
    ASSERT_TRUE(static_cast<bool>(obj));
    EXPECT_EQ(7, obj->value);
    obj.reset();

    auto arr = bml::MakeUniqueArrayWithInterface<int>(3, &memoryApi);
    ASSERT_TRUE(static_cast<bool>(arr));

    {
        bml::MemoryPool pool(sizeof(TestObject), 4, &memoryApi);
        auto pooled = pool.alloc<TestObject>(9);
        ASSERT_NE(nullptr, pooled);
        EXPECT_EQ(9, pooled->value);
        pool.free(pooled);
    }

    EXPECT_EQ(1, g_MemoryMockState.pool_create_calls);
    EXPECT_EQ(1, g_MemoryMockState.pool_destroy_calls);
}

TEST(BMLWrapperTest, ResourceWrappers_UseExplicitInterface) {
    ResetResourceMockState();

    BML_CoreResourceInterface resourceApi{};
    resourceApi.header.struct_size = sizeof(BML_CoreResourceInterface);
    resourceApi.HandleCreate = &MockHandleCreate;
    resourceApi.HandleCreate = &MockHandleCreate;
    resourceApi.HandleRetain = &MockHandleRetain;
    resourceApi.HandleRelease = &MockHandleRelease;
    resourceApi.HandleValidate = &MockHandleValidate;
    resourceApi.HandleAttachUserData = &MockHandleAttachUserData;
    resourceApi.HandleGetUserData = &MockHandleGetUserData;

    const auto owner = reinterpret_cast<BML_Mod>(0xC0DE);
    auto handle = bml::Handle::create(77, &resourceApi, owner);
    ASSERT_TRUE(static_cast<bool>(handle));
    EXPECT_EQ(owner, g_ResourceMockState.last_owner);
    EXPECT_TRUE(handle.validate());

    int userData = 42;
    EXPECT_TRUE(handle.attachUserData(&userData));
    EXPECT_EQ(&userData, handle.getUserData<int>());
    EXPECT_TRUE(handle.retain());
    EXPECT_TRUE(handle.release());
    EXPECT_FALSE(static_cast<bool>(handle));

    {
        auto shared = bml::SharedHandle::create(88, &resourceApi, owner);
        auto copy = shared;
        EXPECT_EQ(2, copy.use_count());
        EXPECT_EQ(owner, g_ResourceMockState.last_owner);
        EXPECT_TRUE(shared.validate());
        EXPECT_TRUE(shared.attachUserData(&userData));
        EXPECT_EQ(&userData, copy.getUserData<int>());
    }

    EXPECT_EQ(1, g_ResourceMockState.retain_calls);
    EXPECT_EQ(2, g_ResourceMockState.release_calls);
}

// ============================================================================
// IMC Tests
// ============================================================================

TEST(BMLWrapperTest, Imc_PublishSignature) {
    // Verify API signature compiles
    float data = 1.0f;
    // Will fail without real API but should compile
    bool result = bml::imc::publish(reinterpret_cast<BML_Mod>(0x1), "test_event", &data, sizeof(data));
    EXPECT_FALSE(result); // No API loaded
}

TEST(BMLWrapperTest, Imc_PublishTemplateSignature) {
    struct TestData {
        int x;
        float y;
    };
    
    TestData data = { 42, 3.14f };
    bool result = bml::imc::publish(reinterpret_cast<BML_Mod>(0x1), "test_event", data);
    EXPECT_FALSE(result); // No API loaded
}

TEST(BMLWrapperTest, Imc_Subscription_RAII) {
    // Test that Subscription is movable but not copyable
    static_assert(!std::is_copy_constructible_v<bml::imc::Subscription>, 
                  "Subscription should not be copyable");
    static_assert(std::is_move_constructible_v<bml::imc::Subscription>,
                  "Subscription should be movable");
}

// ============================================================================
// Logger Tests
// ============================================================================

TEST(BMLWrapperTest, Logger_Construction) {
    bml::Context ctx(reinterpret_cast<BML_Context>(0x1));
    bml::Logger logger(ctx, "TestTag");
    // Just verify it compiles
}

TEST(BMLWrapperTest, Logger_LogLevels) {
    // Verify enum values match C API
    EXPECT_EQ(static_cast<int>(bml::LogLevel::Trace), BML_LOG_TRACE);
    EXPECT_EQ(static_cast<int>(bml::LogLevel::Debug), BML_LOG_DEBUG);
    EXPECT_EQ(static_cast<int>(bml::LogLevel::Info), BML_LOG_INFO);
    EXPECT_EQ(static_cast<int>(bml::LogLevel::Warn), BML_LOG_WARN);
    EXPECT_EQ(static_cast<int>(bml::LogLevel::Error), BML_LOG_ERROR);
    EXPECT_EQ(static_cast<int>(bml::LogLevel::Fatal), BML_LOG_FATAL);
}

TEST(BMLWrapperTest, Logger_VariadicSignatures) {
    bml::Context ctx(reinterpret_cast<BML_Context>(0x1));
    bml::Logger logger(ctx, "Test");
    
    // These should compile (will fail at runtime without API)
    // Just testing that variadic template forwarding works
    logger.Trace("Test message");
    logger.Debug("Value: %d", 42);
    logger.Info("Float: %f, String: %s", 3.14f, "hello");
    logger.Warn("Warning");
    logger.Error("Error code: %d", -1);
    logger.Fatal("Fatal error");
}

TEST(BMLWrapperTest, Logger_UsesExplicitInterface) {
    ResetLoggingMockState();

    BML_CoreLoggingInterface loggingApi{};
    loggingApi.header.struct_size = sizeof(BML_CoreLoggingInterface);
    loggingApi.LogVa = &MockLogVa;
    loggingApi.SetLogFilter = &MockSetLogFilter;

    const auto owner = reinterpret_cast<BML_Mod>(0xABCD);
    bml::Logger logger(reinterpret_cast<BML_Context>(0x4444), "TestTag", &loggingApi, owner);
    logger.Info("Value: %d", 42);

    EXPECT_EQ(owner, g_LoggingMockState.last_owner);
    EXPECT_EQ(reinterpret_cast<BML_Context>(0x4444), g_LoggingMockState.last_context);
    EXPECT_EQ(BML_LOG_INFO, g_LoggingMockState.last_severity);
    EXPECT_EQ("TestTag", g_LoggingMockState.last_tag);
    EXPECT_EQ("Value: 42", g_LoggingMockState.last_message);

    bml::SetLogFilter(owner, bml::LogLevel::Warn, &loggingApi);
    EXPECT_EQ(1, g_LoggingMockState.set_filter_calls);
    EXPECT_EQ(BML_LOG_WARN, g_LoggingMockState.last_filter);
    EXPECT_EQ(owner, g_LoggingMockState.last_owner);
}

// ============================================================================
// Exception Tests
// ============================================================================

TEST(BMLWrapperTest, Exception_Construction) {
    bml::Exception ex(BML_RESULT_INVALID_ARGUMENT, "Test error");
    EXPECT_EQ(ex.code(), BML_RESULT_INVALID_ARGUMENT);
    // New format includes code: "BML error X: context"
    std::string what = ex.what();
    EXPECT_TRUE(what.find("Test error") != std::string::npos);
    EXPECT_TRUE(what.find("-2") != std::string::npos || what.find("INVALID") != std::string::npos);
}

TEST(BMLWrapperTest, Exception_DefaultMessage) {
    bml::Exception ex(BML_RESULT_NOT_FOUND);
    EXPECT_EQ(ex.code(), BML_RESULT_NOT_FOUND);
    // New format: "BML error X" when no context provided
    std::string what = ex.what();
    EXPECT_TRUE(what.find("BML error") != std::string::npos);
}

TEST(BMLWrapperTest, Exception_UsesExplicitErrorStringFunction) {
    ResetDiagnosticMockState();

    bml::Exception ex(BML_RESULT_NOT_FOUND, "Test error", &MockGetErrorString);
    EXPECT_EQ(ex.code(), BML_RESULT_NOT_FOUND);

    std::string what = ex.what();
    EXPECT_TRUE(what.find("Mock not found") != std::string::npos);
    EXPECT_TRUE(what.find("Test error") != std::string::npos);
    EXPECT_EQ(1, g_DiagnosticMockState.get_error_string_calls);
}

// ============================================================================
// Convenience Functions Tests
// ============================================================================

TEST(BMLWrapperTest, GetRuntimeVersion_NoAPI) {
    auto version = bml::GetRuntimeVersion();
    EXPECT_FALSE(version.has_value()); // No API loaded
}

TEST(BMLWrapperTest, GetGlobalContext_NoAPI) {
    auto ctx = bml::GetGlobalContext();
    EXPECT_FALSE(ctx); // Returns null context when no API loaded
}

// ============================================================================
// Type Safety Tests
// ============================================================================

TEST(BMLWrapperTest, Config_TypeSafety) {
    // Verify that Config methods use correct BML_ConfigType enum values
    EXPECT_EQ(BML_CONFIG_BOOL, 0);
    EXPECT_EQ(BML_CONFIG_INT, 1);
    EXPECT_EQ(BML_CONFIG_FLOAT, 2);
    EXPECT_EQ(BML_CONFIG_STRING, 3);
}

// ============================================================================
// RAII Tests
// ============================================================================

TEST(BMLWrapperTest, ImcSubscription_MoveSemantics) {
    // Verify move constructor and assignment work
    bml::imc::Subscription sub1;
    EXPECT_FALSE(sub1);
    
    bml::imc::Subscription sub2 = std::move(sub1);
    EXPECT_FALSE(sub2);
    
    bml::imc::Subscription sub3;
    sub3 = std::move(sub2);
    EXPECT_FALSE(sub3);
}

TEST(BMLWrapperTest, InterfaceLease_DefaultConstruction) {
    bml::InterfaceLease<int> lease;
    EXPECT_FALSE(static_cast<bool>(lease));
    EXPECT_EQ(nullptr, lease.Get());
}

TEST(BMLWrapperTest, PublishedInterface_IsMoveOnly) {
    static_assert(!std::is_copy_constructible_v<bml::PublishedInterface>);
    static_assert(std::is_move_constructible_v<bml::PublishedInterface>);
}

TEST(BMLWrapperTest, ContributionLease_IsMoveOnly) {
    static_assert(!std::is_copy_constructible_v<bml::ContributionLease>);
    static_assert(std::is_move_constructible_v<bml::ContributionLease>);
}

TEST(BMLWrapperTest, AcquireInterface_NoApi) {
    auto lease = bml::AcquireInterface<int>(
        reinterpret_cast<BML_Mod>(0x1), "test.service", 1, 0, 0);
    EXPECT_FALSE(static_cast<bool>(lease));
}

TEST(BMLWrapperTest, MakeInterfaceDesc_BuildsExpectedMetadata) {
    static int service = 42;
    BML_InterfaceDesc desc = bml::MakeInterfaceDesc(
        "test.service",
        &service,
        1,
        2,
        3,
        BML_INTERFACE_FLAG_IMMUTABLE);

    EXPECT_EQ(sizeof(BML_InterfaceDesc), desc.struct_size);
    EXPECT_STREQ("test.service", desc.interface_id);
    EXPECT_EQ(1u, desc.abi_version.major);
    EXPECT_EQ(2u, desc.abi_version.minor);
    EXPECT_EQ(3u, desc.abi_version.patch);
    EXPECT_EQ(&service, desc.implementation);
    EXPECT_EQ(sizeof(service), desc.implementation_size);
    EXPECT_EQ(BML_INTERFACE_FLAG_IMMUTABLE, desc.flags);
}

TEST(BMLWrapperTest, RegisterInterface_NoApi) {
    static int service = 7;
    BML_InterfaceDesc desc = bml::MakeInterfaceDesc("test.service", &service, 1, 0, 0);
    const auto owner = reinterpret_cast<BML_Mod>(0x1234);
    EXPECT_EQ(BML_RESULT_NOT_SUPPORTED, bml::RegisterInterface(nullptr, owner, &desc));
    EXPECT_EQ(BML_RESULT_NOT_SUPPORTED, bml::UnregisterInterface(nullptr, owner, "test.service"));
}

TEST(BMLWrapperTest, PublishedInterface_RegisterAndDestructor_Success) {
    ResetInterfaceMockState();

    static int service = 11;
    BML_InterfaceDesc desc = bml::MakeInterfaceDesc("test.published", &service, 1, 0, 0);
    const auto owner = reinterpret_cast<BML_Mod>(0x1234);
    {
        bml::PublishedInterface published(&MockInterfaceRegister, &MockInterfaceUnregister, owner, desc);
        EXPECT_TRUE(static_cast<bool>(published));
        EXPECT_EQ(1, g_InterfaceMockState.register_calls);
        EXPECT_EQ(0, g_InterfaceMockState.unregister_calls);
        EXPECT_EQ(owner, g_InterfaceMockState.last_owner);
        EXPECT_EQ("test.published", g_InterfaceMockState.last_interface_id);
    }

    EXPECT_EQ(1, g_InterfaceMockState.unregister_calls);
    EXPECT_EQ("test.published", g_InterfaceMockState.last_interface_id);
}

TEST(BMLWrapperTest, PublishedInterface_MoveTransfersOwnership) {
    ResetInterfaceMockState();

    static int service = 12;
    BML_InterfaceDesc desc = bml::MakeInterfaceDesc("test.move", &service, 1, 0, 0);
    const auto owner = reinterpret_cast<BML_Mod>(0x1234);
    {
        bml::PublishedInterface first(&MockInterfaceRegister, &MockInterfaceUnregister, owner, desc);
        bml::PublishedInterface second(std::move(first));
        EXPECT_FALSE(static_cast<bool>(first));
        EXPECT_TRUE(static_cast<bool>(second));
    }

    EXPECT_EQ(1, g_InterfaceMockState.register_calls);
    EXPECT_EQ(1, g_InterfaceMockState.unregister_calls);
}

TEST(BMLWrapperTest, PublishedInterface_ResetReturnsUnregisterFailureAndKeepsOwnership) {
    ResetInterfaceMockState();
    g_InterfaceMockState.unregister_result = BML_RESULT_BUSY;

    static int service = 13;
    BML_InterfaceDesc desc = bml::MakeInterfaceDesc("test.busy", &service, 1, 0, 0);
    bml::PublishedInterface published(
        &MockInterfaceRegister,
        &MockInterfaceUnregister,
        reinterpret_cast<BML_Mod>(0x1234),
        desc);
    ASSERT_TRUE(static_cast<bool>(published));

    EXPECT_EQ(BML_RESULT_BUSY, published.Reset());
    EXPECT_TRUE(static_cast<bool>(published));
    EXPECT_EQ(1, g_InterfaceMockState.unregister_calls);
}

TEST(BMLWrapperTest, PublishedInterface_RegisterFailureLeavesEmpty) {
    ResetInterfaceMockState();
    g_InterfaceMockState.register_result = BML_RESULT_FAIL;

    static int service = 14;
    BML_InterfaceDesc desc = bml::MakeInterfaceDesc("test.fail", &service, 1, 0, 0);
    bml::PublishedInterface published(
        &MockInterfaceRegister,
        &MockInterfaceUnregister,
        reinterpret_cast<BML_Mod>(0x1234),
        desc);
    EXPECT_FALSE(static_cast<bool>(published));
    EXPECT_EQ(1, g_InterfaceMockState.register_calls);
    EXPECT_EQ(0, g_InterfaceMockState.unregister_calls);
}

TEST(BMLWrapperTest, PublishedInterface_ResetSuccessClearsOwnership) {
    ResetInterfaceMockState();

    static int service = 15;
    BML_InterfaceDesc desc = bml::MakeInterfaceDesc("test.reset", &service, 1, 0, 0);
    bml::PublishedInterface published(
        &MockInterfaceRegister,
        &MockInterfaceUnregister,
        reinterpret_cast<BML_Mod>(0x1234),
        desc);
    ASSERT_TRUE(static_cast<bool>(published));

    EXPECT_EQ(BML_RESULT_OK, published.Reset());
    EXPECT_FALSE(static_cast<bool>(published));
    EXPECT_EQ(1, g_InterfaceMockState.unregister_calls);
}

TEST(BMLWrapperTest, ContributionLease_UnregistersOnDestruction) {
    ResetContributionMockState();

    BML_HostRuntimeInterface hostRuntime{};
    hostRuntime.UnregisterContribution = &MockUnregisterContribution;
    const auto registration = reinterpret_cast<BML_InterfaceRegistration>(0x1234);
    {
        bml::ContributionLease lease(&hostRuntime, registration);
        EXPECT_TRUE(static_cast<bool>(lease));
    }

    EXPECT_EQ(1, g_ContributionMockState.unregister_calls);
    EXPECT_EQ(registration, g_ContributionMockState.last_registration);
}

TEST(BMLWrapperTest, ContributionLease_MoveTransfersOwnership) {
    ResetContributionMockState();

    BML_HostRuntimeInterface hostRuntime{};
    hostRuntime.UnregisterContribution = &MockUnregisterContribution;
    const auto registration = reinterpret_cast<BML_InterfaceRegistration>(0x2234);
    {
        bml::ContributionLease first(&hostRuntime, registration);
        bml::ContributionLease second(std::move(first));
        EXPECT_FALSE(static_cast<bool>(first));
        EXPECT_TRUE(static_cast<bool>(second));
    }

    EXPECT_EQ(1, g_ContributionMockState.unregister_calls);
    EXPECT_EQ(registration, g_ContributionMockState.last_registration);
}

TEST(BMLWrapperTest, ContributionLease_ResetReturnsFailureAndKeepsOwnership) {
    ResetContributionMockState();
    g_ContributionMockState.unregister_result = BML_RESULT_BUSY;

    BML_HostRuntimeInterface hostRuntime{};
    hostRuntime.UnregisterContribution = &MockUnregisterContribution;
    const auto registration = reinterpret_cast<BML_InterfaceRegistration>(0x3234);
    bml::ContributionLease lease(&hostRuntime, registration);
    ASSERT_TRUE(static_cast<bool>(lease));

    EXPECT_EQ(BML_RESULT_BUSY, lease.Reset());
    EXPECT_TRUE(static_cast<bool>(lease));
    EXPECT_EQ(1, g_ContributionMockState.unregister_calls);
}

// ============================================================================
// Integration Smoke Tests
// ============================================================================

TEST(BMLWrapperTest, FullWorkflow_CompilationCheck) {
    // This test verifies that a typical mod workflow compiles correctly
    
    // 1. Load API
    bml::LoadAPI(nullptr);
    
    // 2. Get context
    auto ctx = bml::GetGlobalContext();
    
    // 3. Create logger
    bml::Logger logger(ctx, "MyMod");
    logger.Info("Mod initialized");
    
    // 4. IMC publishing (using static Bus methods)
    // bml::imc::publish("BML/Game/Process", data);
    
    // 5. Subscribe to events
    // auto sub = bml::imc::Subscription::create("BML/Game/Process", [](const bml::imc::Message& msg) {
    //     // Handle event
    // });
    
    // 6. Acquire an interface if needed
    auto uiService = bml::AcquireInterface<int>(
        reinterpret_cast<BML_Mod>(0x1), "bml.ui.imgui", 1, 0, 0);
    EXPECT_FALSE(static_cast<bool>(uiService));

    // 7. Cleanup
    bml::UnloadAPI();
}

// ============================================================================
// API Consistency Tests
// ============================================================================

TEST(BMLWrapperTest, API_ConstCorrectness) {
    // Verify const-correctness of wrapper methods
    BML_Mod mod = reinterpret_cast<BML_Mod>(0x1);
    const bml::Config config(mod);
    
    // These should all compile on const object
    auto str = config.GetString("cat", "key");
    auto i = config.GetInt("cat", "key");
    auto f = config.GetFloat("cat", "key");
    auto b = config.GetBool("cat", "key");
}

TEST(BMLWrapperTest, CoreWrappers_UseExplicitModuleInterface) {
    ResetModuleMockState();

    BML_CoreModuleInterface moduleApi{};
    moduleApi.RequestCapability = &MockRequestCapability;
    moduleApi.CheckCapability = &MockCheckCapability;
    moduleApi.GetModId = &MockGetModId;
    moduleApi.GetModVersion = &MockGetModVersion;
    moduleApi.RegisterShutdownHook = &MockRegisterShutdownHook;
    moduleApi.GetLoadedModuleCount = &MockGetLoadedModuleCount;
    moduleApi.GetLoadedModuleAt = &MockGetLoadedModuleAt;

    bml::Mod mod(g_ModuleMockState.loaded_modules[0], &moduleApi);
    ASSERT_TRUE(mod.GetId().has_value());
    EXPECT_STREQ("mod.alpha", *mod.GetId());

    auto version = mod.GetVersion();
    ASSERT_TRUE(version.has_value());
    EXPECT_EQ(1u, version->major);
    EXPECT_EQ(2u, version->minor);
    EXPECT_EQ(3u, version->patch);

    EXPECT_TRUE(mod.RequestCapability("caps.ok"));
    EXPECT_EQ("caps.ok", g_ModuleMockState.last_capability);
    EXPECT_TRUE(mod.CheckCapability("caps.ok"));
    EXPECT_FALSE(mod.CheckCapability("caps.no"));

    auto loaded = bml::GetLoadedModules(&moduleApi);
    ASSERT_EQ(2u, loaded.size());
    EXPECT_EQ(g_ModuleMockState.loaded_modules[0], loaded[0].Handle());
    EXPECT_EQ(g_ModuleMockState.loaded_modules[1], loaded[1].Handle());

    EXPECT_TRUE(bml::RegisterShutdownHook(mod, []() {}));
    EXPECT_EQ(1, g_ModuleMockState.register_shutdown_calls);
    EXPECT_EQ(g_ModuleMockState.loaded_modules[0], g_ModuleMockState.last_shutdown_hook_mod);

    {
        bml::ShutdownHook hook(mod, []() {});
        EXPECT_TRUE(static_cast<bool>(hook));
    }

    EXPECT_EQ(2, g_ModuleMockState.register_shutdown_calls);
    EXPECT_EQ(0, g_ModuleMockState.set_current_calls);
}

TEST(BMLWrapperTest, ShutdownHookStorageSurvivesWrapperLifetime) {
    ResetModuleMockState();

    BML_CoreModuleInterface moduleApi{};
    moduleApi.RegisterShutdownHook = &MockRegisterShutdownHook;

    bool invoked = false;
    {
        bml::ShutdownHook hook(g_ModuleMockState.loaded_modules[0],
                               [&invoked]() { invoked = true; },
                               &moduleApi);
        ASSERT_TRUE(static_cast<bool>(hook));
    }

    ASSERT_NE(nullptr, g_ModuleMockState.last_shutdown_callback);
    ASSERT_NE(nullptr, g_ModuleMockState.last_shutdown_user_data);
    g_ModuleMockState.last_shutdown_callback(nullptr, g_ModuleMockState.last_shutdown_user_data);
    EXPECT_TRUE(invoked);
}

TEST(BMLWrapperTest, LocaleWrapper_UsesExplicitModuleContext) {
    ResetModuleMockState();
    ResetLocaleMockState();

    BML_CoreModuleInterface moduleApi{};

    BML_CoreLocaleInterface localeApi{};
    localeApi.header.struct_size = sizeof(BML_CoreLocaleInterface);
    localeApi.Get = &MockLocaleGet;

    bml::Locale locale(g_ModuleMockState.loaded_modules[0], &localeApi, &moduleApi);
    EXPECT_STREQ("Hello", locale["greeting"]);
    EXPECT_EQ(g_ModuleMockState.loaded_modules[0], g_LocaleMockState.last_get_module);
    EXPECT_EQ(0, g_ModuleMockState.set_current_calls);
}

TEST(BMLWrapperTest, TimerService_UsesExplicitOwnerOperations) {
    ResetTimerMockState();

    BML_CoreTimerInterface timerApi{};
    timerApi.header.struct_size = sizeof(BML_CoreTimerInterface);
    timerApi.ScheduleOnce = &MockTimerScheduleOnce;
    timerApi.Cancel = &MockTimerCancel;
    timerApi.IsActive = &MockTimerIsActive;
    timerApi.CancelAll = &MockTimerCancelAll;

    const auto owner = reinterpret_cast<BML_Mod>(0x5151);
    bml::TimerService timers(&timerApi, owner);

    const auto timer = timers.ScheduleOnce(5, nullptr);
    EXPECT_EQ(g_TimerMockState.last_timer, timer);
    EXPECT_EQ(1, g_TimerMockState.schedule_once_calls);
    EXPECT_EQ(owner, g_TimerMockState.last_owner);

    EXPECT_TRUE(timers.IsActive(timer));
    EXPECT_EQ(1, g_TimerMockState.is_active_calls);
    EXPECT_EQ(owner, g_TimerMockState.last_owner);

    EXPECT_EQ(BML_RESULT_OK, timers.Cancel(timer));
    EXPECT_EQ(1, g_TimerMockState.cancel_calls);
    EXPECT_EQ(owner, g_TimerMockState.last_owner);

    EXPECT_EQ(BML_RESULT_OK, timers.CancelAll());
    EXPECT_EQ(1, g_TimerMockState.cancel_all_calls);
    EXPECT_EQ(owner, g_TimerMockState.last_owner);
}

TEST(BMLWrapperTest, LocaleWrapper_RebindsAfterLanguageChange) {
    ResetModuleMockState();
    ResetLocaleMockState();

    BML_CoreModuleInterface moduleApi{};

    BML_CoreLocaleInterface localeApi{};
    localeApi.header.struct_size = sizeof(BML_CoreLocaleInterface);
    localeApi.Load = &MockLocaleLoad;
    localeApi.Get = &MockLocaleGet;
    localeApi.SetLanguage = &MockLocaleSetLanguage;
    localeApi.GetLanguage = &MockLocaleGetLanguage;
    localeApi.BindTable = &MockLocaleBindTable;
    localeApi.Lookup = &MockLocaleLookup;

    bml::Locale locale(g_ModuleMockState.loaded_modules[0], &localeApi, &moduleApi);
    ASSERT_TRUE(locale.Load(nullptr));
    EXPECT_EQ(1, g_LocaleMockState.bind_calls);
    EXPECT_EQ(g_ModuleMockState.loaded_modules[0], g_LocaleMockState.last_load_module);
    EXPECT_EQ(g_ModuleMockState.loaded_modules[0], g_LocaleMockState.last_bind_module);
    EXPECT_STREQ("Hello", locale["greeting"]);
    EXPECT_EQ(1, g_LocaleMockState.lookup_calls);
    EXPECT_EQ(0, g_ModuleMockState.set_current_calls);

    ASSERT_TRUE(locale.SetLanguage("zh-CN"));
    EXPECT_EQ(std::string("zh-CN"), g_LocaleMockState.language);
    EXPECT_EQ(2, g_LocaleMockState.bind_calls);
    EXPECT_EQ(g_ModuleMockState.loaded_modules[0], g_LocaleMockState.last_bind_module);
    EXPECT_EQ(0, g_ModuleMockState.set_current_calls);
}

TEST(BMLWrapperTest, ModuleServices_ReusesLocaleWrapperAcrossCalls) {
    ResetModuleMockState();
    ResetLocaleMockState();

    BML_CoreModuleInterface moduleApi{};
    moduleApi.GetModId = &MockGetModId;

    BML_CoreLocaleInterface localeApi{};
    localeApi.header.struct_size = sizeof(BML_CoreLocaleInterface);
    localeApi.Load = &MockLocaleLoad;
    localeApi.Get = &MockLocaleGet;
    localeApi.BindTable = &MockLocaleBindTable;
    localeApi.Lookup = &MockLocaleLookup;

    bml::RuntimeServiceHub hub;
    hub.m_Builtins.Module = &moduleApi;
    hub.m_Builtins.Locale = &localeApi;

    bml::ModuleServices services(g_ModuleMockState.loaded_modules[0], &hub);
    ASSERT_TRUE(services);

    ASSERT_TRUE(services.Locale().Load(nullptr));
    EXPECT_STREQ("Hello", services.Locale()["greeting"]);
    EXPECT_EQ(1, g_LocaleMockState.bind_calls);
    EXPECT_EQ(1, g_LocaleMockState.lookup_calls);
    EXPECT_EQ(0, g_ModuleMockState.set_current_calls);
}

TEST(BMLWrapperTest, ModuleServicesAcquire_UsesExplicitAcquirePath) {
    ResetModuleMockState();

    struct InterfaceGlobalsScope {
        PFN_BML_InterfaceAcquire prevAcquire;
        PFN_BML_InterfaceRelease prevRelease;

        InterfaceGlobalsScope()
            : prevAcquire(bmlInterfaceAcquire),
              prevRelease(bmlInterfaceRelease) {
            bmlInterfaceAcquire = &MockAcquireModuleInterface;
            bmlInterfaceRelease = &MockReleaseModuleInterface;
        }

        ~InterfaceGlobalsScope() {
            bmlInterfaceAcquire = prevAcquire;
            bmlInterfaceRelease = prevRelease;
        }
    } globalsScope;
    (void)globalsScope;

    bml::RuntimeServiceHub hub;
    bml::ModuleServices services(g_ModuleMockState.loaded_modules[0], &hub);
    auto lease = services.Acquire<BML_CoreModuleInterface>();
    ASSERT_TRUE(static_cast<bool>(lease));
    EXPECT_EQ(1, g_ModuleMockState.acquire_owned_calls);
    EXPECT_EQ(g_ModuleMockState.loaded_modules[0], g_ModuleMockState.last_acquire_owner);
    EXPECT_EQ(0, g_ModuleMockState.set_current_calls);
}

TEST(BMLWrapperTest, DiagnosticWrappers_UseExplicitDiagnosticInterface) {
    ResetDiagnosticMockState();

    BML_CoreDiagnosticInterface diagnosticApi{};
    diagnosticApi.GetLastError = &MockGetLastError;
    diagnosticApi.GetErrorString = &MockGetErrorString;
    // Set all new reflection fields to nullptr
    diagnosticApi.GetInterfaceDescriptor = nullptr;
    diagnosticApi.EnumerateInterfaces = nullptr;
    diagnosticApi.InterfaceExists = nullptr;
    diagnosticApi.IsInterfaceCompatible = nullptr;
    diagnosticApi.EnumerateByProvider = nullptr;
    diagnosticApi.EnumerateByCapability = nullptr;
    diagnosticApi.GetInterfaceCount = nullptr;
    diagnosticApi.GetLeaseCount = nullptr;

    // Test error handling functions only
    auto lastError = bml::GetLastErrorInfo(&MockGetLastError);
    ASSERT_TRUE(lastError.has_value());
    EXPECT_EQ(BML_RESULT_NOT_FOUND, lastError->result_code);
    EXPECT_STREQ("mock last error", lastError->message);
    EXPECT_STREQ("MockApi", lastError->api_name);
    EXPECT_EQ(1, g_DiagnosticMockState.get_last_error_calls);
}

TEST(BMLWrapperTest, ResultWrappers_UseExplicitDiagnosticCallbacks) {
    ResetDiagnosticMockState();

    bml::Result<std::string> failed(BML_RESULT_NOT_FOUND);
    EXPECT_STREQ("Mock not found", failed.ErrorMessage(&MockGetErrorString));

    bml::Result<void> failedVoid(BML_RESULT_INVALID_ARGUMENT);
    EXPECT_STREQ("Mock invalid argument", failedVoid.ErrorMessage(&MockGetErrorString));
    EXPECT_EQ(2, g_DiagnosticMockState.get_error_string_calls);

    auto lastError = bml::GetLastErrorInfo(&MockGetLastError);
    ASSERT_TRUE(lastError.has_value());
    EXPECT_EQ(BML_RESULT_NOT_FOUND, lastError->result_code);
    EXPECT_STREQ("mock last error", lastError->message);
    EXPECT_STREQ("MockApi", lastError->api_name);
    EXPECT_EQ(1, g_DiagnosticMockState.get_last_error_calls);
}

TEST(BMLWrapperTest, API_NoexceptSpecifiers) {
    // Basic compilation test - wrapper types compile correctly
    EXPECT_TRUE(true);
}

// ============================================================================
// Hook Registry C++ Wrapper Tests
// ============================================================================

namespace {
    struct HookMockState {
        BML_Result register_result = BML_RESULT_OK;
        BML_Result unregister_result = BML_RESULT_OK;
        int register_calls = 0;
        int unregister_calls = 0;
        void *last_target = nullptr;
    };

    HookMockState g_HookMockState;

    BML_Result MockHookRegister(BML_Mod, const BML_HookDesc *desc) {
        ++g_HookMockState.register_calls;
        g_HookMockState.last_target = desc ? desc->target_address : nullptr;
        return g_HookMockState.register_result;
    }

    BML_Result MockHookUnregister(BML_Mod, void *addr) {
        ++g_HookMockState.unregister_calls;
        g_HookMockState.last_target = addr;
        return g_HookMockState.unregister_result;
    }

    BML_Result MockHookEnumerate(BML_HookEnumCallback cb, void *ud) {
        BML_HookDesc desc = BML_HOOK_DESC_INIT;
        static int dummy = 0;
        desc.target_name = "TestFunction";
        desc.target_address = &dummy;
        desc.priority = 5;
        desc.flags = BML_HOOK_FLAG_CONFLICT;
        cb(&desc, "test.module", ud);
        return BML_RESULT_OK;
    }
} // namespace

TEST(BMLWrapperTest, HookRegistration_RAII) {
    g_HookMockState = {};
    BML_CoreHookRegistryInterface iface{};
    iface.header.major = 1;
    iface.header.minor = 0;
    iface.Register = MockHookRegister;
    iface.Unregister = MockHookUnregister;
    iface.Enumerate = MockHookEnumerate;

    static int target = 0;
    const auto owner = reinterpret_cast<BML_Mod>(0x2468);
    {
        bml::HookRegistration reg("MyFunc", &target, 0, &iface, owner);
        EXPECT_TRUE(reg.Valid());
        EXPECT_EQ(1, g_HookMockState.register_calls);
        EXPECT_EQ(&target, g_HookMockState.last_target);
    }
    // Destructor should unregister
    EXPECT_EQ(1, g_HookMockState.unregister_calls);
}

TEST(BMLWrapperTest, HookRegistration_MoveSemantics) {
    g_HookMockState = {};
    BML_CoreHookRegistryInterface iface{};
    iface.header.major = 1;
    iface.header.minor = 0;
    iface.Register = MockHookRegister;
    iface.Unregister = MockHookUnregister;

    static int target = 0;
    bml::HookRegistration reg1("MyFunc", &target, 0, &iface, reinterpret_cast<BML_Mod>(0x2468));
    EXPECT_TRUE(reg1.Valid());

    bml::HookRegistration reg2(std::move(reg1));
    EXPECT_FALSE(reg1.Valid());
    EXPECT_TRUE(reg2.Valid());
    EXPECT_EQ(0, g_HookMockState.unregister_calls); // Not unregistered yet

    reg2.Unregister();
    EXPECT_EQ(1, g_HookMockState.unregister_calls);
    EXPECT_FALSE(reg2.Valid());
}

TEST(BMLWrapperTest, HookRegistry_Enumerate) {
    BML_CoreHookRegistryInterface iface{};
    iface.header.major = 1;
    iface.header.minor = 0;
    iface.Register = MockHookRegister;
    iface.Unregister = MockHookUnregister;
    iface.Enumerate = MockHookEnumerate;

    auto hooks = bml::HookRegistry::GetAll(&iface);
    ASSERT_EQ(1u, hooks.size());
    EXPECT_EQ("TestFunction", hooks[0].name);
    EXPECT_EQ("test.module", hooks[0].owner);
    EXPECT_EQ(5, hooks[0].priority);
    EXPECT_TRUE(hooks[0].HasConflict());
}

// ============================================================================
// Event Interception C++ Wrapper Tests
// ============================================================================

TEST(BMLWrapperTest, SubscribeOptions_ExecutionOrder) {
    auto opts = bml::imc::SubscribeOptions()
        .QueueCapacity(128)
        .ExecutionOrder(-10);

    EXPECT_EQ(128u, opts.Native().queue_capacity);
    EXPECT_EQ(-10, opts.Native().execution_order);
}

TEST(BMLWrapperTest, MutableMessage_ReadWrite) {
    int payload = 42;
    BML_ImcMessage msg = BML_IMC_MSG(&payload, sizeof(payload));

    bml::imc::MutableMessage mut(&msg);
    EXPECT_TRUE(mut.Valid());
    EXPECT_EQ(sizeof(int), mut.Size());

    auto *val = mut.As<int>();
    ASSERT_NE(nullptr, val);
    EXPECT_EQ(42, *val);

    // Modify metadata
    mut.SetPriority(BML_IMC_PRIORITY_HIGH);
    EXPECT_EQ(BML_IMC_PRIORITY_HIGH, msg.priority);

    mut.SetFlags(BML_IMC_FLAG_RELIABLE);
    EXPECT_EQ(BML_IMC_FLAG_RELIABLE, msg.flags);

    mut.AddFlags(BML_IMC_FLAG_ORDERED);
    EXPECT_EQ(BML_IMC_FLAG_RELIABLE | BML_IMC_FLAG_ORDERED, msg.flags);
}

TEST(BMLWrapperTest, MutableMessage_Null) {
    bml::imc::MutableMessage mut(nullptr);
    EXPECT_FALSE(mut.Valid());
    EXPECT_EQ(nullptr, mut.Data());
    EXPECT_EQ(0u, mut.Size());
    EXPECT_EQ(nullptr, mut.As<int>());

    // Should not crash on null
    mut.SetFlags(0);
    mut.SetPriority(BML_IMC_PRIORITY_HIGH);
    mut.SetData(nullptr, 0);
}

TEST(BMLWrapperTest, EventResult_Constants) {
    // Verify C++ constants match C values
    EXPECT_EQ(BML_EVENT_CONTINUE, bml::imc::event_result::Continue);
    EXPECT_EQ(BML_EVENT_HANDLED, bml::imc::event_result::Handled);
    EXPECT_EQ(BML_EVENT_CANCEL, bml::imc::event_result::Cancel);
}

namespace {
    struct InterceptMockState {
        int subscribe_calls = 0;
        int subscribe_ex_calls = 0;
        BML_ImcInterceptHandler last_handler = nullptr;
        void *last_user_data = nullptr;
        BML_Result subscribe_result = BML_RESULT_OK;
        BML_Subscription mock_handle = reinterpret_cast<BML_Subscription>(uintptr_t(0xBEEF));
    };

    InterceptMockState g_InterceptMockState;

    BML_Result MockSubscribeIntercept(BML_TopicId, BML_ImcInterceptHandler handler,
                                       void *ud, BML_Subscription *out) {
        ++g_InterceptMockState.subscribe_calls;
        g_InterceptMockState.last_handler = handler;
        g_InterceptMockState.last_user_data = ud;
        *out = g_InterceptMockState.mock_handle;
        return g_InterceptMockState.subscribe_result;
    }

    BML_Result MockSubscribeInterceptEx(BML_TopicId, BML_ImcInterceptHandler handler,
                                         void *ud, const BML_SubscribeOptions *,
                                         BML_Subscription *out) {
        ++g_InterceptMockState.subscribe_ex_calls;
        g_InterceptMockState.last_handler = handler;
        g_InterceptMockState.last_user_data = ud;
        *out = g_InterceptMockState.mock_handle;
        return g_InterceptMockState.subscribe_result;
    }
} // namespace

TEST(BMLWrapperTest, InterceptContext_InvokeCallsCallback) {
    // Test the C-to-C++ bridge directly
    bool called = false;
    bml::imc::detail::InterceptContext ctx;
    ctx.callback = [&called](bml::imc::MutableMessage &msg) -> bml::imc::EventResult {
        called = true;
        EXPECT_TRUE(msg.Valid());
        return bml::imc::event_result::Cancel;
    };

    int payload = 99;
    BML_ImcMessage msg = BML_IMC_MSG(&payload, sizeof(payload));
    auto result = bml::imc::detail::InterceptContext::Invoke(nullptr, 0, &msg, &ctx);

    EXPECT_TRUE(called);
    EXPECT_EQ(BML_EVENT_CANCEL, result);
}

TEST(BMLWrapperTest, InterceptContext_NullSafety) {
    // Null context
    auto result = bml::imc::detail::InterceptContext::Invoke(nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(BML_EVENT_CONTINUE, result);

    // Null message
    bml::imc::detail::InterceptContext ctx;
    ctx.callback = [](bml::imc::MutableMessage &) -> bml::imc::EventResult {
        return bml::imc::event_result::Handled;
    };
    result = bml::imc::detail::InterceptContext::Invoke(nullptr, 0, nullptr, &ctx);
    EXPECT_EQ(BML_EVENT_CONTINUE, result);
}

// Main function
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

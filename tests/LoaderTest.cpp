/**
 * @file LoaderTest.cpp
 * @brief Unit tests for the bootstrap-only loader path.
 */

#include <gtest/gtest.h>

#include <cstring>

#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"
#include "bml.h"
#include "bml_services.hpp"

namespace {
    struct FakeLease {
        uint32_t id;
    };

    bool g_BlockCoreConfig = false;

    BML_Result DummyContextRetain(BML_Context) { return BML_RESULT_OK; }
    BML_Result DummyContextRelease(BML_Context) { return BML_RESULT_OK; }
    const BML_Version *DummyGetRuntimeVersion(BML_Context) {
        static BML_Version version = BML_VERSION_INIT(1, 0, 0);
        return &version;
    }
    BML_Result DummyContextSetUserData(BML_Context, const char *, void *, BML_UserDataDestructor) {
        return BML_RESULT_OK;
    }
    BML_Result DummyContextGetUserData(BML_Context, const char *, void **) { return BML_RESULT_OK; }

    BML_Result DummyRequestCapability(BML_Mod, const char *) { return BML_RESULT_OK; }
    BML_Result DummyCheckCapability(BML_Mod, const char *, BML_Bool *out_supported) {
        if (out_supported) {
            *out_supported = BML_TRUE;
        }
        return BML_RESULT_OK;
    }
    BML_Result DummyGetModId(BML_Mod, const char **out_id) {
        static const char kId[] = "test.mod";
        if (!out_id) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        *out_id = kId;
        return BML_RESULT_OK;
    }
    BML_Result DummyGetModVersion(BML_Mod, BML_Version *out_version) {
        if (!out_version) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        *out_version = BML_VERSION_INIT(1, 0, 0);
        return BML_RESULT_OK;
    }
    BML_Result DummyRegisterShutdownHook(BML_Mod, BML_ShutdownCallback, void *) { return BML_RESULT_OK; }
    uint32_t DummyGetLoadedModuleCount(BML_Context) { return 0; }
    BML_Mod DummyGetLoadedModuleAt(BML_Context, uint32_t) { return nullptr; }
    BML_Mod DummyFindModuleById(BML_Context, const char *) { return nullptr; }

    void DummyLog(BML_Mod, BML_LogSeverity, const char *, const char *, ...) {}
    void DummyLogVa(BML_Mod, BML_LogSeverity, const char *, const char *, va_list) {}
    void DummySetLogFilter(BML_Mod, BML_LogSeverity) {}
    BML_Result DummyRegisterLogSinkOverride(BML_Mod, const BML_LogSinkOverrideDesc *) {
        return BML_RESULT_OK;
    }
    BML_Result DummyClearLogSinkOverride(BML_Mod) { return BML_RESULT_OK; }

    BML_Result DummyConfigGet(BML_Mod, const BML_ConfigKey *, BML_ConfigValue *) { return BML_RESULT_OK; }
    BML_Result DummyConfigSet(BML_Mod, const BML_ConfigKey *, const BML_ConfigValue *) { return BML_RESULT_OK; }
    BML_Result DummyConfigReset(BML_Mod, const BML_ConfigKey *) { return BML_RESULT_OK; }
    BML_Result DummyConfigEnumerate(BML_Mod, BML_ConfigEnumCallback, void *) { return BML_RESULT_OK; }
    BML_Result DummyConfigBatchBegin(BML_Mod, BML_ConfigBatch *) { return BML_RESULT_OK; }
    BML_Result DummyConfigBatchSet(BML_ConfigBatch, const BML_ConfigKey *, const BML_ConfigValue *) {
        return BML_RESULT_OK;
    }
    BML_Result DummyConfigBatchCommit(BML_ConfigBatch) { return BML_RESULT_OK; }
    BML_Result DummyConfigBatchDiscard(BML_ConfigBatch) { return BML_RESULT_OK; }
    BML_Result DummyRegisterConfigLoadHooks(BML_Mod, const BML_ConfigLoadHooks *) {
        return BML_RESULT_OK;
    }

    BML_Result DummyGetTopicId(const char *, BML_TopicId *out_id) {
        if (!out_id) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        *out_id = 42;
        return BML_RESULT_OK;
    }
    BML_Result DummyServiceGetTopicId(BML_Context, const char *name, BML_TopicId *out_id) {
        return DummyGetTopicId(name, out_id);
    }
    BML_Result DummyGetRpcId(const char *, BML_RpcId *out_id) {
        if (!out_id) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        *out_id = 7;
        return BML_RESULT_OK;
    }
    BML_Result DummyPublish(BML_Mod, BML_TopicId, const void *, size_t, BML_PayloadTypeId) { return BML_RESULT_OK; }
    BML_Result DummyPublishEx(BML_Mod, BML_TopicId, const BML_ImcMessage *) { return BML_RESULT_OK; }
    BML_Result DummyPublishBuffer(BML_Mod, BML_TopicId, const BML_ImcBuffer *) { return BML_RESULT_OK; }
    BML_Result DummyPublishMulti(BML_Mod, const BML_TopicId *, size_t, const void *, size_t, const BML_ImcMessage *, size_t *) {
        return BML_RESULT_OK;
    }
    BML_Result DummySubscribe(BML_Mod, BML_TopicId, BML_ImcHandler, void *, BML_Subscription *out_sub) {
        if (!out_sub) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        *out_sub = reinterpret_cast<BML_Subscription>(0x2);
        return BML_RESULT_OK;
    }
    BML_Result DummySubscribeEx(BML_Mod owner,
                                BML_TopicId topic,
                                BML_ImcHandler handler,
                                void *userData,
                                const BML_SubscribeOptions *,
                                BML_Subscription *out_sub) {
        return DummySubscribe(owner, topic, handler, userData, out_sub);
    }
    BML_Result DummyUnsubscribe(BML_Subscription) { return BML_RESULT_OK; }
    BML_Result DummySubscriptionIsActive(BML_Subscription, BML_Bool *out_active) {
        if (out_active) {
            *out_active = BML_TRUE;
        }
        return BML_RESULT_OK;
    }
    BML_Result DummyRegisterRpc(BML_RpcId, BML_RpcHandler, void *) { return BML_RESULT_OK; }
    BML_Result DummyUnregisterRpc(BML_RpcId) { return BML_RESULT_OK; }
    BML_Result DummyCallRpc(BML_RpcId, const BML_ImcMessage *, BML_Future *) { return BML_RESULT_OK; }
    BML_Result DummyFutureAwait(BML_Future, uint32_t) { return BML_RESULT_OK; }
    BML_Result DummyFutureGetResult(BML_Future, BML_ImcMessage *) { return BML_RESULT_OK; }
    BML_Result DummyFutureGetState(BML_Future, BML_FutureState *out_state) {
        if (out_state) {
            *out_state = BML_FUTURE_READY;
        }
        return BML_RESULT_OK;
    }
    BML_Result DummyFutureCancel(BML_Future) { return BML_RESULT_OK; }
    BML_Result DummyFutureOnComplete(BML_Future, BML_FutureCallback, void *) { return BML_RESULT_OK; }
    BML_Result DummyFutureRelease(BML_Future) { return BML_RESULT_OK; }
    void DummyPump(size_t) {}
    void DummyServicePump(BML_Context, size_t maxPerSub) { DummyPump(maxPerSub); }
    BML_Result DummyGetSubscriptionStats(BML_Subscription, BML_SubscriptionStats *) { return BML_RESULT_OK; }
    BML_Result DummyGetStats(BML_ImcStats *) { return BML_RESULT_OK; }
    BML_Result DummyServiceGetStats(BML_Context, BML_ImcStats *out_stats) {
        return DummyGetStats(out_stats);
    }
    BML_Result DummyResetStats() { return BML_RESULT_OK; }
    BML_Result DummyServiceResetStats(BML_Context) { return DummyResetStats(); }
    BML_Result DummyGetTopicInfo(BML_TopicId, BML_TopicInfo *) { return BML_RESULT_OK; }
    BML_Result DummyServiceGetTopicInfo(BML_Context, BML_TopicId topic, BML_TopicInfo *out_info) {
        return DummyGetTopicInfo(topic, out_info);
    }
    BML_Result DummyGetTopicName(BML_TopicId, char *, size_t, size_t *) { return BML_RESULT_OK; }
    BML_Result DummyServiceGetTopicName(BML_Context,
                                        BML_TopicId topic,
                                        char *buffer,
                                        size_t bufferSize,
                                        size_t *outLength) {
        return DummyGetTopicName(topic, buffer, bufferSize, outLength);
    }
    BML_Result DummyPublishState(BML_Mod, BML_TopicId, const BML_ImcMessage *) {
        return BML_RESULT_OK;
    }
    BML_Result DummyCopyState(BML_Context, BML_TopicId, void *, size_t, size_t *, BML_ImcStateMeta *) {
        return BML_RESULT_OK;
    }
    BML_Result DummyClearState(BML_Context, BML_TopicId) { return BML_RESULT_OK; }

    void *DummyAlloc(BML_Context, size_t size) {
        return size == 0 ? nullptr : reinterpret_cast<void *>(0x1000);
    }
    void *DummyCalloc(BML_Context, size_t, size_t size) {
        return size == 0 ? nullptr : reinterpret_cast<void *>(0x1000);
    }
    void *DummyRealloc(BML_Context, void *, size_t, size_t size) {
        return size == 0 ? nullptr : reinterpret_cast<void *>(0x1000);
    }
    void DummyFree(BML_Context, void *) {}
    void *DummyAllocAligned(BML_Context, size_t size, size_t) {
        return size == 0 ? nullptr : reinterpret_cast<void *>(0x1000);
    }
    void DummyFreeAligned(BML_Context, void *) {}
    BML_Result DummyMemoryPoolCreate(BML_Context, size_t, uint32_t, BML_MemoryPool *) {
        return BML_RESULT_OK;
    }
    void *DummyMemoryPoolAlloc(BML_Context, BML_MemoryPool) { return reinterpret_cast<void *>(0x1000); }
    void DummyMemoryPoolFree(BML_Context, BML_MemoryPool, void *) {}
    void DummyMemoryPoolDestroy(BML_Context, BML_MemoryPool) {}
    BML_Result DummyGetMemoryStats(BML_Context, BML_MemoryStats *) { return BML_RESULT_OK; }

    BML_Result DummyHandleCreate(BML_Mod, BML_HandleType, BML_HandleDesc *out_desc) {
        if (!out_desc) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        *out_desc = BML_HANDLE_DESC_INIT;
        return BML_RESULT_OK;
    }
    BML_Result DummyHandleRetain(const BML_HandleDesc *) { return BML_RESULT_OK; }
    BML_Result DummyHandleRelease(const BML_HandleDesc *) { return BML_RESULT_OK; }
    BML_Result DummyHandleValidate(const BML_HandleDesc *, BML_Bool *out_valid) {
        if (out_valid) {
            *out_valid = BML_TRUE;
        }
        return BML_RESULT_OK;
    }
    BML_Result DummyHandleAttachUserData(const BML_HandleDesc *, void *) { return BML_RESULT_OK; }
    BML_Result DummyHandleGetUserData(const BML_HandleDesc *, void **) { return BML_RESULT_OK; }
    BML_Result DummyRegisterResourceType(BML_Mod, const BML_ResourceTypeDesc *, BML_HandleType *out_type) {
        if (out_type) {
            *out_type = 1;
        }
        return BML_RESULT_OK;
    }

    BML_Result DummyGetLastError(BML_Context, BML_ErrorInfo *) { return BML_RESULT_OK; }
    void DummyClearLastError(BML_Context) {}
    const char *DummyGetErrorString(BML_Result) { return "ok"; }
    BML_Bool DummyGetInterfaceDescriptor(BML_Context, const char *, BML_InterfaceRuntimeDesc *) {
        return BML_FALSE;
    }
    void DummyEnumerateInterfaces(BML_Context,
                                  PFN_BML_InterfaceRuntimeEnumerator,
                                  void *,
                                  uint64_t) {}
    BML_Bool DummyInterfaceExists(BML_Context, const char *) { return BML_FALSE; }
    BML_Bool DummyIsInterfaceCompatible(BML_Context, const char *, const BML_Version *) {
        return BML_FALSE;
    }
    void DummyEnumerateByProvider(BML_Context,
                                  const char *,
                                  PFN_BML_InterfaceRuntimeEnumerator,
                                  void *) {}
    void DummyEnumerateByCapability(BML_Context,
                                    uint64_t,
                                    PFN_BML_InterfaceRuntimeEnumerator,
                                    void *) {}
    uint32_t DummyGetInterfaceCount(BML_Context) { return 0; }
    uint32_t DummyGetLeaseCount(BML_Context, const char *) { return 0; }

    const BML_Context kServiceContext = reinterpret_cast<BML_Context>(0x1);

    BML_CoreContextInterface g_CoreContextInterface{};
    BML_CoreModuleInterface g_CoreModuleInterface{};
    BML_CoreLoggingInterface g_CoreLoggingInterface{};
    BML_CoreConfigInterface g_CoreConfigInterface{};
    BML_CoreMemoryInterface g_CoreMemoryInterface{};
    BML_CoreResourceInterface g_CoreResourceInterface{};
    BML_CoreDiagnosticInterface g_CoreDiagnosticInterface{};

    struct InterfaceInitializer {
        InterfaceInitializer() {
            g_CoreContextInterface.header = BML_IFACE_HEADER(
                BML_CoreContextInterface, BML_CORE_CONTEXT_INTERFACE_ID, 1, 0);
            g_CoreContextInterface.Context = kServiceContext;
            g_CoreContextInterface.GetRuntimeVersion = DummyGetRuntimeVersion;
            g_CoreContextInterface.Retain = DummyContextRetain;
            g_CoreContextInterface.Release = DummyContextRelease;
            g_CoreContextInterface.SetUserData = DummyContextSetUserData;
            g_CoreContextInterface.GetUserData = DummyContextGetUserData;

            g_CoreModuleInterface.header = BML_IFACE_HEADER(
                BML_CoreModuleInterface, BML_CORE_MODULE_INTERFACE_ID, 2, 0);
            g_CoreModuleInterface.Context = kServiceContext;
            g_CoreModuleInterface.GetModId = DummyGetModId;
            g_CoreModuleInterface.GetModVersion = DummyGetModVersion;
            g_CoreModuleInterface.RequestCapability = DummyRequestCapability;
            g_CoreModuleInterface.CheckCapability = DummyCheckCapability;
            g_CoreModuleInterface.RegisterShutdownHook = DummyRegisterShutdownHook;
            g_CoreModuleInterface.GetLoadedModuleCount = DummyGetLoadedModuleCount;
            g_CoreModuleInterface.GetLoadedModuleAt = DummyGetLoadedModuleAt;
            g_CoreModuleInterface.FindModuleById = DummyFindModuleById;

            g_CoreLoggingInterface.header = BML_IFACE_HEADER(
                BML_CoreLoggingInterface, BML_CORE_LOGGING_INTERFACE_ID, 1, 0);
            g_CoreLoggingInterface.Context = kServiceContext;
            g_CoreLoggingInterface.Log = DummyLog;
            g_CoreLoggingInterface.LogVa = DummyLogVa;
            g_CoreLoggingInterface.SetLogFilter = DummySetLogFilter;
            g_CoreLoggingInterface.RegisterSinkOverride = DummyRegisterLogSinkOverride;
            g_CoreLoggingInterface.ClearSinkOverride = DummyClearLogSinkOverride;

            g_CoreConfigInterface.header = BML_IFACE_HEADER(
                BML_CoreConfigInterface, BML_CORE_CONFIG_INTERFACE_ID, 1, 0);
            g_CoreConfigInterface.Get = DummyConfigGet;
            g_CoreConfigInterface.Set = DummyConfigSet;
            g_CoreConfigInterface.Reset = DummyConfigReset;
            g_CoreConfigInterface.Enumerate = DummyConfigEnumerate;
            g_CoreConfigInterface.BatchBegin = DummyConfigBatchBegin;
            g_CoreConfigInterface.BatchSet = DummyConfigBatchSet;
            g_CoreConfigInterface.BatchCommit = DummyConfigBatchCommit;
            g_CoreConfigInterface.BatchDiscard = DummyConfigBatchDiscard;
            g_CoreConfigInterface.RegisterLoadHooks = DummyRegisterConfigLoadHooks;

            g_CoreMemoryInterface.header = BML_IFACE_HEADER(
                BML_CoreMemoryInterface, BML_CORE_MEMORY_INTERFACE_ID, 1, 0);
            g_CoreMemoryInterface.Context = kServiceContext;
            g_CoreMemoryInterface.Alloc = DummyAlloc;
            g_CoreMemoryInterface.Calloc = DummyCalloc;
            g_CoreMemoryInterface.Realloc = DummyRealloc;
            g_CoreMemoryInterface.Free = DummyFree;
            g_CoreMemoryInterface.AllocAligned = DummyAllocAligned;
            g_CoreMemoryInterface.FreeAligned = DummyFreeAligned;
            g_CoreMemoryInterface.MemoryPoolCreate = DummyMemoryPoolCreate;
            g_CoreMemoryInterface.MemoryPoolAlloc = DummyMemoryPoolAlloc;
            g_CoreMemoryInterface.MemoryPoolFree = DummyMemoryPoolFree;
            g_CoreMemoryInterface.MemoryPoolDestroy = DummyMemoryPoolDestroy;
            g_CoreMemoryInterface.GetMemoryStats = DummyGetMemoryStats;

            g_CoreResourceInterface.header = BML_IFACE_HEADER(
                BML_CoreResourceInterface, BML_CORE_RESOURCE_INTERFACE_ID, 1, 0);
            g_CoreResourceInterface.Context = kServiceContext;
            g_CoreResourceInterface.RegisterResourceType = DummyRegisterResourceType;
            g_CoreResourceInterface.HandleCreate = DummyHandleCreate;
            g_CoreResourceInterface.HandleRetain = DummyHandleRetain;
            g_CoreResourceInterface.HandleRelease = DummyHandleRelease;
            g_CoreResourceInterface.HandleValidate = DummyHandleValidate;
            g_CoreResourceInterface.HandleAttachUserData = DummyHandleAttachUserData;
            g_CoreResourceInterface.HandleGetUserData = DummyHandleGetUserData;

            g_CoreDiagnosticInterface.header = BML_IFACE_HEADER(
                BML_CoreDiagnosticInterface, BML_CORE_DIAGNOSTIC_INTERFACE_ID, 1, 0);
            g_CoreDiagnosticInterface.Context = kServiceContext;
            g_CoreDiagnosticInterface.GetLastError = DummyGetLastError;
            g_CoreDiagnosticInterface.ClearLastError = DummyClearLastError;
            g_CoreDiagnosticInterface.GetErrorString = DummyGetErrorString;
            g_CoreDiagnosticInterface.InterfaceExists = DummyInterfaceExists;
            g_CoreDiagnosticInterface.GetInterfaceDescriptor = DummyGetInterfaceDescriptor;
            g_CoreDiagnosticInterface.IsInterfaceCompatible = DummyIsInterfaceCompatible;
            g_CoreDiagnosticInterface.GetInterfaceCount = DummyGetInterfaceCount;
            g_CoreDiagnosticInterface.GetLeaseCount = DummyGetLeaseCount;
            g_CoreDiagnosticInterface.EnumerateInterfaces = DummyEnumerateInterfaces;
            g_CoreDiagnosticInterface.EnumerateByProvider = DummyEnumerateByProvider;
            g_CoreDiagnosticInterface.EnumerateByCapability = DummyEnumerateByCapability;
        }
    } g_InterfaceInitializer;
    BML_ImcBusInterface g_ImcBusInterface{
        BML_IFACE_HEADER(BML_ImcBusInterface, BML_IMC_BUS_INTERFACE_ID, 1, 0),
        kServiceContext,
        DummyServiceGetTopicId,
        DummyPublish,
        DummyPublishEx,
        DummyPublishBuffer,
        DummyPublishMulti,
        nullptr, // PublishInterceptable
        DummySubscribe,
        DummySubscribeEx,
        nullptr, // SubscribeIntercept
        nullptr, // SubscribeInterceptEx
        DummyUnsubscribe,
        DummySubscriptionIsActive,
        DummyPublishState,
        DummyCopyState,
        DummyClearState,
        DummyServicePump,
        DummyGetSubscriptionStats,
        DummyServiceGetStats,
        DummyServiceResetStats,
        DummyServiceGetTopicInfo,
        DummyServiceGetTopicName,
    };

    void *MockGetProcAddress(const char *proc_name);

    BML_Result MockInterfaceAcquire(BML_Mod,
                                    const char *interface_id,
                                    const BML_Version *,
                                    const void **out_implementation,
                                    BML_InterfaceLease *out_lease) {
        if (!interface_id || !out_implementation || !out_lease) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (g_BlockCoreConfig && std::strcmp(interface_id, BML_CORE_CONFIG_INTERFACE_ID) == 0) {
            return BML_RESULT_NOT_FOUND;
        }

        const void *implementation = nullptr;
        if (std::strcmp(interface_id, BML_CORE_CONTEXT_INTERFACE_ID) == 0) {
            implementation = &g_CoreContextInterface;
        } else if (std::strcmp(interface_id, BML_CORE_MODULE_INTERFACE_ID) == 0) {
            implementation = &g_CoreModuleInterface;
        } else if (std::strcmp(interface_id, BML_CORE_LOGGING_INTERFACE_ID) == 0) {
            implementation = &g_CoreLoggingInterface;
        } else if (std::strcmp(interface_id, BML_CORE_CONFIG_INTERFACE_ID) == 0) {
            implementation = &g_CoreConfigInterface;
        } else if (std::strcmp(interface_id, BML_CORE_MEMORY_INTERFACE_ID) == 0) {
            implementation = &g_CoreMemoryInterface;
        } else if (std::strcmp(interface_id, BML_CORE_RESOURCE_INTERFACE_ID) == 0) {
            implementation = &g_CoreResourceInterface;
        } else if (std::strcmp(interface_id, BML_CORE_DIAGNOSTIC_INTERFACE_ID) == 0) {
            implementation = &g_CoreDiagnosticInterface;
        } else if (std::strcmp(interface_id, BML_IMC_BUS_INTERFACE_ID) == 0) {
            implementation = &g_ImcBusInterface;
        } else {
            return BML_RESULT_NOT_FOUND;
        }

        static FakeLease leases[16]{};
        static uint32_t next_id = 1;
        leases[next_id].id = next_id;
        *out_implementation = implementation;
        *out_lease = reinterpret_cast<BML_InterfaceLease>(&leases[next_id]);
        ++next_id;
        return BML_RESULT_OK;
    }

    BML_Result MockInterfaceRelease(BML_InterfaceLease) { return BML_RESULT_OK; }
    BML_Result MockInterfaceRegister(BML_Mod, const BML_InterfaceDesc *) { return BML_RESULT_OK; }
    BML_Result MockInterfaceUnregister(BML_Mod, const char *) { return BML_RESULT_OK; }

    BML_CoreInterfaceControlInterface g_InterfaceControl{
        BML_IFACE_HEADER(BML_CoreInterfaceControlInterface,
                         BML_CORE_INTERFACE_CONTROL_INTERFACE_ID,
                         1,
                         0),
        kServiceContext,
        MockInterfaceRegister,
        MockInterfaceAcquire,
        MockInterfaceRelease,
        MockInterfaceUnregister,
    };

    BML_Services g_Services{
        &g_CoreContextInterface,
        &g_CoreLoggingInterface,
        &g_CoreModuleInterface,
        &g_CoreConfigInterface,
        &g_CoreMemoryInterface,
        &g_CoreResourceInterface,
        &g_CoreDiagnosticInterface,
        &g_InterfaceControl,
        &g_ImcBusInterface,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
    };

    void *MockGetProcAddress(const char *proc_name) {
        if (!proc_name) {
            return nullptr;
        }
        if (std::strcmp(proc_name, "bmlGetProcAddress") == 0) {
            return reinterpret_cast<void *>(&MockGetProcAddress);
        }
        if (std::strcmp(proc_name, "bmlInterfaceRegister") == 0) {
            return reinterpret_cast<void *>(&MockInterfaceRegister);
        }
        if (std::strcmp(proc_name, "bmlInterfaceAcquire") == 0) {
            return reinterpret_cast<void *>(&MockInterfaceAcquire);
        }
        if (std::strcmp(proc_name, "bmlInterfaceRelease") == 0) {
            return reinterpret_cast<void *>(&MockInterfaceRelease);
        }
        if (std::strcmp(proc_name, "bmlInterfaceUnregister") == 0) {
            return reinterpret_cast<void *>(&MockInterfaceUnregister);
        }
        return nullptr;
    }

} // namespace

class LoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_BlockCoreConfig = false;
        bmlUnloadAPI();
    }

    void TearDown() override {
        bmlUnloadAPI();
    }
};

TEST_F(LoaderTest, LoadAPI_BootstrapAndServices_Success) {
    ASSERT_EQ(BML_RESULT_OK, BML_BOOTSTRAP_LOAD(MockGetProcAddress));
    EXPECT_TRUE(bmlIsApiLoaded());
    EXPECT_NE(nullptr, bmlInterfaceRegister);
    EXPECT_NE(nullptr, bmlInterfaceAcquire);
    EXPECT_NE(nullptr, bmlInterfaceRelease);
    EXPECT_NE(nullptr, bmlInterfaceUnregister);
}

TEST_F(LoaderTest, LoadAPI_NullGetProcAddress_ReturnsInvalidArgument) {
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, BML_BOOTSTRAP_LOAD(nullptr));
    EXPECT_FALSE(bmlIsApiLoaded());
}

TEST_F(LoaderTest, LoadAPI_MissingBootstrapExport_ReturnsNotFound) {
    auto missingBootstrap = [](const char *proc_name) -> void * {
        if (proc_name && std::strcmp(proc_name, "bmlInterfaceRelease") == 0) {
            return nullptr;
        }
        return MockGetProcAddress(proc_name);
    };

    EXPECT_EQ(BML_RESULT_NOT_FOUND, BML_BOOTSTRAP_LOAD(missingBootstrap));
    EXPECT_FALSE(bmlIsApiLoaded());
    EXPECT_EQ(nullptr, bmlInterfaceAcquire);
}

TEST_F(LoaderTest, LoadAPI_MissingNonBootstrapInterface_DoesNotFail) {
    g_BlockCoreConfig = true;
    EXPECT_EQ(BML_RESULT_OK, BML_BOOTSTRAP_LOAD(MockGetProcAddress));
    EXPECT_TRUE(bmlIsApiLoaded());
}

TEST_F(LoaderTest, BindServices_Success) {
    bmlBindServices(&g_Services);
    EXPECT_TRUE(bmlIsApiLoaded());
    EXPECT_NE(nullptr, bmlInterfaceRegister);
    EXPECT_NE(nullptr, bmlInterfaceAcquire);
    EXPECT_NE(nullptr, bmlInterfaceRelease);
    EXPECT_NE(nullptr, bmlInterfaceUnregister);
}

TEST_F(LoaderTest, BindServices_NullBundleLeavesLoaderUnloaded) {
    bmlBindServices(nullptr);
    EXPECT_FALSE(bmlIsApiLoaded());
}

TEST_F(LoaderTest, UnloadAPI_ClearsCompatibilitySurface) {
    ASSERT_EQ(BML_RESULT_OK, BML_BOOTSTRAP_LOAD(MockGetProcAddress));
    ASSERT_TRUE(bmlIsApiLoaded());

    bmlUnloadAPI();

    EXPECT_FALSE(bmlIsApiLoaded());
    EXPECT_EQ(nullptr, bmlInterfaceAcquire);
    EXPECT_EQ(nullptr, bmlInterfaceRelease);
    EXPECT_EQ(nullptr, bmlInterfaceAcquire);
}

TEST_F(LoaderTest, ApiCountsReflectBootstrapMinimum) {
    EXPECT_EQ(4u, bmlGetApiCount());
    EXPECT_EQ(4u, bmlGetRequiredApiCount());
}

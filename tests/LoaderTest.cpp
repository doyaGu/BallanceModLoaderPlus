/**
 * @file LoaderTest.cpp
 * @brief Unit tests for the bootstrap-only loader path.
 */

#include <gtest/gtest.h>

#include <cstring>

#include "bml.h"
#include "bml_builtin_interfaces.h"

namespace {
    struct FakeLease {
        uint32_t id;
    };

    BML_Mod g_CurrentModule = nullptr;
    bool g_BlockCoreConfig = false;

    BML_Result DummyContextRetain(BML_Context) { return BML_RESULT_OK; }
    BML_Result DummyContextRelease(BML_Context) { return BML_RESULT_OK; }
    BML_Context DummyGetGlobalContext() { return reinterpret_cast<BML_Context>(0x1); }
    const BML_Version *DummyGetRuntimeVersion() {
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
    BML_Result DummySetCurrentModule(BML_Mod mod) {
        g_CurrentModule = mod;
        return BML_RESULT_OK;
    }
    BML_Mod DummyGetCurrentModule() { return g_CurrentModule; }
    uint32_t DummyGetLoadedModuleCount() { return 0; }
    BML_Mod DummyGetLoadedModuleAt(uint32_t) { return nullptr; }

    void DummyLog(BML_Context, BML_LogSeverity, const char *, const char *, ...) {}
    void DummyLogVa(BML_Context, BML_LogSeverity, const char *, const char *, va_list) {}
    void DummySetLogFilter(BML_LogSeverity) {}
    BML_Result DummyRegisterLogSinkOverride(const BML_LogSinkOverrideDesc *) { return BML_RESULT_OK; }
    BML_Result DummyClearLogSinkOverride() { return BML_RESULT_OK; }

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
    BML_Result DummyRegisterConfigLoadHooks(const BML_ConfigLoadHooks *) { return BML_RESULT_OK; }

    BML_Result DummyGetTopicId(const char *, BML_TopicId *out_id) {
        if (!out_id) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        *out_id = 42;
        return BML_RESULT_OK;
    }
    BML_Result DummyGetRpcId(const char *, BML_RpcId *out_id) {
        if (!out_id) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        *out_id = 7;
        return BML_RESULT_OK;
    }
    BML_Result DummyPublish(BML_TopicId, const void *, size_t) { return BML_RESULT_OK; }
    BML_Result DummyPublishEx(BML_TopicId, const BML_ImcMessage *) { return BML_RESULT_OK; }
    BML_Result DummyPublishBuffer(BML_TopicId, const BML_ImcBuffer *) { return BML_RESULT_OK; }
    BML_Result DummyPublishMulti(const BML_TopicId *, size_t, const void *, size_t, const BML_ImcMessage *, size_t *) {
        return BML_RESULT_OK;
    }
    BML_Result DummySubscribe(BML_TopicId, BML_ImcHandler, void *, BML_Subscription *out_sub) {
        if (!out_sub) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        *out_sub = reinterpret_cast<BML_Subscription>(0x2);
        return BML_RESULT_OK;
    }
    BML_Result DummySubscribeEx(BML_TopicId, BML_ImcHandler, void *, const BML_SubscribeOptions *, BML_Subscription *out_sub) {
        return DummySubscribe(0, nullptr, nullptr, out_sub);
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
    BML_Result DummyGetSubscriptionStats(BML_Subscription, BML_SubscriptionStats *) { return BML_RESULT_OK; }
    BML_Result DummyGetStats(BML_ImcStats *) { return BML_RESULT_OK; }
    BML_Result DummyResetStats() { return BML_RESULT_OK; }
    BML_Result DummyGetTopicInfo(BML_TopicId, BML_TopicInfo *) { return BML_RESULT_OK; }
    BML_Result DummyGetTopicName(BML_TopicId, char *, size_t, size_t *) { return BML_RESULT_OK; }
    BML_Result DummyPublishState(BML_TopicId, const BML_ImcMessage *) { return BML_RESULT_OK; }
    BML_Result DummyCopyState(BML_TopicId, void *, size_t, size_t *, BML_ImcStateMeta *) { return BML_RESULT_OK; }
    BML_Result DummyClearState(BML_TopicId) { return BML_RESULT_OK; }

    void *DummyAlloc(size_t size) { return size == 0 ? nullptr : reinterpret_cast<void *>(0x1000); }
    void *DummyCalloc(size_t, size_t size) { return size == 0 ? nullptr : reinterpret_cast<void *>(0x1000); }
    void *DummyRealloc(void *, size_t, size_t size) { return size == 0 ? nullptr : reinterpret_cast<void *>(0x1000); }
    void DummyFree(void *) {}
    void *DummyAllocAligned(size_t size, size_t) { return size == 0 ? nullptr : reinterpret_cast<void *>(0x1000); }
    void DummyFreeAligned(void *) {}
    BML_Result DummyMemoryPoolCreate(size_t, uint32_t, BML_MemoryPool *) { return BML_RESULT_OK; }
    void *DummyMemoryPoolAlloc(BML_MemoryPool) { return reinterpret_cast<void *>(0x1000); }
    void DummyMemoryPoolFree(BML_MemoryPool, void *) {}
    void DummyMemoryPoolDestroy(BML_MemoryPool) {}
    BML_Result DummyGetMemoryStats(BML_MemoryStats *) { return BML_RESULT_OK; }

    BML_Result DummyHandleCreate(BML_HandleType, BML_HandleDesc *out_desc) {
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
    BML_Result DummyRegisterResourceType(const BML_ResourceTypeDesc *, BML_HandleType *out_type) {
        if (out_type) {
            *out_type = 1;
        }
        return BML_RESULT_OK;
    }

    BML_Result DummyGetLastError(BML_ErrorInfo *) { return BML_RESULT_OK; }
    void DummyClearLastError() {}
    const char *DummyGetErrorString(BML_Result) { return "ok"; }
    BML_Bool DummyGetInterfaceDescriptor(const char *, BML_InterfaceRuntimeDesc *) { return BML_FALSE; }
    void DummyEnumerateInterfaces(PFN_BML_InterfaceRuntimeEnumerator, void *, uint64_t) {}
    BML_Bool DummyInterfaceExists(const char *) { return BML_FALSE; }
    BML_Bool DummyIsInterfaceCompatible(const char *, const BML_Version *) { return BML_FALSE; }
    void DummyEnumerateByProvider(const char *, PFN_BML_InterfaceRuntimeEnumerator, void *) {}
    void DummyEnumerateByCapability(uint64_t, PFN_BML_InterfaceRuntimeEnumerator, void *) {}
    uint32_t DummyGetInterfaceCount(void) { return 0; }
    uint32_t DummyGetLeaseCount(const char *) { return 0; }

    BML_CoreContextInterface g_CoreContextInterface{
        BML_IFACE_HEADER(BML_CoreContextInterface, BML_CORE_CONTEXT_INTERFACE_ID, 1, 0),
        DummyGetGlobalContext,
        DummyGetRuntimeVersion,
        DummyContextRetain,
        DummyContextRelease,
        DummyContextSetUserData,
        DummyContextGetUserData,
    };
    BML_CoreModuleInterface g_CoreModuleInterface{
        BML_IFACE_HEADER(BML_CoreModuleInterface, BML_CORE_MODULE_INTERFACE_ID, 1, 0),
        DummyGetModId,
        DummyGetModVersion,
        DummyRequestCapability,
        DummyCheckCapability,
        DummySetCurrentModule,
        DummyGetCurrentModule,
        DummyRegisterShutdownHook,
        DummyGetLoadedModuleCount,
        DummyGetLoadedModuleAt,
    };
    BML_CoreLoggingInterface g_CoreLoggingInterface{
        BML_IFACE_HEADER(BML_CoreLoggingInterface, BML_CORE_LOGGING_INTERFACE_ID, 1, 0),
        DummyLog,
        DummyLogVa,
        DummySetLogFilter,
        DummyRegisterLogSinkOverride,
        DummyClearLogSinkOverride,
    };
    BML_CoreConfigInterface g_CoreConfigInterface{
        BML_IFACE_HEADER(BML_CoreConfigInterface, BML_CORE_CONFIG_INTERFACE_ID, 1, 0),
        DummyConfigGet,
        DummyConfigSet,
        DummyConfigReset,
        DummyConfigEnumerate,
        DummyConfigBatchBegin,
        DummyConfigBatchSet,
        DummyConfigBatchCommit,
        DummyConfigBatchDiscard,
        DummyRegisterConfigLoadHooks,
    };
    BML_CoreMemoryInterface g_CoreMemoryInterface{
        BML_IFACE_HEADER(BML_CoreMemoryInterface, BML_CORE_MEMORY_INTERFACE_ID, 1, 0),
        DummyAlloc,
        DummyCalloc,
        DummyRealloc,
        DummyFree,
        DummyAllocAligned,
        DummyFreeAligned,
        DummyMemoryPoolCreate,
        DummyMemoryPoolAlloc,
        DummyMemoryPoolFree,
        DummyMemoryPoolDestroy,
        DummyGetMemoryStats,
    };
    BML_CoreResourceInterface g_CoreResourceInterface{
        BML_IFACE_HEADER(BML_CoreResourceInterface, BML_CORE_RESOURCE_INTERFACE_ID, 1, 0),
        DummyRegisterResourceType,
        DummyHandleCreate,
        DummyHandleRetain,
        DummyHandleRelease,
        DummyHandleValidate,
        DummyHandleAttachUserData,
        DummyHandleGetUserData,
    };
    BML_CoreDiagnosticInterface g_CoreDiagnosticInterface{
        BML_IFACE_HEADER(BML_CoreDiagnosticInterface, BML_CORE_DIAGNOSTIC_INTERFACE_ID, 1, 0),
        DummyGetLastError,
        DummyClearLastError,
        DummyGetErrorString,
        DummyInterfaceExists,
        DummyGetInterfaceDescriptor,
        DummyIsInterfaceCompatible,
        DummyGetInterfaceCount,
        DummyGetLeaseCount,
        DummyEnumerateInterfaces,
        DummyEnumerateByProvider,
        DummyEnumerateByCapability,
    };
    BML_ImcBusInterface g_ImcBusInterface{
        BML_IFACE_HEADER(BML_ImcBusInterface, BML_IMC_BUS_INTERFACE_ID, 1, 0),
        DummyGetTopicId,
        DummyGetRpcId,
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
        DummyRegisterRpc,
        DummyUnregisterRpc,
        DummyCallRpc,
        DummyFutureAwait,
        DummyFutureGetResult,
        DummyFutureGetState,
        DummyFutureCancel,
        DummyFutureOnComplete,
        DummyFutureRelease,
        DummyPublishState,
        DummyCopyState,
        DummyClearState,
        DummyPump,
        DummyGetSubscriptionStats,
        DummyGetStats,
        DummyResetStats,
        DummyGetTopicInfo,
        DummyGetTopicName,
    };

    void *MockGetProcAddress(const char *proc_name);

    BML_Result MockInterfaceAcquire(const char *interface_id,
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

    void *MockGetProcAddress(const char *proc_name) {
        if (!proc_name) {
            return nullptr;
        }
        if (std::strcmp(proc_name, "bmlGetProcAddress") == 0) {
            return reinterpret_cast<void *>(&MockGetProcAddress);
        }
        if (std::strcmp(proc_name, "bmlInterfaceAcquire") == 0) {
            return reinterpret_cast<void *>(&MockInterfaceAcquire);
        }
        if (std::strcmp(proc_name, "bmlInterfaceRelease") == 0) {
            return reinterpret_cast<void *>(&MockInterfaceRelease);
        }
        if (std::strcmp(proc_name, "bmlSetCurrentModule") == 0) {
            return reinterpret_cast<void *>(&DummySetCurrentModule);
        }
        return nullptr;
    }
} // namespace

#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"

class LoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_BlockCoreConfig = false;
        g_CurrentModule = nullptr;
        bmlUnloadAPI();
    }

    void TearDown() override {
        bmlUnloadAPI();
    }
};

TEST_F(LoaderTest, LoadAPI_BootstrapAndBuiltins_Success) {
    ASSERT_EQ(BML_RESULT_OK, BML_BOOTSTRAP_LOAD(MockGetProcAddress));
    EXPECT_TRUE(bmlIsApiLoaded());
    EXPECT_NE(nullptr, bmlInterfaceAcquire);
    EXPECT_NE(nullptr, bmlInterfaceRelease);
    EXPECT_NE(nullptr, bmlSetCurrentModule);
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

TEST_F(LoaderTest, UnloadAPI_ClearsCompatibilitySurface) {
    ASSERT_EQ(BML_RESULT_OK, BML_BOOTSTRAP_LOAD(MockGetProcAddress));
    ASSERT_TRUE(bmlIsApiLoaded());

    bmlUnloadAPI();

    EXPECT_FALSE(bmlIsApiLoaded());
    EXPECT_EQ(nullptr, bmlInterfaceAcquire);
    EXPECT_EQ(nullptr, bmlInterfaceRelease);
    EXPECT_EQ(nullptr, bmlSetCurrentModule);
}

TEST_F(LoaderTest, ApiCountsReflectBootstrapMinimum) {
    EXPECT_EQ(4u, bmlGetApiCount());
    EXPECT_EQ(4u, bmlGetRequiredApiCount());
}

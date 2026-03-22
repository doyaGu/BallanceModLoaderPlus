/**
 * @file BMLIntegrationTests.cpp
 * @brief Integration tests that link against the real BML.dll target
 *
 * Unlike unit tests that compile individual .cpp files, these tests exercise
 * the exported API surface of the built BML shared library, verifying that
 * bootstrap, API lookup, IMC pub/sub, service registration, and diagnostics
 * work end-to-end.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

#include "bml_export.h"
#include "bml_bootstrap.h"
#include "bml_builtin_interfaces.h"
#include "bml_imc.h"
#include "bml_interface.h"
#include "bml_topics.h"
#include "Core/ModHandle.h"

#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"

// ========================================================================
// Test Fixture
// ========================================================================

class BMLIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Bootstrap core only, skip module discovery and loading
        BML_BootstrapConfig config = BML_BOOTSTRAP_CONFIG_INIT;
        config.flags = BML_BOOTSTRAP_FLAG_SKIP_DISCOVERY | BML_BOOTSTRAP_FLAG_SKIP_LOAD;
        BML_Result res = bmlBootstrap(&config);
        ASSERT_EQ(res, BML_RESULT_OK) << "Bootstrap failed";
    }

    void TearDown() override {
        bmlShutdown();
    }
};

// ========================================================================
// Test: CoreBootstrapAndState
// ========================================================================

TEST_F(BMLIntegrationTest, CoreBootstrapAndState) {
    BML_BootstrapState state = bmlGetBootstrapState();
    // With SKIP_DISCOVERY | SKIP_LOAD, state should be CORE_INITIALIZED
    EXPECT_EQ(state, BML_BOOTSTRAP_STATE_CORE_INITIALIZED);
}

TEST_F(BMLIntegrationTest, ConcurrentBootstrapSerializesCoreInitialization) {
    bmlShutdown();

    constexpr size_t kThreadCount = 8;
    std::atomic<size_t> failures{0};
    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);

    for (size_t i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&failures]() {
            BML_BootstrapConfig config = BML_BOOTSTRAP_CONFIG_INIT;
            config.flags = BML_BOOTSTRAP_FLAG_SKIP_DISCOVERY | BML_BOOTSTRAP_FLAG_SKIP_LOAD;
            if (bmlBootstrap(&config) != BML_RESULT_OK) {
                failures.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_EQ(0u, failures.load(std::memory_order_relaxed));
    EXPECT_EQ(BML_BOOTSTRAP_STATE_CORE_INITIALIZED, bmlGetBootstrapState());
}

// ========================================================================
// Test: ProcAddressLookup
// ========================================================================

TEST_F(BMLIntegrationTest, ProcAddressLookup) {
    // Bootstrap export lookup must self-resolve.
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlGetProcAddress"));

    // IMC APIs should be registered
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlImcGetTopicId"));
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlImcPublish"));
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlImcSubscribe"));
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlImcUnsubscribe"));
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlImcPump"));

    // Service APIs should be registered
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlInterfaceRegister"));
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlInterfaceAcquire"));
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlInterfaceUnregister"));

    // Config APIs should be registered
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlConfigGet"));

    // Non-existent API returns null
    EXPECT_EQ(nullptr, bmlGetProcAddress("bmlNonExistentFunction"));
    EXPECT_EQ(nullptr, bmlGetProcAddress(nullptr));
}

// ========================================================================
// Test: FullIMCPublishSubscribeCycle
// ========================================================================

namespace {
    struct TestHandlerState {
        std::atomic<uint32_t> call_count{0};
        uint32_t last_payload{0};
    };

    BML_Mod LookupHostMod();

    void TestImcHandler(BML_Context ctx,
                        BML_TopicId topic,
                        const BML_ImcMessage *msg,
                        void *user_data) {
        (void)ctx;
        (void)topic;
        auto *state = static_cast<TestHandlerState *>(user_data);
        if (!state || !msg)
            return;
        if (msg->data && msg->size == sizeof(uint32_t)) {
            std::memcpy(&state->last_payload, msg->data, sizeof(uint32_t));
        }
        state->call_count.fetch_add(1, std::memory_order_relaxed);
    }
} // namespace

TEST_F(BMLIntegrationTest, FullIMCPublishSubscribeCycle) {
    auto getTopicId = (PFN_BML_ImcGetTopicId)bmlGetProcAddress("bmlImcGetTopicId");
    auto publish = (PFN_BML_ImcPublish)bmlGetProcAddress("bmlImcPublish");
    auto subscribe = (PFN_BML_ImcSubscribe)bmlGetProcAddress("bmlImcSubscribe");
    auto unsubscribe = (PFN_BML_ImcUnsubscribe)bmlGetProcAddress("bmlImcUnsubscribe");
    auto pump = (PFN_BML_ImcPump)bmlGetProcAddress("bmlImcPump");

    ASSERT_NE(nullptr, getTopicId);
    ASSERT_NE(nullptr, publish);
    ASSERT_NE(nullptr, subscribe);
    ASSERT_NE(nullptr, unsubscribe);
    ASSERT_NE(nullptr, pump);

    BML_Mod hostMod = LookupHostMod();
    ASSERT_NE(nullptr, hostMod);

    // Resolve topic
    BML_TopicId topic = 0;
    ASSERT_EQ(BML_RESULT_OK, getTopicId("test/integration/pubsub", &topic));
    EXPECT_NE(BML_TOPIC_ID_INVALID, topic);

    // Subscribe
    TestHandlerState state;
    BML_Subscription sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, subscribe(hostMod, topic, TestImcHandler, &state, &sub));
    ASSERT_NE(nullptr, sub);

    // Publish
    uint32_t payload = 42;
    ASSERT_EQ(BML_RESULT_OK, publish(hostMod, topic, &payload, sizeof(payload)));

    // Pump to deliver
    pump(100);

    EXPECT_EQ(1u, state.call_count.load());
    EXPECT_EQ(42u, state.last_payload);

    // Cleanup
    ASSERT_EQ(BML_RESULT_OK, unsubscribe(sub));
}

// ========================================================================
// Test: TopicIdConsistency
// ========================================================================

TEST_F(BMLIntegrationTest, TopicIdConsistency) {
    auto getTopicId = (PFN_BML_ImcGetTopicId)bmlGetProcAddress("bmlImcGetTopicId");
    ASSERT_NE(nullptr, getTopicId);

    BML_TopicId id1 = 0, id2 = 0, id3 = 0;
    ASSERT_EQ(BML_RESULT_OK, getTopicId("test/consistency/a", &id1));
    ASSERT_EQ(BML_RESULT_OK, getTopicId("test/consistency/a", &id2));
    ASSERT_EQ(BML_RESULT_OK, getTopicId("test/consistency/b", &id3));

    // Same string -> same ID
    EXPECT_EQ(id1, id2);
    // Different strings -> different IDs
    EXPECT_NE(id1, id3);
}

// ========================================================================
// Test: ServiceRegisterAcquireCycle
// ========================================================================

namespace {
    struct TestServiceApi {
        int (*add)(int a, int b);
        int (*multiply)(int a, int b);
    };

    std::filesystem::path MakeTempTestDirectory(const char *prefix) {
        static std::atomic<uint32_t> counter{0};
        const uint32_t value = counter.fetch_add(1, std::memory_order_relaxed);
        auto path = std::filesystem::temp_directory_path() /
            (std::string(prefix) + "-" + std::to_string(value));
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path);
        return path;
    }

    int testAdd(int a, int b) { return a + b; }
    int testMultiply(int a, int b) { return a * b; }

    BML_Mod LookupHostMod() {
        auto findModuleById = reinterpret_cast<PFN_BML_FindModuleById>(
            bmlGetProcAddress("bmlFindModuleById"));
        if (!findModuleById) {
            return nullptr;
        }
        return findModuleById("ModLoader");
    }
} // namespace

TEST_F(BMLIntegrationTest, InterfaceRegisterAcquireCycle) {
    auto serviceRegister = (PFN_BML_InterfaceRegister)bmlGetProcAddress("bmlInterfaceRegister");
    auto serviceAcquire = (PFN_BML_InterfaceAcquire)bmlGetProcAddress("bmlInterfaceAcquire");
    auto serviceRelease = (PFN_BML_InterfaceRelease)bmlGetProcAddress("bmlInterfaceRelease");
    auto serviceUnregister = (PFN_BML_InterfaceUnregister)bmlGetProcAddress("bmlInterfaceUnregister");

    ASSERT_NE(nullptr, serviceRegister);
    ASSERT_NE(nullptr, serviceAcquire);
    ASSERT_NE(nullptr, serviceRelease);
    ASSERT_NE(nullptr, serviceUnregister);

    BML_Mod hostMod = LookupHostMod();
    ASSERT_NE(nullptr, hostMod);

    static TestServiceApi s_api = {testAdd, testMultiply};

    BML_InterfaceDesc desc = BML_INTERFACE_DESC_INIT;
    desc.interface_id = "test.integration.service";
    desc.abi_version = bmlMakeVersion(1, 0, 0);
    desc.implementation = &s_api;
    desc.implementation_size = sizeof(TestServiceApi);

    ASSERT_EQ(BML_RESULT_OK, serviceRegister(hostMod, &desc));

    const void *loadedRaw = nullptr;
    BML_InterfaceLease lease = nullptr;
    BML_Version req = bmlMakeVersion(1, 0, 0);
    ASSERT_EQ(BML_RESULT_OK,
              serviceAcquire(hostMod, "test.integration.service", &req, &loadedRaw, &lease));
    auto *loaded = static_cast<const TestServiceApi *>(loadedRaw);
    ASSERT_NE(nullptr, loaded);
    ASSERT_NE(nullptr, lease);

    EXPECT_EQ(5, loaded->add(2, 3));
    EXPECT_EQ(12, loaded->multiply(3, 4));

    EXPECT_EQ(BML_RESULT_BUSY, serviceUnregister(hostMod, "test.integration.service"));

    ASSERT_EQ(BML_RESULT_OK, serviceRelease(lease));

    ASSERT_EQ(BML_RESULT_OK, serviceUnregister(hostMod, "test.integration.service"));
}

TEST_F(BMLIntegrationTest, InterfaceRegisterRejectsUntrackedOwner) {
    auto serviceRegister = reinterpret_cast<PFN_BML_InterfaceRegister>(
        bmlGetProcAddress("bmlInterfaceRegister"));
    ASSERT_NE(nullptr, serviceRegister);

    BML_Mod_T fakeMod;
    fakeMod.id = "com.test.fake";

    static int service = 5;
    BML_InterfaceDesc desc = BML_INTERFACE_DESC_INIT;
    desc.interface_id = "test.integration.invalid_current";
    desc.abi_version = bmlMakeVersion(1, 0, 0);
    desc.implementation = &service;
    desc.implementation_size = sizeof(service);

    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, serviceRegister(&fakeMod, &desc));
}

TEST_F(BMLIntegrationTest, InterfaceReleaseRejectsForgedOpaqueHandle) {
    auto serviceRelease = reinterpret_cast<PFN_BML_InterfaceRelease>(
        bmlGetProcAddress("bmlInterfaceRelease"));
    ASSERT_NE(nullptr, serviceRelease);

    auto fakeLease = reinterpret_cast<BML_InterfaceLease>(static_cast<uintptr_t>(0x1234));
    EXPECT_EQ(BML_RESULT_INVALID_HANDLE, serviceRelease(fakeLease));
}

TEST_F(BMLIntegrationTest, BuiltinInterfacesAvailableAfterBootstrap) {
    auto serviceAcquire = (PFN_BML_InterfaceAcquire) bmlGetProcAddress("bmlInterfaceAcquire");
    auto serviceRelease = (PFN_BML_InterfaceRelease) bmlGetProcAddress("bmlInterfaceRelease");
    ASSERT_NE(nullptr, serviceAcquire);
    ASSERT_NE(nullptr, serviceRelease);

    BML_Mod hostMod = LookupHostMod();
    ASSERT_NE(nullptr, hostMod);

    static const char *kBuiltinInterfaceIds[] = {
        BML_CORE_CONTEXT_INTERFACE_ID,
        BML_CORE_LOGGING_INTERFACE_ID,
        BML_CORE_CONFIG_INTERFACE_ID,
        BML_CORE_MEMORY_INTERFACE_ID,
        BML_CORE_RESOURCE_INTERFACE_ID,
        BML_CORE_DIAGNOSTIC_INTERFACE_ID,
        BML_IMC_BUS_INTERFACE_ID,
    };

    BML_Version req = bmlMakeVersion(1, 0, 0);
    for (const char *interfaceId : kBuiltinInterfaceIds) {
        const void *implementation = nullptr;
        BML_InterfaceLease lease = nullptr;

        ASSERT_EQ(BML_RESULT_OK, serviceAcquire(hostMod, interfaceId, &req, &implementation, &lease))
            << interfaceId;
        ASSERT_NE(nullptr, implementation) << interfaceId;
        ASSERT_NE(nullptr, lease) << interfaceId;
        EXPECT_EQ(BML_RESULT_OK, serviceRelease(lease)) << interfaceId;
    }

    {
        const void *implementation = nullptr;
        BML_InterfaceLease lease = nullptr;
        BML_Version imcReq = bmlMakeVersion(1, 0, 0);

        ASSERT_EQ(BML_RESULT_OK,
                  serviceAcquire(hostMod, BML_IMC_BUS_INTERFACE_ID, &imcReq, &implementation, &lease));
        auto *imcBus = static_cast<const BML_ImcBusInterface *>(implementation);
        ASSERT_NE(nullptr, imcBus);
        EXPECT_GE(imcBus->header.struct_size, sizeof(BML_ImcBusInterface));
        EXPECT_NE(nullptr, imcBus->PublishState);
        EXPECT_NE(nullptr, imcBus->CopyState);
        EXPECT_NE(nullptr, imcBus->ClearState);
        EXPECT_EQ(BML_RESULT_OK, serviceRelease(lease));
    }

    // RPC interface (split from IMC Bus)
    {
        const void *implementation = nullptr;
        BML_InterfaceLease lease = nullptr;
        BML_Version rpcReq = bmlMakeVersion(1, 0, 0);

        ASSERT_EQ(BML_RESULT_OK,
                  serviceAcquire(hostMod, BML_IMC_RPC_INTERFACE_ID, &rpcReq, &implementation, &lease));
        auto *rpc = static_cast<const BML_ImcRpcInterface *>(implementation);
        ASSERT_NE(nullptr, rpc);
        EXPECT_GE(rpc->header.struct_size, sizeof(BML_ImcRpcInterface));
        EXPECT_NE(nullptr, rpc->RegisterRpc);
        EXPECT_NE(nullptr, rpc->CallRpc);
        EXPECT_NE(nullptr, rpc->RegisterRpcEx);
        EXPECT_NE(nullptr, rpc->CallRpcEx);
        EXPECT_NE(nullptr, rpc->FutureGetError);
        EXPECT_EQ(BML_RESULT_OK, serviceRelease(lease));
    }

    {
        const void *implementation = nullptr;
        BML_InterfaceLease lease = nullptr;
        BML_Version moduleReq = bmlMakeVersion(2, 0, 0);

        ASSERT_EQ(BML_RESULT_OK,
                  serviceAcquire(hostMod,
                                 BML_CORE_MODULE_INTERFACE_ID,
                                 &moduleReq,
                                 &implementation,
                                 &lease));
        auto *moduleApi = static_cast<const BML_CoreModuleInterface *>(implementation);
        ASSERT_NE(nullptr, moduleApi);
        EXPECT_EQ(2u, moduleApi->header.major);
        EXPECT_EQ(0u, moduleApi->header.minor);
        EXPECT_NE(nullptr, moduleApi->FindModuleById);
        EXPECT_EQ(BML_RESULT_OK, serviceRelease(lease));
    }

    {
        const void *implementation = nullptr;
        BML_InterfaceLease lease = nullptr;
        BML_Version configReq = bmlMakeVersion(1, 1, 0);

        ASSERT_EQ(BML_RESULT_OK,
                  serviceAcquire(hostMod,
                                 BML_CORE_CONFIG_INTERFACE_ID,
                                 &configReq,
                                 &implementation,
                                 &lease));
        auto *configApi = static_cast<const BML_CoreConfigInterface *>(implementation);
        ASSERT_NE(nullptr, configApi);
        EXPECT_EQ(1u, configApi->header.major);
        EXPECT_EQ(2u, configApi->header.minor);
        EXPECT_NE(nullptr, configApi->GetString);
        EXPECT_EQ(BML_RESULT_OK, serviceRelease(lease));
    }

    {
        const void *implementation = nullptr;
        BML_InterfaceLease lease = nullptr;
        BML_Version imcReq = bmlMakeVersion(1, 0, 0);

        ASSERT_EQ(BML_RESULT_OK,
                  serviceAcquire(hostMod, BML_IMC_BUS_INTERFACE_ID, &imcReq, &implementation, &lease));
        auto *imcBus = static_cast<const BML_ImcBusInterface *>(implementation);
        ASSERT_NE(nullptr, imcBus);
        EXPECT_EQ(1u, imcBus->header.major);
        EXPECT_EQ(2u, imcBus->header.minor);
        EXPECT_NE(nullptr, imcBus->GetTopicName);
        EXPECT_EQ(BML_RESULT_OK, serviceRelease(lease));
    }
}

TEST_F(BMLIntegrationTest, InternalAndHostPermissionsAreEnforced) {
    auto serviceRegister = (PFN_BML_InterfaceRegister) bmlGetProcAddress("bmlInterfaceRegister");
    auto serviceAcquire = (PFN_BML_InterfaceAcquire) bmlGetProcAddress("bmlInterfaceAcquire");
    auto serviceRelease = (PFN_BML_InterfaceRelease) bmlGetProcAddress("bmlInterfaceRelease");
    auto serviceUnregister = (PFN_BML_InterfaceUnregister) bmlGetProcAddress("bmlInterfaceUnregister");

    ASSERT_NE(nullptr, serviceRegister);
    ASSERT_NE(nullptr, serviceAcquire);
    ASSERT_NE(nullptr, serviceRelease);
    ASSERT_NE(nullptr, serviceUnregister);

    BML_Mod hostMod = LookupHostMod();
    ASSERT_NE(nullptr, hostMod);

    BML_Version req = bmlMakeVersion(1, 0, 0);
    const void *implementation = nullptr;
    BML_InterfaceLease hostLease = nullptr;

    static int s_hostMarker = 7;
    BML_InterfaceDesc desc = BML_INTERFACE_DESC_INIT;
    desc.interface_id = "test.integration.host_owned";
    desc.abi_version = req;
    desc.implementation = &s_hostMarker;
    desc.implementation_size = sizeof(s_hostMarker);
    desc.flags = BML_INTERFACE_FLAG_HOST_OWNED | BML_INTERFACE_FLAG_IMMUTABLE;

    EXPECT_EQ(BML_RESULT_PERMISSION_DENIED, serviceRegister(hostMod, &desc));

    ASSERT_EQ(BML_RESULT_OK,
              serviceAcquire(hostMod,
                             BML_CORE_HOST_RUNTIME_INTERFACE_ID,
                             &req,
                             &implementation,
                             &hostLease));
    ASSERT_NE(nullptr, implementation);
    ASSERT_NE(nullptr, hostLease);

    ASSERT_EQ(BML_RESULT_OK, serviceRegister(hostMod, &desc));
    ASSERT_EQ(BML_RESULT_OK, serviceRelease(hostLease));
    ASSERT_EQ(BML_RESULT_OK, serviceUnregister(hostMod, desc.interface_id));
}

TEST_F(BMLIntegrationTest, BootstrapLoaderLoadsOnlyBootstrapMinimum) {
    ASSERT_EQ(BML_RESULT_OK, BML_BOOTSTRAP_LOAD(bmlGetProcAddress));
    ASSERT_TRUE(bmlIsApiLoaded());
    EXPECT_NE(nullptr, bmlInterfaceAcquire);
    EXPECT_NE(nullptr, bmlInterfaceRelease);
    bmlUnloadAPI();
}

// ========================================================================
// Test: DiagnosticsAvailable
// ========================================================================

TEST_F(BMLIntegrationTest, DiagnosticsAvailable) {
    const BML_BootstrapDiagnostics *diag = bmlGetBootstrapDiagnostics();
    ASSERT_NE(nullptr, diag);
}

TEST_F(BMLIntegrationTest, BootstrapDiagnosticsSnapshotSurvivesShutdownUntilNextQuery) {
    namespace fs = std::filesystem;

    const fs::path tempDir = MakeTempTestDirectory("bml-bootstrap-diagnostics");
    const fs::path modDir = tempDir / "BrokenMod";
    std::error_code ec;
    ASSERT_TRUE(fs::create_directories(modDir, ec));
    ASSERT_FALSE(ec);

    {
        std::ofstream manifest(modDir / "mod.toml", std::ios::binary);
        ASSERT_TRUE(manifest.is_open());
        manifest << "id = \"broken.mod\"\n";
        manifest << "version = [\n";
    }

    bmlShutdown();

    BML_BootstrapConfig config = BML_BOOTSTRAP_CONFIG_INIT;
    config.flags = BML_BOOTSTRAP_FLAG_SKIP_LOAD;
    const std::string tempDirUtf8 = tempDir.generic_string();
    config.mods_dir_utf8 = tempDirUtf8.c_str();

    ASSERT_EQ(BML_RESULT_FAIL, bmlBootstrap(&config));

    const BML_BootstrapDiagnostics *diag = bmlGetBootstrapDiagnostics();
    ASSERT_NE(nullptr, diag);
    ASSERT_GT(diag->manifest_error_count, 0u);
    ASSERT_NE(nullptr, diag->manifest_errors);
    ASSERT_NE(nullptr, diag->manifest_errors[0].message);

    const uint32_t manifestErrorCount = diag->manifest_error_count;
    const std::string firstMessage = diag->manifest_errors[0].message;
    ASSERT_FALSE(firstMessage.empty());

    bmlShutdown();

    EXPECT_EQ(manifestErrorCount, diag->manifest_error_count);
    EXPECT_STREQ(firstMessage.c_str(), diag->manifest_errors[0].message);

    const BML_BootstrapDiagnostics *cleared = bmlGetBootstrapDiagnostics();
    ASSERT_NE(nullptr, cleared);
    EXPECT_EQ(0u, cleared->manifest_error_count);

    fs::remove_all(tempDir, ec);
}

TEST_F(BMLIntegrationTest, CheckCapabilityRejectsNullOutputPointer) {
    auto findModuleById = reinterpret_cast<PFN_BML_FindModuleById>(bmlGetProcAddress("bmlFindModuleById"));
    auto checkCapability =
        reinterpret_cast<PFN_BML_CheckCapability>(bmlGetProcAddress("bmlCheckCapability"));
    ASSERT_NE(nullptr, findModuleById);
    ASSERT_NE(nullptr, checkCapability);

    BML_Mod host = findModuleById("ModLoader");
    ASSERT_NE(nullptr, host);
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT,
              checkCapability(host, "bml.internal.runtime", nullptr));
}

/**
 * @file ModuleFrameworkIntegrationTests.cpp
 * @brief Integration tests for the Module framework and C++ wrappers against real BML.dll
 *
 * Tests:
 * - bml::Module + ModuleEntryHelper lifecycle (attach, detach, failure rollback)
 * - C++ wrapper APIs against real runtime with explicit builtin interface acquisition
 */

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"
#include "bml_builtin_interfaces.h"
#include "bml_topics.h"
#include "Core/ModHandle.h"

// ============================================================================
// Test Fixture: Bootstrap BML
// ============================================================================

class ModuleFrameworkIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        BML_BootstrapConfig config = BML_BOOTSTRAP_CONFIG_INIT;
        config.flags = BML_BOOTSTRAP_FLAG_SKIP_DISCOVERY | BML_BOOTSTRAP_FLAG_SKIP_LOAD;
        BML_Result res = bmlBootstrap(&config);
        ASSERT_EQ(res, BML_RESULT_OK) << "Bootstrap failed";
    }

    void TearDown() override {
        // Ensure APIs are unloaded (may already be null after Detach tests)
        bmlUnloadAPI();
        bmlShutdown();
    }

    static BML_ModAttachArgs MakeAttachArgs(BML_Mod mod = reinterpret_cast<BML_Mod>(0x1)) {
        BML_ModAttachArgs args{};
        args.struct_size = sizeof(BML_ModAttachArgs);
        args.api_version = BML_MOD_ENTRYPOINT_API_VERSION;
        args.mod = mod;
        args.get_proc = bmlGetProcAddress;
        return args;
    }

    static BML_ModDetachArgs MakeDetachArgs(BML_Mod mod = reinterpret_cast<BML_Mod>(0x1)) {
        BML_ModDetachArgs args{};
        args.struct_size = sizeof(BML_ModDetachArgs);
        args.api_version = BML_MOD_ENTRYPOINT_API_VERSION;
        args.mod = mod;
        return args;
    }

    void EnsureApiLoaded() {
        if (!bml::IsApiLoaded()) {
            BML_Result res = BML_BOOTSTRAP_LOAD(bmlGetProcAddress);
            ASSERT_EQ(res, BML_RESULT_OK) << "API loading failed";
        }
    }

    static bml::InterfaceLease<BML_CoreContextInterface> AcquireCoreContext() {
        return bml::AcquireInterface<BML_CoreContextInterface>(BML_CORE_CONTEXT_INTERFACE_ID, 1);
    }

    static bml::InterfaceLease<BML_CoreLoggingInterface> AcquireCoreLogging() {
        return bml::AcquireInterface<BML_CoreLoggingInterface>(BML_CORE_LOGGING_INTERFACE_ID, 1);
    }

    static bml::InterfaceLease<BML_CoreConfigInterface> AcquireCoreConfig() {
        return bml::AcquireInterface<BML_CoreConfigInterface>(BML_CORE_CONFIG_INTERFACE_ID, 1);
    }

    static bml::InterfaceLease<BML_ImcBusInterface> AcquireImcBus() {
        return bml::AcquireInterface<BML_ImcBusInterface>(BML_IMC_BUS_INTERFACE_ID, 1);
    }

    static BML_Mod LookupHostMod() {
        auto findModuleById = reinterpret_cast<PFN_BML_FindModuleById>(
            bmlGetProcAddress("bmlFindModuleById"));
        if (!findModuleById) {
            return nullptr;
        }
        return findModuleById("ModLoader");
    }

};

// ============================================================================
// Test Module Definitions
// ============================================================================

namespace {

// Simple module that tracks lifecycle events via flags
class TrackerMod : public bml::Module {
public:
    static std::atomic<int> attach_count;
    static std::atomic<int> detach_count;
    static BML_Mod last_handle;

    BML_Result OnAttach(bml::ModuleServices &services) override {
        (void)services;
        attach_count.fetch_add(1, std::memory_order_relaxed);
        last_handle = Handle();
        return BML_RESULT_OK;
    }

    void OnDetach() override {
        detach_count.fetch_add(1, std::memory_order_relaxed);
    }

    static void Reset() {
        attach_count = 0;
        detach_count = 0;
        last_handle = nullptr;
    }
};

std::atomic<int> TrackerMod::attach_count{0};
std::atomic<int> TrackerMod::detach_count{0};
BML_Mod TrackerMod::last_handle = nullptr;

// Module whose OnAttach deliberately fails
class FailingMod : public bml::Module {
public:
    static std::atomic<int> attach_count;
    static std::atomic<int> detach_count;

    BML_Result OnAttach(bml::ModuleServices &services) override {
        (void)services;
        attach_count.fetch_add(1, std::memory_order_relaxed);
        return BML_RESULT_FAIL;
    }

    void OnDetach() override {
        detach_count.fetch_add(1, std::memory_order_relaxed);
    }

    static void Reset() {
        attach_count = 0;
        detach_count = 0;
    }
};

std::atomic<int> FailingMod::attach_count{0};
std::atomic<int> FailingMod::detach_count{0};

// Module that subscribes to an IMC topic during OnAttach
class SubscriberMod : public bml::Module {
    bml::imc::SubscriptionManager m_Subs;
    bml::InterfaceLease<BML_ImcBusInterface> m_ImcBus;

public:
    static std::atomic<int> message_count;

    BML_Result OnAttach(bml::ModuleServices &services) override {
        (void)services;
        m_ImcBus = Acquire<BML_ImcBusInterface>(BML_IMC_BUS_INTERFACE_ID, 1);
        if (!m_ImcBus) {
            return BML_RESULT_NOT_FOUND;
        }
        m_Subs.Bind(m_ImcBus.Get());
        bool ok = m_Subs.Add("test/framework/event", [](const bml::imc::Message &) {
            message_count.fetch_add(1, std::memory_order_relaxed);
        });
        return ok ? BML_RESULT_OK : BML_RESULT_FAIL;
    }

    void OnDetach() override {
        // SubscriptionManager RAII cleans up after this returns
    }

    static void Reset() {
        message_count = 0;
    }
};

std::atomic<int> SubscriberMod::message_count{0};

struct TestPublishedService {
    uint16_t major;
    uint16_t minor;
    int (*GetValue)();
};

int GetPublishedServiceValue() {
    return 77;
}

class HelperAccessMod : public bml::Module {
public:
    using bml::Module::Acquire;
    using bml::Module::Publish;

    void Initialize(BML_Mod mod, PFN_BML_GetProcAddress getProc) {
        m_Handle = mod;
        m_GetProc = getProc;
    }
};

class PublishingMod : public bml::Module {
public:
    static inline TestPublishedService s_Service{
        1u,
        0u,
        &GetPublishedServiceValue,
    };

    bml::PublishedInterface m_Published;

    BML_Result OnAttach(bml::ModuleServices &services) override {
        (void)services;
        m_Published = Publish("test.framework.published", &s_Service, 1, 0, 0, BML_INTERFACE_FLAG_IMMUTABLE);
        return m_Published ? BML_RESULT_OK : BML_RESULT_FAIL;
    }
};

class FailingPublishingMod : public bml::Module {
public:
    bml::PublishedInterface m_Published;

    BML_Result OnAttach(bml::ModuleServices &services) override {
        (void)services;
        m_Published = Publish("test.framework.rollback_published",
                              &PublishingMod::s_Service,
                              1,
                              0,
                              0,
                              BML_INTERFACE_FLAG_IMMUTABLE);
        return m_Published ? BML_RESULT_FAIL : BML_RESULT_FAIL;
    }
};

using TrackerHelper = bml::detail::ModuleEntryHelper<TrackerMod>;
using FailingHelper = bml::detail::ModuleEntryHelper<FailingMod>;
using SubscriberHelper = bml::detail::ModuleEntryHelper<SubscriberMod>;
using PublishingHelper = bml::detail::ModuleEntryHelper<PublishingMod>;
using FailingPublishingHelper = bml::detail::ModuleEntryHelper<FailingPublishingMod>;

} // namespace

// ============================================================================
// Module Framework: Basic Lifecycle
// ============================================================================

TEST_F(ModuleFrameworkIntegrationTest, AttachDetach_Success) {
    TrackerMod::Reset();

    auto attach_args = MakeAttachArgs();
    ASSERT_EQ(BML_RESULT_OK, TrackerHelper::Entrypoint(BML_MOD_ENTRYPOINT_ATTACH, &attach_args));
    EXPECT_EQ(1, TrackerMod::attach_count.load());
    EXPECT_NE(nullptr, TrackerHelper::GetInstance());

    auto detach_args = MakeDetachArgs();
    ASSERT_EQ(BML_RESULT_OK, TrackerHelper::Entrypoint(BML_MOD_ENTRYPOINT_DETACH, &detach_args));
    EXPECT_EQ(1, TrackerMod::detach_count.load());
    EXPECT_EQ(nullptr, TrackerHelper::GetInstance());
}

TEST_F(ModuleFrameworkIntegrationTest, HandleAssignment) {
    TrackerMod::Reset();

    BML_Mod fake_mod = reinterpret_cast<BML_Mod>(0xDEAD);
    auto attach_args = MakeAttachArgs(fake_mod);
    ASSERT_EQ(BML_RESULT_OK, TrackerHelper::Entrypoint(BML_MOD_ENTRYPOINT_ATTACH, &attach_args));
    EXPECT_EQ(fake_mod, TrackerMod::last_handle);
    EXPECT_EQ(fake_mod, TrackerHelper::GetInstance()->Handle());

    auto detach_args = MakeDetachArgs(fake_mod);
    TrackerHelper::Entrypoint(BML_MOD_ENTRYPOINT_DETACH, &detach_args);
}

// ============================================================================
// Module Framework: Failure Paths
// ============================================================================

TEST_F(ModuleFrameworkIntegrationTest, Attach_NullArgs) {
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT,
              TrackerHelper::Entrypoint(BML_MOD_ENTRYPOINT_ATTACH, nullptr));
}

TEST_F(ModuleFrameworkIntegrationTest, Attach_WrongStructSize) {
    auto args = MakeAttachArgs();
    args.struct_size = 0;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT,
              TrackerHelper::Entrypoint(BML_MOD_ENTRYPOINT_ATTACH, &args));
}

TEST_F(ModuleFrameworkIntegrationTest, Attach_NullMod) {
    auto args = MakeAttachArgs();
    args.mod = nullptr;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT,
              TrackerHelper::Entrypoint(BML_MOD_ENTRYPOINT_ATTACH, &args));
}

TEST_F(ModuleFrameworkIntegrationTest, Attach_NullGetProc) {
    auto args = MakeAttachArgs();
    args.get_proc = nullptr;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT,
              TrackerHelper::Entrypoint(BML_MOD_ENTRYPOINT_ATTACH, &args));
}

TEST_F(ModuleFrameworkIntegrationTest, Attach_VersionMismatch) {
    auto args = MakeAttachArgs();
    args.api_version = 0;  // Below BML_MOD_ENTRYPOINT_API_VERSION (1)
    EXPECT_EQ(BML_RESULT_VERSION_MISMATCH,
              TrackerHelper::Entrypoint(BML_MOD_ENTRYPOINT_ATTACH, &args));
}

TEST_F(ModuleFrameworkIntegrationTest, Detach_NullArgs) {
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT,
              TrackerHelper::Entrypoint(BML_MOD_ENTRYPOINT_DETACH, nullptr));
}

TEST_F(ModuleFrameworkIntegrationTest, Detach_WrongStructSize) {
    auto args = MakeDetachArgs();
    args.struct_size = 0;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT,
              TrackerHelper::Entrypoint(BML_MOD_ENTRYPOINT_DETACH, &args));
}

TEST_F(ModuleFrameworkIntegrationTest, UnknownCommand) {
    auto args = MakeAttachArgs();
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT,
              TrackerHelper::Entrypoint(static_cast<BML_ModEntrypointCommand>(99), &args));
}

// ============================================================================
// Module Framework: OnAttach Failure Rollback
// ============================================================================

TEST_F(ModuleFrameworkIntegrationTest, OnAttachFailure_Rollback) {
    FailingMod::Reset();

    auto attach_args = MakeAttachArgs();
    BML_Result res = FailingHelper::Entrypoint(BML_MOD_ENTRYPOINT_ATTACH, &attach_args);

    EXPECT_EQ(BML_RESULT_FAIL, res);
    EXPECT_EQ(1, FailingMod::attach_count.load());
    // OnDetach should NOT be called on attach failure
    EXPECT_EQ(0, FailingMod::detach_count.load());
    // Instance should be null (deleted during rollback)
    EXPECT_EQ(nullptr, FailingHelper::GetInstance());
}

TEST_F(ModuleFrameworkIntegrationTest, OnAttachFailure_RollbackCleansPublishedInterface) {
    EnsureApiLoaded();
    BML_Mod hostMod = LookupHostMod();
    ASSERT_NE(nullptr, hostMod);

    auto attach_args = MakeAttachArgs(hostMod);
    EXPECT_EQ(BML_RESULT_FAIL,
              FailingPublishingHelper::Entrypoint(BML_MOD_ENTRYPOINT_ATTACH, &attach_args));
    EXPECT_EQ(nullptr, FailingPublishingHelper::GetInstance());

    EnsureApiLoaded();
    auto moduleApi = bml::Acquire<BML_CoreModuleInterface>();
    ASSERT_TRUE(static_cast<bool>(moduleApi));
    bml::CurrentModuleScope scope(hostMod, moduleApi.Get());
    auto lease = bml::AcquireInterface<TestPublishedService>("test.framework.rollback_published", 1);
    EXPECT_FALSE(static_cast<bool>(lease));
}

// ============================================================================
// Module Framework: Subscription Cleanup on Detach
// ============================================================================

TEST_F(ModuleFrameworkIntegrationTest, SubscriptionManager_CleanupOnDetach) {
    SubscriberMod::Reset();
    EnsureApiLoaded();
    auto imcBus = AcquireImcBus();
    ASSERT_TRUE(static_cast<bool>(imcBus));
    BML_Mod hostMod = LookupHostMod();
    ASSERT_NE(nullptr, hostMod);

    auto attach_args = MakeAttachArgs(hostMod);
    ASSERT_EQ(BML_RESULT_OK,
              SubscriberHelper::Entrypoint(BML_MOD_ENTRYPOINT_ATTACH, &attach_args));

    BML_TopicId topic_id = 0;
    ASSERT_EQ(BML_RESULT_OK, imcBus->GetTopicId("test/framework/event", &topic_id));

    uint32_t payload = 42;
    ASSERT_EQ(BML_RESULT_OK, imcBus->Publish(topic_id, &payload, sizeof(payload)));
    imcBus->Pump(100);

    EXPECT_EQ(1, SubscriberMod::message_count.load());

    // Detach - SubscriptionManager destructor should unsubscribe
    auto detach_args = MakeDetachArgs(hostMod);
    ASSERT_EQ(BML_RESULT_OK,
              SubscriberHelper::Entrypoint(BML_MOD_ENTRYPOINT_DETACH, &detach_args));

    EnsureApiLoaded();
    imcBus = AcquireImcBus();
    ASSERT_TRUE(static_cast<bool>(imcBus));

    ASSERT_EQ(BML_RESULT_OK, imcBus->Publish(topic_id, &payload, sizeof(payload)));
    imcBus->Pump(100);

    EXPECT_EQ(1, SubscriberMod::message_count.load()) << "Message received after detach";
}

// ============================================================================
// C++ Wrapper: IMC SubscriptionManager
// ============================================================================

TEST_F(ModuleFrameworkIntegrationTest, CppWrapper_SubscriptionManager_PubSub) {
    EnsureApiLoaded();
    auto imcBus = AcquireImcBus();
    auto moduleApi = bml::Acquire<BML_CoreModuleInterface>();
    ASSERT_TRUE(static_cast<bool>(imcBus));
    ASSERT_TRUE(static_cast<bool>(moduleApi));

    BML_Mod hostMod = LookupHostMod();
    ASSERT_NE(nullptr, hostMod);

    std::atomic<int> received{0};
    uint32_t last_value = 0;

    bml::imc::SubscriptionManager subs(imcBus.Get());
    {
        bml::CurrentModuleScope scope(hostMod, moduleApi.Get());
        bool ok = subs.Add("test/cpp/wrapper/event",
                           [&](const bml::imc::Message &msg) {
                               if (auto *v = msg.As<uint32_t>()) {
                                   last_value = *v;
                               }
                               received.fetch_add(1, std::memory_order_relaxed);
                           });
        ASSERT_TRUE(ok);
        EXPECT_EQ(1u, subs.Count());

        // Publish via C++ wrapper
        ASSERT_TRUE(bml::imc::publish("test/cpp/wrapper/event", uint32_t(99), imcBus.Get()));
        bml::imc::pumpAll(imcBus.Get());
    }

    EXPECT_EQ(1, received.load());
    EXPECT_EQ(99u, last_value);

    // Clear and verify no more messages
    subs.Clear();
    EXPECT_EQ(0u, subs.Count());

    {
        bml::CurrentModuleScope scope(hostMod, moduleApi.Get());
        bml::imc::publish("test/cpp/wrapper/event", uint32_t(100), imcBus.Get());
        bml::imc::pumpAll(imcBus.Get());
    }
    EXPECT_EQ(1, received.load()) << "Received after Clear()";
}

TEST_F(ModuleFrameworkIntegrationTest, CppWrapper_TypedSubscription) {
    EnsureApiLoaded();
    auto imcBus = AcquireImcBus();
    auto moduleApi = bml::Acquire<BML_CoreModuleInterface>();
    ASSERT_TRUE(static_cast<bool>(imcBus));
    ASSERT_TRUE(static_cast<bool>(moduleApi));

    BML_Mod hostMod = LookupHostMod();
    ASSERT_NE(nullptr, hostMod);

    struct TestEvent {
        int32_t x;
        int32_t y;
    };

    TestEvent received_event{};
    std::atomic<int> count{0};

    bml::imc::SubscriptionManager subs(imcBus.Get());
    {
        bml::CurrentModuleScope scope(hostMod, moduleApi.Get());
        bool ok = subs.Add<TestEvent>("test/cpp/typed",
                                      [&](const TestEvent &e) {
                                          received_event = e;
                                          count.fetch_add(1, std::memory_order_relaxed);
                                      });
        ASSERT_TRUE(ok);

        TestEvent sent = {10, 20};
        bml::imc::publish("test/cpp/typed", sent, imcBus.Get());
        bml::imc::pumpAll(imcBus.Get());
    }

    EXPECT_EQ(1, count.load());
    EXPECT_EQ(10, received_event.x);
    EXPECT_EQ(20, received_event.y);
}

// ============================================================================
// C++ Wrapper: Service
// ============================================================================

TEST_F(ModuleFrameworkIntegrationTest, CppWrapper_Service_ApiResolved) {
    EnsureApiLoaded();

    EXPECT_NE(nullptr, reinterpret_cast<void *>(bmlInterfaceAcquire));
    EXPECT_NE(nullptr, reinterpret_cast<void *>(bmlInterfaceRelease));
}

TEST_F(ModuleFrameworkIntegrationTest, CppWrapper_Service_AcquireNotFound) {
    EnsureApiLoaded();

    auto lease = bml::AcquireInterface<int>("test.framework.missing", 1, 0, 0);
    EXPECT_FALSE(static_cast<bool>(lease));
}

TEST_F(ModuleFrameworkIntegrationTest, CppWrapper_Service_AcquireImcBusViaTrait) {
    EnsureApiLoaded();
    BML_Mod hostMod = LookupHostMod();
    ASSERT_NE(nullptr, hostMod);

    bml::CurrentModuleScope scope(hostMod);

    auto lease = bml::Acquire<BML_ImcBusInterface>();
    ASSERT_TRUE(static_cast<bool>(lease));
    EXPECT_NE(nullptr, lease->PublishInterceptable);
}

TEST_F(ModuleFrameworkIntegrationTest, ModuleAcquire_ReturnsBuiltinInterfaceLease) {
    EnsureApiLoaded();
    BML_Mod hostMod = LookupHostMod();
    ASSERT_NE(nullptr, hostMod);

    HelperAccessMod helper;
    helper.Initialize(hostMod, bmlGetProcAddress);

    bml::CurrentModuleScope scope(hostMod);
    auto lease = helper.Acquire<BML_CoreLoggingInterface>(BML_CORE_LOGGING_INTERFACE_ID, 1);
    ASSERT_TRUE(static_cast<bool>(lease));
    ASSERT_NE(nullptr, lease->Log);
}

TEST_F(ModuleFrameworkIntegrationTest, ModuleAcquire_ReturnsEmptyForMissingInterface) {
    EnsureApiLoaded();

    BML_Mod_T mod;
    mod.id = "test.framework.missing";

    HelperAccessMod helper;
    helper.Initialize(&mod, bmlGetProcAddress);

    bml::CurrentModuleScope scope(&mod);
    auto lease = helper.Acquire<int>("test.framework.nope", 1);
    EXPECT_FALSE(static_cast<bool>(lease));
}

TEST_F(ModuleFrameworkIntegrationTest, ModuleAcquire_ReturnsEmptyForVersionMismatch) {
    EnsureApiLoaded();

    BML_Mod_T mod;
    mod.id = "test.framework.version";

    HelperAccessMod helper;
    helper.Initialize(&mod, bmlGetProcAddress);

    bml::CurrentModuleScope scope(&mod);
    auto lease = helper.Acquire<BML_CoreLoggingInterface>(BML_CORE_LOGGING_INTERFACE_ID, 2);
    EXPECT_FALSE(static_cast<bool>(lease));
}

TEST_F(ModuleFrameworkIntegrationTest, ModulePublish_UsesGetProcWithoutBootstrapRegisterGlobals) {
    PublishingMod *instance = nullptr;

    auto setCurrentModule = reinterpret_cast<PFN_BML_SetCurrentModule>(
        bmlGetProcAddress("bmlSetCurrentModule"));
    ASSERT_NE(nullptr, setCurrentModule);

    BML_Mod trackedMod = LookupHostMod();
    ASSERT_NE(nullptr, trackedMod);
    auto attach_args = MakeAttachArgs(trackedMod);

    ASSERT_EQ(BML_RESULT_OK, setCurrentModule(trackedMod));
    ASSERT_EQ(BML_RESULT_OK, PublishingHelper::Entrypoint(BML_MOD_ENTRYPOINT_ATTACH, &attach_args));
    ASSERT_EQ(BML_RESULT_OK, setCurrentModule(nullptr));
    instance = PublishingHelper::GetInstance();
    ASSERT_NE(nullptr, instance);

    ASSERT_TRUE(static_cast<bool>(instance->m_Published));

    ASSERT_EQ(BML_RESULT_OK, setCurrentModule(trackedMod));
    auto lease = bml::AcquireInterface<TestPublishedService>("test.framework.published", 1);
    ASSERT_TRUE(static_cast<bool>(lease));
    EXPECT_EQ(77, lease->GetValue());
    lease.Reset();
    ASSERT_EQ(BML_RESULT_OK, setCurrentModule(nullptr));

    auto detach_args = MakeDetachArgs(trackedMod);
    ASSERT_EQ(BML_RESULT_OK, setCurrentModule(trackedMod));
    ASSERT_EQ(BML_RESULT_OK, PublishingHelper::Entrypoint(BML_MOD_ENTRYPOINT_DETACH, &detach_args));
    ASSERT_EQ(BML_RESULT_OK, setCurrentModule(nullptr));
    EXPECT_EQ(nullptr, PublishingHelper::GetInstance());

    EnsureApiLoaded();

    ASSERT_EQ(BML_RESULT_OK, setCurrentModule(trackedMod));
    auto detached_lease = bml::AcquireInterface<TestPublishedService>("test.framework.published", 1);
    EXPECT_FALSE(static_cast<bool>(detached_lease));
    ASSERT_EQ(BML_RESULT_OK, setCurrentModule(nullptr));
}

// ============================================================================
// C++ Wrapper: Config (no module context -- verifies API availability and graceful failure)
// ============================================================================

TEST_F(ModuleFrameworkIntegrationTest, CppWrapper_Config_ApiResolved) {
    EnsureApiLoaded();
    auto configApi = AcquireCoreConfig();
    ASSERT_TRUE(static_cast<bool>(configApi));
    EXPECT_NE(nullptr, configApi->Get);
    EXPECT_NE(nullptr, configApi->Set);
    EXPECT_NE(nullptr, configApi->Reset);
}

TEST_F(ModuleFrameworkIntegrationTest, CppWrapper_Config_NullModGracefulFailure) {
    EnsureApiLoaded();
    auto configApi = AcquireCoreConfig();
    ASSERT_TRUE(static_cast<bool>(configApi));

    // Config with null mod and no current module context should fail gracefully
    bml::Config config(nullptr, configApi.Get());
    EXPECT_FALSE(config.SetString("test", "key", "val"));
    EXPECT_FALSE(config.GetString("test", "key").has_value());
    EXPECT_FALSE(config.SetInt("test", "key", 42));
    EXPECT_FALSE(config.GetInt("test", "key").has_value());
    EXPECT_FALSE(config.SetFloat("test", "key", 1.0f));
    EXPECT_FALSE(config.GetFloat("test", "key").has_value());
    EXPECT_FALSE(config.SetBool("test", "key", true));
    EXPECT_FALSE(config.GetBool("test", "key").has_value());
}

// Config capabilities test removed -- per-subsystem caps eliminated

// ============================================================================
// C++ Wrapper: Logger (smoke test - no crash)
// ============================================================================

TEST_F(ModuleFrameworkIntegrationTest, CppWrapper_Logger_AllLevels) {
    EnsureApiLoaded();
    auto contextApi = AcquireCoreContext();
    auto loggingApi = AcquireCoreLogging();
    ASSERT_TRUE(static_cast<bool>(contextApi));
    ASSERT_TRUE(static_cast<bool>(loggingApi));

    auto ctx = bml::GetGlobalContext(contextApi.Get());
    bml::Logger logger(ctx, "TestMod", loggingApi.Get());

    // These should not crash
    logger.Trace("trace message %d", 1);
    logger.Debug("debug message %d", 2);
    logger.Info("info message %d", 3);
    logger.Warn("warn message %d", 4);
    logger.Error("error message %d", 5);
}

// ============================================================================
// C++ Wrapper: IMC Statistics
// ============================================================================

TEST_F(ModuleFrameworkIntegrationTest, CppWrapper_ImcStatistics) {
    EnsureApiLoaded();
    auto imcBus = AcquireImcBus();
    ASSERT_TRUE(static_cast<bool>(imcBus));

    auto stats = bml::imc::getStats(imcBus.Get());
    ASSERT_TRUE(stats.has_value());
    // After bootstrap, at least the topics we create should exist
    // Just verify the API returns valid data
    EXPECT_GE(stats->active_topics, 0u);
}

// ============================================================================
// C++ Wrapper: IMC Capabilities
// ============================================================================

// IMC capabilities test removed -- per-subsystem caps eliminated

// ============================================================================
// C++ Wrapper: Context
// ============================================================================

TEST_F(ModuleFrameworkIntegrationTest, CppWrapper_GetGlobalContext) {
    EnsureApiLoaded();
    auto contextApi = AcquireCoreContext();
    ASSERT_TRUE(static_cast<bool>(contextApi));

    auto ctx = bml::GetGlobalContext(contextApi.Get());
    EXPECT_TRUE(ctx);
    EXPECT_NE(nullptr, ctx.Handle());
}

TEST_F(ModuleFrameworkIntegrationTest, CppWrapper_RuntimeVersion) {
    EnsureApiLoaded();
    auto contextApi = AcquireCoreContext();
    ASSERT_TRUE(static_cast<bool>(contextApi));

    auto ver = bml::GetRuntimeVersion(contextApi.Get());
    ASSERT_TRUE(ver.has_value());
    // Version should be non-zero (actual version is set by build system)
    EXPECT_GT(ver->major + ver->minor + ver->patch, 0u);
}

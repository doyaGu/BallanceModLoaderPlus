#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "Core/ApiRegistry.h"
#include "Core/ApiRegistration.h"
#include "Core/ConfigStore.h"
#include "Core/Context.h"
#include "Core/CrashDumpWriter.h"
#include "Core/FaultTracker.h"
#include "Core/ImcBus.h"
#include "Core/InterfaceRegistry.h"
#include "Core/LeaseManager.h"
#include "Core/LocaleManager.h"
#include "Core/ModHandle.h"
#include "Core/ModManifest.h"
#include "Core/ResourceApi.h"
#include "Core/TimerManager.h"
#include "TestKernel.h"

#include "bml_config.h"
#include "bml_errors.h"
#include "bml_hook.h"
#include "bml_imc.h"
#include "bml_interface.h"
#include "bml_locale.h"
#include "bml_logging.h"
#include "bml_resource.h"
#include "bml_timer.h"
#include "bml_version.h"

using BML::Core::ApiRegistry;
using BML::Core::ConfigStore;
using BML::Core::Context;
using BML::Core::Testing::TestKernel;
using namespace BML::Core;

namespace BML::Core {
void RegisterMemoryApis() {}
void RegisterDiagnosticApis() {}
void RegisterSyncApis() {}
void RegisterTracingApis() {}
void RegisterProfilingApis() {}
}

namespace {

struct ConfigHookCapture {
    static constexpr char kPre = 'A';
    static constexpr char kPost = 'B';

    std::mutex mutex;
    std::vector<char> phases;
};

class CoreApisTests : public ::testing::Test {
protected:
    std::vector<std::unique_ptr<BML::Core::ModManifest>> manifests_;
    TestKernel kernel_;

    void SetUp() override {
        kernel_->api_registry = std::make_unique<ApiRegistry>();
        kernel_->config = std::make_unique<ConfigStore>();
        kernel_->crash_dump = std::make_unique<CrashDumpWriter>();
        kernel_->fault_tracker = std::make_unique<FaultTracker>();
        kernel_->imc_bus = std::make_unique<ImcBus>();
        kernel_->context = std::make_unique<Context>(*kernel_->api_registry, *kernel_->config, *kernel_->crash_dump, *kernel_->fault_tracker);
        kernel_->leases = std::make_unique<LeaseManager>();
        kernel_->interface_registry = std::make_unique<InterfaceRegistry>(*kernel_->context, *kernel_->leases);
        kernel_->locale = std::make_unique<LocaleManager>();
        kernel_->timers = std::make_unique<TimerManager>(*kernel_->context);
        kernel_->config->BindContext(*kernel_->context);

        Context::SetCurrentModule(nullptr);
        kernel_->context->Initialize(bmlGetApiVersion());
        kernel_->imc_bus->BindDeps(*kernel_->context);
        Context::SetCurrentModule(nullptr);
        temp_root_ = std::filesystem::temp_directory_path() /
                     ("bml-coreapis-tests-" + std::to_string(test_counter_.fetch_add(1, std::memory_order_relaxed)));
        std::filesystem::create_directories(temp_root_);
    }

    void TearDown() override {
        Context::SetCurrentModule(nullptr);
        std::error_code ec;
        std::filesystem::remove_all(temp_root_, ec);
    }

    BML_Mod CreateTrackedMod(const std::string &id) {
        auto manifest = std::make_unique<BML::Core::ModManifest>();
        manifest->package.id = id;
        manifest->package.name = id;
        manifest->package.version = "1.0.0";
        manifest->package.parsed_version = {1, 0, 0};
        manifest->directory = (temp_root_ / id).wstring();
        std::filesystem::create_directories(manifest->directory);
        manifest->manifest_path = manifest->directory + L"/manifest.toml";

        auto handle = kernel_->context->CreateModHandle(*manifest);
        BML::Core::LoadedModule module{};
        module.id = id;
        module.manifest = manifest.get();
        module.mod_handle = std::move(handle);
        kernel_->context->AddLoadedModule(std::move(module));

        BML_Mod mod = kernel_->context->GetModHandleById(id);
        manifests_.push_back(std::move(manifest));
        return mod;
    }

    void InitConfigBackedMod(const std::string &id) {
        mod_handle_ = CreateTrackedMod(id);
        Context::SetCurrentModule(mod_handle_);
    }

    BML_Mod Mod() const { return mod_handle_; }

    static std::atomic<uint64_t> test_counter_;
    std::filesystem::path temp_root_;
    BML_Mod mod_handle_{nullptr};
};

std::atomic<uint64_t> CoreApisTests::test_counter_{1};

TEST_F(CoreApisTests, MultiThreadedHandleCreationAndRelease) {
    BML::Core::RegisterResourceApis();

    using PFN_Create = BML_Result (*)(BML_Mod, BML_HandleType, BML_HandleDesc *);
    using PFN_Release = BML_Result (*)(const BML_HandleDesc *);
    using PFN_Validate = BML_Result (*)(const BML_HandleDesc *, BML_Bool *);

    auto create_fn = reinterpret_cast<PFN_Create>(kernel_->api_registry->Get("bmlHandleCreate"));
    auto release_fn = reinterpret_cast<PFN_Release>(kernel_->api_registry->Get("bmlHandleRelease"));
    auto validate_fn = reinterpret_cast<PFN_Validate>(kernel_->api_registry->Get("bmlHandleValidate"));

    ASSERT_NE(create_fn, nullptr);
    ASSERT_NE(release_fn, nullptr);
    ASSERT_NE(validate_fn, nullptr);

    std::atomic<int> finalize_count{0};
    BML_ResourceTypeDesc type_desc{};
    type_desc.struct_size = sizeof(BML_ResourceTypeDesc);
    type_desc.name = "coreapis.test.handle";
    type_desc.on_finalize = [](BML_Context, const BML_HandleDesc *, void *user_data) {
        auto *count = static_cast<std::atomic<int> *>(user_data);
        count->fetch_add(1, std::memory_order_relaxed);
    };
    type_desc.user_data = &finalize_count;

    auto owner = CreateTrackedMod("coreapis.test.handle.owner");
    ASSERT_NE(owner, nullptr);

    BML_HandleType handle_type = 0;
    ASSERT_EQ(BML_RESULT_OK, BML::Core::RegisterResourceType(owner, &type_desc, &handle_type));

    constexpr size_t kThreads = 8;
    constexpr size_t kPerThread = 256;

    std::vector<std::vector<BML_HandleDesc>> handles(kThreads);
    std::vector<std::thread> creators;
    creators.reserve(kThreads);

    for (size_t t = 0; t < kThreads; ++t) {
        creators.emplace_back([&, index = t] {
            handles[index].reserve(kPerThread);
            for (size_t i = 0; i < kPerThread; ++i) {
                BML_HandleDesc desc{};
                desc.struct_size = sizeof(BML_HandleDesc);
                ASSERT_EQ(BML_RESULT_OK, create_fn(owner, handle_type, &desc));
                BML_Bool valid = BML_FALSE;
                ASSERT_EQ(BML_RESULT_OK, validate_fn(&desc, &valid));
                ASSERT_EQ(BML_TRUE, valid);
                handles[index].push_back(desc);
            }
        });
    }

    for (auto &thread : creators) {
        thread.join();
    }

    std::vector<std::thread> releasers;
    releasers.reserve(kThreads);
    for (size_t t = 0; t < kThreads; ++t) {
        releasers.emplace_back([&, index = t] {
            for (const auto &desc : handles[index]) {
                ASSERT_EQ(BML_RESULT_OK, release_fn(&desc));
            }
        });
    }

    for (auto &thread : releasers) {
        thread.join();
    }

    EXPECT_EQ(finalize_count.load(), static_cast<int>(kThreads * kPerThread));
}

TEST_F(CoreApisTests, ConfigReloadStressTriggersHooksOncePerLoad) {
    InitConfigBackedMod("coreapis.config");
    BML::Core::RegisterConfigApis();

    using PFN_ConfigSet = BML_Result (*)(BML_Mod, const BML_ConfigKey *, const BML_ConfigValue *);
    using PFN_ConfigGet = BML_Result (*)(BML_Mod, const BML_ConfigKey *, BML_ConfigValue *);

    auto config_set = reinterpret_cast<PFN_ConfigSet>(kernel_->api_registry->Get("bmlConfigSet"));
    auto config_get = reinterpret_cast<PFN_ConfigGet>(kernel_->api_registry->Get("bmlConfigGet"));

    ASSERT_NE(config_set, nullptr);
    ASSERT_NE(config_get, nullptr);

    ConfigHookCapture hook_capture;
    BML_ConfigLoadHooks hooks{};
    hooks.struct_size = sizeof(BML_ConfigLoadHooks);
    hooks.on_pre_load = [](BML_Context, const BML_ConfigLoadContext *, void *user_data) {
        auto *capture = static_cast<ConfigHookCapture *>(user_data);
        std::lock_guard<std::mutex> lock(capture->mutex);
        capture->phases.push_back(ConfigHookCapture::kPre);
    };
    hooks.on_post_load = [](BML_Context, const BML_ConfigLoadContext *, void *user_data) {
        auto *capture = static_cast<ConfigHookCapture *>(user_data);
        std::lock_guard<std::mutex> lock(capture->mutex);
        capture->phases.push_back(ConfigHookCapture::kPost);
    };
    hooks.user_data = &hook_capture;
    ASSERT_EQ(BML_RESULT_OK, BML::Core::RegisterConfigLoadHooks(Mod(), &hooks));

    size_t baseline = 0;
    {
        std::lock_guard<std::mutex> lock(hook_capture.mutex);
        baseline = hook_capture.phases.size();
    }

    constexpr size_t kIterations = 5;
    constexpr size_t kReaders = 4;
    const BML_ConfigKey key{sizeof(BML_ConfigKey), "stress", "value"};

    for (size_t iter = 0; iter < kIterations; ++iter) {
        BML_ConfigValue value{};
        value.struct_size = sizeof(BML_ConfigValue);
        value.type = BML_CONFIG_INT;
        value.data.int_value = static_cast<int32_t>(iter);
        ASSERT_EQ(BML_RESULT_OK, config_set(Mod(), &key, &value));

        kernel_->config->FlushAndRelease(Mod());

        std::vector<std::thread> readers;
        std::atomic<size_t> success_count{0};
        for (size_t r = 0; r < kReaders; ++r) {
            readers.emplace_back([&, expected = static_cast<int32_t>(iter)] {
                BML_ConfigValue read{};
                read.struct_size = sizeof(BML_ConfigValue);
                ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key, &read));
                ASSERT_EQ(BML_CONFIG_INT, read.type);
                EXPECT_EQ(expected, read.data.int_value);
                success_count.fetch_add(1, std::memory_order_relaxed);
            });
        }
        for (auto &reader : readers) {
            reader.join();
        }
        EXPECT_EQ(success_count.load(), kReaders);
    }

    std::vector<char> new_events;
    {
        std::lock_guard<std::mutex> lock(hook_capture.mutex);
        if (hook_capture.phases.size() > baseline) {
            new_events.assign(hook_capture.phases.begin() + baseline, hook_capture.phases.end());
        }
    }

    const size_t expected_loads = kIterations + 1; // initial set + per-iteration reload
    ASSERT_EQ(new_events.size(), expected_loads * 2);
    for (size_t i = 0; i < expected_loads; ++i) {
        EXPECT_EQ(new_events[2 * i], ConfigHookCapture::kPre);
        EXPECT_EQ(new_events[2 * i + 1], ConfigHookCapture::kPost);
    }
}

TEST_F(CoreApisTests, ConfigStoreRequiresExplicitModuleOutsideApiShim) {
    InitConfigBackedMod("coreapis.config.explicit");

    const BML_ConfigKey key{sizeof(BML_ConfigKey), "config", "value"};
    BML_ConfigValue value{};
    value.struct_size = sizeof(BML_ConfigValue);
    value.type = BML_CONFIG_INT;
    value.data.int_value = 17;

    BML_ConfigValue out{};
    out.struct_size = sizeof(BML_ConfigValue);

    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, kernel_->config->SetValue(nullptr, &key, &value));
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, kernel_->config->GetValue(nullptr, &key, &out));

    BML_ConfigBatch batch = nullptr;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, kernel_->config->BatchBegin(nullptr, &batch));
}

TEST_F(CoreApisTests, ConfigAndCoreApisRequireExplicitModuleHandles) {
    InitConfigBackedMod("coreapis.strict.boundary");
    BML::Core::RegisterConfigApis();
    BML::Core::RegisterCoreApis();

    using PFN_ConfigSet = BML_Result (*)(BML_Mod, const BML_ConfigKey *, const BML_ConfigValue *);
    using PFN_ConfigGet = BML_Result (*)(BML_Mod, const BML_ConfigKey *, BML_ConfigValue *);
    using PFN_GetModId = BML_Result (*)(BML_Mod, const char **);

    auto config_set = reinterpret_cast<PFN_ConfigSet>(kernel_->api_registry->Get("bmlConfigSet"));
    auto config_get = reinterpret_cast<PFN_ConfigGet>(kernel_->api_registry->Get("bmlConfigGet"));
    auto register_hooks = reinterpret_cast<PFN_BML_RegisterConfigLoadHooks>(
        kernel_->api_registry->Get("bmlRegisterConfigLoadHooks"));
    auto get_mod_id = reinterpret_cast<PFN_GetModId>(kernel_->api_registry->Get("bmlGetModId"));

    ASSERT_NE(config_set, nullptr);
    ASSERT_NE(config_get, nullptr);
    ASSERT_NE(register_hooks, nullptr);
    ASSERT_NE(get_mod_id, nullptr);

    const BML_ConfigKey key{sizeof(BML_ConfigKey), "legacy", "value"};
    BML_ConfigValue value{};
    value.struct_size = sizeof(BML_ConfigValue);
    value.type = BML_CONFIG_INT;
    value.data.int_value = 29;

    BML_ConfigValue out{};
    out.struct_size = sizeof(BML_ConfigValue);
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, config_set(nullptr, &key, &value));
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, config_get(nullptr, &key, &out));

    BML_ConfigLoadHooks hooks{};
    hooks.struct_size = sizeof(BML_ConfigLoadHooks);
    hooks.on_pre_load = [](BML_Context, const BML_ConfigLoadContext *, void *) {};
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, register_hooks(nullptr, &hooks));

    const char *mod_id = nullptr;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, get_mod_id(nullptr, &mod_id));
}

struct OrderingCapture {
    std::mutex mutex;
    std::vector<uint32_t> values;
};

void OrderingHandler(BML_Context ctx,
                     BML_TopicId topic,
                     const BML_ImcMessage *message,
                     void *user_data) {
    (void)ctx;
    (void)topic;
    if (!message || !message->data || message->size != sizeof(uint32_t) || !user_data)
        return;
    uint32_t value = 0;
    std::memcpy(&value, message->data, sizeof(value));
    auto *capture = static_cast<OrderingCapture *>(user_data);
    std::lock_guard<std::mutex> lock(capture->mutex);
    capture->values.push_back(value);
}

TEST_F(CoreApisTests, ImcBroadcastPreservesPublishOrderPerSubscriber) {
    auto owner = CreateTrackedMod("coreapis.imc.order");
    ASSERT_NE(owner, nullptr);
    Context::SetCurrentModule(owner);

    OrderingCapture first{};
    OrderingCapture second{};

    BML_Subscription sub1 = nullptr;
    BML_Subscription sub2 = nullptr;
    
    BML_TopicId topic_id = 0;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("order.topic", &topic_id));
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(owner, topic_id, OrderingHandler, &first, &sub1));
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(owner, topic_id, OrderingHandler, &second, &sub2));

    constexpr size_t kMessages = 256;
    for (size_t i = 0; i < kMessages; ++i) {
        uint32_t payload = static_cast<uint32_t>(i);
        ASSERT_EQ(BML_RESULT_OK, ImcPublish(owner, topic_id, &payload, sizeof(payload)));
    }

    ImcPump(0);

    auto validate = [kMessages](OrderingCapture &capture) {
        std::lock_guard<std::mutex> lock(capture.mutex);
        ASSERT_EQ(capture.values.size(), kMessages);
        for (size_t i = 0; i < kMessages; ++i) {
            EXPECT_EQ(capture.values[i], i);
        }
    };

    validate(first);
    validate(second);

    ASSERT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub1));
    ASSERT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub2));
    Context::SetCurrentModule(nullptr);
}

TEST_F(CoreApisTests, CurrentModuleApisAreAbsentFromTheRegistry) {
    BML::Core::RegisterCoreApis();

    EXPECT_EQ(nullptr, kernel_->api_registry->Get("bmlSetCurrentModule"));
    EXPECT_EQ(nullptr, kernel_->api_registry->Get("bmlGetCurrentModule"));
}

TEST_F(CoreApisTests, InterfaceApisAcquireWithoutTls) {
    BML::Core::RegisterInterfaceApis();

    auto register_fn = reinterpret_cast<PFN_BML_InterfaceRegister>(
        kernel_->api_registry->Get("bmlInterfaceRegister"));
    auto acquire_fn = reinterpret_cast<PFN_BML_InterfaceAcquire>(
        kernel_->api_registry->Get("bmlInterfaceAcquire"));
    auto release_fn = reinterpret_cast<PFN_BML_InterfaceRelease>(
        kernel_->api_registry->Get("bmlInterfaceRelease"));
    auto unregister_fn = reinterpret_cast<PFN_BML_InterfaceUnregister>(
        kernel_->api_registry->Get("bmlInterfaceUnregister"));
    ASSERT_NE(register_fn, nullptr);
    ASSERT_NE(acquire_fn, nullptr);
    ASSERT_NE(release_fn, nullptr);
    ASSERT_NE(unregister_fn, nullptr);

    auto owner = CreateTrackedMod("coreapis.interface.owner");
    ASSERT_NE(owner, nullptr);
    Context::SetCurrentModule(nullptr);

    static int implementation = 42;
    BML_InterfaceDesc desc = BML_INTERFACE_DESC_INIT;
    desc.interface_id = "coreapis.owned.interface";
    desc.abi_version = bmlMakeVersion(1, 0, 0);
    desc.implementation = &implementation;
    desc.implementation_size = sizeof(implementation);

    ASSERT_EQ(BML_RESULT_OK, register_fn(owner, &desc));

    const void *loaded = nullptr;
    BML_InterfaceLease lease = nullptr;
    const BML_Version req = bmlMakeVersion(1, 0, 0);
    ASSERT_EQ(BML_RESULT_OK, acquire_fn(owner, desc.interface_id, &req, &loaded, &lease));
    ASSERT_NE(nullptr, lease);
    ASSERT_EQ(&implementation, loaded);

    EXPECT_EQ(BML_RESULT_OK, release_fn(lease));
    EXPECT_EQ(BML_RESULT_OK, unregister_fn(owner, desc.interface_id));
}

TEST_F(CoreApisTests, TimerApisScheduleWithoutTls) {
    BML::Core::RegisterTimerApis();

    auto schedule_once = reinterpret_cast<PFN_BML_TimerScheduleOnce>(
        kernel_->api_registry->Get("bmlTimerScheduleOnce"));
    auto is_active = reinterpret_cast<PFN_BML_TimerIsActive>(
        kernel_->api_registry->Get("bmlTimerIsActive"));
    auto cancel_all = reinterpret_cast<PFN_BML_TimerCancelAll>(
        kernel_->api_registry->Get("bmlTimerCancelAll"));
    ASSERT_NE(schedule_once, nullptr);
    ASSERT_NE(is_active, nullptr);
    ASSERT_NE(cancel_all, nullptr);

    auto owner = CreateTrackedMod("coreapis.timer.owner");
    ASSERT_NE(owner, nullptr);
    Context::SetCurrentModule(nullptr);

    int fired = 0;
    BML_Timer timer = nullptr;
    ASSERT_EQ(BML_RESULT_OK,
              schedule_once(
                  owner,
                  100,
                  [](BML_Context, BML_Timer, void *user_data) {
                      auto *value = static_cast<int *>(user_data);
                      ++(*value);
                  },
                  &fired,
                  &timer));
    ASSERT_NE(nullptr, timer);

    BML_Bool active = BML_FALSE;
    ASSERT_EQ(BML_RESULT_OK, is_active(owner, timer, &active));
    EXPECT_EQ(BML_TRUE, active);

    ASSERT_EQ(BML_RESULT_OK, cancel_all(owner));
    ASSERT_EQ(BML_RESULT_OK, is_active(owner, timer, &active));
    EXPECT_EQ(BML_FALSE, active);
    EXPECT_EQ(0, fired);
}

TEST_F(CoreApisTests, ExplicitOwnerApisRejectNullOwner) {
    BML::Core::RegisterHookApis();
    BML::Core::RegisterInterfaceApis();
    BML::Core::RegisterLocaleApis();
    BML::Core::RegisterResourceApis();
    BML::Core::RegisterTimerApis();

    auto hook_register = reinterpret_cast<PFN_BML_HookRegister>(
        kernel_->api_registry->Get("bmlHookRegister"));
    auto interface_register = reinterpret_cast<PFN_BML_InterfaceRegister>(
        kernel_->api_registry->Get("bmlInterfaceRegister"));
    auto interface_acquire = reinterpret_cast<PFN_BML_InterfaceAcquire>(
        kernel_->api_registry->Get("bmlInterfaceAcquire"));
    auto locale_load = reinterpret_cast<PFN_BML_LocaleLoad>(
        kernel_->api_registry->Get("bmlLocaleLoad"));
    auto locale_get = reinterpret_cast<PFN_BML_LocaleGet>(
        kernel_->api_registry->Get("bmlLocaleGet"));
    auto locale_bind = reinterpret_cast<PFN_BML_LocaleBindTable>(
        kernel_->api_registry->Get("bmlLocaleBindTable"));
    auto register_resource_type = reinterpret_cast<PFN_BML_RegisterResourceType>(
        kernel_->api_registry->Get("bmlRegisterResourceType"));
    auto handle_create = reinterpret_cast<PFN_BML_HandleCreate>(
        kernel_->api_registry->Get("bmlHandleCreate"));
    auto timer_schedule_once = reinterpret_cast<PFN_BML_TimerScheduleOnce>(
        kernel_->api_registry->Get("bmlTimerScheduleOnce"));
    ASSERT_NE(hook_register, nullptr);
    ASSERT_NE(interface_register, nullptr);
    ASSERT_NE(interface_acquire, nullptr);
    ASSERT_NE(locale_load, nullptr);
    ASSERT_NE(locale_get, nullptr);
    ASSERT_NE(locale_bind, nullptr);
    ASSERT_NE(register_resource_type, nullptr);
    ASSERT_NE(handle_create, nullptr);
    ASSERT_NE(timer_schedule_once, nullptr);

    auto owner = CreateTrackedMod("coreapis.explicit.strict");
    ASSERT_NE(owner, nullptr);
    Context::SetCurrentModule(owner);

    BML_HookDesc hook_desc = BML_HOOK_DESC_INIT;
    hook_desc.target_name = "strict-hook";
    hook_desc.target_address = reinterpret_cast<void *>(0x1234);
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, hook_register(nullptr, &hook_desc));

    static int implementation = 7;
    BML_InterfaceDesc interface_desc = BML_INTERFACE_DESC_INIT;
    interface_desc.interface_id = "coreapis.explicit.strict.interface";
    interface_desc.abi_version = bmlMakeVersion(1, 0, 0);
    interface_desc.implementation = &implementation;
    interface_desc.implementation_size = sizeof(implementation);
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, interface_register(nullptr, &interface_desc));

    const void *loaded = nullptr;
    BML_InterfaceLease lease = nullptr;
    const auto required_abi = bmlMakeVersion(1, 0, 0);
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT,
              interface_acquire(
                  nullptr,
                  interface_desc.interface_id, &required_abi, &loaded, &lease));
    EXPECT_EQ(nullptr, loaded);
    EXPECT_EQ(nullptr, lease);

    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, locale_load(nullptr, "en"));
    EXPECT_EQ(nullptr, locale_get(nullptr, "missing"));

    BML_LocaleTable table = nullptr;
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, locale_bind(nullptr, &table));
    EXPECT_EQ(nullptr, table);

    BML_ResourceTypeDesc type_desc{};
    type_desc.struct_size = sizeof(BML_ResourceTypeDesc);
    type_desc.name = "coreapis.explicit.strict.resource";
    BML_HandleType handle_type = 0;
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, register_resource_type(nullptr, &type_desc, &handle_type));

    BML_HandleDesc handle_desc = BML_HANDLE_DESC_INIT;
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, handle_create(nullptr, 1, &handle_desc));

    BML_Timer timer = nullptr;
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT,
              timer_schedule_once(
                  nullptr,
                  100,
                  [](BML_Context, BML_Timer, void *) {},
                  nullptr,
                  &timer));
    EXPECT_EQ(nullptr, timer);

    Context::SetCurrentModule(nullptr);
}

TEST_F(CoreApisTests, LocaleApisLoadWithoutTls) {
    BML::Core::RegisterLocaleApis();

    auto load_fn = reinterpret_cast<PFN_BML_LocaleLoad>(
        kernel_->api_registry->Get("bmlLocaleLoad"));
    auto get_fn = reinterpret_cast<PFN_BML_LocaleGet>(
        kernel_->api_registry->Get("bmlLocaleGet"));
    auto bind_fn = reinterpret_cast<PFN_BML_LocaleBindTable>(
        kernel_->api_registry->Get("bmlLocaleBindTable"));
    auto lookup_fn = reinterpret_cast<PFN_BML_LocaleLookup>(
        kernel_->api_registry->Get("bmlLocaleLookup"));
    ASSERT_NE(load_fn, nullptr);
    ASSERT_NE(get_fn, nullptr);
    ASSERT_NE(bind_fn, nullptr);
    ASSERT_NE(lookup_fn, nullptr);

    auto owner = CreateTrackedMod("coreapis.locale.owner");
    ASSERT_NE(owner, nullptr);
    const auto locale_dir = temp_root_ / "coreapis.locale.owner" / "locale";
    std::filesystem::create_directories(locale_dir);
    {
        std::ofstream locale_file(locale_dir / "en.toml", std::ios::binary);
        ASSERT_TRUE(locale_file.is_open());
        locale_file << "greeting = \"Hello from owned locale\"\n";
    }

    Context::SetCurrentModule(nullptr);
    ASSERT_EQ(BML_RESULT_OK, load_fn(owner, "en"));
    EXPECT_STREQ("Hello from owned locale", get_fn(owner, "greeting"));

    BML_LocaleTable table = nullptr;
    ASSERT_EQ(BML_RESULT_OK, bind_fn(owner, &table));
    ASSERT_NE(nullptr, table);
    EXPECT_STREQ("Hello from owned locale", lookup_fn(table, "greeting"));
}

TEST_F(CoreApisTests, ResourceApisCreateWithoutTls) {
    BML::Core::RegisterResourceApis();

    auto register_type = reinterpret_cast<PFN_BML_RegisterResourceType>(
        kernel_->api_registry->Get("bmlRegisterResourceType"));
    auto handle_create = reinterpret_cast<PFN_BML_HandleCreate>(
        kernel_->api_registry->Get("bmlHandleCreate"));
    auto validate = reinterpret_cast<PFN_BML_HandleValidate>(
        kernel_->api_registry->Get("bmlHandleValidate"));
    auto release = reinterpret_cast<PFN_BML_HandleRelease>(
        kernel_->api_registry->Get("bmlHandleRelease"));
    ASSERT_NE(register_type, nullptr);
    ASSERT_NE(handle_create, nullptr);
    ASSERT_NE(validate, nullptr);
    ASSERT_NE(release, nullptr);

    auto owner = CreateTrackedMod("coreapis.resource.owner");
    ASSERT_NE(owner, nullptr);
    Context::SetCurrentModule(nullptr);

    BML_ResourceTypeDesc type_desc{};
    type_desc.struct_size = sizeof(BML_ResourceTypeDesc);
    type_desc.name = "coreapis.resource.owned";

    BML_HandleType handle_type = 0;
    ASSERT_EQ(BML_RESULT_OK, register_type(owner, &type_desc, &handle_type));

    BML_HandleDesc handle = BML_HANDLE_DESC_INIT;
    ASSERT_EQ(BML_RESULT_OK, handle_create(owner, handle_type, &handle));

    BML_Bool valid = BML_FALSE;
    ASSERT_EQ(BML_RESULT_OK, validate(&handle, &valid));
    EXPECT_EQ(BML_TRUE, valid);
    EXPECT_EQ(BML_RESULT_OK, release(&handle));
}

// ========================================================================
// Phase 1a: GetModDirectory
// ========================================================================

TEST_F(CoreApisTests, GetModDirectory_ReturnsUtf8Path) {
    BML::Core::RegisterCoreApis();

    auto fn = reinterpret_cast<BML_Result (*)(BML_Mod, const char **)>(
        kernel_->api_registry->Get("bmlGetModDirectory"));
    ASSERT_NE(fn, nullptr);

    auto mod = CreateTrackedMod("test.directory");
    const char *dir = nullptr;
    EXPECT_EQ(BML_RESULT_OK, fn(mod, &dir));
    ASSERT_NE(dir, nullptr);
    // Should contain the mod ID since temp directory includes it
    EXPECT_NE(std::string(dir).find("test.directory"), std::string::npos);
}

TEST_F(CoreApisTests, GetModDirectory_NullOutput) {
    BML::Core::RegisterCoreApis();

    auto fn = reinterpret_cast<BML_Result (*)(BML_Mod, const char **)>(
        kernel_->api_registry->Get("bmlGetModDirectory"));
    ASSERT_NE(fn, nullptr);

    auto mod = CreateTrackedMod("test.dir.null");
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, fn(mod, nullptr));
}

TEST_F(CoreApisTests, GetModDirectory_NullMod) {
    BML::Core::RegisterCoreApis();

    auto fn = reinterpret_cast<BML_Result (*)(BML_Mod, const char **)>(
        kernel_->api_registry->Get("bmlGetModDirectory"));
    ASSERT_NE(fn, nullptr);

    // Null module handles remain invalid when no explicit owner is provided.
    Context::SetCurrentModule(nullptr);
    const char *dir = nullptr;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, fn(nullptr, &dir));
}

TEST_F(CoreApisTests, GetModDirectory_HostModHasNoManifest) {
    BML::Core::RegisterCoreApis();

    auto fn = reinterpret_cast<BML_Result (*)(BML_Mod, const char **)>(
        kernel_->api_registry->Get("bmlGetModDirectory"));
    ASSERT_NE(fn, nullptr);

    // The synthetic host module has no manifest
    BML_Mod host = kernel_->context->GetSyntheticHostModule();
    ASSERT_NE(host, nullptr);
    const char *dir = nullptr;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, fn(host, &dir));
}

// ========================================================================
// Phase 1b: GetModName, GetModDescription, GetModAuthorCount/At
// ========================================================================

TEST_F(CoreApisTests, GetModName_ReturnsManifestName) {
    BML::Core::RegisterCoreApis();

    auto fn = reinterpret_cast<BML_Result (*)(BML_Mod, const char **)>(
        kernel_->api_registry->Get("bmlGetModName"));
    ASSERT_NE(fn, nullptr);

    auto mod = CreateTrackedMod("test.name");
    const char *name = nullptr;
    EXPECT_EQ(BML_RESULT_OK, fn(mod, &name));
    ASSERT_NE(name, nullptr);
    // CreateTrackedMod sets package.name = id
    EXPECT_STREQ("test.name", name);
}

TEST_F(CoreApisTests, GetModDescription_ReturnsManifestDescription) {
    BML::Core::RegisterCoreApis();

    auto fn = reinterpret_cast<BML_Result (*)(BML_Mod, const char **)>(
        kernel_->api_registry->Get("bmlGetModDescription"));
    ASSERT_NE(fn, nullptr);

    // Create a mod with explicit description
    auto manifest = std::make_unique<BML::Core::ModManifest>();
    manifest->package.id = "test.desc";
    manifest->package.name = "TestDesc";
    manifest->package.version = "1.0.0";
    manifest->package.parsed_version = {1, 0, 0};
    manifest->package.description = "A test module";
    manifest->directory = (temp_root_ / "test.desc").wstring();
    std::filesystem::create_directories(manifest->directory);

    auto handle = kernel_->context->CreateModHandle(*manifest);
    BML::Core::LoadedModule module{};
    module.id = "test.desc";
    module.manifest = manifest.get();
    module.mod_handle = std::move(handle);
    kernel_->context->AddLoadedModule(std::move(module));

    BML_Mod mod = kernel_->context->GetModHandleById("test.desc");
    manifests_.push_back(std::move(manifest));

    const char *desc = nullptr;
    EXPECT_EQ(BML_RESULT_OK, fn(mod, &desc));
    ASSERT_NE(desc, nullptr);
    EXPECT_STREQ("A test module", desc);
}

TEST_F(CoreApisTests, GetModAuthorCount_ReturnsCorrectCount) {
    BML::Core::RegisterCoreApis();

    auto count_fn = reinterpret_cast<BML_Result (*)(BML_Mod, uint32_t *)>(
        kernel_->api_registry->Get("bmlGetModAuthorCount"));
    auto at_fn = reinterpret_cast<BML_Result (*)(BML_Mod, uint32_t, const char **)>(
        kernel_->api_registry->Get("bmlGetModAuthorAt"));
    ASSERT_NE(count_fn, nullptr);
    ASSERT_NE(at_fn, nullptr);

    // Create a mod with authors
    auto manifest = std::make_unique<BML::Core::ModManifest>();
    manifest->package.id = "test.authors";
    manifest->package.name = "TestAuthors";
    manifest->package.version = "1.0.0";
    manifest->package.parsed_version = {1, 0, 0};
    manifest->package.authors = {"Alice", "Bob"};
    manifest->directory = (temp_root_ / "test.authors").wstring();
    std::filesystem::create_directories(manifest->directory);

    auto handle = kernel_->context->CreateModHandle(*manifest);
    BML::Core::LoadedModule module{};
    module.id = "test.authors";
    module.manifest = manifest.get();
    module.mod_handle = std::move(handle);
    kernel_->context->AddLoadedModule(std::move(module));

    BML_Mod mod = kernel_->context->GetModHandleById("test.authors");
    manifests_.push_back(std::move(manifest));

    uint32_t count = 0;
    EXPECT_EQ(BML_RESULT_OK, count_fn(mod, &count));
    EXPECT_EQ(2u, count);

    const char *author = nullptr;
    EXPECT_EQ(BML_RESULT_OK, at_fn(mod, 0, &author));
    EXPECT_STREQ("Alice", author);

    EXPECT_EQ(BML_RESULT_OK, at_fn(mod, 1, &author));
    EXPECT_STREQ("Bob", author);

    // Out of range
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, at_fn(mod, 2, &author));
}

TEST_F(CoreApisTests, GetModName_NullOutput) {
    BML::Core::RegisterCoreApis();

    auto fn = reinterpret_cast<BML_Result (*)(BML_Mod, const char **)>(
        kernel_->api_registry->Get("bmlGetModName"));
    ASSERT_NE(fn, nullptr);

    auto mod = CreateTrackedMod("test.name.null");
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, fn(mod, nullptr));
}

// ========================================================================
// Phase 1c: Config Typed Shortcuts
// ========================================================================

TEST_F(CoreApisTests, ConfigGetInt_KeyFound) {
    InitConfigBackedMod("coreapis.config.int");
    BML::Core::RegisterConfigApis();

    auto set_fn = reinterpret_cast<BML_Result (*)(BML_Mod, const BML_ConfigKey *, const BML_ConfigValue *)>(
        kernel_->api_registry->Get("bmlConfigSet"));
    auto get_int_fn = reinterpret_cast<BML_Result (*)(BML_Mod, const char *, const char *, int32_t, int32_t *)>(
        kernel_->api_registry->Get("bmlConfigGetInt"));
    ASSERT_NE(set_fn, nullptr);
    ASSERT_NE(get_int_fn, nullptr);

    BML_ConfigKey key = BML_CONFIG_KEY_INIT("test", "myint");
    BML_ConfigValue value = BML_CONFIG_VALUE_INIT_INT(42);
    ASSERT_EQ(BML_RESULT_OK, set_fn(Mod(), &key, &value));

    int32_t result = 0;
    EXPECT_EQ(BML_RESULT_OK, get_int_fn(Mod(), "test", "myint", -1, &result));
    EXPECT_EQ(42, result);
}

TEST_F(CoreApisTests, ConfigGetInt_KeyNotFoundReturnsDefault) {
    InitConfigBackedMod("coreapis.config.int.default");
    BML::Core::RegisterConfigApis();

    auto get_int_fn = reinterpret_cast<BML_Result (*)(BML_Mod, const char *, const char *, int32_t, int32_t *)>(
        kernel_->api_registry->Get("bmlConfigGetInt"));
    ASSERT_NE(get_int_fn, nullptr);

    int32_t result = 0;
    EXPECT_EQ(BML_RESULT_OK, get_int_fn(Mod(), "test", "nonexistent", 99, &result));
    EXPECT_EQ(99, result);
}

TEST_F(CoreApisTests, ConfigGetFloat_KeyFound) {
    InitConfigBackedMod("coreapis.config.float");
    BML::Core::RegisterConfigApis();

    auto set_fn = reinterpret_cast<BML_Result (*)(BML_Mod, const BML_ConfigKey *, const BML_ConfigValue *)>(
        kernel_->api_registry->Get("bmlConfigSet"));
    auto get_float_fn = reinterpret_cast<BML_Result (*)(BML_Mod, const char *, const char *, float, float *)>(
        kernel_->api_registry->Get("bmlConfigGetFloat"));
    ASSERT_NE(set_fn, nullptr);
    ASSERT_NE(get_float_fn, nullptr);

    BML_ConfigKey key = BML_CONFIG_KEY_INIT("test", "myfloat");
    BML_ConfigValue value = BML_CONFIG_VALUE_INIT_FLOAT(3.14f);
    ASSERT_EQ(BML_RESULT_OK, set_fn(Mod(), &key, &value));

    float result = 0.0f;
    EXPECT_EQ(BML_RESULT_OK, get_float_fn(Mod(), "test", "myfloat", 0.0f, &result));
    EXPECT_FLOAT_EQ(3.14f, result);
}

TEST_F(CoreApisTests, ConfigGetBool_KeyNotFoundReturnsDefault) {
    InitConfigBackedMod("coreapis.config.bool.default");
    BML::Core::RegisterConfigApis();

    auto get_bool_fn = reinterpret_cast<BML_Result (*)(BML_Mod, const char *, const char *, BML_Bool, BML_Bool *)>(
        kernel_->api_registry->Get("bmlConfigGetBool"));
    ASSERT_NE(get_bool_fn, nullptr);

    BML_Bool result = BML_FALSE;
    EXPECT_EQ(BML_RESULT_OK, get_bool_fn(Mod(), "test", "nonexistent", BML_TRUE, &result));
    EXPECT_EQ(BML_TRUE, result);
}

TEST_F(CoreApisTests, ConfigGetString_KeyFound) {
    InitConfigBackedMod("coreapis.config.string");
    BML::Core::RegisterConfigApis();

    auto set_fn = reinterpret_cast<BML_Result (*)(BML_Mod, const BML_ConfigKey *, const BML_ConfigValue *)>(
        kernel_->api_registry->Get("bmlConfigSet"));
    auto get_string_fn = reinterpret_cast<BML_Result (*)(BML_Mod, const char *, const char *, const char *, const char **)>(
        kernel_->api_registry->Get("bmlConfigGetString"));
    ASSERT_NE(set_fn, nullptr);
    ASSERT_NE(get_string_fn, nullptr);

    BML_ConfigKey key = BML_CONFIG_KEY_INIT("test", "mystr");
    BML_ConfigValue value = BML_CONFIG_VALUE_INIT_STRING("hello");
    ASSERT_EQ(BML_RESULT_OK, set_fn(Mod(), &key, &value));

    const char *result = nullptr;
    EXPECT_EQ(BML_RESULT_OK, get_string_fn(Mod(), "test", "mystr", "default", &result));
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ("hello", result);
}

TEST_F(CoreApisTests, ConfigGetString_KeyNotFoundReturnsDefault) {
    InitConfigBackedMod("coreapis.config.string.default");
    BML::Core::RegisterConfigApis();

    auto get_string_fn = reinterpret_cast<BML_Result (*)(BML_Mod, const char *, const char *, const char *, const char **)>(
        kernel_->api_registry->Get("bmlConfigGetString"));
    ASSERT_NE(get_string_fn, nullptr);

    const char *result = nullptr;
    EXPECT_EQ(BML_RESULT_OK, get_string_fn(Mod(), "test", "nonexistent", "fallback", &result));
    EXPECT_STREQ("fallback", result);
}

TEST_F(CoreApisTests, ConfigGetString_NullDefaultReturnsNull) {
    InitConfigBackedMod("coreapis.config.string.nulldefault");
    BML::Core::RegisterConfigApis();

    auto get_string_fn = reinterpret_cast<BML_Result (*)(BML_Mod, const char *, const char *, const char *, const char **)>(
        kernel_->api_registry->Get("bmlConfigGetString"));
    ASSERT_NE(get_string_fn, nullptr);

    const char *result = reinterpret_cast<const char *>(0xDEAD); // sentinel
    EXPECT_EQ(BML_RESULT_OK, get_string_fn(Mod(), "test", "nonexistent", nullptr, &result));
    EXPECT_EQ(nullptr, result);
}

TEST_F(CoreApisTests, ConfigGetInt_NullOutput) {
    InitConfigBackedMod("coreapis.config.int.null");
    BML::Core::RegisterConfigApis();

    auto get_int_fn = reinterpret_cast<BML_Result (*)(BML_Mod, const char *, const char *, int32_t, int32_t *)>(
        kernel_->api_registry->Get("bmlConfigGetInt"));
    ASSERT_NE(get_int_fn, nullptr);

    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, get_int_fn(Mod(), "test", "key", 0, nullptr));
}

TEST_F(CoreApisTests, ConfigGetInt_TypeMismatch) {
    InitConfigBackedMod("coreapis.config.int.mismatch");
    BML::Core::RegisterConfigApis();

    auto set_fn = reinterpret_cast<BML_Result (*)(BML_Mod, const BML_ConfigKey *, const BML_ConfigValue *)>(
        kernel_->api_registry->Get("bmlConfigSet"));
    auto get_int_fn = reinterpret_cast<BML_Result (*)(BML_Mod, const char *, const char *, int32_t, int32_t *)>(
        kernel_->api_registry->Get("bmlConfigGetInt"));
    ASSERT_NE(set_fn, nullptr);
    ASSERT_NE(get_int_fn, nullptr);

    // Set a string value
    BML_ConfigKey key = BML_CONFIG_KEY_INIT("test", "strkey");
    BML_ConfigValue value = BML_CONFIG_VALUE_INIT_STRING("notanint");
    ASSERT_EQ(BML_RESULT_OK, set_fn(Mod(), &key, &value));

    // Try to read it as int
    int32_t result = 0;
    EXPECT_EQ(BML_RESULT_CONFIG_TYPE_MISMATCH, get_int_fn(Mod(), "test", "strkey", 0, &result));
}

// ========================================================================
// Phase 1d: GetLoadedModuleCount optimization
// ========================================================================

TEST_F(CoreApisTests, GetLoadedModuleCount_MatchesModCount) {
    BML::Core::RegisterCoreApis();

    auto count_fn = reinterpret_cast<uint32_t (*)()>(
        kernel_->api_registry->Get("bmlGetLoadedModuleCount"));
    auto at_fn = reinterpret_cast<BML_Mod (*)(uint32_t)>(
        kernel_->api_registry->Get("bmlGetLoadedModuleAt"));
    ASSERT_NE(count_fn, nullptr);
    ASSERT_NE(at_fn, nullptr);

    EXPECT_EQ(0u, count_fn());

    auto mod1 = CreateTrackedMod("test.count1");
    auto mod2 = CreateTrackedMod("test.count2");
    EXPECT_EQ(2u, count_fn());

    EXPECT_EQ(mod1, at_fn(0));
    EXPECT_EQ(mod2, at_fn(1));
    EXPECT_EQ(nullptr, at_fn(2));
}

} // namespace

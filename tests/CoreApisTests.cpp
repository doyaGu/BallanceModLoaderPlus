#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
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
#include "Core/ImcBus.h"
#include "Core/ModHandle.h"
#include "Core/ModManifest.h"
#include "Core/ResourceApi.h"

#include "bml_config.h"
#include "bml_errors.h"
#include "bml_imc.h"
#include "bml_logging.h"
#include "bml_resource.h"

using BML::Core::ApiRegistry;
using BML::Core::ConfigStore;
using BML::Core::Context;
using BML::Core::ImcBus;

namespace BML::Core {
void RegisterExtensionApis() {}
void RegisterMemoryApis() {}
void RegisterDiagnosticApis() {}
void RegisterSyncApis() {}
void RegisterCapabilityApis() {}
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

ConfigHookCapture &GetConfigHookCapture() {
    static ConfigHookCapture *capture = [] {
        auto *state = new ConfigHookCapture();
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
        hooks.user_data = state;
        BML_Result result = BML::Core::RegisterConfigLoadHooks(&hooks);
        if (result != BML_RESULT_OK) {
            throw std::runtime_error("Failed to register config hooks for tests");
        }
        return state;
    }();
    return *capture;
}

class CoreApisTests : public ::testing::Test {
protected:
    void SetUp() override {
        ApiRegistry::Instance().Clear();
        Context::SetCurrentModule(nullptr);
        ImcBus::Instance().Shutdown();
        temp_root_ = std::filesystem::temp_directory_path() /
                     ("bml-coreapis-tests-" + std::to_string(test_counter_.fetch_add(1, std::memory_order_relaxed)));
        std::filesystem::create_directories(temp_root_);
    }

    void TearDown() override {
        ImcBus::Instance().Shutdown();
        if (mod_handle_) {
            ConfigStore::Instance().FlushAndRelease(mod_handle_.get());
        }
        Context::SetCurrentModule(nullptr);
        std::error_code ec;
        std::filesystem::remove_all(temp_root_, ec);
    }

    void InitConfigBackedMod(const std::string &id) {
        manifest_ = std::make_unique<BML::Core::ModManifest>();
        manifest_->package.id = id;
        manifest_->package.name = id;
        manifest_->package.version = "1.0.0";
        manifest_->package.parsed_version = {1, 0, 0};
        manifest_->directory = (temp_root_ / id).wstring();
        std::filesystem::create_directories(manifest_->directory);
        manifest_->manifest_path = manifest_->directory + L"/manifest.toml";
        mod_handle_ = Context::Instance().CreateModHandle(*manifest_);
        Context::SetCurrentModule(mod_handle_.get());
    }

    BML_Mod Mod() const { return mod_handle_ ? mod_handle_.get() : nullptr; }

    static std::atomic<uint64_t> test_counter_;
    std::filesystem::path temp_root_;
    std::unique_ptr<BML::Core::ModManifest> manifest_;
    std::unique_ptr<BML_Mod_T> mod_handle_;
};

std::atomic<uint64_t> CoreApisTests::test_counter_{1};

TEST_F(CoreApisTests, MultiThreadedHandleCreationAndRelease) {
    BML::Core::RegisterResourceApis();

    using PFN_Create = BML_Result (*)(BML_HandleType, BML_HandleDesc *);
    using PFN_Release = BML_Result (*)(const BML_HandleDesc *);
    using PFN_Validate = BML_Result (*)(const BML_HandleDesc *, BML_Bool *);

    auto create_fn = reinterpret_cast<PFN_Create>(ApiRegistry::Instance().Get("bmlHandleCreate"));
    auto release_fn = reinterpret_cast<PFN_Release>(ApiRegistry::Instance().Get("bmlHandleRelease"));
    auto validate_fn = reinterpret_cast<PFN_Validate>(ApiRegistry::Instance().Get("bmlHandleValidate"));

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

    BML_HandleType handle_type = 0;
    ASSERT_EQ(BML_RESULT_OK, BML::Core::RegisterResourceType(&type_desc, &handle_type));

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
                ASSERT_EQ(BML_RESULT_OK, create_fn(handle_type, &desc));
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

    auto config_set = reinterpret_cast<PFN_ConfigSet>(ApiRegistry::Instance().Get("bmlConfigSet"));
    auto config_get = reinterpret_cast<PFN_ConfigGet>(ApiRegistry::Instance().Get("bmlConfigGet"));

    ASSERT_NE(config_set, nullptr);
    ASSERT_NE(config_get, nullptr);

    auto &hook_capture = GetConfigHookCapture();
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

        ConfigStore::Instance().FlushAndRelease(Mod());

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
    OrderingCapture first{};
    OrderingCapture second{};

    BML_Subscription sub1 = nullptr;
    BML_Subscription sub2 = nullptr;
    
    BML_TopicId topic_id = 0;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("order.topic", &topic_id));
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Subscribe(topic_id, OrderingHandler, &first, &sub1));
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Subscribe(topic_id, OrderingHandler, &second, &sub2));

    constexpr size_t kMessages = 256;
    for (size_t i = 0; i < kMessages; ++i) {
        uint32_t payload = static_cast<uint32_t>(i);
        ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Publish(topic_id, &payload, sizeof(payload)));
    }

    ImcBus::Instance().Pump(0);

    auto validate = [](OrderingCapture &capture) {
        std::lock_guard<std::mutex> lock(capture.mutex);
        ASSERT_EQ(capture.values.size(), kMessages);
        for (size_t i = 0; i < kMessages; ++i) {
            EXPECT_EQ(capture.values[i], i);
        }
    };

    validate(first);
    validate(second);

    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub1));
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub2));
}

TEST_F(CoreApisTests, SetCurrentModuleApiIsThreadLocal) {
    BML::Core::RegisterCoreApis();

    auto set_fn = reinterpret_cast<BML_Result (*)(BML_Mod)>(ApiRegistry::Instance().Get("bmlSetCurrentModule"));
    auto get_fn = reinterpret_cast<BML_Mod (*)()>(ApiRegistry::Instance().Get("bmlGetCurrentModule"));
    ASSERT_NE(set_fn, nullptr);
    ASSERT_NE(get_fn, nullptr);

    auto primary = std::make_unique<BML_Mod_T>();
    primary->id = "coreapis.primary";
    ASSERT_EQ(BML_RESULT_OK, set_fn(primary.get()));
    EXPECT_EQ(get_fn(), primary.get());

    auto worker = std::make_unique<BML_Mod_T>();
    worker->id = "coreapis.worker";
    std::atomic<BML_Mod> worker_seen{nullptr};

    std::thread background([&] {
        EXPECT_EQ(get_fn(), nullptr);
        ASSERT_EQ(BML_RESULT_OK, set_fn(worker.get()));
        worker_seen.store(get_fn(), std::memory_order_release);
        ASSERT_EQ(BML_RESULT_OK, set_fn(nullptr));
    });
    background.join();

    EXPECT_EQ(worker_seen.load(std::memory_order_acquire), worker.get());
    EXPECT_EQ(get_fn(), primary.get());

    ASSERT_EQ(BML_RESULT_OK, set_fn(nullptr));
    EXPECT_EQ(get_fn(), nullptr);
}

} // namespace

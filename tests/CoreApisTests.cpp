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
#include "TestKernel.h"

#include "bml_config.h"
#include "bml_errors.h"
#include "bml_imc.h"
#include "bml_logging.h"
#include "bml_resource.h"
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
    TestKernel kernel_;

    void SetUp() override {
        kernel_->api_registry = std::make_unique<ApiRegistry>();
        kernel_->config = std::make_unique<ConfigStore>();
        kernel_->context = std::make_unique<Context>(*kernel_->api_registry, *kernel_->config);

        Context::SetCurrentModule(nullptr);
        kernel_->context->Initialize(bmlGetApiVersion());
        ImcShutdown();
        Context::SetCurrentModule(nullptr);
        temp_root_ = std::filesystem::temp_directory_path() /
                     ("bml-coreapis-tests-" + std::to_string(test_counter_.fetch_add(1, std::memory_order_relaxed)));
        std::filesystem::create_directories(temp_root_);
    }

    void TearDown() override {
        ImcShutdown();
        Context::SetCurrentModule(nullptr);
        manifests_.clear();
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
    std::vector<std::unique_ptr<BML::Core::ModManifest>> manifests_;
    BML_Mod mod_handle_{nullptr};
};

std::atomic<uint64_t> CoreApisTests::test_counter_{1};

TEST_F(CoreApisTests, MultiThreadedHandleCreationAndRelease) {
    BML::Core::RegisterResourceApis();

    using PFN_Create = BML_Result (*)(BML_HandleType, BML_HandleDesc *);
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
                desc.struct_size = sizeof(BML_HandleDesc);
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

    auto config_set = reinterpret_cast<PFN_ConfigSet>(kernel_->api_registry->Get("bmlConfigSet"));
    auto config_get = reinterpret_cast<PFN_ConfigGet>(kernel_->api_registry->Get("bmlConfigGet"));

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
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(topic_id, OrderingHandler, &first, &sub1));
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(topic_id, OrderingHandler, &second, &sub2));

    constexpr size_t kMessages = 256;
    for (size_t i = 0; i < kMessages; ++i) {
        uint32_t payload = static_cast<uint32_t>(i);
        ASSERT_EQ(BML_RESULT_OK, ImcPublish(topic_id, &payload, sizeof(payload)));
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

TEST_F(CoreApisTests, SetCurrentModuleApiIsThreadLocal) {
    BML::Core::RegisterCoreApis();

    auto set_fn = reinterpret_cast<BML_Result (*)(BML_Mod)>(kernel_->api_registry->Get("bmlSetCurrentModule"));
    auto get_fn = reinterpret_cast<BML_Mod (*)()>(kernel_->api_registry->Get("bmlGetCurrentModule"));
    auto get_id_fn = reinterpret_cast<BML_Result (*)(BML_Mod, const char **)>(
        kernel_->api_registry->Get("bmlGetModId"));
    ASSERT_NE(set_fn, nullptr);
    ASSERT_NE(get_fn, nullptr);
    ASSERT_NE(get_id_fn, nullptr);

    auto primary = CreateTrackedMod("coreapis.primary");
    auto worker = CreateTrackedMod("coreapis.worker");

    ASSERT_NE(primary, nullptr);
    ASSERT_NE(worker, nullptr);

    // Untracked bindings remain thread-local state, but implicit Core resolution
    // must not trust them as valid module handles.
    auto *untracked = new BML_Mod_T();
    untracked->id = "coreapis.untracked";
    EXPECT_EQ(BML_RESULT_OK, set_fn(untracked));
    EXPECT_EQ(get_fn(), untracked);
    const char *id = nullptr;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, get_id_fn(nullptr, &id));
    set_fn(nullptr); // clear
    delete untracked;

    ASSERT_EQ(BML_RESULT_OK, set_fn(primary));
    EXPECT_EQ(get_fn(), primary);

    std::atomic<BML_Mod> worker_seen{nullptr};

    std::thread background([&] {
        EXPECT_EQ(get_fn(), nullptr);
        ASSERT_EQ(BML_RESULT_OK, set_fn(worker));
        worker_seen.store(get_fn(), std::memory_order_release);
        ASSERT_EQ(BML_RESULT_OK, set_fn(nullptr));
    });
    background.join();

    EXPECT_EQ(worker_seen.load(std::memory_order_acquire), worker);
    EXPECT_EQ(get_fn(), primary);

    ASSERT_EQ(BML_RESULT_OK, set_fn(nullptr));
    EXPECT_EQ(get_fn(), nullptr);
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

    // No current module set, so nullptr resolves to nothing
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

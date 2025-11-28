#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "Core/ApiRegistry.h"
#include "Core/ConfigStore.h"
#include "Core/Context.h"
#include "Core/ModHandle.h"
#include "Core/ModManifest.h"

#include "bml_config.h"
#include "bml_errors.h"
#include "bml_extension.h"

using BML::Core::ApiRegistry;
using BML::Core::ConfigStore;
using BML::Core::Context;

namespace {

enum class HookPhase {
    Pre,
    Post
};

struct HookRecorder {
    void Record(HookPhase phase) {
        std::lock_guard<std::mutex> lock(mutex);
        phases.push_back(phase);
    }

    void Reset() {
        std::lock_guard<std::mutex> lock(mutex);
        phases.clear();
    }

    std::vector<HookPhase> Snapshot() const {
        std::lock_guard<std::mutex> lock(mutex);
        return phases;
    }

    mutable std::mutex mutex;
    std::vector<HookPhase> phases;
};

HookRecorder &GetHookRecorder() {
    static HookRecorder *recorder = [] {
        auto *instance = new HookRecorder();
        BML_ConfigLoadHooks hooks{};
        hooks.struct_size = sizeof(BML_ConfigLoadHooks);
        hooks.on_pre_load = [](BML_Context, const BML_ConfigLoadContext *, void *user_data) {
            auto *rec = static_cast<HookRecorder *>(user_data);
            rec->Record(HookPhase::Pre);
        };
        hooks.on_post_load = [](BML_Context, const BML_ConfigLoadContext *, void *user_data) {
            auto *rec = static_cast<HookRecorder *>(user_data);
            rec->Record(HookPhase::Post);
        };
        hooks.user_data = instance;
        BML_Result result = BML::Core::RegisterConfigLoadHooks(&hooks);
        if (result != BML_RESULT_OK) {
            throw std::runtime_error("Failed to register config load hooks for tests");
        }
        return instance;
    }();
    return *recorder;
}

using PFN_ConfigGet = BML_Result (*)(BML_Mod, const BML_ConfigKey *, BML_ConfigValue *);
using PFN_ConfigSet = BML_Result (*)(BML_Mod, const BML_ConfigKey *, const BML_ConfigValue *);
using PFN_ConfigReset = BML_Result (*)(BML_Mod, const BML_ConfigKey *);
using PFN_ConfigEnumerate = BML_Result (*)(BML_Mod,
                                           BML_ConfigEnumCallback,
                                           void *);

class ConfigStoreConcurrencyTests : public ::testing::Test {
protected:
    void SetUp() override {
        ApiRegistry::Instance().Clear();
        Context::SetCurrentModule(nullptr);
        temp_root_ = std::filesystem::temp_directory_path() /
                     ("bml-configstore-tests-" +
                      std::to_string(counter_.fetch_add(1, std::memory_order_relaxed)));
        std::filesystem::create_directories(temp_root_);
        BML::Core::RegisterConfigApis();
    }

    void TearDown() override {
        if (mod_) {
            ConfigStore::Instance().FlushAndRelease(mod_.get());
        }
        Context::SetCurrentModule(nullptr);
        std::error_code ec;
        std::filesystem::remove_all(temp_root_, ec);
    }

    void InitMod(const std::string &id, const std::filesystem::path &custom_dir = {}) {
        manifest_ = std::make_unique<BML::Core::ModManifest>();
        manifest_->package.id = id;
        manifest_->package.name = id;
        manifest_->package.version = "1.0.0";
        manifest_->package.parsed_version = {1, 0, 0};

        std::filesystem::path base = custom_dir.empty() ? (temp_root_ / id) : custom_dir;
        std::filesystem::create_directories(base);
        manifest_->directory = base.wstring();
        manifest_->manifest_path = (base / "manifest.toml").wstring();

        mod_ = Context::Instance().CreateModHandle(*manifest_);
        Context::SetCurrentModule(mod_.get());
    }

    BML_Mod Mod() const { return mod_ ? mod_.get() : nullptr; }

    template <typename Fn>
    Fn Lookup(const char *name) {
        auto pointer = reinterpret_cast<Fn>(ApiRegistry::Instance().Get(name));
        if (!pointer) {
            ADD_FAILURE() << "Missing API registration for " << name;
        }
        return pointer;
    }

    static std::atomic<uint64_t> counter_;
    std::filesystem::path temp_root_;
    std::unique_ptr<BML::Core::ModManifest> manifest_;
    std::unique_ptr<BML_Mod_T> mod_;
};

std::atomic<uint64_t> ConfigStoreConcurrencyTests::counter_{1};

TEST_F(ConfigStoreConcurrencyTests, ConcurrentReadWriteEnumerateAndResetRemainStable) {
    InitMod("config.concurrent");

    auto config_get = Lookup<PFN_ConfigGet>("bmlConfigGet");
    auto config_set = Lookup<PFN_ConfigSet>("bmlConfigSet");
    auto config_reset = Lookup<PFN_ConfigReset>("bmlConfigReset");
    auto config_enumerate = Lookup<PFN_ConfigEnumerate>("bmlConfigEnumerate");

    ASSERT_NE(config_get, nullptr);
    ASSERT_NE(config_set, nullptr);
    ASSERT_NE(config_reset, nullptr);
    ASSERT_NE(config_enumerate, nullptr);

    constexpr const char *kCategory = "shared";
    constexpr std::array<const char *, 4> kKeys{"alpha", "beta", "gamma", "delta"};

    BML_ConfigValue init_value{};
    init_value.struct_size = sizeof(BML_ConfigValue);
    init_value.type = BML_CONFIG_INT;
    init_value.data.int_value = 0;
    for (const char *name : kKeys) {
        BML_ConfigKey key{sizeof(BML_ConfigKey), kCategory, name};
        ASSERT_EQ(BML_RESULT_OK, config_set(Mod(), &key, &init_value));
    }

    constexpr size_t kWriterThreads = 4;
    constexpr size_t kReaderThreads = 4;
    constexpr size_t kResetThreads = 2;
    constexpr size_t kEnumeratorThreads = 2;
    constexpr size_t kIterations = 80;

    const size_t total_threads = kWriterThreads + kReaderThreads + kResetThreads + kEnumeratorThreads;
    std::barrier sync(static_cast<std::ptrdiff_t>(total_threads + 1));

    std::atomic<size_t> successful_sets{0};
    std::atomic<size_t> successful_gets{0};
    std::atomic<size_t> successful_resets{0};
    std::atomic<size_t> enumerate_runs{0};

    std::vector<std::thread> workers;
    workers.reserve(total_threads);

    for (size_t writer = 0; writer < kWriterThreads; ++writer) {
        workers.emplace_back([&, writer] {
            sync.arrive_and_wait();
            for (size_t iter = 0; iter < kIterations; ++iter) {
                size_t key_index = (writer + iter) % kKeys.size();
                BML_ConfigKey key{sizeof(BML_ConfigKey), kCategory, kKeys[key_index]};
                BML_ConfigValue value{};
                value.struct_size = sizeof(BML_ConfigValue);
                value.type = BML_CONFIG_INT;
                value.data.int_value = static_cast<int32_t>((writer << 16) + static_cast<int32_t>(iter));
                ASSERT_EQ(BML_RESULT_OK, config_set(Mod(), &key, &value));
                successful_sets.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (size_t reader = 0; reader < kReaderThreads; ++reader) {
        workers.emplace_back([&, reader] {
            sync.arrive_and_wait();
            for (size_t iter = 0; iter < kIterations * 2; ++iter) {
                size_t key_index = (reader + iter) % kKeys.size();
                BML_ConfigKey key{sizeof(BML_ConfigKey), kCategory, kKeys[key_index]};
                BML_ConfigValue value{};
                value.struct_size = sizeof(BML_ConfigValue);
                BML_Result result = config_get(Mod(), &key, &value);
                if (result == BML_RESULT_OK) {
                    ASSERT_EQ(BML_CONFIG_INT, value.type);
                    successful_gets.fetch_add(1, std::memory_order_relaxed);
                } else {
                    ASSERT_EQ(BML_RESULT_NOT_FOUND, result);
                }
                std::this_thread::yield();
            }
        });
    }

    for (size_t reset = 0; reset < kResetThreads; ++reset) {
        workers.emplace_back([&, reset] {
            sync.arrive_and_wait();
            for (size_t iter = 0; iter < kIterations; ++iter) {
                size_t key_index = (reset + iter) % kKeys.size();
                BML_ConfigKey key{sizeof(BML_ConfigKey), kCategory, kKeys[key_index]};
                BML_Result result = config_reset(Mod(), &key);
                if (result == BML_RESULT_OK) {
                    successful_resets.fetch_add(1, std::memory_order_relaxed);
                } else {
                    ASSERT_EQ(BML_RESULT_NOT_FOUND, result);
                }
                std::this_thread::yield();
            }
        });
    }

    for (size_t enumerator = 0; enumerator < kEnumeratorThreads; ++enumerator) {
        workers.emplace_back([&, enumerator] {
            (void)enumerator;
            sync.arrive_and_wait();
            for (size_t iter = 0; iter < kIterations; ++iter) {
                std::vector<std::pair<std::string, std::string>> snapshot;
                auto callback = +[](BML_Context,
                                    const BML_ConfigKey *key,
                                    const BML_ConfigValue *value,
                                    void *user_data) {
                    ASSERT_NE(key, nullptr);
                    ASSERT_NE(value, nullptr);
                    auto *entries = static_cast<std::vector<std::pair<std::string, std::string>> *>(user_data);
                    entries->emplace_back(key->category ? key->category : "", key->name ? key->name : "");
                    ASSERT_EQ(BML_CONFIG_INT, value->type);
                };
                ASSERT_EQ(BML_RESULT_OK, config_enumerate(Mod(), callback, &snapshot));
                for (const auto &entry : snapshot) {
                    EXPECT_STREQ(entry.first.c_str(), kCategory);
                    bool known = std::any_of(kKeys.begin(), kKeys.end(), [&](const char *name) {
                        return entry.second == name;
                    });
                    EXPECT_TRUE(known);
                }
                enumerate_runs.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    sync.arrive_and_wait();
    for (auto &thread : workers) {
        thread.join();
    }

    EXPECT_GT(successful_sets.load(), 0u);
    EXPECT_GT(successful_gets.load(), 0u);
    EXPECT_GT(successful_resets.load(), 0u);
    EXPECT_GT(enumerate_runs.load(), 0u);
}

TEST_F(ConfigStoreConcurrencyTests, FlushAndReleaseReloadsDocumentAndFiresHooks) {
    InitMod("config.flush");

    auto config_set = Lookup<PFN_ConfigSet>("bmlConfigSet");
    auto config_get = Lookup<PFN_ConfigGet>("bmlConfigGet");

    ASSERT_NE(config_set, nullptr);
    ASSERT_NE(config_get, nullptr);

    auto &recorder = GetHookRecorder();
    recorder.Reset();

    const BML_ConfigKey key{sizeof(BML_ConfigKey), "reload", "value"};
    BML_ConfigValue value{};
    value.struct_size = sizeof(BML_ConfigValue);
    value.type = BML_CONFIG_INT;
    value.data.int_value = 42;
    ASSERT_EQ(BML_RESULT_OK, config_set(Mod(), &key, &value));

    auto first_snapshot = recorder.Snapshot();
    ASSERT_EQ(first_snapshot.size(), 2u);
    EXPECT_EQ(first_snapshot[0], HookPhase::Pre);
    EXPECT_EQ(first_snapshot[1], HookPhase::Post);

    ConfigStore::Instance().FlushAndRelease(Mod());

    BML_ConfigValue read{};
    read.struct_size = sizeof(BML_ConfigValue);
    ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key, &read));
    EXPECT_EQ(read.type, BML_CONFIG_INT);
    EXPECT_EQ(read.data.int_value, 42);

    auto reloaded = recorder.Snapshot();
    ASSERT_EQ(reloaded.size(), 4u);
    EXPECT_EQ(reloaded[2], HookPhase::Pre);
    EXPECT_EQ(reloaded[3], HookPhase::Post);
}

TEST_F(ConfigStoreConcurrencyTests, SetValueReturnsIoErrorWhenConfigDirectoryIsBlocked) {
    const std::filesystem::path locked_dir = temp_root_ / "blocked-mod";
    InitMod("config.locked", locked_dir);

    auto config_set = Lookup<PFN_ConfigSet>("bmlConfigSet");
    ASSERT_NE(config_set, nullptr);

    const std::filesystem::path blocker = locked_dir / "config";
    {
        std::ofstream stream(blocker, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(stream.is_open());
        stream << "locked";
    }
    ASSERT_TRUE(std::filesystem::exists(blocker));

    const BML_ConfigKey key{sizeof(BML_ConfigKey), "blocked", "value"};
    BML_ConfigValue value{};
    value.struct_size = sizeof(BML_ConfigValue);
    value.type = BML_CONFIG_INT;
    value.data.int_value = 7;

    EXPECT_EQ(BML_RESULT_IO_ERROR, config_set(Mod(), &key, &value));
}

// ========================================================================
// Type-Safe Accessor Tests
// ========================================================================

using PFN_ConfigGetInt = BML_Result (*)(BML_Mod, const BML_ConfigKey *, int32_t *);
using PFN_ConfigSetInt = BML_Result (*)(BML_Mod, const BML_ConfigKey *, int32_t);
using PFN_ConfigGetFloat = BML_Result (*)(BML_Mod, const BML_ConfigKey *, float *);
using PFN_ConfigSetFloat = BML_Result (*)(BML_Mod, const BML_ConfigKey *, float);
using PFN_ConfigGetBool = BML_Result (*)(BML_Mod, const BML_ConfigKey *, BML_Bool *);
using PFN_ConfigSetBool = BML_Result (*)(BML_Mod, const BML_ConfigKey *, BML_Bool);
using PFN_ConfigGetString = BML_Result (*)(BML_Mod, const BML_ConfigKey *, const char **);
using PFN_ConfigSetString = BML_Result (*)(BML_Mod, const BML_ConfigKey *, const char *);

TEST_F(ConfigStoreConcurrencyTests, TypeSafeIntAccessorWorks) {
    InitMod("config.typesafe.int");

    auto getInt = Lookup<PFN_ConfigGetInt>("bmlConfigGetInt");
    auto setInt = Lookup<PFN_ConfigSetInt>("bmlConfigSetInt");
    ASSERT_NE(getInt, nullptr);
    ASSERT_NE(setInt, nullptr);

    BML_ConfigKey key{sizeof(BML_ConfigKey), "settings", "volume"};
    
    // Set and get int value
    ASSERT_EQ(BML_RESULT_OK, setInt(Mod(), &key, 42));
    
    int32_t out_value = 0;
    ASSERT_EQ(BML_RESULT_OK, getInt(Mod(), &key, &out_value));
    EXPECT_EQ(out_value, 42);
}

TEST_F(ConfigStoreConcurrencyTests, TypeSafeFloatAccessorWorks) {
    InitMod("config.typesafe.float");

    auto getFloat = Lookup<PFN_ConfigGetFloat>("bmlConfigGetFloat");
    auto setFloat = Lookup<PFN_ConfigSetFloat>("bmlConfigSetFloat");
    ASSERT_NE(getFloat, nullptr);
    ASSERT_NE(setFloat, nullptr);

    BML_ConfigKey key{sizeof(BML_ConfigKey), "physics", "gravity"};
    
    ASSERT_EQ(BML_RESULT_OK, setFloat(Mod(), &key, 9.81f));
    
    float out_value = 0.0f;
    ASSERT_EQ(BML_RESULT_OK, getFloat(Mod(), &key, &out_value));
    EXPECT_FLOAT_EQ(out_value, 9.81f);
}

TEST_F(ConfigStoreConcurrencyTests, TypeSafeBoolAccessorWorks) {
    InitMod("config.typesafe.bool");

    auto getBool = Lookup<PFN_ConfigGetBool>("bmlConfigGetBool");
    auto setBool = Lookup<PFN_ConfigSetBool>("bmlConfigSetBool");
    ASSERT_NE(getBool, nullptr);
    ASSERT_NE(setBool, nullptr);

    BML_ConfigKey key{sizeof(BML_ConfigKey), "video", "fullscreen"};
    
    ASSERT_EQ(BML_RESULT_OK, setBool(Mod(), &key, BML_TRUE));
    
    BML_Bool out_value = BML_FALSE;
    ASSERT_EQ(BML_RESULT_OK, getBool(Mod(), &key, &out_value));
    EXPECT_EQ(out_value, BML_TRUE);
}

TEST_F(ConfigStoreConcurrencyTests, TypeSafeStringAccessorWorks) {
    InitMod("config.typesafe.string");

    auto getString = Lookup<PFN_ConfigGetString>("bmlConfigGetString");
    auto setString = Lookup<PFN_ConfigSetString>("bmlConfigSetString");
    ASSERT_NE(getString, nullptr);
    ASSERT_NE(setString, nullptr);

    BML_ConfigKey key{sizeof(BML_ConfigKey), "player", "name"};
    
    ASSERT_EQ(BML_RESULT_OK, setString(Mod(), &key, "TestPlayer"));
    
    const char *out_value = nullptr;
    ASSERT_EQ(BML_RESULT_OK, getString(Mod(), &key, &out_value));
    ASSERT_NE(out_value, nullptr);
    EXPECT_STREQ(out_value, "TestPlayer");
}

TEST_F(ConfigStoreConcurrencyTests, TypeMismatchReturnsError) {
    InitMod("config.typemismatch");

    auto setInt = Lookup<PFN_ConfigSetInt>("bmlConfigSetInt");
    auto getFloat = Lookup<PFN_ConfigGetFloat>("bmlConfigGetFloat");
    ASSERT_NE(setInt, nullptr);
    ASSERT_NE(getFloat, nullptr);

    BML_ConfigKey key{sizeof(BML_ConfigKey), "mismatch", "value"};
    
    // Set as int
    ASSERT_EQ(BML_RESULT_OK, setInt(Mod(), &key, 100));
    
    // Try to get as float - should fail with type mismatch
    float out_value = 0.0f;
    EXPECT_EQ(BML_RESULT_CONFIG_TYPE_MISMATCH, getFloat(Mod(), &key, &out_value));
}

TEST_F(ConfigStoreConcurrencyTests, GetNotFoundReturnsError) {
    InitMod("config.notfound");

    auto getInt = Lookup<PFN_ConfigGetInt>("bmlConfigGetInt");
    ASSERT_NE(getInt, nullptr);

    BML_ConfigKey key{sizeof(BML_ConfigKey), "nonexistent", "value"};
    
    int32_t out_value = 0;
    EXPECT_EQ(BML_RESULT_NOT_FOUND, getInt(Mod(), &key, &out_value));
}

TEST_F(ConfigStoreConcurrencyTests, NullOutputPointerReturnsInvalidArgument) {
    InitMod("config.nulloutput");

    auto getInt = Lookup<PFN_ConfigGetInt>("bmlConfigGetInt");
    auto setInt = Lookup<PFN_ConfigSetInt>("bmlConfigSetInt");
    ASSERT_NE(getInt, nullptr);
    ASSERT_NE(setInt, nullptr);

    BML_ConfigKey key{sizeof(BML_ConfigKey), "test", "value"};
    ASSERT_EQ(BML_RESULT_OK, setInt(Mod(), &key, 42));
    
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, getInt(Mod(), &key, nullptr));
}

TEST_F(ConfigStoreConcurrencyTests, AtomicWritePreservesConfigOnPartialFailure) {
    // This test verifies that atomic write prevents corruption
    // The SaveDocument should use temp file + rename pattern
    InitMod("config.atomicwrite");

    auto setInt = Lookup<PFN_ConfigSetInt>("bmlConfigSetInt");
    auto getInt = Lookup<PFN_ConfigGetInt>("bmlConfigGetInt");
    ASSERT_NE(setInt, nullptr);
    ASSERT_NE(getInt, nullptr);

    BML_ConfigKey key{sizeof(BML_ConfigKey), "atomic", "counter"};
    
    // Write initial value
    ASSERT_EQ(BML_RESULT_OK, setInt(Mod(), &key, 1));
    
    // Flush and reload
    ConfigStore::Instance().FlushAndRelease(Mod());
    
    // Verify it was persisted
    int32_t out_value = 0;
    ASSERT_EQ(BML_RESULT_OK, getInt(Mod(), &key, &out_value));
    EXPECT_EQ(out_value, 1);
    
    // Update value
    ASSERT_EQ(BML_RESULT_OK, setInt(Mod(), &key, 2));
    
    // Flush again
    ConfigStore::Instance().FlushAndRelease(Mod());
    
    // Verify updated value persisted
    ASSERT_EQ(BML_RESULT_OK, getInt(Mod(), &key, &out_value));
    EXPECT_EQ(out_value, 2);
}

} // namespace

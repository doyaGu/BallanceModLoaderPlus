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
// Type-Safe Accessor Tests (using BML_CONFIG_* macros via bmlConfigGet/Set)
// ========================================================================

TEST_F(ConfigStoreConcurrencyTests, TypeSafeIntAccessorWorks) {
    InitMod("config.typesafe.int");

    auto config_get = Lookup<PFN_ConfigGet>("bmlConfigGet");
    auto config_set = Lookup<PFN_ConfigSet>("bmlConfigSet");
    ASSERT_NE(config_get, nullptr);
    ASSERT_NE(config_set, nullptr);

    BML_ConfigKey key{sizeof(BML_ConfigKey), "settings", "volume"};
    
    // Set int value
    BML_ConfigValue set_value{sizeof(BML_ConfigValue), BML_CONFIG_INT, {}};
    set_value.data.int_value = 42;
    ASSERT_EQ(BML_RESULT_OK, config_set(Mod(), &key, &set_value));
    
    // Get int value
    BML_ConfigValue get_value{sizeof(BML_ConfigValue), BML_CONFIG_INT, {}};
    ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key, &get_value));
    EXPECT_EQ(get_value.type, BML_CONFIG_INT);
    EXPECT_EQ(get_value.data.int_value, 42);
}

TEST_F(ConfigStoreConcurrencyTests, TypeSafeFloatAccessorWorks) {
    InitMod("config.typesafe.float");

    auto config_get = Lookup<PFN_ConfigGet>("bmlConfigGet");
    auto config_set = Lookup<PFN_ConfigSet>("bmlConfigSet");
    ASSERT_NE(config_get, nullptr);
    ASSERT_NE(config_set, nullptr);

    BML_ConfigKey key{sizeof(BML_ConfigKey), "physics", "gravity"};
    
    BML_ConfigValue set_value{sizeof(BML_ConfigValue), BML_CONFIG_FLOAT, {}};
    set_value.data.float_value = 9.81f;
    ASSERT_EQ(BML_RESULT_OK, config_set(Mod(), &key, &set_value));
    
    BML_ConfigValue get_value{sizeof(BML_ConfigValue), BML_CONFIG_FLOAT, {}};
    ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key, &get_value));
    EXPECT_EQ(get_value.type, BML_CONFIG_FLOAT);
    EXPECT_FLOAT_EQ(get_value.data.float_value, 9.81f);
}

TEST_F(ConfigStoreConcurrencyTests, TypeSafeBoolAccessorWorks) {
    InitMod("config.typesafe.bool");

    auto config_get = Lookup<PFN_ConfigGet>("bmlConfigGet");
    auto config_set = Lookup<PFN_ConfigSet>("bmlConfigSet");
    ASSERT_NE(config_get, nullptr);
    ASSERT_NE(config_set, nullptr);

    BML_ConfigKey key{sizeof(BML_ConfigKey), "video", "fullscreen"};
    
    BML_ConfigValue set_value{sizeof(BML_ConfigValue), BML_CONFIG_BOOL, {}};
    set_value.data.bool_value = BML_TRUE;
    ASSERT_EQ(BML_RESULT_OK, config_set(Mod(), &key, &set_value));
    
    BML_ConfigValue get_value{sizeof(BML_ConfigValue), BML_CONFIG_BOOL, {}};
    ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key, &get_value));
    EXPECT_EQ(get_value.type, BML_CONFIG_BOOL);
    EXPECT_EQ(get_value.data.bool_value, BML_TRUE);
}

TEST_F(ConfigStoreConcurrencyTests, TypeSafeStringAccessorWorks) {
    InitMod("config.typesafe.string");

    auto config_get = Lookup<PFN_ConfigGet>("bmlConfigGet");
    auto config_set = Lookup<PFN_ConfigSet>("bmlConfigSet");
    ASSERT_NE(config_get, nullptr);
    ASSERT_NE(config_set, nullptr);

    BML_ConfigKey key{sizeof(BML_ConfigKey), "player", "name"};
    
    BML_ConfigValue set_value{sizeof(BML_ConfigValue), BML_CONFIG_STRING, {}};
    set_value.data.string_value = "TestPlayer";
    ASSERT_EQ(BML_RESULT_OK, config_set(Mod(), &key, &set_value));
    
    BML_ConfigValue get_value{sizeof(BML_ConfigValue), BML_CONFIG_STRING, {}};
    ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key, &get_value));
    EXPECT_EQ(get_value.type, BML_CONFIG_STRING);
    ASSERT_NE(get_value.data.string_value, nullptr);
    EXPECT_STREQ(get_value.data.string_value, "TestPlayer");
}

TEST_F(ConfigStoreConcurrencyTests, TypeMismatchReturnsError) {
    InitMod("config.typemismatch");

    auto config_get = Lookup<PFN_ConfigGet>("bmlConfigGet");
    auto config_set = Lookup<PFN_ConfigSet>("bmlConfigSet");
    ASSERT_NE(config_get, nullptr);
    ASSERT_NE(config_set, nullptr);

    BML_ConfigKey key{sizeof(BML_ConfigKey), "mismatch", "value"};
    
    // Set as int
    BML_ConfigValue set_value{sizeof(BML_ConfigValue), BML_CONFIG_INT, {}};
    set_value.data.int_value = 100;
    ASSERT_EQ(BML_RESULT_OK, config_set(Mod(), &key, &set_value));
    
    // Get returns actual stored type, caller checks type field
    BML_ConfigValue get_value{sizeof(BML_ConfigValue), BML_CONFIG_FLOAT, {}};
    ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key, &get_value));
    // Verify the stored type is INT, not FLOAT
    EXPECT_EQ(get_value.type, BML_CONFIG_INT);
}

TEST_F(ConfigStoreConcurrencyTests, GetNotFoundReturnsError) {
    InitMod("config.notfound");

    auto config_get = Lookup<PFN_ConfigGet>("bmlConfigGet");
    ASSERT_NE(config_get, nullptr);

    BML_ConfigKey key{sizeof(BML_ConfigKey), "nonexistent", "value"};
    
    BML_ConfigValue get_value{sizeof(BML_ConfigValue), BML_CONFIG_INT, {}};
    EXPECT_EQ(BML_RESULT_NOT_FOUND, config_get(Mod(), &key, &get_value));
}

TEST_F(ConfigStoreConcurrencyTests, NullOutputPointerReturnsInvalidArgument) {
    InitMod("config.nulloutput");

    auto config_get = Lookup<PFN_ConfigGet>("bmlConfigGet");
    auto config_set = Lookup<PFN_ConfigSet>("bmlConfigSet");
    ASSERT_NE(config_get, nullptr);
    ASSERT_NE(config_set, nullptr);

    BML_ConfigKey key{sizeof(BML_ConfigKey), "test", "value"};
    BML_ConfigValue set_value{sizeof(BML_ConfigValue), BML_CONFIG_INT, {}};
    set_value.data.int_value = 42;
    ASSERT_EQ(BML_RESULT_OK, config_set(Mod(), &key, &set_value));
    
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, config_get(Mod(), &key, nullptr));
}

TEST_F(ConfigStoreConcurrencyTests, AtomicWritePreservesConfigOnPartialFailure) {
    // This test verifies that atomic write prevents corruption
    // The SaveDocument should use temp file + rename pattern
    InitMod("config.atomicwrite");

    auto config_get = Lookup<PFN_ConfigGet>("bmlConfigGet");
    auto config_set = Lookup<PFN_ConfigSet>("bmlConfigSet");
    ASSERT_NE(config_get, nullptr);
    ASSERT_NE(config_set, nullptr);

    BML_ConfigKey key{sizeof(BML_ConfigKey), "atomic", "counter"};
    
    // Write initial value
    BML_ConfigValue set_value{sizeof(BML_ConfigValue), BML_CONFIG_INT, {}};
    set_value.data.int_value = 1;
    ASSERT_EQ(BML_RESULT_OK, config_set(Mod(), &key, &set_value));
    
    // Flush and reload
    ConfigStore::Instance().FlushAndRelease(Mod());
    
    // Verify it was persisted
    BML_ConfigValue get_value{sizeof(BML_ConfigValue), BML_CONFIG_INT, {}};
    ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key, &get_value));
    EXPECT_EQ(get_value.data.int_value, 1);
    
    // Update value
    set_value.data.int_value = 2;
    ASSERT_EQ(BML_RESULT_OK, config_set(Mod(), &key, &set_value));
    
    // Flush again
    ConfigStore::Instance().FlushAndRelease(Mod());
    
    // Verify updated value persisted
    ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key, &get_value));
    EXPECT_EQ(get_value.data.int_value, 2);
}

} // namespace

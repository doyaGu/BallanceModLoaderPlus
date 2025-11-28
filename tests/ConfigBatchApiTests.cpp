#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "Core/ApiRegistration.h"
#include "Core/ApiRegistry.h"
#include "Core/ConfigStore.h"
#include "Core/Context.h"
#include "Core/ModHandle.h"
#include "Core/ModManifest.h"

#include "bml_config.h"
#include "bml_errors.h"

using BML::Core::ApiRegistry;
using BML::Core::ConfigStore;
using BML::Core::Context;

namespace {

// Function pointer types for batch APIs
using PFN_ConfigGet = BML_Result (*)(BML_Mod, const BML_ConfigKey *, BML_ConfigValue *);
using PFN_ConfigSet = BML_Result (*)(BML_Mod, const BML_ConfigKey *, const BML_ConfigValue *);
using PFN_ConfigBatchBegin = BML_Result (*)(BML_Mod, BML_ConfigBatch *);
using PFN_ConfigBatchSet = BML_Result (*)(BML_ConfigBatch, const BML_ConfigKey *, const BML_ConfigValue *);
using PFN_ConfigBatchCommit = BML_Result (*)(BML_ConfigBatch);
using PFN_ConfigBatchDiscard = BML_Result (*)(BML_ConfigBatch);
using PFN_ConfigGetCaps = BML_Result (*)(BML_ConfigStoreCaps *);

class ConfigBatchApiTests : public ::testing::Test {
protected:
    void SetUp() override {
        ApiRegistry::Instance().Clear();
        Context::SetCurrentModule(nullptr);
        temp_root_ = std::filesystem::temp_directory_path() /
                     ("bml-batch-tests-" +
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

    void InitMod(const std::string &id) {
        manifest_ = std::make_unique<BML::Core::ModManifest>();
        manifest_->package.id = id;
        manifest_->package.name = id;
        manifest_->package.version = "1.0.0";
        manifest_->package.parsed_version = {1, 0, 0};

        std::filesystem::path base = temp_root_ / id;
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

std::atomic<uint64_t> ConfigBatchApiTests::counter_{1};

// ============================================================================
// Basic Batch API Tests
// ============================================================================

TEST_F(ConfigBatchApiTests, BatchApisAreRegistered) {
    auto batch_begin = Lookup<PFN_ConfigBatchBegin>("bmlConfigBatchBegin");
    auto batch_set = Lookup<PFN_ConfigBatchSet>("bmlConfigBatchSet");
    auto batch_commit = Lookup<PFN_ConfigBatchCommit>("bmlConfigBatchCommit");
    auto batch_discard = Lookup<PFN_ConfigBatchDiscard>("bmlConfigBatchDiscard");

    EXPECT_NE(batch_begin, nullptr);
    EXPECT_NE(batch_set, nullptr);
    EXPECT_NE(batch_commit, nullptr);
    EXPECT_NE(batch_discard, nullptr);
}

TEST_F(ConfigBatchApiTests, CapsIncludesBatchCapability) {
    auto get_caps = Lookup<PFN_ConfigGetCaps>("bmlConfigGetCaps");
    ASSERT_NE(get_caps, nullptr);

    BML_ConfigStoreCaps caps{};
    caps.struct_size = sizeof(BML_ConfigStoreCaps);
    ASSERT_EQ(BML_RESULT_OK, get_caps(&caps));

    EXPECT_NE(caps.feature_flags & BML_CONFIG_CAP_BATCH, 0u);
}

TEST_F(ConfigBatchApiTests, BatchBeginReturnsValidHandle) {
    InitMod("batch.begin.test");

    auto batch_begin = Lookup<PFN_ConfigBatchBegin>("bmlConfigBatchBegin");
    auto batch_discard = Lookup<PFN_ConfigBatchDiscard>("bmlConfigBatchDiscard");
    ASSERT_NE(batch_begin, nullptr);
    ASSERT_NE(batch_discard, nullptr);

    BML_ConfigBatch batch = nullptr;
    ASSERT_EQ(BML_RESULT_OK, batch_begin(Mod(), &batch));
    EXPECT_NE(batch, nullptr);

    // Clean up
    EXPECT_EQ(BML_RESULT_OK, batch_discard(batch));
}

TEST_F(ConfigBatchApiTests, BatchBeginRejectsNullOutput) {
    InitMod("batch.null.test");

    auto batch_begin = Lookup<PFN_ConfigBatchBegin>("bmlConfigBatchBegin");
    ASSERT_NE(batch_begin, nullptr);

    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, batch_begin(Mod(), nullptr));
}

TEST_F(ConfigBatchApiTests, BatchSetRejectsInvalidBatch) {
    auto batch_set = Lookup<PFN_ConfigBatchSet>("bmlConfigBatchSet");
    ASSERT_NE(batch_set, nullptr);

    BML_ConfigKey key = BML_CONFIG_KEY_INIT("category", "name");
    BML_ConfigValue value{};
    value.struct_size = sizeof(BML_ConfigValue);
    value.type = BML_CONFIG_INT;
    value.data.int_value = 42;

    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, batch_set(nullptr, &key, &value));
}

TEST_F(ConfigBatchApiTests, BatchSetRejectsNullKey) {
    InitMod("batch.nullkey.test");

    auto batch_begin = Lookup<PFN_ConfigBatchBegin>("bmlConfigBatchBegin");
    auto batch_set = Lookup<PFN_ConfigBatchSet>("bmlConfigBatchSet");
    auto batch_discard = Lookup<PFN_ConfigBatchDiscard>("bmlConfigBatchDiscard");
    ASSERT_NE(batch_begin, nullptr);
    ASSERT_NE(batch_set, nullptr);

    BML_ConfigBatch batch = nullptr;
    ASSERT_EQ(BML_RESULT_OK, batch_begin(Mod(), &batch));

    BML_ConfigValue value{};
    value.struct_size = sizeof(BML_ConfigValue);
    value.type = BML_CONFIG_INT;
    value.data.int_value = 42;

    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, batch_set(batch, nullptr, &value));

    batch_discard(batch);
}

TEST_F(ConfigBatchApiTests, BatchCommitAppliesAllChanges) {
    InitMod("batch.commit.test");

    auto config_get = Lookup<PFN_ConfigGet>("bmlConfigGet");
    auto batch_begin = Lookup<PFN_ConfigBatchBegin>("bmlConfigBatchBegin");
    auto batch_set = Lookup<PFN_ConfigBatchSet>("bmlConfigBatchSet");
    auto batch_commit = Lookup<PFN_ConfigBatchCommit>("bmlConfigBatchCommit");
    ASSERT_NE(config_get, nullptr);
    ASSERT_NE(batch_begin, nullptr);
    ASSERT_NE(batch_set, nullptr);
    ASSERT_NE(batch_commit, nullptr);

    BML_ConfigBatch batch = nullptr;
    ASSERT_EQ(BML_RESULT_OK, batch_begin(Mod(), &batch));

    // Add multiple values in batch
    BML_ConfigKey key1 = BML_CONFIG_KEY_INIT("general", "value1");
    BML_ConfigValue val1{};
    val1.struct_size = sizeof(BML_ConfigValue);
    val1.type = BML_CONFIG_INT;
    val1.data.int_value = 100;
    ASSERT_EQ(BML_RESULT_OK, batch_set(batch, &key1, &val1));

    BML_ConfigKey key2 = BML_CONFIG_KEY_INIT("general", "value2");
    BML_ConfigValue val2{};
    val2.struct_size = sizeof(BML_ConfigValue);
    val2.type = BML_CONFIG_FLOAT;
    val2.data.float_value = 3.14f;
    ASSERT_EQ(BML_RESULT_OK, batch_set(batch, &key2, &val2));

    BML_ConfigKey key3 = BML_CONFIG_KEY_INIT("settings", "enabled");
    BML_ConfigValue val3{};
    val3.struct_size = sizeof(BML_ConfigValue);
    val3.type = BML_CONFIG_BOOL;
    val3.data.bool_value = BML_TRUE;
    ASSERT_EQ(BML_RESULT_OK, batch_set(batch, &key3, &val3));

    // Commit batch
    ASSERT_EQ(BML_RESULT_OK, batch_commit(batch));

    // Verify all values were applied
    BML_ConfigValue result{};
    result.struct_size = sizeof(BML_ConfigValue);

    ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key1, &result));
    EXPECT_EQ(result.type, BML_CONFIG_INT);
    EXPECT_EQ(result.data.int_value, 100);

    ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key2, &result));
    EXPECT_EQ(result.type, BML_CONFIG_FLOAT);
    EXPECT_FLOAT_EQ(result.data.float_value, 3.14f);

    ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key3, &result));
    EXPECT_EQ(result.type, BML_CONFIG_BOOL);
    EXPECT_EQ(result.data.bool_value, BML_TRUE);
}

TEST_F(ConfigBatchApiTests, BatchDiscardDoesNotApplyChanges) {
    InitMod("batch.discard.test");

    auto config_get = Lookup<PFN_ConfigGet>("bmlConfigGet");
    auto config_set = Lookup<PFN_ConfigSet>("bmlConfigSet");
    auto batch_begin = Lookup<PFN_ConfigBatchBegin>("bmlConfigBatchBegin");
    auto batch_set = Lookup<PFN_ConfigBatchSet>("bmlConfigBatchSet");
    auto batch_discard = Lookup<PFN_ConfigBatchDiscard>("bmlConfigBatchDiscard");
    ASSERT_NE(config_get, nullptr);
    ASSERT_NE(config_set, nullptr);
    ASSERT_NE(batch_begin, nullptr);
    ASSERT_NE(batch_set, nullptr);
    ASSERT_NE(batch_discard, nullptr);

    // Set initial value
    BML_ConfigKey key = BML_CONFIG_KEY_INIT("test", "original");
    BML_ConfigValue original{};
    original.struct_size = sizeof(BML_ConfigValue);
    original.type = BML_CONFIG_INT;
    original.data.int_value = 10;
    ASSERT_EQ(BML_RESULT_OK, config_set(Mod(), &key, &original));

    // Start batch and set new value
    BML_ConfigBatch batch = nullptr;
    ASSERT_EQ(BML_RESULT_OK, batch_begin(Mod(), &batch));

    BML_ConfigValue new_val{};
    new_val.struct_size = sizeof(BML_ConfigValue);
    new_val.type = BML_CONFIG_INT;
    new_val.data.int_value = 999;
    ASSERT_EQ(BML_RESULT_OK, batch_set(batch, &key, &new_val));

    // Discard batch
    ASSERT_EQ(BML_RESULT_OK, batch_discard(batch));

    // Verify original value is unchanged
    BML_ConfigValue result{};
    result.struct_size = sizeof(BML_ConfigValue);
    ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key, &result));
    EXPECT_EQ(result.data.int_value, 10);
}

TEST_F(ConfigBatchApiTests, BatchCommitRejectsAlreadyCommittedBatch) {
    InitMod("batch.double.commit");

    auto batch_begin = Lookup<PFN_ConfigBatchBegin>("bmlConfigBatchBegin");
    auto batch_commit = Lookup<PFN_ConfigBatchCommit>("bmlConfigBatchCommit");
    ASSERT_NE(batch_begin, nullptr);
    ASSERT_NE(batch_commit, nullptr);

    BML_ConfigBatch batch = nullptr;
    ASSERT_EQ(BML_RESULT_OK, batch_begin(Mod(), &batch));
    ASSERT_EQ(BML_RESULT_OK, batch_commit(batch));

    // Second commit should fail (batch already consumed)
    EXPECT_NE(BML_RESULT_OK, batch_commit(batch));
}

TEST_F(ConfigBatchApiTests, BatchDiscardRejectsAlreadyDiscardedBatch) {
    InitMod("batch.double.discard");

    auto batch_begin = Lookup<PFN_ConfigBatchBegin>("bmlConfigBatchBegin");
    auto batch_discard = Lookup<PFN_ConfigBatchDiscard>("bmlConfigBatchDiscard");
    ASSERT_NE(batch_begin, nullptr);
    ASSERT_NE(batch_discard, nullptr);

    BML_ConfigBatch batch = nullptr;
    ASSERT_EQ(BML_RESULT_OK, batch_begin(Mod(), &batch));
    ASSERT_EQ(BML_RESULT_OK, batch_discard(batch));

    // Second discard should fail
    EXPECT_NE(BML_RESULT_OK, batch_discard(batch));
}

TEST_F(ConfigBatchApiTests, BatchSetAfterCommitFails) {
    InitMod("batch.set.after.commit");

    auto batch_begin = Lookup<PFN_ConfigBatchBegin>("bmlConfigBatchBegin");
    auto batch_set = Lookup<PFN_ConfigBatchSet>("bmlConfigBatchSet");
    auto batch_commit = Lookup<PFN_ConfigBatchCommit>("bmlConfigBatchCommit");
    ASSERT_NE(batch_begin, nullptr);
    ASSERT_NE(batch_set, nullptr);
    ASSERT_NE(batch_commit, nullptr);

    BML_ConfigBatch batch = nullptr;
    ASSERT_EQ(BML_RESULT_OK, batch_begin(Mod(), &batch));
    ASSERT_EQ(BML_RESULT_OK, batch_commit(batch));

    BML_ConfigKey key = BML_CONFIG_KEY_INIT("test", "value");
    BML_ConfigValue val{};
    val.struct_size = sizeof(BML_ConfigValue);
    val.type = BML_CONFIG_INT;
    val.data.int_value = 42;

    // batch_set after commit should fail (batch no longer exists)
    EXPECT_NE(BML_RESULT_OK, batch_set(batch, &key, &val));
}

TEST_F(ConfigBatchApiTests, BatchSupportsAllConfigTypes) {
    InitMod("batch.types.test");

    auto config_get = Lookup<PFN_ConfigGet>("bmlConfigGet");
    auto batch_begin = Lookup<PFN_ConfigBatchBegin>("bmlConfigBatchBegin");
    auto batch_set = Lookup<PFN_ConfigBatchSet>("bmlConfigBatchSet");
    auto batch_commit = Lookup<PFN_ConfigBatchCommit>("bmlConfigBatchCommit");

    BML_ConfigBatch batch = nullptr;
    ASSERT_EQ(BML_RESULT_OK, batch_begin(Mod(), &batch));

    // Bool
    BML_ConfigKey key_bool = BML_CONFIG_KEY_INIT("types", "bool_val");
    BML_ConfigValue val_bool{};
    val_bool.struct_size = sizeof(BML_ConfigValue);
    val_bool.type = BML_CONFIG_BOOL;
    val_bool.data.bool_value = BML_TRUE;
    ASSERT_EQ(BML_RESULT_OK, batch_set(batch, &key_bool, &val_bool));

    // Int
    BML_ConfigKey key_int = BML_CONFIG_KEY_INIT("types", "int_val");
    BML_ConfigValue val_int{};
    val_int.struct_size = sizeof(BML_ConfigValue);
    val_int.type = BML_CONFIG_INT;
    val_int.data.int_value = -12345;
    ASSERT_EQ(BML_RESULT_OK, batch_set(batch, &key_int, &val_int));

    // Float
    BML_ConfigKey key_float = BML_CONFIG_KEY_INIT("types", "float_val");
    BML_ConfigValue val_float{};
    val_float.struct_size = sizeof(BML_ConfigValue);
    val_float.type = BML_CONFIG_FLOAT;
    val_float.data.float_value = 2.71828f;
    ASSERT_EQ(BML_RESULT_OK, batch_set(batch, &key_float, &val_float));

    // String
    BML_ConfigKey key_string = BML_CONFIG_KEY_INIT("types", "string_val");
    BML_ConfigValue val_string{};
    val_string.struct_size = sizeof(BML_ConfigValue);
    val_string.type = BML_CONFIG_STRING;
    val_string.data.string_value = "Hello, Batch!";
    ASSERT_EQ(BML_RESULT_OK, batch_set(batch, &key_string, &val_string));

    ASSERT_EQ(BML_RESULT_OK, batch_commit(batch));

    // Verify all types
    BML_ConfigValue result{};
    result.struct_size = sizeof(BML_ConfigValue);

    ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key_bool, &result));
    EXPECT_EQ(result.type, BML_CONFIG_BOOL);
    EXPECT_EQ(result.data.bool_value, BML_TRUE);

    ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key_int, &result));
    EXPECT_EQ(result.type, BML_CONFIG_INT);
    EXPECT_EQ(result.data.int_value, -12345);

    ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key_float, &result));
    EXPECT_EQ(result.type, BML_CONFIG_FLOAT);
    EXPECT_FLOAT_EQ(result.data.float_value, 2.71828f);

    ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key_string, &result));
    EXPECT_EQ(result.type, BML_CONFIG_STRING);
    EXPECT_STREQ(result.data.string_value, "Hello, Batch!");
}

TEST_F(ConfigBatchApiTests, MultipleBatchesCanExistConcurrently) {
    InitMod("batch.concurrent.test");

    auto batch_begin = Lookup<PFN_ConfigBatchBegin>("bmlConfigBatchBegin");
    auto batch_set = Lookup<PFN_ConfigBatchSet>("bmlConfigBatchSet");
    auto batch_commit = Lookup<PFN_ConfigBatchCommit>("bmlConfigBatchCommit");
    auto batch_discard = Lookup<PFN_ConfigBatchDiscard>("bmlConfigBatchDiscard");
    auto config_get = Lookup<PFN_ConfigGet>("bmlConfigGet");

    // Create two batches
    BML_ConfigBatch batch1 = nullptr, batch2 = nullptr;
    ASSERT_EQ(BML_RESULT_OK, batch_begin(Mod(), &batch1));
    ASSERT_EQ(BML_RESULT_OK, batch_begin(Mod(), &batch2));
    EXPECT_NE(batch1, batch2);

    // Set different values in each batch
    BML_ConfigKey key1 = BML_CONFIG_KEY_INIT("multi", "batch1_val");
    BML_ConfigValue val1{};
    val1.struct_size = sizeof(BML_ConfigValue);
    val1.type = BML_CONFIG_INT;
    val1.data.int_value = 111;
    ASSERT_EQ(BML_RESULT_OK, batch_set(batch1, &key1, &val1));

    BML_ConfigKey key2 = BML_CONFIG_KEY_INIT("multi", "batch2_val");
    BML_ConfigValue val2{};
    val2.struct_size = sizeof(BML_ConfigValue);
    val2.type = BML_CONFIG_INT;
    val2.data.int_value = 222;
    ASSERT_EQ(BML_RESULT_OK, batch_set(batch2, &key2, &val2));

    // Commit batch1, discard batch2
    ASSERT_EQ(BML_RESULT_OK, batch_commit(batch1));
    ASSERT_EQ(BML_RESULT_OK, batch_discard(batch2));

    // Verify batch1 changes applied, batch2 not
    BML_ConfigValue result{};
    result.struct_size = sizeof(BML_ConfigValue);
    ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key1, &result));
    EXPECT_EQ(result.data.int_value, 111);

    EXPECT_NE(BML_RESULT_OK, config_get(Mod(), &key2, &result));
}

TEST_F(ConfigBatchApiTests, BatchOverwritesSameKeyMultipleTimes) {
    InitMod("batch.overwrite.test");

    auto config_get = Lookup<PFN_ConfigGet>("bmlConfigGet");
    auto batch_begin = Lookup<PFN_ConfigBatchBegin>("bmlConfigBatchBegin");
    auto batch_set = Lookup<PFN_ConfigBatchSet>("bmlConfigBatchSet");
    auto batch_commit = Lookup<PFN_ConfigBatchCommit>("bmlConfigBatchCommit");

    BML_ConfigBatch batch = nullptr;
    ASSERT_EQ(BML_RESULT_OK, batch_begin(Mod(), &batch));

    BML_ConfigKey key = BML_CONFIG_KEY_INIT("test", "overwrite");

    // Set same key multiple times
    BML_ConfigValue val1{};
    val1.struct_size = sizeof(BML_ConfigValue);
    val1.type = BML_CONFIG_INT;
    val1.data.int_value = 1;
    ASSERT_EQ(BML_RESULT_OK, batch_set(batch, &key, &val1));

    BML_ConfigValue val2{};
    val2.struct_size = sizeof(BML_ConfigValue);
    val2.type = BML_CONFIG_INT;
    val2.data.int_value = 2;
    ASSERT_EQ(BML_RESULT_OK, batch_set(batch, &key, &val2));

    BML_ConfigValue val3{};
    val3.struct_size = sizeof(BML_ConfigValue);
    val3.type = BML_CONFIG_INT;
    val3.data.int_value = 3;
    ASSERT_EQ(BML_RESULT_OK, batch_set(batch, &key, &val3));

    ASSERT_EQ(BML_RESULT_OK, batch_commit(batch));

    // Last value wins
    BML_ConfigValue result{};
    result.struct_size = sizeof(BML_ConfigValue);
    ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key, &result));
    EXPECT_EQ(result.data.int_value, 3);
}

TEST_F(ConfigBatchApiTests, EmptyBatchCommitsSuccessfully) {
    InitMod("batch.empty.test");

    auto batch_begin = Lookup<PFN_ConfigBatchBegin>("bmlConfigBatchBegin");
    auto batch_commit = Lookup<PFN_ConfigBatchCommit>("bmlConfigBatchCommit");
    ASSERT_NE(batch_begin, nullptr);
    ASSERT_NE(batch_commit, nullptr);

    BML_ConfigBatch batch = nullptr;
    ASSERT_EQ(BML_RESULT_OK, batch_begin(Mod(), &batch));

    // Commit empty batch should succeed
    EXPECT_EQ(BML_RESULT_OK, batch_commit(batch));
}

TEST_F(ConfigBatchApiTests, BatchWithManyEntries) {
    InitMod("batch.many.test");

    auto config_get = Lookup<PFN_ConfigGet>("bmlConfigGet");
    auto batch_begin = Lookup<PFN_ConfigBatchBegin>("bmlConfigBatchBegin");
    auto batch_set = Lookup<PFN_ConfigBatchSet>("bmlConfigBatchSet");
    auto batch_commit = Lookup<PFN_ConfigBatchCommit>("bmlConfigBatchCommit");

    BML_ConfigBatch batch = nullptr;
    ASSERT_EQ(BML_RESULT_OK, batch_begin(Mod(), &batch));

    constexpr int kNumEntries = 100;
    std::vector<std::string> key_names;
    key_names.reserve(kNumEntries);

    for (int i = 0; i < kNumEntries; ++i) {
        key_names.push_back("entry_" + std::to_string(i));
    }

    for (int i = 0; i < kNumEntries; ++i) {
        BML_ConfigKey key = BML_CONFIG_KEY_INIT("stress", key_names[i].c_str());
        BML_ConfigValue val{};
        val.struct_size = sizeof(BML_ConfigValue);
        val.type = BML_CONFIG_INT;
        val.data.int_value = i * 10;
        ASSERT_EQ(BML_RESULT_OK, batch_set(batch, &key, &val));
    }

    ASSERT_EQ(BML_RESULT_OK, batch_commit(batch));

    // Verify all entries
    for (int i = 0; i < kNumEntries; ++i) {
        BML_ConfigKey key = BML_CONFIG_KEY_INIT("stress", key_names[i].c_str());
        BML_ConfigValue result{};
        result.struct_size = sizeof(BML_ConfigValue);
        ASSERT_EQ(BML_RESULT_OK, config_get(Mod(), &key, &result));
        EXPECT_EQ(result.data.int_value, i * 10);
    }
}

} // namespace

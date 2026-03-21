/**
 * @file ConfigBlobTests.cpp
 * @brief Unit tests for Config blob type and internal flag extensions
 */

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "Core/ApiRegistration.h"
#include "Core/ApiRegistry.h"
#include "Core/ConfigStore.h"
#include "Core/Context.h"
#include "Core/ModHandle.h"
#include "Core/ModManifest.h"
#include "TestKernel.h"

#include "bml_config.h"
#include "bml_errors.h"

using BML::Core::ApiRegistry;
using BML::Core::ConfigStore;
using BML::Core::Context;
using BML::Core::Testing::TestKernel;

namespace {

using PFN_ConfigGet = BML_Result (*)(BML_Mod, const BML_ConfigKey *, BML_ConfigValue *);
using PFN_ConfigSet = BML_Result (*)(BML_Mod, const BML_ConfigKey *, const BML_ConfigValue *);
using PFN_ConfigEnum = BML_Result (*)(BML_Mod, BML_ConfigEnumCallback, void *);
using PFN_ConfigBatchBegin = BML_Result (*)(BML_Mod, BML_ConfigBatch *);
using PFN_ConfigBatchSet = BML_Result (*)(BML_ConfigBatch, const BML_ConfigKey *, const BML_ConfigValue *);
using PFN_ConfigBatchCommit = BML_Result (*)(BML_ConfigBatch);

class ConfigBlobTests : public ::testing::Test {
protected:
    TestKernel kernel_;

    void SetUp() override {
        kernel_->api_registry = std::make_unique<ApiRegistry>();
        kernel_->config       = std::make_unique<ConfigStore>();
        kernel_->context      = std::make_unique<Context>(*kernel_->api_registry, *kernel_->config);
        kernel_->api_registry->Clear();
        Context::SetCurrentModule(nullptr);
        temp_root_ = std::filesystem::temp_directory_path() /
                     ("bml-blob-tests-" +
                      std::to_string(counter_.fetch_add(1, std::memory_order_relaxed)));
        std::filesystem::create_directories(temp_root_);
        BML::Core::RegisterConfigApis();
    }

    void TearDown() override {
        if (mod_) {
            kernel_->config->FlushAndRelease(mod_.get());
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

        mod_ = kernel_->context->CreateModHandle(*manifest_);
        Context::SetCurrentModule(mod_.get());
    }

    BML_Mod Mod() const { return mod_ ? mod_.get() : nullptr; }

    template <typename Fn>
    Fn Lookup(const char *name) {
        return reinterpret_cast<Fn>(kernel_->api_registry->Get(name));
    }

    static std::atomic<uint64_t> counter_;
    std::filesystem::path temp_root_;
    std::unique_ptr<BML::Core::ModManifest> manifest_;
    std::unique_ptr<BML_Mod_T> mod_;
};

std::atomic<uint64_t> ConfigBlobTests::counter_{1};

// ============================================================================
// Blob Set/Get Round-Trip
// ============================================================================

TEST_F(ConfigBlobTests, BlobSetGetRoundTrip) {
    InitMod("blob.roundtrip");
    auto configSet = Lookup<PFN_ConfigSet>("bmlConfigSet");
    auto configGet = Lookup<PFN_ConfigGet>("bmlConfigGet");
    ASSERT_NE(configSet, nullptr);
    ASSERT_NE(configGet, nullptr);

    uint8_t blob[] = {0x00, 0x01, 0x42, 0xFF, 0xFE, 0x80, 0x7F};
    BML_ConfigKey key = BML_CONFIG_KEY_INIT("data", "my_blob");
    BML_ConfigValue val = BML_CONFIG_VALUE_INIT_BLOB(blob, sizeof(blob));

    ASSERT_EQ(BML_RESULT_OK, configSet(Mod(), &key, &val));

    BML_ConfigValue out = {sizeof(BML_ConfigValue), BML_CONFIG_BLOB, {}, 0, 0};
    ASSERT_EQ(BML_RESULT_OK, configGet(Mod(), &key, &out));

    EXPECT_EQ(out.type, BML_CONFIG_BLOB);
    ASSERT_EQ(out.blob_size, sizeof(blob));
    EXPECT_EQ(std::memcmp(out.data.blob_value, blob, sizeof(blob)), 0);
}

TEST_F(ConfigBlobTests, EmptyBlobRoundTrip) {
    InitMod("blob.empty");
    auto configSet = Lookup<PFN_ConfigSet>("bmlConfigSet");
    auto configGet = Lookup<PFN_ConfigGet>("bmlConfigGet");

    BML_ConfigKey key = BML_CONFIG_KEY_INIT("data", "empty");
    BML_ConfigValue val = BML_CONFIG_VALUE_INIT_BLOB(nullptr, 0);

    ASSERT_EQ(BML_RESULT_OK, configSet(Mod(), &key, &val));

    BML_ConfigValue out = {sizeof(BML_ConfigValue), BML_CONFIG_BLOB, {}, 0, 0};
    ASSERT_EQ(BML_RESULT_OK, configGet(Mod(), &key, &out));
    EXPECT_EQ(out.type, BML_CONFIG_BLOB);
    EXPECT_EQ(out.blob_size, 0u);
}

TEST_F(ConfigBlobTests, BlobWithNullBytes) {
    InitMod("blob.nullbytes");
    auto configSet = Lookup<PFN_ConfigSet>("bmlConfigSet");
    auto configGet = Lookup<PFN_ConfigGet>("bmlConfigGet");

    // Blob with embedded null bytes -- must survive base64 encode/decode
    uint8_t blob[] = {0x00, 0x00, 0x00, 0x41, 0x00, 0x42};
    BML_ConfigKey key = BML_CONFIG_KEY_INIT("data", "nulls");
    BML_ConfigValue val = BML_CONFIG_VALUE_INIT_BLOB(blob, sizeof(blob));

    ASSERT_EQ(BML_RESULT_OK, configSet(Mod(), &key, &val));

    BML_ConfigValue out = {sizeof(BML_ConfigValue), BML_CONFIG_BLOB, {}, 0, 0};
    ASSERT_EQ(BML_RESULT_OK, configGet(Mod(), &key, &out));
    ASSERT_EQ(out.blob_size, sizeof(blob));
    EXPECT_EQ(std::memcmp(out.data.blob_value, blob, sizeof(blob)), 0);
}

// ============================================================================
// Blob Persistence Through Save/Load Cycle
// ============================================================================

TEST_F(ConfigBlobTests, BlobSurvivesReload) {
    InitMod("blob.persist");
    auto configSet = Lookup<PFN_ConfigSet>("bmlConfigSet");
    auto configGet = Lookup<PFN_ConfigGet>("bmlConfigGet");

    uint8_t blob[] = {0xDE, 0xAD, 0xBE, 0xEF};
    BML_ConfigKey key = BML_CONFIG_KEY_INIT("records", "best_time");
    BML_ConfigValue val = BML_CONFIG_VALUE_INIT_BLOB(blob, sizeof(blob));

    ASSERT_EQ(BML_RESULT_OK, configSet(Mod(), &key, &val));

    // Flush and release, then reload -- simulates game restart
    kernel_->config->FlushAndRelease(mod_.get());

    BML_ConfigValue out = {sizeof(BML_ConfigValue), BML_CONFIG_BLOB, {}, 0, 0};
    ASSERT_EQ(BML_RESULT_OK, configGet(Mod(), &key, &out));
    ASSERT_EQ(out.blob_size, sizeof(blob));
    EXPECT_EQ(std::memcmp(out.data.blob_value, blob, sizeof(blob)), 0);
}

// ============================================================================
// Internal Flag
// ============================================================================

TEST_F(ConfigBlobTests, InternalFlagPreserved) {
    InitMod("flag.internal");
    auto configSet = Lookup<PFN_ConfigSet>("bmlConfigSet");
    auto configGet = Lookup<PFN_ConfigGet>("bmlConfigGet");

    BML_ConfigKey key = BML_CONFIG_KEY_INIT("hidden", "score");
    BML_ConfigValue val = BML_CONFIG_VALUE_INIT_INTERNAL_INT(42);

    ASSERT_EQ(BML_RESULT_OK, configSet(Mod(), &key, &val));

    BML_ConfigValue out = {sizeof(BML_ConfigValue), BML_CONFIG_INT, {}, 0, 0};
    ASSERT_EQ(BML_RESULT_OK, configGet(Mod(), &key, &out));
    EXPECT_EQ(out.data.int_value, 42);
    EXPECT_EQ(out.flags & BML_CONFIG_FLAG_INTERNAL, BML_CONFIG_FLAG_INTERNAL);
}

TEST_F(ConfigBlobTests, InternalFlagSurvivesReload) {
    InitMod("flag.persist");
    auto configSet = Lookup<PFN_ConfigSet>("bmlConfigSet");
    auto configGet = Lookup<PFN_ConfigGet>("bmlConfigGet");

    BML_ConfigKey key = BML_CONFIG_KEY_INIT("hidden", "data");
    BML_ConfigValue val = BML_CONFIG_VALUE_INIT_INTERNAL_STRING("secret");

    ASSERT_EQ(BML_RESULT_OK, configSet(Mod(), &key, &val));

    kernel_->config->FlushAndRelease(mod_.get());

    BML_ConfigValue out = {sizeof(BML_ConfigValue), BML_CONFIG_STRING, {}, 0, 0};
    ASSERT_EQ(BML_RESULT_OK, configGet(Mod(), &key, &out));
    EXPECT_STREQ(out.data.string_value, "secret");
    EXPECT_EQ(out.flags & BML_CONFIG_FLAG_INTERNAL, BML_CONFIG_FLAG_INTERNAL);
}

TEST_F(ConfigBlobTests, NonInternalFlagDefaultsToZero) {
    InitMod("flag.default");
    auto configSet = Lookup<PFN_ConfigSet>("bmlConfigSet");
    auto configGet = Lookup<PFN_ConfigGet>("bmlConfigGet");

    BML_ConfigKey key = BML_CONFIG_KEY_INIT("visible", "setting");
    BML_ConfigValue val = BML_CONFIG_VALUE_INIT_INT(7);

    ASSERT_EQ(BML_RESULT_OK, configSet(Mod(), &key, &val));

    BML_ConfigValue out = {sizeof(BML_ConfigValue), BML_CONFIG_INT, {}, 0, 0};
    ASSERT_EQ(BML_RESULT_OK, configGet(Mod(), &key, &out));
    EXPECT_EQ(out.flags, 0u);
}

// ============================================================================
// Internal Blob (combined)
// ============================================================================

TEST_F(ConfigBlobTests, InternalBlobRoundTrip) {
    InitMod("blob.internal");
    auto configSet = Lookup<PFN_ConfigSet>("bmlConfigSet");
    auto configGet = Lookup<PFN_ConfigGet>("bmlConfigGet");

    uint8_t blob[] = {1, 2, 3};
    BML_ConfigKey key = BML_CONFIG_KEY_INIT("internal", "cache");
    BML_ConfigValue val = BML_CONFIG_VALUE_INIT_BLOB(blob, sizeof(blob));
    val.flags = BML_CONFIG_FLAG_INTERNAL;

    ASSERT_EQ(BML_RESULT_OK, configSet(Mod(), &key, &val));

    kernel_->config->FlushAndRelease(mod_.get());

    BML_ConfigValue out = {sizeof(BML_ConfigValue), BML_CONFIG_BLOB, {}, 0, 0};
    ASSERT_EQ(BML_RESULT_OK, configGet(Mod(), &key, &out));
    EXPECT_EQ(out.type, BML_CONFIG_BLOB);
    ASSERT_EQ(out.blob_size, sizeof(blob));
    EXPECT_EQ(std::memcmp(out.data.blob_value, blob, sizeof(blob)), 0);
    EXPECT_EQ(out.flags & BML_CONFIG_FLAG_INTERNAL, BML_CONFIG_FLAG_INTERNAL);
}

// ============================================================================
// Blob via Batch API
// ============================================================================

TEST_F(ConfigBlobTests, BlobViaBatch) {
    InitMod("blob.batch");
    auto configGet = Lookup<PFN_ConfigGet>("bmlConfigGet");
    auto batchBegin = Lookup<PFN_ConfigBatchBegin>("bmlConfigBatchBegin");
    auto batchSet = Lookup<PFN_ConfigBatchSet>("bmlConfigBatchSet");
    auto batchCommit = Lookup<PFN_ConfigBatchCommit>("bmlConfigBatchCommit");
    ASSERT_NE(batchBegin, nullptr);

    uint8_t blob[] = {0xCA, 0xFE};
    BML_ConfigKey key = BML_CONFIG_KEY_INIT("batch", "blob");
    BML_ConfigValue val = BML_CONFIG_VALUE_INIT_BLOB(blob, sizeof(blob));

    BML_ConfigBatch batch = nullptr;
    ASSERT_EQ(BML_RESULT_OK, batchBegin(Mod(), &batch));
    ASSERT_EQ(BML_RESULT_OK, batchSet(batch, &key, &val));
    ASSERT_EQ(BML_RESULT_OK, batchCommit(batch));

    BML_ConfigValue out = {sizeof(BML_ConfigValue), BML_CONFIG_BLOB, {}, 0, 0};
    ASSERT_EQ(BML_RESULT_OK, configGet(Mod(), &key, &out));
    ASSERT_EQ(out.blob_size, sizeof(blob));
    EXPECT_EQ(std::memcmp(out.data.blob_value, blob, sizeof(blob)), 0);
}

// ============================================================================
// Enumerate includes blob entries
// ============================================================================

TEST_F(ConfigBlobTests, EnumerateIncludesBlob) {
    InitMod("blob.enum");
    auto configSet = Lookup<PFN_ConfigSet>("bmlConfigSet");
    auto configEnum = Lookup<PFN_ConfigEnum>("bmlConfigEnumerate");
    ASSERT_NE(configEnum, nullptr);

    // Set one int and one blob
    BML_ConfigKey k1 = BML_CONFIG_KEY_INIT("cat", "int_val");
    BML_ConfigValue v1 = BML_CONFIG_VALUE_INIT_INT(99);
    configSet(Mod(), &k1, &v1);

    uint8_t blob[] = {0xAB};
    BML_ConfigKey k2 = BML_CONFIG_KEY_INIT("cat", "blob_val");
    BML_ConfigValue v2 = BML_CONFIG_VALUE_INIT_BLOB(blob, 1);
    configSet(Mod(), &k2, &v2);

    struct EnumResult {
        int count = 0;
        bool found_blob = false;
    } result;

    auto callback = [](BML_Context, const BML_ConfigKey *key,
                        const BML_ConfigValue *value, void *ud) {
        auto *r = static_cast<EnumResult *>(ud);
        r->count++;
        if (value->type == BML_CONFIG_BLOB)
            r->found_blob = true;
    };

    ASSERT_EQ(BML_RESULT_OK, configEnum(Mod(), callback, &result));
    EXPECT_EQ(result.count, 2);
    EXPECT_TRUE(result.found_blob);
}

// ============================================================================
// Validation
// ============================================================================

TEST_F(ConfigBlobTests, BlobWithNullDataAndNonZeroSizeRejected) {
    InitMod("blob.validate");
    auto configSet = Lookup<PFN_ConfigSet>("bmlConfigSet");

    BML_ConfigKey key = BML_CONFIG_KEY_INIT("data", "bad");
    BML_ConfigValue val = {sizeof(BML_ConfigValue), BML_CONFIG_BLOB, {}, 0, 0};
    val.data.blob_value = nullptr;
    val.blob_size = 10;  // non-zero size with null pointer

    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, configSet(Mod(), &key, &val));
}

} // namespace

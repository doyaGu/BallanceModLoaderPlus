#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "Core/ApiRegistration.h"
#include "Core/ApiRegistry.h"
#include "Core/Context.h"
#include "Core/ModHandle.h"
#include "Core/ModManifest.h"

#include "bml_extension.h"
#include "bml_errors.h"
#include "bml_version.h"

using BML::Core::ApiRegistry;
using BML::Core::Context;

namespace {

// Function pointer types
using PFN_ExtensionRegister = BML_Result (*)(const BML_ExtensionDesc *);
using PFN_ExtensionQuery = BML_Result (*)(const char *, BML_ExtensionInfo *);
using PFN_ExtensionLoad = BML_Result (*)(const char *, const BML_Version *, void **, BML_ExtensionInfo *);
using PFN_ExtensionUnload = BML_Result (*)(const char *);
using PFN_ExtensionUnregister = BML_Result (*)(const char *);
using PFN_ExtensionGetRefCount = BML_Result (*)(const char *, uint32_t *);
using PFN_ExtensionEnumerate = BML_Result (*)(const BML_ExtensionFilter *, BML_ExtensionEnumCallback, void *);
using PFN_ExtensionCount = BML_Result (*)(const BML_ExtensionFilter *, uint32_t *);

class ExtensionRefCountTests : public ::testing::Test {
protected:
    void SetUp() override {
        ApiRegistry::Instance().Clear();
        Context::SetCurrentModule(nullptr);
        BML::Core::RegisterExtensionApis();
    }

    void TearDown() override {
        Context::SetCurrentModule(nullptr);
        mods_.clear();
        manifests_.clear();
    }

    template <typename Fn>
    Fn Lookup(const char *name) {
        auto fn = reinterpret_cast<Fn>(ApiRegistry::Instance().Get(name));
        if (!fn) {
            ADD_FAILURE() << "Missing API registration for " << name;
        }
        return fn;
    }

    BML_Mod MakeMod(const std::string &id) {
        auto manifest = std::make_unique<BML::Core::ModManifest>();
        manifest->package.id = id;
        manifest->package.name = id;
        manifest->package.version = "1.0.0";
        manifest->package.parsed_version = {1, 0, 0};
        manifest->directory = L"";
        manifest->manifest_path = L"";
        auto handle = Context::Instance().CreateModHandle(*manifest);
        BML_Mod mod = handle.get();
        manifests_.push_back(std::move(manifest));
        mods_.push_back(std::move(handle));
        return mod;
    }

private:
    std::vector<std::unique_ptr<BML::Core::ModManifest>> manifests_;
    std::vector<std::unique_ptr<BML_Mod_T>> mods_;
};

// ============================================================================
// Reference Count API Tests
// ============================================================================

TEST_F(ExtensionRefCountTests, RefCountApisAreRegistered) {
    auto unload = Lookup<PFN_ExtensionUnload>("bmlExtensionUnload");
    auto get_ref_count = Lookup<PFN_ExtensionGetRefCount>("bmlExtensionGetRefCount");

    EXPECT_NE(unload, nullptr);
    EXPECT_NE(get_ref_count, nullptr);
}

TEST_F(ExtensionRefCountTests, InitialRefCountIsZero) {
    auto reg = Lookup<PFN_ExtensionRegister>("bmlExtensionRegister");
    auto get_ref_count = Lookup<PFN_ExtensionGetRefCount>("bmlExtensionGetRefCount");
    ASSERT_NE(reg, nullptr);
    ASSERT_NE(get_ref_count, nullptr);

    auto provider = MakeMod("refcount.provider");
    Context::SetCurrentModule(provider);

    int api = 42;
    BML_ExtensionDesc desc = BML_EXTENSION_DESC_INIT;
    desc.name = "RefCount.Test";
    desc.version = bmlMakeVersion(1, 0, 0);
    desc.api_table = &api;
    desc.api_size = sizeof(api);

    ASSERT_EQ(BML_RESULT_OK, reg(&desc));

    uint32_t count = 999;
    ASSERT_EQ(BML_RESULT_OK, get_ref_count("RefCount.Test", &count));
    EXPECT_EQ(count, 0u);
}

TEST_F(ExtensionRefCountTests, LoadIncrementsRefCount) {
    auto reg = Lookup<PFN_ExtensionRegister>("bmlExtensionRegister");
    auto load = Lookup<PFN_ExtensionLoad>("bmlExtensionLoad");
    auto get_ref_count = Lookup<PFN_ExtensionGetRefCount>("bmlExtensionGetRefCount");
    ASSERT_NE(reg, nullptr);
    ASSERT_NE(load, nullptr);
    ASSERT_NE(get_ref_count, nullptr);

    auto provider = MakeMod("load.increment");
    Context::SetCurrentModule(provider);

    int api = 100;
    BML_ExtensionDesc desc = BML_EXTENSION_DESC_INIT;
    desc.name = "Load.Increment";
    desc.version = bmlMakeVersion(1, 0, 0);
    desc.api_table = &api;
    desc.api_size = sizeof(api);

    ASSERT_EQ(BML_RESULT_OK, reg(&desc));

    void *loaded = nullptr;
    BML_Version req_ver = bmlMakeVersion(1, 0, 0);
    ASSERT_EQ(BML_RESULT_OK, load("Load.Increment", &req_ver, &loaded, nullptr));

    uint32_t count = 0;
    ASSERT_EQ(BML_RESULT_OK, get_ref_count("Load.Increment", &count));
    EXPECT_EQ(count, 1u);

    // Load again
    ASSERT_EQ(BML_RESULT_OK, load("Load.Increment", &req_ver, &loaded, nullptr));
    ASSERT_EQ(BML_RESULT_OK, get_ref_count("Load.Increment", &count));
    EXPECT_EQ(count, 2u);
}

TEST_F(ExtensionRefCountTests, UnloadDecrementsRefCount) {
    auto reg = Lookup<PFN_ExtensionRegister>("bmlExtensionRegister");
    auto load = Lookup<PFN_ExtensionLoad>("bmlExtensionLoad");
    auto unload = Lookup<PFN_ExtensionUnload>("bmlExtensionUnload");
    auto get_ref_count = Lookup<PFN_ExtensionGetRefCount>("bmlExtensionGetRefCount");
    ASSERT_NE(reg, nullptr);
    ASSERT_NE(load, nullptr);
    ASSERT_NE(unload, nullptr);
    ASSERT_NE(get_ref_count, nullptr);

    auto provider = MakeMod("unload.decrement");
    Context::SetCurrentModule(provider);

    int api = 200;
    BML_ExtensionDesc desc = BML_EXTENSION_DESC_INIT;
    desc.name = "Unload.Decrement";
    desc.version = bmlMakeVersion(1, 0, 0);
    desc.api_table = &api;
    desc.api_size = sizeof(api);

    ASSERT_EQ(BML_RESULT_OK, reg(&desc));

    void *loaded = nullptr;
    BML_Version req_ver = bmlMakeVersion(1, 0, 0);
    ASSERT_EQ(BML_RESULT_OK, load("Unload.Decrement", &req_ver, &loaded, nullptr));
    ASSERT_EQ(BML_RESULT_OK, load("Unload.Decrement", &req_ver, &loaded, nullptr));

    uint32_t count = 0;
    ASSERT_EQ(BML_RESULT_OK, get_ref_count("Unload.Decrement", &count));
    EXPECT_EQ(count, 2u);

    ASSERT_EQ(BML_RESULT_OK, unload("Unload.Decrement"));
    ASSERT_EQ(BML_RESULT_OK, get_ref_count("Unload.Decrement", &count));
    EXPECT_EQ(count, 1u);

    ASSERT_EQ(BML_RESULT_OK, unload("Unload.Decrement"));
    ASSERT_EQ(BML_RESULT_OK, get_ref_count("Unload.Decrement", &count));
    EXPECT_EQ(count, 0u);
}

TEST_F(ExtensionRefCountTests, UnloadBelowZeroFails) {
    auto reg = Lookup<PFN_ExtensionRegister>("bmlExtensionRegister");
    auto unload = Lookup<PFN_ExtensionUnload>("bmlExtensionUnload");
    ASSERT_NE(reg, nullptr);
    ASSERT_NE(unload, nullptr);

    auto provider = MakeMod("unload.zero");
    Context::SetCurrentModule(provider);

    int api = 300;
    BML_ExtensionDesc desc = BML_EXTENSION_DESC_INIT;
    desc.name = "Unload.Zero";
    desc.version = bmlMakeVersion(1, 0, 0);
    desc.api_table = &api;
    desc.api_size = sizeof(api);

    ASSERT_EQ(BML_RESULT_OK, reg(&desc));

    // Unload without load should fail or return error
    BML_Result result = unload("Unload.Zero");
    EXPECT_NE(result, BML_RESULT_OK);
}

TEST_F(ExtensionRefCountTests, UnregisterWithRefCountFails) {
    auto reg = Lookup<PFN_ExtensionRegister>("bmlExtensionRegister");
    auto load = Lookup<PFN_ExtensionLoad>("bmlExtensionLoad");
    auto unregister = Lookup<PFN_ExtensionUnregister>("bmlExtensionUnregister");
    ASSERT_NE(reg, nullptr);
    ASSERT_NE(load, nullptr);
    ASSERT_NE(unregister, nullptr);

    auto provider = MakeMod("unregister.refcount");
    Context::SetCurrentModule(provider);

    int api = 400;
    BML_ExtensionDesc desc = BML_EXTENSION_DESC_INIT;
    desc.name = "Unregister.RefCount";
    desc.version = bmlMakeVersion(1, 0, 0);
    desc.api_table = &api;
    desc.api_size = sizeof(api);

    ASSERT_EQ(BML_RESULT_OK, reg(&desc));

    void *loaded = nullptr;
    BML_Version req_ver = bmlMakeVersion(1, 0, 0);
    ASSERT_EQ(BML_RESULT_OK, load("Unregister.RefCount", &req_ver, &loaded, nullptr));

    // Unregister should fail because ref count > 0
    EXPECT_EQ(BML_RESULT_EXTENSION_IN_USE, unregister("Unregister.RefCount"));
}

TEST_F(ExtensionRefCountTests, UnregisterAfterAllUnloadsSucceeds) {
    auto reg = Lookup<PFN_ExtensionRegister>("bmlExtensionRegister");
    auto load = Lookup<PFN_ExtensionLoad>("bmlExtensionLoad");
    auto unload = Lookup<PFN_ExtensionUnload>("bmlExtensionUnload");
    auto unregister = Lookup<PFN_ExtensionUnregister>("bmlExtensionUnregister");
    auto query = Lookup<PFN_ExtensionQuery>("bmlExtensionQuery");
    ASSERT_NE(reg, nullptr);
    ASSERT_NE(load, nullptr);
    ASSERT_NE(unload, nullptr);
    ASSERT_NE(unregister, nullptr);

    auto provider = MakeMod("unregister.success");
    Context::SetCurrentModule(provider);

    int api = 500;
    BML_ExtensionDesc desc = BML_EXTENSION_DESC_INIT;
    desc.name = "Unregister.Success";
    desc.version = bmlMakeVersion(1, 0, 0);
    desc.api_table = &api;
    desc.api_size = sizeof(api);

    ASSERT_EQ(BML_RESULT_OK, reg(&desc));

    void *loaded = nullptr;
    BML_Version req_ver = bmlMakeVersion(1, 0, 0);
    ASSERT_EQ(BML_RESULT_OK, load("Unregister.Success", &req_ver, &loaded, nullptr));
    ASSERT_EQ(BML_RESULT_OK, unload("Unregister.Success"));

    // Now unregister should succeed
    EXPECT_EQ(BML_RESULT_OK, unregister("Unregister.Success"));

    // Verify extension is gone
    EXPECT_EQ(BML_RESULT_NOT_FOUND, query("Unregister.Success", nullptr));
}

TEST_F(ExtensionRefCountTests, GetRefCountRejectsNullPointer) {
    auto get_ref_count = Lookup<PFN_ExtensionGetRefCount>("bmlExtensionGetRefCount");
    ASSERT_NE(get_ref_count, nullptr);

    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, get_ref_count("SomeExtension", nullptr));
}

TEST_F(ExtensionRefCountTests, GetRefCountReturnsNotFoundForUnknown) {
    auto get_ref_count = Lookup<PFN_ExtensionGetRefCount>("bmlExtensionGetRefCount");
    ASSERT_NE(get_ref_count, nullptr);

    uint32_t count = 999;
    EXPECT_EQ(BML_RESULT_NOT_FOUND, get_ref_count("NonExistent.Extension", &count));
}

// ============================================================================
// Glob Pattern Matching Tests (via Enumerate filter)
// ============================================================================

class ExtensionGlobMatchTests : public ExtensionRefCountTests {
protected:
    void SetUp() override {
        ExtensionRefCountTests::SetUp();
        RegisterTestExtensions();
    }

    void RegisterTestExtensions() {
        auto reg = Lookup<PFN_ExtensionRegister>("bmlExtensionRegister");
        ASSERT_NE(reg, nullptr);

        auto provider = MakeMod("glob.provider");
        Context::SetCurrentModule(provider);

        // Register several extensions with different name patterns
        const char *names[] = {
            "Audio.Player",
            "Audio.Recorder",
            "Video.Player",
            "Video.Encoder",
            "Input.Keyboard",
            "Input.Mouse",
            "Network.Http",
            "Network.WebSocket"
        };

        for (const char *name : names) {
            int *api = new int(static_cast<int>(reinterpret_cast<intptr_t>(name))); // Unique per extension
            apis_.push_back(api);

            BML_ExtensionDesc desc = BML_EXTENSION_DESC_INIT;
            desc.name = name;
            desc.version = bmlMakeVersion(1, 0, 0);
            desc.api_table = api;
            desc.api_size = sizeof(int);

            ASSERT_EQ(BML_RESULT_OK, reg(&desc)) << "Failed to register " << name;
        }
    }

    void TearDown() override {
        for (int *api : apis_) {
            delete api;
        }
        apis_.clear();
        ExtensionRefCountTests::TearDown();
    }

    std::vector<std::string> EnumerateWithPattern(const char *pattern) {
        auto enumerate = Lookup<PFN_ExtensionEnumerate>("bmlExtensionEnumerate");
        EXPECT_NE(enumerate, nullptr);

        BML_ExtensionFilter filter{};
        filter.struct_size = sizeof(BML_ExtensionFilter);
        filter.name_pattern = pattern;

        std::vector<std::string> results;
        auto callback = +[](BML_Context, const BML_ExtensionInfo *info, void *user_data) -> BML_Bool {
            auto *names = static_cast<std::vector<std::string> *>(user_data);
            if (info && info->name) {
                names->push_back(info->name);
            }
            return BML_TRUE;
        };

        enumerate(&filter, callback, &results);
        return results;
    }

    uint32_t CountWithPattern(const char *pattern) {
        auto count = Lookup<PFN_ExtensionCount>("bmlExtensionCount");
        EXPECT_NE(count, nullptr);

        BML_ExtensionFilter filter{};
        filter.struct_size = sizeof(BML_ExtensionFilter);
        filter.name_pattern = pattern;

        uint32_t result = 0;
        count(&filter, &result);
        return result;
    }

    std::vector<int *> apis_;
};

TEST_F(ExtensionGlobMatchTests, NoFilterReturnsAll) {
    auto results = EnumerateWithPattern(nullptr);
    EXPECT_EQ(results.size(), 8u);
}

TEST_F(ExtensionGlobMatchTests, ExactNameMatch) {
    auto results = EnumerateWithPattern("Audio.Player");
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], "Audio.Player");
}

TEST_F(ExtensionGlobMatchTests, WildcardSuffixMatch) {
    auto results = EnumerateWithPattern("Audio.*");
    EXPECT_EQ(results.size(), 2u);
    EXPECT_TRUE(std::find(results.begin(), results.end(), "Audio.Player") != results.end());
    EXPECT_TRUE(std::find(results.begin(), results.end(), "Audio.Recorder") != results.end());
}

TEST_F(ExtensionGlobMatchTests, WildcardPrefixMatch) {
    auto results = EnumerateWithPattern("*.Player");
    EXPECT_EQ(results.size(), 2u);
    EXPECT_TRUE(std::find(results.begin(), results.end(), "Audio.Player") != results.end());
    EXPECT_TRUE(std::find(results.begin(), results.end(), "Video.Player") != results.end());
}

TEST_F(ExtensionGlobMatchTests, WildcardMiddleMatch) {
    auto results = EnumerateWithPattern("*.*");
    EXPECT_EQ(results.size(), 8u); // All extensions match
}

TEST_F(ExtensionGlobMatchTests, SingleCharWildcard) {
    auto results = EnumerateWithPattern("Input.?ouse");
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], "Input.Mouse");
}

TEST_F(ExtensionGlobMatchTests, NoMatchReturnsEmpty) {
    auto results = EnumerateWithPattern("NonExistent.*");
    EXPECT_TRUE(results.empty());
}

TEST_F(ExtensionGlobMatchTests, CountMatchesEnumerate) {
    EXPECT_EQ(CountWithPattern("Audio.*"), 2u);
    EXPECT_EQ(CountWithPattern("*.Player"), 2u);
    EXPECT_EQ(CountWithPattern("Network.*"), 2u);
    EXPECT_EQ(CountWithPattern("*"), 8u);
}

TEST_F(ExtensionGlobMatchTests, ComplexPatternMatch) {
    // Pattern: Video.* should match Video.Player and Video.Encoder
    auto results = EnumerateWithPattern("Video.*");
    EXPECT_EQ(results.size(), 2u);
}

TEST_F(ExtensionGlobMatchTests, DoubleAsteriskMatch) {
    // ** should behave same as *
    auto results = EnumerateWithPattern("**");
    EXPECT_EQ(results.size(), 8u);
}

// ============================================================================
// ApiRegistry UpdateApiTable and MarkDeprecated Tests
// ============================================================================

class ApiRegistryUpdateTests : public ::testing::Test {
protected:
    void SetUp() override {
        ApiRegistry::Instance().Clear();
    }

    void TearDown() override {
        ApiRegistry::Instance().Clear();
    }
};

TEST_F(ApiRegistryUpdateTests, UpdateApiTableChangesPointer) {
    auto &registry = ApiRegistry::Instance();

    // Register initial API (must be EXTENSION type for UpdateApiTable to work)
    ApiRegistry::ApiMetadata meta{};
    meta.name = "test.update";
    meta.id = 50001;
    meta.pointer = reinterpret_cast<void *>(0x1000);
    meta.version_major = 1;
    meta.version_minor = 0;
    meta.version_patch = 0;
    meta.api_size = 8;
    meta.type = BML_API_TYPE_EXTENSION;  // Must be extension type
    meta.threading = BML_THREADING_FREE;
    meta.provider_mod = "test";

    registry.RegisterApi(meta);
    EXPECT_EQ(registry.Get("test.update"), reinterpret_cast<void *>(0x1000));

    // Update the API table
    void *new_table = reinterpret_cast<void *>(0x2000);
    EXPECT_TRUE(registry.UpdateApiTable("test.update", new_table, 16));

    // Verify update
    EXPECT_EQ(registry.Get("test.update"), reinterpret_cast<void *>(0x2000));
}

TEST_F(ApiRegistryUpdateTests, UpdateApiTableReturnsFalseForUnknown) {
    auto &registry = ApiRegistry::Instance();

    void *table = reinterpret_cast<void *>(0x3000);
    EXPECT_FALSE(registry.UpdateApiTable("nonexistent.api", table, 8));
}

TEST_F(ApiRegistryUpdateTests, MarkDeprecatedSetsFlag) {
    auto &registry = ApiRegistry::Instance();

    // Register API
    ApiRegistry::ApiMetadata meta{};
    meta.name = "test.deprecated";
    meta.id = 50002;
    meta.pointer = reinterpret_cast<void *>(0x4000);
    meta.version_major = 1;
    meta.version_minor = 0;
    meta.version_patch = 0;
    meta.type = BML_API_TYPE_CORE;
    meta.threading = BML_THREADING_FREE;
    meta.provider_mod = "test";

    registry.RegisterApi(meta);

    EXPECT_TRUE(registry.MarkDeprecated("test.deprecated", "test.newapi", "Use newapi instead"));

    // API should still be accessible
    EXPECT_NE(registry.Get("test.deprecated"), nullptr);
}

TEST_F(ApiRegistryUpdateTests, MarkDeprecatedReturnsFalseForUnknown) {
    auto &registry = ApiRegistry::Instance();

    EXPECT_FALSE(registry.MarkDeprecated("nonexistent.api", "replacement", "message"));
}

} // namespace

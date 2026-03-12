#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "Core/ApiRegistration.h"
#include "Core/ApiRegistry.h"
#include "Core/Context.h"
#include "Core/ModHandle.h"
#include "Core/ModManifest.h"

#include "bml_extension.h"
#include "bml_version.h"

using BML::Core::ApiRegistry;
using BML::Core::Context;

namespace {

class ExtensionApiValidationTests : public ::testing::Test {
protected:
    void SetUp() override {
        Context::SetCurrentModule(nullptr);
        Context::Instance().Cleanup();
        Context::Instance().Initialize(bmlGetApiVersion());
        ApiRegistry::Instance().Clear();
        Context::SetCurrentModule(nullptr);
        BML::Core::RegisterExtensionApis();
    }

    void TearDown() override {
        Context::SetCurrentModule(nullptr);
        Context::Instance().Cleanup();
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

        BML::Core::LoadedModule module{};
        module.id = id;
        module.manifest = manifest.get();
        module.mod_handle = std::move(handle);
        Context::Instance().AddLoadedModule(std::move(module));

        BML_Mod mod = Context::Instance().GetModHandleById(id);
        manifests_.push_back(std::move(manifest));
        return mod;
    }

private:
    std::vector<std::unique_ptr<BML::Core::ModManifest>> manifests_;
};

TEST_F(ExtensionApiValidationTests, RegisterQueryLoadEnumerateAndUnregister) {
    auto reg = Lookup<PFN_BML_ExtensionRegister>("bmlExtensionRegister");
    auto query = Lookup<PFN_BML_ExtensionQuery>("bmlExtensionQuery");
    auto load = Lookup<PFN_BML_ExtensionLoad>("bmlExtensionLoad");
    auto unload = Lookup<PFN_BML_ExtensionUnload>("bmlExtensionUnload");
    auto enumerate = Lookup<PFN_BML_ExtensionEnumerate>("bmlExtensionEnumerate");
    auto unregister = Lookup<PFN_BML_ExtensionUnregister>("bmlExtensionUnregister");
    auto get_caps = Lookup<PFN_BML_ExtensionGetCaps>("bmlExtensionGetCaps");

    ASSERT_NE(reg, nullptr);
    ASSERT_NE(query, nullptr);
    ASSERT_NE(load, nullptr);
    ASSERT_NE(enumerate, nullptr);
    ASSERT_NE(unregister, nullptr);
    ASSERT_NE(get_caps, nullptr);

    auto provider = MakeMod("ext.provider");
    Context::SetCurrentModule(provider);

    struct DummyApi {
        int value;
    } api{42};

    BML_ExtensionDesc desc = BML_EXTENSION_DESC_INIT;
    desc.name = "Test.Extension";
    desc.version = bmlMakeVersion(1, 2, 0);
    desc.api_table = &api;
    desc.api_size = sizeof(api);
    desc.description = "Test extension";

    ASSERT_EQ(BML_RESULT_OK, reg(&desc));

    BML_ExtensionCaps caps = BML_EXTENSION_CAPS_INIT;
    ASSERT_EQ(BML_RESULT_OK, get_caps(&caps));
    EXPECT_NE(caps.capability_flags & BML_EXTENSION_CAP_REGISTER, 0u);
    EXPECT_NE(caps.capability_flags & BML_EXTENSION_CAP_LOAD, 0u);
    EXPECT_GE(caps.registered_count, 1u);

    BML_ExtensionInfo info = BML_EXTENSION_INFO_INIT;
    ASSERT_EQ(BML_RESULT_OK, query("Test.Extension", &info));
    EXPECT_STREQ(info.provider_id, "ext.provider");
    EXPECT_EQ(info.version.major, 1u);
    EXPECT_EQ(info.version.minor, 2u);
    EXPECT_EQ(info.api_size, sizeof(api));
    EXPECT_NE(info.capabilities, 0u);

    void *loaded = nullptr;
    BML_Version req_ver = bmlMakeVersion(1, 0, 0);
    ASSERT_EQ(BML_RESULT_OK, load("Test.Extension", &req_ver, &loaded, nullptr));
    EXPECT_EQ(static_cast<void *>(&api), loaded);

    std::vector<std::string> enumerated;
    auto callback = +[](BML_Context, const BML_ExtensionInfo *entry, void *user_data) -> BML_Bool {
        auto *names = static_cast<std::vector<std::string> *>(user_data);
        names->push_back(entry && entry->name ? entry->name : "");
        return BML_TRUE;
    };
    ASSERT_EQ(BML_RESULT_OK, enumerate(nullptr, callback, &enumerated));
    ASSERT_EQ(enumerated.size(), 1u);
    EXPECT_EQ(enumerated[0], "Test.Extension");

    // Unload before unregistering to decrement refcount
    ASSERT_EQ(BML_RESULT_OK, unload("Test.Extension"));

    ASSERT_EQ(BML_RESULT_OK, unregister("Test.Extension"));
    EXPECT_EQ(BML_RESULT_NOT_FOUND, query("Test.Extension", nullptr));
}

TEST_F(ExtensionApiValidationTests, RegisterRejectsUndersizedDescriptor) {
    auto reg = Lookup<PFN_BML_ExtensionRegister>("bmlExtensionRegister");
    ASSERT_NE(reg, nullptr);

    auto provider = MakeMod("ext.invalid.desc");
    Context::SetCurrentModule(provider);

    int api = 1;
    BML_ExtensionDesc desc = BML_EXTENSION_DESC_INIT;
    desc.struct_size = sizeof(BML_ExtensionDesc) - 4;
    desc.name = "Invalid.Desc";
    desc.version = bmlMakeVersion(1, 0, 0);
    desc.api_table = &api;
    desc.api_size = sizeof(api);

    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, reg(&desc));
}

TEST_F(ExtensionApiValidationTests, QueryRejectsUndersizedInfoStruct) {
    auto reg = Lookup<PFN_BML_ExtensionRegister>("bmlExtensionRegister");
    auto query = Lookup<PFN_BML_ExtensionQuery>("bmlExtensionQuery");
    ASSERT_NE(reg, nullptr);
    ASSERT_NE(query, nullptr);

    auto provider = MakeMod("ext.query.size");
    Context::SetCurrentModule(provider);

    int api = 2;
    BML_ExtensionDesc desc = BML_EXTENSION_DESC_INIT;
    desc.name = "Query.Size";
    desc.version = bmlMakeVersion(1, 0, 0);
    desc.api_table = &api;
    desc.api_size = sizeof(api);
    ASSERT_EQ(BML_RESULT_OK, reg(&desc));

    BML_ExtensionInfo info{};
    info.struct_size = sizeof(BML_ExtensionInfo) - 4;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, query("Query.Size", &info));
}

TEST_F(ExtensionApiValidationTests, LoadRejectsUndersizedInfoStruct) {
    auto reg = Lookup<PFN_BML_ExtensionRegister>("bmlExtensionRegister");
    auto load = Lookup<PFN_BML_ExtensionLoad>("bmlExtensionLoad");
    ASSERT_NE(reg, nullptr);
    ASSERT_NE(load, nullptr);

    auto provider = MakeMod("ext.load.size");
    Context::SetCurrentModule(provider);

    int api = 3;
    BML_ExtensionDesc desc = BML_EXTENSION_DESC_INIT;
    desc.name = "Load.Size";
    desc.version = bmlMakeVersion(1, 0, 0);
    desc.api_table = &api;
    desc.api_size = sizeof(api);
    ASSERT_EQ(BML_RESULT_OK, reg(&desc));

    void *loaded = nullptr;
    BML_Version req = bmlMakeVersion(1, 0, 0);
    BML_ExtensionInfo info{};
    info.struct_size = sizeof(BML_ExtensionInfo) - 8;

    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, load("Load.Size", &req, &loaded, &info));
}

TEST_F(ExtensionApiValidationTests, EnumerateRejectsUndersizedFilter) {
    auto enumerate = Lookup<PFN_BML_ExtensionEnumerate>("bmlExtensionEnumerate");
    ASSERT_NE(enumerate, nullptr);

    BML_ExtensionFilter filter{};
    filter.struct_size = sizeof(BML_ExtensionFilter) - 4;

    auto callback = +[](BML_Context, const BML_ExtensionInfo *, void *) -> BML_Bool {
        return BML_TRUE;
    };

    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, enumerate(&filter, callback, nullptr));
}

TEST_F(ExtensionApiValidationTests, CountRejectsUndersizedFilter) {
    auto count = Lookup<PFN_BML_ExtensionCount>("bmlExtensionCount");
    ASSERT_NE(count, nullptr);

    BML_ExtensionFilter filter{};
    filter.struct_size = sizeof(BML_ExtensionFilter) - 4;

    uint32_t value = 0;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, count(&filter, &value));
}

TEST_F(ExtensionApiValidationTests, CapsRequireSizedStruct) {
    auto get_caps = Lookup<PFN_BML_ExtensionGetCaps>("bmlExtensionGetCaps");
    ASSERT_NE(get_caps, nullptr);

    BML_ExtensionCaps caps{};
    caps.struct_size = sizeof(BML_ExtensionCaps) - 4;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, get_caps(&caps));
}

TEST_F(ExtensionApiValidationTests, UnregisterRequiresOwnership) {
    auto reg = Lookup<PFN_BML_ExtensionRegister>("bmlExtensionRegister");
    auto unregister = Lookup<PFN_BML_ExtensionUnregister>("bmlExtensionUnregister");

    ASSERT_NE(reg, nullptr);
    ASSERT_NE(unregister, nullptr);

    auto owner = MakeMod("owner.mod");
    auto intruder = MakeMod("intruder.mod");

    Context::SetCurrentModule(owner);
    int api_value = 7;

    BML_ExtensionDesc desc = BML_EXTENSION_DESC_INIT;
    desc.name = "Ownership.Extension";
    desc.version = bmlMakeVersion(1, 0, 0);
    desc.api_table = &api_value;
    desc.api_size = sizeof(api_value);

    ASSERT_EQ(BML_RESULT_OK, reg(&desc));

    Context::SetCurrentModule(intruder);
    EXPECT_EQ(BML_RESULT_PERMISSION_DENIED, unregister("Ownership.Extension"));

    Context::SetCurrentModule(owner);
    EXPECT_EQ(BML_RESULT_OK, unregister("Ownership.Extension"));
}

TEST_F(ExtensionApiValidationTests, LoadRequiresCompatibleVersion) {
    auto reg = Lookup<PFN_BML_ExtensionRegister>("bmlExtensionRegister");
    auto load = Lookup<PFN_BML_ExtensionLoad>("bmlExtensionLoad");

    ASSERT_NE(reg, nullptr);
    ASSERT_NE(load, nullptr);

    auto provider = MakeMod("version.mod");
    Context::SetCurrentModule(provider);

    double api_table = 3.14;

    BML_ExtensionDesc desc = BML_EXTENSION_DESC_INIT;
    desc.name = "Versioned.Extension";
    desc.version = bmlMakeVersion(2, 5, 0);
    desc.api_table = &api_table;
    desc.api_size = sizeof(api_table);

    ASSERT_EQ(BML_RESULT_OK, reg(&desc));

    void *loaded = nullptr;
    BML_ExtensionInfo info = BML_EXTENSION_INFO_INIT;

    // Compatible: require 2.4, have 2.5
    BML_Version req_v24 = bmlMakeVersion(2, 4, 0);
    ASSERT_EQ(BML_RESULT_OK, load("Versioned.Extension", &req_v24, &loaded, &info));
    EXPECT_EQ(static_cast<void *>(&api_table), loaded);
    EXPECT_EQ(2u, info.version.major);
    EXPECT_EQ(5u, info.version.minor);

    // Incompatible: major version mismatch
    BML_Version req_v10 = bmlMakeVersion(1, 0, 0);
    EXPECT_EQ(BML_RESULT_VERSION_MISMATCH, load("Versioned.Extension", &req_v10, &loaded, nullptr));
    // Incompatible: minor version too high
    BML_Version req_v26 = bmlMakeVersion(2, 6, 0);
    EXPECT_EQ(BML_RESULT_VERSION_MISMATCH, load("Versioned.Extension", &req_v26, &loaded, nullptr));
}

TEST_F(ExtensionApiValidationTests, ExtensionCountWorks) {
    auto reg = Lookup<PFN_BML_ExtensionRegister>("bmlExtensionRegister");
    auto count = Lookup<PFN_BML_ExtensionCount>("bmlExtensionCount");

    ASSERT_NE(reg, nullptr);
    ASSERT_NE(count, nullptr);

    auto provider = MakeMod("count.mod");
    Context::SetCurrentModule(provider);

    uint32_t initial_count = 0;
    ASSERT_EQ(BML_RESULT_OK, count(nullptr, &initial_count));

    int api1 = 1, api2 = 2;
    BML_ExtensionDesc desc1 = BML_EXTENSION_DESC_INIT;
    desc1.name = "Count.Ext1";
    desc1.version = bmlMakeVersion(1, 0, 0);
    desc1.api_table = &api1;
    desc1.api_size = sizeof(api1);

    BML_ExtensionDesc desc2 = BML_EXTENSION_DESC_INIT;
    desc2.name = "Count.Ext2";
    desc2.version = bmlMakeVersion(1, 0, 0);
    desc2.api_table = &api2;
    desc2.api_size = sizeof(api2);

    ASSERT_EQ(BML_RESULT_OK, reg(&desc1));
    ASSERT_EQ(BML_RESULT_OK, reg(&desc2));

    uint32_t new_count = 0;
    ASSERT_EQ(BML_RESULT_OK, count(nullptr, &new_count));
    EXPECT_EQ(new_count, initial_count + 2);
}

TEST_F(ExtensionApiValidationTests, DeprecatedStateIsReportedByQueryAndLoad) {
    auto reg = Lookup<PFN_BML_ExtensionRegister>("bmlExtensionRegister");
    auto dep = Lookup<PFN_BML_ExtensionDeprecate>("bmlExtensionDeprecate");
    auto query = Lookup<PFN_BML_ExtensionQuery>("bmlExtensionQuery");
    auto load = Lookup<PFN_BML_ExtensionLoad>("bmlExtensionLoad");
    auto unload = Lookup<PFN_BML_ExtensionUnload>("bmlExtensionUnload");

    ASSERT_NE(reg, nullptr);
    ASSERT_NE(dep, nullptr);
    ASSERT_NE(query, nullptr);
    ASSERT_NE(load, nullptr);
    ASSERT_NE(unload, nullptr);

    auto provider = MakeMod("deprecated.provider");
    Context::SetCurrentModule(provider);

    int api = 11;
    BML_ExtensionDesc desc = BML_EXTENSION_DESC_INIT;
    desc.name = "Deprecated.Extension";
    desc.version = bmlMakeVersion(1, 0, 0);
    desc.api_table = &api;
    desc.api_size = sizeof(api);
    desc.description = "deprecated test";
    ASSERT_EQ(BML_RESULT_OK, reg(&desc));

    ASSERT_EQ(BML_RESULT_OK, dep("Deprecated.Extension", "Replacement.Extension", "moved"));

    BML_ExtensionInfo info = BML_EXTENSION_INFO_INIT;
    ASSERT_EQ(BML_RESULT_OK, query("Deprecated.Extension", &info));
    EXPECT_EQ(BML_EXTENSION_STATE_DEPRECATED, info.state);
    ASSERT_NE(info.replacement_name, nullptr);
    EXPECT_STREQ("Replacement.Extension", info.replacement_name);
    ASSERT_NE(info.deprecation_message, nullptr);
    EXPECT_STREQ("moved", info.deprecation_message);

    void *loaded = nullptr;
    BML_Version req = bmlMakeVersion(1, 0, 0);
    BML_ExtensionInfo load_info = BML_EXTENSION_INFO_INIT;
    ASSERT_EQ(BML_RESULT_OK, load("Deprecated.Extension", &req, &loaded, &load_info));
    EXPECT_EQ(BML_EXTENSION_STATE_DEPRECATED, load_info.state);
    ASSERT_EQ(BML_RESULT_OK, unload("Deprecated.Extension"));
}

TEST_F(ExtensionApiValidationTests, QueryInfoPointersStayStableAcrossQueries) {
    auto reg = Lookup<PFN_BML_ExtensionRegister>("bmlExtensionRegister");
    auto query = Lookup<PFN_BML_ExtensionQuery>("bmlExtensionQuery");

    ASSERT_NE(reg, nullptr);
    ASSERT_NE(query, nullptr);

    auto provider = MakeMod("stable.provider");
    Context::SetCurrentModule(provider);

    int api1 = 1;
    const char *tags1[] = {"alpha"};
    BML_ExtensionDesc first = BML_EXTENSION_DESC_INIT;
    first.name = "Stable.First";
    first.version = bmlMakeVersion(1, 0, 0);
    first.api_table = &api1;
    first.api_size = sizeof(api1);
    first.description = "first description";
    first.tags = tags1;
    first.tag_count = 1;
    ASSERT_EQ(BML_RESULT_OK, reg(&first));

    int api2 = 2;
    const char *tags2[] = {"beta"};
    BML_ExtensionDesc second = BML_EXTENSION_DESC_INIT;
    second.name = "Stable.Second";
    second.version = bmlMakeVersion(1, 0, 0);
    second.api_table = &api2;
    second.api_size = sizeof(api2);
    second.description = "second description";
    second.tags = tags2;
    second.tag_count = 1;
    ASSERT_EQ(BML_RESULT_OK, reg(&second));

    BML_ExtensionInfo first_info = BML_EXTENSION_INFO_INIT;
    BML_ExtensionInfo second_info = BML_EXTENSION_INFO_INIT;
    ASSERT_EQ(BML_RESULT_OK, query("Stable.First", &first_info));
    ASSERT_EQ(BML_RESULT_OK, query("Stable.Second", &second_info));

    ASSERT_NE(first_info.name, nullptr);
    EXPECT_STREQ("Stable.First", first_info.name);
    ASSERT_NE(first_info.description, nullptr);
    EXPECT_STREQ("first description", first_info.description);
    ASSERT_NE(first_info.tags, nullptr);
    ASSERT_GE(first_info.tag_count, 1u);
    EXPECT_STREQ("alpha", first_info.tags[0]);

    ASSERT_NE(second_info.name, nullptr);
    EXPECT_STREQ("Stable.Second", second_info.name);
    ASSERT_NE(second_info.description, nullptr);
    EXPECT_STREQ("second description", second_info.description);
    ASSERT_NE(second_info.tags, nullptr);
    ASSERT_GE(second_info.tag_count, 1u);
    EXPECT_STREQ("beta", second_info.tags[0]);
}

} // namespace

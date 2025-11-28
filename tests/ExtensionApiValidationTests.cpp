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

TEST_F(ExtensionApiValidationTests, RegisterQueryLoadEnumerateAndUnregister) {
    auto reg = Lookup<PFN_BML_ExtensionRegister>("bmlExtensionRegister");
    auto query = Lookup<PFN_BML_ExtensionQuery>("bmlExtensionQuery");
    auto load = Lookup<PFN_BML_ExtensionLoad>("bmlExtensionLoad");
    auto enumerate = Lookup<PFN_BML_ExtensionEnumerate>("bmlExtensionEnumerate");
    auto unregister = Lookup<PFN_BML_ExtensionUnregister>("bmlExtensionUnregister");
    auto get_caps = Lookup<PFN_BML_GetExtensionCaps>("bmlGetExtensionCaps");

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

    ASSERT_EQ(BML_RESULT_OK, unregister("Test.Extension"));
    EXPECT_EQ(BML_RESULT_NOT_FOUND, query("Test.Extension", nullptr));
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

} // namespace

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "Core/ApiRegistration.h"
#include "Core/ApiRegistry.h"
#include "Core/Context.h"
#include "Core/ModHandle.h"
#include "Core/ModManifest.h"

#include "bml_extension.h"

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

    ASSERT_EQ(BML_RESULT_OK, reg("Test.Extension", 1, 2, &api, sizeof(api)));

    BML_ExtensionCaps caps{};
    ASSERT_EQ(BML_RESULT_OK, get_caps(&caps));
    EXPECT_NE(caps.capability_flags & BML_EXTENSION_CAP_REGISTER, 0u);
    EXPECT_NE(caps.capability_flags & BML_EXTENSION_CAP_LOAD, 0u);
    EXPECT_GE(caps.max_extensions_hint, 1u);

    BML_ExtensionInfo info{};
    BML_Bool supported = BML_FALSE;
    ASSERT_EQ(BML_RESULT_OK, query("Test.Extension", &info, &supported));
    EXPECT_EQ(BML_TRUE, supported);
    EXPECT_STREQ(info.provider_id, "ext.provider");
    EXPECT_EQ(info.version_major, 1u);
    EXPECT_EQ(info.version_minor, 2u);

    void *loaded = nullptr;
    ASSERT_EQ(BML_RESULT_OK, load("Test.Extension", &loaded));
    EXPECT_EQ(static_cast<void *>(&api), loaded);

    std::vector<std::string> enumerated;
    auto callback = +[](BML_Context, const BML_ExtensionInfo *entry, void *user_data) {
        auto *names = static_cast<std::vector<std::string> *>(user_data);
        names->push_back(entry && entry->name ? entry->name : "");
    };
    ASSERT_EQ(BML_RESULT_OK, enumerate(callback, &enumerated));
    ASSERT_EQ(enumerated.size(), 1u);
    EXPECT_EQ(enumerated[0], "Test.Extension");

    ASSERT_EQ(BML_RESULT_OK, unregister("Test.Extension"));
    BML_Bool still_supported = BML_TRUE;
    EXPECT_EQ(BML_RESULT_NOT_FOUND, query("Test.Extension", nullptr, &still_supported));
    EXPECT_EQ(BML_FALSE, still_supported);
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
    ASSERT_EQ(BML_RESULT_OK, reg("Ownership.Extension", 1, 0, &api_value, sizeof(api_value)));

    Context::SetCurrentModule(intruder);
    EXPECT_EQ(BML_RESULT_PERMISSION_DENIED, unregister("Ownership.Extension"));

    Context::SetCurrentModule(owner);
    EXPECT_EQ(BML_RESULT_OK, unregister("Ownership.Extension"));
}

TEST_F(ExtensionApiValidationTests, LoadVersionedRequiresCompatibleVersion) {
    auto reg = Lookup<PFN_BML_ExtensionRegister>("bmlExtensionRegister");
    auto load_versioned = Lookup<PFN_BML_ExtensionLoadVersioned>("bmlExtensionLoadVersioned");

    ASSERT_NE(reg, nullptr);
    ASSERT_NE(load_versioned, nullptr);

    auto provider = MakeMod("version.mod");
    Context::SetCurrentModule(provider);

    double api_table = 3.14;
    ASSERT_EQ(BML_RESULT_OK, reg("Versioned.Extension", 2, 5, &api_table, sizeof(api_table)));

    void *loaded = nullptr;
    uint32_t actual_major = 0;
    uint32_t actual_minor = 0;
    ASSERT_EQ(BML_RESULT_OK,
              load_versioned("Versioned.Extension",
                              2,
                              4,
                              &loaded,
                              &actual_major,
                              &actual_minor));
    EXPECT_EQ(static_cast<void *>(&api_table), loaded);
    EXPECT_EQ(2u, actual_major);
    EXPECT_EQ(5u, actual_minor);

    EXPECT_EQ(BML_RESULT_VERSION_MISMATCH,
              load_versioned("Versioned.Extension", 1, 0, &loaded, nullptr, nullptr));
    EXPECT_EQ(BML_RESULT_VERSION_MISMATCH,
              load_versioned("Versioned.Extension", 2, 6, &loaded, nullptr, nullptr));
}

} // namespace

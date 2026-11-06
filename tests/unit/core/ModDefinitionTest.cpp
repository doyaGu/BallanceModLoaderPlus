#include "BML/Mod.hpp"
#include <gtest/gtest.h>

namespace {
int declarations = 0, constructions = 0, destructions = 0, starts = 0;
bool failDeclaration = false;

std::string ModId() {
    ++declarations;
    if (failDeclaration)
        throw std::bad_alloc();
    return "example.inline-definition-longer-than-SSO";
}

class InlineMod final : public BML::Mod::Instance {
  public:
    InlineMod() { ++constructions; }
    ~InlineMod() noexcept override { ++destructions; }
    void OnStartLevel(const BML::Mod::Context &) override { ++starts; }
};
} // namespace

int BML_CDECL BML_GetInterface(const char *, uint16_t, const void **out) {
    *out = nullptr;
    return BML_ERROR_NOT_FOUND;
}

BML_DEFINE_MOD(InlineMod, {
                              .Id = ModId(),
                              .Name = "内联声明与 \"quoted\" 名称",
                              .Version = "1.2.3",
                              .Dependencies = {{std::string("example.provider-longer-than-SSO"),
                                                std::string("2.0.0")},
                                               {"example.optional", "", true}},
                          })

TEST(ModDefinition, InlineFieldsAreLazyOwnedAndUseTheExistingProtocol) {
    const BML_ModInterface *api = reinterpret_cast<const BML_ModInterface *>(1);
    EXPECT_EQ(BML_GetModInterface(2, &api), BML_ERROR_VERSION_MISMATCH);
    EXPECT_EQ(api, nullptr);
    EXPECT_EQ(BML_GetModInterface(1, nullptr), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(declarations, 0);

    failDeclaration = true;
    EXPECT_EQ(BML_GetModInterface(1, &api), BML_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(api, nullptr);
    EXPECT_EQ(constructions, 0);

    failDeclaration = false;
    ASSERT_EQ(BML_GetModInterface(1, &api), BML_OK);
    const auto *first = api;
    ASSERT_EQ(BML_GetModInterface(1, &api), BML_OK);
    EXPECT_EQ(api, first);
    EXPECT_EQ(declarations, 2);
    EXPECT_EQ(constructions, 0);
    const auto &definition = *api->Definition;
    EXPECT_STREQ(definition.Id, "example.inline-definition-longer-than-SSO");
    EXPECT_STREQ(definition.Name, "内联声明与 \"quoted\" 名称");
    EXPECT_STREQ(definition.Version, "1.2.3");
    EXPECT_STREQ(definition.Author, "");
    EXPECT_STREQ(definition.Description, "");
    EXPECT_STREQ(definition.MinimumLoaderVersion, "");
    ASSERT_EQ(definition.DependencyCount, 2u);
    ASSERT_EQ(definition.DependencyStride, sizeof(BML_ModDependency));
    EXPECT_STREQ(definition.Dependencies[0].Id, "example.provider-longer-than-SSO");
    EXPECT_STREQ(definition.Dependencies[0].MinimumVersion, "2.0.0");
    EXPECT_EQ(definition.Dependencies[0].Optional, 0u);
    EXPECT_EQ(definition.Dependencies[1].Optional, 1u);
    EXPECT_EQ(api->Callbacks->OnLoad, nullptr);
    EXPECT_EQ(api->Callbacks->OnUnload, nullptr);
    EXPECT_EQ(api->Callbacks->OnProcess, nullptr);
    EXPECT_NE(api->Callbacks->OnGameEvent, nullptr);

    void *instance = nullptr;
    ASSERT_EQ(api->Create(123, &instance), BML_OK);
    ASSERT_NE(instance, nullptr);
    EXPECT_EQ(constructions, 1);
    EXPECT_EQ(api->Callbacks->OnGameEvent(instance, 123, BML_GAME_EVENT_START_LEVEL), BML_OK);
    EXPECT_EQ(starts, 1);
    api->Destroy(instance);
    EXPECT_EQ(destructions, 1);
    EXPECT_STREQ(api->Definition->Id, "example.inline-definition-longer-than-SSO");
}

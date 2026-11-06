#include "Loader/NativeMod.h"
#include <gtest/gtest.h>
#include <cstdlib>

extern "C" void *BML_CDECL BML_Malloc(size_t size) { return std::malloc(size); }
extern "C" void BML_CDECL BML_Free(void *pointer) { std::free(pointer); }

IMod::~IMod() = default;
ILogger *IMod::GetLogger() { return nullptr; }
IConfig *IMod::GetConfig() { return nullptr; }
namespace {
int creates, loads, unloads, destroys;
int unloadCode = BML_OK, loadCode = BML_OK;
int BML_CDECL Create(BML_ModContextHandle, void **out) {
    ++creates;
    *out = new int(123);
    return BML_OK;
}
void BML_CDECL Destroy(void *value) {
    ++destroys;
    delete static_cast<int *>(value);
}
int BML_CDECL Load(void *, BML_ModContextHandle) {
    ++loads;
    return loadCode;
}
int BML_CDECL Unload(void *, BML_ModContextHandle) {
    ++unloads;
    return unloadCode;
}
struct Fixture {
    std::string Id = "test.native", Name = "metadata name", Dependency = "test.provider";
    BML_ModDependency Dep{sizeof(Dep), Dependency.c_str(), "1.2.3", 0};
    BML_ModDefinition Definition{
        sizeof(Definition), Id.c_str(), Name.c_str(), "2.3.4", "author", "", "", &Dep, 1,
        sizeof(Dep)};
    BML_ModCallbacks Callbacks{sizeof(Callbacks), Load, Unload};
    BML_ModInterface Api{BML_IFACE_HEADER(BML_ModInterface, BML_MOD_INTERFACE_ID, 1, 0),
                         &Definition, &Callbacks, Create, Destroy};
    Fixture() {
        creates = loads = unloads = destroys = 0;
        loadCode = unloadCode = BML_OK;
    }
};
TEST(NativeMod, QueryCopiesMetadataWithoutConstructingAndAcceptsShortCallbacks) {
    Fixture fixture;
    fixture.Callbacks.StructSize = offsetof(BML_ModCallbacks, OnProcess);
    std::string diagnostic;
    auto mod = NativeMod::Query(nullptr, &fixture.Api, diagnostic);
    ASSERT_TRUE(mod) << diagnostic;
    EXPECT_EQ(creates, 0);
    fixture.Id.assign(100, 'x');
    fixture.Name.assign(200, 'y');
    fixture.Dependency.assign(100, 'z');
    EXPECT_STREQ(mod->GetID(), "test.native");
    EXPECT_STREQ(mod->GetName(), "metadata name");
    EXPECT_STREQ(mod->Definition().Dependencies[0].Id, "test.provider");
    EXPECT_EQ(mod->Callbacks().OnProcess, nullptr);
    ASSERT_EQ(mod->Create(123), BML_OK);
    ASSERT_EQ(mod->Load(), BML_OK);
    EXPECT_TRUE(mod->IsLoaded());
    EXPECT_EQ(mod->Unload(), BML_OK);
    EXPECT_EQ(mod->Unload(), BML_OK);
    mod->Destroy();
    mod->Destroy();
    EXPECT_EQ(creates, 1);
    EXPECT_EQ(loads, 1);
    EXPECT_EQ(unloads, 1);
    EXPECT_EQ(destroys, 1);
}
TEST(NativeMod, FailedOnLoadStillUnloadsAndFailedCleanupCanBeRetried) {
    Fixture fixture;
    std::string diagnostic;
    auto mod = NativeMod::Query(nullptr, &fixture.Api, diagnostic);
    ASSERT_TRUE(mod);
    ASSERT_EQ(mod->Create(123), BML_OK);
    loadCode = BML_ERROR_FAIL;
    EXPECT_EQ(mod->Load(), BML_ERROR_FAIL);
    EXPECT_TRUE(mod->IsFailed());
    EXPECT_FALSE(mod->IsLoading());
    EXPECT_TRUE(mod->HasInstance());
    unloadCode = BML_ERROR_BUSY;
    EXPECT_EQ(mod->Unload(), BML_ERROR_BUSY);
    EXPECT_FALSE(mod->CanDestroy());
    EXPECT_EQ(destroys, 0);
    unloadCode = BML_OK;
    EXPECT_EQ(mod->Unload(), BML_OK);
    EXPECT_TRUE(mod->CanDestroy());
    mod->Destroy();
    EXPECT_EQ(unloads, 2);
    EXPECT_EQ(destroys, 1);
}
TEST(NativeMod, MalformedMetadataIsRejectedBeforeCreate) {
    Fixture fixture;
    std::string diagnostic;
    fixture.Definition.DependencyStride = sizeof(BML_ModDependency) - 1;
    EXPECT_FALSE(NativeMod::Query(nullptr, &fixture.Api, diagnostic));
    fixture.Definition.DependencyStride = sizeof(BML_ModDependency);
    fixture.Dep.Optional = 2;
    EXPECT_FALSE(NativeMod::Query(nullptr, &fixture.Api, diagnostic));
    fixture.Dep.Optional = 0;
    fixture.Dep.Id = fixture.Definition.Id;
    EXPECT_FALSE(NativeMod::Query(nullptr, &fixture.Api, diagnostic));
    fixture.Definition.DependencyCount = 0;
    fixture.Definition.Version = "01.0.0";
    EXPECT_FALSE(NativeMod::Query(nullptr, &fixture.Api, diagnostic));
    fixture.Definition.Version = "1.0.0";
    fixture.Api.Header.MajorVersion = 2;
    EXPECT_FALSE(NativeMod::Query(nullptr, &fixture.Api, diagnostic));
    EXPECT_EQ(creates, 0);
}
} // namespace

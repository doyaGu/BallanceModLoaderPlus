#include <gtest/gtest.h>

#include <memory>

#include "NativeModRegistry.h"

namespace {
std::shared_ptr<void> BorrowedHandle(void *handle) {
    return std::shared_ptr<void>(handle, [](void *) {});
}
}

TEST(NativeModRegistryTest, KeepsOneHandleAndIndexesModsById) {
    int module = 0;
    const std::shared_ptr<void> handle = BorrowedHandle(&module);
    NativeModRegistry registry;

    ASSERT_TRUE(registry.Add(handle, "first"));
    ASSERT_TRUE(registry.Add(handle, "second"));

    EXPECT_EQ(registry.FindDllForMod("first").get(), &module);
    EXPECT_EQ(registry.FindDllForMod("second").get(), &module);
    EXPECT_TRUE(registry.Owns(&module, "first"));
    EXPECT_TRUE(registry.Owns(&module, "second"));
    EXPECT_TRUE(registry.GetUniqueModId(&module).empty());

    const std::vector<std::string> ids = registry.SnapshotMods(&module);
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], "first");
    EXPECT_EQ(ids[1], "second");

    EXPECT_TRUE(registry.Remove("first"));
    EXPECT_FALSE(registry.FindDllForMod("first"));
    EXPECT_EQ(registry.GetUniqueModId(&module), "second");

    EXPECT_TRUE(registry.Remove("second"));
    EXPECT_TRUE(registry.SnapshotMods(&module).empty());
    EXPECT_FALSE(registry.Remove("second"));
}

TEST(NativeModRegistryTest, RejectsDuplicateIdsAcrossDlls) {
    int firstModule = 0;
    int secondModule = 0;
    NativeModRegistry registry;

    ASSERT_TRUE(registry.Add(BorrowedHandle(&firstModule), "shared.id"));
    EXPECT_FALSE(registry.Add(BorrowedHandle(&secondModule), "shared.id"));
    EXPECT_TRUE(registry.Owns(&firstModule, "shared.id"));
    EXPECT_FALSE(registry.Owns(&secondModule, "shared.id"));
}

TEST(NativeModRegistryTest, KeepsTheFirstOwnerForTheSameRawHandle) {
    int module = 0;
    int firstReleases = 0;
    int secondReleases = 0;
    std::shared_ptr<void> firstOwner(&module, [&firstReleases](void *) { ++firstReleases; });
    std::shared_ptr<void> secondOwner(&module, [&secondReleases](void *) { ++secondReleases; });
    NativeModRegistry registry;

    ASSERT_TRUE(registry.Add(firstOwner, "first"));
    ASSERT_TRUE(registry.Add(secondOwner, "second"));

    firstOwner.reset();
    secondOwner.reset();
    EXPECT_EQ(firstReleases, 0);
    EXPECT_EQ(secondReleases, 1);

    EXPECT_TRUE(registry.Remove("first"));
    EXPECT_EQ(firstReleases, 0);
    EXPECT_TRUE(registry.Remove("second"));
    EXPECT_EQ(firstReleases, 1);
}

TEST(NativeModRegistryTest, ReleasesDllOwnershipAfterTheLastMod) {
    int releases = 0;
    std::shared_ptr<void> handle(new int(1), [&releases](void *value) {
        ++releases;
        delete static_cast<int *>(value);
    });
    NativeModRegistry registry;

    ASSERT_TRUE(registry.Add(handle, "first"));
    ASSERT_TRUE(registry.Add(handle, "second"));
    handle.reset();

    EXPECT_TRUE(registry.Remove("first"));
    EXPECT_EQ(releases, 0);
    EXPECT_TRUE(registry.Remove("second"));
    EXPECT_EQ(releases, 1);
}

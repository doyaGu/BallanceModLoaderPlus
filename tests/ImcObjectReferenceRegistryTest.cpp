#include "ImcObjectReferenceRegistry.h"

#include <gtest/gtest.h>

namespace {

TEST(ImcObjectReferenceRegistryTest, DeletionInvalidatesAReusedSlotEvenAtTheSameAddress) {
    ImcObjectReferenceRegistry references;
    int storage = 0;

    const BML_ObjectRef first = references.Make(1, 42, &storage);
    ASSERT_NE(0u, first.Generation);
    EXPECT_TRUE(references.Matches(42, first.Generation, &storage));

    references.Invalidate(42);
    EXPECT_FALSE(references.Matches(42, first.Generation, &storage));

    const BML_ObjectRef replacement = references.Make(1, 42, &storage);
    EXPECT_NE(first.Generation, replacement.Generation);
    EXPECT_FALSE(references.Matches(42, first.Generation, &storage));
    EXPECT_TRUE(references.Matches(42, replacement.Generation, &storage));
}

TEST(ImcObjectReferenceRegistryTest, ResetInvalidatesEveryTrackedReference) {
    ImcObjectReferenceRegistry references;
    int firstStorage = 0;
    int secondStorage = 0;
    const BML_ObjectRef first = references.Make(1, 1, &firstStorage);
    const BML_ObjectRef second = references.Make(1, 2, &secondStorage);

    references.InvalidateAll();

    EXPECT_FALSE(references.Matches(first.Slot, first.Generation, &firstStorage));
    EXPECT_FALSE(references.Matches(second.Slot, second.Generation, &secondStorage));
    EXPECT_EQ(0u, references.Size());
}

TEST(ImcObjectReferenceRegistryTest, DeletionReleasesBookkeepingWithoutRevalidatingStaleRefs) {
    ImcObjectReferenceRegistry references;
    int storage = 0;
    const BML_ObjectRef first = references.Make(1, 42, &storage);
    ASSERT_EQ(1u, references.Size());

    references.Invalidate(42);
    EXPECT_EQ(0u, references.Size());
    const BML_ObjectRef replacement = references.Make(1, 42, &storage);
    EXPECT_NE(first.Generation, replacement.Generation);
    EXPECT_FALSE(references.Matches(first.Slot, first.Generation, &storage));
}

} // namespace

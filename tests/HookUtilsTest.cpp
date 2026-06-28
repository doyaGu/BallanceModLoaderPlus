#include "HookUtils.h"

#include <limits>

#include <gtest/gtest.h>

TEST(HookUtilsTest, ComputesVTableRegionSizeThroughHighestSlot) {
    const size_t slots[] = {2, 5, 1};
    size_t regionSize = 0;

    EXPECT_TRUE(utils::TryGetVTableRegionSize(slots, 3, &regionSize));
    EXPECT_EQ(regionSize, 6u * sizeof(void *));
}

TEST(HookUtilsTest, RejectsInvalidVTableRegionInputs) {
    const size_t slots[] = {0};
    size_t regionSize = 123;

    EXPECT_FALSE(utils::TryGetVTableRegionSize(nullptr, 1, &regionSize));
    EXPECT_EQ(regionSize, 0u);
    regionSize = 123;
    EXPECT_FALSE(utils::TryGetVTableRegionSize(slots, 0, &regionSize));
    EXPECT_EQ(regionSize, 0u);
    EXPECT_FALSE(utils::TryGetVTableRegionSize(slots, 1, nullptr));
}

TEST(HookUtilsTest, RejectsOverflowingVTableRegionSize) {
    const size_t maxSlot = (std::numeric_limits<size_t>::max)();
    const size_t firstOverflowingSlot = (std::numeric_limits<size_t>::max)() / sizeof(void *);
    const size_t slots[] = {0, maxSlot};
    const size_t multiplyOverflowSlots[] = {firstOverflowingSlot};
    size_t regionSize = 123;

    EXPECT_FALSE(utils::TryGetVTableRegionSize(slots, 2, &regionSize));
    EXPECT_EQ(regionSize, 0u);
    regionSize = 123;
    EXPECT_FALSE(utils::TryGetVTableRegionSize(multiplyOverflowSlots, 1, &regionSize));
    EXPECT_EQ(regionSize, 0u);
}

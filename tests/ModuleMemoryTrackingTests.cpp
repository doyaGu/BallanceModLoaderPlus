#include <gtest/gtest.h>

#include "Core/MemoryManager.h"
#include "TestKernel.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {
using BML::Core::Testing::TestKernel;

class ModuleMemoryTrackingTest : public ::testing::Test {
protected:
    TestKernel kernel_;

    void SetUp() override {
        kernel_->memory = std::make_unique<BML::Core::MemoryManager>();
        kernel_->memory->SetTrackingEnabled(true);
#if defined(BML_TEST)
        kernel_->memory->ResetStatsForTesting();
#endif
    }
};

TEST_F(ModuleMemoryTrackingTest, RangeLookupFindsModule) {
    auto &mgr = *kernel_->memory;

    // Simulate module ranges
    std::vector<BML::Core::ModuleRange> ranges;
    ranges.push_back({0x10000000, 0x10010000, "mod.a", 0});
    ranges.push_back({0x20000000, 0x20020000, "mod.b", 1});
    ranges.push_back({0x30000000, 0x30005000, "mod.c", 2});
    mgr.SetModuleRanges(std::move(ranges));

    EXPECT_EQ(mgr.FindModuleIndex(reinterpret_cast<void *>(0x10000500)), 0);
    EXPECT_EQ(mgr.FindModuleIndex(reinterpret_cast<void *>(0x2000FFFF)), 1);
    EXPECT_EQ(mgr.FindModuleIndex(reinterpret_cast<void *>(0x30000000)), 2);
    // Outside all ranges
    EXPECT_EQ(mgr.FindModuleIndex(reinterpret_cast<void *>(0x40000000)), -1);
    EXPECT_EQ(mgr.FindModuleIndex(reinterpret_cast<void *>(0x00000001)), -1);
}

TEST_F(ModuleMemoryTrackingTest, TrackedAllocAndFreeAttributeCorrectly) {
    auto &mgr = *kernel_->memory;

    std::vector<BML::Core::ModuleRange> ranges;
    ranges.push_back({0x10000000, 0x10010000, "mod.test", 0});
    mgr.SetModuleRanges(std::move(ranges));
    mgr.EnablePerModuleTracking(true);

    void *ptr = mgr.AllocTracked(256, reinterpret_cast<void *>(0x10000100));
    ASSERT_NE(ptr, nullptr);

    auto stats = mgr.GetPerModuleStats();
    ASSERT_GE(stats.size(), 1u);
    // Find "mod.test" entry
    auto it = std::find_if(stats.begin(), stats.end(),
        [](const auto &s) { return s.module_id == "mod.test"; });
    ASSERT_NE(it, stats.end());
    EXPECT_EQ(it->total_allocated, 256u);
    EXPECT_EQ(it->active_alloc_count, 1u);

    mgr.FreeTracked(ptr, reinterpret_cast<void *>(0x10000200));
    stats = mgr.GetPerModuleStats();
    it = std::find_if(stats.begin(), stats.end(),
        [](const auto &s) { return s.module_id == "mod.test"; });
    ASSERT_NE(it, stats.end());
    EXPECT_EQ(it->total_allocated, 0u);
    EXPECT_EQ(it->active_alloc_count, 0u);
    EXPECT_GE(it->peak_allocated, 256u);
}

TEST_F(ModuleMemoryTrackingTest, UnknownCallerAttributedToCore) {
    auto &mgr = *kernel_->memory;

    std::vector<BML::Core::ModuleRange> ranges;
    ranges.push_back({0x10000000, 0x10010000, "mod.x", 0});
    mgr.SetModuleRanges(std::move(ranges));
    mgr.EnablePerModuleTracking(true);

    // Caller outside any range
    void *ptr = mgr.AllocTracked(64, reinterpret_cast<void *>(0x99999999));
    ASSERT_NE(ptr, nullptr);

    auto stats = mgr.GetPerModuleStats();
    // Should have a "<core>" entry
    bool found_core = false;
    for (const auto &s : stats) {
        if (s.module_id == "<core>") {
            found_core = true;
            EXPECT_EQ(s.total_allocated, 64u);
        }
    }
    EXPECT_TRUE(found_core);

    mgr.FreeTracked(ptr, reinterpret_cast<void *>(0x99999999));
}

} // namespace

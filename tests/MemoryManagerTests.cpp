#include "gtest/gtest.h"

#include "Core/MemoryManager.h"

#include "bml_memory.h"

#include <array>
#include <cstdint>
#include <vector>

namespace {
class MemoryManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto &manager = BML::Core::MemoryManager::Instance();
        manager.SetTrackingEnabled(true);
    #if defined(BML_TEST)
        manager.ResetStatsForTesting();
    #endif
    }
};
}

TEST_F(MemoryManagerTest, TracksAllocAndFreeBytes) {
    auto &manager = BML::Core::MemoryManager::Instance();

    void *block = manager.Alloc(256);
    ASSERT_NE(block, nullptr);

    BML_MemoryStats stats{};
    ASSERT_EQ(manager.GetStats(&stats), BML_RESULT_OK);
    EXPECT_EQ(stats.total_allocated, 256u);
    EXPECT_EQ(stats.active_alloc_count, 1u);

    manager.Free(block);
    ASSERT_EQ(manager.GetStats(&stats), BML_RESULT_OK);
    EXPECT_EQ(stats.total_allocated, 0u);
    EXPECT_EQ(stats.active_alloc_count, 0u);
    EXPECT_GE(stats.peak_allocated, 256u);
}

TEST_F(MemoryManagerTest, AllocatorSmokeTest) {
    auto &manager = BML::Core::MemoryManager::Instance();

    void *alloc_block = manager.Alloc(128);
    ASSERT_NE(alloc_block, nullptr);

    void *calloc_block = manager.Calloc(4, 32);
    ASSERT_NE(calloc_block, nullptr);

    void *realloc_block = manager.Realloc(alloc_block, 128, 256);
    ASSERT_NE(realloc_block, nullptr);

    void *aligned_block = manager.AllocAligned(64, 64);
    ASSERT_NE(aligned_block, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(aligned_block) % 64u, 0u);

    manager.FreeWithSize(calloc_block, 4 * 32);
    manager.Free(realloc_block);
    manager.FreeAligned(aligned_block);

    BML_MemoryStats stats{};
    ASSERT_EQ(manager.GetStats(&stats), BML_RESULT_OK);
    EXPECT_EQ(stats.total_allocated, 0u);
    EXPECT_EQ(stats.active_alloc_count, 0u);
    EXPECT_GE(stats.peak_allocated, 256u);
}

TEST_F(MemoryManagerTest, ReallocPreservesDataAndStats) {
    auto &manager = BML::Core::MemoryManager::Instance();

    auto *bytes = static_cast<uint8_t *>(manager.Alloc(32));
    ASSERT_NE(bytes, nullptr);
    for (uint8_t i = 0; i < 32; ++i) {
        bytes[i] = i;
    }

    bytes = static_cast<uint8_t *>(manager.ReallocUnknownSize(bytes, 128));
    ASSERT_NE(bytes, nullptr);
    for (uint8_t i = 0; i < 32; ++i) {
        EXPECT_EQ(bytes[i], i);
    }

    bytes = static_cast<uint8_t *>(manager.Realloc(bytes, 128, 16));
    ASSERT_NE(bytes, nullptr);
    for (uint8_t i = 0; i < 16; ++i) {
        EXPECT_EQ(bytes[i], i);
    }

    BML_MemoryStats stats{};
    ASSERT_EQ(manager.GetStats(&stats), BML_RESULT_OK);
    EXPECT_EQ(stats.total_allocated, 16u);
    EXPECT_EQ(stats.active_alloc_count, 1u);

    manager.Free(bytes);
    ASSERT_EQ(manager.GetStats(&stats), BML_RESULT_OK);
    EXPECT_EQ(stats.total_allocated, 0u);
    EXPECT_EQ(stats.active_alloc_count, 0u);
}

TEST_F(MemoryManagerTest, AllocAlignedRespectsAlignment) {
    auto &manager = BML::Core::MemoryManager::Instance();

    std::array<size_t, 4> alignments{8u, 32u, 256u, 1024u};
    std::vector<void *> blocks;
    blocks.reserve(alignments.size());

    for (size_t alignment : alignments) {
        void *ptr = manager.AllocAligned(64, alignment);
        ASSERT_NE(ptr, nullptr);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignment, 0u);
        blocks.push_back(ptr);
    }

    for (void *ptr : blocks) {
        manager.FreeAligned(ptr);
    }

    BML_MemoryStats stats{};
    ASSERT_EQ(manager.GetStats(&stats), BML_RESULT_OK);
    EXPECT_EQ(stats.total_allocated, 0u);
    EXPECT_EQ(stats.active_alloc_count, 0u);
}

TEST_F(MemoryManagerTest, AllocAlignedRejectsNonPowerOfTwoAlignment) {
    auto &manager = BML::Core::MemoryManager::Instance();
    EXPECT_EQ(manager.AllocAligned(32, 12), nullptr);
    EXPECT_EQ(manager.AllocAligned(32, 0), nullptr);
}

TEST_F(MemoryManagerTest, MemoryPoolAllocatesAndDestroys) {
    auto &manager = BML::Core::MemoryManager::Instance();

    BML_MemoryPool pool = nullptr;
    ASSERT_EQ(manager.CreatePool(64, 8, &pool), BML_RESULT_OK);

    const size_t allocation_count = 40;
    std::vector<void *> blocks;
    blocks.reserve(allocation_count);
    for (size_t i = 0; i < allocation_count; ++i) {
        void *ptr = manager.PoolAlloc(pool);
        ASSERT_NE(ptr, nullptr);
        blocks.push_back(ptr);
    }

    for (void *ptr : blocks) {
        manager.PoolFree(pool, ptr);
    }

    manager.DestroyPool(pool);
    EXPECT_EQ(manager.PoolAlloc(pool), nullptr);
}

TEST_F(MemoryManagerTest, CreatePoolRejectsInvalidParameters) {
    auto &manager = BML::Core::MemoryManager::Instance();
    BML_MemoryPool pool = nullptr;

    EXPECT_EQ(manager.CreatePool(4, 16, &pool), BML_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(manager.CreatePool(2 * 1024 * 1024, 16, &pool), BML_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(manager.CreatePool(64, 0, nullptr), BML_RESULT_INVALID_ARGUMENT);
}

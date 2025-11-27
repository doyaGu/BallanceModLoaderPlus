/**
 * @file MpscRingBufferTests.cpp
 * @brief Comprehensive tests for MpscRingBuffer (Multi-Producer Single-Consumer)
 * 
 * Tests cover:
 * - Basic enqueue/dequeue operations
 * - Full buffer handling
 * - Concurrent producer access
 * - Memory ordering correctness
 * - Edge cases
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <algorithm>
#include <atomic>
#include <barrier>
#include <chrono>
#include <numeric>
#include <set>
#include <thread>
#include <vector>

#include "Core/MpscRingBuffer.h"

using BML::Core::MpscRingBuffer;

namespace {

class MpscRingBufferTests : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(MpscRingBufferTests, ConstructWithCapacity) {
    MpscRingBuffer<int> buffer(128);
    EXPECT_TRUE(buffer.IsEmpty());
    EXPECT_GE(buffer.Capacity(), 128u);
}

TEST_F(MpscRingBufferTests, ConstructWithSmallCapacity) {
    MpscRingBuffer<int> buffer(2);
    EXPECT_TRUE(buffer.IsEmpty());
    EXPECT_GE(buffer.Capacity(), 2u);
}

TEST_F(MpscRingBufferTests, CapacityRoundedToPowerOfTwo) {
    // Capacity should be rounded up to power of two for efficiency
    MpscRingBuffer<int> buffer(100);
    size_t capacity = buffer.Capacity();
    EXPECT_GE(capacity, 100u);
    // Verify power of two
    EXPECT_EQ(capacity & (capacity - 1), 0u) << "Capacity " << capacity << " is not power of two";
}

// ============================================================================
// Basic Operations Tests
// ============================================================================

TEST_F(MpscRingBufferTests, EnqueueAndDequeue) {
    MpscRingBuffer<int> buffer(16);
    
    EXPECT_TRUE(buffer.Enqueue(42));
    EXPECT_FALSE(buffer.IsEmpty());
    EXPECT_EQ(buffer.ApproximateSize(), 1u);
    
    int value = 0;
    EXPECT_TRUE(buffer.Dequeue(value));
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(buffer.IsEmpty());
}

TEST_F(MpscRingBufferTests, MultipleEnqueueDequeue) {
    MpscRingBuffer<int> buffer(32);
    
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(buffer.Enqueue(i));
    }
    
    EXPECT_EQ(buffer.ApproximateSize(), 10u);
    
    for (int i = 0; i < 10; ++i) {
        int value = -1;
        EXPECT_TRUE(buffer.Dequeue(value));
        EXPECT_EQ(value, i);
    }
    
    EXPECT_TRUE(buffer.IsEmpty());
}

TEST_F(MpscRingBufferTests, DequeueFromEmptyReturnsFalse) {
    MpscRingBuffer<int> buffer(16);
    
    int value = 999;
    EXPECT_FALSE(buffer.Dequeue(value));
    EXPECT_EQ(value, 999);  // Value should be unchanged
}

TEST_F(MpscRingBufferTests, EnqueueToFullReturnsFalse) {
    MpscRingBuffer<int> buffer(4);  // Small buffer
    size_t capacity = buffer.Capacity();
    
    // Fill the buffer
    for (size_t i = 0; i < capacity; ++i) {
        EXPECT_TRUE(buffer.Enqueue(static_cast<int>(i))) << "Failed at " << i;
    }
    
    // Next enqueue should fail
    EXPECT_FALSE(buffer.Enqueue(999));
}

TEST_F(MpscRingBufferTests, FifoOrdering) {
    MpscRingBuffer<int> buffer(64);
    
    std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    for (int v : input) {
        EXPECT_TRUE(buffer.Enqueue(v));
    }
    
    std::vector<int> output;
    int value;
    while (buffer.Dequeue(value)) {
        output.push_back(value);
    }
    
    EXPECT_EQ(output, input);
}

// ============================================================================
// Wraparound Tests
// ============================================================================

TEST_F(MpscRingBufferTests, WraparoundBehavior) {
    MpscRingBuffer<int> buffer(8);
    size_t capacity = buffer.Capacity();
    
    // Fill and drain multiple times to test wraparound
    for (int round = 0; round < 10; ++round) {
        // Fill partially
        for (size_t i = 0; i < capacity / 2; ++i) {
            EXPECT_TRUE(buffer.Enqueue(round * 100 + static_cast<int>(i)));
        }
        
        // Drain all
        int value;
        size_t count = 0;
        while (buffer.Dequeue(value)) {
            EXPECT_EQ(value, round * 100 + static_cast<int>(count));
            ++count;
        }
        
        EXPECT_EQ(count, capacity / 2);
    }
}

// ============================================================================
// Pointer Type Tests
// ============================================================================

TEST_F(MpscRingBufferTests, PointerTypeEnqueueDequeue) {
    struct TestData {
        int value;
        TestData(int v) : value(v) {}
    };
    
    MpscRingBuffer<TestData*> buffer(16);
    
    auto* data1 = new TestData(100);
    auto* data2 = new TestData(200);
    
    EXPECT_TRUE(buffer.Enqueue(data1));
    EXPECT_TRUE(buffer.Enqueue(data2));
    
    TestData* result = nullptr;
    EXPECT_TRUE(buffer.Dequeue(result));
    EXPECT_EQ(result, data1);
    EXPECT_EQ(result->value, 100);
    
    EXPECT_TRUE(buffer.Dequeue(result));
    EXPECT_EQ(result, data2);
    EXPECT_EQ(result->value, 200);
    
    delete data1;
    delete data2;
}

// ============================================================================
// Concurrent Tests - Multiple Producers, Single Consumer
// ============================================================================

TEST_F(MpscRingBufferTests, MultiProducerSingleConsumer) {
    constexpr size_t kNumProducers = 4;
    constexpr size_t kItemsPerProducer = 1000;
    constexpr size_t kTotalItems = kNumProducers * kItemsPerProducer;
    
    MpscRingBuffer<int> buffer(kTotalItems * 2);  // Large enough to not block
    
    std::barrier start(static_cast<std::ptrdiff_t>(kNumProducers + 2));  // producers + consumer + main
    std::atomic<bool> producers_done{false};
    std::atomic<size_t> produced_count{0};
    
    std::vector<std::thread> producers;
    producers.reserve(kNumProducers);
    
    // Producer threads
    for (size_t p = 0; p < kNumProducers; ++p) {
        producers.emplace_back([&, producer_id = p] {
            start.arrive_and_wait();
            
            for (size_t i = 0; i < kItemsPerProducer; ++i) {
                // Encode producer_id and sequence number
                int value = static_cast<int>(producer_id * 10000 + i);
                while (!buffer.Enqueue(value)) {
                    std::this_thread::yield();
                }
                produced_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    
    // Consumer thread
    std::set<int> consumed_values;
    std::thread consumer([&] {
        start.arrive_and_wait();
        
        while (!producers_done.load(std::memory_order_acquire) || !buffer.IsEmpty()) {
            int value;
            if (buffer.Dequeue(value)) {
                consumed_values.insert(value);
            } else {
                std::this_thread::yield();
            }
        }
        
        // Drain any remaining
        int value;
        while (buffer.Dequeue(value)) {
            consumed_values.insert(value);
        }
    });
    
    start.arrive_and_wait();
    
    // Wait for producers
    for (auto& t : producers) {
        t.join();
    }
    producers_done.store(true, std::memory_order_release);
    
    consumer.join();
    
    EXPECT_EQ(produced_count.load(), kTotalItems);
    EXPECT_EQ(consumed_values.size(), kTotalItems);
    
    // Verify all expected values are present
    for (size_t p = 0; p < kNumProducers; ++p) {
        for (size_t i = 0; i < kItemsPerProducer; ++i) {
            int expected = static_cast<int>(p * 10000 + i);
            EXPECT_TRUE(consumed_values.count(expected) == 1) 
                << "Missing value: " << expected;
        }
    }
}

TEST_F(MpscRingBufferTests, ConcurrentProducersWithBackpressure) {
    constexpr size_t kNumProducers = 8;
    constexpr size_t kItemsPerProducer = 500;
    constexpr size_t kBufferSize = 64;  // Small buffer to cause backpressure
    
    MpscRingBuffer<int> buffer(kBufferSize);
    
    std::barrier start(static_cast<std::ptrdiff_t>(kNumProducers + 2));
    std::atomic<bool> stop{false};
    std::atomic<size_t> total_produced{0};
    std::atomic<size_t> total_consumed{0};
    std::atomic<size_t> enqueue_retries{0};
    
    std::vector<std::thread> producers;
    producers.reserve(kNumProducers);
    
    // Producers
    for (size_t p = 0; p < kNumProducers; ++p) {
        producers.emplace_back([&] {
            start.arrive_and_wait();
            
            for (size_t i = 0; i < kItemsPerProducer; ++i) {
                while (!buffer.Enqueue(1)) {
                    enqueue_retries.fetch_add(1, std::memory_order_relaxed);
                    std::this_thread::yield();
                }
                total_produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    
    // Consumer
    std::thread consumer([&] {
        start.arrive_and_wait();
        
        while (!stop.load(std::memory_order_acquire)) {
            int value;
            if (buffer.Dequeue(value)) {
                total_consumed.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
        
        // Drain remaining
        int value;
        while (buffer.Dequeue(value)) {
            total_consumed.fetch_add(1, std::memory_order_relaxed);
        }
    });
    
    start.arrive_and_wait();
    
    for (auto& t : producers) {
        t.join();
    }
    stop.store(true, std::memory_order_release);
    consumer.join();
    
    EXPECT_EQ(total_produced.load(), kNumProducers * kItemsPerProducer);
    EXPECT_EQ(total_consumed.load(), total_produced.load());
    EXPECT_GT(enqueue_retries.load(), 0u) << "Expected some backpressure";
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(MpscRingBufferTests, HighThroughputStress) {
    constexpr size_t kNumProducers = 4;
    constexpr auto kDuration = std::chrono::milliseconds(100);
    
    MpscRingBuffer<uint64_t> buffer(4096);
    
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> total_produced{0};
    std::atomic<uint64_t> total_consumed{0};
    std::atomic<size_t> producers_done{0};
    
    std::vector<std::thread> producers;
    producers.reserve(kNumProducers);
    
    for (size_t p = 0; p < kNumProducers; ++p) {
        producers.emplace_back([&] {
            uint64_t local_count = 0;
            while (!stop.load(std::memory_order_acquire)) {
                if (buffer.Enqueue(local_count)) {
                    ++local_count;
                }
            }
            // Report produced count before signaling done
            total_produced.fetch_add(local_count, std::memory_order_release);
            producers_done.fetch_add(1, std::memory_order_release);
        });
    }
    
    std::thread consumer([&] {
        uint64_t local_count = 0;
        // Keep consuming until all producers are done AND buffer is empty
        // We need to recheck IsEmpty() after seeing all producers done
        // because there might be items in flight
        while (true) {
            uint64_t value;
            if (buffer.Dequeue(value)) {
                ++local_count;
            } else {
                // Buffer is empty - check if we should exit
                if (producers_done.load(std::memory_order_acquire) == kNumProducers) {
                    // All producers done, drain any remaining items
                    while (buffer.Dequeue(value)) {
                        ++local_count;
                    }
                    break;
                }
                // Producers still running, yield and retry
                std::this_thread::yield();
            }
        }
        total_consumed.store(local_count, std::memory_order_release);
    });
    
    std::this_thread::sleep_for(kDuration);
    stop.store(true, std::memory_order_release);
    
    for (auto& t : producers) {
        t.join();
    }
    consumer.join();
    
    EXPECT_EQ(total_consumed.load(), total_produced.load());
    EXPECT_GT(total_produced.load(), 0u);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(MpscRingBufferTests, MinimalCapacityBuffer) {
    // Note: Capacity 1 is normalized to 2 (power of two)
    MpscRingBuffer<int> buffer(2);
    size_t capacity = buffer.Capacity();
    
    // Fill it
    for (size_t i = 0; i < capacity; ++i) {
        EXPECT_TRUE(buffer.Enqueue(static_cast<int>(i + 42)));
    }
    
    // Should be full now
    EXPECT_FALSE(buffer.Enqueue(999));
    
    // Drain and verify order
    for (size_t i = 0; i < capacity; ++i) {
        int value;
        EXPECT_TRUE(buffer.Dequeue(value));
        EXPECT_EQ(value, static_cast<int>(i + 42));
    }
}

TEST_F(MpscRingBufferTests, LargeItems) {
    struct LargeStruct {
        char data[1024];
        int id;
    };
    
    MpscRingBuffer<LargeStruct> buffer(16);
    
    LargeStruct item{};
    item.id = 12345;
    std::memset(item.data, 'X', sizeof(item.data));
    
    EXPECT_TRUE(buffer.Enqueue(item));
    
    LargeStruct result{};
    EXPECT_TRUE(buffer.Dequeue(result));
    EXPECT_EQ(result.id, 12345);
    EXPECT_EQ(result.data[0], 'X');
}

TEST_F(MpscRingBufferTests, ApproximateSizeIsApproximate) {
    MpscRingBuffer<int> buffer(64);
    
    // Empty
    EXPECT_EQ(buffer.ApproximateSize(), 0u);
    
    // Add some items
    for (int i = 0; i < 10; ++i) {
        buffer.Enqueue(i);
    }
    
    // Size should be approximately 10 (may vary slightly due to concurrent nature)
    size_t size = buffer.ApproximateSize();
    EXPECT_GE(size, 8u);
    EXPECT_LE(size, 12u);
}

TEST_F(MpscRingBufferTests, IsEmptyReflectsState) {
    MpscRingBuffer<int> buffer(16);
    
    EXPECT_TRUE(buffer.IsEmpty());
    
    buffer.Enqueue(1);
    EXPECT_FALSE(buffer.IsEmpty());
    
    int value;
    buffer.Dequeue(value);
    EXPECT_TRUE(buffer.IsEmpty());
}

} // namespace

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <numeric>
#include <thread>
#include <vector>

#include "ConcurrentRingBuffer.h"

using devtools::ConcurrentRingBuffer;

namespace {

TEST(ConcurrentRingBufferTest, PushAndDrainSingle) {
    ConcurrentRingBuffer<int, 16> buf;
    EXPECT_TRUE(buf.TryPush(42));

    int out[16]{};
    size_t n = buf.Drain(out, 16);
    ASSERT_EQ(n, 1u);
    EXPECT_EQ(out[0], 42);
}

TEST(ConcurrentRingBufferTest, DrainEmptyReturnsZero) {
    ConcurrentRingBuffer<int, 16> buf;
    int out[4]{};
    EXPECT_EQ(buf.Drain(out, 4), 0u);
}

TEST(ConcurrentRingBufferTest, FillToCapacity) {
    ConcurrentRingBuffer<int, 8> buf;
    for (int i = 0; i < 8; ++i)
        EXPECT_TRUE(buf.TryPush(i));
    EXPECT_FALSE(buf.TryPush(99));
}

TEST(ConcurrentRingBufferTest, WrapAround) {
    ConcurrentRingBuffer<int, 4> buf;
    for (int round = 0; round < 3; ++round) {
        for (int i = 0; i < 4; ++i)
            EXPECT_TRUE(buf.TryPush(round * 10 + i));
        int out[4]{};
        size_t n = buf.Drain(out, 4);
        ASSERT_EQ(n, 4u);
        for (int i = 0; i < 4; ++i)
            EXPECT_EQ(out[i], round * 10 + i);
    }
}

TEST(ConcurrentRingBufferTest, DrainPartial) {
    ConcurrentRingBuffer<int, 16> buf;
    for (int i = 0; i < 8; ++i)
        buf.TryPush(i);

    int out[3]{};
    size_t n = buf.Drain(out, 3);
    ASSERT_EQ(n, 3u);
    EXPECT_EQ(out[0], 0);
    EXPECT_EQ(out[1], 1);
    EXPECT_EQ(out[2], 2);

    int out2[16]{};
    n = buf.Drain(out2, 16);
    ASSERT_EQ(n, 5u);
    EXPECT_EQ(out2[0], 3);
}

TEST(ConcurrentRingBufferTest, ConcurrentProducersSingleConsumer) {
    constexpr size_t kBufSize = 4096;
    constexpr int kThreads = 4;
    constexpr int kPerThread = 500;

    ConcurrentRingBuffer<int, kBufSize> buf;
    std::atomic<bool> go{false};

    auto producer = [&](int base) {
        while (!go.load(std::memory_order_acquire)) {}
        for (int i = 0; i < kPerThread; ++i) {
            while (!buf.TryPush(base + i)) {
                std::this_thread::yield();
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t)
        threads.emplace_back(producer, t * kPerThread);

    go.store(true, std::memory_order_release);

    std::vector<int> collected;
    collected.reserve(kThreads * kPerThread);
    int out[256];
    while (collected.size() < static_cast<size_t>(kThreads * kPerThread)) {
        size_t n = buf.Drain(out, 256);
        for (size_t i = 0; i < n; ++i)
            collected.push_back(out[i]);
        if (n == 0) std::this_thread::yield();
    }

    for (auto &t : threads) t.join();

    std::sort(collected.begin(), collected.end());
    std::vector<int> expected(kThreads * kPerThread);
    std::iota(expected.begin(), expected.end(), 0);
    EXPECT_EQ(collected, expected);
}

} // namespace

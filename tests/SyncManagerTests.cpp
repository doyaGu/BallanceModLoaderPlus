/**
 * @file SyncManagerTests.cpp
 * @brief Comprehensive tests for SyncManager synchronization primitives
 * 
 * Tests cover:
 * - Mutex creation, locking, unlocking
 * - RwLock read/write locking
 * - Semaphore acquire/release
 * - Condition variable wait/signal
 * - SpinLock operations
 * - TLS key management
 * - Concurrent access patterns
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <atomic>
#include <barrier>
#include <thread>
#include <vector>
#include <chrono>

#include "Core/SyncManager.h"
#include "Core/CoreErrors.h"

using BML::Core::SyncManager;

namespace {

void ClearSyncLastError() {
    BML::Core::ClearLastErrorInfo();
}

void ExpectLastErrorCode(BML_Result expected) {
    BML_ErrorInfo info{};
    info.struct_size = sizeof(BML_ErrorInfo);
    ASSERT_EQ(BML::Core::GetLastErrorInfo(&info), BML_RESULT_OK);
    EXPECT_EQ(info.result_code, expected);
}

class SyncManagerTests : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Mutex Tests
// ============================================================================

TEST_F(SyncManagerTests, MutexCreateAndDestroy) {
    BML_Mutex mutex = nullptr;
    EXPECT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateMutex(&mutex));
    EXPECT_NE(mutex, nullptr);
    SyncManager::Instance().DestroyMutex(mutex);
}

TEST_F(SyncManagerTests, MutexLockUnlock) {
    BML_Mutex mutex = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateMutex(&mutex));
    
    SyncManager::Instance().LockMutex(mutex);
    SyncManager::Instance().UnlockMutex(mutex);
    
    SyncManager::Instance().DestroyMutex(mutex);
}

TEST_F(SyncManagerTests, MutexTryLock) {
    BML_Mutex mutex = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateMutex(&mutex));
    
    // Should succeed when unlocked
    EXPECT_TRUE(SyncManager::Instance().TryLockMutex(mutex));
    SyncManager::Instance().UnlockMutex(mutex);
    
    SyncManager::Instance().DestroyMutex(mutex);
}

TEST_F(SyncManagerTests, MutexTryLockFails_WhenAlreadyLocked) {
    BML_Mutex mutex = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateMutex(&mutex));
    
    std::atomic<bool> lock_held{false};
    std::atomic<bool> try_result{true};
    
    std::thread holder([&] {
        SyncManager::Instance().LockMutex(mutex);
        lock_held.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        SyncManager::Instance().UnlockMutex(mutex);
    });
    
    // Wait for holder to acquire lock
    while (!lock_held.load()) {
        std::this_thread::yield();
    }
    
    std::thread trier([&] {
        try_result.store(SyncManager::Instance().TryLockMutex(mutex));
    });
    
    trier.join();
    holder.join();
    
    // TryLock should have failed
    EXPECT_FALSE(try_result.load());
    
    SyncManager::Instance().DestroyMutex(mutex);
}

TEST_F(SyncManagerTests, MutexConcurrentAccess) {
    BML_Mutex mutex = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateMutex(&mutex));
    
    constexpr int kThreads = 4;
    constexpr int kIterations = 1000;
    std::atomic<int> counter{0};
    int protected_counter = 0;
    
    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < kIterations; ++j) {
                SyncManager::Instance().LockMutex(mutex);
                ++protected_counter;
                ++counter;
                SyncManager::Instance().UnlockMutex(mutex);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(counter.load(), kThreads * kIterations);
    EXPECT_EQ(protected_counter, kThreads * kIterations);
    
    SyncManager::Instance().DestroyMutex(mutex);
}

// ============================================================================
// RwLock Tests
// ============================================================================

TEST_F(SyncManagerTests, RwLockCreateAndDestroy) {
    BML_RwLock lock = nullptr;
    EXPECT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateRwLock(&lock));
    EXPECT_NE(lock, nullptr);
    SyncManager::Instance().DestroyRwLock(lock);
}

TEST_F(SyncManagerTests, RwLockReadLockUnlock) {
    BML_RwLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateRwLock(&lock));
    
    SyncManager::Instance().ReadLockRwLock(lock);
    SyncManager::Instance().UnlockRwLock(lock);
    
    SyncManager::Instance().DestroyRwLock(lock);
}

TEST_F(SyncManagerTests, RwLockWriteLockUnlock) {
    BML_RwLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateRwLock(&lock));
    
    SyncManager::Instance().WriteLockRwLock(lock);
    SyncManager::Instance().UnlockRwLock(lock);
    
    SyncManager::Instance().DestroyRwLock(lock);
}

TEST_F(SyncManagerTests, RwLockMultipleReaders) {
    BML_RwLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateRwLock(&lock));
    
    constexpr int kReaders = 4;
    std::atomic<int> readers_in{0};
    std::atomic<bool> done{false};
    
    std::vector<std::thread> threads;
    for (int i = 0; i < kReaders; ++i) {
        threads.emplace_back([&] {
            SyncManager::Instance().ReadLockRwLock(lock);
            ++readers_in;
            while (!done.load()) {
                std::this_thread::yield();
            }
            --readers_in;
            SyncManager::Instance().UnlockRwLock(lock);
        });
    }
    
    // Wait for all readers to be inside
    while (readers_in.load() < kReaders) {
        std::this_thread::yield();
    }
    
    // All readers should be holding the lock simultaneously
    EXPECT_EQ(readers_in.load(), kReaders);
    
    done.store(true);
    for (auto& t : threads) {
        t.join();
    }
    
    SyncManager::Instance().DestroyRwLock(lock);
}

TEST_F(SyncManagerTests, RwLockTryReadLock) {
    BML_RwLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateRwLock(&lock));
    
    EXPECT_TRUE(SyncManager::Instance().TryReadLockRwLock(lock));
    SyncManager::Instance().UnlockRwLock(lock);
    
    SyncManager::Instance().DestroyRwLock(lock);
}

TEST_F(SyncManagerTests, RwLockTryWriteLock) {
    BML_RwLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateRwLock(&lock));
    
    EXPECT_TRUE(SyncManager::Instance().TryWriteLockRwLock(lock));
    SyncManager::Instance().UnlockRwLock(lock);
    
    SyncManager::Instance().DestroyRwLock(lock);
}

TEST_F(SyncManagerTests, RwLockWriteLockRejectsUpgradeFromRead) {
    BML_RwLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateRwLock(&lock));

    SyncManager::Instance().ReadLockRwLock(lock);
    ClearSyncLastError();
    SyncManager::Instance().WriteLockRwLock(lock);
    ExpectLastErrorCode(BML_RESULT_INVALID_ARGUMENT);
    SyncManager::Instance().ReadUnlockRwLock(lock);

    SyncManager::Instance().DestroyRwLock(lock);
}

// ============================================================================
// Semaphore Tests
// ============================================================================

TEST_F(SyncManagerTests, SemaphoreCreateAndDestroy) {
    BML_Semaphore sem = nullptr;
    EXPECT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateSemaphore(1, 10, &sem));
    EXPECT_NE(sem, nullptr);
    SyncManager::Instance().DestroySemaphore(sem);
}

TEST_F(SyncManagerTests, SemaphoreWaitAndSignal) {
    BML_Semaphore sem = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateSemaphore(1, 10, &sem));
    
    // Wait (acquire) the initial count - should succeed immediately
    EXPECT_EQ(BML_RESULT_OK, SyncManager::Instance().WaitSemaphore(sem, 0));
    
    // Signal (release) it back
    EXPECT_EQ(BML_RESULT_OK, SyncManager::Instance().SignalSemaphore(sem, 1));
    
    SyncManager::Instance().DestroySemaphore(sem);
}

TEST_F(SyncManagerTests, SemaphoreWaitTimeout) {
    BML_Semaphore sem = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateSemaphore(0, 10, &sem));
    
    // Should timeout since count is 0
    auto start = std::chrono::steady_clock::now();
    BML_Result result = SyncManager::Instance().WaitSemaphore(sem, 10);
    auto elapsed = std::chrono::steady_clock::now() - start;
    
    EXPECT_NE(result, BML_RESULT_OK);
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 5);
    
    SyncManager::Instance().DestroySemaphore(sem);
}

TEST_F(SyncManagerTests, SemaphoreSignalMultiple) {
    BML_Semaphore sem = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateSemaphore(0, 10, &sem));
    
    // Signal 3 at once
    EXPECT_EQ(BML_RESULT_OK, SyncManager::Instance().SignalSemaphore(sem, 3));
    
    // Should be able to wait 3 times
    EXPECT_EQ(BML_RESULT_OK, SyncManager::Instance().WaitSemaphore(sem, 0));
    EXPECT_EQ(BML_RESULT_OK, SyncManager::Instance().WaitSemaphore(sem, 0));
    EXPECT_EQ(BML_RESULT_OK, SyncManager::Instance().WaitSemaphore(sem, 0));
    
    // Fourth wait should timeout
    EXPECT_NE(BML_RESULT_OK, SyncManager::Instance().WaitSemaphore(sem, 0));
    
    SyncManager::Instance().DestroySemaphore(sem);
}

// ============================================================================
// Condition Variable Tests
// ============================================================================

TEST_F(SyncManagerTests, CondVarCreateAndDestroy) {
    BML_CondVar cv = nullptr;
    EXPECT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateCondVar(&cv));
    EXPECT_NE(cv, nullptr);
    SyncManager::Instance().DestroyCondVar(cv);
}

TEST_F(SyncManagerTests, CondVarSignalOne) {
    BML_Mutex mutex = nullptr;
    BML_CondVar cv = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateMutex(&mutex));
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateCondVar(&cv));
    
    std::atomic<bool> ready{false};
    std::atomic<bool> signaled{false};
    
    std::atomic<BML_Result> wait_result{BML_RESULT_UNKNOWN_ERROR};
    std::thread waiter([&] {
        SyncManager::Instance().LockMutex(mutex);
        ready.store(true);
        wait_result.store(SyncManager::Instance().WaitCondVar(cv, mutex));
        signaled.store(true);
        SyncManager::Instance().UnlockMutex(mutex);
    });
    
    // Wait for waiter to be ready
    while (!ready.load()) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Signal the condition
    SyncManager::Instance().SignalCondVar(cv);
    
    waiter.join();
    EXPECT_TRUE(signaled.load());
    EXPECT_EQ(wait_result.load(), BML_RESULT_OK);
    
    SyncManager::Instance().DestroyCondVar(cv);
    SyncManager::Instance().DestroyMutex(mutex);
}

TEST_F(SyncManagerTests, CondVarBroadcast) {
    BML_Mutex mutex = nullptr;
    BML_CondVar cv = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateMutex(&mutex));
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateCondVar(&cv));
    
    constexpr int kWaiters = 3;
    std::atomic<int> ready_count{0};
    std::atomic<int> woken_count{0};
    
    std::vector<std::thread> waiters;
    std::vector<BML_Result> wait_results(kWaiters, BML_RESULT_UNKNOWN_ERROR);
    for (int i = 0; i < kWaiters; ++i) {
        waiters.emplace_back([&, i] {
            SyncManager::Instance().LockMutex(mutex);
            ++ready_count;
            wait_results[i] = SyncManager::Instance().WaitCondVar(cv, mutex);
            ++woken_count;
            SyncManager::Instance().UnlockMutex(mutex);
        });
    }
    
    // Wait for all waiters
    while (ready_count.load() < kWaiters) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Broadcast to wake all
    SyncManager::Instance().BroadcastCondVar(cv);
    
    for (auto& t : waiters) {
        t.join();
    }
    
    EXPECT_EQ(woken_count.load(), kWaiters);
    for (auto result : wait_results) {
        EXPECT_EQ(result, BML_RESULT_OK);
    }
    
    SyncManager::Instance().DestroyCondVar(cv);
    SyncManager::Instance().DestroyMutex(mutex);
}

TEST_F(SyncManagerTests, CondVarTimedWait) {
    BML_Mutex mutex = nullptr;
    BML_CondVar cv = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateMutex(&mutex));
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateCondVar(&cv));
    
    SyncManager::Instance().LockMutex(mutex);
    
    auto start = std::chrono::steady_clock::now();
    // Wait with very short timeout - should timeout
    BML_Result result = SyncManager::Instance().WaitCondVarTimeout(cv, mutex, 10);
    auto elapsed = std::chrono::steady_clock::now() - start;
    
    SyncManager::Instance().UnlockMutex(mutex);
    
    // Should have returned timeout
    EXPECT_EQ(result, BML_RESULT_TIMEOUT);
    // Should have taken at least ~10ms
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 5);
    
    SyncManager::Instance().DestroyCondVar(cv);
    SyncManager::Instance().DestroyMutex(mutex);
}

TEST_F(SyncManagerTests, CondVarWaitRejectsInvalidHandles) {
    ClearSyncLastError();
    BML_Result result = SyncManager::Instance().WaitCondVar(
        reinterpret_cast<BML_CondVar>(0x1), reinterpret_cast<BML_Mutex>(0x2));
    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
    ExpectLastErrorCode(BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(SyncManagerTests, CondVarTimeoutPrecisionWithinBounds) {
    BML_Mutex mutex = nullptr;
    BML_CondVar cv = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateMutex(&mutex));
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateCondVar(&cv));

    SyncManager::Instance().LockMutex(mutex);
    const uint32_t timeout_ms = 25;
    auto start = std::chrono::steady_clock::now();
    auto result = SyncManager::Instance().WaitCondVarTimeout(cv, mutex, timeout_ms);
    auto elapsed = std::chrono::steady_clock::now() - start;
    SyncManager::Instance().UnlockMutex(mutex);

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    EXPECT_EQ(result, BML_RESULT_TIMEOUT);
    EXPECT_GE(elapsed_ms, static_cast<long long>(timeout_ms * 0.6));
    EXPECT_LE(elapsed_ms, static_cast<long long>(timeout_ms * 2));

    SyncManager::Instance().DestroyCondVar(cv);
    SyncManager::Instance().DestroyMutex(mutex);
}

// ============================================================================
// SpinLock Tests
// ============================================================================

TEST_F(SyncManagerTests, SpinLockCreateAndDestroy) {
    BML_SpinLock lock = nullptr;
    EXPECT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateSpinLock(&lock));
    EXPECT_NE(lock, nullptr);
    SyncManager::Instance().DestroySpinLock(lock);
}

TEST_F(SyncManagerTests, SpinLockLockUnlock) {
    BML_SpinLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateSpinLock(&lock));
    
    SyncManager::Instance().LockSpinLock(lock);
    SyncManager::Instance().UnlockSpinLock(lock);
    
    SyncManager::Instance().DestroySpinLock(lock);
}

TEST_F(SyncManagerTests, SpinLockTryLock) {
    BML_SpinLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateSpinLock(&lock));
    
    EXPECT_TRUE(SyncManager::Instance().TryLockSpinLock(lock));
    SyncManager::Instance().UnlockSpinLock(lock);
    
    SyncManager::Instance().DestroySpinLock(lock);
}

TEST_F(SyncManagerTests, SpinLockConcurrentAccess) {
    BML_SpinLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateSpinLock(&lock));
    
    constexpr int kThreads = 4;
    constexpr int kIterations = 10000;
    int counter = 0;
    
    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < kIterations; ++j) {
                SyncManager::Instance().LockSpinLock(lock);
                ++counter;
                SyncManager::Instance().UnlockSpinLock(lock);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(counter, kThreads * kIterations);
    
    SyncManager::Instance().DestroySpinLock(lock);
}

TEST_F(SyncManagerTests, SpinLockFairnessPreventsStarvation) {
    BML_SpinLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateSpinLock(&lock));

    std::atomic<bool> waiter_acquired{false};

    std::thread hog([&] {
        for (int i = 0; i < 50000; ++i) {
            SyncManager::Instance().LockSpinLock(lock);
            SyncManager::Instance().UnlockSpinLock(lock);
        }
    });

    std::thread waiter([&] {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
        while (!waiter_acquired.load(std::memory_order_acquire) &&
               std::chrono::steady_clock::now() < deadline) {
            SyncManager::Instance().LockSpinLock(lock);
            waiter_acquired.store(true, std::memory_order_release);
            SyncManager::Instance().UnlockSpinLock(lock);
        }
    });

    hog.join();
    waiter.join();

    EXPECT_TRUE(waiter_acquired.load()) << "Waiter thread never acquired the spin lock";

    SyncManager::Instance().DestroySpinLock(lock);
}

// ============================================================================
// TLS Tests
// ============================================================================

TEST_F(SyncManagerTests, TlsCreateAndDestroy) {
    BML_TlsKey key = nullptr;
    EXPECT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateTls(nullptr, &key));
    EXPECT_NE(key, nullptr);
    SyncManager::Instance().DestroyTls(key);
}

TEST_F(SyncManagerTests, TlsSetGet) {
    BML_TlsKey key = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateTls(nullptr, &key));
    
    int value = 42;
    EXPECT_EQ(BML_RESULT_OK, SyncManager::Instance().SetTls(key, &value));
    
    void* retrieved = SyncManager::Instance().GetTls(key);
    EXPECT_EQ(retrieved, &value);
    EXPECT_EQ(*static_cast<int*>(retrieved), 42);
    
    SyncManager::Instance().DestroyTls(key);
}

TEST_F(SyncManagerTests, TlsThreadLocal) {
    BML_TlsKey key = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateTls(nullptr, &key));
    
    int main_value = 1;
    int thread_value = 2;
    
    EXPECT_EQ(BML_RESULT_OK, SyncManager::Instance().SetTls(key, &main_value));
    
    std::atomic<bool> checked{false};
    std::thread t([&] {
        // Initially should be null in new thread
        void* initial = SyncManager::Instance().GetTls(key);
        EXPECT_EQ(initial, nullptr);
        
        // Set different value
        EXPECT_EQ(BML_RESULT_OK, SyncManager::Instance().SetTls(key, &thread_value));
        void* retrieved = SyncManager::Instance().GetTls(key);
        EXPECT_EQ(retrieved, &thread_value);
        
        checked.store(true);
    });
    
    t.join();
    EXPECT_TRUE(checked.load());
    
    // Main thread should still have its value
    EXPECT_EQ(SyncManager::Instance().GetTls(key), &main_value);
    
    SyncManager::Instance().DestroyTls(key);
}

TEST_F(SyncManagerTests, TlsDestructorRunsOnThreadExit) {
    std::atomic<int> destructor_calls{0};
    auto destructor = [](void *value) {
        if (value) {
            auto *counter = static_cast<std::atomic<int> *>(value);
            counter->fetch_add(1, std::memory_order_relaxed);
        }
    };

    BML_TlsKey key = nullptr;
    ASSERT_EQ(BML_RESULT_OK, SyncManager::Instance().CreateTls(destructor, &key));

    {
        std::thread worker([&] {
            EXPECT_EQ(SyncManager::Instance().SetTls(key, &destructor_calls), BML_RESULT_OK);
        });
        worker.join();
    }

    // Destructor should have run when the worker thread exited
    EXPECT_EQ(destructor_calls.load(), 1);
    SyncManager::Instance().DestroyTls(key);
}

// ============================================================================
// Null Handle Tests
// ============================================================================

TEST_F(SyncManagerTests, MutexCreateRejectsNull) {
    EXPECT_NE(BML_RESULT_OK, SyncManager::Instance().CreateMutex(nullptr));
}

TEST_F(SyncManagerTests, RwLockCreateRejectsNull) {
    EXPECT_NE(BML_RESULT_OK, SyncManager::Instance().CreateRwLock(nullptr));
}

TEST_F(SyncManagerTests, SemaphoreCreateRejectsNull) {
    EXPECT_NE(BML_RESULT_OK, SyncManager::Instance().CreateSemaphore(1, 10, nullptr));
}

TEST_F(SyncManagerTests, CondVarCreateRejectsNull) {
    EXPECT_NE(BML_RESULT_OK, SyncManager::Instance().CreateCondVar(nullptr));
}

TEST_F(SyncManagerTests, SpinLockCreateRejectsNull) {
    EXPECT_NE(BML_RESULT_OK, SyncManager::Instance().CreateSpinLock(nullptr));
}

TEST_F(SyncManagerTests, TlsCreateRejectsNull) {
    EXPECT_NE(BML_RESULT_OK, SyncManager::Instance().CreateTls(nullptr, nullptr));
}

TEST_F(SyncManagerTests, MutexLockRejectsInvalidHandle) {
    ClearSyncLastError();
    SyncManager::Instance().LockMutex(reinterpret_cast<BML_Mutex>(0x1234));
    ExpectLastErrorCode(BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(SyncManagerTests, SemaphoreWaitRejectsInvalidHandle) {
    ClearSyncLastError();
    auto result = SyncManager::Instance().WaitSemaphore(reinterpret_cast<BML_Semaphore>(0x5678), 0);
    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
    ExpectLastErrorCode(BML_RESULT_INVALID_ARGUMENT);
}

// ============================================================================
// Invalid Initial Values
// ============================================================================

TEST_F(SyncManagerTests, SemaphoreRejectsInvalidCounts) {
    BML_Semaphore sem = nullptr;
    // Initial count > max count should fail
    EXPECT_NE(BML_RESULT_OK, SyncManager::Instance().CreateSemaphore(10, 5, &sem));
}

} // namespace

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
#include "TestKernel.h"

using BML::Core::SyncManager;

namespace {
using BML::Core::Testing::TestKernel;

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
    TestKernel kernel_;

    void SetUp() override {
        kernel_->sync = std::make_unique<SyncManager>();
    }

    void TearDown() override {
    }
};

// ============================================================================
// Mutex Tests
// ============================================================================

TEST_F(SyncManagerTests, MutexCreateAndDestroy) {
    BML_Mutex mutex = nullptr;
    EXPECT_EQ(BML_RESULT_OK, kernel_->sync->CreateMutex(nullptr, &mutex));
    EXPECT_NE(mutex, nullptr);
    kernel_->sync->DestroyMutex(mutex);
}

TEST_F(SyncManagerTests, MutexLockUnlock) {
    BML_Mutex mutex = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateMutex(nullptr, &mutex));

    kernel_->sync->LockMutex(mutex);
    kernel_->sync->UnlockMutex(mutex);

    kernel_->sync->DestroyMutex(mutex);
}

TEST_F(SyncManagerTests, MutexTryLock) {
    BML_Mutex mutex = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateMutex(nullptr, &mutex));

    // Should succeed when unlocked
    EXPECT_TRUE(kernel_->sync->TryLockMutex(mutex));
    kernel_->sync->UnlockMutex(mutex);

    kernel_->sync->DestroyMutex(mutex);
}

TEST_F(SyncManagerTests, MutexTryLockFailsForSameThreadReentry) {
    BML_Mutex mutex = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateMutex(nullptr, &mutex));

    kernel_->sync->LockMutex(mutex);
    EXPECT_FALSE(kernel_->sync->TryLockMutex(mutex));
    kernel_->sync->UnlockMutex(mutex);

    kernel_->sync->DestroyMutex(mutex);
}

TEST_F(SyncManagerTests, MutexLockTimeoutRejectsSameThreadReentry) {
    BML_Mutex mutex = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateMutex(nullptr, &mutex));

    kernel_->sync->LockMutex(mutex);
    EXPECT_EQ(BML_RESULT_SYNC_DEADLOCK, kernel_->sync->LockMutexTimeout(mutex, 1));
    kernel_->sync->UnlockMutex(mutex);

    kernel_->sync->DestroyMutex(mutex);
}

TEST_F(SyncManagerTests, MutexTryLockFails_WhenAlreadyLocked) {
    BML_Mutex mutex = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateMutex(nullptr, &mutex));

    std::atomic<bool> lock_held{false};
    std::atomic<bool> try_result{true};

    std::thread holder([&] {
        kernel_->sync->LockMutex(mutex);
        lock_held.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        kernel_->sync->UnlockMutex(mutex);
    });

    // Wait for holder to acquire lock
    while (!lock_held.load()) {
        std::this_thread::yield();
    }

    std::thread trier([&] {
        try_result.store(kernel_->sync->TryLockMutex(mutex));
    });

    trier.join();
    holder.join();

    // TryLock should have failed
    EXPECT_FALSE(try_result.load());

    kernel_->sync->DestroyMutex(mutex);
}

TEST_F(SyncManagerTests, MutexConcurrentAccess) {
    BML_Mutex mutex = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateMutex(nullptr, &mutex));

    constexpr int kThreads = 4;
    constexpr int kIterations = 1000;
    std::atomic<int> counter{0};
    int protected_counter = 0;

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < kIterations; ++j) {
                kernel_->sync->LockMutex(mutex);
                ++protected_counter;
                ++counter;
                kernel_->sync->UnlockMutex(mutex);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(counter.load(), kThreads * kIterations);
    EXPECT_EQ(protected_counter, kThreads * kIterations);

    kernel_->sync->DestroyMutex(mutex);
}

// ============================================================================
// RwLock Tests
// ============================================================================

TEST_F(SyncManagerTests, RwLockCreateAndDestroy) {
    BML_RwLock lock = nullptr;
    EXPECT_EQ(BML_RESULT_OK, kernel_->sync->CreateRwLock(nullptr, &lock));
    EXPECT_NE(lock, nullptr);
    kernel_->sync->DestroyRwLock(lock);
}

TEST_F(SyncManagerTests, RwLockReadLockUnlock) {
    BML_RwLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateRwLock(nullptr, &lock));

    kernel_->sync->ReadLockRwLock(lock);
    kernel_->sync->UnlockRwLock(lock);

    kernel_->sync->DestroyRwLock(lock);
}

TEST_F(SyncManagerTests, RwLockWriteLockUnlock) {
    BML_RwLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateRwLock(nullptr, &lock));

    kernel_->sync->WriteLockRwLock(lock);
    kernel_->sync->UnlockRwLock(lock);

    kernel_->sync->DestroyRwLock(lock);
}

TEST_F(SyncManagerTests, RwLockMultipleReaders) {
    BML_RwLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateRwLock(nullptr, &lock));

    constexpr int kReaders = 4;
    std::atomic<int> readers_in{0};
    std::atomic<bool> done{false};

    std::vector<std::thread> threads;
    for (int i = 0; i < kReaders; ++i) {
        threads.emplace_back([&] {
            kernel_->sync->ReadLockRwLock(lock);
            ++readers_in;
            while (!done.load()) {
                std::this_thread::yield();
            }
            --readers_in;
            kernel_->sync->UnlockRwLock(lock);
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

    kernel_->sync->DestroyRwLock(lock);
}

TEST_F(SyncManagerTests, RwLockTryReadLock) {
    BML_RwLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateRwLock(nullptr, &lock));

    EXPECT_TRUE(kernel_->sync->TryReadLockRwLock(lock));
    kernel_->sync->UnlockRwLock(lock);

    kernel_->sync->DestroyRwLock(lock);
}

TEST_F(SyncManagerTests, RwLockTryWriteLock) {
    BML_RwLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateRwLock(nullptr, &lock));

    EXPECT_TRUE(kernel_->sync->TryWriteLockRwLock(lock));
    kernel_->sync->UnlockRwLock(lock);

    kernel_->sync->DestroyRwLock(lock);
}

TEST_F(SyncManagerTests, RwLockWriteLockRejectsUpgradeFromRead) {
    BML_RwLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateRwLock(nullptr, &lock));

    kernel_->sync->ReadLockRwLock(lock);
    ClearSyncLastError();
    kernel_->sync->WriteLockRwLock(lock);
    ExpectLastErrorCode(BML_RESULT_INVALID_ARGUMENT);
    kernel_->sync->ReadUnlockRwLock(lock);

    kernel_->sync->DestroyRwLock(lock);
}

// ============================================================================
// Semaphore Tests
// ============================================================================

TEST_F(SyncManagerTests, SemaphoreCreateAndDestroy) {
    BML_Semaphore sem = nullptr;
    EXPECT_EQ(BML_RESULT_OK, kernel_->sync->CreateSemaphore(nullptr,1, 10, &sem));
    EXPECT_NE(sem, nullptr);
    kernel_->sync->DestroySemaphore(sem);
}

TEST_F(SyncManagerTests, SemaphoreWaitAndSignal) {
    BML_Semaphore sem = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateSemaphore(nullptr,1, 10, &sem));

    // Wait (acquire) the initial count - should succeed immediately
    EXPECT_EQ(BML_RESULT_OK, kernel_->sync->WaitSemaphore(sem, 0));

    // Signal (release) it back
    EXPECT_EQ(BML_RESULT_OK, kernel_->sync->SignalSemaphore(sem, 1));

    kernel_->sync->DestroySemaphore(sem);
}

TEST_F(SyncManagerTests, SemaphoreWaitTimeout) {
    BML_Semaphore sem = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateSemaphore(nullptr,0, 10, &sem));

    // Should timeout since count is 0
    auto start = std::chrono::steady_clock::now();
    BML_Result result = kernel_->sync->WaitSemaphore(sem, 10);
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_NE(result, BML_RESULT_OK);
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 5);

    kernel_->sync->DestroySemaphore(sem);
}

TEST_F(SyncManagerTests, SemaphoreSignalMultiple) {
    BML_Semaphore sem = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateSemaphore(nullptr,0, 10, &sem));

    // Signal 3 at once
    EXPECT_EQ(BML_RESULT_OK, kernel_->sync->SignalSemaphore(sem, 3));

    // Should be able to wait 3 times
    EXPECT_EQ(BML_RESULT_OK, kernel_->sync->WaitSemaphore(sem, 0));
    EXPECT_EQ(BML_RESULT_OK, kernel_->sync->WaitSemaphore(sem, 0));
    EXPECT_EQ(BML_RESULT_OK, kernel_->sync->WaitSemaphore(sem, 0));

    // Fourth wait should timeout
    EXPECT_NE(BML_RESULT_OK, kernel_->sync->WaitSemaphore(sem, 0));

    kernel_->sync->DestroySemaphore(sem);
}

// ============================================================================
// Condition Variable Tests
// ============================================================================

TEST_F(SyncManagerTests, CondVarCreateAndDestroy) {
    BML_CondVar cv = nullptr;
    EXPECT_EQ(BML_RESULT_OK, kernel_->sync->CreateCondVar(nullptr, &cv));
    EXPECT_NE(cv, nullptr);
    kernel_->sync->DestroyCondVar(cv);
}

TEST_F(SyncManagerTests, CondVarSignalOne) {
    BML_Mutex mutex = nullptr;
    BML_CondVar cv = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateMutex(nullptr, &mutex));
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateCondVar(nullptr, &cv));

    std::atomic<bool> ready{false};
    std::atomic<bool> signaled{false};

    std::atomic<BML_Result> wait_result{BML_RESULT_UNKNOWN_ERROR};
    std::thread waiter([&] {
        kernel_->sync->LockMutex(mutex);
        ready.store(true);
        wait_result.store(kernel_->sync->WaitCondVar(cv, mutex));
        signaled.store(true);
        kernel_->sync->UnlockMutex(mutex);
    });

    // Wait for waiter to be ready
    while (!ready.load()) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Signal the condition
    kernel_->sync->SignalCondVar(cv);

    waiter.join();
    EXPECT_TRUE(signaled.load());
    EXPECT_EQ(wait_result.load(), BML_RESULT_OK);

    kernel_->sync->DestroyCondVar(cv);
    kernel_->sync->DestroyMutex(mutex);
}

TEST_F(SyncManagerTests, CondVarBroadcast) {
    BML_Mutex mutex = nullptr;
    BML_CondVar cv = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateMutex(nullptr, &mutex));
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateCondVar(nullptr, &cv));

    constexpr int kWaiters = 3;
    std::atomic<int> ready_count{0};
    std::atomic<int> woken_count{0};

    std::vector<std::thread> waiters;
    std::vector<BML_Result> wait_results(kWaiters, BML_RESULT_UNKNOWN_ERROR);
    for (int i = 0; i < kWaiters; ++i) {
        waiters.emplace_back([&, i] {
            kernel_->sync->LockMutex(mutex);
            ++ready_count;
            wait_results[i] = kernel_->sync->WaitCondVar(cv, mutex);
            ++woken_count;
            kernel_->sync->UnlockMutex(mutex);
        });
    }

    // Wait for all waiters
    while (ready_count.load() < kWaiters) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Broadcast to wake all
    kernel_->sync->BroadcastCondVar(cv);

    for (auto& t : waiters) {
        t.join();
    }

    EXPECT_EQ(woken_count.load(), kWaiters);
    for (auto result : wait_results) {
        EXPECT_EQ(result, BML_RESULT_OK);
    }

    kernel_->sync->DestroyCondVar(cv);
    kernel_->sync->DestroyMutex(mutex);
}

TEST_F(SyncManagerTests, CondVarTimedWait) {
    BML_Mutex mutex = nullptr;
    BML_CondVar cv = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateMutex(nullptr, &mutex));
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateCondVar(nullptr, &cv));

    kernel_->sync->LockMutex(mutex);

    auto start = std::chrono::steady_clock::now();
    // Wait with very short timeout - should timeout
    BML_Result result = kernel_->sync->WaitCondVarTimeout(cv, mutex, 10);
    auto elapsed = std::chrono::steady_clock::now() - start;

    kernel_->sync->UnlockMutex(mutex);

    // Should have returned timeout
    EXPECT_EQ(result, BML_RESULT_TIMEOUT);
    // Should have taken at least ~10ms
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 5);

    kernel_->sync->DestroyCondVar(cv);
    kernel_->sync->DestroyMutex(mutex);
}

TEST_F(SyncManagerTests, CondVarWaitRejectsInvalidHandles) {
    ClearSyncLastError();
    BML_Result result = kernel_->sync->WaitCondVar(
        reinterpret_cast<BML_CondVar>(0x1), reinterpret_cast<BML_Mutex>(0x2));
    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
    ExpectLastErrorCode(BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(SyncManagerTests, CondVarTimeoutPrecisionWithinBounds) {
    BML_Mutex mutex = nullptr;
    BML_CondVar cv = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateMutex(nullptr, &mutex));
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateCondVar(nullptr, &cv));

    kernel_->sync->LockMutex(mutex);
    const uint32_t timeout_ms = 25;
    auto start = std::chrono::steady_clock::now();
    auto result = kernel_->sync->WaitCondVarTimeout(cv, mutex, timeout_ms);
    auto elapsed = std::chrono::steady_clock::now() - start;
    kernel_->sync->UnlockMutex(mutex);

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    EXPECT_EQ(result, BML_RESULT_TIMEOUT);
    EXPECT_GE(elapsed_ms, static_cast<long long>(timeout_ms * 0.6));
    EXPECT_LE(elapsed_ms, static_cast<long long>(timeout_ms * 2));

    kernel_->sync->DestroyCondVar(cv);
    kernel_->sync->DestroyMutex(mutex);
}

// ============================================================================
// SpinLock Tests
// ============================================================================

TEST_F(SyncManagerTests, SpinLockCreateAndDestroy) {
    BML_SpinLock lock = nullptr;
    EXPECT_EQ(BML_RESULT_OK, kernel_->sync->CreateSpinLock(nullptr, &lock));
    EXPECT_NE(lock, nullptr);
    kernel_->sync->DestroySpinLock(lock);
}

TEST_F(SyncManagerTests, SpinLockLockUnlock) {
    BML_SpinLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateSpinLock(nullptr, &lock));

    kernel_->sync->LockSpinLock(lock);
    kernel_->sync->UnlockSpinLock(lock);

    kernel_->sync->DestroySpinLock(lock);
}

TEST_F(SyncManagerTests, SpinLockTryLock) {
    BML_SpinLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateSpinLock(nullptr, &lock));

    EXPECT_TRUE(kernel_->sync->TryLockSpinLock(lock));
    kernel_->sync->UnlockSpinLock(lock);

    kernel_->sync->DestroySpinLock(lock);
}

TEST_F(SyncManagerTests, SpinLockConcurrentAccess) {
    BML_SpinLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateSpinLock(nullptr, &lock));

    constexpr int kThreads = 4;
    constexpr int kIterations = 10000;
    int counter = 0;

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < kIterations; ++j) {
                kernel_->sync->LockSpinLock(lock);
                ++counter;
                kernel_->sync->UnlockSpinLock(lock);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(counter, kThreads * kIterations);

    kernel_->sync->DestroySpinLock(lock);
}

TEST_F(SyncManagerTests, SpinLockFairnessPreventsStarvation) {
    BML_SpinLock lock = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateSpinLock(nullptr, &lock));

    std::atomic<bool> waiter_acquired{false};

    std::thread hog([&] {
        for (int i = 0; i < 50000; ++i) {
            kernel_->sync->LockSpinLock(lock);
            kernel_->sync->UnlockSpinLock(lock);
        }
    });

    std::thread waiter([&] {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
        while (!waiter_acquired.load(std::memory_order_acquire) &&
               std::chrono::steady_clock::now() < deadline) {
            kernel_->sync->LockSpinLock(lock);
            waiter_acquired.store(true, std::memory_order_release);
            kernel_->sync->UnlockSpinLock(lock);
        }
    });

    hog.join();
    waiter.join();

    EXPECT_TRUE(waiter_acquired.load()) << "Waiter thread never acquired the spin lock";

    kernel_->sync->DestroySpinLock(lock);
}

// ============================================================================
// TLS Tests
// ============================================================================

TEST_F(SyncManagerTests, TlsCreateAndDestroy) {
    BML_TlsKey key = nullptr;
    EXPECT_EQ(BML_RESULT_OK, kernel_->sync->CreateTls(nullptr, nullptr, &key));
    EXPECT_NE(key, nullptr);
    kernel_->sync->DestroyTls(key);
}

TEST_F(SyncManagerTests, TlsSetGet) {
    BML_TlsKey key = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateTls(nullptr, nullptr, &key));

    int value = 42;
    EXPECT_EQ(BML_RESULT_OK, kernel_->sync->SetTls(key, &value));

    void* retrieved = kernel_->sync->GetTls(key);
    EXPECT_EQ(retrieved, &value);
    EXPECT_EQ(*static_cast<int*>(retrieved), 42);

    kernel_->sync->DestroyTls(key);
}

TEST_F(SyncManagerTests, TlsThreadLocal) {
    BML_TlsKey key = nullptr;
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateTls(nullptr, nullptr, &key));

    int main_value = 1;
    int thread_value = 2;

    EXPECT_EQ(BML_RESULT_OK, kernel_->sync->SetTls(key, &main_value));

    std::atomic<bool> checked{false};
    std::thread t([&] {
        // Initially should be null in new thread
        void* initial = kernel_->sync->GetTls(key);
        EXPECT_EQ(initial, nullptr);

        // Set different value
        EXPECT_EQ(BML_RESULT_OK, kernel_->sync->SetTls(key, &thread_value));
        void* retrieved = kernel_->sync->GetTls(key);
        EXPECT_EQ(retrieved, &thread_value);

        checked.store(true);
    });

    t.join();
    EXPECT_TRUE(checked.load());

    // Main thread should still have its value
    EXPECT_EQ(kernel_->sync->GetTls(key), &main_value);

    kernel_->sync->DestroyTls(key);
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
    ASSERT_EQ(BML_RESULT_OK, kernel_->sync->CreateTls(nullptr, destructor, &key));

    {
        std::thread worker([&] {
            EXPECT_EQ(kernel_->sync->SetTls(key, &destructor_calls), BML_RESULT_OK);
        });
        worker.join();
    }

    // Destructor should have run when the worker thread exited
    EXPECT_EQ(destructor_calls.load(), 1);
    kernel_->sync->DestroyTls(key);
}

// ============================================================================
// Null Handle Tests
// ============================================================================

TEST_F(SyncManagerTests, MutexCreateRejectsNull) {
    EXPECT_NE(BML_RESULT_OK, kernel_->sync->CreateMutex(nullptr, nullptr));
}

TEST_F(SyncManagerTests, RwLockCreateRejectsNull) {
    EXPECT_NE(BML_RESULT_OK, kernel_->sync->CreateRwLock(nullptr, nullptr));
}

TEST_F(SyncManagerTests, SemaphoreCreateRejectsNull) {
    EXPECT_NE(BML_RESULT_OK, kernel_->sync->CreateSemaphore(nullptr, 1, 10, nullptr));
}

TEST_F(SyncManagerTests, CondVarCreateRejectsNull) {
    EXPECT_NE(BML_RESULT_OK, kernel_->sync->CreateCondVar(nullptr, nullptr));
}

TEST_F(SyncManagerTests, SpinLockCreateRejectsNull) {
    EXPECT_NE(BML_RESULT_OK, kernel_->sync->CreateSpinLock(nullptr, nullptr));
}

TEST_F(SyncManagerTests, TlsCreateRejectsNull) {
    EXPECT_NE(BML_RESULT_OK, kernel_->sync->CreateTls(nullptr, nullptr, nullptr));
}

TEST_F(SyncManagerTests, MutexLockRejectsInvalidHandle) {
    ClearSyncLastError();
    kernel_->sync->LockMutex(reinterpret_cast<BML_Mutex>(0x1234));
    ExpectLastErrorCode(BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(SyncManagerTests, SemaphoreWaitRejectsInvalidHandle) {
    ClearSyncLastError();
    auto result = kernel_->sync->WaitSemaphore(reinterpret_cast<BML_Semaphore>(0x5678), 0);
    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
    ExpectLastErrorCode(BML_RESULT_INVALID_ARGUMENT);
}

// ============================================================================
// Invalid Initial Values
// ============================================================================

TEST_F(SyncManagerTests, SemaphoreRejectsInvalidCounts) {
    BML_Semaphore sem = nullptr;
    // Initial count > max count should fail
    EXPECT_NE(BML_RESULT_OK, kernel_->sync->CreateSemaphore(nullptr,10, 5, &sem));
}

} // namespace

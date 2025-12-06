/**
 * @file SyncPrimitivesTest.cpp
 * @brief Tests for synchronization primitives (CondVar, SpinLock)
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "bml_sync.h"
#include "bml_errors.h"
#include "Core/CoreErrors.h"
#include "Core/SyncManager.h"

using namespace BML::Core;

namespace {

struct DeadlockInfo {
    BML_Result code{BML_RESULT_OK};
    std::string api;
};

template <typename Handle, typename LockFunc, typename UnlockFunc>
std::pair<BML_Result, std::string> RunDeadlockDetectionScenario(
    Handle first,
    Handle second,
    LockFunc lock_func,
    UnlockFunc unlock_func) {

    std::promise<std::pair<BML_Result, std::string>> result_promise;
    auto result_future = result_promise.get_future();

    std::mutex state_mutex;
    std::condition_variable state_cv;
    bool a_has_first = false;
    bool b_has_second = false;
    bool a_attempting_second = false;

    const auto settle_delay = std::chrono::milliseconds(5);

    std::thread thread_a([&] {
        BML::Core::ClearLastErrorInfo();
        lock_func(first);
        {
            std::lock_guard<std::mutex> guard(state_mutex);
            a_has_first = true;
        }
        state_cv.notify_all();

        {
            std::unique_lock<std::mutex> lk(state_mutex);
            state_cv.wait(lk, [&] { return b_has_second; });
        }

        {
            std::lock_guard<std::mutex> guard(state_mutex);
            a_attempting_second = true;
        }
        state_cv.notify_all();

        lock_func(second);
        unlock_func(second);
        unlock_func(first);
    });

    std::thread thread_b([&] {
        BML::Core::ClearLastErrorInfo();

        {
            std::unique_lock<std::mutex> lk(state_mutex);
            state_cv.wait(lk, [&] { return a_has_first; });
        }

        lock_func(second);
        {
            std::lock_guard<std::mutex> guard(state_mutex);
            b_has_second = true;
        }
        state_cv.notify_all();

        {
            std::unique_lock<std::mutex> lk(state_mutex);
            state_cv.wait(lk, [&] { return a_attempting_second; });
        }

        std::this_thread::sleep_for(settle_delay);

        BML::Core::ClearLastErrorInfo();
        lock_func(first);

        BML_ErrorInfo info = BML_ERROR_INFO_INIT;
        BML_Result status = BML::Core::GetLastErrorInfo(&info);
        BML_Result code = BML_RESULT_OK;
        std::string api_name;
        bool acquired_first = false;

        if (status == BML_RESULT_OK) {
            code = info.result_code;
            if (info.api_name) {
                api_name = info.api_name;
            }
            if (code != BML_RESULT_SYNC_DEADLOCK) {
                acquired_first = true;
            }
        } else if (status == BML_RESULT_NOT_FOUND) {
            acquired_first = true;
        } else {
            code = status;
        }

        if (acquired_first) {
            unlock_func(first);
        }

        unlock_func(second);
        result_promise.set_value({code, api_name});
    });

    thread_a.join();
    thread_b.join();
    return result_future.get();
}

} // namespace

class SyncPrimitivesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // SyncManager is a singleton, no setup needed
    }

    void TearDown() override {
        // Cleanup handled by test cases
    }
};

// ============================================================================
// CondVar Tests
// ============================================================================

TEST_F(SyncPrimitivesTest, CondVarCreateDestroy) {
    BML_CondVar condvar = nullptr;
    BML_Result result = SyncManager::Instance().CreateCondVar(&condvar);
    ASSERT_EQ(result, BML_RESULT_OK);
    ASSERT_NE(condvar, nullptr);
    
    SyncManager::Instance().DestroyCondVar(condvar);
}

TEST_F(SyncPrimitivesTest, CondVarCreateNullOutput) {
    BML_Result result = SyncManager::Instance().CreateCondVar(nullptr);
    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(SyncPrimitivesTest, CondVarSignalWait) {
    BML_Mutex mutex = nullptr;
    BML_CondVar condvar = nullptr;
    
    ASSERT_EQ(SyncManager::Instance().CreateMutex(&mutex), BML_RESULT_OK);
    ASSERT_EQ(SyncManager::Instance().CreateCondVar(&condvar), BML_RESULT_OK);
    
    std::atomic<bool> ready{false};
    std::atomic<bool> done{false};
    
    // Consumer thread
    std::thread consumer([&]() {
        SyncManager::Instance().LockMutex(mutex);
        while (!ready.load()) {
            // Wait for signal (with timeout to prevent deadlock in test)
            BML_Result result = SyncManager::Instance().WaitCondVarTimeout(condvar, mutex, 1000);
            if (result == BML_RESULT_TIMEOUT && !ready.load()) {
                // Continue waiting
                continue;
            }
        }
        done.store(true);
        SyncManager::Instance().UnlockMutex(mutex);
    });
    
    // Producer: signal after short delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    SyncManager::Instance().LockMutex(mutex);
    ready.store(true);
    SyncManager::Instance().SignalCondVar(condvar);
    SyncManager::Instance().UnlockMutex(mutex);
    
    consumer.join();
    
    EXPECT_TRUE(done.load());
    
    SyncManager::Instance().DestroyCondVar(condvar);
    SyncManager::Instance().DestroyMutex(mutex);
}

TEST_F(SyncPrimitivesTest, CondVarWaitTimeout) {
    BML_Mutex mutex = nullptr;
    BML_CondVar condvar = nullptr;
    
    ASSERT_EQ(SyncManager::Instance().CreateMutex(&mutex), BML_RESULT_OK);
    ASSERT_EQ(SyncManager::Instance().CreateCondVar(&condvar), BML_RESULT_OK);
    
    SyncManager::Instance().LockMutex(mutex);
    
    auto start = std::chrono::steady_clock::now();
    BML_Result result = SyncManager::Instance().WaitCondVarTimeout(condvar, mutex, 100);
    auto end = std::chrono::steady_clock::now();
    
    SyncManager::Instance().UnlockMutex(mutex);
    
    EXPECT_EQ(result, BML_RESULT_TIMEOUT);
    
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_GE(elapsed_ms, 80);  // Allow some tolerance
    EXPECT_LE(elapsed_ms, 200); // But not too long
    
    SyncManager::Instance().DestroyCondVar(condvar);
    SyncManager::Instance().DestroyMutex(mutex);
}

TEST_F(SyncPrimitivesTest, CondVarBroadcast) {
    BML_Mutex mutex = nullptr;
    BML_CondVar condvar = nullptr;
    
    ASSERT_EQ(SyncManager::Instance().CreateMutex(&mutex), BML_RESULT_OK);
    ASSERT_EQ(SyncManager::Instance().CreateCondVar(&condvar), BML_RESULT_OK);
    
    std::atomic<bool> ready{false};
    std::atomic<int> woken_count{0};
    constexpr int NUM_THREADS = 3;
    
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&]() {
            SyncManager::Instance().LockMutex(mutex);
            while (!ready.load()) {
                BML_Result result = SyncManager::Instance().WaitCondVarTimeout(condvar, mutex, 1000);
                if (result == BML_RESULT_TIMEOUT && !ready.load()) {
                    continue;
                }
            }
            woken_count.fetch_add(1);
            SyncManager::Instance().UnlockMutex(mutex);
        });
    }
    
    // Let all threads start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Broadcast to wake all threads
    SyncManager::Instance().LockMutex(mutex);
    ready.store(true);
    SyncManager::Instance().BroadcastCondVar(condvar);
    SyncManager::Instance().UnlockMutex(mutex);
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(woken_count.load(), NUM_THREADS);
    
    SyncManager::Instance().DestroyCondVar(condvar);
    SyncManager::Instance().DestroyMutex(mutex);
}

// ============================================================================
// SpinLock Tests
// ============================================================================

TEST_F(SyncPrimitivesTest, SpinLockCreateDestroy) {
    BML_SpinLock lock = nullptr;
    BML_Result result = SyncManager::Instance().CreateSpinLock(&lock);
    ASSERT_EQ(result, BML_RESULT_OK);
    ASSERT_NE(lock, nullptr);
    
    SyncManager::Instance().DestroySpinLock(lock);
}

TEST_F(SyncPrimitivesTest, SpinLockCreateNullOutput) {
    BML_Result result = SyncManager::Instance().CreateSpinLock(nullptr);
    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(SyncPrimitivesTest, SpinLockBasicLocking) {
    BML_SpinLock lock = nullptr;
    ASSERT_EQ(SyncManager::Instance().CreateSpinLock(&lock), BML_RESULT_OK);
    
    SyncManager::Instance().LockSpinLock(lock);
    // Lock acquired
    SyncManager::Instance().UnlockSpinLock(lock);
    // Lock released
    
    SyncManager::Instance().DestroySpinLock(lock);
}

TEST_F(SyncPrimitivesTest, SpinLockTryLock) {
    BML_SpinLock lock = nullptr;
    ASSERT_EQ(SyncManager::Instance().CreateSpinLock(&lock), BML_RESULT_OK);
    
    // Should succeed on unlocked lock
    EXPECT_EQ(SyncManager::Instance().TryLockSpinLock(lock), BML_TRUE);
    
    // Try from another thread - should fail
    std::atomic<BML_Bool> try_result{BML_TRUE};
    std::thread t([&]() {
        try_result.store(SyncManager::Instance().TryLockSpinLock(lock));
    });
    t.join();
    
    EXPECT_EQ(try_result.load(), BML_FALSE);
    
    SyncManager::Instance().UnlockSpinLock(lock);
    
    // Now should succeed again
    EXPECT_EQ(SyncManager::Instance().TryLockSpinLock(lock), BML_TRUE);
    SyncManager::Instance().UnlockSpinLock(lock);
    
    SyncManager::Instance().DestroySpinLock(lock);
}

TEST_F(SyncPrimitivesTest, SpinLockConcurrentAccess) {
    BML_SpinLock lock = nullptr;
    ASSERT_EQ(SyncManager::Instance().CreateSpinLock(&lock), BML_RESULT_OK);
    
    std::atomic<int> counter{0};
    constexpr int INCREMENTS_PER_THREAD = 1000;
    constexpr int NUM_THREADS = 4;
    
    auto increment_func = [&]() {
        for (int i = 0; i < INCREMENTS_PER_THREAD; ++i) {
            SyncManager::Instance().LockSpinLock(lock);
            ++counter;
            SyncManager::Instance().UnlockSpinLock(lock);
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(increment_func);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(counter.load(), INCREMENTS_PER_THREAD * NUM_THREADS);
    
    SyncManager::Instance().DestroySpinLock(lock);
}

// ============================================================================
// Deadlock Detection Tests
// ============================================================================

TEST_F(SyncPrimitivesTest, MutexDeadlockDetectionSetsLastError) {
    BML_Mutex first = nullptr;
    BML_Mutex second = nullptr;
    ASSERT_EQ(SyncManager::Instance().CreateMutex(&first), BML_RESULT_OK);
    ASSERT_EQ(SyncManager::Instance().CreateMutex(&second), BML_RESULT_OK);

    auto [code, api_name] = RunDeadlockDetectionScenario(
        first,
        second,
        [](BML_Mutex mutex) { SyncManager::Instance().LockMutex(mutex); },
        [](BML_Mutex mutex) { SyncManager::Instance().UnlockMutex(mutex); });

    EXPECT_EQ(code, BML_RESULT_SYNC_DEADLOCK);
    EXPECT_EQ(api_name, "bmlMutexLock");

    SyncManager::Instance().DestroyMutex(first);
    SyncManager::Instance().DestroyMutex(second);
}

TEST_F(SyncPrimitivesTest, RwLockDeadlockDetectionSetsLastError) {
    BML_RwLock first = nullptr;
    BML_RwLock second = nullptr;
    ASSERT_EQ(SyncManager::Instance().CreateRwLock(&first), BML_RESULT_OK);
    ASSERT_EQ(SyncManager::Instance().CreateRwLock(&second), BML_RESULT_OK);

    auto [code, api_name] = RunDeadlockDetectionScenario(
        first,
        second,
        [](BML_RwLock lock) { SyncManager::Instance().WriteLockRwLock(lock); },
        [](BML_RwLock lock) { SyncManager::Instance().WriteUnlockRwLock(lock); });

    EXPECT_EQ(code, BML_RESULT_SYNC_DEADLOCK);
    EXPECT_EQ(api_name, "bmlRwLockWriteLock");

    SyncManager::Instance().DestroyRwLock(first);
    SyncManager::Instance().DestroyRwLock(second);
}

TEST_F(SyncPrimitivesTest, SpinLockDeadlockDetectionSetsLastError) {
    BML_SpinLock first = nullptr;
    BML_SpinLock second = nullptr;
    ASSERT_EQ(SyncManager::Instance().CreateSpinLock(&first), BML_RESULT_OK);
    ASSERT_EQ(SyncManager::Instance().CreateSpinLock(&second), BML_RESULT_OK);

    auto [code, api_name] = RunDeadlockDetectionScenario(
        first,
        second,
        [](BML_SpinLock lock) { SyncManager::Instance().LockSpinLock(lock); },
        [](BML_SpinLock lock) { SyncManager::Instance().UnlockSpinLock(lock); });

    EXPECT_EQ(code, BML_RESULT_SYNC_DEADLOCK);
    EXPECT_EQ(api_name, "bmlSpinLockLock");

    SyncManager::Instance().DestroySpinLock(first);
    SyncManager::Instance().DestroySpinLock(second);
}

TEST_F(SyncPrimitivesTest, CondVarWaitDeadlockDetectionWithExtraMutex) {
    BML_Mutex signal_mutex = nullptr;
    BML_Mutex payload_mutex = nullptr;
    BML_CondVar condvar = nullptr;
    ASSERT_EQ(SyncManager::Instance().CreateMutex(&signal_mutex), BML_RESULT_OK);
    ASSERT_EQ(SyncManager::Instance().CreateMutex(&payload_mutex), BML_RESULT_OK);
    ASSERT_EQ(SyncManager::Instance().CreateCondVar(&condvar), BML_RESULT_OK);

    std::atomic<bool> wait_entered{false};
    std::promise<BML_Result> wait_result_promise;
    auto wait_result_future = wait_result_promise.get_future();
    std::promise<DeadlockInfo> contender_result_promise;
    auto contender_future = contender_result_promise.get_future();

    std::thread waiter([&]() {
        BML::Core::ClearLastErrorInfo();
        SyncManager::Instance().LockMutex(signal_mutex);
        SyncManager::Instance().LockMutex(payload_mutex);
        wait_entered.store(true, std::memory_order_release);

        BML_Result wait_result = SyncManager::Instance().WaitCondVarTimeout(condvar, signal_mutex, 200);
        wait_result_promise.set_value(wait_result);

        SyncManager::Instance().UnlockMutex(payload_mutex);
        SyncManager::Instance().UnlockMutex(signal_mutex);
    });

    std::thread contender([&]() {
        BML::Core::ClearLastErrorInfo();
        while (!wait_entered.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        SyncManager::Instance().LockMutex(signal_mutex);
        BML::Core::ClearLastErrorInfo();
        SyncManager::Instance().LockMutex(payload_mutex);

        BML_ErrorInfo info = BML_ERROR_INFO_INIT;
        DeadlockInfo result{};
        if (BML::Core::GetLastErrorInfo(&info) == BML_RESULT_OK) {
            result.code = info.result_code;
            if (info.api_name) {
                result.api = info.api_name;
            }
        }

        if (result.code != BML_RESULT_SYNC_DEADLOCK) {
            SyncManager::Instance().UnlockMutex(payload_mutex);
        }

        SyncManager::Instance().UnlockMutex(signal_mutex);
        contender_result_promise.set_value(std::move(result));
    });

    auto contender_result = contender_future.get();
    auto wait_result = wait_result_future.get();

    waiter.join();
    contender.join();

    EXPECT_EQ(contender_result.code, BML_RESULT_SYNC_DEADLOCK);
    EXPECT_EQ(contender_result.api, "bmlMutexLock");
    EXPECT_EQ(wait_result, BML_RESULT_TIMEOUT);

    SyncManager::Instance().DestroyCondVar(condvar);
    SyncManager::Instance().DestroyMutex(payload_mutex);
    SyncManager::Instance().DestroyMutex(signal_mutex);
}

TEST_F(SyncPrimitivesTest, MutexTimeoutDeadlockDetectionSetsLastError) {
    BML_Mutex first = nullptr;
    BML_Mutex second = nullptr;
    ASSERT_EQ(SyncManager::Instance().CreateMutex(&first), BML_RESULT_OK);
    ASSERT_EQ(SyncManager::Instance().CreateMutex(&second), BML_RESULT_OK);

    std::mutex state_mutex;
    std::condition_variable state_cv;
    bool a_has_first = false;
    bool b_has_second = false;
    bool a_attempting_second = false;

    std::promise<DeadlockInfo> result_promise;
    auto result_future = result_promise.get_future();

    std::thread thread_a([&]() {
        SyncManager::Instance().LockMutex(first);
        {
            std::lock_guard<std::mutex> guard(state_mutex);
            a_has_first = true;
        }
        state_cv.notify_all();

        {
            std::unique_lock<std::mutex> lk(state_mutex);
            state_cv.wait(lk, [&] { return b_has_second; });
            a_attempting_second = true;
        }
        state_cv.notify_all();

        auto result = SyncManager::Instance().LockMutexTimeout(second, 1000);
        if (result == BML_RESULT_OK) {
            SyncManager::Instance().UnlockMutex(second);
        }
        SyncManager::Instance().UnlockMutex(first);
    });

    std::thread thread_b([&]() {
        BML::Core::ClearLastErrorInfo();
        {
            std::unique_lock<std::mutex> lk(state_mutex);
            state_cv.wait(lk, [&] { return a_has_first; });
        }

        SyncManager::Instance().LockMutex(second);
        {
            std::lock_guard<std::mutex> guard(state_mutex);
            b_has_second = true;
        }
        state_cv.notify_all();

        {
            std::unique_lock<std::mutex> lk(state_mutex);
            state_cv.wait(lk, [&] { return a_attempting_second; });
        }

        auto lock_result = SyncManager::Instance().LockMutexTimeout(first, 1000);
        DeadlockInfo info{};
        info.code = lock_result;
        if (lock_result == BML_RESULT_SYNC_DEADLOCK) {
            BML_ErrorInfo err = BML_ERROR_INFO_INIT;
            if (BML::Core::GetLastErrorInfo(&err) == BML_RESULT_OK && err.api_name) {
                info.api = err.api_name;
            }
        }

        if (lock_result == BML_RESULT_OK) {
            SyncManager::Instance().UnlockMutex(first);
        }
        SyncManager::Instance().UnlockMutex(second);
        result_promise.set_value(info);
    });

    auto info = result_future.get();
    thread_a.join();
    thread_b.join();

    EXPECT_EQ(info.code, BML_RESULT_SYNC_DEADLOCK);
    EXPECT_EQ(info.api, "bmlMutexLockTimeout");

    SyncManager::Instance().DestroyMutex(second);
    SyncManager::Instance().DestroyMutex(first);
}

TEST_F(SyncPrimitivesTest, RwLockWriteTimeoutDeadlockDetectionSetsLastError) {
    BML_RwLock first = nullptr;
    BML_RwLock second = nullptr;
    ASSERT_EQ(SyncManager::Instance().CreateRwLock(&first), BML_RESULT_OK);
    ASSERT_EQ(SyncManager::Instance().CreateRwLock(&second), BML_RESULT_OK);

    std::mutex state_mutex;
    std::condition_variable state_cv;
    bool a_has_first = false;
    bool b_has_second = false;
    bool a_attempting_second = false;

    std::promise<DeadlockInfo> result_promise;
    auto result_future = result_promise.get_future();

    std::thread thread_a([&]() {
        SyncManager::Instance().WriteLockRwLock(first);
        {
            std::lock_guard<std::mutex> guard(state_mutex);
            a_has_first = true;
        }
        state_cv.notify_all();

        {
            std::unique_lock<std::mutex> lk(state_mutex);
            state_cv.wait(lk, [&] { return b_has_second; });
            a_attempting_second = true;
        }
        state_cv.notify_all();

        auto result = SyncManager::Instance().WriteLockRwLockTimeout(second, 1000);
        if (result == BML_RESULT_OK) {
            SyncManager::Instance().WriteUnlockRwLock(second);
        }
        SyncManager::Instance().WriteUnlockRwLock(first);
    });

    std::thread thread_b([&]() {
        BML::Core::ClearLastErrorInfo();
        {
            std::unique_lock<std::mutex> lk(state_mutex);
            state_cv.wait(lk, [&] { return a_has_first; });
        }

        SyncManager::Instance().WriteLockRwLock(second);
        {
            std::lock_guard<std::mutex> guard(state_mutex);
            b_has_second = true;
        }
        state_cv.notify_all();

        {
            std::unique_lock<std::mutex> lk(state_mutex);
            state_cv.wait(lk, [&] { return a_attempting_second; });
        }

        auto lock_result = SyncManager::Instance().WriteLockRwLockTimeout(first, 1000);
        DeadlockInfo info{};
        info.code = lock_result;
        if (lock_result == BML_RESULT_SYNC_DEADLOCK) {
            BML_ErrorInfo err = BML_ERROR_INFO_INIT;
            if (BML::Core::GetLastErrorInfo(&err) == BML_RESULT_OK && err.api_name) {
                info.api = err.api_name;
            }
        }

        if (lock_result == BML_RESULT_OK) {
            SyncManager::Instance().WriteUnlockRwLock(first);
        }
        SyncManager::Instance().WriteUnlockRwLock(second);
        result_promise.set_value(info);
    });

    auto info = result_future.get();
    thread_a.join();
    thread_b.join();

    EXPECT_EQ(info.code, BML_RESULT_SYNC_DEADLOCK);
    EXPECT_EQ(info.api, "bmlRwLockWriteLockTimeout");

    SyncManager::Instance().DestroyRwLock(second);
    SyncManager::Instance().DestroyRwLock(first);
}

TEST_F(SyncPrimitivesTest, SemaphoreWaitDeadlockDetectionSetsLastError) {
    BML_Semaphore first = nullptr;
    BML_Semaphore second = nullptr;
    ASSERT_EQ(SyncManager::Instance().CreateSemaphore(1, 1, &first), BML_RESULT_OK);
    ASSERT_EQ(SyncManager::Instance().CreateSemaphore(1, 1, &second), BML_RESULT_OK);

    std::mutex state_mutex;
    std::condition_variable state_cv;
    bool a_has_first = false;
    bool b_has_second = false;
    bool a_attempting_second = false;

    std::promise<DeadlockInfo> result_promise;
    auto result_future = result_promise.get_future();

    std::thread thread_a([&]() {
        ASSERT_EQ(SyncManager::Instance().WaitSemaphore(first, BML_TIMEOUT_INFINITE), BML_RESULT_OK);
        {
            std::lock_guard<std::mutex> guard(state_mutex);
            a_has_first = true;
        }
        state_cv.notify_all();

        {
            std::unique_lock<std::mutex> lk(state_mutex);
            state_cv.wait(lk, [&] { return b_has_second; });
            a_attempting_second = true;
        }
        state_cv.notify_all();

        auto result = SyncManager::Instance().WaitSemaphore(second, 1000);
        if (result == BML_RESULT_OK) {
            SyncManager::Instance().SignalSemaphore(second, 1);
        }
        SyncManager::Instance().SignalSemaphore(first, 1);
    });

    std::thread thread_b([&]() {
        BML::Core::ClearLastErrorInfo();
        {
            std::unique_lock<std::mutex> lk(state_mutex);
            state_cv.wait(lk, [&] { return a_has_first; });
        }

        ASSERT_EQ(SyncManager::Instance().WaitSemaphore(second, BML_TIMEOUT_INFINITE), BML_RESULT_OK);
        {
            std::lock_guard<std::mutex> guard(state_mutex);
            b_has_second = true;
        }
        state_cv.notify_all();

        {
            std::unique_lock<std::mutex> lk(state_mutex);
            state_cv.wait(lk, [&] { return a_attempting_second; });
        }

        BML::Core::ClearLastErrorInfo();
        auto lock_result = SyncManager::Instance().WaitSemaphore(first, 1000);

        DeadlockInfo info{};
        info.code = lock_result;
        if (lock_result == BML_RESULT_SYNC_DEADLOCK) {
            BML_ErrorInfo err = BML_ERROR_INFO_INIT;
            if (BML::Core::GetLastErrorInfo(&err) == BML_RESULT_OK && err.api_name) {
                info.api = err.api_name;
            }
        }

        if (lock_result == BML_RESULT_OK) {
            SyncManager::Instance().SignalSemaphore(first, 1);
        }
        SyncManager::Instance().SignalSemaphore(second, 1);
        result_promise.set_value(info);
    });

    auto info = result_future.get();
    thread_a.join();
    thread_b.join();

    EXPECT_EQ(info.code, BML_RESULT_SYNC_DEADLOCK);
    EXPECT_EQ(info.api, "bmlSemaphoreWait");

    SyncManager::Instance().DestroySemaphore(second);
    SyncManager::Instance().DestroySemaphore(first);
}

// ============================================================================
// Capabilities Test
// ============================================================================

TEST_F(SyncPrimitivesTest, CapsIncludeCondVarAndSpinLock) {
    BML_SyncCaps caps = {};
    caps.struct_size = sizeof(BML_SyncCaps);
    
    BML_Result result = SyncManager::Instance().GetCaps(&caps);
    ASSERT_EQ(result, BML_RESULT_OK);
    
    EXPECT_TRUE(caps.capability_flags & BML_SYNC_CAP_CONDVAR);
    EXPECT_TRUE(caps.capability_flags & BML_SYNC_CAP_SPINLOCK);
}

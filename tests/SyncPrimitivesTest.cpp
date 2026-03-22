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
#include "TestKernel.h"

using namespace BML::Core;

namespace {
using BML::Core::Testing::TestKernel;

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
    TestKernel kernel_;

    void SetUp() override {
        kernel_->sync = std::make_unique<SyncManager>();
    }

    void TearDown() override {
    }
};

// ============================================================================
// CondVar Tests
// ============================================================================

TEST_F(SyncPrimitivesTest, CondVarCreateDestroy) {
    BML_CondVar condvar = nullptr;
    BML_Result result = kernel_->sync->CreateCondVar(&condvar);
    ASSERT_EQ(result, BML_RESULT_OK);
    ASSERT_NE(condvar, nullptr);

    kernel_->sync->DestroyCondVar(condvar);
}

TEST_F(SyncPrimitivesTest, CondVarCreateNullOutput) {
    BML_Result result = kernel_->sync->CreateCondVar(nullptr);
    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(SyncPrimitivesTest, CondVarSignalWait) {
    BML_Mutex mutex = nullptr;
    BML_CondVar condvar = nullptr;

    ASSERT_EQ(kernel_->sync->CreateMutex(&mutex), BML_RESULT_OK);
    ASSERT_EQ(kernel_->sync->CreateCondVar(&condvar), BML_RESULT_OK);

    std::atomic<bool> ready{false};
    std::atomic<bool> done{false};

    // Consumer thread
    std::thread consumer([&]() {
        kernel_->sync->LockMutex(mutex);
        while (!ready.load()) {
            // Wait for signal (with timeout to prevent deadlock in test)
            BML_Result result = kernel_->sync->WaitCondVarTimeout(condvar, mutex, 1000);
            if (result == BML_RESULT_TIMEOUT && !ready.load()) {
                // Continue waiting
                continue;
            }
        }
        done.store(true);
        kernel_->sync->UnlockMutex(mutex);
    });

    // Producer: signal after short delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    kernel_->sync->LockMutex(mutex);
    ready.store(true);
    kernel_->sync->SignalCondVar(condvar);
    kernel_->sync->UnlockMutex(mutex);

    consumer.join();

    EXPECT_TRUE(done.load());

    kernel_->sync->DestroyCondVar(condvar);
    kernel_->sync->DestroyMutex(mutex);
}

TEST_F(SyncPrimitivesTest, CondVarWaitTimeout) {
    BML_Mutex mutex = nullptr;
    BML_CondVar condvar = nullptr;

    ASSERT_EQ(kernel_->sync->CreateMutex(&mutex), BML_RESULT_OK);
    ASSERT_EQ(kernel_->sync->CreateCondVar(&condvar), BML_RESULT_OK);

    kernel_->sync->LockMutex(mutex);

    auto start = std::chrono::steady_clock::now();
    BML_Result result = kernel_->sync->WaitCondVarTimeout(condvar, mutex, 100);
    auto end = std::chrono::steady_clock::now();

    kernel_->sync->UnlockMutex(mutex);

    EXPECT_EQ(result, BML_RESULT_TIMEOUT);

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_GE(elapsed_ms, 80);  // Allow some tolerance
    EXPECT_LE(elapsed_ms, 200); // But not too long

    kernel_->sync->DestroyCondVar(condvar);
    kernel_->sync->DestroyMutex(mutex);
}

TEST_F(SyncPrimitivesTest, CondVarBroadcast) {
    BML_Mutex mutex = nullptr;
    BML_CondVar condvar = nullptr;

    ASSERT_EQ(kernel_->sync->CreateMutex(&mutex), BML_RESULT_OK);
    ASSERT_EQ(kernel_->sync->CreateCondVar(&condvar), BML_RESULT_OK);

    std::atomic<bool> ready{false};
    std::atomic<int> woken_count{0};
    constexpr int NUM_THREADS = 3;

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&]() {
            kernel_->sync->LockMutex(mutex);
            while (!ready.load()) {
                BML_Result result = kernel_->sync->WaitCondVarTimeout(condvar, mutex, 1000);
                if (result == BML_RESULT_TIMEOUT && !ready.load()) {
                    continue;
                }
            }
            woken_count.fetch_add(1);
            kernel_->sync->UnlockMutex(mutex);
        });
    }

    // Let all threads start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Broadcast to wake all threads
    kernel_->sync->LockMutex(mutex);
    ready.store(true);
    kernel_->sync->BroadcastCondVar(condvar);
    kernel_->sync->UnlockMutex(mutex);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(woken_count.load(), NUM_THREADS);

    kernel_->sync->DestroyCondVar(condvar);
    kernel_->sync->DestroyMutex(mutex);
}

// ============================================================================
// SpinLock Tests
// ============================================================================

TEST_F(SyncPrimitivesTest, SpinLockCreateDestroy) {
    BML_SpinLock lock = nullptr;
    BML_Result result = kernel_->sync->CreateSpinLock(&lock);
    ASSERT_EQ(result, BML_RESULT_OK);
    ASSERT_NE(lock, nullptr);

    kernel_->sync->DestroySpinLock(lock);
}

TEST_F(SyncPrimitivesTest, SpinLockCreateNullOutput) {
    BML_Result result = kernel_->sync->CreateSpinLock(nullptr);
    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(SyncPrimitivesTest, SpinLockBasicLocking) {
    BML_SpinLock lock = nullptr;
    ASSERT_EQ(kernel_->sync->CreateSpinLock(&lock), BML_RESULT_OK);

    kernel_->sync->LockSpinLock(lock);
    // Lock acquired
    kernel_->sync->UnlockSpinLock(lock);
    // Lock released

    kernel_->sync->DestroySpinLock(lock);
}

TEST_F(SyncPrimitivesTest, SpinLockTryLock) {
    BML_SpinLock lock = nullptr;
    ASSERT_EQ(kernel_->sync->CreateSpinLock(&lock), BML_RESULT_OK);

    // Should succeed on unlocked lock
    EXPECT_EQ(kernel_->sync->TryLockSpinLock(lock), BML_TRUE);

    // Try from another thread - should fail
    std::atomic<BML_Bool> try_result{BML_TRUE};
    std::thread t([&]() {
        try_result.store(kernel_->sync->TryLockSpinLock(lock));
    });
    t.join();

    EXPECT_EQ(try_result.load(), BML_FALSE);

    kernel_->sync->UnlockSpinLock(lock);

    // Now should succeed again
    EXPECT_EQ(kernel_->sync->TryLockSpinLock(lock), BML_TRUE);
    kernel_->sync->UnlockSpinLock(lock);

    kernel_->sync->DestroySpinLock(lock);
}

TEST_F(SyncPrimitivesTest, SpinLockConcurrentAccess) {
    BML_SpinLock lock = nullptr;
    ASSERT_EQ(kernel_->sync->CreateSpinLock(&lock), BML_RESULT_OK);

    std::atomic<int> counter{0};
    constexpr int INCREMENTS_PER_THREAD = 1000;
    constexpr int NUM_THREADS = 4;

    auto increment_func = [&]() {
        for (int i = 0; i < INCREMENTS_PER_THREAD; ++i) {
            kernel_->sync->LockSpinLock(lock);
            ++counter;
            kernel_->sync->UnlockSpinLock(lock);
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

    kernel_->sync->DestroySpinLock(lock);
}

// ============================================================================
// Deadlock Detection Tests
// ============================================================================

TEST_F(SyncPrimitivesTest, MutexDeadlockDetectionSetsLastError) {
    BML_Mutex first = nullptr;
    BML_Mutex second = nullptr;
    ASSERT_EQ(kernel_->sync->CreateMutex(&first), BML_RESULT_OK);
    ASSERT_EQ(kernel_->sync->CreateMutex(&second), BML_RESULT_OK);

    auto [code, api_name] = RunDeadlockDetectionScenario(
        first,
        second,
        [this](BML_Mutex mutex) { kernel_->sync->LockMutex(mutex); },
        [this](BML_Mutex mutex) { kernel_->sync->UnlockMutex(mutex); });

    EXPECT_EQ(code, BML_RESULT_SYNC_DEADLOCK);
    EXPECT_EQ(api_name, "bmlMutexLock");

    kernel_->sync->DestroyMutex(first);
    kernel_->sync->DestroyMutex(second);
}

TEST_F(SyncPrimitivesTest, RwLockDeadlockDetectionSetsLastError) {
    BML_RwLock first = nullptr;
    BML_RwLock second = nullptr;
    ASSERT_EQ(kernel_->sync->CreateRwLock(&first), BML_RESULT_OK);
    ASSERT_EQ(kernel_->sync->CreateRwLock(&second), BML_RESULT_OK);

    auto [code, api_name] = RunDeadlockDetectionScenario(
        first,
        second,
        [this](BML_RwLock lock) { kernel_->sync->WriteLockRwLock(lock); },
        [this](BML_RwLock lock) { kernel_->sync->WriteUnlockRwLock(lock); });

    EXPECT_EQ(code, BML_RESULT_SYNC_DEADLOCK);
    EXPECT_EQ(api_name, "bmlRwLockWriteLock");

    kernel_->sync->DestroyRwLock(first);
    kernel_->sync->DestroyRwLock(second);
}

TEST_F(SyncPrimitivesTest, SpinLockDeadlockDetectionSetsLastError) {
    BML_SpinLock first = nullptr;
    BML_SpinLock second = nullptr;
    ASSERT_EQ(kernel_->sync->CreateSpinLock(&first), BML_RESULT_OK);
    ASSERT_EQ(kernel_->sync->CreateSpinLock(&second), BML_RESULT_OK);

    auto [code, api_name] = RunDeadlockDetectionScenario(
        first,
        second,
        [this](BML_SpinLock lock) { kernel_->sync->LockSpinLock(lock); },
        [this](BML_SpinLock lock) { kernel_->sync->UnlockSpinLock(lock); });

    EXPECT_EQ(code, BML_RESULT_SYNC_DEADLOCK);
    EXPECT_EQ(api_name, "bmlSpinLockLock");

    kernel_->sync->DestroySpinLock(first);
    kernel_->sync->DestroySpinLock(second);
}

TEST_F(SyncPrimitivesTest, CondVarWaitDeadlockDetectionWithExtraMutex) {
    BML_Mutex signal_mutex = nullptr;
    BML_Mutex payload_mutex = nullptr;
    BML_CondVar condvar = nullptr;
    ASSERT_EQ(kernel_->sync->CreateMutex(&signal_mutex), BML_RESULT_OK);
    ASSERT_EQ(kernel_->sync->CreateMutex(&payload_mutex), BML_RESULT_OK);
    ASSERT_EQ(kernel_->sync->CreateCondVar(&condvar), BML_RESULT_OK);

    std::atomic<bool> wait_entered{false};
    std::promise<BML_Result> wait_result_promise;
    auto wait_result_future = wait_result_promise.get_future();
    std::promise<DeadlockInfo> contender_result_promise;
    auto contender_future = contender_result_promise.get_future();

    std::thread waiter([&]() {
        BML::Core::ClearLastErrorInfo();
        kernel_->sync->LockMutex(signal_mutex);
        kernel_->sync->LockMutex(payload_mutex);
        wait_entered.store(true, std::memory_order_release);

        BML_Result wait_result = kernel_->sync->WaitCondVarTimeout(condvar, signal_mutex, 200);
        wait_result_promise.set_value(wait_result);

        kernel_->sync->UnlockMutex(payload_mutex);
        kernel_->sync->UnlockMutex(signal_mutex);
    });

    std::thread contender([&]() {
        BML::Core::ClearLastErrorInfo();
        while (!wait_entered.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        kernel_->sync->LockMutex(signal_mutex);
        BML::Core::ClearLastErrorInfo();
        kernel_->sync->LockMutex(payload_mutex);

        BML_ErrorInfo info = BML_ERROR_INFO_INIT;
        DeadlockInfo result{};
        if (BML::Core::GetLastErrorInfo(&info) == BML_RESULT_OK) {
            result.code = info.result_code;
            if (info.api_name) {
                result.api = info.api_name;
            }
        }

        if (result.code != BML_RESULT_SYNC_DEADLOCK) {
            kernel_->sync->UnlockMutex(payload_mutex);
        }

        kernel_->sync->UnlockMutex(signal_mutex);
        contender_result_promise.set_value(std::move(result));
    });

    auto contender_result = contender_future.get();
    auto wait_result = wait_result_future.get();

    waiter.join();
    contender.join();

    EXPECT_EQ(contender_result.code, BML_RESULT_SYNC_DEADLOCK);
    EXPECT_EQ(contender_result.api, "bmlMutexLock");
    // The waiter may observe either the timeout or a spurious wakeup. The
    // deadlock-detection contract under test is that the contender reports the
    // cycle and the waiter does not fail with an unrelated sync error.
    EXPECT_TRUE(wait_result == BML_RESULT_TIMEOUT || wait_result == BML_RESULT_OK);

    kernel_->sync->DestroyCondVar(condvar);
    kernel_->sync->DestroyMutex(payload_mutex);
    kernel_->sync->DestroyMutex(signal_mutex);
}

TEST_F(SyncPrimitivesTest, MutexTimeoutDeadlockDetectionSetsLastError) {
    BML_Mutex first = nullptr;
    BML_Mutex second = nullptr;
    ASSERT_EQ(kernel_->sync->CreateMutex(&first), BML_RESULT_OK);
    ASSERT_EQ(kernel_->sync->CreateMutex(&second), BML_RESULT_OK);

    std::mutex state_mutex;
    std::condition_variable state_cv;
    bool a_has_first = false;
    bool b_has_second = false;
    bool a_attempting_second = false;

    std::promise<DeadlockInfo> result_promise;
    auto result_future = result_promise.get_future();

    std::thread thread_a([&]() {
        kernel_->sync->LockMutex(first);
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

        auto result = kernel_->sync->LockMutexTimeout(second, 1000);
        if (result == BML_RESULT_OK) {
            kernel_->sync->UnlockMutex(second);
        }
        kernel_->sync->UnlockMutex(first);
    });

    std::thread thread_b([&]() {
        BML::Core::ClearLastErrorInfo();
        {
            std::unique_lock<std::mutex> lk(state_mutex);
            state_cv.wait(lk, [&] { return a_has_first; });
        }

        kernel_->sync->LockMutex(second);
        {
            std::lock_guard<std::mutex> guard(state_mutex);
            b_has_second = true;
        }
        state_cv.notify_all();

        {
            std::unique_lock<std::mutex> lk(state_mutex);
            state_cv.wait(lk, [&] { return a_attempting_second; });
        }

        auto lock_result = kernel_->sync->LockMutexTimeout(first, 1000);
        DeadlockInfo info{};
        info.code = lock_result;
        if (lock_result == BML_RESULT_SYNC_DEADLOCK) {
            BML_ErrorInfo err = BML_ERROR_INFO_INIT;
            if (BML::Core::GetLastErrorInfo(&err) == BML_RESULT_OK && err.api_name) {
                info.api = err.api_name;
            }
        }

        if (lock_result == BML_RESULT_OK) {
            kernel_->sync->UnlockMutex(first);
        }
        kernel_->sync->UnlockMutex(second);
        result_promise.set_value(info);
    });

    auto info = result_future.get();
    thread_a.join();
    thread_b.join();

    EXPECT_EQ(info.code, BML_RESULT_SYNC_DEADLOCK);
    EXPECT_EQ(info.api, "bmlMutexLockTimeout");

    kernel_->sync->DestroyMutex(second);
    kernel_->sync->DestroyMutex(first);
}

TEST_F(SyncPrimitivesTest, RwLockWriteTimeoutDeadlockDetectionSetsLastError) {
    BML_RwLock first = nullptr;
    BML_RwLock second = nullptr;
    ASSERT_EQ(kernel_->sync->CreateRwLock(&first), BML_RESULT_OK);
    ASSERT_EQ(kernel_->sync->CreateRwLock(&second), BML_RESULT_OK);

    std::mutex state_mutex;
    std::condition_variable state_cv;
    bool a_has_first = false;
    bool b_has_second = false;
    bool a_attempting_second = false;

    std::promise<DeadlockInfo> result_promise;
    auto result_future = result_promise.get_future();

    std::thread thread_a([&]() {
        kernel_->sync->WriteLockRwLock(first);
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

        auto result = kernel_->sync->WriteLockRwLockTimeout(second, 1000);
        if (result == BML_RESULT_OK) {
            kernel_->sync->WriteUnlockRwLock(second);
        }
        kernel_->sync->WriteUnlockRwLock(first);
    });

    std::thread thread_b([&]() {
        BML::Core::ClearLastErrorInfo();
        {
            std::unique_lock<std::mutex> lk(state_mutex);
            state_cv.wait(lk, [&] { return a_has_first; });
        }

        kernel_->sync->WriteLockRwLock(second);
        {
            std::lock_guard<std::mutex> guard(state_mutex);
            b_has_second = true;
        }
        state_cv.notify_all();

        {
            std::unique_lock<std::mutex> lk(state_mutex);
            state_cv.wait(lk, [&] { return a_attempting_second; });
        }

        auto lock_result = kernel_->sync->WriteLockRwLockTimeout(first, 1000);
        DeadlockInfo info{};
        info.code = lock_result;
        if (lock_result == BML_RESULT_SYNC_DEADLOCK) {
            BML_ErrorInfo err = BML_ERROR_INFO_INIT;
            if (BML::Core::GetLastErrorInfo(&err) == BML_RESULT_OK && err.api_name) {
                info.api = err.api_name;
            }
        }

        if (lock_result == BML_RESULT_OK) {
            kernel_->sync->WriteUnlockRwLock(first);
        }
        kernel_->sync->WriteUnlockRwLock(second);
        result_promise.set_value(info);
    });

    auto info = result_future.get();
    thread_a.join();
    thread_b.join();

    EXPECT_EQ(info.code, BML_RESULT_SYNC_DEADLOCK);
    EXPECT_EQ(info.api, "bmlRwLockWriteLockTimeout");

    kernel_->sync->DestroyRwLock(second);
    kernel_->sync->DestroyRwLock(first);
}

TEST_F(SyncPrimitivesTest, SemaphoreWaitDeadlockDetectionSetsLastError) {
    BML_Semaphore first = nullptr;
    BML_Semaphore second = nullptr;
    ASSERT_EQ(kernel_->sync->CreateSemaphore(1, 1, &first), BML_RESULT_OK);
    ASSERT_EQ(kernel_->sync->CreateSemaphore(1, 1, &second), BML_RESULT_OK);

    std::mutex state_mutex;
    std::condition_variable state_cv;
    bool a_has_first = false;
    bool b_has_second = false;
    bool a_attempting_second = false;

    std::promise<DeadlockInfo> result_promise;
    auto result_future = result_promise.get_future();

    std::thread thread_a([&]() {
        ASSERT_EQ(kernel_->sync->WaitSemaphore(first, BML_TIMEOUT_INFINITE), BML_RESULT_OK);
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

        auto result = kernel_->sync->WaitSemaphore(second, 1000);
        if (result == BML_RESULT_OK) {
            kernel_->sync->SignalSemaphore(second, 1);
        }
        kernel_->sync->SignalSemaphore(first, 1);
    });

    std::thread thread_b([&]() {
        BML::Core::ClearLastErrorInfo();
        {
            std::unique_lock<std::mutex> lk(state_mutex);
            state_cv.wait(lk, [&] { return a_has_first; });
        }

        ASSERT_EQ(kernel_->sync->WaitSemaphore(second, BML_TIMEOUT_INFINITE), BML_RESULT_OK);
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
        auto lock_result = kernel_->sync->WaitSemaphore(first, 1000);

        DeadlockInfo info{};
        info.code = lock_result;
        if (lock_result == BML_RESULT_SYNC_DEADLOCK) {
            BML_ErrorInfo err = BML_ERROR_INFO_INIT;
            if (BML::Core::GetLastErrorInfo(&err) == BML_RESULT_OK && err.api_name) {
                info.api = err.api_name;
            }
        }

        if (lock_result == BML_RESULT_OK) {
            kernel_->sync->SignalSemaphore(first, 1);
        }
        kernel_->sync->SignalSemaphore(second, 1);
        result_promise.set_value(info);
    });

    auto info = result_future.get();
    thread_a.join();
    thread_b.join();

    EXPECT_EQ(info.code, BML_RESULT_SYNC_DEADLOCK);
    EXPECT_EQ(info.api, "bmlSemaphoreWait");

    kernel_->sync->DestroySemaphore(second);
    kernel_->sync->DestroySemaphore(first);
}

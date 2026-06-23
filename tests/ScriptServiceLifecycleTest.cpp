#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

#include "AngelScript/ScriptCommandService.h"
#include "AngelScript/ScriptDataShareService.h"
#include "AngelScript/ScriptTimerService.h"
#include "BML/DataShare.h"
#include "BML/Timer.h"

namespace BML {
namespace Test {

class ScriptServiceLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        Timer::CancelAll();
        Timer::ProcessAll(0, 0.0f);
        BML_DataShare_DestroyAll();
    }

    void TearDown() override {
        Timer::CancelAll();
        Timer::ProcessAll(0, 0.0f);
        BML_DataShare_DestroyAll();
    }
};

TEST_F(ScriptServiceLifecycleTest, TimerReleaseInvalidatesOldRef) {
    int oldCalls = 0;
    int newCalls = 0;

    ScriptTimerService service;
    ScriptTimerRef *ref = service.AddTestTimerForRelease("script-service-old-timer", [&oldCalls]() {
        ++oldCalls;
    });
    ASSERT_NE(nullptr, ref);
    ASSERT_TRUE(ref->IsValid());
    EXPECT_EQ(1u, service.GetActiveCount());

    service.Release();

    ScriptTimerService newService;
    ScriptTimerRef *newRef = newService.AddTestTimerForRelease("script-service-new-timer", [&newCalls]() {
        ++newCalls;
    });
    ASSERT_NE(nullptr, newRef);

    Timer::ProcessAll(1000, 0.0f);

    EXPECT_FALSE(ref->IsValid());
    EXPECT_EQ(0, oldCalls);
    EXPECT_EQ(1, newCalls);
    EXPECT_EQ(0u, service.GetActiveCount());
    ref->Release();
    newRef->Release();
}

TEST_F(ScriptServiceLifecycleTest, TimerReleaseSuppressesOldCallbacksAndRunsNewCallbacksOnce) {
    constexpr int kTimerCount = 8;
    int oldCalls = 0;
    int newCalls = 0;
    std::vector<ScriptTimerRef *> oldRefs;
    std::vector<ScriptTimerRef *> newRefs;

    ScriptTimerService service;
    for (int i = 0; i < kTimerCount; ++i) {
        ScriptTimerRef *ref = service.AddTestTimerForRelease("script-service-old-timer-" + std::to_string(i),
                                                             [&oldCalls]() {
                                                                 ++oldCalls;
                                                             });
        ASSERT_NE(nullptr, ref);
        ASSERT_TRUE(ref->IsValid());
        oldRefs.push_back(ref);
    }
    EXPECT_EQ(static_cast<size_t>(kTimerCount), service.GetActiveCount());

    service.Release();

    ScriptTimerService newService;
    for (int i = 0; i < kTimerCount; ++i) {
        ScriptTimerRef *ref = newService.AddTestTimerForRelease("script-service-new-timer-" + std::to_string(i),
                                                                [&newCalls]() {
                                                                    ++newCalls;
                                                                });
        ASSERT_NE(nullptr, ref);
        ASSERT_TRUE(ref->IsValid());
        newRefs.push_back(ref);
    }

    Timer::ProcessAll(1000, 0.0f);

    EXPECT_EQ(0, oldCalls);
    EXPECT_EQ(kTimerCount, newCalls);
    EXPECT_EQ(0u, service.GetActiveCount());
    for (ScriptTimerRef *ref : oldRefs) {
        EXPECT_FALSE(ref->IsValid());
        ref->Release();
    }
    for (ScriptTimerRef *ref : newRefs)
        ref->Release();
}

TEST_F(ScriptServiceLifecycleTest, CommandReleaseInvalidatesOldRef) {
    ScriptCommandService service;
    ScriptCommandRef *ref = service.AddTestCommandForRelease("script-service-old-command", "ssoc");
    ASSERT_NE(nullptr, ref);
    ASSERT_TRUE(ref->IsValid());
    EXPECT_EQ("script-service-old-command", ref->GetName());
    EXPECT_EQ(1u, service.GetActiveCount());

    service.Release();

    EXPECT_FALSE(ref->IsValid());
    EXPECT_EQ(0u, service.GetActiveCount());
    ref->Release();
}

TEST_F(ScriptServiceLifecycleTest, CommandReleaseSuppressesOldCallbackAndRunsNewCallbackOnce) {
    int oldCalls = 0;
    int newCalls = 0;
    const std::vector<std::string> args = {"arg"};

    ScriptCommandService service;
    ScriptCommandRef *ref = service.AddTestCommandForRelease("script-service-command", "",
                                                             [&oldCalls](const std::vector<std::string> &received) {
                                                                 EXPECT_EQ(1u, received.size());
                                                                 ++oldCalls;
                                                             });
    ASSERT_NE(nullptr, ref);
    EXPECT_TRUE(service.InvokeTestCommandForRelease("script-service-command", args));
    EXPECT_EQ(1, oldCalls);

    service.Release();

    EXPECT_FALSE(service.InvokeTestCommandForRelease("script-service-command", args));
    EXPECT_EQ(1, oldCalls);
    EXPECT_FALSE(ref->IsValid());

    ScriptCommandService newService;
    ScriptCommandRef *newRef = newService.AddTestCommandForRelease("script-service-command", "",
                                                                   [&newCalls](const std::vector<std::string> &received) {
                                                                       EXPECT_EQ(1u, received.size());
                                                                       ++newCalls;
                                                                   });
    ASSERT_NE(nullptr, newRef);
    EXPECT_TRUE(newService.InvokeTestCommandForRelease("script-service-command", args));
    EXPECT_EQ(1, oldCalls);
    EXPECT_EQ(1, newCalls);

    ref->Release();
    newRef->Release();
}

TEST_F(ScriptServiceLifecycleTest, DataShareReleaseInvalidatesOldRefAndSuppressesPendingRequest) {
    ScriptDataShareService service;
    ScriptDataShareRequestRef *ref = service.AddTestRequestForRelease("script-service-old-datashare",
                                                                      ScriptDataShareRequestType::String);
    ASSERT_NE(nullptr, ref);
    ASSERT_TRUE(ref->IsValid());
    EXPECT_EQ(1u, service.GetActiveCount());

    service.Release();

    EXPECT_FALSE(ref->IsValid());
    EXPECT_EQ(0u, service.GetActiveCount());

    BML_DataShare *share = BML_GetDataShare(nullptr);
    ASSERT_NE(nullptr, share);
    const char value[] = "late";
    EXPECT_EQ(1, BML_DataShare_Set(share, "script-service-old-datashare", value, sizeof(value)));
    BML_DataShare_Release(share);

    EXPECT_FALSE(ref->IsValid());
    ref->Release();
}

TEST_F(ScriptServiceLifecycleTest, DataShareReleaseSuppressesOldCallbackAndRunsNewCallbackOnce) {
    int oldCalls = 0;
    int newCalls = 0;
    const char value[] = "late";

    ScriptDataShareService service;
    ScriptDataShareRequestRef *ref = service.AddTestRequestForRelease(
        "script-service-reload-datashare",
        ScriptDataShareRequestType::String,
        [&oldCalls](const ScriptDataShareEventView &) {
            ++oldCalls;
        });
    ASSERT_NE(nullptr, ref);
    ASSERT_TRUE(ref->IsValid());

    service.Release();

    ScriptDataShareService newService;
    ScriptDataShareRequestRef *newRef = newService.AddTestRequestForRelease(
        "script-service-reload-datashare",
        ScriptDataShareRequestType::String,
        [&newCalls](const ScriptDataShareEventView &event) {
            EXPECT_TRUE(event.Exists());
            EXPECT_EQ(std::string("late"), event.GetString());
            ++newCalls;
        });
    ASSERT_NE(nullptr, newRef);
    ASSERT_TRUE(newRef->IsValid());

    BML_DataShare *share = BML_GetDataShare(nullptr);
    ASSERT_NE(nullptr, share);
    EXPECT_EQ(1, BML_DataShare_Set(share, "script-service-reload-datashare", value, sizeof(value)));
    BML_DataShare_Release(share);

    EXPECT_EQ(0, oldCalls);
    EXPECT_EQ(0, newCalls);
    ASSERT_TRUE(newRef->IsValid());
    newService.ProcessQueuedCallbacks();
    EXPECT_EQ(1, newCalls);
    EXPECT_FALSE(ref->IsValid());
    EXPECT_FALSE(newRef->IsValid());

    ref->Release();
    newRef->Release();
}

TEST_F(ScriptServiceLifecycleTest, DataShareCallbackQueuesUntilServiceDrain) {
    int calls = 0;
    const char value[] = "queued";

    ScriptDataShareService service;
    ScriptDataShareRequestRef *ref = service.AddTestRequestForRelease(
        "script-service-queued-datashare",
        ScriptDataShareRequestType::String,
        [&calls](const ScriptDataShareEventView &event) {
            EXPECT_TRUE(event.Exists());
            EXPECT_EQ(std::string("queued"), event.GetString());
            ++calls;
        });
    ASSERT_NE(nullptr, ref);

    BML_DataShare *share = BML_GetDataShare(nullptr);
    ASSERT_NE(nullptr, share);
    EXPECT_EQ(1, BML_DataShare_Set(share, "script-service-queued-datashare", value, sizeof(value)));
    BML_DataShare_Release(share);

    EXPECT_EQ(0, calls);
    EXPECT_TRUE(ref->IsValid());

    service.ProcessQueuedCallbacks();

    EXPECT_EQ(1, calls);
    EXPECT_FALSE(ref->IsValid());
    ref->Release();
}

TEST_F(ScriptServiceLifecycleTest, DataShareReleaseDropsQueuedCallback) {
    int calls = 0;
    const char value[] = "dropped";

    ScriptDataShareService service;
    ScriptDataShareRequestRef *ref = service.AddTestRequestForRelease(
        "script-service-drop-queued-datashare",
        ScriptDataShareRequestType::String,
        [&calls](const ScriptDataShareEventView &) {
            ++calls;
        });
    ASSERT_NE(nullptr, ref);

    BML_DataShare *share = BML_GetDataShare(nullptr);
    ASSERT_NE(nullptr, share);
    EXPECT_EQ(1, BML_DataShare_Set(share, "script-service-drop-queued-datashare", value, sizeof(value)));
    BML_DataShare_Release(share);

    service.Release();
    service.ProcessQueuedCallbacks();

    EXPECT_EQ(0, calls);
    EXPECT_FALSE(ref->IsValid());
    ref->Release();
}

TEST_F(ScriptServiceLifecycleTest, DataShareCancelDropsQueuedCallback) {
    int calls = 0;
    const char value[] = "cancelled";

    ScriptDataShareService service;
    ScriptDataShareRequestRef *ref = service.AddTestRequestForRelease(
        "script-service-cancel-queued-datashare",
        ScriptDataShareRequestType::String,
        [&calls](const ScriptDataShareEventView &) {
            ++calls;
        });
    ASSERT_NE(nullptr, ref);

    BML_DataShare *share = BML_GetDataShare(nullptr);
    ASSERT_NE(nullptr, share);
    EXPECT_EQ(1, BML_DataShare_Set(share, "script-service-cancel-queued-datashare", value, sizeof(value)));
    BML_DataShare_Release(share);

    EXPECT_TRUE(ref->IsValid());
    EXPECT_TRUE(ref->Cancel());
    EXPECT_FALSE(ref->IsValid());

    service.ProcessQueuedCallbacks();

    EXPECT_EQ(0, calls);
    EXPECT_FALSE(ref->IsValid());
    ref->Release();
}

TEST_F(ScriptServiceLifecycleTest, DataShareWorkerThreadTriggerRunsOnServiceDrainThread) {
    int calls = 0;
    std::thread::id triggerThread;
    std::thread::id callbackThread;
    const std::thread::id drainThread = std::this_thread::get_id();
    const char value[] = "worker";

    ScriptDataShareService service;
    ScriptDataShareRequestRef *ref = service.AddTestRequestForRelease(
        "script-service-worker-datashare",
        ScriptDataShareRequestType::String,
        [&](const ScriptDataShareEventView &event) {
            EXPECT_TRUE(event.Exists());
            EXPECT_EQ(std::string("worker"), event.GetString());
            callbackThread = std::this_thread::get_id();
            ++calls;
        });
    ASSERT_NE(nullptr, ref);

    std::thread worker([&]() {
        triggerThread = std::this_thread::get_id();
        BML_DataShare *share = BML_GetDataShare(nullptr);
        ASSERT_NE(nullptr, share);
        EXPECT_EQ(1, BML_DataShare_Set(share, "script-service-worker-datashare", value, sizeof(value)));
        BML_DataShare_Release(share);
    });
    worker.join();

    EXPECT_EQ(0, calls);

    service.ProcessQueuedCallbacks();

    EXPECT_EQ(1, calls);
    EXPECT_EQ(drainThread, callbackThread);
    EXPECT_NE(triggerThread, callbackThread);
    EXPECT_FALSE(ref->IsValid());
    ref->Release();
}

} // namespace Test
} // namespace BML

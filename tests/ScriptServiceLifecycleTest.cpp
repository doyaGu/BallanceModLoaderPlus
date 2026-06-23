#include <gtest/gtest.h>

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

} // namespace Test
} // namespace BML

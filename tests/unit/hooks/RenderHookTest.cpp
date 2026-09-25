#include "Hooks/RenderHook.h"
#include "Hooks/VTables.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

namespace {

CKERROR RenderOriginal(CKRenderContext *, CK_RENDER_FLAGS) {
    return CK_OK;
}

struct FakeRenderContext {
    void **VTable = nullptr;
};

}

TEST(RenderHookTest, TransfersProcessWidePatchOnlyAfterItsOwnerDetaches) {
    using RenderContextVTable = CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext>;
    const std::size_t renderSlot = offsetof(RenderContextVTable, Render) / sizeof(void *);
    std::vector<void *> vtable(renderSlot + 1, reinterpret_cast<void *>(&RenderOriginal));
    FakeRenderContext fake{vtable.data()};
    auto *context = reinterpret_cast<CKRenderContext *>(&fake);

    RenderHook first;
    RenderHook second;
    ASSERT_TRUE(first.Attach(context));
    EXPECT_TRUE(first.IsAttached());
    EXPECT_TRUE(RenderHook::IsSkipRenderAvailable());
    EXPECT_FALSE(second.Attach(context));
    EXPECT_TRUE(second.Detach());

    ASSERT_TRUE(first.Detach());
    EXPECT_FALSE(first.IsAttached());
    EXPECT_FALSE(RenderHook::IsSkipRenderAvailable());
    ASSERT_TRUE(second.Attach(context));
    EXPECT_TRUE(second.IsAttached());
    EXPECT_TRUE(second.Detach());
}

TEST(RenderHookTest, KeepsFourByThreeFieldOfView) {
    const float cameraFov = 1.1f;
    float correctedFov = 0.0f;

    ASSERT_TRUE(RenderHook::CalculateWidescreenFov(cameraFov, 4.0f / 3.0f, &correctedFov));
    EXPECT_FLOAT_EQ(correctedFov, cameraFov);
}

TEST(RenderHookTest, ExpandsHorizontalFieldOfViewForWidescreen) {
    const float cameraFov = 1.1f;
    const float aspectRatio = 16.0f / 9.0f;
    float correctedFov = 0.0f;

    ASSERT_TRUE(RenderHook::CalculateWidescreenFov(cameraFov, aspectRatio, &correctedFov));
    const float expected = 2.0f * std::atan(std::tan(cameraFov * 0.5f) * 0.75f * aspectRatio);
    EXPECT_FLOAT_EQ(correctedFov, expected);
    EXPECT_GT(correctedFov, cameraFov);
}

TEST(RenderHookTest, LeavesNarrowerViewportsUnchanged) {
    const float cameraFov = 1.1f;
    float correctedFov = 0.0f;

    ASSERT_TRUE(RenderHook::CalculateWidescreenFov(cameraFov, 5.0f / 4.0f, &correctedFov));
    EXPECT_FLOAT_EQ(correctedFov, cameraFov);
}

TEST(RenderHookTest, RejectsInvalidProjectionInputs) {
    float correctedFov = 0.0f;

    EXPECT_FALSE(RenderHook::CalculateWidescreenFov(0.0f, 16.0f / 9.0f, &correctedFov));
    EXPECT_FALSE(RenderHook::CalculateWidescreenFov(1.0f, 0.0f, &correctedFov));
    EXPECT_FALSE(RenderHook::CalculateWidescreenFov(1.0f, 16.0f / 9.0f, nullptr));
    EXPECT_FALSE(RenderHook::CalculateWidescreenFov(
        std::numeric_limits<float>::quiet_NaN(), 16.0f / 9.0f, &correctedFov));
}

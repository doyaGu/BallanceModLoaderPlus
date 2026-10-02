#include "Hooks/RenderHook.h"
#include "Hooks/VTables.h"

#include <cstddef>
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
    EXPECT_FALSE(RenderHook::IsWidescreenFixAvailable());
    RenderHook::EnableWidescreenFix(true);
    EXPECT_FALSE(RenderHook::IsWidescreenFixAvailable());
    RenderHook::EnableWidescreenFix(false);
    EXPECT_FALSE(second.Attach(context));
    EXPECT_TRUE(second.Detach());

    ASSERT_TRUE(first.Detach());
    EXPECT_FALSE(first.IsAttached());
    EXPECT_FALSE(RenderHook::IsSkipRenderAvailable());
    ASSERT_TRUE(second.Attach(context));
    EXPECT_TRUE(second.IsAttached());
    EXPECT_TRUE(second.Detach());
}

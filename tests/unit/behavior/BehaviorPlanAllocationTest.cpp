#include "Behavior/Plan.h"

#include <atomic>
#include <cstdlib>
#include <memory>
#include <new>

#include <gtest/gtest.h>

namespace {

std::atomic<std::size_t> g_Allocations{0};
thread_local bool g_CountAllocations = false;

class AllocationScope final {
public:
    AllocationScope() noexcept {
        g_Allocations.store(0, std::memory_order_relaxed);
        g_CountAllocations = true;
    }

    ~AllocationScope() { g_CountAllocations = false; }

    [[nodiscard]] std::size_t Count() const noexcept {
        return g_Allocations.load(std::memory_order_relaxed);
    }
};

class World final : public BML::Behavior::Internal::Plan::World {
public:
    BML::Behavior::Internal::Status
    Install(const BML::Behavior::Internal::PatchKey &,
            const BML::Behavior::Internal::ObjectRef &,
            BML::Behavior::Internal::Epoch,
            BML::Behavior::Internal::Installation &out) override {
        ++Installs;
        out = 1;
        return {};
    }

    BML::Behavior::Internal::Status
    Close(BML::Behavior::Internal::Installation) override {
        ++Closes;
        return {};
    }

    std::size_t Installs = 0;
    std::size_t Closes = 0;
};

TEST(BehaviorPlanAllocation, SettledPlanDoesNoWorkOnUnchangedFrames) {
    using namespace BML::Behavior::Internal;

    std::size_t emptyAllocations = 0;
    {
        Plans empty;
        AllocationScope scope;
        for (std::size_t frame = 0; frame < 4096; ++frame)
            ASSERT_TRUE(empty.ProcessFrame());
        emptyAllocations = scope.Count();
    }

    Plans plans;
    auto world = std::make_shared<World>();
    PlanId plan = 0;
    ASSERT_TRUE(
        plans.Submit({"mod", "events"}, 1, {"Gameplay_Events"}, world, plan));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", ObjectRef{1, 2, 3}));
    ASSERT_TRUE(plans.ProcessFrame());
    ASSERT_EQ(world->Installs, 1u);
    ASSERT_EQ(world->Closes, 0u);

    bool succeeded = true;
    std::size_t allocations = 0;
    {
        AllocationScope scope;
        for (std::size_t frame = 0; frame < 4096; ++frame) {
            if (!plans.ProcessFrame()) {
                succeeded = false;
                break;
            }
        }
        allocations = scope.Count();
    }

    EXPECT_TRUE(succeeded);
#if defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL > 0
    // MSVC's checked map iterators allocate debug proxy state even for an
    // empty Plans collection. A settled Plan must add no work above that
    // toolchain baseline.
    EXPECT_EQ(allocations, emptyAllocations);
#else
    EXPECT_EQ(allocations, 0u);
#endif
    EXPECT_EQ(world->Installs, 1u);
    EXPECT_EQ(world->Closes, 0u);
}

} // namespace

void *operator new(std::size_t size) {
    if (g_CountAllocations)
        g_Allocations.fetch_add(1, std::memory_order_relaxed);
    if (void *memory = std::malloc(size ? size : 1u))
        return memory;
    throw std::bad_alloc();
}

void *operator new[](std::size_t size) { return ::operator new(size); }

void operator delete(void *memory) noexcept { std::free(memory); }

void operator delete[](void *memory) noexcept { std::free(memory); }

void operator delete(void *memory, std::size_t) noexcept { std::free(memory); }

void operator delete[](void *memory, std::size_t) noexcept {
    std::free(memory);
}

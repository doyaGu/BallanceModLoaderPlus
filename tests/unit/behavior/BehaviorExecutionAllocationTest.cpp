#include "Behavior/Execution.h"

#include <atomic>
#include <cstdlib>
#include <new>
#include <vector>

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

class Adapter final : public BML::Behavior::Internal::ExecutionAdapter {
public:
    bool Resolve(const BML::Behavior::Internal::ExecutionInput &input,
                 BML::Behavior::Internal::ResolvedInput &resolved,
                 BML::Behavior::Internal::ExecutionFault &) override {
        resolved.Index = input.Index;
        return true;
    }

    bool Activate(const BML::Behavior::Internal::ResolvedInput &,
                  BML::Behavior::Internal::ExecutionFault &) override {
        return true;
    }

    BML::Behavior::Internal::NativeExecution Execute() override {
        BML::Behavior::Internal::NativeExecution result;
        result.ReturnCode = 0;
        return result;
    }

    bool ReadOutputs(
        std::vector<BML::Behavior::Internal::ExecutionOutput> &,
        BML::Behavior::Internal::ExecutionFault &) override {
        return true;
    }

    bool ReadPouts(std::vector<BML::Behavior::Internal::Pout> &,
                   BML::Behavior::Internal::ExecutionFault &) override {
        return true;
    }

    bool ClearOutputs(
        const std::vector<BML::Behavior::Internal::ExecutionOutput> &,
        BML::Behavior::Internal::ExecutionFault &) override {
        return true;
    }
};

TEST(BehaviorExecutionAllocation, ReusesStorageForSteadyStatePulse) {
    using namespace BML::Behavior::Internal;

    Execution execution(FrameRetention::Ignore());
    Adapter adapter;
    const ExecutionInput input = ExecutionInput::At(0, 1);

    // Populate every Instance-owned buffer before measuring steady state.
    ASSERT_TRUE(execution.Pulse(input, 1, adapter));

    std::size_t stepAllocations = 0;
    {
        AllocationScope scope;
        const ExecutionResult result = execution.Step(2, adapter);
        stepAllocations = scope.Count();
        EXPECT_TRUE(result);
    }

    std::size_t pulseAllocations = 0;
    {
        AllocationScope scope;
        const ExecutionResult result =
            execution.Pulse(input, 3, adapter);
        pulseAllocations = scope.Count();
        EXPECT_TRUE(result);
    }

    // Debug CRT iterator bookkeeping affects both paths equally. A warm
    // single-input Pulse must not allocate beyond an otherwise identical
    // Step once the Instance has established its storage capacity.
#if defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL > 0
    // MSVC's checked std::string allocates iterator-proxy state for the local
    // admission diagnostic even when its message stays empty.
    EXPECT_LE(pulseAllocations, stepAllocations + 2u);
#else
    EXPECT_EQ(pulseAllocations, stepAllocations);
#endif
}

} // namespace

void *operator new(std::size_t size) {
    if (g_CountAllocations)
        g_Allocations.fetch_add(1, std::memory_order_relaxed);
    if (void *memory = std::malloc(size ? size : 1u))
        return memory;
    throw std::bad_alloc();
}

void *operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void *memory) noexcept {
    std::free(memory);
}

void operator delete[](void *memory) noexcept {
    std::free(memory);
}

void operator delete(void *memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void *memory, std::size_t) noexcept {
    std::free(memory);
}

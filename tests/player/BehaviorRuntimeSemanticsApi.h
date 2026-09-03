#ifndef BML_TESTS_PLAYER_BEHAVIORRUNTIMESEMANTICSAPI_H
#define BML_TESTS_PLAYER_BEHAVIORRUNTIMESEMANTICSAPI_H

#include <cstdint>

enum BMLBehaviorRuntimeSemanticsState : std::uint32_t {
    BML_BEHAVIOR_RUNTIME_SEMANTICS_PENDING = 0,
    BML_BEHAVIOR_RUNTIME_SEMANTICS_PASSED = 1,
    BML_BEHAVIOR_RUNTIME_SEMANTICS_FAILED = 2,
};

struct BMLBehaviorRuntimeSemanticsResult {
    std::uint32_t Size = sizeof(BMLBehaviorRuntimeSemanticsResult);
    std::uint32_t Version = 6;
    std::uint32_t State = BML_BEHAVIOR_RUNTIME_SEMANTICS_PENDING;
    std::uint32_t LifecyclePassed = 0;
    std::uint32_t AdditiveEditPassed = 0;
    std::uint32_t RelationsPassed = 0;
    std::uint32_t PhysicsForcePassed = 0;
    std::uint32_t HookErrorPassed = 0;
    std::uint32_t VisualPassed = 0;
    char Detail[96]{};
};

using BMLBehaviorRuntimeSemanticsReadFn = int (__cdecl *)(
    BMLBehaviorRuntimeSemanticsResult *);

#endif // BML_TESTS_PLAYER_BEHAVIORRUNTIMESEMANTICSAPI_H

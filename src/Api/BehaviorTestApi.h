#ifndef BML_API_BEHAVIORTESTAPI_H
#define BML_API_BEHAVIORTESTAPI_H

#include <cstdint>

#include "BML/Behavior.h"

#define BML_BEHAVIOR_TEST_INTERFACE_ID "bml.test.behavior"
#define BML_BEHAVIOR_TEST_INTERFACE_MAJOR 1u
#define BML_BEHAVIOR_TEST_INTERFACE_MINOR 4u

enum BML_BehaviorTestPatchState : std::uint32_t {
    BML_BEHAVIOR_TEST_PATCH_PENDING = 1,
    BML_BEHAVIOR_TEST_PATCH_ACTIVE = 2,
    BML_BEHAVIOR_TEST_PATCH_CLOSING = 3,
    BML_BEHAVIOR_TEST_PATCH_REPAIR_REQUIRED = 4,
    BML_BEHAVIOR_TEST_PATCH_CLOSED = 5,
    BML_BEHAVIOR_TEST_PATCH_FAILED = 6,
};

enum BML_BehaviorTestPlanState : std::uint32_t {
    BML_BEHAVIOR_TEST_PLAN_RECONCILING = 1,
    BML_BEHAVIOR_TEST_PLAN_ACTIVE = 2,
    BML_BEHAVIOR_TEST_PLAN_UNSATISFIED = 3,
    BML_BEHAVIOR_TEST_PLAN_REPAIR_REQUIRED = 4,
    BML_BEHAVIOR_TEST_PLAN_RETIRING = 5,
};

struct BML_BehaviorTestInterface {
    BML_InterfaceHeader Header;
    int (BML_BEHAVIOR_CALL *InstallSplice)(
        BML_BehaviorSession session, void *graph, void *link,
        BML_BehaviorGuid prototype, const char *name,
        std::uintptr_t *patch);
    int (BML_BEHAVIOR_CALL *InstallTextSplice)(
        BML_BehaviorSession session, void *graph, void *link,
        void *target, const char *text, const char *name,
        std::uintptr_t *patch);
    int (BML_BEHAVIOR_CALL *ReadPatch)(
        BML_BehaviorSession session, std::uintptr_t patch,
        std::uint32_t *state);
    int (BML_BEHAVIOR_CALL *ClosePatch)(
        BML_BehaviorSession session, std::uintptr_t patch);
    int (BML_BEHAVIOR_CALL *ResetPatches)(
        BML_BehaviorSession session);
    int (BML_BEHAVIOR_CALL *RetirePatches)(
        BML_BehaviorSession session);
    int (BML_BEHAVIOR_CALL *ObserveScript)(
        BML_BehaviorSession session, void *script);
    int (BML_BEHAVIOR_CALL *SubmitEdit)(
        BML_BehaviorSession session, const char *script,
        const char *sourceNode, const char *sinkNode,
        BML_BehaviorGuid prototype, const char *name,
        std::uintptr_t *plan);
    int (BML_BEHAVIOR_CALL *ReadPlan)(
        BML_BehaviorSession session, std::uintptr_t plan,
        std::uint32_t *state, std::uint32_t *matches,
        std::uint32_t *installations, std::uint64_t *world);
    int (BML_BEHAVIOR_CALL *ClosePlan)(
        BML_BehaviorSession session, std::uintptr_t plan);
    int (BML_BEHAVIOR_CALL *ResetPlans)(
        BML_BehaviorSession session);
    int (BML_BEHAVIOR_CALL *ReadHooks)(
        BML_BehaviorSession session, std::uint32_t *retains,
        std::uint32_t *releases, std::uint32_t *taps,
        std::uint32_t *afters);
};

namespace BML::Api {

const BML_BehaviorTestInterface &BehaviorTestInterface();

} // namespace BML::Api

#endif // BML_API_BEHAVIORTESTAPI_H

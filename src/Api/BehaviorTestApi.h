#ifndef BML_API_BEHAVIORTESTAPI_H
#define BML_API_BEHAVIORTESTAPI_H

#include <cstdint>

#include "BML/Behavior.h"

#define BML_BEHAVIOR_TEST_INTERFACE_ID "bml.test.behavior"
#define BML_BEHAVIOR_TEST_INTERFACE_MAJOR 1u
#define BML_BEHAVIOR_TEST_INTERFACE_MINOR 0u

enum BML_BehaviorTestPatchState : std::uint32_t {
    BML_BEHAVIOR_TEST_PATCH_PENDING = 1,
    BML_BEHAVIOR_TEST_PATCH_ACTIVE = 2,
    BML_BEHAVIOR_TEST_PATCH_CLOSING = 3,
    BML_BEHAVIOR_TEST_PATCH_REPAIR_REQUIRED = 4,
    BML_BEHAVIOR_TEST_PATCH_CLOSED = 5,
    BML_BEHAVIOR_TEST_PATCH_FAILED = 6,
};

struct BML_BehaviorTestInterface {
    BML_InterfaceHeader Header;
    int (BML_BEHAVIOR_CALL *InstallSplice)(
        BML_BehaviorSession session, void *graph, void *link,
        BML_BehaviorGuid prototype, const char *name,
        std::uintptr_t *patch);
    int (BML_BEHAVIOR_CALL *ReadPatch)(
        BML_BehaviorSession session, std::uintptr_t patch,
        std::uint32_t *state);
    int (BML_BEHAVIOR_CALL *ClosePatch)(
        BML_BehaviorSession session, std::uintptr_t patch);
};

namespace BML::Api {

const BML_BehaviorTestInterface &BehaviorTestInterface();

} // namespace BML::Api

#endif // BML_API_BEHAVIORTESTAPI_H

#ifndef BML_TESTS_PLAYER_BEHAVIOR_C_PROBE_H
#define BML_TESTS_PLAYER_BEHAVIOR_C_PROBE_H

#include <BML/Behavior.h>

BML_BEGIN_CDECLS

enum BML_BehaviorCProbeCheck {
    BML_BEHAVIOR_C_PROBE_INTERFACE = 1u << 0,
    BML_BEHAVIOR_C_PROBE_SESSION = 1u << 1,
    BML_BEHAVIOR_C_PROBE_PROTOTYPE = 1u << 2,
    BML_BEHAVIOR_C_PROBE_CALL = 1u << 3,
    BML_BEHAVIOR_C_PROBE_FRAME = 1u << 4,
    BML_BEHAVIOR_C_PROBE_OUT = 1u << 5,
    BML_BEHAVIOR_C_PROBE_CLOSED = 1u << 6,
    BML_BEHAVIOR_C_PROBE_ALL = (1u << 7) - 1u
};

typedef struct BML_BehaviorCProbeResult {
    uint32_t StructSize;
    uint32_t Checks;
    int32_t Code;
    uint32_t Reserved;
    uint64_t Sequence;
    BML_BehaviorStatus Status;
} BML_BehaviorCProbeResult;

// Compiled as C and invoked inside the real Player test Mod. The function
// acquires bml.behavior itself and performs discovery, execution, frame
// transport, and teardown without using the C++ facade or Virtools headers.
int BML_TestBehaviorFromC(BML_BehaviorGuid prototype,
                          BML_BehaviorCProbeResult *result);

BML_END_CDECLS

#endif // BML_TESTS_PLAYER_BEHAVIOR_C_PROBE_H

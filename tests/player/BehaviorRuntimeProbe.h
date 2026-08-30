#ifndef BML_TESTS_PLAYER_BEHAVIORRUNTIMEPROBE_H
#define BML_TESTS_PLAYER_BEHAVIORRUNTIMEPROBE_H

#include <string>

class CK3dObject;
class CKContext;

struct BehaviorRuntimeProbeResult {
    bool Passed = false;
    std::string Detail;
};

BehaviorRuntimeProbeResult RunBehaviorRuntimeProbe(CKContext *context,
                                                   CK3dObject *owner);

#endif // BML_TESTS_PLAYER_BEHAVIORRUNTIMEPROBE_H

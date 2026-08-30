#ifndef BML_TESTS_PLAYER_BEHAVIORRUNTIMEPROBE_H
#define BML_TESTS_PLAYER_BEHAVIORRUNTIMEPROBE_H

#include <memory>
#include <string>

class CK3dObject;
class CKContext;

struct BehaviorRuntimeProbeResult {
    bool Passed = false;
    std::string Detail;
};

class BehaviorRuntimeProbe final {
public:
    BehaviorRuntimeProbe(CKContext *context, CK3dObject *owner);
    ~BehaviorRuntimeProbe();

    BehaviorRuntimeProbe(const BehaviorRuntimeProbe &) = delete;
    BehaviorRuntimeProbe &operator=(const BehaviorRuntimeProbe &) = delete;

    // Called exactly once from each real ExecuteBBTest::OnProcess frame.
    void Advance(int playerFrame);
    [[nodiscard]] bool Done() const;
    [[nodiscard]] BehaviorRuntimeProbeResult Result() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_Impl;
};

#endif // BML_TESTS_PLAYER_BEHAVIORRUNTIMEPROBE_H

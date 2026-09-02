#ifndef BML_TESTS_PLAYER_BEHAVIORRUNTIMESEMANTICS_H
#define BML_TESTS_PLAYER_BEHAVIORRUNTIMESEMANTICS_H

#include <memory>
#include <string>

class CK3dObject;
class CKContext;

struct BehaviorRuntimeSemanticsResult {
    bool Passed = false;
    bool LifecyclePassed = false;
    bool AdditiveEditPassed = false;
    bool PhysicsForcePassed = false;
    std::string Detail;
};

class BehaviorRuntimeSemantics final {
public:
    BehaviorRuntimeSemantics(CKContext *context, CK3dObject *owner);
    ~BehaviorRuntimeSemantics();

    BehaviorRuntimeSemantics(const BehaviorRuntimeSemantics &) = delete;
    BehaviorRuntimeSemantics &operator=(const BehaviorRuntimeSemantics &) = delete;

    // Called exactly once from each fixture OnProcess frame in Ballance Player.
    void Advance(int playerFrame);
    [[nodiscard]] bool Done() const;
    [[nodiscard]] BehaviorRuntimeSemanticsResult Result() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_Impl;
};

#endif // BML_TESTS_PLAYER_BEHAVIORRUNTIMESEMANTICS_H

#ifndef BML_BEHAVIOR_PHYSICSIMPULSE_H
#define BML_BEHAVIOR_PHYSICSIMPULSE_H

#include "Behavior/Runtime.h"

namespace BML::Behavior::PhysicsImpulse {

struct Options {
    CK3dEntity *Target = nullptr;
    VxVector Position{0.0f, 0.0f, 0.0f};
    CK3dEntity *PositionReference = nullptr;
    VxVector Direction{0.0f, 0.0f, 0.0f};
    CK3dEntity *DirectionReference = nullptr;
    float Magnitude = 0.0f;
    CKBOOL DirectionAsPoint = FALSE;
    CKBOOL ConstantForce = FALSE;
};

Spec Make(const Options &options);

} // namespace BML::Behavior::PhysicsImpulse

#endif // BML_BEHAVIOR_PHYSICSIMPULSE_H

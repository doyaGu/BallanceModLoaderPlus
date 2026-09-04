#ifndef BML_BEHAVIOR_PHYSICALIZE_H
#define BML_BEHAVIOR_PHYSICALIZE_H

#include <string>

#include "Behavior/Runtime.h"

namespace BML::Behavior::Physicalize {

struct Options {
    CK3dEntity *Target = nullptr;
    CKBOOL Fixed = FALSE;
    float Friction = 0.7f;
    float Elasticity = 0.4f;
    float Mass = 1.0f;
    std::string CollisionGroup;
    CKBOOL StartFrozen = FALSE;
    CKBOOL EnableCollision = TRUE;
    CKBOOL CalculateMassCenter = FALSE;
    float LinearDamping = 0.1f;
    float RotationalDamping = 0.1f;
    std::string CollisionSurface;
    VxVector MassCenter{0.0f, 0.0f, 0.0f};
};

Spec Convex(const Options &options, CKMesh *mesh = nullptr);
Spec Ball(const Options &options,
          VxVector center = VxVector(0.0f, 0.0f, 0.0f),
          float radius = 2.0f);
Spec Concave(const Options &options, CKMesh *mesh = nullptr);

} // namespace BML::Behavior::Physicalize

#endif // BML_BEHAVIOR_PHYSICALIZE_H

#include "Behavior/PhysicsWakeUp.h"

#include "BML/Guids/physics_RT.h"

namespace BML::Behavior::PhysicsWakeUp {

Spec Make(CK3dEntity *target) {
    Spec spec(PHYSICS_RT_PHYSICSWAKEUP);
    spec.Target(CKPGUID_3DENTITY, target);
    return spec;
}

} // namespace BML::Behavior::PhysicsWakeUp

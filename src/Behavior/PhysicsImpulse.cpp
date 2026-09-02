#include "Behavior/PhysicsImpulse.h"

#include "BML/Guids/physics_RT.h"

namespace BML::Behavior::PhysicsImpulse {

Spec Make(const Options &options) {
    Spec spec(PHYSICS_RT_PHYSICSIMPULSE);
    spec.Target(CKPGUID_3DENTITY, options.Target)
        .Setting(Slot::At(SlotKind::Setting, 0, CKPGUID_BOOL),
                 Value::From(CKPGUID_BOOL, options.DirectionAsPoint))
        .Setting(Slot::At(SlotKind::Setting, 1, CKPGUID_BOOL),
                 Value::From(CKPGUID_BOOL, options.ConstantForce))
        .RefreshLayout()
        .Input(Slot::At(SlotKind::InputParameter, 0, CKPGUID_VECTOR),
               Value::From(CKPGUID_VECTOR, options.Position))
        .Input(Slot::At(SlotKind::InputParameter, 1, CKPGUID_3DENTITY),
               Parameter::Binding::Object(
                   CKPGUID_3DENTITY, options.PositionReference))
        .Input(Slot::At(SlotKind::InputParameter, 2, CKPGUID_VECTOR),
               Value::From(CKPGUID_VECTOR, options.Direction))
        .Input(Slot::At(SlotKind::InputParameter, 3, CKPGUID_3DENTITY),
               Parameter::Binding::Object(
                   CKPGUID_3DENTITY, options.DirectionReference))
        .Input(Slot::At(SlotKind::InputParameter, 4, CKPGUID_FLOAT),
               Value::From(CKPGUID_FLOAT, options.Magnitude));
    return spec;
}

} // namespace BML::Behavior::PhysicsImpulse

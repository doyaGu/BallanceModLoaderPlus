#include "Behavior/Physicalize.h"

#include "BML/Guids/physics_RT.h"

namespace BML::Behavior::Physicalize {
namespace {

Spec Base(const Options &options) {
    Spec spec(PHYSICS_RT_PHYSICALIZE);
    spec.Target(CKPGUID_3DENTITY, options.Target)
        .Input(Slot::At(SlotKind::InputParameter, 0, CKPGUID_BOOL),
               Value::From(CKPGUID_BOOL, options.Fixed))
        .Input(Slot::At(SlotKind::InputParameter, 1, CKPGUID_FLOAT),
               Value::From(CKPGUID_FLOAT, options.Friction))
        .Input(Slot::At(SlotKind::InputParameter, 2, CKPGUID_FLOAT),
               Value::From(CKPGUID_FLOAT, options.Elasticity))
        .Input(Slot::At(SlotKind::InputParameter, 3, CKPGUID_FLOAT),
               Value::From(CKPGUID_FLOAT, options.Mass))
        .Input(Slot::At(SlotKind::InputParameter, 4, CKPGUID_STRING),
               Value::String(options.CollisionGroup))
        .Input(Slot::At(SlotKind::InputParameter, 5, CKPGUID_BOOL),
               Value::From(CKPGUID_BOOL, options.StartFrozen))
        .Input(Slot::At(SlotKind::InputParameter, 6, CKPGUID_BOOL),
               Value::From(CKPGUID_BOOL, options.EnableCollision))
        .Input(Slot::At(SlotKind::InputParameter, 7, CKPGUID_BOOL),
               Value::From(CKPGUID_BOOL, options.CalculateMassCenter))
        .Input(Slot::At(SlotKind::InputParameter, 8, CKPGUID_FLOAT),
               Value::From(CKPGUID_FLOAT, options.LinearDamping))
        .Input(Slot::At(SlotKind::InputParameter, 9, CKPGUID_FLOAT),
               Value::From(CKPGUID_FLOAT, options.RotationalDamping))
        .Input(Slot::At(SlotKind::InputParameter, 10, CKPGUID_STRING),
               Value::String(options.CollisionSurface))
        // The BB exposes settings in the native local array before Mass Center.
        .Local(Slot::At(SlotKind::Local, 3, CKPGUID_VECTOR),
               Value::From(CKPGUID_VECTOR, options.MassCenter));
    return spec;
}

} // namespace

Spec Convex(const Options &options, CKMesh *mesh) {
    Spec spec = Base(options);
    spec.Input(Slot::At(SlotKind::InputParameter, 11, CKPGUID_MESH),
               Value::Object(CKPGUID_MESH, mesh));
    return spec;
}

Spec Ball(const Options &options, VxVector center, float radius) {
    Spec spec = Base(options);
    const int convex = 0;
    const int ball = 1;
    spec.Setting(Slot::At(SlotKind::Setting, 0, CKGUID()),
                 Value::UntypedRaw(&convex, sizeof(convex)))
        .Setting(Slot::At(SlotKind::Setting, 1, CKGUID()),
                 Value::UntypedRaw(&ball, sizeof(ball)))
        .RefreshLayout()
        .Input(Slot::At(SlotKind::InputParameter, 11, CKPGUID_VECTOR),
               Value::From(CKPGUID_VECTOR, center))
        .Input(Slot::At(SlotKind::InputParameter, 12, CKPGUID_FLOAT),
               Value::From(CKPGUID_FLOAT, radius));
    return spec;
}

Spec Concave(const Options &options, CKMesh *mesh) {
    Spec spec = Base(options);
    const int convex = 0;
    const int concave = 1;
    spec.Setting(Slot::At(SlotKind::Setting, 0, CKGUID()),
                 Value::UntypedRaw(&convex, sizeof(convex)))
        .Setting(Slot::At(SlotKind::Setting, 2, CKGUID()),
                 Value::UntypedRaw(&concave, sizeof(concave)))
        .RefreshLayout()
        .Input(Slot::At(SlotKind::InputParameter, 11, CKPGUID_MESH),
               Value::Object(CKPGUID_MESH, mesh));
    return spec;
}

} // namespace BML::Behavior::Physicalize

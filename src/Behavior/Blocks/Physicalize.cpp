#include "Behavior/Blocks/Physicalize.h"

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
        // Physicalize declares Shift Mass Center with DeclareSetting. It is
        // consumed at execution time, but must still travel through the
        // settings lifecycle instead of the ordinary-local shortcut.
        .Setting(Slot::At(SlotKind::Setting, 3, CKPGUID_VECTOR),
                 Value::From(CKPGUID_VECTOR, options.MassCenter));
    return spec;
}

} // namespace

Spec Convex(const Options &options, CKMesh *mesh) {
    Spec spec = Base(options);
    const int convex = 1;
    const int ball = 0;
    const int concave = 0;
    spec.Setting(Slot::At(SlotKind::Setting, 0, CKPGUID_INT),
                 Value::From(CKPGUID_INT, convex))
        .Setting(Slot::At(SlotKind::Setting, 1, CKPGUID_INT),
                 Value::From(CKPGUID_INT, ball))
        .Setting(Slot::At(SlotKind::Setting, 2, CKPGUID_INT),
                 Value::From(CKPGUID_INT, concave))
        .RefreshLayout()
        .Input(Slot::At(SlotKind::InputParameter, 11, CKPGUID_MESH),
               Parameter::Binding::Object(CKPGUID_MESH, mesh));
    return spec;
}

Spec Ball(const Options &options, VxVector center, float radius) {
    Spec spec = Base(options);
    const int convex = 0;
    const int ball = 1;
    const int concave = 0;
    spec.Setting(Slot::At(SlotKind::Setting, 0, CKPGUID_INT),
                 Value::From(CKPGUID_INT, convex))
        .Setting(Slot::At(SlotKind::Setting, 1, CKPGUID_INT),
                 Value::From(CKPGUID_INT, ball))
        .Setting(Slot::At(SlotKind::Setting, 2, CKPGUID_INT),
                 Value::From(CKPGUID_INT, concave))
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
    const int ball = 0;
    const int concave = 1;
    spec.Setting(Slot::At(SlotKind::Setting, 0, CKPGUID_INT),
                 Value::From(CKPGUID_INT, convex))
        .Setting(Slot::At(SlotKind::Setting, 1, CKPGUID_INT),
                 Value::From(CKPGUID_INT, ball))
        .Setting(Slot::At(SlotKind::Setting, 2, CKPGUID_INT),
                 Value::From(CKPGUID_INT, concave))
        .RefreshLayout()
        .Input(Slot::At(SlotKind::InputParameter, 11, CKPGUID_MESH),
               Parameter::Binding::Object(CKPGUID_MESH, mesh));
    return spec;
}

} // namespace BML::Behavior::Physicalize

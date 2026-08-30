#include "Virtools/BallanceBehaviorPresets.h"

#include "BML/Guids/Hooks.h"
#include "BML/Guids/Interface.h"
#include "BML/Guids/Logics.h"
#include "BML/Guids/Narratives.h"
#include "BML/Guids/physics_RT.h"

namespace BML::Virtools::Presets {
namespace {

SlotSelector Pin(int index, CKGUID type) {
    return SlotSelector::At(BehaviorSlotKind::InputParameter, index, type);
}

SlotSelector Setting(int index, CKGUID type) {
    return SlotSelector::At(BehaviorSlotKind::Setting, index, type);
}

SlotSelector Local(int index, CKGUID type) {
    return SlotSelector::At(BehaviorSlotKind::Local, index, type);
}

BehaviorSpec PhysicalizeBase(const PhysicalizeOptions &options) {
    BehaviorSpec spec(PHYSICS_RT_PHYSICALIZE);
    spec.Target(CKPGUID_3DENTITY, options.Target)
        .Input(Pin(0, CKPGUID_BOOL), ParameterValue::Value(CKPGUID_BOOL, options.Fixed))
        .Input(Pin(1, CKPGUID_FLOAT), ParameterValue::Value(CKPGUID_FLOAT, options.Friction))
        .Input(Pin(2, CKPGUID_FLOAT), ParameterValue::Value(CKPGUID_FLOAT, options.Elasticity))
        .Input(Pin(3, CKPGUID_FLOAT), ParameterValue::Value(CKPGUID_FLOAT, options.Mass))
        .Input(Pin(4, CKPGUID_STRING), ParameterValue::String(options.CollisionGroup))
        .Input(Pin(5, CKPGUID_BOOL), ParameterValue::Value(CKPGUID_BOOL, options.StartFrozen))
        .Input(Pin(6, CKPGUID_BOOL), ParameterValue::Value(CKPGUID_BOOL, options.EnableCollision))
        .Input(Pin(7, CKPGUID_BOOL), ParameterValue::Value(CKPGUID_BOOL, options.CalculateMassCenter))
        .Input(Pin(8, CKPGUID_FLOAT), ParameterValue::Value(CKPGUID_FLOAT, options.LinearDamping))
        .Input(Pin(9, CKPGUID_FLOAT), ParameterValue::Value(CKPGUID_FLOAT, options.RotationalDamping))
        .Input(Pin(10, CKPGUID_STRING), ParameterValue::String(options.CollisionSurface))
        .Local(Local(0, CKPGUID_VECTOR), ParameterValue::Value(CKPGUID_VECTOR, options.MassCenter));
    return spec;
}

BehaviorSpec Force(CKGUID prototype, const ForceOptions &options) {
    BehaviorSpec spec(prototype);
    spec.Target(CKPGUID_3DENTITY, options.Target)
        .Input(Pin(0, CKPGUID_VECTOR), ParameterValue::Value(CKPGUID_VECTOR, options.Position))
        .Input(Pin(1, CKPGUID_3DENTITY), ParameterValue::Object(CKPGUID_3DENTITY, options.PositionReference))
        .Input(Pin(2, CKPGUID_VECTOR), ParameterValue::Value(CKPGUID_VECTOR, options.Direction))
        .Input(Pin(3, CKPGUID_3DENTITY), ParameterValue::Object(CKPGUID_3DENTITY, options.DirectionReference))
        .Input(Pin(4, CKPGUID_FLOAT), ParameterValue::Value(CKPGUID_FLOAT, options.Magnitude));
    return spec;
}

} // namespace

BehaviorSpec Text2D(const Text2DOptions &options) {
    BehaviorSpec spec(VT_INTERFACE_2DTEXT);
    spec.Target(CKPGUID_2DENTITY, options.Target)
        .Input(Pin(0, CKPGUID_FONT), ParameterValue::Value(CKPGUID_FONT, options.FontIndex))
        .Input(Pin(1, CKPGUID_STRING), ParameterValue::String(options.Text))
        .Input(Pin(2, CKPGUID_ALIGNMENT), ParameterValue::Value(CKPGUID_ALIGNMENT, options.Alignment))
        .Input(Pin(3, CKPGUID_RECT), ParameterValue::Value(CKPGUID_RECT, options.Margin))
        .Input(Pin(4, CKPGUID_2DVECTOR), ParameterValue::Value(CKPGUID_2DVECTOR, options.Offset))
        .Input(Pin(5, CKPGUID_2DVECTOR), ParameterValue::Value(CKPGUID_2DVECTOR, options.ParagraphIndentation))
        .Input(Pin(6, CKPGUID_MATERIAL), ParameterValue::Object(CKPGUID_MATERIAL, options.BackgroundMaterial))
        .Input(Pin(7, CKPGUID_PERCENTAGE), ParameterValue::Value(CKPGUID_PERCENTAGE, options.CaretSize))
        .Input(Pin(8, CKPGUID_MATERIAL), ParameterValue::Object(CKPGUID_MATERIAL, options.CaretMaterial))
        .Local(Local(0, CKGUID()), ParameterValue::UntypedRaw(&options.Flags, sizeof(options.Flags)));
    return spec;
}

BehaviorSpec PhysicalizeConvex(const PhysicalizeOptions &options, CKMesh *mesh) {
    BehaviorSpec spec = PhysicalizeBase(options);
    spec.Input(Pin(11, CKPGUID_MESH), ParameterValue::Object(CKPGUID_MESH, mesh));
    return spec;
}

BehaviorSpec PhysicalizeBall(const PhysicalizeOptions &options, VxVector center, float radius) {
    BehaviorSpec spec = PhysicalizeBase(options);
    const int convex = 0;
    const int ball = 1;
    spec.Setting(Setting(0, CKGUID()), ParameterValue::UntypedRaw(&convex, sizeof(convex)))
        .Setting(Setting(1, CKGUID()), ParameterValue::UntypedRaw(&ball, sizeof(ball)))
        .RefreshLayout()
        .Input(Pin(11, CKPGUID_VECTOR), ParameterValue::Value(CKPGUID_VECTOR, center))
        .Input(Pin(12, CKPGUID_FLOAT), ParameterValue::Value(CKPGUID_FLOAT, radius));
    return spec;
}

BehaviorSpec PhysicalizeConcave(const PhysicalizeOptions &options, CKMesh *mesh) {
    BehaviorSpec spec = PhysicalizeBase(options);
    const int convex = 0;
    const int concave = 1;
    spec.Setting(Setting(0, CKGUID()), ParameterValue::UntypedRaw(&convex, sizeof(convex)))
        .Setting(Setting(2, CKGUID()), ParameterValue::UntypedRaw(&concave, sizeof(concave)))
        .RefreshLayout()
        .Input(Pin(11, CKPGUID_MESH), ParameterValue::Object(CKPGUID_MESH, mesh));
    return spec;
}

BehaviorSpec PhysicsForce(const ForceOptions &options) {
    return Force(PHYSICS_RT_PHYSICSFORCE, options);
}

BehaviorSpec PhysicsImpulse(const ForceOptions &options) {
    return Force(PHYSICS_RT_PHYSICSIMPULSE, options);
}

BehaviorSpec PhysicsWakeUp(CK3dEntity *target) {
    BehaviorSpec spec(PHYSICS_RT_PHYSICSWAKEUP);
    spec.Target(CKPGUID_3DENTITY, target);
    return spec;
}

BehaviorSpec ObjectLoad(const ObjectLoadOptions &options) {
    BehaviorSpec spec(VT_NARRATIVES_OBJECTLOAD);
    spec.Input(Pin(0, CKPGUID_STRING), ParameterValue::String(options.File))
        .Input(Pin(1, CKPGUID_STRING), ParameterValue::String(options.MasterName))
        .Input(Pin(2, CKPGUID_CLASSID), ParameterValue::Value(CKPGUID_CLASSID, options.FilterClass))
        .Input(Pin(3, CKPGUID_BOOL), ParameterValue::Value(CKPGUID_BOOL, options.AddToScene))
        .Input(Pin(4, CKPGUID_BOOL), ParameterValue::Value(CKPGUID_BOOL, options.ReuseMeshes))
        .Input(Pin(5, CKPGUID_BOOL), ParameterValue::Value(CKPGUID_BOOL, options.ReuseMaterials))
        .Local(Local(0, CKPGUID_BOOL), ParameterValue::Value(CKPGUID_BOOL, options.Dynamic));
    return spec;
}

BehaviorSpec SendMessage(const char *message, CKBeObject *destination) {
    BehaviorSpec spec(VT_LOGICS_SENDMESSAGE);
    spec.Input(Pin(0, CKPGUID_STRING), ParameterValue::String(message ? message : ""))
        .Input(Pin(1, CKPGUID_BEOBJECT), ParameterValue::Object(CKPGUID_BEOBJECT, destination));
    return spec;
}

BehaviorSpec Hook(HookCallback callback, void *argument, int inputCount, int outputCount) {
    BehaviorSpec spec(HOOKS_HOOKBLOCK_GUID);
    if (!callback || inputCount < 0 || outputCount < 0)
        return BehaviorSpec();
    CKBOOL autoActivate = TRUE;
    spec.Local(Local(0, CKPGUID_POINTER), ParameterValue::Value(CKPGUID_POINTER, callback))
        .Local(Local(1, CKPGUID_POINTER), ParameterValue::Value(CKPGUID_POINTER, argument))
        .Local(Local(2, CKPGUID_BOOL), ParameterValue::Value(CKPGUID_BOOL, autoActivate));
    for (int i = 0; i < inputCount; ++i)
        spec.AddInput("In " + std::to_string(i));
    for (int i = 0; i < outputCount; ++i)
        spec.AddOutput("Out " + std::to_string(i));
    return spec;
}

} // namespace BML::Virtools::Presets

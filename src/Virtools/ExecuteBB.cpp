#include "BML/ExecuteBB.h"

#include "Loader/ModContext.h"
#include "UI/GameFontCatalog.h"
#include "Virtools/BallanceBehaviorPresets.h"
#include "Virtools/LegacyExecuteBBAdapter.h"

namespace {

BML::Virtools::LegacyExecuteBBAdapter *GetAdapter() {
    ModContext *context = BML_GetModContext();
    return context ? &context->GetLegacyExecuteBB() : nullptr;
}

BML::GameFont ToGameFont(ExecuteBB::FontType font) {
    const int value = static_cast<int>(font);
    return value >= static_cast<int>(ExecuteBB::NOFONT) &&
           value <= static_cast<int>(ExecuteBB::GAMEFONT_CREDITS_BIG)
        ? static_cast<BML::GameFont>(value)
        : BML::GameFont::None;
}

int ResolveFont(ExecuteBB::FontType font) {
    ModContext *context = BML_GetModContext();
    return context ? context->GetGameFonts().Resolve(ToGameFont(font)) : static_cast<int>(font);
}

BML::Virtools::Presets::PhysicalizeOptions MakePhysicalizeOptions(
    CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
    const char *collisionGroup, CKBOOL startFrozen, CKBOOL enableCollision,
    CKBOOL calculateMassCenter, float linearDamping, float rotationalDamping,
    const char *collisionSurface, VxVector massCenter) {
    BML::Virtools::Presets::PhysicalizeOptions definition;
    definition.Target = target;
    definition.Fixed = fixed;
    definition.Friction = friction;
    definition.Elasticity = elasticity;
    definition.Mass = mass;
    definition.CollisionGroup = collisionGroup ? collisionGroup : "";
    definition.StartFrozen = startFrozen;
    definition.EnableCollision = enableCollision;
    definition.CalculateMassCenter = calculateMassCenter;
    definition.LinearDamping = linearDamping;
    definition.RotationalDamping = rotationalDamping;
    definition.CollisionSurface = collisionSurface ? collisionSurface : "";
    definition.MassCenter = massCenter;
    return definition;
}

BML::Virtools::Presets::ForceOptions MakeForceOptions(
    CK3dEntity *target, VxVector position, CK3dEntity *positionReference,
    VxVector direction, CK3dEntity *directionReference, float magnitude) {
    BML::Virtools::Presets::ForceOptions definition;
    definition.Target = target;
    definition.Position = position;
    definition.PositionReference = positionReference;
    definition.Direction = direction;
    definition.DirectionReference = directionReference;
    definition.Magnitude = magnitude;
    return definition;
}

BML::Virtools::Presets::ObjectLoadOptions MakeObjectLoadOptions(
    const char *file, bool rename, const char *masterName, CK_CLASSID filter,
    CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic) {
    BML::Virtools::Presets::ObjectLoadOptions definition;
    definition.File = file ? file : "";
    definition.Rename = rename;
    definition.MasterName = masterName ? masterName : "";
    definition.FilterClass = filter;
    definition.AddToScene = addToScene;
    definition.ReuseMeshes = reuseMeshes;
    definition.ReuseMaterials = reuseMaterials;
    definition.Dynamic = dynamic;
    return definition;
}

} // namespace

namespace ExecuteBB {

void PhysicalizeConvex(CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
                       const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                       float linearDamp, float rotDamp, const char *collSurface, VxVector massCenter,
                       CKMesh *mesh) {
    if (auto *adapter = GetAdapter())
        adapter->Run(target, BML::Virtools::Presets::PhysicalizeConvex(
            MakePhysicalizeOptions(target, fixed, friction, elasticity, mass, collGroup,
                                   startFrozen, enableColl, calcMassCenter, linearDamp,
                                   rotDamp, collSurface, massCenter), mesh));
}

void PhysicalizeBall(CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
                     const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                     float linearDamp, float rotDamp, const char *collSurface, VxVector massCenter,
                     VxVector ballCenter, float ballRadius) {
    if (auto *adapter = GetAdapter())
        adapter->Run(target, BML::Virtools::Presets::PhysicalizeBall(
            MakePhysicalizeOptions(target, fixed, friction, elasticity, mass, collGroup,
                                   startFrozen, enableColl, calcMassCenter, linearDamp,
                                   rotDamp, collSurface, massCenter), ballCenter, ballRadius));
}

void PhysicalizeConcave(CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
                        const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                        float linearDamp, float rotDamp, const char *collSurface, VxVector massCenter,
                        CKMesh *mesh) {
    if (auto *adapter = GetAdapter())
        adapter->Run(target, BML::Virtools::Presets::PhysicalizeConcave(
            MakePhysicalizeOptions(target, fixed, friction, elasticity, mass, collGroup,
                                   startFrozen, enableColl, calcMassCenter, linearDamp,
                                   rotDamp, collSurface, massCenter), mesh));
}

void Unphysicalize(CK3dEntity *target) {
    if (auto *adapter = GetAdapter())
        adapter->Run(target, BML::Virtools::Presets::PhysicalizeConvex(
            MakePhysicalizeOptions(target, FALSE, 0.7f, 0.4f, 1.0f, "", FALSE,
                                   TRUE, FALSE, 0.1f, 0.1f, "", VxVector())), 1);
}

void SetPhysicsForce(CK3dEntity *target, VxVector position, CK3dEntity *posRef,
                     VxVector direction, CK3dEntity *directionRef, float force) {
    if (auto *adapter = GetAdapter())
        adapter->SetPhysicsForce(MakeForceOptions(target, position, posRef, direction, directionRef, force));
}

void UnsetPhysicsForce(CK3dEntity *target) {
    if (auto *adapter = GetAdapter())
        adapter->UnsetPhysicsForce(target);
}

void PhysicsImpulse(CK3dEntity *target, VxVector position, CK3dEntity *posRef,
                    VxVector direction, CK3dEntity *dirRef, float impulse) {
    if (auto *adapter = GetAdapter())
        adapter->Run(target, BML::Virtools::Presets::PhysicsImpulse(
            MakeForceOptions(target, position, posRef, direction, dirRef, impulse)));
}

void PhysicsWakeUp(CK3dEntity *target) {
    if (auto *adapter = GetAdapter())
        adapter->Run(target, BML::Virtools::Presets::PhysicsWakeUp(target));
}

std::pair<XObjectArray *, CKObject *> ObjectLoad(const char *file, bool rename, const char *mastername,
                                                  CK_CLASSID filter, CKBOOL addToScene, CKBOOL reuseMesh,
                                                  CKBOOL reuseMtl, CKBOOL dynamic) {
    auto *adapter = GetAdapter();
    return adapter
        ? adapter->LoadObjects(MakeObjectLoadOptions(file, rename, mastername, filter,
                                                    addToScene, reuseMesh, reuseMtl, dynamic))
        : std::make_pair(nullptr, nullptr);
}

CKBehavior *Create2DText(CKBehavior *script, CK2dEntity *target, FontType font, const char *text,
                         int align, VxRect margin, Vx2DVector offset, Vx2DVector pindent,
                         CKMaterial *bgmat, float caretsize, CKMaterial *caretmat, int flags) {
    BML::Virtools::Presets::Text2DOptions definition;
    definition.Target = target;
    definition.FontIndex = ResolveFont(font);
    definition.Text = text ? text : "";
    definition.Alignment = align;
    definition.Margin = margin;
    definition.Offset = offset;
    definition.ParagraphIndentation = pindent;
    definition.BackgroundMaterial = bgmat;
    definition.CaretSize = caretsize;
    definition.CaretMaterial = caretmat;
    definition.Flags = flags;
    auto *adapter = GetAdapter();
    return adapter ? adapter->AddToGraph(script, BML::Virtools::Presets::Text2D(definition)) : nullptr;
}

CKBehavior *CreatePhysicalizeConvex(CKBehavior *script, CK3dEntity *target, CKBOOL fixed,
                                    float friction, float elasticity, float mass, const char *collGroup,
                                    CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                                    float linearDamp, float rotDamp, const char *collSurface,
                                    VxVector massCenter, CKMesh *mesh) {
    auto *adapter = GetAdapter();
    return adapter ? adapter->AddToGraph(script, BML::Virtools::Presets::PhysicalizeConvex(
        MakePhysicalizeOptions(target, fixed, friction, elasticity, mass, collGroup,
                               startFrozen, enableColl, calcMassCenter, linearDamp,
                               rotDamp, collSurface, massCenter), mesh)) : nullptr;
}

CKBehavior *CreatePhysicalizeBall(CKBehavior *script, CK3dEntity *target, CKBOOL fixed,
                                  float friction, float elasticity, float mass, const char *collGroup,
                                  CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                                  float linearDamp, float rotDamp, const char *collSurface,
                                  VxVector massCenter, VxVector ballCenter, float ballRadius) {
    auto *adapter = GetAdapter();
    return adapter ? adapter->AddToGraph(script, BML::Virtools::Presets::PhysicalizeBall(
        MakePhysicalizeOptions(target, fixed, friction, elasticity, mass, collGroup,
                               startFrozen, enableColl, calcMassCenter, linearDamp,
                               rotDamp, collSurface, massCenter), ballCenter, ballRadius)) : nullptr;
}

CKBehavior *CreatePhysicalizeConcave(CKBehavior *script, CK3dEntity *target, CKBOOL fixed,
                                     float friction, float elasticity, float mass, const char *collGroup,
                                     CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                                     float linearDamp, float rotDamp, const char *collSurface,
                                     VxVector massCenter, CKMesh *mesh) {
    auto *adapter = GetAdapter();
    return adapter ? adapter->AddToGraph(script, BML::Virtools::Presets::PhysicalizeConcave(
        MakePhysicalizeOptions(target, fixed, friction, elasticity, mass, collGroup,
                               startFrozen, enableColl, calcMassCenter, linearDamp,
                               rotDamp, collSurface, massCenter), mesh)) : nullptr;
}

CKBehavior *CreateSetPhysicsForce(CKBehavior *script, CK3dEntity *target, VxVector position,
                                  CK3dEntity *posRef, VxVector direction,
                                  CK3dEntity *directionRef, float force) {
    auto *adapter = GetAdapter();
    return adapter ? adapter->AddToGraph(script, BML::Virtools::Presets::PhysicsForce(
        MakeForceOptions(target, position, posRef, direction, directionRef, force))) : nullptr;
}

CKBehavior *CreatePhysicsImpulse(CKBehavior *script, CK3dEntity *target, VxVector position,
                                 CK3dEntity *posRef, VxVector direction, CK3dEntity *dirRef,
                                 float impulse) {
    auto *adapter = GetAdapter();
    return adapter ? adapter->AddToGraph(script, BML::Virtools::Presets::PhysicsImpulse(
        MakeForceOptions(target, position, posRef, direction, dirRef, impulse))) : nullptr;
}

CKBehavior *CreatePhysicsWakeUp(CKBehavior *script, CK3dEntity *target) {
    auto *adapter = GetAdapter();
    return adapter ? adapter->AddToGraph(script, BML::Virtools::Presets::PhysicsWakeUp(target)) : nullptr;
}

CKBehavior *CreateObjectLoad(CKBehavior *script, const char *file, const char *mastername,
                             CK_CLASSID filter, CKBOOL addToScene, CKBOOL reuseMesh,
                             CKBOOL reuseMtl, CKBOOL dynamic) {
    auto *adapter = GetAdapter();
    return adapter ? adapter->AddToGraph(script, BML::Virtools::Presets::ObjectLoad(
        MakeObjectLoadOptions(file, false, mastername, filter,
                              addToScene, reuseMesh, reuseMtl, dynamic))) : nullptr;
}

CKBehavior *CreateSendMessage(CKBehavior *script, const char *msg, CKBeObject *dest) {
    auto *adapter = GetAdapter();
    return adapter ? adapter->AddToGraph(script, BML::Virtools::Presets::SendMessage(msg, dest)) : nullptr;
}

CKBehavior *CreateHookBlock(CKBehavior *script, CKBehaviorCallback callback, void *arg,
                            int inCount, int outCount) {
    auto *adapter = GetAdapter();
    return adapter ? adapter->AddToGraph(script, BML::Virtools::Presets::Hook(
        callback, arg, inCount, outCount)) : nullptr;
}

} // namespace ExecuteBB

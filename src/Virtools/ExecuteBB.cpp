#include "BML/ExecuteBB.h"

#include "Loader/ModContext.h"
#include "UI/GameFontCatalog.h"
#include "Virtools/BehaviorGraphRecipes.h"
#include "Virtools/VirtoolsActions.h"

namespace {

BML::VirtoolsActions *GetActions() {
    ModContext *context = BML_GetModContext();
    return context ? &context->GetVirtoolsActions() : nullptr;
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

BML::BehaviorGraphRecipes::PhysicalizeDefinition MakePhysicalizeDefinition(
    CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
    const char *collisionGroup, CKBOOL startFrozen, CKBOOL enableCollision,
    CKBOOL calculateMassCenter, float linearDamping, float rotationalDamping,
    const char *collisionSurface, VxVector massCenter) {
    BML::BehaviorGraphRecipes::PhysicalizeDefinition definition;
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

BML::BehaviorGraphRecipes::ForceDefinition MakeForceDefinition(
    CK3dEntity *target, VxVector position, CK3dEntity *positionReference,
    VxVector direction, CK3dEntity *directionReference, float magnitude) {
    BML::BehaviorGraphRecipes::ForceDefinition definition;
    definition.Target = target;
    definition.Position = position;
    definition.PositionReference = positionReference;
    definition.Direction = direction;
    definition.DirectionReference = directionReference;
    definition.Magnitude = magnitude;
    return definition;
}

BML::BehaviorGraphRecipes::ObjectLoadDefinition MakeObjectLoadDefinition(
    const char *file, bool rename, const char *masterName, CK_CLASSID filter,
    CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic) {
    BML::BehaviorGraphRecipes::ObjectLoadDefinition definition;
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
    if (BML::VirtoolsActions *actions = GetActions()) {
        actions->PhysicalizeConvex(MakePhysicalizeDefinition(
            target, fixed, friction, elasticity, mass, collGroup, startFrozen, enableColl,
            calcMassCenter, linearDamp, rotDamp, collSurface, massCenter), mesh);
    }
}

void PhysicalizeBall(CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
                     const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                     float linearDamp, float rotDamp, const char *collSurface, VxVector massCenter,
                     VxVector ballCenter, float ballRadius) {
    if (BML::VirtoolsActions *actions = GetActions()) {
        actions->PhysicalizeBall(MakePhysicalizeDefinition(
            target, fixed, friction, elasticity, mass, collGroup, startFrozen, enableColl,
            calcMassCenter, linearDamp, rotDamp, collSurface, massCenter), ballCenter, ballRadius);
    }
}

void PhysicalizeConcave(CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
                        const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                        float linearDamp, float rotDamp, const char *collSurface, VxVector massCenter,
                        CKMesh *mesh) {
    if (BML::VirtoolsActions *actions = GetActions()) {
        actions->PhysicalizeConcave(MakePhysicalizeDefinition(
            target, fixed, friction, elasticity, mass, collGroup, startFrozen, enableColl,
            calcMassCenter, linearDamp, rotDamp, collSurface, massCenter), mesh);
    }
}

void Unphysicalize(CK3dEntity *target) {
    if (BML::VirtoolsActions *actions = GetActions())
        actions->Unphysicalize(target);
}

void SetPhysicsForce(CK3dEntity *target, VxVector position, CK3dEntity *posRef,
                     VxVector direction, CK3dEntity *directionRef, float force) {
    if (BML::VirtoolsActions *actions = GetActions())
        actions->SetPhysicsForce(MakeForceDefinition(target, position, posRef, direction, directionRef, force));
}

void UnsetPhysicsForce(CK3dEntity *target) {
    if (BML::VirtoolsActions *actions = GetActions())
        actions->UnsetPhysicsForce(target);
}

void PhysicsImpulse(CK3dEntity *target, VxVector position, CK3dEntity *posRef,
                    VxVector direction, CK3dEntity *dirRef, float impulse) {
    if (BML::VirtoolsActions *actions = GetActions())
        actions->PhysicsImpulse(MakeForceDefinition(target, position, posRef, direction, dirRef, impulse));
}

void PhysicsWakeUp(CK3dEntity *target) {
    if (BML::VirtoolsActions *actions = GetActions())
        actions->PhysicsWakeUp(target);
}

std::pair<XObjectArray *, CKObject *> ObjectLoad(const char *file, bool rename, const char *mastername,
                                                  CK_CLASSID filter, CKBOOL addToScene, CKBOOL reuseMesh,
                                                  CKBOOL reuseMtl, CKBOOL dynamic) {
    BML::VirtoolsActions *actions = GetActions();
    return actions
        ? BML::LegacyVirtoolsActions::LoadObjects(
              *actions, MakeObjectLoadDefinition(file, rename, mastername, filter,
                                                 addToScene, reuseMesh, reuseMtl, dynamic))
        : std::make_pair(nullptr, nullptr);
}

CKBehavior *Create2DText(CKBehavior *script, CK2dEntity *target, FontType font, const char *text,
                         int align, VxRect margin, Vx2DVector offset, Vx2DVector pindent,
                         CKMaterial *bgmat, float caretsize, CKMaterial *caretmat, int flags) {
    BML::BehaviorGraphRecipes::Text2DDefinition definition;
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
    return BML::BehaviorGraphRecipes::Add2DText(script, definition);
}

CKBehavior *CreatePhysicalizeConvex(CKBehavior *script, CK3dEntity *target, CKBOOL fixed,
                                    float friction, float elasticity, float mass, const char *collGroup,
                                    CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                                    float linearDamp, float rotDamp, const char *collSurface,
                                    VxVector massCenter, CKMesh *mesh) {
    return BML::BehaviorGraphRecipes::AddPhysicalizeConvex(
        script, MakePhysicalizeDefinition(target, fixed, friction, elasticity, mass, collGroup,
                                          startFrozen, enableColl, calcMassCenter, linearDamp,
                                          rotDamp, collSurface, massCenter), mesh);
}

CKBehavior *CreatePhysicalizeBall(CKBehavior *script, CK3dEntity *target, CKBOOL fixed,
                                  float friction, float elasticity, float mass, const char *collGroup,
                                  CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                                  float linearDamp, float rotDamp, const char *collSurface,
                                  VxVector massCenter, VxVector ballCenter, float ballRadius) {
    return BML::BehaviorGraphRecipes::AddPhysicalizeBall(
        script, MakePhysicalizeDefinition(target, fixed, friction, elasticity, mass, collGroup,
                                          startFrozen, enableColl, calcMassCenter, linearDamp,
                                          rotDamp, collSurface, massCenter), ballCenter, ballRadius);
}

CKBehavior *CreatePhysicalizeConcave(CKBehavior *script, CK3dEntity *target, CKBOOL fixed,
                                     float friction, float elasticity, float mass, const char *collGroup,
                                     CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                                     float linearDamp, float rotDamp, const char *collSurface,
                                     VxVector massCenter, CKMesh *mesh) {
    return BML::BehaviorGraphRecipes::AddPhysicalizeConcave(
        script, MakePhysicalizeDefinition(target, fixed, friction, elasticity, mass, collGroup,
                                          startFrozen, enableColl, calcMassCenter, linearDamp,
                                          rotDamp, collSurface, massCenter), mesh);
}

CKBehavior *CreateSetPhysicsForce(CKBehavior *script, CK3dEntity *target, VxVector position,
                                  CK3dEntity *posRef, VxVector direction,
                                  CK3dEntity *directionRef, float force) {
    return BML::BehaviorGraphRecipes::AddPhysicsForce(
        script, MakeForceDefinition(target, position, posRef, direction, directionRef, force));
}

CKBehavior *CreatePhysicsImpulse(CKBehavior *script, CK3dEntity *target, VxVector position,
                                 CK3dEntity *posRef, VxVector direction, CK3dEntity *dirRef,
                                 float impulse) {
    return BML::BehaviorGraphRecipes::AddPhysicsImpulse(
        script, MakeForceDefinition(target, position, posRef, direction, dirRef, impulse));
}

CKBehavior *CreatePhysicsWakeUp(CKBehavior *script, CK3dEntity *target) {
    return BML::BehaviorGraphRecipes::AddPhysicsWakeUp(script, target);
}

CKBehavior *CreateObjectLoad(CKBehavior *script, const char *file, const char *mastername,
                             CK_CLASSID filter, CKBOOL addToScene, CKBOOL reuseMesh,
                             CKBOOL reuseMtl, CKBOOL dynamic) {
    return BML::BehaviorGraphRecipes::AddObjectLoad(
        script, MakeObjectLoadDefinition(file, false, mastername, filter,
                                         addToScene, reuseMesh, reuseMtl, dynamic));
}

CKBehavior *CreateSendMessage(CKBehavior *script, const char *msg, CKBeObject *dest) {
    return BML::BehaviorGraphRecipes::AddSendMessage(script, msg, dest);
}

CKBehavior *CreateHookBlock(CKBehavior *script, CKBehaviorCallback callback, void *arg,
                            int inCount, int outCount) {
    return BML::BehaviorGraphRecipes::AddHookBlock(script, callback, arg, inCount, outCount);
}

} // namespace ExecuteBB

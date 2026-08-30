#include "BML/ExecuteBB.h"

#include "Loader/ModContext.h"
#include "BML/ILogger.h"
#include "UI/GameFontCatalog.h"
#include "Api/ExecuteBBAdapter.h"
#include "Behavior/HookBlock.h"
#include "Behavior/ObjectLoad.h"
#include "Behavior/Physicalize.h"
#include "Behavior/PhysicsForce.h"
#include "Behavior/PhysicsImpulse.h"
#include "Behavior/PhysicsWakeUp.h"
#include "Behavior/SendMessage.h"
#include "Behavior/Text2D.h"

namespace {

BML::ExecuteBBAdapter *GetAdapter() {
    ModContext *context = BML_GetModContext();
    return context ? &context->ExecuteBB() : nullptr;
}

void ReportFailure(const char *operation,
                         const BML::Behavior::RunResult &result) {
    if (result)
        return;
    ModContext *context = BML_GetModContext();
    ILogger *logger = context ? context->GetLogger() : nullptr;
    if (!logger)
        return;
    const BML::Behavior::Status &status = result.Outcome;
    logger->Error(
        "ExecuteBB::%s failed: error=%s phase=%s ck_error=%d behavior_result=%d message=%s",
        operation ? operation : "<unknown>",
        BML::Behavior::DescribeError(status.Code),
        BML::Behavior::DescribePhase(status.Details.Stage),
        static_cast<int>(status.CkError), status.BehaviorResult,
        status.Message.empty() ? "<none>" : status.Message.c_str());
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

BML::Behavior::Physicalize::Options MakePhysicalizeOptions(
    CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
    const char *collisionGroup, CKBOOL startFrozen, CKBOOL enableCollision,
    CKBOOL calculateMassCenter, float linearDamping, float rotationalDamping,
    const char *collisionSurface, VxVector massCenter) {
    BML::Behavior::Physicalize::Options definition;
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

template <typename Options>
Options MakeForceOptions(
    CK3dEntity *target, VxVector position, CK3dEntity *positionReference,
    VxVector direction, CK3dEntity *directionReference, float magnitude) {
    Options definition;
    definition.Target = target;
    definition.Position = position;
    definition.PositionReference = positionReference;
    definition.Direction = direction;
    definition.DirectionReference = directionReference;
    definition.Magnitude = magnitude;
    return definition;
}

BML::Behavior::ObjectLoad::Options MakeObjectLoadOptions(
    const char *file, const char *masterName, CK_CLASSID filter,
    CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic) {
    BML::Behavior::ObjectLoad::Options definition;
    definition.File = file ? file : "";
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
    if (auto *adapter = GetAdapter()) {
        const auto result = adapter->Run(target, BML::Behavior::Physicalize::Convex(
            MakePhysicalizeOptions(target, fixed, friction, elasticity, mass, collGroup,
                                   startFrozen, enableColl, calcMassCenter, linearDamp,
                                   rotDamp, collSurface, massCenter), mesh));
        ReportFailure("PhysicalizeConvex", result);
    }
}

void PhysicalizeBall(CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
                     const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                     float linearDamp, float rotDamp, const char *collSurface, VxVector massCenter,
                     VxVector ballCenter, float ballRadius) {
    if (auto *adapter = GetAdapter()) {
        const auto result = adapter->Run(target, BML::Behavior::Physicalize::Ball(
            MakePhysicalizeOptions(target, fixed, friction, elasticity, mass, collGroup,
                                   startFrozen, enableColl, calcMassCenter, linearDamp,
                                   rotDamp, collSurface, massCenter), ballCenter, ballRadius));
        ReportFailure("PhysicalizeBall", result);
    }
}

void PhysicalizeConcave(CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
                        const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                        float linearDamp, float rotDamp, const char *collSurface, VxVector massCenter,
                        CKMesh *mesh) {
    if (auto *adapter = GetAdapter()) {
        const auto result = adapter->Run(target, BML::Behavior::Physicalize::Concave(
            MakePhysicalizeOptions(target, fixed, friction, elasticity, mass, collGroup,
                                   startFrozen, enableColl, calcMassCenter, linearDamp,
                                   rotDamp, collSurface, massCenter), mesh));
        ReportFailure("PhysicalizeConcave", result);
    }
}

void Unphysicalize(CK3dEntity *target) {
    if (auto *adapter = GetAdapter()) {
        const auto result = adapter->Run(target, BML::Behavior::Physicalize::Convex(
            MakePhysicalizeOptions(target, FALSE, 0.7f, 0.4f, 1.0f, "", FALSE,
                                   TRUE, FALSE, 0.1f, 0.1f, "", VxVector())), 1);
        ReportFailure("Unphysicalize", result);
    }
}

void SetPhysicsForce(CK3dEntity *target, VxVector position, CK3dEntity *posRef,
                     VxVector direction, CK3dEntity *directionRef, float force) {
    if (auto *adapter = GetAdapter()) {
        const auto result = adapter->SetPhysicsForce(
            MakeForceOptions<BML::Behavior::PhysicsForce::Options>(
                target, position, posRef, direction, directionRef, force));
        ReportFailure("SetPhysicsForce", result);
    }
}

void UnsetPhysicsForce(CK3dEntity *target) {
    if (auto *adapter = GetAdapter()) {
        const auto result = adapter->UnsetPhysicsForce(target);
        ReportFailure("UnsetPhysicsForce", result);
    }
}

void PhysicsImpulse(CK3dEntity *target, VxVector position, CK3dEntity *posRef,
                    VxVector direction, CK3dEntity *dirRef, float impulse) {
    if (auto *adapter = GetAdapter()) {
        const auto result = adapter->Run(target, BML::Behavior::PhysicsImpulse::Make(
            MakeForceOptions<BML::Behavior::PhysicsImpulse::Options>(
                target, position, posRef, direction, dirRef, impulse)));
        ReportFailure("PhysicsImpulse", result);
    }
}

void PhysicsWakeUp(CK3dEntity *target) {
    if (auto *adapter = GetAdapter()) {
        const auto result = adapter->Run(target, BML::Behavior::PhysicsWakeUp::Make(target));
        ReportFailure("PhysicsWakeUp", result);
    }
}

std::pair<XObjectArray *, CKObject *> ObjectLoad(const char *file, bool rename, const char *mastername,
                                                  CK_CLASSID filter, CKBOOL addToScene, CKBOOL reuseMesh,
                                                  CKBOOL reuseMtl, CKBOOL dynamic) {
    auto *adapter = GetAdapter();
    return adapter
        ? adapter->LoadObjects(MakeObjectLoadOptions(file, mastername, filter,
                                                    addToScene, reuseMesh, reuseMtl, dynamic),
                               rename)
        : std::make_pair(nullptr, nullptr);
}

CKBehavior *Create2DText(CKBehavior *script, CK2dEntity *target, FontType font, const char *text,
                         int align, VxRect margin, Vx2DVector offset, Vx2DVector pindent,
                         CKMaterial *bgmat, float caretsize, CKMaterial *caretmat, int flags) {
    BML::Behavior::Text2D::Options definition;
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
    return adapter ? adapter->AddToGraph(script, BML::Behavior::Text2D::Make(definition)) : nullptr;
}

CKBehavior *CreatePhysicalizeConvex(CKBehavior *script, CK3dEntity *target, CKBOOL fixed,
                                    float friction, float elasticity, float mass, const char *collGroup,
                                    CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                                    float linearDamp, float rotDamp, const char *collSurface,
                                    VxVector massCenter, CKMesh *mesh) {
    auto *adapter = GetAdapter();
    return adapter ? adapter->AddToGraph(script, BML::Behavior::Physicalize::Convex(
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
    return adapter ? adapter->AddToGraph(script, BML::Behavior::Physicalize::Ball(
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
    return adapter ? adapter->AddToGraph(script, BML::Behavior::Physicalize::Concave(
        MakePhysicalizeOptions(target, fixed, friction, elasticity, mass, collGroup,
                               startFrozen, enableColl, calcMassCenter, linearDamp,
                               rotDamp, collSurface, massCenter), mesh)) : nullptr;
}

CKBehavior *CreateSetPhysicsForce(CKBehavior *script, CK3dEntity *target, VxVector position,
                                  CK3dEntity *posRef, VxVector direction,
                                  CK3dEntity *directionRef, float force) {
    auto *adapter = GetAdapter();
    return adapter ? adapter->AddToGraph(script, BML::Behavior::PhysicsForce::Make(
        MakeForceOptions<BML::Behavior::PhysicsForce::Options>(
            target, position, posRef, direction, directionRef, force))) : nullptr;
}

CKBehavior *CreatePhysicsImpulse(CKBehavior *script, CK3dEntity *target, VxVector position,
                                 CK3dEntity *posRef, VxVector direction, CK3dEntity *dirRef,
                                 float impulse) {
    auto *adapter = GetAdapter();
    return adapter ? adapter->AddToGraph(script, BML::Behavior::PhysicsImpulse::Make(
        MakeForceOptions<BML::Behavior::PhysicsImpulse::Options>(
            target, position, posRef, direction, dirRef, impulse))) : nullptr;
}

CKBehavior *CreatePhysicsWakeUp(CKBehavior *script, CK3dEntity *target) {
    auto *adapter = GetAdapter();
    return adapter ? adapter->AddToGraph(
        script, BML::Behavior::PhysicsWakeUp::Make(target)) : nullptr;
}

CKBehavior *CreateObjectLoad(CKBehavior *script, const char *file, const char *mastername,
                             CK_CLASSID filter, CKBOOL addToScene, CKBOOL reuseMesh,
                             CKBOOL reuseMtl, CKBOOL dynamic) {
    auto *adapter = GetAdapter();
    return adapter ? adapter->AddToGraph(script, BML::Behavior::ObjectLoad::Make(
        MakeObjectLoadOptions(file, mastername, filter,
                              addToScene, reuseMesh, reuseMtl, dynamic))) : nullptr;
}

CKBehavior *CreateSendMessage(CKBehavior *script, const char *msg, CKBeObject *dest) {
    auto *adapter = GetAdapter();
    return adapter ? adapter->AddToGraph(
        script, BML::Behavior::SendMessage::Make(msg, dest)) : nullptr;
}

CKBehavior *CreateHookBlock(CKBehavior *script, CKBehaviorCallback callback, void *arg,
                            int inCount, int outCount) {
    auto *adapter = GetAdapter();
    return adapter ? adapter->AddToGraph(script, BML::Behavior::HookBlock::Make(
        callback, arg, inCount, outCount)) : nullptr;
}

} // namespace ExecuteBB

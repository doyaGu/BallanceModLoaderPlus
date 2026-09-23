#include "BML/ExecuteBB.h"

#include "Loader/ModContext.h"
#include "BML/ILogger.h"
#include "UI/GameFontCatalog.h"
#include "Api/ExecuteBBAdapter.h"
#include "Behavior/Blocks/HookBlock.h"
#include "Behavior/Block.h"
#include "BML/Behavior/Blocks.hpp"

namespace {

BML::ExecuteBBAdapter *GetAdapter(const ModContextLease &context) {
    return context ? &context->ExecuteBB() : nullptr;
}

void ReportFailure(const char *operation,
                         const BML::Behavior::Internal::RunResult &result) {
    if (result)
        return;
    ModContextLease context;
    ILogger *logger = context ? context->GetLogger() : nullptr;
    if (!logger)
        return;
    const BML::Behavior::Internal::Status &status = result.Detail;
    logger->Error(
        "ExecuteBB::%s failed: error=%s phase=%s ck_error=%d behavior_result=%d message=%s",
        operation ? operation : "<unknown>",
        BML::Behavior::Internal::DescribeError(status.Code),
        BML::Behavior::Internal::DescribePhase(status.Details.Stage),
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
    ModContextLease context;
    return context ? context->GetGameFonts().Resolve(ToGameFont(font)) : static_cast<int>(font);
}

BML::Behavior::Blocks::Physicalize::Options MakePhysicalizeOptions(
    CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
    const char *collisionGroup, CKBOOL startFrozen, CKBOOL enableCollision,
    CKBOOL calculateMassCenter, float linearDamping, float rotationalDamping,
    const char *collisionSurface, VxVector massCenter) {
    BML::Behavior::Blocks::Physicalize::Options definition;
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

BML::Behavior::Blocks::ObjectLoad::Options MakeObjectLoadOptions(
    const char *file, const char *masterName, CK_CLASSID filter,
    CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic) {
    BML::Behavior::Blocks::ObjectLoad::Options definition;
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
    ModContextLease context;
    if (auto *adapter = GetAdapter(context)) {
        auto options = MakePhysicalizeOptions(
            target, fixed, friction, elasticity, mass, collGroup, startFrozen,
            enableColl, calcMassCenter, linearDamp, rotDamp, collSurface,
            massCenter);
        options.Geometry = BML::Behavior::Blocks::Physicalize::Shape::Convex;
        options.Mesh = mesh;
        const auto result = adapter->Run(
            target, BML::Behavior::Internal::BlockSpec::From(options));
        ReportFailure("PhysicalizeConvex", result);
    }
}

void PhysicalizeBall(CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
                     const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                     float linearDamp, float rotDamp, const char *collSurface, VxVector massCenter,
                     VxVector ballCenter, float ballRadius) {
    ModContextLease context;
    if (auto *adapter = GetAdapter(context)) {
        auto options = MakePhysicalizeOptions(
            target, fixed, friction, elasticity, mass, collGroup, startFrozen,
            enableColl, calcMassCenter, linearDamp, rotDamp, collSurface,
            massCenter);
        options.Geometry = BML::Behavior::Blocks::Physicalize::Shape::Ball;
        options.Center = ballCenter;
        options.Radius = ballRadius;
        const auto result = adapter->Run(
            target, BML::Behavior::Internal::BlockSpec::From(options));
        ReportFailure("PhysicalizeBall", result);
    }
}

void PhysicalizeConcave(CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
                        const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                        float linearDamp, float rotDamp, const char *collSurface, VxVector massCenter,
                        CKMesh *mesh) {
    ModContextLease context;
    if (auto *adapter = GetAdapter(context)) {
        auto options = MakePhysicalizeOptions(
            target, fixed, friction, elasticity, mass, collGroup, startFrozen,
            enableColl, calcMassCenter, linearDamp, rotDamp, collSurface,
            massCenter);
        options.Geometry = BML::Behavior::Blocks::Physicalize::Shape::Concave;
        options.Mesh = mesh;
        const auto result = adapter->Run(
            target, BML::Behavior::Internal::BlockSpec::From(options));
        ReportFailure("PhysicalizeConcave", result);
    }
}

void Unphysicalize(CK3dEntity *target) {
    ModContextLease context;
    if (auto *adapter = GetAdapter(context)) {
        auto options = MakePhysicalizeOptions(
            target, FALSE, 0.7f, 0.4f, 1.0f, "", FALSE, TRUE, FALSE,
            0.1f, 0.1f, "", VxVector());
        const auto result = adapter->Run(
            target, BML::Behavior::Internal::BlockSpec::From(options), 1);
        ReportFailure("Unphysicalize", result);
    }
}

void SetPhysicsForce(CK3dEntity *target, VxVector position, CK3dEntity *posRef,
                     VxVector direction, CK3dEntity *directionRef, float force) {
    ModContextLease context;
    if (auto *adapter = GetAdapter(context)) {
        const auto result = adapter->SetPhysicsForce(
            MakeForceOptions<BML::Behavior::Blocks::PhysicsForce::Options>(
                target, position, posRef, direction, directionRef, force));
        ReportFailure("SetPhysicsForce", result);
    }
}

void UnsetPhysicsForce(CK3dEntity *target) {
    ModContextLease context;
    if (auto *adapter = GetAdapter(context)) {
        const auto result = adapter->UnsetPhysicsForce(target);
        ReportFailure("UnsetPhysicsForce", result);
    }
}

void PhysicsImpulse(CK3dEntity *target, VxVector position, CK3dEntity *posRef,
                    VxVector direction, CK3dEntity *dirRef, float impulse) {
    ModContextLease context;
    if (auto *adapter = GetAdapter(context)) {
        const auto result = adapter->Run(target, BML::Behavior::Internal::BlockSpec::From(
            MakeForceOptions<BML::Behavior::Blocks::PhysicsImpulse::Options>(
                target, position, posRef, direction, dirRef, impulse)));
        ReportFailure("PhysicsImpulse", result);
    }
}

void PhysicsWakeUp(CK3dEntity *target) {
    ModContextLease context;
    if (auto *adapter = GetAdapter(context)) {
        const auto result = adapter->Run(
            target, BML::Behavior::Internal::BlockSpec::From(
                BML::Behavior::Blocks::PhysicsWakeUp::Options{target}));
        ReportFailure("PhysicsWakeUp", result);
    }
}

std::pair<XObjectArray *, CKObject *> ObjectLoad(const char *file, bool rename, const char *mastername,
                                                  CK_CLASSID filter, CKBOOL addToScene, CKBOOL reuseMesh,
                                                  CKBOOL reuseMtl, CKBOOL dynamic) {
    ModContextLease context;
    auto *adapter = GetAdapter(context);
    return adapter
        ? adapter->LoadObjects(MakeObjectLoadOptions(file, mastername, filter,
                                                    addToScene, reuseMesh, reuseMtl, dynamic),
                               rename)
        : std::make_pair(nullptr, nullptr);
}

CKBehavior *Create2DText(CKBehavior *script, CK2dEntity *target, FontType font, const char *text,
                         int align, VxRect margin, Vx2DVector offset, Vx2DVector pindent,
                         CKMaterial *bgmat, float caretsize, CKMaterial *caretmat, int flags) {
    BML::Behavior::Blocks::Text2D::Options definition;
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
    ModContextLease context;
    auto *adapter = GetAdapter(context);
    return adapter ? adapter->AddToGraph(
        script, BML::Behavior::Internal::BlockSpec::From(definition)) : nullptr;
}

CKBehavior *CreatePhysicalizeConvex(CKBehavior *script, CK3dEntity *target, CKBOOL fixed,
                                    float friction, float elasticity, float mass, const char *collGroup,
                                    CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                                    float linearDamp, float rotDamp, const char *collSurface,
                                    VxVector massCenter, CKMesh *mesh) {
    ModContextLease context;
    auto *adapter = GetAdapter(context);
    auto options = MakePhysicalizeOptions(
        target, fixed, friction, elasticity, mass, collGroup, startFrozen,
        enableColl, calcMassCenter, linearDamp, rotDamp, collSurface,
        massCenter);
    options.Geometry = BML::Behavior::Blocks::Physicalize::Shape::Convex;
    options.Mesh = mesh;
    return adapter ? adapter->AddToGraph(
        script, BML::Behavior::Internal::BlockSpec::From(options)) : nullptr;
}

CKBehavior *CreatePhysicalizeBall(CKBehavior *script, CK3dEntity *target, CKBOOL fixed,
                                  float friction, float elasticity, float mass, const char *collGroup,
                                  CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                                  float linearDamp, float rotDamp, const char *collSurface,
                                  VxVector massCenter, VxVector ballCenter, float ballRadius) {
    ModContextLease context;
    auto *adapter = GetAdapter(context);
    auto options = MakePhysicalizeOptions(
        target, fixed, friction, elasticity, mass, collGroup, startFrozen,
        enableColl, calcMassCenter, linearDamp, rotDamp, collSurface,
        massCenter);
    options.Geometry = BML::Behavior::Blocks::Physicalize::Shape::Ball;
    options.Center = ballCenter;
    options.Radius = ballRadius;
    return adapter ? adapter->AddToGraph(
        script, BML::Behavior::Internal::BlockSpec::From(options)) : nullptr;
}

CKBehavior *CreatePhysicalizeConcave(CKBehavior *script, CK3dEntity *target, CKBOOL fixed,
                                     float friction, float elasticity, float mass, const char *collGroup,
                                     CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                                     float linearDamp, float rotDamp, const char *collSurface,
                                     VxVector massCenter, CKMesh *mesh) {
    ModContextLease context;
    auto *adapter = GetAdapter(context);
    auto options = MakePhysicalizeOptions(
        target, fixed, friction, elasticity, mass, collGroup, startFrozen,
        enableColl, calcMassCenter, linearDamp, rotDamp, collSurface,
        massCenter);
    options.Geometry = BML::Behavior::Blocks::Physicalize::Shape::Concave;
    options.Mesh = mesh;
    return adapter ? adapter->AddToGraph(
        script, BML::Behavior::Internal::BlockSpec::From(options)) : nullptr;
}

CKBehavior *CreateSetPhysicsForce(CKBehavior *script, CK3dEntity *target, VxVector position,
                                  CK3dEntity *posRef, VxVector direction,
                                  CK3dEntity *directionRef, float force) {
    ModContextLease context;
    auto *adapter = GetAdapter(context);
    return adapter ? adapter->AddToGraph(script, BML::Behavior::Internal::BlockSpec::From(
        MakeForceOptions<BML::Behavior::Blocks::PhysicsForce::Options>(
            target, position, posRef, direction, directionRef, force))) : nullptr;
}

CKBehavior *CreatePhysicsImpulse(CKBehavior *script, CK3dEntity *target, VxVector position,
                                 CK3dEntity *posRef, VxVector direction, CK3dEntity *dirRef,
                                 float impulse) {
    ModContextLease context;
    auto *adapter = GetAdapter(context);
    return adapter ? adapter->AddToGraph(script, BML::Behavior::Internal::BlockSpec::From(
        MakeForceOptions<BML::Behavior::Blocks::PhysicsImpulse::Options>(
            target, position, posRef, direction, dirRef, impulse))) : nullptr;
}

CKBehavior *CreatePhysicsWakeUp(CKBehavior *script, CK3dEntity *target) {
    ModContextLease context;
    auto *adapter = GetAdapter(context);
    return adapter ? adapter->AddToGraph(
        script, BML::Behavior::Internal::BlockSpec::From(
            BML::Behavior::Blocks::PhysicsWakeUp::Options{target})) : nullptr;
}

CKBehavior *CreateObjectLoad(CKBehavior *script, const char *file, const char *mastername,
                             CK_CLASSID filter, CKBOOL addToScene, CKBOOL reuseMesh,
                             CKBOOL reuseMtl, CKBOOL dynamic) {
    ModContextLease context;
    auto *adapter = GetAdapter(context);
    return adapter ? adapter->AddToGraph(script, BML::Behavior::Internal::BlockSpec::From(
        MakeObjectLoadOptions(file, mastername, filter,
                              addToScene, reuseMesh, reuseMtl, dynamic))) : nullptr;
}

CKBehavior *CreateSendMessage(CKBehavior *script, const char *msg, CKBeObject *dest) {
    ModContextLease context;
    auto *adapter = GetAdapter(context);
    return adapter ? adapter->AddToGraph(
        script, BML::Behavior::Internal::BlockSpec::From(
            BML::Behavior::Blocks::Send::Options{
                msg ? msg : "", dest})) : nullptr;
}

CKBehavior *CreateHookBlock(CKBehavior *script, CKBehaviorCallback callback, void *arg,
                            int inCount, int outCount) {
    ModContextLease context;
    auto *adapter = GetAdapter(context);
    return adapter ? adapter->AddToGraph(script, BML::Behavior::Internal::HookBlock::Make(
        callback, arg, inCount, outCount)) : nullptr;
}

} // namespace ExecuteBB

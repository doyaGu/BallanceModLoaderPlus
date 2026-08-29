#ifndef BML_BEHAVIORGRAPHRECIPES_H
#define BML_BEHAVIORGRAPHRECIPES_H

#include <string>

#include "CKAll.h"

namespace BML::BehaviorGraphRecipes {

struct Text2DDefinition {
    CK2dEntity *Target = nullptr;
    int FontIndex = 0;
    std::string Text;
    int Alignment = 0;
    VxRect Margin{2.0f, 2.0f, 2.0f, 2.0f};
    Vx2DVector Offset{0.0f, 0.0f};
    Vx2DVector ParagraphIndentation{0.0f, 0.0f};
    CKMaterial *BackgroundMaterial = nullptr;
    float CaretSize = 0.1f;
    CKMaterial *CaretMaterial = nullptr;
    int Flags = 1;
};

struct PhysicalizeDefinition {
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

struct ForceDefinition {
    CK3dEntity *Target = nullptr;
    VxVector Position{0.0f, 0.0f, 0.0f};
    CK3dEntity *PositionReference = nullptr;
    VxVector Direction{0.0f, 0.0f, 0.0f};
    CK3dEntity *DirectionReference = nullptr;
    float Magnitude = 0.0f;
};

struct ObjectLoadDefinition {
    std::string File;
    std::string MasterName;
    CK_CLASSID FilterClass = CKCID_3DOBJECT;
    CKBOOL AddToScene = TRUE;
    CKBOOL ReuseMeshes = TRUE;
    CKBOOL ReuseMaterials = TRUE;
    CKBOOL Dynamic = TRUE;
    bool Rename = true;
};

using BehaviorCallback = int (*)(const CKBehaviorContext *context, void *argument);

CKBehavior *Add2DText(CKBehavior *ownerScript, const Text2DDefinition &definition);
CKBehavior *AddPhysicalizeConvex(CKBehavior *ownerScript, const PhysicalizeDefinition &definition,
                                 CKMesh *mesh = nullptr);
CKBehavior *AddPhysicalizeBall(CKBehavior *ownerScript, const PhysicalizeDefinition &definition,
                               VxVector ballCenter = VxVector(0.0f, 0.0f, 0.0f), float ballRadius = 2.0f);
CKBehavior *AddPhysicalizeConcave(CKBehavior *ownerScript, const PhysicalizeDefinition &definition,
                                  CKMesh *mesh = nullptr);
CKBehavior *AddPhysicsForce(CKBehavior *ownerScript, const ForceDefinition &definition);
CKBehavior *AddPhysicsImpulse(CKBehavior *ownerScript, const ForceDefinition &definition);
CKBehavior *AddPhysicsWakeUp(CKBehavior *ownerScript, CK3dEntity *target = nullptr);
CKBehavior *AddObjectLoad(CKBehavior *ownerScript, const ObjectLoadDefinition &definition);
CKBehavior *AddSendMessage(CKBehavior *ownerScript, const char *message, CKBeObject *destination);
CKBehavior *AddHookBlock(CKBehavior *ownerScript, BehaviorCallback callback, void *argument = nullptr,
                         int inputCount = 1, int outputCount = 1);

} // namespace BML::BehaviorGraphRecipes

#endif // BML_BEHAVIORGRAPHRECIPES_H

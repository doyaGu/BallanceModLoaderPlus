#ifndef BML_BALLANCEBEHAVIORPRESETS_H
#define BML_BALLANCEBEHAVIORPRESETS_H

#include <string>

#include "Virtools/BehaviorRuntime.h"

namespace BML::Virtools::Presets {

struct Text2DOptions {
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

struct PhysicalizeOptions {
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

struct ForceOptions {
    CK3dEntity *Target = nullptr;
    VxVector Position{0.0f, 0.0f, 0.0f};
    CK3dEntity *PositionReference = nullptr;
    VxVector Direction{0.0f, 0.0f, 0.0f};
    CK3dEntity *DirectionReference = nullptr;
    float Magnitude = 0.0f;
};

struct ObjectLoadOptions {
    std::string File;
    std::string MasterName;
    CK_CLASSID FilterClass = CKCID_3DOBJECT;
    CKBOOL AddToScene = TRUE;
    CKBOOL ReuseMeshes = TRUE;
    CKBOOL ReuseMaterials = TRUE;
    CKBOOL Dynamic = TRUE;
    bool Rename = true;
};

using HookCallback = int (*)(const CKBehaviorContext *context, void *argument);

BehaviorSpec Text2D(const Text2DOptions &options);
BehaviorSpec PhysicalizeConvex(const PhysicalizeOptions &options, CKMesh *mesh = nullptr);
BehaviorSpec PhysicalizeBall(const PhysicalizeOptions &options,
                             VxVector center = VxVector(0.0f, 0.0f, 0.0f), float radius = 2.0f);
BehaviorSpec PhysicalizeConcave(const PhysicalizeOptions &options, CKMesh *mesh = nullptr);
BehaviorSpec PhysicsForce(const ForceOptions &options);
BehaviorSpec PhysicsImpulse(const ForceOptions &options);
BehaviorSpec PhysicsWakeUp(CK3dEntity *target);
BehaviorSpec ObjectLoad(const ObjectLoadOptions &options);
BehaviorSpec SendMessage(const char *message, CKBeObject *destination);
BehaviorSpec Hook(HookCallback callback, void *argument = nullptr,
                  int inputCount = 1, int outputCount = 1);

} // namespace BML::Virtools::Presets

#endif // BML_BALLANCEBEHAVIORPRESETS_H

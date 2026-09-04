// The Physicalize Building Block. Its geometry pin exists only after the
// counts are edited, so that slot is addressed by index past the stage
// boundary instead of by name.
#ifndef BML_BEHAVIOR_BLOCKS_PHYSICALIZE_HPP
#define BML_BEHAVIOR_BLOCKS_PHYSICALIZE_HPP

#include <string>
#include <string_view>

#include "CKAll.h"
#ifdef BML_BEHAVIOR_INTERNAL
#include "Behavior/Blocks/Definition.h"
#else
#include "BML/Behavior/Detail/Blocks.hpp"
#endif
#include "BML/Guids/physics_RT.h"

namespace BML::Behavior::Blocks {
namespace Physicalize {

enum class Shape {
    Convex,
    Ball,
    Concave,
};

struct Options {
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
    Shape Geometry = Shape::Convex;
    CKMesh *Mesh = nullptr;
    VxVector Center{0.0f, 0.0f, 0.0f};
    float Radius = 2.0f;
};

namespace Detail {
template <class Definition>
void Base(Definition &block, const Options &options) {
    block.Target(CKPGUID_3DENTITY, options.Target);
    block.Pin(0, CKPGUID_BOOL, options.Fixed != FALSE);
    block.Pin(1, CKPGUID_FLOAT, options.Friction);
    block.Pin(2, CKPGUID_FLOAT, options.Elasticity);
    block.Pin(3, CKPGUID_FLOAT, options.Mass);
    block.Pin(4, CKPGUID_STRING, std::string_view(options.CollisionGroup));
    block.Pin(5, CKPGUID_BOOL, options.StartFrozen != FALSE);
    block.Pin(6, CKPGUID_BOOL, options.EnableCollision != FALSE);
    block.Pin(7, CKPGUID_BOOL, options.CalculateMassCenter != FALSE);
    block.Pin(8, CKPGUID_FLOAT, options.LinearDamping);
    block.Pin(9, CKPGUID_FLOAT, options.RotationalDamping);
    block.Pin(10, CKPGUID_STRING,
              std::string_view(options.CollisionSurface));
}

template <class Definition>
void ShapeSettings(Definition &block, const Options &options, int convex,
                   int ball, int concave) {
    Base(block, options);
    block.Setting(0, CKPGUID_INT, convex);
    block.Setting(1, CKPGUID_INT, ball);
    block.Setting(2, CKPGUID_INT, concave);
    block.Setting(3, CKPGUID_VECTOR, options.MassCenter);
    block.NextStage();
}

template <class Definition>
void Convex(Definition &block, const Options &options, CKMesh *mesh) {
    ShapeSettings(block, options, 1, 0, 0);
    block.ObjectPin(11, CKPGUID_MESH, mesh);
}

template <class Definition>
void Ball(Definition &block, const Options &options,
          const VxVector &center, float radius) {
    ShapeSettings(block, options, 0, 1, 0);
    block.Pin(11, CKPGUID_VECTOR, center);
    block.Pin(12, CKPGUID_FLOAT, radius);
}

template <class Definition>
void Concave(Definition &block, const Options &options, CKMesh *mesh) {
    ShapeSettings(block, options, 0, 0, 1);
    block.ObjectPin(11, CKPGUID_MESH, mesh);
}
} // namespace Detail

#ifdef BML_BEHAVIOR_INTERNAL
inline Spec Make(const Options &options) {
    Blocks::Detail::Definition block(PHYSICS_RT_PHYSICALIZE);
    switch (options.Geometry) {
    case Shape::Convex:
        Detail::Convex(block, options, options.Mesh);
        break;
    case Shape::Ball:
        Detail::Ball(block, options, options.Center, options.Radius);
        break;
    case Shape::Concave:
        Detail::Concave(block, options, options.Mesh);
        break;
    }
    return std::move(block).Build();
}
#else
inline Result<Block> Make(const Session &session, const Options &options) {
    BML::Behavior::Detail::Definition block(session, PHYSICS_RT_PHYSICALIZE);
    switch (options.Geometry) {
    case Shape::Convex:
        Detail::Convex(block, options, options.Mesh);
        break;
    case Shape::Ball:
        Detail::Ball(block, options, options.Center, options.Radius);
        break;
    case Shape::Concave:
        Detail::Concave(block, options, options.Mesh);
        break;
    }
    return std::move(block).Build();
}
#endif


} // namespace Physicalize
} // namespace BML::Behavior::Blocks

#endif // BML_BEHAVIOR_BLOCKS_PHYSICALIZE_HPP

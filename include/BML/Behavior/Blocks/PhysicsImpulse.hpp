// The Physics Impulse Building Block: one impulse on one target. Its two
// settings decide how the Block reads its geometry pins, so the pins travel
// behind the stage boundary's layout refresh.
#ifndef BML_BEHAVIOR_BLOCKS_PHYSICSIMPULSE_HPP
#define BML_BEHAVIOR_BLOCKS_PHYSICSIMPULSE_HPP

#include "CKAll.h"
#ifdef BML_BEHAVIOR_INTERNAL
#include "Behavior/Blocks/Definition.h"
#else
#include "BML/Behavior/Detail/Blocks.hpp"
#endif
#include "BML/Guids/physics_RT.h"

namespace BML::Behavior::Blocks {
namespace PhysicsImpulse {

struct Options {
    CK3dEntity *Target = nullptr;
    VxVector Position{0.0f, 0.0f, 0.0f};
    CK3dEntity *PositionReference = nullptr;
    VxVector Direction{0.0f, 0.0f, 0.0f};
    CK3dEntity *DirectionReference = nullptr;
    float Magnitude = 0.0f;
    CKBOOL DirectionAsPoint = FALSE;
    CKBOOL ConstantForce = FALSE;
};

namespace Detail {
template <class Definition>
void Define(Definition &block, const Options &options) {
    block.Target(CKPGUID_3DENTITY, options.Target);
    block.Setting(0, CKPGUID_BOOL, options.DirectionAsPoint != FALSE);
    block.Setting(1, CKPGUID_BOOL, options.ConstantForce != FALSE);
    block.NextStage();
    block.Pin(0, CKPGUID_VECTOR, options.Position);
    block.ObjectPin(1, CKPGUID_3DENTITY, options.PositionReference);
    block.Pin(2, CKPGUID_VECTOR, options.Direction);
    block.ObjectPin(3, CKPGUID_3DENTITY, options.DirectionReference);
    block.Pin(4, CKPGUID_FLOAT, options.Magnitude);
}
} // namespace Detail

#ifdef BML_BEHAVIOR_INTERNAL
inline Spec Make(const Options &options) {
    Blocks::Detail::Definition block(PHYSICS_RT_PHYSICSIMPULSE);
    Detail::Define(block, options);
    return std::move(block).Build();
}
#else
inline Result<Block> Make(const Session &session, const Options &options) {
    BML::Behavior::Detail::Definition block(session, PHYSICS_RT_PHYSICSIMPULSE);
    Detail::Define(block, options);
    return std::move(block).Build();
}
#endif


} // namespace PhysicsImpulse
} // namespace BML::Behavior::Blocks

#endif // BML_BEHAVIOR_BLOCKS_PHYSICSIMPULSE_HPP

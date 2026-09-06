// The Physics Impulse Building Block: one impulse on one target. Its two
// settings decide how the Block reads its geometry pins, so the pins travel
// behind the stage boundary's layout refresh.
#ifndef BML_BEHAVIOR_BLOCKS_PHYSICSIMPULSE_HPP
#define BML_BEHAVIOR_BLOCKS_PHYSICSIMPULSE_HPP

#include "CKAll.h"
#include "BML/Behavior/Detail/Blocks.hpp"
#include "BML/Guids/physics_RT.h"

namespace BML::Behavior::Blocks {
namespace PhysicsImpulse {

struct Options {
    [[nodiscard]] static CKGUID Prototype() noexcept {
        return PHYSICS_RT_PHYSICSIMPULSE;
    }

    CK3dEntity *Target = nullptr;
    VxVector Position{0.0f, 0.0f, 0.0f};
    CK3dEntity *PositionReference = nullptr;
    VxVector Direction{0.0f, 0.0f, 0.0f};
    CK3dEntity *DirectionReference = nullptr;
    float Magnitude = 0.0f;
    CKBOOL DirectionAsPoint = FALSE;
    CKBOOL ConstantForce = FALSE;

private:
    template <class Definition>
    void Configure(Definition &block) const {
        block.Target(CKPGUID_3DENTITY, Target);
        block.Setting(0, CKPGUID_BOOL, DirectionAsPoint != FALSE);
        block.Setting(1, CKPGUID_BOOL, ConstantForce != FALSE);
        block.NextStage();
        block.Pin(0, CKPGUID_VECTOR, Position);
        block.ObjectPin(1, CKPGUID_3DENTITY, PositionReference);
        block.Pin(2, CKPGUID_VECTOR, Direction);
        block.ObjectPin(3, CKPGUID_3DENTITY, DirectionReference);
        block.Pin(4, CKPGUID_FLOAT, Magnitude);
    }

    friend class BML::Behavior::Detail::BlockAccess;
};

inline Result<Block> Make(const Session &session, const Options &options) {
    BML::Behavior::Detail::Definition block(session, Options::Prototype());
    BML::Behavior::Detail::BlockAccess::Configure(options, block);
    return std::move(block).Build();
}

} // namespace PhysicsImpulse
} // namespace BML::Behavior::Blocks

#endif // BML_BEHAVIOR_BLOCKS_PHYSICSIMPULSE_HPP

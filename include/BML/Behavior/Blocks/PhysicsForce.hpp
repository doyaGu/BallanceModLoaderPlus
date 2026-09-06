// The Physics Force Building Block: one continuous force on one target.
#ifndef BML_BEHAVIOR_BLOCKS_PHYSICSFORCE_HPP
#define BML_BEHAVIOR_BLOCKS_PHYSICSFORCE_HPP

#include "CKAll.h"
#include "BML/Behavior/Detail/Blocks.hpp"
#include "BML/Guids/physics_RT.h"

namespace BML::Behavior::Blocks {
namespace PhysicsForce {

struct Options {
    [[nodiscard]] static CKGUID Prototype() noexcept {
        return PHYSICS_RT_PHYSICSFORCE;
    }

    CK3dEntity *Target = nullptr;
    VxVector Position{0.0f, 0.0f, 0.0f};
    CK3dEntity *PositionReference = nullptr;
    VxVector Direction{0.0f, 0.0f, 0.0f};
    CK3dEntity *DirectionReference = nullptr;
    float Magnitude = 0.0f;

private:
    template <class Definition>
    void Configure(Definition &block) const {
        block.Target(CKPGUID_3DENTITY, Target);
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

} // namespace PhysicsForce
} // namespace BML::Behavior::Blocks

#endif // BML_BEHAVIOR_BLOCKS_PHYSICSFORCE_HPP

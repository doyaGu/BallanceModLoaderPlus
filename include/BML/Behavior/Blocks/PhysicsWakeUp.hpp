// The Physics Wake Up Building Block: wake one target's simulation.
#ifndef BML_BEHAVIOR_BLOCKS_PHYSICSWAKEUP_HPP
#define BML_BEHAVIOR_BLOCKS_PHYSICSWAKEUP_HPP

#include "CKAll.h"
#include "BML/Behavior/Detail/Blocks.hpp"
#include "BML/Guids/physics_RT.h"

namespace BML::Behavior::Blocks {
namespace PhysicsWakeUp {

struct Options {
    [[nodiscard]] static CKGUID Prototype() noexcept {
        return PHYSICS_RT_PHYSICSWAKEUP;
    }

    CK3dEntity *Target = nullptr;

private:
    template <class Definition>
    void Configure(Definition &block) const {
        block.Target(CKPGUID_3DENTITY, Target);
    }

    friend class BML::Behavior::Detail::BlockAccess;
};

inline Result<Block> Make(const Session &session, const Options &options) {
    BML::Behavior::Detail::Definition block(session, Options::Prototype());
    BML::Behavior::Detail::BlockAccess::Configure(options, block);
    return std::move(block).Build();
}

} // namespace PhysicsWakeUp
} // namespace BML::Behavior::Blocks

#endif // BML_BEHAVIOR_BLOCKS_PHYSICSWAKEUP_HPP

// The Physics Wake Up Building Block: wake one target's simulation.
#ifndef BML_BEHAVIOR_BLOCKS_PHYSICSWAKEUP_HPP
#define BML_BEHAVIOR_BLOCKS_PHYSICSWAKEUP_HPP

#include "CKAll.h"
#ifdef BML_BEHAVIOR_INTERNAL
#include "Behavior/Blocks/Definition.h"
#else
#include "BML/Behavior/Detail/Blocks.hpp"
#endif
#include "BML/Guids/physics_RT.h"

namespace BML::Behavior::Blocks {
namespace PhysicsWakeUp {

struct Options {
    CK3dEntity *Target = nullptr;
};

namespace Detail {
template <class Definition>
void Define(Definition &block, const Options &options) {
    block.Target(CKPGUID_3DENTITY, options.Target);
}
} // namespace Detail

#ifdef BML_BEHAVIOR_INTERNAL
inline BlockSpec Make(const Options &options) {
    Blocks::Detail::Definition block(PHYSICS_RT_PHYSICSWAKEUP);
    Detail::Define(block, options);
    return std::move(block).Build();
}
#else
inline Result<Block> Make(const Session &session, const Options &options) {
    BML::Behavior::Detail::Definition block(session, PHYSICS_RT_PHYSICSWAKEUP);
    Detail::Define(block, options);
    return std::move(block).Build();
}
#endif


} // namespace PhysicsWakeUp
} // namespace BML::Behavior::Blocks

#endif // BML_BEHAVIOR_BLOCKS_PHYSICSWAKEUP_HPP

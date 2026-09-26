#ifndef BML_API_EXECUTEBBADAPTER_H
#define BML_API_EXECUTEBBADAPTER_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "Behavior/Block.h"
#include "BML/Behavior/Blocks/ObjectLoad.hpp"
#include "BML/Behavior/Blocks/PhysicsForce.hpp"
#include "Behavior/Blocks/PhysicsForce.h"
#include "Behavior/Sessions.h"

namespace BML {

// Adapts the exported ExecuteBB free-function API to the Behavior module and owns
// the state needed by operations that outlive one call. Its Runs belong to the
// Loader's Session, which retires with the built-in Mod.
class ExecuteBBAdapter final {
public:
    ExecuteBBAdapter(Behavior::Internal::Sessions &sessions,
                     Behavior::Internal::PhysicsForce::Sessions &physicsForce)
        : m_Sessions(sessions), m_PhysicsForce(physicsForce) {}

    // Opens the Loader's Session for the built-in Mod's owner.
    Behavior::Internal::Status Open(const std::string &ownerId);

    Behavior::Internal::RunResult Run(CKBeObject *owner, const Behavior::Internal::BlockSpec &spec,
                            int input = 0);
    Behavior::Internal::RunResult SetPhysicsForce(
        const Behavior::Blocks::PhysicsForce::Options &options);
    Behavior::Internal::RunResult UnsetPhysicsForce(CK3dEntity *target);
    // The returned array belongs to the load's Block, which stays open until
    // the next load replaces it.
    std::pair<XObjectArray *, CKObject *> LoadObjects(
        const Behavior::Blocks::ObjectLoad::Options &options, bool rename);
    CKBehavior *CreateUnmanaged(CKBehavior *parent, const Behavior::Internal::BlockSpec &spec);
    void ProcessFrame();
    void Reset();

private:
    Behavior::Internal::Sessions &m_Sessions;
    Behavior::Internal::PhysicsForce::Sessions &m_PhysicsForce;
    std::uintptr_t m_Session = 0;
    std::uintptr_t m_LastObjectLoad = 0;
    std::vector<std::uintptr_t> m_Tasks;
    // Never reset: an object that survives a world reset keeps its suffix, so
    // a later load must not hand the same one out again.
    unsigned int m_LoadCount = 0;
};

} // namespace BML

#endif // BML_API_EXECUTEBBADAPTER_H

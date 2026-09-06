#ifndef BML_API_EXECUTEBBADAPTER_H
#define BML_API_EXECUTEBBADAPTER_H

#include <utility>
#include <vector>

#include "Behavior/Block.h"
#include "BML/Behavior/Blocks/ObjectLoad.hpp"
#include "BML/Behavior/Blocks/PhysicsForce.hpp"
#include "Behavior/PhysicsForce.h"
#include "Behavior/Runtime.h"

namespace BML {

// Adapts the exported ExecuteBB free-function API to the Behavior module and owns
// the state needed by operations that outlive one call.
class ExecuteBBAdapter final {
public:
    ExecuteBBAdapter(Behavior::Internal::Runtime &runtime,
                     Behavior::Internal::PhysicsForce::Sessions &physicsForce)
        : m_Runtime(runtime), m_PhysicsForce(physicsForce) {}

    Behavior::Internal::RunResult Run(CKBeObject *owner, const Behavior::Internal::BlockSpec &spec,
                            int input = 0);
    Behavior::Internal::RunResult SetPhysicsForce(
        const Behavior::Blocks::PhysicsForce::Options &options);
    Behavior::Internal::RunResult UnsetPhysicsForce(CK3dEntity *target);
    std::pair<XObjectArray *, CKObject *> LoadObjects(
        const Behavior::Blocks::ObjectLoad::Options &options, bool rename);
    CKBehavior *AddToGraph(CKBehavior *parent, const Behavior::Internal::BlockSpec &spec);
    void ProcessFrame();
    void Reset();

private:
    Behavior::Internal::Runtime &m_Runtime;
    Behavior::Internal::PhysicsForce::Sessions &m_PhysicsForce;
    Behavior::Internal::Instance m_LastObjectLoad;
    std::vector<Behavior::Internal::Instance> m_Tasks;
    unsigned int m_LoadCount = 0;
};

} // namespace BML

#endif // BML_API_EXECUTEBBADAPTER_H

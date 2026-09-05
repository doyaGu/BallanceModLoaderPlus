#ifndef BML_API_EXECUTEBBADAPTER_H
#define BML_API_EXECUTEBBADAPTER_H

#include <utility>
#include <vector>

#include "Behavior/Blocks.h"
#include "Behavior/PhysicsForce.h"
#include "Behavior/Runtime.h"

namespace BML {

// Adapts the exported ExecuteBB free-function API to the Behavior module and owns
// the state needed by operations that outlive one call.
class ExecuteBBAdapter final {
public:
    ExecuteBBAdapter(Behavior::Runtime &runtime,
                     Behavior::PhysicsForce::Sessions &physicsForce)
        : m_Runtime(runtime), m_PhysicsForce(physicsForce) {}

    Behavior::RunResult Run(CKBeObject *owner, const Behavior::BlockSpec &spec,
                            int input = 0);
    Behavior::RunResult SetPhysicsForce(
        const Behavior::Blocks::PhysicsForce::Options &options);
    Behavior::RunResult UnsetPhysicsForce(CK3dEntity *target);
    std::pair<XObjectArray *, CKObject *> LoadObjects(
        const Behavior::Blocks::ObjectLoad::Options &options, bool rename);
    CKBehavior *AddToGraph(CKBehavior *parent, const Behavior::BlockSpec &spec);
    void ProcessFrame();
    void Reset();

private:
    Behavior::Runtime &m_Runtime;
    Behavior::PhysicsForce::Sessions &m_PhysicsForce;
    Behavior::Instance m_LastObjectLoad;
    std::vector<Behavior::Instance> m_Tasks;
    unsigned int m_LoadCount = 0;
};

} // namespace BML

#endif // BML_API_EXECUTEBBADAPTER_H

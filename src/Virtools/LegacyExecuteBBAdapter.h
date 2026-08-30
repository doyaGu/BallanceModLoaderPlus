#ifndef BML_LEGACYEXECUTEBBADAPTER_H
#define BML_LEGACYEXECUTEBBADAPTER_H

#include <utility>
#include <vector>

#include "Virtools/PhysicsForceSessions.h"

namespace BML::Virtools {

// State required only by the exported v0.3 ExecuteBB compatibility surface.
// New loader code calls BehaviorRuntime directly.
class LegacyExecuteBBAdapter final {
public:
    LegacyExecuteBBAdapter(BehaviorRuntime &runtime, PhysicsForceSessions &physicsForces)
        : m_Runtime(runtime), m_PhysicsForces(physicsForces) {}

    ExecutionResult Run(CKBeObject *owner, const BehaviorSpec &spec, int input = 0);
    ExecutionResult SetPhysicsForce(const Presets::ForceOptions &options);
    ExecutionResult UnsetPhysicsForce(CK3dEntity *target);
    std::pair<XObjectArray *, CKObject *> LoadObjects(const Presets::ObjectLoadOptions &options);
    CKBehavior *AddToGraph(CKBehavior *parent, const BehaviorSpec &spec);
    void ProcessFrame();
    void Reset();

private:
    BehaviorRuntime &m_Runtime;
    PhysicsForceSessions &m_PhysicsForces;
    BehaviorInstance m_LastObjectLoad;
    std::vector<BehaviorInstance> m_Tasks;
    unsigned int m_LoadCount = 0;
};

} // namespace BML::Virtools

#endif // BML_LEGACYEXECUTEBBADAPTER_H

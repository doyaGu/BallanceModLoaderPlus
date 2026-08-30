#include "Virtools/LegacyExecuteBBAdapter.h"

#include <algorithm>
#include <string>

namespace BML::Virtools {

ExecutionResult LegacyExecuteBBAdapter::Run(CKBeObject *owner, const BehaviorSpec &spec, int input) {
    InstanceResult created = m_Runtime.Instantiate(owner, spec);
    if (!created)
        return {std::move(created.Status), ExecutionState::Failed, CKBR_BEHAVIORERROR, {}};
    ExecutionResult result = m_Runtime.StartTask(
        created.Instance, SlotSelector::At(BehaviorSlotKind::Input, input));
    if (result.State == ExecutionState::Continuing || result.State == ExecutionState::Suspended)
        m_Tasks.push_back(std::move(created.Instance));
    return result;
}

ExecutionResult LegacyExecuteBBAdapter::SetPhysicsForce(const Presets::ForceOptions &options) {
    return m_PhysicsForces.Set(options);
}

ExecutionResult LegacyExecuteBBAdapter::UnsetPhysicsForce(CK3dEntity *target) {
    return m_PhysicsForces.Clear(target);
}

std::pair<XObjectArray *, CKObject *> LegacyExecuteBBAdapter::LoadObjects(
    const Presets::ObjectLoadOptions &options) {
    m_LastObjectLoad.Reset();
    InstanceResult created = m_Runtime.Instantiate(nullptr, Presets::ObjectLoad(options));
    if (!created)
        return {nullptr, nullptr};
    ExecutionResult executed = m_Runtime.Pulse(
        created.Instance, SlotSelector::At(BehaviorSlotKind::Input, 0));
    if (!executed || executed.State != ExecutionState::Completed)
        return {nullptr, nullptr};

    CKBehavior *behavior = created.Instance.Get();
    if (!behavior)
        return {nullptr, nullptr};
    XObjectArray *objects = behavior->GetOutputParameterCount() > 0
        ? *static_cast<XObjectArray **>(behavior->GetOutputParameterWriteDataPtr(0)) : nullptr;
    CKObject *master = behavior->GetOutputParameterCount() > 1
        ? behavior->GetOutputParameterObject(1) : nullptr;

    if (options.Rename && objects) {
        const unsigned int suffix = ++m_LoadCount;
        for (CK_ID *id = objects->Begin(); id != objects->End(); ++id) {
            CKObject *object = behavior->GetCKContext()->GetObject(*id);
            if (!object || !CKIsChildClassOf(object, CKCID_BEOBJECT))
                continue;
            std::string name = object->GetName() ? object->GetName() : "";
            name += "_BMLLoad_" + std::to_string(suffix);
            object->SetName(const_cast<CKSTRING>(name.c_str()));
        }
    }

    m_LastObjectLoad = std::move(created.Instance);
    return {objects, master};
}

CKBehavior *LegacyExecuteBBAdapter::AddToGraph(CKBehavior *parent, const BehaviorSpec &spec) {
    GraphBlockResult created = m_Runtime.AddToGraph(parent, spec);
    return created ? created.Behavior : nullptr;
}

void LegacyExecuteBBAdapter::ProcessFrame() {
    m_Tasks.erase(
        std::remove_if(m_Tasks.begin(), m_Tasks.end(),
                       [&](const BehaviorInstance &task) { return !m_Runtime.IsTaskActive(task); }),
        m_Tasks.end());
}

void LegacyExecuteBBAdapter::Reset() {
    m_LastObjectLoad.Reset();
    m_Tasks.clear();
    m_LoadCount = 0;
}

} // namespace BML::Virtools

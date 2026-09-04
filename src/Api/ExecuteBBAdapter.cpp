#include "Api/ExecuteBBAdapter.h"

#include <algorithm>
#include <string>

namespace BML {

Behavior::RunResult ExecuteBBAdapter::Run(
    CKBeObject *owner, const Behavior::Spec &spec, int input) {
    Behavior::CreateResult created = m_Runtime.Instantiate(owner, spec);
    if (!created)
        return {std::move(created.Detail), Behavior::RunState::Failed,
                CKBR_BEHAVIORERROR, {}};
    Behavior::RunResult result = m_Runtime.StartTask(
        created.Handle, Behavior::Slot::At(Behavior::SlotKind::Input, input));
    if (result.State == Behavior::RunState::Pending)
        m_Tasks.push_back(std::move(created.Handle));
    return result;
}

Behavior::RunResult ExecuteBBAdapter::SetPhysicsForce(
    const Behavior::Blocks::PhysicsForce::Options &options) {
    return m_PhysicsForce.Set(options);
}

Behavior::RunResult ExecuteBBAdapter::UnsetPhysicsForce(CK3dEntity *target) {
    return m_PhysicsForce.Clear(target);
}

std::pair<XObjectArray *, CKObject *> ExecuteBBAdapter::LoadObjects(
    const Behavior::Blocks::ObjectLoad::Options &options, bool rename) {
    m_LastObjectLoad.Reset();
    Behavior::CreateResult created = m_Runtime.Instantiate(
        nullptr, Behavior::Blocks::ObjectLoad::Make(options));
    if (!created)
        return {nullptr, nullptr};
    Behavior::RunResult executed = m_Runtime.Pulse(
        created.Handle, Behavior::Slot::At(Behavior::SlotKind::Input, 0));
    if (!executed || executed.State != Behavior::RunState::Ready)
        return {nullptr, nullptr};

    CKBehavior *behavior = created.Handle.Get();
    if (!behavior)
        return {nullptr, nullptr};
    XObjectArray *objects = behavior->GetOutputParameterCount() > 0
        ? *static_cast<XObjectArray **>(behavior->GetOutputParameterWriteDataPtr(0)) : nullptr;
    CKObject *master = behavior->GetOutputParameterCount() > 1
        ? behavior->GetOutputParameterObject(1) : nullptr;

    if (rename && objects) {
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

    m_LastObjectLoad = std::move(created.Handle);
    return {objects, master};
}

CKBehavior *ExecuteBBAdapter::AddToGraph(
    CKBehavior *parent, const Behavior::Spec &spec) {
    Behavior::AttachResult created = m_Runtime.AddToGraph(parent, spec);
    return created ? created.Block : nullptr;
}

void ExecuteBBAdapter::ProcessFrame() {
    m_Tasks.erase(
        std::remove_if(m_Tasks.begin(), m_Tasks.end(),
                       [&](const Behavior::Instance &task) {
                           return !m_Runtime.IsTaskActive(task);
                       }),
        m_Tasks.end());
}

void ExecuteBBAdapter::Reset() {
    m_LastObjectLoad.Reset();
    m_Tasks.clear();
    m_LoadCount = 0;
}

} // namespace BML

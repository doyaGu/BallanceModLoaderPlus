#include "Api/ExecuteBBAdapter.h"

#include <algorithm>
#include <string>

namespace BML {
namespace {

using Behavior::Internal::OpenRun;
using Behavior::Internal::RunResult;
using Behavior::Internal::RunState;

// Start keeps a Run whose Block already executed even when that execution
// failed, and reports the failure on the Run rather than on the call.
RunResult ReadResult(const OpenRun &run) {
    if (!run)
        return {run.Result, RunState::Failed, CKBR_BEHAVIORERROR, {}};
    return {run.Info.LastStatus, run.Info.State};
}

} // namespace

Behavior::Internal::Status ExecuteBBAdapter::Open(const std::string &ownerId) {
    m_Sessions.CloseSession(std::exchange(m_Session, 0));
    return m_Sessions.OpenSession(ownerId, m_Session);
}

Behavior::Internal::RunResult ExecuteBBAdapter::Run(
    CKBeObject *owner, const Behavior::Internal::BlockSpec &spec, int input) {
    const OpenRun run = m_Sessions.Start(
        m_Session, owner, spec,
        Behavior::Internal::Slot::At(Behavior::Internal::SlotKind::Input, input));
    if (run.Id && run.Info.State == RunState::Pending)
        m_Tasks.push_back(run.Id);
    else
        m_Sessions.CloseRun(run.Id);
    return ReadResult(run);
}

Behavior::Internal::RunResult ExecuteBBAdapter::SetPhysicsForce(
    const Behavior::Blocks::PhysicsForce::Options &options) {
    return m_PhysicsForce.Set(options);
}

Behavior::Internal::RunResult ExecuteBBAdapter::UnsetPhysicsForce(CK3dEntity *target) {
    return m_PhysicsForce.Clear(target);
}

std::pair<XObjectArray *, CKObject *> ExecuteBBAdapter::LoadObjects(
    const Behavior::Blocks::ObjectLoad::Options &options, bool rename) {
    m_Sessions.CloseRun(std::exchange(m_LastObjectLoad, 0));
    const OpenRun load = m_Sessions.Start(
        m_Session, nullptr, Behavior::Internal::BlockSpec::From(options),
        Behavior::Internal::Slot::At(Behavior::Internal::SlotKind::Input, 0));
    m_LastObjectLoad = load.Id;
    CKBehavior *behavior = load && load.Info.State == RunState::Ready
        ? m_Sessions.Block(load.Id) : nullptr;
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
    return {objects, master};
}

CKBehavior *ExecuteBBAdapter::CreateUnmanaged(
    CKBehavior *parent, const Behavior::Internal::BlockSpec &spec) {
    return m_Sessions.CreateUnmanaged(parent, spec);
}

void ExecuteBBAdapter::ProcessFrame() {
    m_Tasks.erase(
        std::remove_if(m_Tasks.begin(), m_Tasks.end(),
                       [&](std::uintptr_t task) {
                           Behavior::Internal::RunInfo info;
                           if (m_Sessions.ReadRun(task, info) &&
                               info.State == RunState::Pending)
                               return false;
                           m_Sessions.CloseRun(task);
                           return true;
                       }),
        m_Tasks.end());
}

void ExecuteBBAdapter::Reset() {
    m_Sessions.CloseRun(std::exchange(m_LastObjectLoad, 0));
    for (std::uintptr_t task : m_Tasks)
        m_Sessions.CloseRun(task);
    m_Tasks.clear();
}

} // namespace BML

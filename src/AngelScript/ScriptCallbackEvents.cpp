#include "ScriptCallbackEvents.h"

namespace BML {

namespace {

static const char *SafeString(const char *value) {
    return value ? value : "";
}

static const char *ObjectName(CKObject *object) {
    return SafeString(object ? object->GetName() : nullptr);
}

} // namespace

ScriptRenderEventView::ScriptRenderEventView(CK_RENDER_FLAGS flags)
    : m_Flags(static_cast<int>(flags)) {}

ScriptCheatEventView::ScriptCheatEventView(bool enabled)
    : m_Enabled(enabled) {}

ScriptLoadObjectEventView::ScriptLoadObjectEventView(const char *filename,
                                                     CKBOOL isMap,
                                                     const char *masterName,
                                                     CK_CLASSID filterClass,
                                                     CKBOOL addToScene,
                                                     CKBOOL reuseMeshes,
                                                     CKBOOL reuseMaterials,
                                                     CKBOOL dynamic)
    : m_Filename(SafeString(filename)),
      m_IsMap(isMap != 0),
      m_MasterName(SafeString(masterName)),
      m_FilterClass(static_cast<int>(filterClass)),
      m_AddToScene(addToScene != 0),
      m_ReuseMeshes(reuseMeshes != 0),
      m_ReuseMaterials(reuseMaterials != 0),
      m_Dynamic(dynamic != 0) {}

std::string ScriptLoadObjectEventView::GetFilename() const {
    return SafeString(m_Filename);
}

std::string ScriptLoadObjectEventView::GetMasterName() const {
    return SafeString(m_MasterName);
}

ScriptLoadScriptEventView::ScriptLoadScriptEventView(const char *filename, CK_ID scriptId)
    : m_Filename(SafeString(filename)), m_ScriptId(static_cast<int>(scriptId)) {}

std::string ScriptLoadScriptEventView::GetFilename() const {
    return SafeString(m_Filename);
}

ScriptCommandEventView::ScriptCommandEventView(ScriptCommandEventPhase phase,
                                               ICommand *command,
                                               const std::vector<std::string> *args)
    : m_Phase(phase), m_Command(command), m_Args(args) {}

size_t ScriptCommandEventView::GetUserArgStart() const {
    return (m_Args && !m_Args->empty()) ? 1u : 0u;
}

std::string ScriptCommandEventView::GetCommandName() const {
    if (m_Command)
        return m_Command->GetName();
    return (m_Args && !m_Args->empty()) ? (*m_Args)[0] : std::string();
}

int ScriptCommandEventView::GetArgCount() const {
    if (!m_Args)
        return 0;
    const size_t start = GetUserArgStart();
    return m_Args->size() > start ? static_cast<int>(m_Args->size() - start) : 0;
}

std::string ScriptCommandEventView::GetArg(int index) const {
    if (!m_Args || index < 0)
        return {};
    const size_t realIndex = GetUserArgStart() + static_cast<size_t>(index);
    return realIndex < m_Args->size() ? (*m_Args)[realIndex] : std::string();
}

std::string ScriptCommandEventView::GetArgsText() const {
    std::string text;
    const int count = GetArgCount();
    for (int i = 0; i < count; ++i) {
        if (!text.empty())
            text.push_back(' ');
        text += GetArg(i);
    }
    return text;
}

bool ScriptCommandEventView::IsCheat() const {
    return m_Command && m_Command->IsCheat();
}

ScriptPhysicalizeEventView::ScriptPhysicalizeEventView(CK3dEntity *target,
                                                       CKBOOL fixed,
                                                       float friction,
                                                       float elasticity,
                                                       float mass,
                                                       const char *collGroup,
                                                       CKBOOL startFrozen,
                                                       CKBOOL enableColl,
                                                       CKBOOL calcMassCenter,
                                                       float linearDamp,
                                                       float rotDamp,
                                                       const char *collSurface,
                                                       VxVector massCenter,
                                                       int convexCnt,
                                                       int ballCnt,
                                                       int concaveCnt)
    : m_TargetId(target ? static_cast<int>(target->GetID()) : 0),
      m_TargetName(ObjectName(target)),
      m_Fixed(fixed != 0),
      m_Friction(friction),
      m_Elasticity(elasticity),
      m_Mass(mass),
      m_CollisionGroup(SafeString(collGroup)),
      m_StartFrozen(startFrozen != 0),
      m_EnableCollision(enableColl != 0),
      m_AutoCalcMassCenter(calcMassCenter != 0),
      m_LinearDamp(linearDamp),
      m_RotDamp(rotDamp),
      m_CollisionSurface(SafeString(collSurface)),
      m_MassCenter(massCenter),
      m_ConvexCount(convexCnt),
      m_BallCount(ballCnt),
      m_ConcaveCount(concaveCnt) {}

std::string ScriptPhysicalizeEventView::GetTargetName() const {
    return SafeString(m_TargetName);
}

std::string ScriptPhysicalizeEventView::GetCollisionGroup() const {
    return SafeString(m_CollisionGroup);
}

std::string ScriptPhysicalizeEventView::GetCollisionSurface() const {
    return SafeString(m_CollisionSurface);
}

ScriptObjectEventView::ScriptObjectEventView(CK3dEntity *target)
    : m_TargetId(target ? static_cast<int>(target->GetID()) : 0),
      m_TargetName(ObjectName(target)) {}

std::string ScriptObjectEventView::GetTargetName() const {
    return SafeString(m_TargetName);
}

} // namespace BML

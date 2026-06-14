#include "ScriptCallbackEvents.h"

namespace BML {

namespace {

static const char *SafeString(const char *value) {
    return value ? value : "";
}

static const char *ObjectName(CKObject *object) {
    return SafeString(object ? object->GetName() : nullptr);
}

static bool IsIndexInRange(int index, int count) {
    return index >= 0 && index < count;
}

static const char *kGameEventNames[] = {
    "GAME_EVENT_PRE_START_MENU",
    "GAME_EVENT_POST_START_MENU",
    "GAME_EVENT_EXIT_GAME",
    "GAME_EVENT_PRE_LOAD_LEVEL",
    "GAME_EVENT_POST_LOAD_LEVEL",
    "GAME_EVENT_START_LEVEL",
    "GAME_EVENT_PRE_RESET_LEVEL",
    "GAME_EVENT_POST_RESET_LEVEL",
    "GAME_EVENT_PAUSE_LEVEL",
    "GAME_EVENT_UNPAUSE_LEVEL",
    "GAME_EVENT_PRE_EXIT_LEVEL",
    "GAME_EVENT_POST_EXIT_LEVEL",
    "GAME_EVENT_PRE_NEXT_LEVEL",
    "GAME_EVENT_POST_NEXT_LEVEL",
    "GAME_EVENT_DEAD",
    "GAME_EVENT_PRE_END_LEVEL",
    "GAME_EVENT_POST_END_LEVEL",
    "GAME_EVENT_COUNTER_ACTIVE",
    "GAME_EVENT_COUNTER_INACTIVE",
    "GAME_EVENT_BALL_NAV_ACTIVE",
    "GAME_EVENT_BALL_NAV_INACTIVE",
    "GAME_EVENT_CAM_NAV_ACTIVE",
    "GAME_EVENT_CAM_NAV_INACTIVE",
    "GAME_EVENT_BALL_OFF",
    "GAME_EVENT_PRE_CHECKPOINT_REACHED",
    "GAME_EVENT_POST_CHECKPOINT_REACHED",
    "GAME_EVENT_LEVEL_FINISH",
    "GAME_EVENT_GAME_OVER",
    "GAME_EVENT_EXTRA_POINT",
    "GAME_EVENT_PRE_SUB_LIFE",
    "GAME_EVENT_POST_SUB_LIFE",
    "GAME_EVENT_PRE_LIFE_UP",
    "GAME_EVENT_POST_LIFE_UP",
};

} // namespace

const char *GetScriptGameEventName(int event) {
    return IsIndexInRange(event, static_cast<int>(sizeof(kGameEventNames) / sizeof(kGameEventNames[0])))
               ? kGameEventNames[event]
               : "";
}

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
                                                     CKBOOL dynamic,
                                                     CKContext *context,
                                                     XObjectArray *objectArray,
                                                     CKObject *masterObject)
    : m_Filename(SafeString(filename)),
      m_IsMap(isMap != 0),
      m_MasterName(SafeString(masterName)),
      m_FilterClass(static_cast<int>(filterClass)),
      m_AddToScene(addToScene != 0),
      m_ReuseMeshes(reuseMeshes != 0),
      m_ReuseMaterials(reuseMaterials != 0),
      m_Dynamic(dynamic != 0),
      m_Context(context),
      m_ObjectArray(objectArray),
      m_MasterObject(masterObject) {}

std::string ScriptLoadObjectEventView::GetFilename() const {
    return SafeString(m_Filename);
}

std::string ScriptLoadObjectEventView::GetMasterName() const {
    return SafeString(m_MasterName);
}

int ScriptLoadObjectEventView::GetObjectCount() const {
    return m_ObjectArray ? static_cast<int>(m_ObjectArray->Size()) : 0;
}

int ScriptLoadObjectEventView::GetObjectId(int index) const {
    if (!m_ObjectArray || !IsIndexInRange(index, GetObjectCount()))
        return 0;
    return static_cast<int>(m_ObjectArray->GetObjectID(static_cast<unsigned int>(index)));
}

CKObject *ScriptLoadObjectEventView::BorrowObject(int index) const {
    if (!m_Context || !m_ObjectArray || !IsIndexInRange(index, GetObjectCount()))
        return nullptr;
    return m_ObjectArray->GetObject(m_Context, static_cast<unsigned int>(index));
}

ScriptLoadScriptEventView::ScriptLoadScriptEventView(const char *filename, CKBehavior *script)
    : m_Filename(SafeString(filename)), m_ScriptId(script ? static_cast<int>(script->GetID()) : 0), m_Script(script) {}

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

ScriptConfigEventView::ScriptConfigEventView(const char *modId, const char *category, const char *key, IProperty *property)
    : m_ModId(SafeString(modId)),
      m_Category(SafeString(category)),
      m_Key(SafeString(key)),
      m_Type(property ? static_cast<int>(property->GetType()) : static_cast<int>(IProperty::NONE)),
      m_HasProperty(property != nullptr) {}

std::string ScriptConfigEventView::GetModId() const {
    return SafeString(m_ModId);
}

std::string ScriptConfigEventView::GetCategory() const {
    return SafeString(m_Category);
}

std::string ScriptConfigEventView::GetKey() const {
    return SafeString(m_Key);
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
                                                       CKMesh **convexMesh,
                                                       int ballCnt,
                                                       VxVector *ballCenter,
                                                       float *ballRadius,
                                                       int concaveCnt,
                                                       CKMesh **concaveMesh)
    : m_Target(target),
      m_TargetId(target ? static_cast<int>(target->GetID()) : 0),
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
      m_ConvexMesh(convexMesh),
      m_BallCount(ballCnt),
      m_BallCenter(ballCenter),
      m_BallRadius(ballRadius),
      m_ConcaveCount(concaveCnt),
      m_ConcaveMesh(concaveMesh) {}

std::string ScriptPhysicalizeEventView::GetTargetName() const {
    return SafeString(m_TargetName);
}

std::string ScriptPhysicalizeEventView::GetCollisionGroup() const {
    return SafeString(m_CollisionGroup);
}

std::string ScriptPhysicalizeEventView::GetCollisionSurface() const {
    return SafeString(m_CollisionSurface);
}

CKMesh *ScriptPhysicalizeEventView::BorrowConvexMesh(int index) const {
    return m_ConvexMesh && IsIndexInRange(index, m_ConvexCount) ? m_ConvexMesh[index] : nullptr;
}

VxVector ScriptPhysicalizeEventView::GetBallCenter(int index) const {
    return m_BallCenter && IsIndexInRange(index, m_BallCount) ? m_BallCenter[index] : VxVector();
}

float ScriptPhysicalizeEventView::GetBallRadius(int index) const {
    return m_BallRadius && IsIndexInRange(index, m_BallCount) ? m_BallRadius[index] : 0.0f;
}

CKMesh *ScriptPhysicalizeEventView::BorrowConcaveMesh(int index) const {
    return m_ConcaveMesh && IsIndexInRange(index, m_ConcaveCount) ? m_ConcaveMesh[index] : nullptr;
}

ScriptObjectEventView::ScriptObjectEventView(CK3dEntity *target)
    : m_Target(target),
      m_TargetId(target ? static_cast<int>(target->GetID()) : 0),
      m_TargetName(ObjectName(target)) {}

std::string ScriptObjectEventView::GetTargetName() const {
    return SafeString(m_TargetName);
}

} // namespace BML

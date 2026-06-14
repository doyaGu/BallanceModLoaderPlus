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

static CK_ID ObjectId(CKObject *object) {
    return object ? object->GetID() : 0;
}

static CKObject *ResolveObject(CKContext *context, CK_ID id, CK_CLASSID classId) {
    CKObject *object = context && id != 0 ? context->GetObject(id) : nullptr;
    return object && CKIsChildClassOf(object, classId) ? object : nullptr;
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
      m_MasterObjectId(ObjectId(masterObject)) {
    const int count = objectArray ? static_cast<int>(objectArray->Size()) : 0;
    m_ObjectIds.reserve(count > 0 ? static_cast<std::size_t>(count) : 0u);
    for (int index = 0; index < count; ++index)
        m_ObjectIds.push_back(static_cast<CK_ID>(objectArray->GetObjectID(static_cast<unsigned int>(index))));
}

std::string ScriptLoadObjectEventView::GetFilename() const {
    return m_Filename;
}

std::string ScriptLoadObjectEventView::GetMasterName() const {
    return m_MasterName;
}

int ScriptLoadObjectEventView::GetObjectCount() const {
    return static_cast<int>(m_ObjectIds.size());
}

int ScriptLoadObjectEventView::GetObjectId(int index) const {
    if (!IsIndexInRange(index, GetObjectCount()))
        return 0;
    return static_cast<int>(m_ObjectIds[static_cast<std::size_t>(index)]);
}

CKObject *ScriptLoadObjectEventView::BorrowObject(int index) const {
    if (!IsIndexInRange(index, GetObjectCount()))
        return nullptr;
    return ResolveObject(m_ObjectIds[static_cast<std::size_t>(index)], CKCID_OBJECT);
}

CKObject *ScriptLoadObjectEventView::BorrowMasterObject() const {
    return ResolveObject(m_MasterObjectId, CKCID_OBJECT);
}

CKObject *ScriptLoadObjectEventView::ResolveObject(CK_ID id, CK_CLASSID classId) const {
    return BML::ResolveObject(m_Context, id, classId);
}

ScriptLoadScriptEventView::ScriptLoadScriptEventView(CKContext *context, const char *filename, CKBehavior *script)
    : m_Context(context), m_Filename(SafeString(filename)), m_ScriptId(ObjectId(script)) {}

std::string ScriptLoadScriptEventView::GetFilename() const {
    return m_Filename;
}

CKBehavior *ScriptLoadScriptEventView::BorrowScript() const {
    return static_cast<CKBehavior *>(ResolveObject(m_Context, m_ScriptId, CKCID_BEHAVIOR));
}

ScriptCommandEventView::ScriptCommandEventView(ScriptCommandEventPhase phase,
                                               ICommand *command,
                                               const std::vector<std::string> *args)
    : m_Phase(phase), m_IsCheat(command && command->IsCheat()) {
    if (command)
        m_CommandName = command->GetName();
    else if (args && !args->empty())
        m_CommandName = (*args)[0];

    const std::size_t start = args && !args->empty() ? 1u : 0u;
    if (args && args->size() > start) {
        auto firstArg = args->begin();
        if (start != 0)
            ++firstArg;
        m_Args.assign(firstArg, args->end());
    }
}

std::string ScriptCommandEventView::GetCommandName() const {
    return m_CommandName;
}

int ScriptCommandEventView::GetArgCount() const {
    return static_cast<int>(m_Args.size());
}

std::string ScriptCommandEventView::GetArg(int index) const {
    if (!IsIndexInRange(index, GetArgCount()))
        return {};
    return m_Args[static_cast<std::size_t>(index)];
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
    return m_IsCheat;
}

ScriptConfigEventView::ScriptConfigEventView(const char *modId, const char *category, const char *key, IProperty *property)
    : m_ModId(SafeString(modId)),
      m_Category(SafeString(category)),
      m_Key(SafeString(key)),
      m_Type(property ? static_cast<int>(property->GetType()) : static_cast<int>(IProperty::NONE)),
      m_HasProperty(property != nullptr) {}

std::string ScriptConfigEventView::GetModId() const {
    return m_ModId;
}

std::string ScriptConfigEventView::GetCategory() const {
    return m_Category;
}

std::string ScriptConfigEventView::GetKey() const {
    return m_Key;
}

ScriptPhysicalizeEventView::ScriptPhysicalizeEventView(CKContext *context,
                                                       CK3dEntity *target,
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
    : m_Context(context),
      m_TargetId(ObjectId(target)),
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
      m_ConvexCount(convexCnt > 0 ? convexCnt : 0),
      m_BallCount(ballCnt > 0 ? ballCnt : 0),
      m_ConcaveCount(concaveCnt > 0 ? concaveCnt : 0) {
    m_ConvexMeshIds.reserve(static_cast<std::size_t>(m_ConvexCount));
    for (int index = 0; index < m_ConvexCount; ++index)
        m_ConvexMeshIds.push_back(ObjectId(convexMesh ? convexMesh[index] : nullptr));

    m_BallCenters.reserve(static_cast<std::size_t>(m_BallCount));
    m_BallRadii.reserve(static_cast<std::size_t>(m_BallCount));
    for (int index = 0; index < m_BallCount; ++index) {
        m_BallCenters.push_back(ballCenter ? ballCenter[index] : VxVector());
        m_BallRadii.push_back(ballRadius ? ballRadius[index] : 0.0f);
    }

    m_ConcaveMeshIds.reserve(static_cast<std::size_t>(m_ConcaveCount));
    for (int index = 0; index < m_ConcaveCount; ++index)
        m_ConcaveMeshIds.push_back(ObjectId(concaveMesh ? concaveMesh[index] : nullptr));
}

std::string ScriptPhysicalizeEventView::GetTargetName() const {
    return m_TargetName;
}

std::string ScriptPhysicalizeEventView::GetCollisionGroup() const {
    return m_CollisionGroup;
}

std::string ScriptPhysicalizeEventView::GetCollisionSurface() const {
    return m_CollisionSurface;
}

CKMesh *ScriptPhysicalizeEventView::BorrowConvexMesh(int index) const {
    return IsIndexInRange(index, static_cast<int>(m_ConvexMeshIds.size()))
               ? static_cast<CKMesh *>(ResolveObject(m_ConvexMeshIds[static_cast<std::size_t>(index)], CKCID_MESH))
               : nullptr;
}

VxVector ScriptPhysicalizeEventView::GetBallCenter(int index) const {
    return IsIndexInRange(index, static_cast<int>(m_BallCenters.size()))
               ? m_BallCenters[static_cast<std::size_t>(index)]
               : VxVector();
}

float ScriptPhysicalizeEventView::GetBallRadius(int index) const {
    return IsIndexInRange(index, static_cast<int>(m_BallRadii.size()))
               ? m_BallRadii[static_cast<std::size_t>(index)]
               : 0.0f;
}

CKMesh *ScriptPhysicalizeEventView::BorrowConcaveMesh(int index) const {
    return IsIndexInRange(index, static_cast<int>(m_ConcaveMeshIds.size()))
               ? static_cast<CKMesh *>(ResolveObject(m_ConcaveMeshIds[static_cast<std::size_t>(index)], CKCID_MESH))
               : nullptr;
}

CK3dEntity *ScriptPhysicalizeEventView::BorrowTarget() const {
    return static_cast<CK3dEntity *>(ResolveObject(m_TargetId, CKCID_3DENTITY));
}

CKObject *ScriptPhysicalizeEventView::ResolveObject(CK_ID id, CK_CLASSID classId) const {
    return BML::ResolveObject(m_Context, id, classId);
}

ScriptObjectEventView::ScriptObjectEventView(CKContext *context, CK3dEntity *target)
    : m_Context(context),
      m_TargetId(ObjectId(target)),
      m_TargetName(ObjectName(target)) {}

std::string ScriptObjectEventView::GetTargetName() const {
    return m_TargetName;
}

CK3dEntity *ScriptObjectEventView::BorrowTarget() const {
    return static_cast<CK3dEntity *>(ResolveObject(m_Context, m_TargetId, CKCID_3DENTITY));
}

} // namespace BML

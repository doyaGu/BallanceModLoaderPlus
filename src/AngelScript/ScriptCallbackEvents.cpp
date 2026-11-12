#include "ScriptCallbackEvents.h"
#include "Config/Config.h"
#include "Loader/ModContext.h"
#include "ScriptMod.h"

#include <new>

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

static BML_ObjectRef ReferenceObject(CKContext *engine, CKObject *object) {
    ModContextLease context;
    return context && context->GetCKContext() == engine ? context->ObjectRefs().Issue(object) : BML_ObjectRef{};
}
static CKObject *ResolveObject(BML_ObjectRef reference, CK_CLASSID classId) {
    ModContextLease context;
    if (!context || !context->IsMainThread()) return nullptr;
    auto *object = context->ObjectRefs().Resolve(reference);
    return object && CKIsChildClassOf(object->GetClassID(), classId) ? object : nullptr;
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
      m_MasterObjectId(ObjectId(masterObject)), m_MasterRef(ReferenceObject(context, masterObject)) {
    const int count = objectArray ? static_cast<int>(objectArray->Size()) : 0;
    m_ObjectIds.reserve(count > 0 ? static_cast<std::size_t>(count) : 0u);
    m_ObjectRefs.reserve(count > 0 ? static_cast<size_t>(count) : 0);
    for (int index = 0; index < count; ++index) {
        const auto id = static_cast<CK_ID>(objectArray->GetObjectID(static_cast<unsigned int>(index)));
        m_ObjectIds.push_back(id);
        m_ObjectRefs.push_back(ReferenceObject(context, context ? context->GetObject(id) : nullptr));
    }
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
    return ResolveObject(m_ObjectRefs[static_cast<std::size_t>(index)], CKCID_OBJECT);
}

CKObject *ScriptLoadObjectEventView::BorrowMasterObject() const {
    return ResolveObject(m_MasterRef, CKCID_OBJECT);
}

CKObject *ScriptLoadObjectEventView::ResolveObject(BML_ObjectRef reference, CK_CLASSID classId) const {
    return BML::ResolveObject(reference, classId);
}

ScriptLoadScriptEventView::ScriptLoadScriptEventView(CKContext *context, const char *filename, CKBehavior *script)
    : m_Filename(SafeString(filename)), m_ScriptId(ObjectId(script)), m_ScriptRef(ReferenceObject(context, script)) {}

std::string ScriptLoadScriptEventView::GetFilename() const {
    return m_Filename;
}

CKBehavior *ScriptLoadScriptEventView::BorrowScript() const {
    return static_cast<CKBehavior *>(ResolveObject(m_ScriptRef, CKCID_BEHAVIOR));
}

ScriptCommandEventView::ScriptCommandEventView(ScriptCommandEventPhase phase,
                                               BML::CommandRecord *command,
                                               const std::vector<std::string> *args)
    : m_Phase(phase), m_IsCheat(command && command->Info.Cheat) {
    if (command)
        m_CommandName = command->Info.Name;
    else if (args && !args->empty())
        m_CommandName = (*args)[0];

    m_InvokedAs = args && !args->empty() ? args->front() : m_CommandName;
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

int ScriptCommandEventView::ReadArgument(unsigned index, std::string &out) const try {
    if (index >= m_Args.size())
        return BML_ERROR_NOT_FOUND;
    out = m_Args[index];
    return BML_OK;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
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

ScriptConfigEventView::ScriptConfigEventView(const char *modId, const char *category,
                                             const char *key, Property *property)
    : m_ModId(SafeString(modId)), m_Category(SafeString(category)), m_Key(SafeString(key)),
      m_Type(property ? static_cast<int>(property->GetType())
                      : static_cast<int>(PropertyType::NONE)),
      m_HasProperty(property != nullptr) {
    ModContextLease context;
    auto *record = context ? context->GetModLoader().FindMod(m_ModId.c_str()) : nullptr;
    if (record && record->Script) {
        m_PropertyScope = record->Script->BorrowContextView()->RuntimeScope();
        m_PropertyHandle = context->GetConfigApi().HandleFor(*record, property);
    }
}

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
    : m_TargetId(ObjectId(target)),
      m_TargetRef(ReferenceObject(context, target)),
      m_TargetName(ObjectName(target)),
      m_Fixed(fixed != 0),
      m_Friction(friction),
      m_Elasticity(elasticity),
      m_Mass(mass),
      m_HasCollisionGroup(collGroup != nullptr), m_HasCollisionSurface(collSurface != nullptr),
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
    m_ConvexMeshRefs.reserve(static_cast<std::size_t>(m_ConvexCount));
    for (int index = 0; index < m_ConvexCount; ++index)
        m_ConvexMeshRefs.push_back(ReferenceObject(context, convexMesh ? convexMesh[index] : nullptr));

    m_BallCenters.reserve(static_cast<std::size_t>(m_BallCount));
    m_BallRadii.reserve(static_cast<std::size_t>(m_BallCount));
    for (int index = 0; index < m_BallCount; ++index) {
        m_BallCenters.push_back(ballCenter ? ballCenter[index] : VxVector());
        m_BallRadii.push_back(ballRadius ? ballRadius[index] : 0.0f);
    }

    m_ConcaveMeshRefs.reserve(static_cast<std::size_t>(m_ConcaveCount));
    for (int index = 0; index < m_ConcaveCount; ++index)
        m_ConcaveMeshRefs.push_back(ReferenceObject(context, concaveMesh ? concaveMesh[index] : nullptr));
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
    return IsIndexInRange(index, static_cast<int>(m_ConvexMeshRefs.size()))
               ? static_cast<CKMesh *>(ResolveObject(m_ConvexMeshRefs[static_cast<std::size_t>(index)], CKCID_MESH))
               : nullptr;
}

VxVector ScriptPhysicalizeEventView::GetBallCenter(int index) const {
    return IsIndexInRange(index, static_cast<int>(m_BallCenters.size()))
               ? m_BallCenters[static_cast<std::size_t>(index)]
               : VxVector(0, 0, 0);
}

float ScriptPhysicalizeEventView::GetBallRadius(int index) const {
    return IsIndexInRange(index, static_cast<int>(m_BallRadii.size()))
               ? m_BallRadii[static_cast<std::size_t>(index)]
               : 0.0f;
}

CKMesh *ScriptPhysicalizeEventView::BorrowConcaveMesh(int index) const {
    return IsIndexInRange(index, static_cast<int>(m_ConcaveMeshRefs.size()))
               ? static_cast<CKMesh *>(ResolveObject(m_ConcaveMeshRefs[static_cast<std::size_t>(index)], CKCID_MESH))
               : nullptr;
}

CK3dEntity *ScriptPhysicalizeEventView::BorrowTarget() const {
    return static_cast<CK3dEntity *>(ResolveObject(m_TargetRef, CKCID_3DENTITY));
}

CKObject *ScriptPhysicalizeEventView::ResolveObject(BML_ObjectRef reference, CK_CLASSID classId) const {
    return BML::ResolveObject(reference, classId);
}

ScriptObjectEventView::ScriptObjectEventView(CKContext *context, CK3dEntity *target)
    : m_TargetId(ObjectId(target)),
      m_TargetRef(ReferenceObject(context, target)),
      m_TargetName(ObjectName(target)) {}

std::string ScriptObjectEventView::GetTargetName() const {
    return m_TargetName;
}

CK3dEntity *ScriptObjectEventView::BorrowTarget() const {
    return static_cast<CK3dEntity *>(ResolveObject(m_TargetRef, CKCID_3DENTITY));
}

} // namespace BML

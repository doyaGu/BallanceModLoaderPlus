#include "ScriptModContextView.h"

#include <angelscript.h>

#include "BML/IMod.h"
#include "BML/ILogger.h"
#include "ModContext.h"
#include "ScriptFacadeAccess.h"
#include "ScriptMod.h"

namespace BML {

ScriptModContextView::ScriptModContextView(ModContext *context, ScriptMod *owner)
    : m_Context(context), m_Owner(owner) {}

std::string ScriptModContextView::GetModId() const {
    return m_Owner ? m_Owner->GetID() : "";
}

std::string ScriptModContextView::GetModName() const {
    return m_Owner ? m_Owner->GetName() : "";
}

CKContext *ScriptModContextView::GetCKContext() const {
    return m_Context ? m_Context->GetCKContext() : nullptr;
}

CKRenderContext *ScriptModContextView::GetRenderContext() const {
    return m_Context ? m_Context->GetRenderContext() : nullptr;
}

CKAttributeManager *ScriptModContextView::GetAttributeManager() const {
    return m_Context ? m_Context->GetAttributeManager() : nullptr;
}

CKBehaviorManager *ScriptModContextView::GetBehaviorManager() const {
    return m_Context ? m_Context->GetBehaviorManager() : nullptr;
}

CKCollisionManager *ScriptModContextView::GetCollisionManager() const {
    return m_Context ? m_Context->GetCollisionManager() : nullptr;
}

InputHook *ScriptModContextView::GetInputManager() const {
    return m_Context ? m_Context->GetInputManager() : nullptr;
}

CKMessageManager *ScriptModContextView::GetMessageManager() const {
    return m_Context ? m_Context->GetMessageManager() : nullptr;
}

CKPathManager *ScriptModContextView::GetPathManager() const {
    return m_Context ? m_Context->GetPathManager() : nullptr;
}

CKParameterManager *ScriptModContextView::GetParameterManager() const {
    return m_Context ? m_Context->GetParameterManager() : nullptr;
}

CKRenderManager *ScriptModContextView::GetRenderManager() const {
    return m_Context ? m_Context->GetRenderManager() : nullptr;
}

CKSoundManager *ScriptModContextView::GetSoundManager() const {
    return m_Context ? m_Context->GetSoundManager() : nullptr;
}

CKTimeManager *ScriptModContextView::GetTimeManager() const {
    return m_Context ? m_Context->GetTimeManager() : nullptr;
}

bool ScriptModContextView::IsInGame() const {
    return m_Context && m_Context->IsIngame();
}

bool ScriptModContextView::IsInLevel() const {
    return m_Context && m_Context->IsInLevel();
}

bool ScriptModContextView::IsPaused() const {
    return m_Context && m_Context->IsPaused();
}

bool ScriptModContextView::IsPlaying() const {
    return m_Context && m_Context->IsPlaying();
}

bool ScriptModContextView::IsCheatEnabled() const {
    return m_Context && m_Context->IsCheatEnabled();
}

void ScriptModContextView::EnableCheat(bool enable) const {
    if (m_Context)
        m_Context->EnableCheat(enable);
}

void ScriptModContextView::ExitGame() const {
    if (m_Context)
        m_Context->ExitGame();
}

float ScriptModContextView::GetSRScore() const {
    return m_Context ? m_Context->GetSRScore() : 0.0f;
}

int ScriptModContextView::GetHSScore() const {
    return m_Context ? m_Context->GetHSScore() : 0;
}

std::string ScriptModContextView::GetDirectoryUtf8(int type) const {
    if (!m_Context)
        return "";
    const char *directory = m_Context->GetDirectoryUtf8(static_cast<DirectoryType>(type));
    return directory ? directory : "";
}

float ScriptModContextView::GetTimeMs() const {
    return ScriptFacadeAccess::GetTimeMs(GetTimeManager());
}

float ScriptModContextView::GetAbsoluteTimeMs() const {
    return ScriptFacadeAccess::GetAbsoluteTimeMs(GetTimeManager());
}

float ScriptModContextView::GetDeltaTimeMs() const {
    return ScriptFacadeAccess::GetDeltaTimeMs(GetTimeManager());
}

unsigned int ScriptModContextView::GetFrameCount() const {
    return ScriptFacadeAccess::GetFrameCount(GetTimeManager());
}

bool ScriptModContextView::IsKeyboardAttached() const {
    return ScriptFacadeAccess::IsKeyboardAttached(GetInputManager());
}

bool ScriptModContextView::IsMouseAttached() const {
    return ScriptFacadeAccess::IsMouseAttached(GetInputManager());
}

bool ScriptModContextView::IsKeyDown(CKKEYBOARD key) const {
    return ScriptFacadeAccess::IsKeyDown(GetInputManager(), key);
}

bool ScriptModContextView::IsKeyUp(CKKEYBOARD key) const {
    return ScriptFacadeAccess::IsKeyUp(GetInputManager(), key);
}

bool ScriptModContextView::IsKeyPressed(CKKEYBOARD key) const {
    return ScriptFacadeAccess::IsKeyPressed(GetInputManager(), key);
}

bool ScriptModContextView::IsKeyReleased(CKKEYBOARD key) const {
    return ScriptFacadeAccess::IsKeyReleased(GetInputManager(), key);
}

bool ScriptModContextView::IsKeyToggled(CKKEYBOARD key) const {
    return ScriptFacadeAccess::IsKeyToggled(GetInputManager(), key);
}

std::string ScriptModContextView::GetKeyName(CKKEYBOARD key) const {
    return ScriptFacadeAccess::GetKeyName(GetInputManager(), key);
}

int ScriptModContextView::GetKeyFromName(const std::string &name) const {
    return ScriptFacadeAccess::GetKeyFromName(GetInputManager(), name);
}

bool ScriptModContextView::IsMouseButtonDown(CK_MOUSEBUTTON button) const {
    return ScriptFacadeAccess::IsMouseButtonDown(GetInputManager(), button);
}

bool ScriptModContextView::IsMouseClicked(CK_MOUSEBUTTON button) const {
    return ScriptFacadeAccess::IsMouseClicked(GetInputManager(), button);
}

bool ScriptModContextView::IsMouseToggled(CK_MOUSEBUTTON button) const {
    return ScriptFacadeAccess::IsMouseToggled(GetInputManager(), button);
}

Vx2DVector ScriptModContextView::GetMousePosition(bool absolute) const {
    return ScriptFacadeAccess::GetMousePosition(GetInputManager(), absolute);
}

Vx2DVector ScriptModContextView::GetLastMousePosition() const {
    return ScriptFacadeAccess::GetLastMousePosition(GetInputManager());
}

VxVector ScriptModContextView::GetMouseRelativePosition() const {
    return ScriptFacadeAccess::GetMouseRelativePosition(GetInputManager());
}

bool ScriptModContextView::IsObjectValid(CKObject *object) const {
    return ScriptFacadeAccess::IsObjectValid(object);
}

int ScriptModContextView::GetObjectId(CKObject *object) const {
    return ScriptFacadeAccess::GetObjectId(object);
}

std::string ScriptModContextView::GetObjectName(CKObject *object) const {
    return ScriptFacadeAccess::GetObjectName(object);
}

int ScriptModContextView::GetObjectClassId(CKObject *object) const {
    return ScriptFacadeAccess::GetObjectClassId(object);
}

bool ScriptModContextView::IsObjectVisible(CKObject *object) const {
    return ScriptFacadeAccess::IsObjectVisible(object);
}

bool ScriptModContextView::IsObjectDynamic(CKObject *object) const {
    return ScriptFacadeAccess::IsObjectDynamic(object);
}

int ScriptModContextView::GetBeObjectPriority(CKBeObject *object) const {
    return ScriptFacadeAccess::GetBeObjectPriority(object);
}

int ScriptModContextView::GetBeObjectScriptCount(CKBeObject *object) const {
    return ScriptFacadeAccess::GetBeObjectScriptCount(object);
}

int ScriptModContextView::GetBeObjectAttributeCount(CKBeObject *object) const {
    return ScriptFacadeAccess::GetBeObjectAttributeCount(object);
}

VxVector ScriptModContextView::Get3dEntityPosition(CK3dEntity *entity) const {
    return ScriptFacadeAccess::Get3dEntityPosition(entity);
}

VxVector ScriptModContextView::Get3dEntityScale(CK3dEntity *entity, bool local) const {
    return ScriptFacadeAccess::Get3dEntityScale(entity, local);
}

int ScriptModContextView::Get3dEntityChildCount(CK3dEntity *entity) const {
    return ScriptFacadeAccess::Get3dEntityChildCount(entity);
}

CK3dEntity *ScriptModContextView::Get3dEntityParent(CK3dEntity *entity) const {
    return ScriptFacadeAccess::Get3dEntityParent(entity);
}

CKDataArray *ScriptModContextView::GetArrayByName(const std::string &name) const {
    return m_Context ? m_Context->GetArrayByName(name.c_str()) : nullptr;
}

CKGroup *ScriptModContextView::GetGroupByName(const std::string &name) const {
    return m_Context ? m_Context->GetGroupByName(name.c_str()) : nullptr;
}

CKMaterial *ScriptModContextView::GetMaterialByName(const std::string &name) const {
    return m_Context ? m_Context->GetMaterialByName(name.c_str()) : nullptr;
}

CKMesh *ScriptModContextView::GetMeshByName(const std::string &name) const {
    return m_Context ? m_Context->GetMeshByName(name.c_str()) : nullptr;
}

CK2dEntity *ScriptModContextView::Get2dEntityByName(const std::string &name) const {
    return m_Context ? m_Context->Get2dEntityByName(name.c_str()) : nullptr;
}

CK3dEntity *ScriptModContextView::Get3dEntityByName(const std::string &name) const {
    return m_Context ? m_Context->Get3dEntityByName(name.c_str()) : nullptr;
}

CK3dObject *ScriptModContextView::Get3dObjectByName(const std::string &name) const {
    return m_Context ? m_Context->Get3dObjectByName(name.c_str()) : nullptr;
}

CKCamera *ScriptModContextView::GetCameraByName(const std::string &name) const {
    return m_Context ? m_Context->GetCameraByName(name.c_str()) : nullptr;
}

CKTargetCamera *ScriptModContextView::GetTargetCameraByName(const std::string &name) const {
    return m_Context ? m_Context->GetTargetCameraByName(name.c_str()) : nullptr;
}

CKLight *ScriptModContextView::GetLightByName(const std::string &name) const {
    return m_Context ? m_Context->GetLightByName(name.c_str()) : nullptr;
}

CKTargetLight *ScriptModContextView::GetTargetLightByName(const std::string &name) const {
    return m_Context ? m_Context->GetTargetLightByName(name.c_str()) : nullptr;
}

CKSound *ScriptModContextView::GetSoundByName(const std::string &name) const {
    return m_Context ? m_Context->GetSoundByName(name.c_str()) : nullptr;
}

CKTexture *ScriptModContextView::GetTextureByName(const std::string &name) const {
    return m_Context ? m_Context->GetTextureByName(name.c_str()) : nullptr;
}

CKBehavior *ScriptModContextView::GetScriptByName(const std::string &name) const {
    return m_Context ? m_Context->GetScriptByName(name.c_str()) : nullptr;
}

void ScriptModContextView::SetIC(CKBeObject *object, bool hierarchy) const {
    if (m_Context && object)
        m_Context->SetIC(object, hierarchy);
}

void ScriptModContextView::RestoreIC(CKBeObject *object, bool hierarchy) const {
    if (m_Context && object)
        m_Context->RestoreIC(object, hierarchy);
}

void ScriptModContextView::Show(CKBeObject *object, CK_OBJECT_SHOWOPTION show, bool hierarchy) const {
    if (m_Context && object)
        m_Context->Show(object, show, hierarchy);
}

int ScriptModContextView::GetHUD() const {
    return m_Context ? m_Context->GetHUD() : 0;
}

void ScriptModContextView::SetHUD(int mode) const {
    if (m_Context)
        m_Context->SetHUD(mode);
}

void ScriptModContextView::ShowTitle(bool show) const {
    if (m_Context)
        m_Context->ShowTitle(show);
}

void ScriptModContextView::ShowFPS(bool show) const {
    if (m_Context)
        m_Context->ShowFPS(show);
}

void ScriptModContextView::ShowSRTimer(bool show) const {
    if (m_Context)
        m_Context->ShowSRTimer(show);
}

void ScriptModContextView::StartSRTimer() const {
    if (m_Context)
        m_Context->StartSRTimer();
}

void ScriptModContextView::PauseSRTimer() const {
    if (m_Context)
        m_Context->PauseSRTimer();
}

void ScriptModContextView::ResetSRTimer() const {
    if (m_Context)
        m_Context->ResetSRTimer();
}

float ScriptModContextView::GetSRTime() const {
    return m_Context ? m_Context->GetSRTime() : 0.0f;
}

ScriptTimerRef *ScriptModContextView::AddTimer(asIScriptObject *timer) const {
    if (m_Owner)
        return m_Owner->AddScriptTimer(timer);
    return nullptr;
}

ScriptCommandRef *ScriptModContextView::RegisterCommand(asIScriptObject *command) const {
    if (m_Owner)
        return m_Owner->RegisterScriptCommand(command);
    return nullptr;
}

bool ScriptModContextView::UnregisterCommand(const std::string &name) const {
    return m_Owner && m_Owner->UnregisterScriptCommand(name);
}

ScriptDataShareRequestRef *ScriptModContextView::RequestDataShare(asIScriptObject *request) const {
    if (m_Owner)
        return m_Owner->RequestScriptDataShare(request);
    return nullptr;
}

bool ScriptModContextView::RegisterBallType(const std::string &ballFile,
                                            const std::string &ballId,
                                            const std::string &ballName,
                                            const std::string &objName,
                                            float friction,
                                            float elasticity,
                                            float mass,
                                            const std::string &collGroup,
                                            float linearDamp,
                                            float rotDamp,
                                            float force,
                                            float radius) const {
    return m_Owner && m_Owner->RegisterScriptBallType(ballFile, ballId, ballName, objName, friction, elasticity, mass,
                                                      collGroup, linearDamp, rotDamp, force, radius);
}

bool ScriptModContextView::RegisterFloorType(const std::string &floorName,
                                             float friction,
                                             float elasticity,
                                             float mass,
                                             const std::string &collGroup,
                                             bool enableColl) const {
    return m_Owner && m_Owner->RegisterScriptFloorType(floorName, friction, elasticity, mass, collGroup, enableColl);
}

bool ScriptModContextView::RegisterModulBall(const std::string &modulName,
                                             bool fixed,
                                             float friction,
                                             float elasticity,
                                             float mass,
                                             const std::string &collGroup,
                                             bool frozen,
                                             bool enableColl,
                                             bool calcMassCenter,
                                             float linearDamp,
                                             float rotDamp,
                                             float radius) const {
    return m_Owner && m_Owner->RegisterScriptModulBall(modulName, fixed, friction, elasticity, mass, collGroup,
                                                       frozen, enableColl, calcMassCenter, linearDamp, rotDamp, radius);
}

bool ScriptModContextView::RegisterModulConvex(const std::string &modulName,
                                               bool fixed,
                                               float friction,
                                               float elasticity,
                                               float mass,
                                               const std::string &collGroup,
                                               bool frozen,
                                               bool enableColl,
                                               bool calcMassCenter,
                                               float linearDamp,
                                               float rotDamp) const {
    return m_Owner && m_Owner->RegisterScriptModulConvex(modulName, fixed, friction, elasticity, mass, collGroup,
                                                         frozen, enableColl, calcMassCenter, linearDamp, rotDamp);
}

bool ScriptModContextView::RegisterTrafo(const std::string &modulName) const {
    return m_Owner && m_Owner->RegisterScriptTrafo(modulName);
}

bool ScriptModContextView::RegisterModul(const std::string &modulName) const {
    return m_Owner && m_Owner->RegisterScriptModul(modulName);
}

std::string ScriptModContextView::GetModRootUtf8() const {
    return m_Owner ? m_Owner->GetRootDirectoryUtf8() : "";
}

std::string ScriptModContextView::ResolveModPathUtf8(const std::string &relativePath) const {
    return m_Owner ? m_Owner->ResolveResourcePathUtf8(relativePath) : "";
}

bool ScriptModContextView::ModFileExistsUtf8(const std::string &relativePath) const {
    return m_Owner && m_Owner->ModFileExistsUtf8(relativePath);
}

bool ScriptModContextView::ModDirectoryExistsUtf8(const std::string &relativePath) const {
    return m_Owner && m_Owner->ModDirectoryExistsUtf8(relativePath);
}

std::string ScriptModContextView::ReadModTextFileUtf8(const std::string &relativePath,
                                                      const std::string &defaultValue) const {
    return m_Owner ? m_Owner->ReadModTextFileUtf8(relativePath, defaultValue) : defaultValue;
}

int ScriptModContextView::GetCommandCount() const {
    return m_Context ? m_Context->GetCommandCount() : 0;
}

std::string ScriptModContextView::GetCommandName(int index) const {
    ICommand *command = m_Context ? m_Context->GetCommand(index) : nullptr;
    return command ? command->GetName() : "";
}

std::string ScriptModContextView::GetCommandAlias(int index) const {
    ICommand *command = m_Context ? m_Context->GetCommand(index) : nullptr;
    return command ? command->GetAlias() : "";
}

std::string ScriptModContextView::GetCommandDescription(int index) const {
    ICommand *command = m_Context ? m_Context->GetCommand(index) : nullptr;
    return command ? command->GetDescription() : "";
}

bool ScriptModContextView::HasCommand(const std::string &name) const {
    return m_Context && m_Context->FindCommand(name.c_str()) != nullptr;
}

bool ScriptModContextView::IsCommandCheat(const std::string &name) const {
    ICommand *command = m_Context ? m_Context->FindCommand(name.c_str()) : nullptr;
    return command && command->IsCheat();
}

int ScriptModContextView::GetGlobalModCount() const {
    return m_Context ? m_Context->GetModCount() : 0;
}

std::string ScriptModContextView::GetGlobalModId(int index) const {
    IMod *mod = m_Context ? m_Context->GetMod(index) : nullptr;
    return mod ? mod->GetID() : "";
}

void ScriptModContextView::SendIngameMessage(const std::string &message) const {
    if (m_Context)
        m_Context->SendIngameMessage(message.c_str());
}

void ScriptModContextView::ClearIngameMessages() const {
    if (m_Context)
        m_Context->ClearIngameMessages();
}

void ScriptModContextView::ExecuteCommand(const std::string &command) const {
    if (m_Context)
        m_Context->ExecuteCommand(command.c_str());
}

void ScriptModContextView::SkipRenderForNextTick() const {
    if (m_Context)
        m_Context->SkipRenderForNextTick();
}

void ScriptModContextView::OpenModsMenu() const {
    if (m_Context)
        m_Context->OpenModsMenu();
}

void ScriptModContextView::CloseModsMenu() const {
    if (m_Context)
        m_Context->CloseModsMenu();
}

void ScriptModContextView::OpenMapMenu() const {
    if (m_Context)
        m_Context->OpenMapMenu();
}

void ScriptModContextView::CloseMapMenu() const {
    if (m_Context)
        m_Context->CloseMapMenu();
}

std::string ScriptModContextView::GetConfigString(const std::string &key,
                                                  const std::string &defaultValue) const {
    return m_Owner ? m_Owner->GetConfigString(key, defaultValue) : defaultValue;
}

void ScriptModContextView::SetConfigString(const std::string &key, const std::string &value) const {
    if (m_Owner)
        m_Owner->SetConfigString(key, value);
}

bool ScriptModContextView::GetConfigBool(const std::string &key, bool defaultValue) const {
    return m_Owner ? m_Owner->GetConfigBool(key, defaultValue) : defaultValue;
}

void ScriptModContextView::SetConfigBool(const std::string &key, bool value) const {
    if (m_Owner)
        m_Owner->SetConfigBool(key, value);
}

int ScriptModContextView::GetConfigInt(const std::string &key, int defaultValue) const {
    return m_Owner ? m_Owner->GetConfigInt(key, defaultValue) : defaultValue;
}

void ScriptModContextView::SetConfigInt(const std::string &key, int value) const {
    if (m_Owner)
        m_Owner->SetConfigInt(key, value);
}

float ScriptModContextView::GetConfigFloat(const std::string &key, float defaultValue) const {
    return m_Owner ? m_Owner->GetConfigFloat(key, defaultValue) : defaultValue;
}

void ScriptModContextView::SetConfigFloat(const std::string &key, float value) const {
    if (m_Owner)
        m_Owner->SetConfigFloat(key, value);
}

void ScriptModContextView::LogInfo(const std::string &message) const {
    if (m_Context && m_Context->GetLogger())
        m_Context->GetLogger()->Info("[%s] %s", GetModId().c_str(), message.c_str());
}

void ScriptModContextView::LogWarn(const std::string &message) const {
    if (m_Context && m_Context->GetLogger())
        m_Context->GetLogger()->Warn("[%s] %s", GetModId().c_str(), message.c_str());
}

void ScriptModContextView::LogError(const std::string &message) const {
    if (m_Context && m_Context->GetLogger())
        m_Context->GetLogger()->Error("[%s] %s", GetModId().c_str(), message.c_str());
}

} // namespace BML

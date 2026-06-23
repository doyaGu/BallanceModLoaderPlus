#include "ScriptModContextView.h"

#include <angelscript.h>

#include "BML/IMod.h"
#include "BML/ILogger.h"
#include "ModContext.h"
#include "ScriptFacadeAccess.h"
#include "ScriptMod.h"
#include "ScriptModRuntime.h"

namespace BML {

namespace {

bool RejectScriptObjectConstructionHostCall(const char *apiName) {
    return ScriptModRuntime::RecordConstructionHostCallViolation(apiName);
}

} // namespace

ScriptModContextView::ScriptModContextView(ModContext *context, ScriptMod *owner)
    : m_Context(context), m_Owner(owner) {}

std::string ScriptModContextView::GetModId() const {
    return m_Owner ? m_Owner->GetID() : "";
}

std::string ScriptModContextView::GetModName() const {
    return m_Owner ? m_Owner->GetName() : "";
}

bool ScriptModContextView::IsReloading() const {
    return m_Owner && m_Owner->GetReloadPhase() != ScriptModReloadPhase::None;
}

ScriptModReloadPhase ScriptModContextView::GetReloadPhase() const {
    return m_Owner ? m_Owner->GetReloadPhase() : ScriptModReloadPhase::None;
}

unsigned int ScriptModContextView::GetReloadAttemptId() const {
    return m_Owner ? m_Owner->GetReloadAttemptId() : 0;
}

unsigned int ScriptModContextView::GetModGeneration() const {
    return m_Owner ? m_Owner->GetModGeneration() : 0;
}

unsigned int ScriptModContextView::GetRuntimeGeneration() const {
    return m_Owner ? m_Owner->GetRuntimeGeneration() : 0;
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
    if (RejectScriptObjectConstructionHostCall("ModContext::EnableCheat"))
        return;
    if (m_Context)
        m_Context->EnableCheat(enable);
}

void ScriptModContextView::ExitGame() const {
    if (RejectScriptObjectConstructionHostCall("ModContext::ExitGame"))
        return;
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
    if (RejectScriptObjectConstructionHostCall("ModContext::SetIC"))
        return;
    if (m_Context && object)
        m_Context->SetIC(object, hierarchy);
}

void ScriptModContextView::RestoreIC(CKBeObject *object, bool hierarchy) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::RestoreIC"))
        return;
    if (m_Context && object)
        m_Context->RestoreIC(object, hierarchy);
}

void ScriptModContextView::Show(CKBeObject *object, CK_OBJECT_SHOWOPTION show, bool hierarchy) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::Show"))
        return;
    if (m_Context && object)
        m_Context->Show(object, show, hierarchy);
}

int ScriptModContextView::GetHUD() const {
    return m_Context ? m_Context->GetHUD() : 0;
}

void ScriptModContextView::SetHUD(int mode) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::SetHUD"))
        return;
    if (m_Context)
        m_Context->SetHUD(mode);
}

void ScriptModContextView::ShowTitle(bool show) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::ShowTitle"))
        return;
    if (m_Context)
        m_Context->ShowTitle(show);
}

void ScriptModContextView::ShowFPS(bool show) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::ShowFPS"))
        return;
    if (m_Context)
        m_Context->ShowFPS(show);
}

void ScriptModContextView::ShowSRTimer(bool show) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::ShowSRTimer"))
        return;
    if (m_Context)
        m_Context->ShowSRTimer(show);
}

void ScriptModContextView::StartSRTimer() const {
    if (RejectScriptObjectConstructionHostCall("ModContext::StartSRTimer"))
        return;
    if (m_Context)
        m_Context->StartSRTimer();
}

void ScriptModContextView::PauseSRTimer() const {
    if (RejectScriptObjectConstructionHostCall("ModContext::PauseSRTimer"))
        return;
    if (m_Context)
        m_Context->PauseSRTimer();
}

void ScriptModContextView::ResetSRTimer() const {
    if (RejectScriptObjectConstructionHostCall("ModContext::ResetSRTimer"))
        return;
    if (m_Context)
        m_Context->ResetSRTimer();
}

float ScriptModContextView::GetSRTime() const {
    return m_Context ? m_Context->GetSRTime() : 0.0f;
}

ScriptTimerRef *ScriptModContextView::AddTimer(asIScriptObject *timer) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::AddTimer"))
        return nullptr;
    if (m_Owner)
        return m_Owner->AddScriptTimer(timer);
    return nullptr;
}

ScriptTimerRef *ScriptModContextView::SetTimeoutTicks(unsigned int delayTicks,
                                                      asIScriptFunction *callback,
                                                      const std::string &name) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::SetTimeoutTicks"))
        return nullptr;
    return m_Owner ? m_Owner->AddScriptTimeoutTicks(delayTicks, callback, name) : nullptr;
}

ScriptTimerRef *ScriptModContextView::SetTimeout(float delayMs,
                                                 asIScriptFunction *callback,
                                                 const std::string &name) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::SetTimeout"))
        return nullptr;
    return m_Owner ? m_Owner->AddScriptTimeoutMs(delayMs, callback, name) : nullptr;
}

ScriptTimerRef *ScriptModContextView::SetIntervalTicks(unsigned int delayTicks,
                                                       asIScriptFunction *callback,
                                                       const std::string &name) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::SetIntervalTicks"))
        return nullptr;
    return m_Owner ? m_Owner->AddScriptIntervalTicks(delayTicks, callback, name) : nullptr;
}

ScriptTimerRef *ScriptModContextView::SetInterval(float delayMs,
                                                  asIScriptFunction *callback,
                                                  const std::string &name) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::SetInterval"))
        return nullptr;
    return m_Owner ? m_Owner->AddScriptIntervalMs(delayMs, callback, name) : nullptr;
}

ScriptCommandRef *ScriptModContextView::RegisterCommand(asIScriptObject *command) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::RegisterCommand"))
        return nullptr;
    if (m_Owner)
        return m_Owner->RegisterScriptCommand(command);
    return nullptr;
}

ScriptCommandRef *ScriptModContextView::RegisterCommand(const ScriptCommandDefinition &definition,
                                                        asIScriptFunction *execute,
                                                        asIScriptFunction *complete) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::RegisterCommand"))
        return nullptr;
    return m_Owner ? m_Owner->RegisterScriptCommand(definition, execute, complete) : nullptr;
}

bool ScriptModContextView::UnregisterCommand(const std::string &name) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::UnregisterCommand"))
        return false;
    return m_Owner && m_Owner->UnregisterScriptCommand(name);
}

ScriptDataShareRequestRef *ScriptModContextView::RequestDataShare(asIScriptObject *request) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::RequestDataShare"))
        return nullptr;
    if (m_Owner)
        return m_Owner->RequestScriptDataShare(request);
    return nullptr;
}

ScriptDataShareRequestRef *ScriptModContextView::RequestDataShare(const std::string &key,
                                                                  int type,
                                                                  asIScriptFunction *callback,
                                                                  const std::string &name) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::RequestDataShare"))
        return nullptr;
    return m_Owner ? m_Owner->RequestScriptDataShare(key, type, callback, name) : nullptr;
}

ScriptHookBlockRef *ScriptModContextView::CreateHookBlock(CKBehavior *ownerScript,
                                                          asIScriptFunction *callback,
                                                          const std::string &name,
                                                          int inputCount,
                                                          int outputCount) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::CreateHookBlock"))
        return nullptr;
    return m_Owner ? m_Owner->CreateScriptHookBlock(ownerScript, callback, name, inputCount, outputCount) : nullptr;
}

ScriptHookBlockRef *ScriptModContextView::InsertHookBlockAfter(CKBehavior *ownerScript,
                                                               CKBehavior *source,
                                                               asIScriptFunction *callback,
                                                               const std::string &name,
                                                               int sourceOutput,
                                                               int targetInput) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::InsertHookBlockAfter"))
        return nullptr;
    return m_Owner ? m_Owner->InsertScriptHookBlockAfter(ownerScript, source, callback, name, sourceOutput, targetInput) : nullptr;
}

ScriptHookBlockRef *ScriptModContextView::InsertHookBlockBefore(CKBehavior *ownerScript,
                                                                CKBehavior *target,
                                                                asIScriptFunction *callback,
                                                                const std::string &name,
                                                                int sourceOutput,
                                                                int targetInput) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::InsertHookBlockBefore"))
        return nullptr;
    return m_Owner ? m_Owner->InsertScriptHookBlockBefore(ownerScript, target, callback, name, sourceOutput, targetInput) : nullptr;
}

ScriptHookBlockRef *ScriptModContextView::InsertHookBlockBetween(CKBehavior *ownerScript,
                                                                 CKBehavior *source,
                                                                 CKBehavior *target,
                                                                 asIScriptFunction *callback,
                                                                 const std::string &name,
                                                                 int sourceOutput,
                                                                 int targetInput) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::InsertHookBlockBetween"))
        return nullptr;
    return m_Owner ? m_Owner->InsertScriptHookBlockBetween(ownerScript, source, target, callback, name, sourceOutput, targetInput) : nullptr;
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
    if (RejectScriptObjectConstructionHostCall("ModContext::RegisterBallType"))
        return false;
    return m_Owner && m_Owner->RegisterScriptBallType(ballFile, ballId, ballName, objName, friction, elasticity, mass,
                                                      collGroup, linearDamp, rotDamp, force, radius);
}

bool ScriptModContextView::RegisterFloorType(const std::string &floorName,
                                             float friction,
                                             float elasticity,
                                             float mass,
                                             const std::string &collGroup,
                                             bool enableColl) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::RegisterFloorType"))
        return false;
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
    if (RejectScriptObjectConstructionHostCall("ModContext::RegisterModulBall"))
        return false;
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
    if (RejectScriptObjectConstructionHostCall("ModContext::RegisterModulConvex"))
        return false;
    return m_Owner && m_Owner->RegisterScriptModulConvex(modulName, fixed, friction, elasticity, mass, collGroup,
                                                         frozen, enableColl, calcMassCenter, linearDamp, rotDamp);
}

bool ScriptModContextView::RegisterTrafo(const std::string &modulName) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::RegisterTrafo"))
        return false;
    return m_Owner && m_Owner->RegisterScriptTrafo(modulName);
}

bool ScriptModContextView::RegisterModul(const std::string &modulName) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::RegisterModul"))
        return false;
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
    if (RejectScriptObjectConstructionHostCall("ModContext::SendIngameMessage"))
        return;
    if (m_Context)
        m_Context->SendIngameMessage(message.c_str());
}

void ScriptModContextView::ClearIngameMessages() const {
    if (RejectScriptObjectConstructionHostCall("ModContext::ClearIngameMessages"))
        return;
    if (m_Context)
        m_Context->ClearIngameMessages();
}

void ScriptModContextView::ExecuteCommand(const std::string &command) const {
    if (RejectScriptObjectConstructionHostCall("ModContext::ExecuteCommand"))
        return;
    if (m_Context)
        m_Context->ExecuteCommand(command.c_str());
}

void ScriptModContextView::SkipRenderForNextTick() const {
    if (RejectScriptObjectConstructionHostCall("ModContext::SkipRenderForNextTick"))
        return;
    if (m_Context)
        m_Context->SkipRenderForNextTick();
}

void ScriptModContextView::OpenModsMenu() const {
    if (RejectScriptObjectConstructionHostCall("ModContext::OpenModsMenu"))
        return;
    if (m_Context)
        m_Context->OpenModsMenu();
}

void ScriptModContextView::CloseModsMenu() const {
    if (RejectScriptObjectConstructionHostCall("ModContext::CloseModsMenu"))
        return;
    if (m_Context)
        m_Context->CloseModsMenu();
}

void ScriptModContextView::OpenMapMenu() const {
    if (RejectScriptObjectConstructionHostCall("ModContext::OpenMapMenu"))
        return;
    if (m_Context)
        m_Context->OpenMapMenu();
}

void ScriptModContextView::CloseMapMenu() const {
    if (RejectScriptObjectConstructionHostCall("ModContext::CloseMapMenu"))
        return;
    if (m_Context)
        m_Context->CloseMapMenu();
}

} // namespace BML

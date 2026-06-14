#include "AngelScriptBindings.h"

#include "ModContext.h"

#include <string>

#include "BML/BML.h"
#include "BML/Bui.h"
#include "BML/DataShare.h"
#include "BML/InputHook.h"
#include "BML/Interop.h"
#include "BML/ILogger.h"
#include "CallFrameInternal.h"

#if BML_ENABLE_ANGELSCRIPT

#include <cstdint>
#include <cstring>
#include <new>
#include <utility>

#include <angelscript.h>

#include "AngelScript/generated/BMLImGuiAngelScriptBindings.h"
#include "AngelScriptImGuiBindings.h"
#include "BMLMod.h"
#include "CKAngelScriptAdapter.h"
#include "ScriptApiContract.h"
#include "ScriptCallbackEvents.h"
#include "ScriptFacadeAccess.h"
#include "ScriptMod.h"
#include "ScriptModContextView.h"
#include "ScriptTimerService.h"

static constexpr const char *kExtensionName = "BML";
static constexpr CKDWORD kRegistrationRetryTicks = 300;

static CKAngelScriptAdapter g_AngelScriptHost;
static std::string g_LastRegistrationError;
static CKAngelScriptAdapter::State g_LastUnavailableState = CKAngelScriptAdapter::State::Unchecked;
static std::string g_LastUnavailableDiagnostic;
static CKDWORD g_NextRegistrationAttemptTick = 0;

static ModContext *GetActiveContext() {
    ModContext *context = BML_GetModContext();
    return context && context->IsInited() ? context : nullptr;
}

static bool RequireContext(ModContext *&context) {
    context = GetActiveContext();
    return context != nullptr;
}

static bool RequireLoadedContext(ModContext *&context) {
    return RequireContext(context) && context->AreModsLoaded();
}

static std::string CopyAndFree(char *value) {
    if (!value)
        return {};
    std::string result(value);
    BML_FreeString(value);
    return result;
}

std::string BMLAS_GetVersion() { return BML_VERSION; }
int BMLAS_GetVersionMajor() { return BML_MAJOR_VERSION; }
int BMLAS_GetVersionMinor() { return BML_MINOR_VERSION; }
int BMLAS_GetVersionPatch() { return BML_PATCH_VERSION; }

std::string BMLAS_GetErrorString(int errorCode) {
    const char *message = BML_GetErrorString(errorCode);
    return message ? message : "";
}

bool BMLAS_IsInitialized() { return GetActiveContext() != nullptr; }
bool BMLAS_IsIngame() { ModContext *ctx = nullptr; return RequireContext(ctx) && ctx->IsIngame(); }
bool BMLAS_IsInLevel() { ModContext *ctx = nullptr; return RequireContext(ctx) && ctx->IsInLevel(); }
bool BMLAS_IsPaused() { ModContext *ctx = nullptr; return RequireContext(ctx) && ctx->IsPaused(); }
bool BMLAS_IsPlaying() { ModContext *ctx = nullptr; return RequireContext(ctx) && ctx->IsPlaying(); }
bool BMLAS_IsCheatEnabled() { ModContext *ctx = nullptr; return RequireContext(ctx) && ctx->IsCheatEnabled(); }

void BMLAS_EnableCheat(bool enable) {
    ModContext *ctx = nullptr;
    if (RequireContext(ctx))
        ctx->EnableCheat(enable);
}

void BMLAS_SendIngameMessage(const std::string &message) {
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->SendIngameMessage(message.c_str());
}

void BMLAS_ExecuteCommand(const std::string &command) {
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->ExecuteCommand(command.c_str());
}

void BMLAS_OpenModsMenu() {
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->OpenModsMenu();
}

void BMLAS_CloseModsMenu() {
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->CloseModsMenu();
}

void BMLAS_OpenMapMenu() {
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->OpenMapMenu();
}

void BMLAS_CloseMapMenu() {
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->CloseMapMenu();
}

void BMLAS_ClearIngameMessages() {
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->ClearIngameMessages();
}

float BMLAS_GetSRScore() {
    ModContext *ctx = nullptr;
    return RequireLoadedContext(ctx) ? ctx->GetSRScore() : 0.0f;
}

int BMLAS_GetHSScore() {
    ModContext *ctx = nullptr;
    return RequireLoadedContext(ctx) ? ctx->GetHSScore() : 0;
}

int BMLAS_GetHUD() {
    ModContext *ctx = nullptr;
    return RequireLoadedContext(ctx) ? ctx->GetHUD() : 0;
}

void BMLAS_SetHUD(int mode) {
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->SetHUD(mode);
}

void BMLAS_ShowTitle(bool show) {
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->ShowTitle(show);
}

void BMLAS_ShowFPS(bool show) {
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->ShowFPS(show);
}

void BMLAS_ShowSRTimer(bool show) {
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->ShowSRTimer(show);
}

void BMLAS_StartSRTimer() {
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->StartSRTimer();
}

void BMLAS_PauseSRTimer() {
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->PauseSRTimer();
}

void BMLAS_ResetSRTimer() {
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->ResetSRTimer();
}

float BMLAS_GetSRTime() {
    ModContext *ctx = nullptr;
    return RequireLoadedContext(ctx) ? ctx->GetSRTime() : 0.0f;
}

void BMLAS_SkipRenderForNextTick() {
    ModContext *ctx = nullptr;
    if (RequireContext(ctx))
        ctx->SkipRenderForNextTick();
}

CKContext *BMLAS_GetCKContext() {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetCKContext() : nullptr;
}

CKRenderContext *BMLAS_GetRenderContext() {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetRenderContext() : nullptr;
}

void BMLAS_ExitGame() {
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->ExitGame();
}

CKAttributeManager *BMLAS_GetAttributeManager() {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetAttributeManager() : nullptr;
}

CKBehaviorManager *BMLAS_GetBehaviorManager() {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetBehaviorManager() : nullptr;
}

CKCollisionManager *BMLAS_GetCollisionManager() {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetCollisionManager() : nullptr;
}

CKMessageManager *BMLAS_GetMessageManager() {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetMessageManager() : nullptr;
}

CKPathManager *BMLAS_GetPathManager() {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetPathManager() : nullptr;
}

CKParameterManager *BMLAS_GetParameterManager() {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetParameterManager() : nullptr;
}

CKRenderManager *BMLAS_GetRenderManager() {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetRenderManager() : nullptr;
}

CKSoundManager *BMLAS_GetSoundManager() {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetSoundManager() : nullptr;
}

CKTimeManager *BMLAS_GetTimeManager() {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetTimeManager() : nullptr;
}

float BMLAS_GetTimeMs() {
    ModContext *ctx = nullptr;
    return BML::ScriptFacadeAccess::GetTimeMs(RequireContext(ctx) ? ctx->GetTimeManager() : nullptr);
}

float BMLAS_GetAbsoluteTimeMs() {
    ModContext *ctx = nullptr;
    return BML::ScriptFacadeAccess::GetAbsoluteTimeMs(RequireContext(ctx) ? ctx->GetTimeManager() : nullptr);
}

float BMLAS_GetDeltaTimeMs() {
    ModContext *ctx = nullptr;
    return BML::ScriptFacadeAccess::GetDeltaTimeMs(RequireContext(ctx) ? ctx->GetTimeManager() : nullptr);
}

uint32_t BMLAS_GetFrameCount() {
    ModContext *ctx = nullptr;
    return BML::ScriptFacadeAccess::GetFrameCount(RequireContext(ctx) ? ctx->GetTimeManager() : nullptr);
}

std::string BMLAS_GetDirectoryUtf8(DirectoryType type) {
    ModContext *ctx = nullptr;
    if (!RequireContext(ctx))
        return {};
    const char *dir = ctx->GetDirectoryUtf8(type);
    return dir ? dir : "";
}

InputHook *BMLAS_GetInputHook() {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetInputManager() : nullptr;
}

static bool BMLAS_InputHookIsKeyboardAttached(InputHook *input) { return BML::ScriptFacadeAccess::IsKeyboardAttached(input); }
static bool BMLAS_InputHookIsMouseAttached(InputHook *input) { return BML::ScriptFacadeAccess::IsMouseAttached(input); }
static bool BMLAS_InputHookIsKeyDown(InputHook *input, CKKEYBOARD key) { return BML::ScriptFacadeAccess::IsKeyDown(input, key); }
static bool BMLAS_InputHookIsKeyUp(InputHook *input, CKKEYBOARD key) { return BML::ScriptFacadeAccess::IsKeyUp(input, key); }
static bool BMLAS_InputHookIsKeyPressed(InputHook *input, CKKEYBOARD key) { return BML::ScriptFacadeAccess::IsKeyPressed(input, key); }
static bool BMLAS_InputHookIsKeyReleased(InputHook *input, CKKEYBOARD key) { return BML::ScriptFacadeAccess::IsKeyReleased(input, key); }
static bool BMLAS_InputHookIsKeyToggled(InputHook *input, CKKEYBOARD key) { return BML::ScriptFacadeAccess::IsKeyToggled(input, key); }
static std::string BMLAS_InputHookGetKeyName(InputHook *input, CKKEYBOARD key) {
    return BML::ScriptFacadeAccess::GetKeyName(input, key);
}
static int BMLAS_InputHookGetKeyFromName(InputHook *input, const std::string &name) {
    return BML::ScriptFacadeAccess::GetKeyFromName(input, name);
}
static bool BMLAS_InputHookIsMouseButtonDown(InputHook *input, CK_MOUSEBUTTON button) { return BML::ScriptFacadeAccess::IsMouseButtonDown(input, button); }
static bool BMLAS_InputHookIsMouseClicked(InputHook *input, CK_MOUSEBUTTON button) { return BML::ScriptFacadeAccess::IsMouseClicked(input, button); }
static bool BMLAS_InputHookIsMouseToggled(InputHook *input, CK_MOUSEBUTTON button) { return BML::ScriptFacadeAccess::IsMouseToggled(input, button); }
static Vx2DVector BMLAS_InputHookGetMousePosition(InputHook *input, bool absolute) {
    return BML::ScriptFacadeAccess::GetMousePosition(input, absolute);
}
static Vx2DVector BMLAS_InputHookGetLastMousePosition(InputHook *input) {
    return BML::ScriptFacadeAccess::GetLastMousePosition(input);
}
static VxVector BMLAS_InputHookGetMouseRelativePosition(InputHook *input) {
    return BML::ScriptFacadeAccess::GetMouseRelativePosition(input);
}

bool BMLAS_IsKeyboardAttached() {
    return BML::ScriptFacadeAccess::IsKeyboardAttached(BMLAS_GetInputHook());
}

bool BMLAS_IsMouseAttached() {
    return BML::ScriptFacadeAccess::IsMouseAttached(BMLAS_GetInputHook());
}

bool BMLAS_IsKeyDown(CKKEYBOARD key) {
    return BML::ScriptFacadeAccess::IsKeyDown(BMLAS_GetInputHook(), key);
}

bool BMLAS_IsKeyUp(CKKEYBOARD key) {
    return BML::ScriptFacadeAccess::IsKeyUp(BMLAS_GetInputHook(), key);
}

bool BMLAS_IsKeyPressed(CKKEYBOARD key) {
    return BML::ScriptFacadeAccess::IsKeyPressed(BMLAS_GetInputHook(), key);
}

bool BMLAS_IsKeyReleased(CKKEYBOARD key) {
    return BML::ScriptFacadeAccess::IsKeyReleased(BMLAS_GetInputHook(), key);
}

bool BMLAS_IsKeyToggled(CKKEYBOARD key) {
    return BML::ScriptFacadeAccess::IsKeyToggled(BMLAS_GetInputHook(), key);
}

std::string BMLAS_GetKeyName(CKKEYBOARD key) {
    return BML::ScriptFacadeAccess::GetKeyName(BMLAS_GetInputHook(), key);
}

int BMLAS_GetKeyFromName(const std::string &name) {
    return BML::ScriptFacadeAccess::GetKeyFromName(BMLAS_GetInputHook(), name);
}

bool BMLAS_IsMouseButtonDown(CK_MOUSEBUTTON button) {
    return BML::ScriptFacadeAccess::IsMouseButtonDown(BMLAS_GetInputHook(), button);
}

bool BMLAS_IsMouseClicked(CK_MOUSEBUTTON button) {
    return BML::ScriptFacadeAccess::IsMouseClicked(BMLAS_GetInputHook(), button);
}

bool BMLAS_IsMouseToggled(CK_MOUSEBUTTON button) {
    return BML::ScriptFacadeAccess::IsMouseToggled(BMLAS_GetInputHook(), button);
}

Vx2DVector BMLAS_GetMousePosition(bool absolute) {
    return BML::ScriptFacadeAccess::GetMousePosition(BMLAS_GetInputHook(), absolute);
}

Vx2DVector BMLAS_GetLastMousePosition() {
    return BML::ScriptFacadeAccess::GetLastMousePosition(BMLAS_GetInputHook());
}

VxVector BMLAS_GetMouseRelativePosition() {
    return BML::ScriptFacadeAccess::GetMouseRelativePosition(BMLAS_GetInputHook());
}

bool BMLAS_IsObjectValid(CKObject *object) {
    return BML::ScriptFacadeAccess::IsObjectValid(object);
}

int BMLAS_GetObjectId(CKObject *object) {
    return BML::ScriptFacadeAccess::GetObjectId(object);
}

std::string BMLAS_GetObjectName(CKObject *object) {
    return BML::ScriptFacadeAccess::GetObjectName(object);
}

int BMLAS_GetObjectClassId(CKObject *object) {
    return BML::ScriptFacadeAccess::GetObjectClassId(object);
}

bool BMLAS_IsObjectVisible(CKObject *object) {
    return BML::ScriptFacadeAccess::IsObjectVisible(object);
}

bool BMLAS_IsObjectDynamic(CKObject *object) {
    return BML::ScriptFacadeAccess::IsObjectDynamic(object);
}

int BMLAS_GetBeObjectPriority(CKBeObject *object) {
    return BML::ScriptFacadeAccess::GetBeObjectPriority(object);
}

int BMLAS_GetBeObjectScriptCount(CKBeObject *object) {
    return BML::ScriptFacadeAccess::GetBeObjectScriptCount(object);
}

int BMLAS_GetBeObjectAttributeCount(CKBeObject *object) {
    return BML::ScriptFacadeAccess::GetBeObjectAttributeCount(object);
}

VxVector BMLAS_Get3dEntityPosition(CK3dEntity *entity) {
    return BML::ScriptFacadeAccess::Get3dEntityPosition(entity);
}

VxVector BMLAS_Get3dEntityScale(CK3dEntity *entity, bool local) {
    return BML::ScriptFacadeAccess::Get3dEntityScale(entity, local);
}

int BMLAS_Get3dEntityChildCount(CK3dEntity *entity) {
    return BML::ScriptFacadeAccess::Get3dEntityChildCount(entity);
}

CK3dEntity *BMLAS_Get3dEntityParent(CK3dEntity *entity) {
    return BML::ScriptFacadeAccess::Get3dEntityParent(entity);
}

CKDataArray *BMLAS_GetArrayByName(const std::string &name) {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetArrayByName(name.c_str()) : nullptr;
}

CKGroup *BMLAS_GetGroupByName(const std::string &name) {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetGroupByName(name.c_str()) : nullptr;
}

CKMaterial *BMLAS_GetMaterialByName(const std::string &name) {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetMaterialByName(name.c_str()) : nullptr;
}

CKMesh *BMLAS_GetMeshByName(const std::string &name) {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetMeshByName(name.c_str()) : nullptr;
}

CK2dEntity *BMLAS_Get2dEntityByName(const std::string &name) {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->Get2dEntityByName(name.c_str()) : nullptr;
}

CK3dEntity *BMLAS_Get3dEntityByName(const std::string &name) {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->Get3dEntityByName(name.c_str()) : nullptr;
}

CK3dObject *BMLAS_Get3dObjectByName(const std::string &name) {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->Get3dObjectByName(name.c_str()) : nullptr;
}

CKCamera *BMLAS_GetCameraByName(const std::string &name) {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetCameraByName(name.c_str()) : nullptr;
}

CKTargetCamera *BMLAS_GetTargetCameraByName(const std::string &name) {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetTargetCameraByName(name.c_str()) : nullptr;
}

CKLight *BMLAS_GetLightByName(const std::string &name) {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetLightByName(name.c_str()) : nullptr;
}

CKTargetLight *BMLAS_GetTargetLightByName(const std::string &name) {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetTargetLightByName(name.c_str()) : nullptr;
}

CKSound *BMLAS_GetSoundByName(const std::string &name) {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetSoundByName(name.c_str()) : nullptr;
}

CKTexture *BMLAS_GetTextureByName(const std::string &name) {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetTextureByName(name.c_str()) : nullptr;
}

CKBehavior *BMLAS_GetScriptByName(const std::string &name) {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetScriptByName(name.c_str()) : nullptr;
}

void BMLAS_SetIC(CKBeObject *object, bool hierarchy) {
    ModContext *ctx = nullptr;
    if (RequireContext(ctx) && object)
        ctx->SetIC(object, hierarchy);
}

void BMLAS_RestoreIC(CKBeObject *object, bool hierarchy) {
    ModContext *ctx = nullptr;
    if (RequireContext(ctx) && object)
        ctx->RestoreIC(object, hierarchy);
}

void BMLAS_Show(CKBeObject *object, CK_OBJECT_SHOWOPTION show, bool hierarchy) {
    ModContext *ctx = nullptr;
    if (RequireContext(ctx) && object)
        ctx->Show(object, show, hierarchy);
}

bool BMLAS_FileExistsUtf8(const std::string &path) { return BML_FileExistsUtf8(path.c_str()) != 0; }
bool BMLAS_DirectoryExistsUtf8(const std::string &path) { return BML_DirectoryExistsUtf8(path.c_str()) != 0; }
bool BMLAS_PathExistsUtf8(const std::string &path) { return BML_PathExistsUtf8(path.c_str()) != 0; }
bool BMLAS_IsPathValidUtf8(const std::string &path) { return BML_IsPathValidUtf8(path.c_str()) != 0; }
bool BMLAS_IsAbsolutePathUtf8(const std::string &path) { return BML_IsAbsolutePathUtf8(path.c_str()) != 0; }
bool BMLAS_IsRelativePathUtf8(const std::string &path) { return BML_IsRelativePathUtf8(path.c_str()) != 0; }
std::string BMLAS_CombinePathUtf8(const std::string &left, const std::string &right) { return CopyAndFree(BML_CombinePathUtf8(left.c_str(), right.c_str())); }
std::string BMLAS_NormalizePathUtf8(const std::string &path) { return CopyAndFree(BML_NormalizePathUtf8(path.c_str())); }
std::string BMLAS_GetFileNameUtf8(const std::string &path) { return CopyAndFree(BML_GetFileNameUtf8(path.c_str())); }
std::string BMLAS_GetExtensionUtf8(const std::string &path) { return CopyAndFree(BML_GetExtensionUtf8(path.c_str())); }
std::string BMLAS_RemoveExtensionUtf8(const std::string &path) { return CopyAndFree(BML_RemoveExtensionUtf8(path.c_str())); }
std::string BMLAS_ReadTextFileUtf8(const std::string &path) { return CopyAndFree(BML_ReadTextFileUtf8(path.c_str())); }

bool BMLAS_DataShareSetString(const std::string &key, const std::string &value, const std::string &name) {
    BML_DataShare *share = BML_GetDataShare(name.empty() ? nullptr : name.c_str());
    if (!share)
        return false;
    const int result = BML_DataShare_Set(share, key.c_str(), value.c_str(), value.size() + 1);
    BML_DataShare_Release(share);
    return result != 0;
}

template <typename T>
bool BMLAS_DataShareSetValue(const std::string &key, T value, const std::string &name) {
    BML_DataShare *share = BML_GetDataShare(name.empty() ? nullptr : name.c_str());
    if (!share)
        return false;

    const int result = BML_DataShare_Set(share, key.c_str(), &value, sizeof(value));
    BML_DataShare_Release(share);
    return result != 0;
}

template <typename T>
T BMLAS_DataShareGetValue(const std::string &key, T defaultValue, const std::string &name) {
    BML_DataShare *share = BML_GetDataShare(name.empty() ? nullptr : name.c_str());
    if (!share)
        return defaultValue;

    T value = {};
    size_t fullSize = 0;
    const int result = BML_DataShare_CopyEx(share, key.c_str(), &value, sizeof(value), &fullSize);
    BML_DataShare_Release(share);
    if (result != 1 || fullSize != sizeof(value))
        return defaultValue;

    return value;
}

bool BMLAS_DataShareSetBool(const std::string &key, bool value, const std::string &name) {
    return BMLAS_DataShareSetValue(key, value, name);
}

bool BMLAS_DataShareGetBool(const std::string &key, bool defaultValue, const std::string &name) {
    return BMLAS_DataShareGetValue(key, defaultValue, name);
}

bool BMLAS_DataShareSetInt(const std::string &key, int value, const std::string &name) {
    return BMLAS_DataShareSetValue(key, value, name);
}

int BMLAS_DataShareGetInt(const std::string &key, int defaultValue, const std::string &name) {
    return BMLAS_DataShareGetValue(key, defaultValue, name);
}

bool BMLAS_DataShareSetFloat(const std::string &key, float value, const std::string &name) {
    return BMLAS_DataShareSetValue(key, value, name);
}

float BMLAS_DataShareGetFloat(const std::string &key, float defaultValue, const std::string &name) {
    return BMLAS_DataShareGetValue(key, defaultValue, name);
}

std::string BMLAS_DataShareGetString(const std::string &key,
                                     const std::string &defaultValue,
                                     const std::string &name) {
    BML_DataShare *share = BML_GetDataShare(name.empty() ? nullptr : name.c_str());
    if (!share)
        return defaultValue;

    size_t size = 0;
    const void *data = BML_DataShare_Get(share, key.c_str(), &size);
    std::string result = defaultValue;
    if (data && size > 0) {
        result.assign(static_cast<const char *>(data), strnlen(static_cast<const char *>(data), size));
    }

    BML_DataShare_Release(share);
    return result;
}

bool BMLAS_DataShareHas(const std::string &key, const std::string &name) {
    BML_DataShare *share = BML_GetDataShare(name.empty() ? nullptr : name.c_str());
    if (!share)
        return false;
    const int result = BML_DataShare_Has(share, key.c_str());
    BML_DataShare_Release(share);
    return result != 0;
}

void BMLAS_DataShareRemove(const std::string &key, const std::string &name) {
    BML_DataShare *share = BML_GetDataShare(name.empty() ? nullptr : name.c_str());
    if (!share)
        return;
    BML_DataShare_Remove(share, key.c_str());
    BML_DataShare_Release(share);
}

int BMLAS_DataShareSizeOf(const std::string &key, const std::string &name) {
    BML_DataShare *share = BML_GetDataShare(name.empty() ? nullptr : name.c_str());
    if (!share)
        return 0;
    const int size = static_cast<int>(BML_DataShare_SizeOf(share, key.c_str()));
    BML_DataShare_Release(share);
    return size;
}

static BML::ScriptMod *BMLAS_CurrentScriptMod() {
    return BML::ScriptModRuntime::GetCurrentScriptMod();
}

static BML::ScriptTimerRef *BMLAS_AddTimer(asIScriptObject *timer) {
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    if (owner)
        return owner->AddScriptTimer(timer);
    return nullptr;
}

static BML::ScriptCommandRef *BMLAS_RegisterCommand(asIScriptObject *command) {
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    if (owner)
        return owner->RegisterScriptCommand(command);
    return nullptr;
}

static bool BMLAS_UnregisterCommand(const std::string &name) {
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    return owner && owner->UnregisterScriptCommand(name);
}

static BML::ScriptDataShareRequestRef *BMLAS_RequestDataShare(asIScriptObject *request) {
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    if (owner)
        return owner->RequestScriptDataShare(request);
    return nullptr;
}

static bool BMLAS_RegisterBallType(const std::string &ballFile,
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
                                   float radius) {
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    return owner && owner->RegisterScriptBallType(ballFile, ballId, ballName, objName, friction, elasticity, mass,
                                                 collGroup, linearDamp, rotDamp, force, radius);
}

static bool BMLAS_RegisterFloorType(const std::string &floorName,
                                    float friction,
                                    float elasticity,
                                    float mass,
                                    const std::string &collGroup,
                                    bool enableColl) {
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    return owner && owner->RegisterScriptFloorType(floorName, friction, elasticity, mass, collGroup, enableColl);
}

static bool BMLAS_RegisterModulBall(const std::string &modulName,
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
                                    float radius) {
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    return owner && owner->RegisterScriptModulBall(modulName, fixed, friction, elasticity, mass, collGroup,
                                                   frozen, enableColl, calcMassCenter, linearDamp, rotDamp, radius);
}

static bool BMLAS_RegisterModulConvex(const std::string &modulName,
                                      bool fixed,
                                      float friction,
                                      float elasticity,
                                      float mass,
                                      const std::string &collGroup,
                                      bool frozen,
                                      bool enableColl,
                                      bool calcMassCenter,
                                      float linearDamp,
                                      float rotDamp) {
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    return owner && owner->RegisterScriptModulConvex(modulName, fixed, friction, elasticity, mass, collGroup,
                                                     frozen, enableColl, calcMassCenter, linearDamp, rotDamp);
}

static bool BMLAS_RegisterTrafo(const std::string &modulName) {
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    return owner && owner->RegisterScriptTrafo(modulName);
}

static bool BMLAS_RegisterModul(const std::string &modulName) {
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    return owner && owner->RegisterScriptModul(modulName);
}

class BMLAS_ExportRef;
class BMLAS_CallFrame;

typedef int (*BMLAS_ModExportStringReader)(const BML_ModExport *, char *, size_t, size_t *);

static std::string BMLAS_ReadModExportString(const BML_ModExport *handle, BMLAS_ModExportStringReader reader) {
    if (!handle || !reader)
        return "";

    size_t required = 0;
    if (reader(handle, nullptr, 0, &required) != BML_OK || required == 0)
        return "";

    std::string value(required, '\0');
    if (reader(handle, &value[0], value.size(), &required) != BML_OK)
        return "";
    if (!value.empty() && value.back() == '\0')
        value.pop_back();
    return value;
}

class BMLAS_ModRef {
private:
    struct DependencyInfo {
        bool Valid = false;
        std::string Id;
        int Major = 0;
        int Minor = 0;
        int Patch = 0;
        bool Optional = false;
    };

    struct ExportInfo {
        bool Valid = false;
        std::string Name;
        std::string Signature;
    };

public:
    explicit BMLAS_ModRef(std::string id) : m_Id(std::move(id)) {}

    void AddRef() { ++m_RefCount; }

    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    std::string GetId() const { return m_Id; }

    std::string GetName() const {
        IMod *mod = Resolve();
        return mod ? mod->GetName() : "";
    }

    std::string GetVersion() const {
        IMod *mod = Resolve();
        return mod ? mod->GetVersion() : "";
    }

    std::string GetAuthor() const {
        IMod *mod = Resolve();
        return mod ? mod->GetAuthor() : "";
    }

    std::string GetDescription() const {
        IMod *mod = Resolve();
        return mod ? mod->GetDescription() : "";
    }

    std::string GetBMLVersion() const {
        IMod *mod = Resolve();
        return mod ? mod->GetBMLVersion().ToString() : "";
    }

    int GetBMLVersionMajor() const {
        IMod *mod = Resolve();
        return mod ? mod->GetBMLVersion().major : 0;
    }

    int GetBMLVersionMinor() const {
        IMod *mod = Resolve();
        return mod ? mod->GetBMLVersion().minor : 0;
    }

    int GetBMLVersionPatch() const {
        IMod *mod = Resolve();
        return mod ? mod->GetBMLVersion().patch : 0;
    }

    int GetKind() const { return BML_GetModKind(m_Id.c_str()); }

    int GetState() const { return BML_GetModState(m_Id.c_str()); }

    bool IsValid() const { return Resolve() != nullptr; }

    bool IsScript() const {
        return GetKind() == BML_MOD_KIND_SCRIPT;
    }

    bool IsFailed() const {
        return GetState() == BML_MOD_STATE_FAILED;
    }

    int CheckDependencies() const {
        ModContext *ctx = nullptr;
        IMod *mod = Resolve(&ctx);
        return ctx && mod ? ctx->CheckDependencies(mod) : 0;
    }

    int GetDependencyCount() const {
        ModContext *ctx = nullptr;
        IMod *mod = Resolve(&ctx);
        return ctx && mod ? ctx->GetDependencyCount(mod) : -1;
    }

    std::string GetDependencyId(int index) const {
        DependencyInfo info = GetDependencyInfo(index);
        return info.Valid ? info.Id : "";
    }

    std::string GetDependencyVersion(int index) const {
        DependencyInfo info = GetDependencyInfo(index);
        if (!info.Valid)
            return "";
        return std::to_string(info.Major) + "." +
               std::to_string(info.Minor) + "." +
               std::to_string(info.Patch);
    }

    int GetDependencyVersionMajor(int index) const {
        DependencyInfo info = GetDependencyInfo(index);
        return info.Valid ? info.Major : 0;
    }

    int GetDependencyVersionMinor(int index) const {
        DependencyInfo info = GetDependencyInfo(index);
        return info.Valid ? info.Minor : 0;
    }

    int GetDependencyVersionPatch(int index) const {
        DependencyInfo info = GetDependencyInfo(index);
        return info.Valid ? info.Patch : 0;
    }

    bool IsDependencyOptional(int index) const {
        DependencyInfo info = GetDependencyInfo(index);
        return info.Valid && info.Optional;
    }

    int GetExportCount() const {
        const int count = BML_GetModExportCount(m_Id.c_str());
        return count > 0 ? count : 0;
    }

    std::string GetExportName(int index) const {
        ExportInfo info = GetExportInfo(index);
        return info.Valid ? info.Name : "";
    }

    std::string GetExportSignature(int index) const {
        ExportInfo info = GetExportInfo(index);
        return info.Valid ? info.Signature : "";
    }

    BMLAS_ExportRef *GetExport(int index) const;

    std::string GetDiagnostic() const {
        size_t required = 0;
        if (BML_GetModDiagnostic(m_Id.c_str(), nullptr, 0, &required) != BML_OK || required == 0)
            return "";
        std::string diagnostic(required, '\0');
        if (BML_GetModDiagnostic(m_Id.c_str(), &diagnostic[0], diagnostic.size(), &required) != BML_OK)
            return "";
        if (!diagnostic.empty() && diagnostic.back() == '\0')
            diagnostic.pop_back();
        return diagnostic;
    }

    BMLAS_ExportRef *FindExport(const std::string &name, const std::string &signature) const;
    int TryFindExport(const std::string &name, BMLAS_ExportRef *&outExport, const std::string &signature) const;

private:
    IMod *Resolve() const {
        return Resolve(nullptr);
    }

    IMod *Resolve(ModContext **outContext) const {
        ModContext *ctx = nullptr;
        if (!RequireContext(ctx))
            return nullptr;
        if (outContext)
            *outContext = ctx;
        return ctx->FindMod(m_Id.c_str());
    }

    DependencyInfo GetDependencyInfo(int index) const {
        DependencyInfo info;
        ModContext *ctx = nullptr;
        IMod *mod = Resolve(&ctx);
        if (!ctx || !mod)
            return info;

        char dependencyId[256] = {};
        int major = 0;
        int minor = 0;
        int patch = 0;
        int optional = 0;
        const int status = ctx->GetDependencyInfo(mod,
                                                  index,
                                                  dependencyId,
                                                  static_cast<int>(sizeof(dependencyId)),
                                                  &major,
                                                  &minor,
                                                  &patch,
                                                  &optional);
        if (status != BML_OK)
            return info;

        info.Valid = true;
        info.Id = dependencyId;
        info.Major = major;
        info.Minor = minor;
        info.Patch = patch;
        info.Optional = optional != 0;
        return info;
    }

    ExportInfo GetExportInfo(int index) const {
        ExportInfo info;
        if (index < 0)
            return info;

        size_t nameRequired = 0;
        size_t signatureRequired = 0;
        int status = BML_GetModExportInfo(m_Id.c_str(),
                                          static_cast<size_t>(index),
                                          nullptr,
                                          0,
                                          &nameRequired,
                                          nullptr,
                                          0,
                                          &signatureRequired);
        if (status != BML_OK || nameRequired == 0 || signatureRequired == 0)
            return info;

        std::string name(nameRequired, '\0');
        std::string signature(signatureRequired, '\0');
        status = BML_GetModExportInfo(m_Id.c_str(),
                                      static_cast<size_t>(index),
                                      &name[0],
                                      name.size(),
                                      &nameRequired,
                                      &signature[0],
                                      signature.size(),
                                      &signatureRequired);
        if (status != BML_OK)
            return info;

        if (!name.empty() && name.back() == '\0')
            name.pop_back();
        if (!signature.empty() && signature.back() == '\0')
            signature.pop_back();

        info.Valid = true;
        info.Name = std::move(name);
        info.Signature = std::move(signature);
        return info;
    }

    int m_RefCount = 1;
    std::string m_Id;
};

class BMLAS_CallFrame {
public:
    BMLAS_CallFrame() : m_Frame(BML_CreateCallFrame()) {}

    ~BMLAS_CallFrame() {
        if (m_Frame)
            BML_DestroyCallFrame(m_Frame);
    }

    void AddRef() { ++m_RefCount; }

    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    bool IsValid() const { return m_Frame != nullptr; }

    void Clear() {
        if (m_Frame)
            BML_CallFrame_Clear(m_Frame);
    }

    int GetArgCount() const {
        return m_Frame ? static_cast<int>(BML_CallFrame_GetArgCount(m_Frame)) : 0;
    }

    int GetArgType(unsigned int index) const {
        return m_Frame ? BML_CallFrame_GetArgType(m_Frame, index) : BML_CALL_VALUE_EMPTY;
    }

    int ClearArg(unsigned int index) {
        return m_Frame ? BML_CallFrame_ClearArg(m_Frame, index) : BML_ERROR_INTEROP_BAD_CALL_FRAME;
    }

    int SetBool(unsigned int index, bool value) {
        return m_Frame ? BML_CallFrame_SetBool(m_Frame, index, value ? 1 : 0) : BML_ERROR_INVALID_PARAMETER;
    }

    int GetBool(unsigned int index, bool &value) const {
        if (!m_Frame)
            return BML_ERROR_INTEROP_BAD_CALL_FRAME;
        int rawValue = 0;
        const int status = BML_CallFrame_GetBool(m_Frame, index, &rawValue);
        if (status == BML_OK)
            value = rawValue != 0;
        return status;
    }

    int SetInt(unsigned int index, int value) {
        return m_Frame ? BML_CallFrame_SetInt(m_Frame, index, value) : BML_ERROR_INVALID_PARAMETER;
    }

    int GetInt(unsigned int index, int &value) const {
        return m_Frame ? BML_CallFrame_GetInt(m_Frame, index, &value) : BML_ERROR_INTEROP_BAD_CALL_FRAME;
    }

    int SetFloat(unsigned int index, float value) {
        return m_Frame ? BML_CallFrame_SetFloat(m_Frame, index, value) : BML_ERROR_INVALID_PARAMETER;
    }

    int GetFloat(unsigned int index, float &value) const {
        return m_Frame ? BML_CallFrame_GetFloat(m_Frame, index, &value) : BML_ERROR_INTEROP_BAD_CALL_FRAME;
    }

    int SetString(unsigned int index, const std::string &value) {
        return m_Frame ? BML_CallFrame_SetString(m_Frame, index, value.c_str()) : BML_ERROR_INVALID_PARAMETER;
    }

    int GetString(unsigned int index, std::string &value) const {
        if (!m_Frame)
            return BML_ERROR_INTEROP_BAD_CALL_FRAME;

        size_t required = 0;
        int status = BML_CallFrame_GetString(m_Frame, index, nullptr, 0, &required);
        if (status != BML_OK)
            return status;
        value.assign(required, '\0');
        status = BML_CallFrame_GetString(m_Frame, index, &value[0], value.size(), &required);
        if (status == BML_OK && !value.empty() && value.back() == '\0')
            value.pop_back();
        return status;
    }

    int SetResultBool(bool value) {
        return m_Frame ? BML_CallFrame_SetResultBool(m_Frame, value ? 1 : 0) : BML_ERROR_INVALID_PARAMETER;
    }

    int GetResultType() const {
        return m_Frame ? BML_CallFrame_GetResultType(m_Frame) : BML_CALL_VALUE_EMPTY;
    }

    int ClearResult() {
        return m_Frame ? BML_CallFrame_ClearResult(m_Frame) : BML_ERROR_INTEROP_BAD_CALL_FRAME;
    }

    int GetResultBool(bool &value) const {
        if (!m_Frame)
            return BML_ERROR_INTEROP_BAD_CALL_FRAME;
        int rawValue = 0;
        const int status = BML_CallFrame_GetResultBool(m_Frame, &rawValue);
        if (status == BML_OK)
            value = rawValue != 0;
        return status;
    }

    int SetResultInt(int value) {
        return m_Frame ? BML_CallFrame_SetResultInt(m_Frame, value) : BML_ERROR_INVALID_PARAMETER;
    }

    int GetResultInt(int &value) const {
        return m_Frame ? BML_CallFrame_GetResultInt(m_Frame, &value) : BML_ERROR_INTEROP_BAD_CALL_FRAME;
    }

    int SetResultFloat(float value) {
        return m_Frame ? BML_CallFrame_SetResultFloat(m_Frame, value) : BML_ERROR_INVALID_PARAMETER;
    }

    int GetResultFloat(float &value) const {
        return m_Frame ? BML_CallFrame_GetResultFloat(m_Frame, &value) : BML_ERROR_INTEROP_BAD_CALL_FRAME;
    }

    int SetResultString(const std::string &value) {
        return m_Frame ? BML_CallFrame_SetResultString(m_Frame, value.c_str()) : BML_ERROR_INVALID_PARAMETER;
    }

    int GetResultString(std::string &value) const {
        if (!m_Frame)
            return BML_ERROR_INTEROP_BAD_CALL_FRAME;

        size_t required = 0;
        int status = BML_CallFrame_GetResultString(m_Frame, nullptr, 0, &required);
        if (status != BML_OK)
            return status;
        value.assign(required, '\0');
        status = BML_CallFrame_GetResultString(m_Frame, &value[0], value.size(), &required);
        if (status == BML_OK && !value.empty() && value.back() == '\0')
            value.pop_back();
        return status;
    }

    BML_CallFrame *GetNativeFrame() const { return m_Frame; }

private:
    int m_RefCount = 1;
    BML_CallFrame *m_Frame = nullptr;
};

BMLAS_CallFrame *BMLAS_CreateCallFrame() {
    return new BMLAS_CallFrame();
}

class BMLAS_ExportRef {
public:
    BMLAS_ExportRef(std::string modId, std::string name, std::string signature, BML_ModExport *handle)
        : m_ModId(std::move(modId)),
          m_Name(std::move(name)),
          m_Signature(std::move(signature)),
          m_Handle(handle) {}

    ~BMLAS_ExportRef() {
        if (m_Handle)
            BML_ReleaseModExport(m_Handle);
    }

    void AddRef() { ++m_RefCount; }

    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    std::string GetModId() const { return m_ModId; }
    std::string GetName() const { return m_Name; }
    std::string GetSignature() const { return m_Signature; }

    bool IsValid() const { return m_Handle && BML_IsModExportValid(m_Handle) != 0; }

    int Call(BMLAS_CallFrame *frame) const {
        if (!frame || !frame->GetNativeFrame())
            return BML_ERROR_INTEROP_BAD_CALL_FRAME;
        return BML_CallModExport(m_Handle, frame->GetNativeFrame());
    }

    int CallVoid() const {
        BML_CallFrame frame;
        return BML_CallModExport(m_Handle, &frame);
    }

    int CallString(const std::string &argument, std::string &result) const {
        BML_CallFrame frame;
        int status = BML_CallFrame_SetString(&frame, 0, argument.c_str());
        if (status == BML_OK)
            status = BML_CallModExport(m_Handle, &frame);
        if (status == BML_OK)
            status = ReadFrameResultString(&frame, result);
        return status;
    }

    int CallStringNoArgs(std::string &result) const {
        BML_CallFrame frame;
        int status = BML_CallModExport(m_Handle, &frame);
        if (status == BML_OK)
            status = ReadFrameResultString(&frame, result);
        return status;
    }

    int CallBool(bool argument, bool &result) const {
        BML_CallFrame frame;
        int status = BML_CallFrame_SetBool(&frame, 0, argument ? 1 : 0);
        if (status == BML_OK)
            status = BML_CallModExport(m_Handle, &frame);
        if (status == BML_OK)
            status = ReadFrameResultBool(&frame, result);
        return status;
    }

    int CallBoolNoArgs(bool &result) const {
        BML_CallFrame frame;
        int status = BML_CallModExport(m_Handle, &frame);
        if (status == BML_OK)
            status = ReadFrameResultBool(&frame, result);
        return status;
    }

    int CallInt(int argument, int &result) const {
        BML_CallFrame frame;
        int status = BML_CallFrame_SetInt(&frame, 0, argument);
        if (status == BML_OK)
            status = BML_CallModExport(m_Handle, &frame);
        if (status == BML_OK)
            status = BML_CallFrame_GetResultInt(&frame, &result);
        return status;
    }

    int CallIntNoArgs(int &result) const {
        BML_CallFrame frame;
        int status = BML_CallModExport(m_Handle, &frame);
        if (status == BML_OK)
            status = BML_CallFrame_GetResultInt(&frame, &result);
        return status;
    }

    int CallFloat(float argument, float &result) const {
        BML_CallFrame frame;
        int status = BML_CallFrame_SetFloat(&frame, 0, argument);
        if (status == BML_OK)
            status = BML_CallModExport(m_Handle, &frame);
        if (status == BML_OK)
            status = BML_CallFrame_GetResultFloat(&frame, &result);
        return status;
    }

    int CallFloatNoArgs(float &result) const {
        BML_CallFrame frame;
        int status = BML_CallModExport(m_Handle, &frame);
        if (status == BML_OK)
            status = BML_CallFrame_GetResultFloat(&frame, &result);
        return status;
    }

private:
    static int ReadFrameResultBool(BML_CallFrame *frame, bool &result) {
        int value = 0;
        const int status = BML_CallFrame_GetResultBool(frame, &value);
        if (status != BML_OK)
            return status;
        result = value != 0;
        return BML_OK;
    }

    static int ReadFrameResultString(BML_CallFrame *frame, std::string &result) {
        size_t required = 0;
        int status = BML_CallFrame_GetResultString(frame, nullptr, 0, &required);
        if (status != BML_OK)
            return status;

        std::string value(required, '\0');
        status = BML_CallFrame_GetResultString(frame, &value[0], value.size(), &required);
        if (status != BML_OK)
            return status;
        if (!value.empty() && value.back() == '\0')
            value.pop_back();
        result = std::move(value);
        return BML_OK;
    }

    int m_RefCount = 1;
    std::string m_ModId;
    std::string m_Name;
    std::string m_Signature;
    BML_ModExport *m_Handle = nullptr;
};

BMLAS_ExportRef *BMLAS_ModRef::FindExport(const std::string &name, const std::string &signature) const {
    BMLAS_ExportRef *exportRef = nullptr;
    return TryFindExport(name, exportRef, signature) == BML_OK ? exportRef : nullptr;
}

int BMLAS_ModRef::TryFindExport(const std::string &name,
                                BMLAS_ExportRef *&outExport,
                                const std::string &signature) const {
    outExport = nullptr;

    BML_ModExport *handle = nullptr;
    const int status = BML_FindModExportEx(m_Id.c_str(),
                                           name.c_str(),
                                           signature.empty() ? nullptr : signature.c_str(),
                                           &handle);
    if (status != BML_OK)
        return status;

    std::string resolvedSignature = BMLAS_ReadModExportString(handle, BML_GetModExportSignature);
    if (resolvedSignature.empty())
        resolvedSignature = signature;

    outExport = new (std::nothrow) BMLAS_ExportRef(m_Id, name, resolvedSignature, handle);
    if (!outExport) {
        BML_ReleaseModExport(handle);
        return BML_ERROR_OUT_OF_MEMORY;
    }
    return BML_OK;
}

BMLAS_ExportRef *BMLAS_ModRef::GetExport(int index) const {
    ExportInfo info = GetExportInfo(index);
    if (!info.Valid)
        return nullptr;

    BML_ModExport *handle = BML_FindModExport(m_Id.c_str(), info.Name.c_str(), info.Signature.c_str());
    return handle ? new BMLAS_ExportRef(m_Id, info.Name, info.Signature, handle) : nullptr;
}

BMLAS_ModRef *BMLAS_FindMod(const std::string &id) {
    ModContext *ctx = nullptr;
    if (!RequireContext(ctx))
        return nullptr;
    IMod *mod = ctx->FindMod(id.c_str());
    return mod ? new BMLAS_ModRef(mod->GetID()) : nullptr;
}

int BMLAS_GetModCount() {
    ModContext *ctx = nullptr;
    return RequireContext(ctx) ? ctx->GetModCount() : 0;
}

std::string BMLAS_GetModId(int index) {
    ModContext *ctx = nullptr;
    if (!RequireContext(ctx))
        return "";
    IMod *mod = ctx->GetMod(index);
    return mod ? mod->GetID() : "";
}

BMLAS_ModRef *BMLAS_GetMod(int index) {
    ModContext *ctx = nullptr;
    if (!RequireContext(ctx))
        return nullptr;
    IMod *mod = ctx->GetMod(index);
    return mod ? new BMLAS_ModRef(mod->GetID()) : nullptr;
}

BMLAS_ModRef *BMLAS_ContextFindMod(BML::ScriptModContextView *view, const std::string &id) {
    return view && view->HasContext() ? BMLAS_FindMod(id) : nullptr;
}

BMLAS_ModRef *BMLAS_ContextGetMod(BML::ScriptModContextView *view, int index) {
    return view && view->HasContext() ? BMLAS_GetMod(index) : nullptr;
}

bool BMLAS_UI_BeginRenderCall() {
    if (BML::ScriptModRuntime::IsInRenderCallback() && Bui::GetImGuiContext())
        return true;

    static bool logged = false;
    if (!logged) {
        logged = true;
        ModContext *ctx = GetActiveContext();
        if (ctx && ctx->GetLogger()) {
            ctx->GetLogger()->Warn("BML::UI drawing calls are only active during script OnRender callbacks.");
        }
    }
    return false;
}

Bui::ButtonType BMLAS_UI_ToButtonType(int type) {
    return type >= 0 && type < Bui::BUTTON_COUNT
               ? static_cast<Bui::ButtonType>(type)
               : Bui::BUTTON_COUNT;
}

void BMLAS_UI_SetCursorCoord(float x, float y) {
    if (!BMLAS_UI_BeginRenderCall())
        return;
    Bui::ImGuiContextScope scope;
    ImGui::SetCursorScreenPos(Bui::CoordToPixel(ImVec2(x, y)));
}

float BMLAS_UI_CoordToPixelX(float x) {
    if (!BMLAS_UI_BeginRenderCall())
        return 0.0f;
    Bui::ImGuiContextScope scope;
    return Bui::CoordToPixel(ImVec2(x, 0.0f)).x;
}

float BMLAS_UI_CoordToPixelY(float y) {
    if (!BMLAS_UI_BeginRenderCall())
        return 0.0f;
    Bui::ImGuiContextScope scope;
    return Bui::CoordToPixel(ImVec2(0.0f, y)).y;
}

float BMLAS_UI_GetMenuPosX() {
    if (!BMLAS_UI_BeginRenderCall())
        return 0.0f;
    Bui::ImGuiContextScope scope;
    return Bui::GetMenuPos().x;
}

float BMLAS_UI_GetMenuPosY() {
    if (!BMLAS_UI_BeginRenderCall())
        return 0.0f;
    Bui::ImGuiContextScope scope;
    return Bui::GetMenuPos().y;
}

float BMLAS_UI_GetMenuSizeX() {
    if (!BMLAS_UI_BeginRenderCall())
        return 0.0f;
    Bui::ImGuiContextScope scope;
    return Bui::GetMenuSize().x;
}

float BMLAS_UI_GetMenuSizeY() {
    if (!BMLAS_UI_BeginRenderCall())
        return 0.0f;
    Bui::ImGuiContextScope scope;
    return Bui::GetMenuSize().y;
}

int BMLAS_UI_CalcPageCount(int totalCount, int pageSize) {
    return Bui::CalcPageCount(totalCount, pageSize);
}

bool BMLAS_UI_CanPrevPage(int pageIndex) {
    return Bui::CanPrevPage(pageIndex);
}

bool BMLAS_UI_CanNextPage(int pageIndex, int totalCount, int pageSize) {
    return Bui::CanNextPage(pageIndex, totalCount, pageSize);
}

bool BMLAS_UI_MainButton(const std::string &label) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    return Bui::MainButton(label.c_str());
}

bool BMLAS_UI_BackButton(const std::string &label) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    return Bui::BackButton(label.c_str());
}

bool BMLAS_UI_OptionButton(const std::string &label) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    return Bui::OptionButton(label.c_str());
}

bool BMLAS_UI_LevelButton(const std::string &label) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    return Bui::LevelButton(label.c_str());
}

bool BMLAS_UI_LevelButtonSelected(const std::string &label, bool &selected) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    return Bui::LevelButton(label.c_str(), &selected);
}

bool BMLAS_UI_SmallButton(const std::string &label) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    return Bui::SmallButton(label.c_str());
}

bool BMLAS_UI_SmallButtonSelected(const std::string &label, bool &selected) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    return Bui::SmallButton(label.c_str(), &selected);
}

bool BMLAS_UI_LeftButton(const std::string &label) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    return Bui::LeftButton(label.c_str());
}

bool BMLAS_UI_RightButton(const std::string &label) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    return Bui::RightButton(label.c_str());
}

bool BMLAS_UI_PlusButton(const std::string &label) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    return Bui::PlusButton(label.c_str());
}

bool BMLAS_UI_MinusButton(const std::string &label) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    return Bui::MinusButton(label.c_str());
}

void BMLAS_UI_Title(const std::string &text, float y, float scale) {
    if (!BMLAS_UI_BeginRenderCall())
        return;
    Bui::ImGuiContextScope scope;
    Bui::Title(text.c_str(), y, scale);
}

void BMLAS_UI_WrappedText(const std::string &text, float width, float baseX, float scale) {
    if (!BMLAS_UI_BeginRenderCall())
        return;
    Bui::ImGuiContextScope scope;
    Bui::WrappedText(text.c_str(), width, baseX, scale);
}

bool BMLAS_UI_NavLeft(float x, float y) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    return Bui::NavLeft(x, y);
}

bool BMLAS_UI_NavRight(float x, float y) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    return Bui::NavRight(x, y);
}

bool BMLAS_UI_NavBack(float x, float y) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    return Bui::NavBack(x, y);
}

bool BMLAS_UI_YesNoButton(const std::string &label, bool &value) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    return Bui::YesNoButton(label.c_str(), &value);
}

bool BMLAS_UI_InputIntButton(const std::string &label, int &value, int step, int stepFast) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    return Bui::InputIntButton(label.c_str(), &value, step, stepFast);
}

bool BMLAS_UI_InputFloatButton(const std::string &label, float &value, float step, float stepFast) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    return Bui::InputFloatButton(label.c_str(), &value, step, stepFast);
}

bool BMLAS_UI_SearchBar(std::string &text, float x, float y, float width) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    char buffer[512] = {};
    std::strncpy(buffer, text.c_str(), sizeof(buffer) - 1);
    const bool changed = Bui::SearchBar(buffer, sizeof(buffer), x, y, width);
    if (changed)
        text = buffer;
    return changed;
}

void BMLAS_UI_PlayMenuClickSound() {
    Bui::PlayMenuClickSound();
}

float BMLAS_UI_GetButtonSizeX(int type) {
    if (!BMLAS_UI_BeginRenderCall())
        return 0.0f;
    Bui::ImGuiContextScope scope;
    return Bui::GetButtonSize(BMLAS_UI_ToButtonType(type)).x;
}

float BMLAS_UI_GetButtonSizeY(int type) {
    if (!BMLAS_UI_BeginRenderCall())
        return 0.0f;
    Bui::ImGuiContextScope scope;
    return Bui::GetButtonSize(BMLAS_UI_ToButtonType(type)).y;
}

float BMLAS_UI_GetButtonIndent(int type) {
    if (!BMLAS_UI_BeginRenderCall())
        return 0.0f;
    Bui::ImGuiContextScope scope;
    return Bui::GetButtonIndent(BMLAS_UI_ToButtonType(type));
}

float BMLAS_UI_GetButtonSizeCoordX(int type) {
    return Bui::GetButtonSizeInCoord(BMLAS_UI_ToButtonType(type)).x;
}

float BMLAS_UI_GetButtonSizeCoordY(int type) {
    return Bui::GetButtonSizeInCoord(BMLAS_UI_ToButtonType(type)).y;
}

float BMLAS_UI_GetButtonIndentCoord(int type) {
    return Bui::GetButtonIndentInCoord(BMLAS_UI_ToButtonType(type));
}

BML::ScriptModContextView *BMLAS_CreateInvalidModContext() {
    static BML::ScriptModContextView invalidContext;
    return &invalidContext;
}

void BMLAS_ReleaseModContext(BML::ScriptModContextView *) {}

BML::ScriptCommandCompletion *BMLAS_CreateInvalidCommandCompletion() {
    static BML::ScriptCommandCompletion invalidList;
    return &invalidList;
}

void BMLAS_ReleaseCommandCompletion(BML::ScriptCommandCompletion *) {}

bool ShouldLogAngelScriptUnavailable(CKAngelScriptAdapter::State state, const std::string &diagnostic) {
    if (state == CKAngelScriptAdapter::State::MissingModule)
        return false;
    if (state == g_LastUnavailableState && diagnostic == g_LastUnavailableDiagnostic)
        return false;

    g_LastUnavailableState = state;
    g_LastUnavailableDiagnostic = diagnostic;
    return true;
}

void ResetAngelScriptUnavailableLog() {
    g_LastUnavailableState = CKAngelScriptAdapter::State::Unchecked;
    g_LastUnavailableDiagnostic.clear();
}

bool IsForbiddenRegistrationDeclaration(const char *declaration, const char **matchedToken) {
    const char *forbiddenTokens[] = {
        "CKBehaviorContext",
        "CKBehaviorPrototype",
        "CKBehaviorIO",
        "CKBehaviorLink",
        "CKBehaviorPin",
        "CKObjectDeclaration",
        "CKParameter@",
        "CKParameterIn",
        "CKParameterOut",
        "CKBehaviorSlot",
        "BehaviorSlot",
        "InputParameter",
        "OutputParameter",
        "RegisterBehavior",
        "RegisterBB",
        "BB_",
        "generated helper",
        "GeneratedHelper",
    };

    if (!declaration)
        return false;

    for (const char *token : forbiddenTokens) {
        if (std::strstr(declaration, token)) {
            if (matchedToken)
                *matchedToken = token;
            return true;
        }
    }
    return false;
}

bool CheckRegistrationSurfaceText(const char *category, const char *owner, const char *text) {
    const char *matchedToken = nullptr;
    if (!IsForbiddenRegistrationDeclaration(text, &matchedToken))
        return true;

    g_LastRegistrationError = "BML AngelScript ";
    g_LastRegistrationError += category ? category : "registration text";
    if (owner && owner[0]) {
        g_LastRegistrationError += " for ";
        g_LastRegistrationError += owner;
    }
    g_LastRegistrationError += " exposes forbidden symbol '";
    g_LastRegistrationError += matchedToken ? matchedToken : "<unknown>";
    g_LastRegistrationError += "': ";
    g_LastRegistrationError += text ? text : "<unknown>";
    return false;
}

bool CheckRegistrationDeclaration(const char *declaration) {
    return CheckRegistrationSurfaceText("declaration", nullptr, declaration);
}

bool CheckRegistration(int code, const char *declaration) {
    if (code >= 0)
        return true;

    g_LastRegistrationError = "Failed to register BML AngelScript declaration: ";
    g_LastRegistrationError += declaration ? declaration : "<unknown>";
    g_LastRegistrationError += " returned ";
    g_LastRegistrationError += std::to_string(code);
    return false;
}

bool RequireType(asIScriptEngine *engine, const char *typeName, const char **errorMessage) {
    if (engine->GetTypeInfoByName(typeName))
        return true;

    g_LastRegistrationError = "BML AngelScript registration requires type ";
    g_LastRegistrationError += typeName;
    if (errorMessage)
        *errorMessage = g_LastRegistrationError.c_str();
    return false;
}

#define BML_AS_REGISTER(expr, declaration)                        \
    do {                                                          \
        if (!CheckRegistrationDeclaration(declaration)) {          \
            engine->SetDefaultNamespace("");                      \
            if (errorMessage) *errorMessage = g_LastRegistrationError.c_str(); \
            return asERROR;                                       \
        }                                                         \
        const int bmlAsResult = (expr);                           \
        if (!CheckRegistration(bmlAsResult, declaration)) {       \
            engine->SetDefaultNamespace("");                      \
            if (errorMessage) *errorMessage = g_LastRegistrationError.c_str(); \
            return bmlAsResult;                                   \
        }                                                         \
    } while (false)

struct ScriptObjectTypeRegistration {
    const char *Name;
    const char *Declaration;
    int Size;
    asDWORD Flags;
};

struct ScriptObjectBehaviourRegistration {
    const char *TypeName;
    asEBehaviours Behaviour;
    const char *Declaration;
    const char *DiagnosticName;
    asSFuncPtr Function;
    asDWORD CallConvention;
};

struct ScriptObjectMethodRegistration {
    const char *TypeName;
    const char *Declaration;
    const char *DiagnosticName;
    asSFuncPtr Function;
    asDWORD CallConvention;
};

struct ScriptInterfaceMethodRegistration {
    const char *InterfaceName;
    const char *Declaration;
    const char *DiagnosticName;
};

struct ScriptInterfaceRegistration {
    const char *Name;
    const char *DiagnosticName;
};

struct ScriptGlobalFunctionRegistration {
    const char *Declaration;
    const char *DiagnosticName;
    asSFuncPtr Function;
    asDWORD CallConvention;
};

struct ScriptUiEnumValueRegistration {
    const char *Name;
    int Value;
    const char *DiagnosticName;
};

struct ScriptUiFunctionRegistration {
    const char *Declaration;
    const char *DiagnosticName;
    asSFuncPtr Function;
    asDWORD CallConvention;
};

static const ScriptObjectTypeRegistration kObjectTypeRegistrations[] = {
    {"ModContext", "class ModContext", 0, asOBJ_REF | asOBJ_SCOPED},
    {"InputHook", "class InputHook", 0, asOBJ_REF | asOBJ_NOCOUNT},
    {"TimerRef", "class TimerRef", 0, asOBJ_REF},
    {"TimerEvent", "class TimerEvent", sizeof(BML::ScriptTimerEventView), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS},
    {"RenderEvent", "class RenderEvent", sizeof(BML::ScriptRenderEventView), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS},
    {"CheatEvent", "class CheatEvent", sizeof(BML::ScriptCheatEventView), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS},
    {"LoadObjectEvent", "class LoadObjectEvent", sizeof(BML::ScriptLoadObjectEventView), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS},
    {"LoadScriptEvent", "class LoadScriptEvent", sizeof(BML::ScriptLoadScriptEventView), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS},
    {"CommandEvent", "class CommandEvent", sizeof(BML::ScriptCommandEventView), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS},
    {"CommandCompletion", "class CommandCompletion", 0, asOBJ_REF | asOBJ_SCOPED},
    {"CommandRef", "class CommandRef", 0, asOBJ_REF},
    {"DataShareEvent", "class DataShareEvent", sizeof(BML::ScriptDataShareEventView), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS},
    {"DataShareRequestRef", "class DataShareRequestRef", 0, asOBJ_REF},
    {"PhysicalizeEvent", "class PhysicalizeEvent", sizeof(BML::ScriptPhysicalizeEventView), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS},
    {"ObjectEvent", "class ObjectEvent", sizeof(BML::ScriptObjectEventView), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS},
    {"ModRef", "class ModRef", 0, asOBJ_REF},
    {"ExportRef", "class ExportRef", 0, asOBJ_REF},
    {"CallFrame", "class CallFrame", 0, asOBJ_REF},
};

static const ScriptInterfaceRegistration kInterfaceRegistrations[] = {
    {"Command", "interface Command"},
    {"Timer", "interface Timer"},
    {"DataShareRequest", "interface DataShareRequest"},
};

static const ScriptInterfaceMethodRegistration kInterfaceMethodRegistrations[] = {
    {"Command", "string get_Name() const", "string Command::get_Name() const"},
    {"Command", "void Execute(const BML::ModContext &in, const BML::CommandEvent &in)", "void Command::Execute(...)"},
    {"Timer", "bool Tick(const BML::ModContext &in, const BML::TimerEvent &in)", "bool Timer::Tick(...)"},
    {"DataShareRequest", "string get_Key() const", "string DataShareRequest::get_Key() const"},
    {"DataShareRequest", "int get_Type() const", "int DataShareRequest::get_Type() const"},
    {"DataShareRequest", "void Receive(const BML::ModContext &in, const BML::DataShareEvent &in)", "void DataShareRequest::Receive(...)"},
};

static const ScriptObjectBehaviourRegistration kObjectBehaviourRegistrations[] = {
    {"ModContext", asBEHAVE_FACTORY, "ModContext@ f()", "ModContext@ ModContext factory", asFUNCTION(BMLAS_CreateInvalidModContext), asCALL_CDECL},
    {"ModContext", asBEHAVE_RELEASE, "void f()", "void ModContext release", asFUNCTION(BMLAS_ReleaseModContext), asCALL_CDECL_OBJLAST},
    {"CommandCompletion", asBEHAVE_FACTORY, "CommandCompletion@ f()", "CommandCompletion@ factory", asFUNCTION(BMLAS_CreateInvalidCommandCompletion), asCALL_CDECL},
    {"CommandCompletion", asBEHAVE_RELEASE, "void f()", "void CommandCompletion release", asFUNCTION(BMLAS_ReleaseCommandCompletion), asCALL_CDECL_OBJLAST},
    {"CommandRef", asBEHAVE_ADDREF, "void f()", "void CommandRef addref", asMETHOD(BML::ScriptCommandRef, AddRef), asCALL_THISCALL},
    {"CommandRef", asBEHAVE_RELEASE, "void f()", "void CommandRef release", asMETHOD(BML::ScriptCommandRef, Release), asCALL_THISCALL},
    {"DataShareRequestRef", asBEHAVE_ADDREF, "void f()", "void DataShareRequestRef addref", asMETHOD(BML::ScriptDataShareRequestRef, AddRef), asCALL_THISCALL},
    {"DataShareRequestRef", asBEHAVE_RELEASE, "void f()", "void DataShareRequestRef release", asMETHOD(BML::ScriptDataShareRequestRef, Release), asCALL_THISCALL},
    {"TimerRef", asBEHAVE_ADDREF, "void f()", "void TimerRef addref", asMETHOD(BML::ScriptTimerRef, AddRef), asCALL_THISCALL},
    {"TimerRef", asBEHAVE_RELEASE, "void f()", "void TimerRef release", asMETHOD(BML::ScriptTimerRef, Release), asCALL_THISCALL},
    {"CallFrame", asBEHAVE_FACTORY, "CallFrame@ f()", "CallFrame@ CallFrame factory", asFUNCTION(BMLAS_CreateCallFrame), asCALL_CDECL},
    {"CallFrame", asBEHAVE_ADDREF, "void f()", "void CallFrame addref", asMETHOD(BMLAS_CallFrame, AddRef), asCALL_THISCALL},
    {"CallFrame", asBEHAVE_RELEASE, "void f()", "void CallFrame release", asMETHOD(BMLAS_CallFrame, Release), asCALL_THISCALL},
    {"ModRef", asBEHAVE_ADDREF, "void f()", "void ModRef addref", asMETHOD(BMLAS_ModRef, AddRef), asCALL_THISCALL},
    {"ModRef", asBEHAVE_RELEASE, "void f()", "void ModRef release", asMETHOD(BMLAS_ModRef, Release), asCALL_THISCALL},
    {"ExportRef", asBEHAVE_ADDREF, "void f()", "void ExportRef addref", asMETHOD(BMLAS_ExportRef, AddRef), asCALL_THISCALL},
    {"ExportRef", asBEHAVE_RELEASE, "void f()", "void ExportRef release", asMETHOD(BMLAS_ExportRef, Release), asCALL_THISCALL},
};

static const ScriptObjectMethodRegistration kObjectMethodRegistrations[] = {
    {"ModContext", "string get_ModId() const", "string ModContext::get_ModId() const", asMETHOD(BML::ScriptModContextView, GetModId), asCALL_THISCALL},
    {"ModContext", "string GetModId() const", "string ModContext::GetModId() const", asMETHOD(BML::ScriptModContextView, GetModId), asCALL_THISCALL},
    {"ModContext", "string get_ModName() const", "string ModContext::get_ModName() const", asMETHOD(BML::ScriptModContextView, GetModName), asCALL_THISCALL},
    {"ModContext", "string GetModName() const", "string ModContext::GetModName() const", asMETHOD(BML::ScriptModContextView, GetModName), asCALL_THISCALL},
    {"ModContext", "CKContext@ GetCKContext() const", "CKContext@ ModContext::GetCKContext() const", asMETHOD(BML::ScriptModContextView, GetCKContext), asCALL_THISCALL},
    {"ModContext", "CKRenderContext@ GetRenderContext() const", "CKRenderContext@ ModContext::GetRenderContext() const", asMETHOD(BML::ScriptModContextView, GetRenderContext), asCALL_THISCALL},
    {"ModContext", "CKAttributeManager@ GetAttributeManager() const", "CKAttributeManager@ ModContext::GetAttributeManager() const", asMETHOD(BML::ScriptModContextView, GetAttributeManager), asCALL_THISCALL},
    {"ModContext", "CKBehaviorManager@ GetBehaviorManager() const", "CKBehaviorManager@ ModContext::GetBehaviorManager() const", asMETHOD(BML::ScriptModContextView, GetBehaviorManager), asCALL_THISCALL},
    {"ModContext", "CKCollisionManager@ GetCollisionManager() const", "CKCollisionManager@ ModContext::GetCollisionManager() const", asMETHOD(BML::ScriptModContextView, GetCollisionManager), asCALL_THISCALL},
    {"ModContext", "InputHook@ GetInputManager() const", "InputHook@ ModContext::GetInputManager() const", asMETHOD(BML::ScriptModContextView, GetInputManager), asCALL_THISCALL},
    {"ModContext", "CKMessageManager@ GetMessageManager() const", "CKMessageManager@ ModContext::GetMessageManager() const", asMETHOD(BML::ScriptModContextView, GetMessageManager), asCALL_THISCALL},
    {"ModContext", "CKPathManager@ GetPathManager() const", "CKPathManager@ ModContext::GetPathManager() const", asMETHOD(BML::ScriptModContextView, GetPathManager), asCALL_THISCALL},
    {"ModContext", "CKParameterManager@ GetParameterManager() const", "CKParameterManager@ ModContext::GetParameterManager() const", asMETHOD(BML::ScriptModContextView, GetParameterManager), asCALL_THISCALL},
    {"ModContext", "CKRenderManager@ GetRenderManager() const", "CKRenderManager@ ModContext::GetRenderManager() const", asMETHOD(BML::ScriptModContextView, GetRenderManager), asCALL_THISCALL},
    {"ModContext", "CKSoundManager@ GetSoundManager() const", "CKSoundManager@ ModContext::GetSoundManager() const", asMETHOD(BML::ScriptModContextView, GetSoundManager), asCALL_THISCALL},
    {"ModContext", "CKTimeManager@ GetTimeManager() const", "CKTimeManager@ ModContext::GetTimeManager() const", asMETHOD(BML::ScriptModContextView, GetTimeManager), asCALL_THISCALL},
    {"ModContext", "bool get_IsInGame() const", "bool ModContext::get_IsInGame() const", asMETHOD(BML::ScriptModContextView, IsInGame), asCALL_THISCALL},
    {"ModContext", "bool GetIsInGame() const", "bool ModContext::GetIsInGame() const", asMETHOD(BML::ScriptModContextView, IsInGame), asCALL_THISCALL},
    {"ModContext", "bool get_IsInLevel() const", "bool ModContext::get_IsInLevel() const", asMETHOD(BML::ScriptModContextView, IsInLevel), asCALL_THISCALL},
    {"ModContext", "bool GetIsInLevel() const", "bool ModContext::GetIsInLevel() const", asMETHOD(BML::ScriptModContextView, IsInLevel), asCALL_THISCALL},
    {"ModContext", "bool get_IsPaused() const", "bool ModContext::get_IsPaused() const", asMETHOD(BML::ScriptModContextView, IsPaused), asCALL_THISCALL},
    {"ModContext", "bool GetIsPaused() const", "bool ModContext::GetIsPaused() const", asMETHOD(BML::ScriptModContextView, IsPaused), asCALL_THISCALL},
    {"ModContext", "bool get_IsPlaying() const", "bool ModContext::get_IsPlaying() const", asMETHOD(BML::ScriptModContextView, IsPlaying), asCALL_THISCALL},
    {"ModContext", "bool GetIsPlaying() const", "bool ModContext::GetIsPlaying() const", asMETHOD(BML::ScriptModContextView, IsPlaying), asCALL_THISCALL},
    {"ModContext", "bool get_IsCheatEnabled() const", "bool ModContext::get_IsCheatEnabled() const", asMETHOD(BML::ScriptModContextView, IsCheatEnabled), asCALL_THISCALL},
    {"ModContext", "bool GetIsCheatEnabled() const", "bool ModContext::GetIsCheatEnabled() const", asMETHOD(BML::ScriptModContextView, IsCheatEnabled), asCALL_THISCALL},
    {"ModContext", "void EnableCheat(bool enable) const", "void ModContext::EnableCheat(bool enable) const", asMETHOD(BML::ScriptModContextView, EnableCheat), asCALL_THISCALL},
    {"ModContext", "void ExitGame() const", "void ModContext::ExitGame() const", asMETHOD(BML::ScriptModContextView, ExitGame), asCALL_THISCALL},
    {"ModContext", "float GetSRScore() const", "float ModContext::GetSRScore() const", asMETHOD(BML::ScriptModContextView, GetSRScore), asCALL_THISCALL},
    {"ModContext", "int GetHSScore() const", "int ModContext::GetHSScore() const", asMETHOD(BML::ScriptModContextView, GetHSScore), asCALL_THISCALL},
    {"ModContext", "string GetDirectoryUtf8(int type) const", "string ModContext::GetDirectoryUtf8(int type) const", asMETHOD(BML::ScriptModContextView, GetDirectoryUtf8), asCALL_THISCALL},
    {"ModContext", "float GetTimeMs() const", "float ModContext::GetTimeMs() const", asMETHOD(BML::ScriptModContextView, GetTimeMs), asCALL_THISCALL},
    {"ModContext", "float GetAbsoluteTimeMs() const", "float ModContext::GetAbsoluteTimeMs() const", asMETHOD(BML::ScriptModContextView, GetAbsoluteTimeMs), asCALL_THISCALL},
    {"ModContext", "float GetDeltaTimeMs() const", "float ModContext::GetDeltaTimeMs() const", asMETHOD(BML::ScriptModContextView, GetDeltaTimeMs), asCALL_THISCALL},
    {"ModContext", "uint GetFrameCount() const", "uint ModContext::GetFrameCount() const", asMETHOD(BML::ScriptModContextView, GetFrameCount), asCALL_THISCALL},
    {"ModContext", "bool IsKeyboardAttached() const", "bool ModContext::IsKeyboardAttached() const", asMETHOD(BML::ScriptModContextView, IsKeyboardAttached), asCALL_THISCALL},
    {"ModContext", "bool IsMouseAttached() const", "bool ModContext::IsMouseAttached() const", asMETHOD(BML::ScriptModContextView, IsMouseAttached), asCALL_THISCALL},
    {"ModContext", "bool IsKeyDown(CKKEYBOARD key) const", "bool ModContext::IsKeyDown(CKKEYBOARD key) const", asMETHOD(BML::ScriptModContextView, IsKeyDown), asCALL_THISCALL},
    {"ModContext", "bool IsKeyUp(CKKEYBOARD key) const", "bool ModContext::IsKeyUp(CKKEYBOARD key) const", asMETHOD(BML::ScriptModContextView, IsKeyUp), asCALL_THISCALL},
    {"ModContext", "bool IsKeyPressed(CKKEYBOARD key) const", "bool ModContext::IsKeyPressed(CKKEYBOARD key) const", asMETHOD(BML::ScriptModContextView, IsKeyPressed), asCALL_THISCALL},
    {"ModContext", "bool IsKeyReleased(CKKEYBOARD key) const", "bool ModContext::IsKeyReleased(CKKEYBOARD key) const", asMETHOD(BML::ScriptModContextView, IsKeyReleased), asCALL_THISCALL},
    {"ModContext", "bool IsKeyToggled(CKKEYBOARD key) const", "bool ModContext::IsKeyToggled(CKKEYBOARD key) const", asMETHOD(BML::ScriptModContextView, IsKeyToggled), asCALL_THISCALL},
    {"ModContext", "string GetKeyName(CKKEYBOARD key) const", "string ModContext::GetKeyName(CKKEYBOARD key) const", asMETHOD(BML::ScriptModContextView, GetKeyName), asCALL_THISCALL},
    {"ModContext", "int GetKeyFromName(const string &in name) const", "int ModContext::GetKeyFromName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetKeyFromName), asCALL_THISCALL},
    {"ModContext", "bool IsMouseButtonDown(CK_MOUSEBUTTON button) const", "bool ModContext::IsMouseButtonDown(CK_MOUSEBUTTON button) const", asMETHOD(BML::ScriptModContextView, IsMouseButtonDown), asCALL_THISCALL},
    {"ModContext", "bool IsMouseClicked(CK_MOUSEBUTTON button) const", "bool ModContext::IsMouseClicked(CK_MOUSEBUTTON button) const", asMETHOD(BML::ScriptModContextView, IsMouseClicked), asCALL_THISCALL},
    {"ModContext", "bool IsMouseToggled(CK_MOUSEBUTTON button) const", "bool ModContext::IsMouseToggled(CK_MOUSEBUTTON button) const", asMETHOD(BML::ScriptModContextView, IsMouseToggled), asCALL_THISCALL},
    {"ModContext", "Vx2DVector GetMousePosition(bool absolute = true) const", "Vx2DVector ModContext::GetMousePosition(bool absolute) const", asMETHOD(BML::ScriptModContextView, GetMousePosition), asCALL_THISCALL},
    {"ModContext", "Vx2DVector GetLastMousePosition() const", "Vx2DVector ModContext::GetLastMousePosition() const", asMETHOD(BML::ScriptModContextView, GetLastMousePosition), asCALL_THISCALL},
    {"ModContext", "VxVector GetMouseRelativePosition() const", "VxVector ModContext::GetMouseRelativePosition() const", asMETHOD(BML::ScriptModContextView, GetMouseRelativePosition), asCALL_THISCALL},
    {"ModContext", "bool IsObjectValid(CKObject@ object) const", "bool ModContext::IsObjectValid(CKObject@ object) const", asMETHOD(BML::ScriptModContextView, IsObjectValid), asCALL_THISCALL},
    {"ModContext", "int GetObjectId(CKObject@ object) const", "int ModContext::GetObjectId(CKObject@ object) const", asMETHOD(BML::ScriptModContextView, GetObjectId), asCALL_THISCALL},
    {"ModContext", "string GetObjectName(CKObject@ object) const", "string ModContext::GetObjectName(CKObject@ object) const", asMETHOD(BML::ScriptModContextView, GetObjectName), asCALL_THISCALL},
    {"ModContext", "int GetObjectClassId(CKObject@ object) const", "int ModContext::GetObjectClassId(CKObject@ object) const", asMETHOD(BML::ScriptModContextView, GetObjectClassId), asCALL_THISCALL},
    {"ModContext", "bool IsObjectVisible(CKObject@ object) const", "bool ModContext::IsObjectVisible(CKObject@ object) const", asMETHOD(BML::ScriptModContextView, IsObjectVisible), asCALL_THISCALL},
    {"ModContext", "bool IsObjectDynamic(CKObject@ object) const", "bool ModContext::IsObjectDynamic(CKObject@ object) const", asMETHOD(BML::ScriptModContextView, IsObjectDynamic), asCALL_THISCALL},
    {"ModContext", "int GetBeObjectPriority(CKBeObject@ object) const", "int ModContext::GetBeObjectPriority(CKBeObject@ object) const", asMETHOD(BML::ScriptModContextView, GetBeObjectPriority), asCALL_THISCALL},
    {"ModContext", "int GetBeObjectScriptCount(CKBeObject@ object) const", "int ModContext::GetBeObjectScriptCount(CKBeObject@ object) const", asMETHOD(BML::ScriptModContextView, GetBeObjectScriptCount), asCALL_THISCALL},
    {"ModContext", "int GetBeObjectAttributeCount(CKBeObject@ object) const", "int ModContext::GetBeObjectAttributeCount(CKBeObject@ object) const", asMETHOD(BML::ScriptModContextView, GetBeObjectAttributeCount), asCALL_THISCALL},
    {"ModContext", "VxVector Get3dEntityPosition(CK3dEntity@ entity) const", "VxVector ModContext::Get3dEntityPosition(CK3dEntity@ entity) const", asMETHOD(BML::ScriptModContextView, Get3dEntityPosition), asCALL_THISCALL},
    {"ModContext", "VxVector Get3dEntityScale(CK3dEntity@ entity, bool local = true) const", "VxVector ModContext::Get3dEntityScale(CK3dEntity@ entity, bool local) const", asMETHOD(BML::ScriptModContextView, Get3dEntityScale), asCALL_THISCALL},
    {"ModContext", "int Get3dEntityChildCount(CK3dEntity@ entity) const", "int ModContext::Get3dEntityChildCount(CK3dEntity@ entity) const", asMETHOD(BML::ScriptModContextView, Get3dEntityChildCount), asCALL_THISCALL},
    {"ModContext", "CK3dEntity@ Get3dEntityParent(CK3dEntity@ entity) const", "CK3dEntity@ ModContext::Get3dEntityParent(CK3dEntity@ entity) const", asMETHOD(BML::ScriptModContextView, Get3dEntityParent), asCALL_THISCALL},
    {"ModContext", "CKDataArray@ GetArrayByName(const string &in name) const", "CKDataArray@ ModContext::GetArrayByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetArrayByName), asCALL_THISCALL},
    {"ModContext", "CKGroup@ GetGroupByName(const string &in name) const", "CKGroup@ ModContext::GetGroupByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetGroupByName), asCALL_THISCALL},
    {"ModContext", "CKMaterial@ GetMaterialByName(const string &in name) const", "CKMaterial@ ModContext::GetMaterialByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetMaterialByName), asCALL_THISCALL},
    {"ModContext", "CKMesh@ GetMeshByName(const string &in name) const", "CKMesh@ ModContext::GetMeshByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetMeshByName), asCALL_THISCALL},
    {"ModContext", "CK2dEntity@ Get2dEntityByName(const string &in name) const", "CK2dEntity@ ModContext::Get2dEntityByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, Get2dEntityByName), asCALL_THISCALL},
    {"ModContext", "CK3dEntity@ Get3dEntityByName(const string &in name) const", "CK3dEntity@ ModContext::Get3dEntityByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, Get3dEntityByName), asCALL_THISCALL},
    {"ModContext", "CK3dObject@ Get3dObjectByName(const string &in name) const", "CK3dObject@ ModContext::Get3dObjectByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, Get3dObjectByName), asCALL_THISCALL},
    {"ModContext", "CKCamera@ GetCameraByName(const string &in name) const", "CKCamera@ ModContext::GetCameraByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetCameraByName), asCALL_THISCALL},
    {"ModContext", "CKTargetCamera@ GetTargetCameraByName(const string &in name) const", "CKTargetCamera@ ModContext::GetTargetCameraByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetTargetCameraByName), asCALL_THISCALL},
    {"ModContext", "CKLight@ GetLightByName(const string &in name) const", "CKLight@ ModContext::GetLightByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetLightByName), asCALL_THISCALL},
    {"ModContext", "CKTargetLight@ GetTargetLightByName(const string &in name) const", "CKTargetLight@ ModContext::GetTargetLightByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetTargetLightByName), asCALL_THISCALL},
    {"ModContext", "CKSound@ GetSoundByName(const string &in name) const", "CKSound@ ModContext::GetSoundByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetSoundByName), asCALL_THISCALL},
    {"ModContext", "CKTexture@ GetTextureByName(const string &in name) const", "CKTexture@ ModContext::GetTextureByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetTextureByName), asCALL_THISCALL},
    {"ModContext", "CKBehavior@ GetScriptByName(const string &in name) const", "CKBehavior@ ModContext::GetScriptByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetScriptByName), asCALL_THISCALL},
    {"ModContext", "void SetIC(CKBeObject@ object, bool hierarchy = false) const", "void ModContext::SetIC(CKBeObject@ object, bool hierarchy) const", asMETHOD(BML::ScriptModContextView, SetIC), asCALL_THISCALL},
    {"ModContext", "void RestoreIC(CKBeObject@ object, bool hierarchy = false) const", "void ModContext::RestoreIC(CKBeObject@ object, bool hierarchy) const", asMETHOD(BML::ScriptModContextView, RestoreIC), asCALL_THISCALL},
    {"ModContext", "void Show(CKBeObject@ object, CK_OBJECT_SHOWOPTION show = CKSHOW, bool hierarchy = false) const", "void ModContext::Show(CKBeObject@ object, CK_OBJECT_SHOWOPTION show, bool hierarchy) const", asMETHOD(BML::ScriptModContextView, Show), asCALL_THISCALL},
    {"ModContext", "int GetHUD() const", "int ModContext::GetHUD() const", asMETHOD(BML::ScriptModContextView, GetHUD), asCALL_THISCALL},
    {"ModContext", "void SetHUD(int mode) const", "void ModContext::SetHUD(int mode) const", asMETHOD(BML::ScriptModContextView, SetHUD), asCALL_THISCALL},
    {"ModContext", "void ShowTitle(bool show) const", "void ModContext::ShowTitle(bool show) const", asMETHOD(BML::ScriptModContextView, ShowTitle), asCALL_THISCALL},
    {"ModContext", "void ShowFPS(bool show) const", "void ModContext::ShowFPS(bool show) const", asMETHOD(BML::ScriptModContextView, ShowFPS), asCALL_THISCALL},
    {"ModContext", "void ShowSRTimer(bool show) const", "void ModContext::ShowSRTimer(bool show) const", asMETHOD(BML::ScriptModContextView, ShowSRTimer), asCALL_THISCALL},
    {"ModContext", "void StartSRTimer() const", "void ModContext::StartSRTimer() const", asMETHOD(BML::ScriptModContextView, StartSRTimer), asCALL_THISCALL},
    {"ModContext", "void PauseSRTimer() const", "void ModContext::PauseSRTimer() const", asMETHOD(BML::ScriptModContextView, PauseSRTimer), asCALL_THISCALL},
    {"ModContext", "void ResetSRTimer() const", "void ModContext::ResetSRTimer() const", asMETHOD(BML::ScriptModContextView, ResetSRTimer), asCALL_THISCALL},
    {"ModContext", "float GetSRTime() const", "float ModContext::GetSRTime() const", asMETHOD(BML::ScriptModContextView, GetSRTime), asCALL_THISCALL},
    {"ModContext", "TimerRef@ AddTimer(Timer@+ timer) const", "TimerRef@ ModContext::AddTimer(Timer@+ timer) const", asMETHOD(BML::ScriptModContextView, AddTimer), asCALL_THISCALL},
    {"ModContext", "CommandRef@ RegisterCommand(Command@+ command) const", "CommandRef@ ModContext::RegisterCommand(Command@+ command) const", asMETHOD(BML::ScriptModContextView, RegisterCommand), asCALL_THISCALL},
    {"ModContext", "bool UnregisterCommand(const string &in name) const", "bool ModContext::UnregisterCommand(const string &in name) const", asMETHOD(BML::ScriptModContextView, UnregisterCommand), asCALL_THISCALL},
    {"ModContext", "DataShareRequestRef@ RequestDataShare(DataShareRequest@+ request) const", "DataShareRequestRef@ ModContext::RequestDataShare(DataShareRequest@+ request) const", asMETHOD(BML::ScriptModContextView, RequestDataShare), asCALL_THISCALL},
    {"ModContext", "bool RegisterBallType(const string &in ballFile, const string &in ballId, const string &in ballName, const string &in objName, float friction, float elasticity, float mass, const string &in collGroup, float linearDamp, float rotDamp, float force, float radius) const", "bool ModContext::RegisterBallType(...) const", asMETHOD(BML::ScriptModContextView, RegisterBallType), asCALL_THISCALL},
    {"ModContext", "bool RegisterFloorType(const string &in floorName, float friction, float elasticity, float mass, const string &in collGroup, bool enableColl) const", "bool ModContext::RegisterFloorType(...) const", asMETHOD(BML::ScriptModContextView, RegisterFloorType), asCALL_THISCALL},
    {"ModContext", "bool RegisterModulBall(const string &in modulName, bool fixed, float friction, float elasticity, float mass, const string &in collGroup, bool frozen, bool enableColl, bool calcMassCenter, float linearDamp, float rotDamp, float radius) const", "bool ModContext::RegisterModulBall(...) const", asMETHOD(BML::ScriptModContextView, RegisterModulBall), asCALL_THISCALL},
    {"ModContext", "bool RegisterModulConvex(const string &in modulName, bool fixed, float friction, float elasticity, float mass, const string &in collGroup, bool frozen, bool enableColl, bool calcMassCenter, float linearDamp, float rotDamp) const", "bool ModContext::RegisterModulConvex(...) const", asMETHOD(BML::ScriptModContextView, RegisterModulConvex), asCALL_THISCALL},
    {"ModContext", "bool RegisterTrafo(const string &in modulName) const", "bool ModContext::RegisterTrafo(const string &in modulName) const", asMETHOD(BML::ScriptModContextView, RegisterTrafo), asCALL_THISCALL},
    {"ModContext", "bool RegisterModul(const string &in modulName) const", "bool ModContext::RegisterModul(const string &in modulName) const", asMETHOD(BML::ScriptModContextView, RegisterModul), asCALL_THISCALL},
    {"ModContext", "string GetModRootUtf8() const", "string ModContext::GetModRootUtf8() const", asMETHOD(BML::ScriptModContextView, GetModRootUtf8), asCALL_THISCALL},
    {"ModContext", "string ResolveModPathUtf8(const string &in relativePath) const", "string ModContext::ResolveModPathUtf8(const string &in relativePath) const", asMETHOD(BML::ScriptModContextView, ResolveModPathUtf8), asCALL_THISCALL},
    {"ModContext", "bool ModFileExistsUtf8(const string &in relativePath) const", "bool ModContext::ModFileExistsUtf8(const string &in relativePath) const", asMETHOD(BML::ScriptModContextView, ModFileExistsUtf8), asCALL_THISCALL},
    {"ModContext", "bool ModDirectoryExistsUtf8(const string &in relativePath) const", "bool ModContext::ModDirectoryExistsUtf8(const string &in relativePath) const", asMETHOD(BML::ScriptModContextView, ModDirectoryExistsUtf8), asCALL_THISCALL},
    {"ModContext", "string ReadModTextFileUtf8(const string &in relativePath, const string &in defaultValue = \"\") const", "string ModContext::ReadModTextFileUtf8(const string &in relativePath, const string &in defaultValue = \"\") const", asMETHOD(BML::ScriptModContextView, ReadModTextFileUtf8), asCALL_THISCALL},
    {"ModContext", "int GetCommandCount() const", "int ModContext::GetCommandCount() const", asMETHOD(BML::ScriptModContextView, GetCommandCount), asCALL_THISCALL},
    {"ModContext", "string GetCommandName(int index) const", "string ModContext::GetCommandName(int index) const", asMETHOD(BML::ScriptModContextView, GetCommandName), asCALL_THISCALL},
    {"ModContext", "string GetCommandAlias(int index) const", "string ModContext::GetCommandAlias(int index) const", asMETHOD(BML::ScriptModContextView, GetCommandAlias), asCALL_THISCALL},
    {"ModContext", "string GetCommandDescription(int index) const", "string ModContext::GetCommandDescription(int index) const", asMETHOD(BML::ScriptModContextView, GetCommandDescription), asCALL_THISCALL},
    {"ModContext", "bool HasCommand(const string &in name) const", "bool ModContext::HasCommand(const string &in name) const", asMETHOD(BML::ScriptModContextView, HasCommand), asCALL_THISCALL},
    {"ModContext", "bool IsCommandCheat(const string &in name) const", "bool ModContext::IsCommandCheat(const string &in name) const", asMETHOD(BML::ScriptModContextView, IsCommandCheat), asCALL_THISCALL},
    {"ModContext", "ModRef@ FindMod(const string &in id) const", "ModRef@ ModContext::FindMod(const string &in id) const", asFUNCTION(BMLAS_ContextFindMod), asCALL_CDECL_OBJFIRST},
    {"ModContext", "int GetModCount() const", "int ModContext::GetModCount() const", asMETHOD(BML::ScriptModContextView, GetGlobalModCount), asCALL_THISCALL},
    {"ModContext", "string GetModId(int index) const", "string ModContext::GetModId(int index) const", asMETHOD(BML::ScriptModContextView, GetGlobalModId), asCALL_THISCALL},
    {"ModContext", "ModRef@ GetMod(int index) const", "ModRef@ ModContext::GetMod(int index) const", asFUNCTION(BMLAS_ContextGetMod), asCALL_CDECL_OBJFIRST},
    {"ModContext", "void SendIngameMessage(const string &in message) const", "void ModContext::SendIngameMessage(const string &in message) const", asMETHOD(BML::ScriptModContextView, SendIngameMessage), asCALL_THISCALL},
    {"ModContext", "void ClearIngameMessages() const", "void ModContext::ClearIngameMessages() const", asMETHOD(BML::ScriptModContextView, ClearIngameMessages), asCALL_THISCALL},
    {"ModContext", "void ExecuteCommand(const string &in command) const", "void ModContext::ExecuteCommand(const string &in command) const", asMETHOD(BML::ScriptModContextView, ExecuteCommand), asCALL_THISCALL},
    {"ModContext", "void SkipRenderForNextTick() const", "void ModContext::SkipRenderForNextTick() const", asMETHOD(BML::ScriptModContextView, SkipRenderForNextTick), asCALL_THISCALL},
    {"ModContext", "void OpenModsMenu() const", "void ModContext::OpenModsMenu() const", asMETHOD(BML::ScriptModContextView, OpenModsMenu), asCALL_THISCALL},
    {"ModContext", "void CloseModsMenu() const", "void ModContext::CloseModsMenu() const", asMETHOD(BML::ScriptModContextView, CloseModsMenu), asCALL_THISCALL},
    {"ModContext", "void OpenMapMenu() const", "void ModContext::OpenMapMenu() const", asMETHOD(BML::ScriptModContextView, OpenMapMenu), asCALL_THISCALL},
    {"ModContext", "void CloseMapMenu() const", "void ModContext::CloseMapMenu() const", asMETHOD(BML::ScriptModContextView, CloseMapMenu), asCALL_THISCALL},
    {"ModContext", "string GetConfigString(const string &in key, const string &in defaultValue = \"\") const", "string ModContext::GetConfigString(const string &in key, const string &in defaultValue = \"\") const", asMETHOD(BML::ScriptModContextView, GetConfigString), asCALL_THISCALL},
    {"ModContext", "void SetConfigString(const string &in key, const string &in value) const", "void ModContext::SetConfigString(const string &in key, const string &in value) const", asMETHOD(BML::ScriptModContextView, SetConfigString), asCALL_THISCALL},
    {"ModContext", "bool GetConfigBool(const string &in key, bool defaultValue = false) const", "bool ModContext::GetConfigBool(const string &in key, bool defaultValue = false) const", asMETHOD(BML::ScriptModContextView, GetConfigBool), asCALL_THISCALL},
    {"ModContext", "void SetConfigBool(const string &in key, bool value) const", "void ModContext::SetConfigBool(const string &in key, bool value) const", asMETHOD(BML::ScriptModContextView, SetConfigBool), asCALL_THISCALL},
    {"ModContext", "int GetConfigInt(const string &in key, int defaultValue = 0) const", "int ModContext::GetConfigInt(const string &in key, int defaultValue = 0) const", asMETHOD(BML::ScriptModContextView, GetConfigInt), asCALL_THISCALL},
    {"ModContext", "void SetConfigInt(const string &in key, int value) const", "void ModContext::SetConfigInt(const string &in key, int value) const", asMETHOD(BML::ScriptModContextView, SetConfigInt), asCALL_THISCALL},
    {"ModContext", "float GetConfigFloat(const string &in key, float defaultValue = 0.0f) const", "float ModContext::GetConfigFloat(const string &in key, float defaultValue = 0.0f) const", asMETHOD(BML::ScriptModContextView, GetConfigFloat), asCALL_THISCALL},
    {"ModContext", "void SetConfigFloat(const string &in key, float value) const", "void ModContext::SetConfigFloat(const string &in key, float value) const", asMETHOD(BML::ScriptModContextView, SetConfigFloat), asCALL_THISCALL},
    {"ModContext", "void LogInfo(const string &in message) const", "void ModContext::LogInfo(const string &in message) const", asMETHOD(BML::ScriptModContextView, LogInfo), asCALL_THISCALL},
    {"ModContext", "void LogWarn(const string &in message) const", "void ModContext::LogWarn(const string &in message) const", asMETHOD(BML::ScriptModContextView, LogWarn), asCALL_THISCALL},
    {"ModContext", "void LogError(const string &in message) const", "void ModContext::LogError(const string &in message) const", asMETHOD(BML::ScriptModContextView, LogError), asCALL_THISCALL},
    {"CallFrame", "bool get_IsValid() const", "bool CallFrame::get_IsValid() const", asMETHOD(BMLAS_CallFrame, IsValid), asCALL_THISCALL},
    {"CallFrame", "void Clear()", "void CallFrame::Clear()", asMETHOD(BMLAS_CallFrame, Clear), asCALL_THISCALL},
    {"CallFrame", "int get_ArgCount() const", "int CallFrame::get_ArgCount() const", asMETHOD(BMLAS_CallFrame, GetArgCount), asCALL_THISCALL},
    {"CallFrame", "int GetArgCount() const", "int CallFrame::GetArgCount() const", asMETHOD(BMLAS_CallFrame, GetArgCount), asCALL_THISCALL},
    {"CallFrame", "int GetArgType(uint index) const", "int CallFrame::GetArgType(uint index) const", asMETHOD(BMLAS_CallFrame, GetArgType), asCALL_THISCALL},
    {"CallFrame", "int ClearArg(uint index)", "int CallFrame::ClearArg(uint index)", asMETHOD(BMLAS_CallFrame, ClearArg), asCALL_THISCALL},
    {"CallFrame", "int SetBool(uint index, bool value)", "int CallFrame::SetBool(uint index, bool value)", asMETHOD(BMLAS_CallFrame, SetBool), asCALL_THISCALL},
    {"CallFrame", "int GetBool(uint index, bool &out value) const", "int CallFrame::GetBool(uint index, bool &out value) const", asMETHOD(BMLAS_CallFrame, GetBool), asCALL_THISCALL},
    {"CallFrame", "int SetInt(uint index, int value)", "int CallFrame::SetInt(uint index, int value)", asMETHOD(BMLAS_CallFrame, SetInt), asCALL_THISCALL},
    {"CallFrame", "int GetInt(uint index, int &out value) const", "int CallFrame::GetInt(uint index, int &out value) const", asMETHOD(BMLAS_CallFrame, GetInt), asCALL_THISCALL},
    {"CallFrame", "int SetFloat(uint index, float value)", "int CallFrame::SetFloat(uint index, float value)", asMETHOD(BMLAS_CallFrame, SetFloat), asCALL_THISCALL},
    {"CallFrame", "int GetFloat(uint index, float &out value) const", "int CallFrame::GetFloat(uint index, float &out value) const", asMETHOD(BMLAS_CallFrame, GetFloat), asCALL_THISCALL},
    {"CallFrame", "int SetString(uint index, const string &in value)", "int CallFrame::SetString(uint index, const string &in value)", asMETHOD(BMLAS_CallFrame, SetString), asCALL_THISCALL},
    {"CallFrame", "int GetString(uint index, string &out value) const", "int CallFrame::GetString(uint index, string &out value) const", asMETHOD(BMLAS_CallFrame, GetString), asCALL_THISCALL},
    {"CallFrame", "int SetResultBool(bool value)", "int CallFrame::SetResultBool(bool value)", asMETHOD(BMLAS_CallFrame, SetResultBool), asCALL_THISCALL},
    {"CallFrame", "int get_ResultType() const", "int CallFrame::get_ResultType() const", asMETHOD(BMLAS_CallFrame, GetResultType), asCALL_THISCALL},
    {"CallFrame", "int GetResultType() const", "int CallFrame::GetResultType() const", asMETHOD(BMLAS_CallFrame, GetResultType), asCALL_THISCALL},
    {"CallFrame", "int ClearResult()", "int CallFrame::ClearResult()", asMETHOD(BMLAS_CallFrame, ClearResult), asCALL_THISCALL},
    {"CallFrame", "int GetResultBool(bool &out value) const", "int CallFrame::GetResultBool(bool &out value) const", asMETHOD(BMLAS_CallFrame, GetResultBool), asCALL_THISCALL},
    {"CallFrame", "int SetResultInt(int value)", "int CallFrame::SetResultInt(int value)", asMETHOD(BMLAS_CallFrame, SetResultInt), asCALL_THISCALL},
    {"CallFrame", "int GetResultInt(int &out value) const", "int CallFrame::GetResultInt(int &out value) const", asMETHOD(BMLAS_CallFrame, GetResultInt), asCALL_THISCALL},
    {"CallFrame", "int SetResultFloat(float value)", "int CallFrame::SetResultFloat(float value)", asMETHOD(BMLAS_CallFrame, SetResultFloat), asCALL_THISCALL},
    {"CallFrame", "int GetResultFloat(float &out value) const", "int CallFrame::GetResultFloat(float &out value) const", asMETHOD(BMLAS_CallFrame, GetResultFloat), asCALL_THISCALL},
    {"CallFrame", "int SetResultString(const string &in value)", "int CallFrame::SetResultString(const string &in value)", asMETHOD(BMLAS_CallFrame, SetResultString), asCALL_THISCALL},
    {"CallFrame", "int GetResultString(string &out value) const", "int CallFrame::GetResultString(string &out value) const", asMETHOD(BMLAS_CallFrame, GetResultString), asCALL_THISCALL},
    {"TimerRef", "bool get_IsValid() const", "bool TimerRef::get_IsValid() const", asMETHOD(BML::ScriptTimerRef, IsValid), asCALL_THISCALL},
    {"TimerRef", "bool IsValid() const", "bool TimerRef::IsValid() const", asMETHOD(BML::ScriptTimerRef, IsValid), asCALL_THISCALL},
    {"TimerRef", "int get_Id() const", "int TimerRef::get_Id() const", asMETHOD(BML::ScriptTimerRef, GetId), asCALL_THISCALL},
    {"TimerRef", "string get_Name() const", "string TimerRef::get_Name() const", asMETHOD(BML::ScriptTimerRef, GetName), asCALL_THISCALL},
    {"TimerRef", "int get_State() const", "int TimerRef::get_State() const", asMETHOD(BML::ScriptTimerRef, GetState), asCALL_THISCALL},
    {"TimerRef", "int get_CompletedIterations() const", "int TimerRef::get_CompletedIterations() const", asMETHOD(BML::ScriptTimerRef, GetCompletedIterations), asCALL_THISCALL},
    {"TimerRef", "int get_RemainingIterations() const", "int TimerRef::get_RemainingIterations() const", asMETHOD(BML::ScriptTimerRef, GetRemainingIterations), asCALL_THISCALL},
    {"TimerRef", "float get_Progress() const", "float TimerRef::get_Progress() const", asMETHOD(BML::ScriptTimerRef, GetProgress), asCALL_THISCALL},
    {"TimerRef", "void Pause()", "void TimerRef::Pause()", asMETHOD(BML::ScriptTimerRef, Pause), asCALL_THISCALL},
    {"TimerRef", "void Resume()", "void TimerRef::Resume()", asMETHOD(BML::ScriptTimerRef, Resume), asCALL_THISCALL},
    {"TimerRef", "void Cancel()", "void TimerRef::Cancel()", asMETHOD(BML::ScriptTimerRef, Cancel), asCALL_THISCALL},
    {"TimerEvent", "bool get_IsValid() const", "bool TimerEvent::get_IsValid() const", asMETHOD(BML::ScriptTimerEventView, IsValid), asCALL_THISCALL},
    {"TimerEvent", "int get_Id() const", "int TimerEvent::get_Id() const", asMETHOD(BML::ScriptTimerEventView, GetId), asCALL_THISCALL},
    {"TimerEvent", "string get_Name() const", "string TimerEvent::get_Name() const", asMETHOD(BML::ScriptTimerEventView, GetName), asCALL_THISCALL},
    {"TimerEvent", "int get_State() const", "int TimerEvent::get_State() const", asMETHOD(BML::ScriptTimerEventView, GetState), asCALL_THISCALL},
    {"TimerEvent", "int get_Type() const", "int TimerEvent::get_Type() const", asMETHOD(BML::ScriptTimerEventView, GetType), asCALL_THISCALL},
    {"TimerEvent", "int get_TimeBase() const", "int TimerEvent::get_TimeBase() const", asMETHOD(BML::ScriptTimerEventView, GetTimeBase), asCALL_THISCALL},
    {"TimerEvent", "int get_CompletedIterations() const", "int TimerEvent::get_CompletedIterations() const", asMETHOD(BML::ScriptTimerEventView, GetCompletedIterations), asCALL_THISCALL},
    {"TimerEvent", "int get_RemainingIterations() const", "int TimerEvent::get_RemainingIterations() const", asMETHOD(BML::ScriptTimerEventView, GetRemainingIterations), asCALL_THISCALL},
    {"TimerEvent", "float get_Progress() const", "float TimerEvent::get_Progress() const", asMETHOD(BML::ScriptTimerEventView, GetProgress), asCALL_THISCALL},
    {"DataShareEvent", "bool get_Exists() const", "bool DataShareEvent::get_Exists() const", asMETHOD(BML::ScriptDataShareEventView, Exists), asCALL_THISCALL},
    {"DataShareEvent", "string get_Key() const", "string DataShareEvent::get_Key() const", asMETHOD(BML::ScriptDataShareEventView, GetKey), asCALL_THISCALL},
    {"DataShareEvent", "int get_Type() const", "int DataShareEvent::get_Type() const", asMETHOD(BML::ScriptDataShareEventView, GetType), asCALL_THISCALL},
    {"DataShareEvent", "string get_StringValue() const", "string DataShareEvent::get_StringValue() const", asMETHOD(BML::ScriptDataShareEventView, GetString), asCALL_THISCALL},
    {"DataShareEvent", "bool get_BoolValue() const", "bool DataShareEvent::get_BoolValue() const", asMETHOD(BML::ScriptDataShareEventView, GetBool), asCALL_THISCALL},
    {"DataShareEvent", "int get_IntValue() const", "int DataShareEvent::get_IntValue() const", asMETHOD(BML::ScriptDataShareEventView, GetInt), asCALL_THISCALL},
    {"DataShareEvent", "float get_FloatValue() const", "float DataShareEvent::get_FloatValue() const", asMETHOD(BML::ScriptDataShareEventView, GetFloat), asCALL_THISCALL},
    {"DataShareRequestRef", "bool get_IsValid() const", "bool DataShareRequestRef::get_IsValid() const", asMETHOD(BML::ScriptDataShareRequestRef, IsValid), asCALL_THISCALL},
    {"DataShareRequestRef", "string get_Key() const", "string DataShareRequestRef::get_Key() const", asMETHOD(BML::ScriptDataShareRequestRef, GetKey), asCALL_THISCALL},
    {"DataShareRequestRef", "int get_Type() const", "int DataShareRequestRef::get_Type() const", asMETHOD(BML::ScriptDataShareRequestRef, GetType), asCALL_THISCALL},
    {"DataShareRequestRef", "bool Cancel()", "bool DataShareRequestRef::Cancel()", asMETHOD(BML::ScriptDataShareRequestRef, Cancel), asCALL_THISCALL},
    {"InputHook", "bool IsKeyboardAttached() const", "bool InputHook::IsKeyboardAttached() const", asFUNCTION(BMLAS_InputHookIsKeyboardAttached), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsMouseAttached() const", "bool InputHook::IsMouseAttached() const", asFUNCTION(BMLAS_InputHookIsMouseAttached), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsKeyDown(CKKEYBOARD key) const", "bool InputHook::IsKeyDown(CKKEYBOARD key) const", asFUNCTION(BMLAS_InputHookIsKeyDown), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsKeyUp(CKKEYBOARD key) const", "bool InputHook::IsKeyUp(CKKEYBOARD key) const", asFUNCTION(BMLAS_InputHookIsKeyUp), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsKeyPressed(CKKEYBOARD key) const", "bool InputHook::IsKeyPressed(CKKEYBOARD key) const", asFUNCTION(BMLAS_InputHookIsKeyPressed), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsKeyReleased(CKKEYBOARD key) const", "bool InputHook::IsKeyReleased(CKKEYBOARD key) const", asFUNCTION(BMLAS_InputHookIsKeyReleased), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsKeyToggled(CKKEYBOARD key) const", "bool InputHook::IsKeyToggled(CKKEYBOARD key) const", asFUNCTION(BMLAS_InputHookIsKeyToggled), asCALL_CDECL_OBJFIRST},
    {"InputHook", "string GetKeyName(CKKEYBOARD key) const", "string InputHook::GetKeyName(CKKEYBOARD key) const", asFUNCTION(BMLAS_InputHookGetKeyName), asCALL_CDECL_OBJFIRST},
    {"InputHook", "int GetKeyFromName(const string &in name) const", "int InputHook::GetKeyFromName(const string &in name) const", asFUNCTION(BMLAS_InputHookGetKeyFromName), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsMouseButtonDown(CK_MOUSEBUTTON button) const", "bool InputHook::IsMouseButtonDown(CK_MOUSEBUTTON button) const", asFUNCTION(BMLAS_InputHookIsMouseButtonDown), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsMouseClicked(CK_MOUSEBUTTON button) const", "bool InputHook::IsMouseClicked(CK_MOUSEBUTTON button) const", asFUNCTION(BMLAS_InputHookIsMouseClicked), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsMouseToggled(CK_MOUSEBUTTON button) const", "bool InputHook::IsMouseToggled(CK_MOUSEBUTTON button) const", asFUNCTION(BMLAS_InputHookIsMouseToggled), asCALL_CDECL_OBJFIRST},
    {"InputHook", "Vx2DVector GetMousePosition(bool absolute = true) const", "Vx2DVector InputHook::GetMousePosition(bool absolute) const", asFUNCTION(BMLAS_InputHookGetMousePosition), asCALL_CDECL_OBJFIRST},
    {"InputHook", "Vx2DVector GetLastMousePosition() const", "Vx2DVector InputHook::GetLastMousePosition() const", asFUNCTION(BMLAS_InputHookGetLastMousePosition), asCALL_CDECL_OBJFIRST},
    {"InputHook", "VxVector GetMouseRelativePosition() const", "VxVector InputHook::GetMouseRelativePosition() const", asFUNCTION(BMLAS_InputHookGetMouseRelativePosition), asCALL_CDECL_OBJFIRST},
    {"ModRef", "string get_Id() const", "string ModRef::get_Id() const", asMETHOD(BMLAS_ModRef, GetId), asCALL_THISCALL},
    {"ModRef", "string GetId() const", "string ModRef::GetId() const", asMETHOD(BMLAS_ModRef, GetId), asCALL_THISCALL},
    {"ModRef", "string get_Name() const", "string ModRef::get_Name() const", asMETHOD(BMLAS_ModRef, GetName), asCALL_THISCALL},
    {"ModRef", "string get_Version() const", "string ModRef::get_Version() const", asMETHOD(BMLAS_ModRef, GetVersion), asCALL_THISCALL},
    {"ModRef", "string get_Author() const", "string ModRef::get_Author() const", asMETHOD(BMLAS_ModRef, GetAuthor), asCALL_THISCALL},
    {"ModRef", "string get_Description() const", "string ModRef::get_Description() const", asMETHOD(BMLAS_ModRef, GetDescription), asCALL_THISCALL},
    {"ModRef", "string get_BMLVersion() const", "string ModRef::get_BMLVersion() const", asMETHOD(BMLAS_ModRef, GetBMLVersion), asCALL_THISCALL},
    {"ModRef", "int get_BMLVersionMajor() const", "int ModRef::get_BMLVersionMajor() const", asMETHOD(BMLAS_ModRef, GetBMLVersionMajor), asCALL_THISCALL},
    {"ModRef", "int get_BMLVersionMinor() const", "int ModRef::get_BMLVersionMinor() const", asMETHOD(BMLAS_ModRef, GetBMLVersionMinor), asCALL_THISCALL},
    {"ModRef", "int get_BMLVersionPatch() const", "int ModRef::get_BMLVersionPatch() const", asMETHOD(BMLAS_ModRef, GetBMLVersionPatch), asCALL_THISCALL},
    {"ModRef", "int get_Kind() const", "int ModRef::get_Kind() const", asMETHOD(BMLAS_ModRef, GetKind), asCALL_THISCALL},
    {"ModRef", "int get_State() const", "int ModRef::get_State() const", asMETHOD(BMLAS_ModRef, GetState), asCALL_THISCALL},
    {"ModRef", "bool get_IsValid() const", "bool ModRef::get_IsValid() const", asMETHOD(BMLAS_ModRef, IsValid), asCALL_THISCALL},
    {"ModRef", "bool get_IsScript() const", "bool ModRef::get_IsScript() const", asMETHOD(BMLAS_ModRef, IsScript), asCALL_THISCALL},
    {"ModRef", "bool get_IsFailed() const", "bool ModRef::get_IsFailed() const", asMETHOD(BMLAS_ModRef, IsFailed), asCALL_THISCALL},
    {"ModRef", "int CheckDependencies() const", "int ModRef::CheckDependencies() const", asMETHOD(BMLAS_ModRef, CheckDependencies), asCALL_THISCALL},
    {"ModRef", "int GetDependencyCount() const", "int ModRef::GetDependencyCount() const", asMETHOD(BMLAS_ModRef, GetDependencyCount), asCALL_THISCALL},
    {"ModRef", "string GetDependencyId(int index) const", "string ModRef::GetDependencyId(int index) const", asMETHOD(BMLAS_ModRef, GetDependencyId), asCALL_THISCALL},
    {"ModRef", "string GetDependencyVersion(int index) const", "string ModRef::GetDependencyVersion(int index) const", asMETHOD(BMLAS_ModRef, GetDependencyVersion), asCALL_THISCALL},
    {"ModRef", "int GetDependencyVersionMajor(int index) const", "int ModRef::GetDependencyVersionMajor(int index) const", asMETHOD(BMLAS_ModRef, GetDependencyVersionMajor), asCALL_THISCALL},
    {"ModRef", "int GetDependencyVersionMinor(int index) const", "int ModRef::GetDependencyVersionMinor(int index) const", asMETHOD(BMLAS_ModRef, GetDependencyVersionMinor), asCALL_THISCALL},
    {"ModRef", "int GetDependencyVersionPatch(int index) const", "int ModRef::GetDependencyVersionPatch(int index) const", asMETHOD(BMLAS_ModRef, GetDependencyVersionPatch), asCALL_THISCALL},
    {"ModRef", "bool IsDependencyOptional(int index) const", "bool ModRef::IsDependencyOptional(int index) const", asMETHOD(BMLAS_ModRef, IsDependencyOptional), asCALL_THISCALL},
    {"ModRef", "int GetExportCount() const", "int ModRef::GetExportCount() const", asMETHOD(BMLAS_ModRef, GetExportCount), asCALL_THISCALL},
    {"ModRef", "string GetExportName(int index) const", "string ModRef::GetExportName(int index) const", asMETHOD(BMLAS_ModRef, GetExportName), asCALL_THISCALL},
    {"ModRef", "string GetExportSignature(int index) const", "string ModRef::GetExportSignature(int index) const", asMETHOD(BMLAS_ModRef, GetExportSignature), asCALL_THISCALL},
    {"ModRef", "ExportRef@ GetExport(int index) const", "ExportRef@ ModRef::GetExport(int index) const", asMETHOD(BMLAS_ModRef, GetExport), asCALL_THISCALL},
    {"ModRef", "string get_Diagnostic() const", "string ModRef::get_Diagnostic() const", asMETHOD(BMLAS_ModRef, GetDiagnostic), asCALL_THISCALL},
    {"ModRef", "string GetDiagnostic() const", "string ModRef::GetDiagnostic() const", asMETHOD(BMLAS_ModRef, GetDiagnostic), asCALL_THISCALL},
    {"ModRef", "ExportRef@ FindExport(const string &in name, const string &in signature = \"\") const", "ExportRef@ ModRef::FindExport(const string &in name, const string &in signature = \"\") const", asMETHOD(BMLAS_ModRef, FindExport), asCALL_THISCALL},
    {"ModRef", "int TryFindExport(const string &in name, ExportRef@ &out exportRef, const string &in signature = \"\") const", "int ModRef::TryFindExport(const string &in name, ExportRef@ &out exportRef, const string &in signature = \"\") const", asMETHOD(BMLAS_ModRef, TryFindExport), asCALL_THISCALL},
    {"ExportRef", "string get_ModId() const", "string ExportRef::get_ModId() const", asMETHOD(BMLAS_ExportRef, GetModId), asCALL_THISCALL},
    {"ExportRef", "string get_Name() const", "string ExportRef::get_Name() const", asMETHOD(BMLAS_ExportRef, GetName), asCALL_THISCALL},
    {"ExportRef", "string get_Signature() const", "string ExportRef::get_Signature() const", asMETHOD(BMLAS_ExportRef, GetSignature), asCALL_THISCALL},
    {"ExportRef", "bool get_IsValid() const", "bool ExportRef::get_IsValid() const", asMETHOD(BMLAS_ExportRef, IsValid), asCALL_THISCALL},
    {"ExportRef", "int Call(CallFrame@ frame) const", "int ExportRef::Call(CallFrame@ frame) const", asMETHOD(BMLAS_ExportRef, Call), asCALL_THISCALL},
    {"ExportRef", "int CallVoid() const", "int ExportRef::CallVoid() const", asMETHOD(BMLAS_ExportRef, CallVoid), asCALL_THISCALL},
    {"ExportRef", "int CallString(const string &in argument, string &out result) const", "int ExportRef::CallString(const string &in argument, string &out result) const", asMETHOD(BMLAS_ExportRef, CallString), asCALL_THISCALL},
    {"ExportRef", "int CallString(string &out result) const", "int ExportRef::CallString(string &out result) const", asMETHOD(BMLAS_ExportRef, CallStringNoArgs), asCALL_THISCALL},
    {"ExportRef", "int CallBool(bool argument, bool &out result) const", "int ExportRef::CallBool(bool argument, bool &out result) const", asMETHOD(BMLAS_ExportRef, CallBool), asCALL_THISCALL},
    {"ExportRef", "int CallBool(bool &out result) const", "int ExportRef::CallBool(bool &out result) const", asMETHOD(BMLAS_ExportRef, CallBoolNoArgs), asCALL_THISCALL},
    {"ExportRef", "int CallInt(int argument, int &out result) const", "int ExportRef::CallInt(int argument, int &out result) const", asMETHOD(BMLAS_ExportRef, CallInt), asCALL_THISCALL},
    {"ExportRef", "int CallInt(int &out result) const", "int ExportRef::CallInt(int &out result) const", asMETHOD(BMLAS_ExportRef, CallIntNoArgs), asCALL_THISCALL},
    {"ExportRef", "int CallFloat(float argument, float &out result) const", "int ExportRef::CallFloat(float argument, float &out result) const", asMETHOD(BMLAS_ExportRef, CallFloat), asCALL_THISCALL},
    {"ExportRef", "int CallFloat(float &out result) const", "int ExportRef::CallFloat(float &out result) const", asMETHOD(BMLAS_ExportRef, CallFloatNoArgs), asCALL_THISCALL},
    {"RenderEvent", "int get_Flags() const", "int RenderEvent::get_Flags() const", asMETHOD(BML::ScriptRenderEventView, GetFlags), asCALL_THISCALL},
    {"RenderEvent", "int GetFlags() const", "int RenderEvent::GetFlags() const", asMETHOD(BML::ScriptRenderEventView, GetFlags), asCALL_THISCALL},
    {"CheatEvent", "bool get_Enabled() const", "bool CheatEvent::get_Enabled() const", asMETHOD(BML::ScriptCheatEventView, GetEnabled), asCALL_THISCALL},
    {"CheatEvent", "bool IsEnabled() const", "bool CheatEvent::IsEnabled() const", asMETHOD(BML::ScriptCheatEventView, IsEnabled), asCALL_THISCALL},
    {"LoadObjectEvent", "string get_Filename() const", "string LoadObjectEvent::get_Filename() const", asMETHOD(BML::ScriptLoadObjectEventView, GetFilename), asCALL_THISCALL},
    {"LoadObjectEvent", "bool get_IsMap() const", "bool LoadObjectEvent::get_IsMap() const", asMETHOD(BML::ScriptLoadObjectEventView, IsMap), asCALL_THISCALL},
    {"LoadObjectEvent", "string get_MasterName() const", "string LoadObjectEvent::get_MasterName() const", asMETHOD(BML::ScriptLoadObjectEventView, GetMasterName), asCALL_THISCALL},
    {"LoadObjectEvent", "int get_FilterClass() const", "int LoadObjectEvent::get_FilterClass() const", asMETHOD(BML::ScriptLoadObjectEventView, GetFilterClass), asCALL_THISCALL},
    {"LoadObjectEvent", "bool get_AddToScene() const", "bool LoadObjectEvent::get_AddToScene() const", asMETHOD(BML::ScriptLoadObjectEventView, GetAddToScene), asCALL_THISCALL},
    {"LoadObjectEvent", "bool get_ReuseMeshes() const", "bool LoadObjectEvent::get_ReuseMeshes() const", asMETHOD(BML::ScriptLoadObjectEventView, GetReuseMeshes), asCALL_THISCALL},
    {"LoadObjectEvent", "bool get_ReuseMaterials() const", "bool LoadObjectEvent::get_ReuseMaterials() const", asMETHOD(BML::ScriptLoadObjectEventView, GetReuseMaterials), asCALL_THISCALL},
    {"LoadObjectEvent", "bool get_IsDynamic() const", "bool LoadObjectEvent::get_IsDynamic() const", asMETHOD(BML::ScriptLoadObjectEventView, IsDynamic), asCALL_THISCALL},
    {"LoadScriptEvent", "string get_Filename() const", "string LoadScriptEvent::get_Filename() const", asMETHOD(BML::ScriptLoadScriptEventView, GetFilename), asCALL_THISCALL},
    {"LoadScriptEvent", "int get_ScriptId() const", "int LoadScriptEvent::get_ScriptId() const", asMETHOD(BML::ScriptLoadScriptEventView, GetScriptId), asCALL_THISCALL},
    {"CommandEvent", "int get_Phase() const", "int CommandEvent::get_Phase() const", asMETHOD(BML::ScriptCommandEventView, GetPhase), asCALL_THISCALL},
    {"CommandEvent", "bool get_IsPre() const", "bool CommandEvent::get_IsPre() const", asMETHOD(BML::ScriptCommandEventView, IsPre), asCALL_THISCALL},
    {"CommandEvent", "bool get_IsPost() const", "bool CommandEvent::get_IsPost() const", asMETHOD(BML::ScriptCommandEventView, IsPost), asCALL_THISCALL},
    {"CommandEvent", "bool get_IsExecute() const", "bool CommandEvent::get_IsExecute() const", asMETHOD(BML::ScriptCommandEventView, IsExecute), asCALL_THISCALL},
    {"CommandEvent", "bool get_IsComplete() const", "bool CommandEvent::get_IsComplete() const", asMETHOD(BML::ScriptCommandEventView, IsComplete), asCALL_THISCALL},
    {"CommandEvent", "string get_CommandName() const", "string CommandEvent::get_CommandName() const", asMETHOD(BML::ScriptCommandEventView, GetCommandName), asCALL_THISCALL},
    {"CommandEvent", "int get_ArgCount() const", "int CommandEvent::get_ArgCount() const", asMETHOD(BML::ScriptCommandEventView, GetArgCount), asCALL_THISCALL},
    {"CommandEvent", "string GetArg(int index) const", "string CommandEvent::GetArg(int index) const", asMETHOD(BML::ScriptCommandEventView, GetArg), asCALL_THISCALL},
    {"CommandEvent", "string get_ArgsText() const", "string CommandEvent::get_ArgsText() const", asMETHOD(BML::ScriptCommandEventView, GetArgsText), asCALL_THISCALL},
    {"CommandEvent", "bool get_IsCheat() const", "bool CommandEvent::get_IsCheat() const", asMETHOD(BML::ScriptCommandEventView, IsCheat), asCALL_THISCALL},
    {"CommandCompletion", "void Add(const string &in value) const", "void CommandCompletion::Add(const string &in value) const", asMETHOD(BML::ScriptCommandCompletion, Add), asCALL_THISCALL},
    {"CommandCompletion", "int get_Count() const", "int CommandCompletion::get_Count() const", asMETHOD(BML::ScriptCommandCompletion, GetCount), asCALL_THISCALL},
    {"CommandCompletion", "string At(int index) const", "string CommandCompletion::At(int index) const", asMETHOD(BML::ScriptCommandCompletion, At), asCALL_THISCALL},
    {"CommandRef", "bool get_IsValid() const", "bool CommandRef::get_IsValid() const", asMETHOD(BML::ScriptCommandRef, IsValid), asCALL_THISCALL},
    {"CommandRef", "string get_Name() const", "string CommandRef::get_Name() const", asMETHOD(BML::ScriptCommandRef, GetName), asCALL_THISCALL},
    {"CommandRef", "string get_Alias() const", "string CommandRef::get_Alias() const", asMETHOD(BML::ScriptCommandRef, GetAlias), asCALL_THISCALL},
    {"CommandRef", "bool get_IsCheat() const", "bool CommandRef::get_IsCheat() const", asMETHOD(BML::ScriptCommandRef, IsCheat), asCALL_THISCALL},
    {"CommandRef", "bool get_IsEnabled() const", "bool CommandRef::get_IsEnabled() const", asMETHOD(BML::ScriptCommandRef, IsEnabled), asCALL_THISCALL},
    {"CommandRef", "bool SetEnabled(bool enabled)", "bool CommandRef::SetEnabled(bool enabled)", asMETHOD(BML::ScriptCommandRef, SetEnabled), asCALL_THISCALL},
    {"CommandRef", "bool Unregister()", "bool CommandRef::Unregister()", asMETHOD(BML::ScriptCommandRef, Unregister), asCALL_THISCALL},
    {"PhysicalizeEvent", "int get_TargetId() const", "int PhysicalizeEvent::get_TargetId() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetTargetId), asCALL_THISCALL},
    {"PhysicalizeEvent", "string get_TargetName() const", "string PhysicalizeEvent::get_TargetName() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetTargetName), asCALL_THISCALL},
    {"PhysicalizeEvent", "bool get_Fixed() const", "bool PhysicalizeEvent::get_Fixed() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetFixed), asCALL_THISCALL},
    {"PhysicalizeEvent", "float get_Friction() const", "float PhysicalizeEvent::get_Friction() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetFriction), asCALL_THISCALL},
    {"PhysicalizeEvent", "float get_Elasticity() const", "float PhysicalizeEvent::get_Elasticity() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetElasticity), asCALL_THISCALL},
    {"PhysicalizeEvent", "float get_Mass() const", "float PhysicalizeEvent::get_Mass() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetMass), asCALL_THISCALL},
    {"PhysicalizeEvent", "string get_CollisionGroup() const", "string PhysicalizeEvent::get_CollisionGroup() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetCollisionGroup), asCALL_THISCALL},
    {"PhysicalizeEvent", "bool get_StartFrozen() const", "bool PhysicalizeEvent::get_StartFrozen() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetStartFrozen), asCALL_THISCALL},
    {"PhysicalizeEvent", "bool get_EnableCollision() const", "bool PhysicalizeEvent::get_EnableCollision() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetEnableCollision), asCALL_THISCALL},
    {"PhysicalizeEvent", "bool get_AutoCalcMassCenter() const", "bool PhysicalizeEvent::get_AutoCalcMassCenter() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetAutoCalcMassCenter), asCALL_THISCALL},
    {"PhysicalizeEvent", "float get_LinearDamp() const", "float PhysicalizeEvent::get_LinearDamp() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetLinearDamp), asCALL_THISCALL},
    {"PhysicalizeEvent", "float get_RotDamp() const", "float PhysicalizeEvent::get_RotDamp() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetRotDamp), asCALL_THISCALL},
    {"PhysicalizeEvent", "string get_CollisionSurface() const", "string PhysicalizeEvent::get_CollisionSurface() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetCollisionSurface), asCALL_THISCALL},
    {"PhysicalizeEvent", "float get_MassCenterX() const", "float PhysicalizeEvent::get_MassCenterX() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetMassCenterX), asCALL_THISCALL},
    {"PhysicalizeEvent", "float get_MassCenterY() const", "float PhysicalizeEvent::get_MassCenterY() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetMassCenterY), asCALL_THISCALL},
    {"PhysicalizeEvent", "float get_MassCenterZ() const", "float PhysicalizeEvent::get_MassCenterZ() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetMassCenterZ), asCALL_THISCALL},
    {"PhysicalizeEvent", "int get_ConvexCount() const", "int PhysicalizeEvent::get_ConvexCount() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetConvexCount), asCALL_THISCALL},
    {"PhysicalizeEvent", "int get_BallCount() const", "int PhysicalizeEvent::get_BallCount() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetBallCount), asCALL_THISCALL},
    {"PhysicalizeEvent", "int get_ConcaveCount() const", "int PhysicalizeEvent::get_ConcaveCount() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetConcaveCount), asCALL_THISCALL},
    {"ObjectEvent", "int get_TargetId() const", "int ObjectEvent::get_TargetId() const", asMETHOD(BML::ScriptObjectEventView, GetTargetId), asCALL_THISCALL},
    {"ObjectEvent", "string get_TargetName() const", "string ObjectEvent::get_TargetName() const", asMETHOD(BML::ScriptObjectEventView, GetTargetName), asCALL_THISCALL},
};

static const ScriptUiEnumValueRegistration kUiButtonTypeRegistrations[] = {
    {"BUTTON_MAIN", Bui::BUTTON_MAIN, "BML::UI::BUTTON_MAIN"},
    {"BUTTON_BACK", Bui::BUTTON_BACK, "BML::UI::BUTTON_BACK"},
    {"BUTTON_OPTION", Bui::BUTTON_OPTION, "BML::UI::BUTTON_OPTION"},
    {"BUTTON_LEVEL", Bui::BUTTON_LEVEL, "BML::UI::BUTTON_LEVEL"},
    {"BUTTON_KEY", Bui::BUTTON_KEY, "BML::UI::BUTTON_KEY"},
    {"BUTTON_SMALL", Bui::BUTTON_SMALL, "BML::UI::BUTTON_SMALL"},
    {"BUTTON_LEFT", Bui::BUTTON_LEFT, "BML::UI::BUTTON_LEFT"},
    {"BUTTON_RIGHT", Bui::BUTTON_RIGHT, "BML::UI::BUTTON_RIGHT"},
    {"BUTTON_PLUS", Bui::BUTTON_PLUS, "BML::UI::BUTTON_PLUS"},
    {"BUTTON_MINUS", Bui::BUTTON_MINUS, "BML::UI::BUTTON_MINUS"},
};

static const ScriptUiFunctionRegistration kUiFunctionRegistrations[] = {
    {"void SetCursorCoord(float x, float y)", "BML::UI::SetCursorCoord", asFUNCTION(BMLAS_UI_SetCursorCoord), asCALL_CDECL},
    {"float CoordToPixelX(float x)", "BML::UI::CoordToPixelX", asFUNCTION(BMLAS_UI_CoordToPixelX), asCALL_CDECL},
    {"float CoordToPixelY(float y)", "BML::UI::CoordToPixelY", asFUNCTION(BMLAS_UI_CoordToPixelY), asCALL_CDECL},
    {"float GetMenuPosX()", "BML::UI::GetMenuPosX", asFUNCTION(BMLAS_UI_GetMenuPosX), asCALL_CDECL},
    {"float GetMenuPosY()", "BML::UI::GetMenuPosY", asFUNCTION(BMLAS_UI_GetMenuPosY), asCALL_CDECL},
    {"float GetMenuSizeX()", "BML::UI::GetMenuSizeX", asFUNCTION(BMLAS_UI_GetMenuSizeX), asCALL_CDECL},
    {"float GetMenuSizeY()", "BML::UI::GetMenuSizeY", asFUNCTION(BMLAS_UI_GetMenuSizeY), asCALL_CDECL},
    {"int CalcPageCount(int totalCount, int pageSize)", "BML::UI::CalcPageCount", asFUNCTION(BMLAS_UI_CalcPageCount), asCALL_CDECL},
    {"bool CanPrevPage(int pageIndex)", "BML::UI::CanPrevPage", asFUNCTION(BMLAS_UI_CanPrevPage), asCALL_CDECL},
    {"bool CanNextPage(int pageIndex, int totalCount, int pageSize)", "BML::UI::CanNextPage", asFUNCTION(BMLAS_UI_CanNextPage), asCALL_CDECL},
    {"bool MainButton(const string &in label)", "BML::UI::MainButton", asFUNCTION(BMLAS_UI_MainButton), asCALL_CDECL},
    {"bool BackButton(const string &in label)", "BML::UI::BackButton", asFUNCTION(BMLAS_UI_BackButton), asCALL_CDECL},
    {"bool OptionButton(const string &in label)", "BML::UI::OptionButton", asFUNCTION(BMLAS_UI_OptionButton), asCALL_CDECL},
    {"bool LevelButton(const string &in label)", "BML::UI::LevelButton", asFUNCTION(BMLAS_UI_LevelButton), asCALL_CDECL},
    {"bool LevelButton(const string &in label, bool &inout selected)", "BML::UI::LevelButton(selected)", asFUNCTION(BMLAS_UI_LevelButtonSelected), asCALL_CDECL},
    {"bool SmallButton(const string &in label)", "BML::UI::SmallButton", asFUNCTION(BMLAS_UI_SmallButton), asCALL_CDECL},
    {"bool SmallButton(const string &in label, bool &inout selected)", "BML::UI::SmallButton(selected)", asFUNCTION(BMLAS_UI_SmallButtonSelected), asCALL_CDECL},
    {"bool LeftButton(const string &in label)", "BML::UI::LeftButton", asFUNCTION(BMLAS_UI_LeftButton), asCALL_CDECL},
    {"bool RightButton(const string &in label)", "BML::UI::RightButton", asFUNCTION(BMLAS_UI_RightButton), asCALL_CDECL},
    {"bool PlusButton(const string &in label)", "BML::UI::PlusButton", asFUNCTION(BMLAS_UI_PlusButton), asCALL_CDECL},
    {"bool MinusButton(const string &in label)", "BML::UI::MinusButton", asFUNCTION(BMLAS_UI_MinusButton), asCALL_CDECL},
    {"void Title(const string &in text, float y = 0.13f, float scale = 1.5f)", "BML::UI::Title", asFUNCTION(BMLAS_UI_Title), asCALL_CDECL},
    {"void WrappedText(const string &in text, float width, float baseX = 0.0f, float scale = 1.0f)", "BML::UI::WrappedText", asFUNCTION(BMLAS_UI_WrappedText), asCALL_CDECL},
    {"bool NavLeft(float x = 0.36f, float y = 0.124f)", "BML::UI::NavLeft", asFUNCTION(BMLAS_UI_NavLeft), asCALL_CDECL},
    {"bool NavRight(float x = 0.6038f, float y = 0.124f)", "BML::UI::NavRight", asFUNCTION(BMLAS_UI_NavRight), asCALL_CDECL},
    {"bool NavBack(float x = 0.4031f, float y = 0.85f)", "BML::UI::NavBack", asFUNCTION(BMLAS_UI_NavBack), asCALL_CDECL},
    {"bool YesNoButton(const string &in label, bool &inout value)", "BML::UI::YesNoButton", asFUNCTION(BMLAS_UI_YesNoButton), asCALL_CDECL},
    {"bool InputIntButton(const string &in label, int &inout value, int step = 1, int stepFast = 100)", "BML::UI::InputIntButton", asFUNCTION(BMLAS_UI_InputIntButton), asCALL_CDECL},
    {"bool InputFloatButton(const string &in label, float &inout value, float step = 0.0f, float stepFast = 0.0f)", "BML::UI::InputFloatButton", asFUNCTION(BMLAS_UI_InputFloatButton), asCALL_CDECL},
    {"bool SearchBar(string &inout text, float x = 0.4f, float y = 0.18f, float width = 0.2f)", "BML::UI::SearchBar", asFUNCTION(BMLAS_UI_SearchBar), asCALL_CDECL},
    {"void PlayMenuClickSound()", "BML::UI::PlayMenuClickSound", asFUNCTION(BMLAS_UI_PlayMenuClickSound), asCALL_CDECL},
    {"float GetButtonSizeX(ButtonType type)", "BML::UI::GetButtonSizeX", asFUNCTION(BMLAS_UI_GetButtonSizeX), asCALL_CDECL},
    {"float GetButtonSizeY(ButtonType type)", "BML::UI::GetButtonSizeY", asFUNCTION(BMLAS_UI_GetButtonSizeY), asCALL_CDECL},
    {"float GetButtonIndent(ButtonType type)", "BML::UI::GetButtonIndent", asFUNCTION(BMLAS_UI_GetButtonIndent), asCALL_CDECL},
    {"float GetButtonSizeCoordX(ButtonType type)", "BML::UI::GetButtonSizeCoordX", asFUNCTION(BMLAS_UI_GetButtonSizeCoordX), asCALL_CDECL},
    {"float GetButtonSizeCoordY(ButtonType type)", "BML::UI::GetButtonSizeCoordY", asFUNCTION(BMLAS_UI_GetButtonSizeCoordY), asCALL_CDECL},
    {"float GetButtonIndentCoord(ButtonType type)", "BML::UI::GetButtonIndentCoord", asFUNCTION(BMLAS_UI_GetButtonIndentCoord), asCALL_CDECL},
};

static const ScriptGlobalFunctionRegistration kGlobalFunctionRegistrations[] = {
    {"ModRef@ FindMod(const string &in id)", "ModRef@ FindMod(const string &in id)", asFUNCTION(BMLAS_FindMod), asCALL_CDECL},
    {"int GetModCount()", "int GetModCount()", asFUNCTION(BMLAS_GetModCount), asCALL_CDECL},
    {"string GetModId(int index)", "string GetModId(int index)", asFUNCTION(BMLAS_GetModId), asCALL_CDECL},
    {"ModRef@ GetMod(int index)", "ModRef@ GetMod(int index)", asFUNCTION(BMLAS_GetMod), asCALL_CDECL},
    {"string GetVersion()", "string GetVersion()", asFUNCTION(BMLAS_GetVersion), asCALL_CDECL},
    {"int GetVersionMajor()", "int GetVersionMajor()", asFUNCTION(BMLAS_GetVersionMajor), asCALL_CDECL},
    {"int GetVersionMinor()", "int GetVersionMinor()", asFUNCTION(BMLAS_GetVersionMinor), asCALL_CDECL},
    {"int GetVersionPatch()", "int GetVersionPatch()", asFUNCTION(BMLAS_GetVersionPatch), asCALL_CDECL},
    {"string GetErrorString(int errorCode)", "string GetErrorString(int errorCode)", asFUNCTION(BMLAS_GetErrorString), asCALL_CDECL},
    {"bool IsInitialized()", "bool IsInitialized()", asFUNCTION(BMLAS_IsInitialized), asCALL_CDECL},
    {"bool IsIngame()", "bool IsIngame()", asFUNCTION(BMLAS_IsIngame), asCALL_CDECL},
    {"bool IsInLevel()", "bool IsInLevel()", asFUNCTION(BMLAS_IsInLevel), asCALL_CDECL},
    {"bool IsPaused()", "bool IsPaused()", asFUNCTION(BMLAS_IsPaused), asCALL_CDECL},
    {"bool IsPlaying()", "bool IsPlaying()", asFUNCTION(BMLAS_IsPlaying), asCALL_CDECL},
    {"bool IsCheatEnabled()", "bool IsCheatEnabled()", asFUNCTION(BMLAS_IsCheatEnabled), asCALL_CDECL},
    {"void EnableCheat(bool enable)", "void EnableCheat(bool enable)", asFUNCTION(BMLAS_EnableCheat), asCALL_CDECL},
    {"void SendIngameMessage(const string &in message)", "void SendIngameMessage(const string &in message)", asFUNCTION(BMLAS_SendIngameMessage), asCALL_CDECL},
    {"void ExecuteCommand(const string &in command)", "void ExecuteCommand(const string &in command)", asFUNCTION(BMLAS_ExecuteCommand), asCALL_CDECL},
    {"void ExitGame()", "void ExitGame()", asFUNCTION(BMLAS_ExitGame), asCALL_CDECL},
    {"void OpenModsMenu()", "void OpenModsMenu()", asFUNCTION(BMLAS_OpenModsMenu), asCALL_CDECL},
    {"void CloseModsMenu()", "void CloseModsMenu()", asFUNCTION(BMLAS_CloseModsMenu), asCALL_CDECL},
    {"void OpenMapMenu()", "void OpenMapMenu()", asFUNCTION(BMLAS_OpenMapMenu), asCALL_CDECL},
    {"void CloseMapMenu()", "void CloseMapMenu()", asFUNCTION(BMLAS_CloseMapMenu), asCALL_CDECL},
    {"void ClearIngameMessages()", "void ClearIngameMessages()", asFUNCTION(BMLAS_ClearIngameMessages), asCALL_CDECL},
    {"float GetSRScore()", "float GetSRScore()", asFUNCTION(BMLAS_GetSRScore), asCALL_CDECL},
    {"int GetHSScore()", "int GetHSScore()", asFUNCTION(BMLAS_GetHSScore), asCALL_CDECL},
    {"int GetHUD()", "int GetHUD()", asFUNCTION(BMLAS_GetHUD), asCALL_CDECL},
    {"void SetHUD(int mode)", "void SetHUD(int mode)", asFUNCTION(BMLAS_SetHUD), asCALL_CDECL},
    {"void ShowTitle(bool show)", "void ShowTitle(bool show)", asFUNCTION(BMLAS_ShowTitle), asCALL_CDECL},
    {"void ShowFPS(bool show)", "void ShowFPS(bool show)", asFUNCTION(BMLAS_ShowFPS), asCALL_CDECL},
    {"void ShowSRTimer(bool show)", "void ShowSRTimer(bool show)", asFUNCTION(BMLAS_ShowSRTimer), asCALL_CDECL},
    {"void StartSRTimer()", "void StartSRTimer()", asFUNCTION(BMLAS_StartSRTimer), asCALL_CDECL},
    {"void PauseSRTimer()", "void PauseSRTimer()", asFUNCTION(BMLAS_PauseSRTimer), asCALL_CDECL},
    {"void ResetSRTimer()", "void ResetSRTimer()", asFUNCTION(BMLAS_ResetSRTimer), asCALL_CDECL},
    {"float GetSRTime()", "float GetSRTime()", asFUNCTION(BMLAS_GetSRTime), asCALL_CDECL},
    {"void SkipRenderForNextTick()", "void SkipRenderForNextTick()", asFUNCTION(BMLAS_SkipRenderForNextTick), asCALL_CDECL},
    {"CKContext@ GetCKContext()", "CKContext@ GetCKContext()", asFUNCTION(BMLAS_GetCKContext), asCALL_CDECL},
    {"CKRenderContext@ GetRenderContext()", "CKRenderContext@ GetRenderContext()", asFUNCTION(BMLAS_GetRenderContext), asCALL_CDECL},
    {"CKAttributeManager@ GetAttributeManager()", "CKAttributeManager@ GetAttributeManager()", asFUNCTION(BMLAS_GetAttributeManager), asCALL_CDECL},
    {"CKBehaviorManager@ GetBehaviorManager()", "CKBehaviorManager@ GetBehaviorManager()", asFUNCTION(BMLAS_GetBehaviorManager), asCALL_CDECL},
    {"CKCollisionManager@ GetCollisionManager()", "CKCollisionManager@ GetCollisionManager()", asFUNCTION(BMLAS_GetCollisionManager), asCALL_CDECL},
    {"InputHook@ GetInputManager()", "InputHook@ GetInputManager()", asFUNCTION(BMLAS_GetInputHook), asCALL_CDECL},
    {"CKMessageManager@ GetMessageManager()", "CKMessageManager@ GetMessageManager()", asFUNCTION(BMLAS_GetMessageManager), asCALL_CDECL},
    {"CKPathManager@ GetPathManager()", "CKPathManager@ GetPathManager()", asFUNCTION(BMLAS_GetPathManager), asCALL_CDECL},
    {"CKParameterManager@ GetParameterManager()", "CKParameterManager@ GetParameterManager()", asFUNCTION(BMLAS_GetParameterManager), asCALL_CDECL},
    {"CKRenderManager@ GetRenderManager()", "CKRenderManager@ GetRenderManager()", asFUNCTION(BMLAS_GetRenderManager), asCALL_CDECL},
    {"CKSoundManager@ GetSoundManager()", "CKSoundManager@ GetSoundManager()", asFUNCTION(BMLAS_GetSoundManager), asCALL_CDECL},
    {"CKTimeManager@ GetTimeManager()", "CKTimeManager@ GetTimeManager()", asFUNCTION(BMLAS_GetTimeManager), asCALL_CDECL},
    {"float GetTimeMs()", "float GetTimeMs()", asFUNCTION(BMLAS_GetTimeMs), asCALL_CDECL},
    {"float GetAbsoluteTimeMs()", "float GetAbsoluteTimeMs()", asFUNCTION(BMLAS_GetAbsoluteTimeMs), asCALL_CDECL},
    {"float GetDeltaTimeMs()", "float GetDeltaTimeMs()", asFUNCTION(BMLAS_GetDeltaTimeMs), asCALL_CDECL},
    {"uint GetFrameCount()", "uint GetFrameCount()", asFUNCTION(BMLAS_GetFrameCount), asCALL_CDECL},
    {"string GetDirectoryUtf8(DirectoryType type)", "string GetDirectoryUtf8(DirectoryType type)", asFUNCTION(BMLAS_GetDirectoryUtf8), asCALL_CDECL},
    {"bool IsKeyboardAttached()", "bool IsKeyboardAttached()", asFUNCTION(BMLAS_IsKeyboardAttached), asCALL_CDECL},
    {"bool IsMouseAttached()", "bool IsMouseAttached()", asFUNCTION(BMLAS_IsMouseAttached), asCALL_CDECL},
    {"bool IsKeyDown(CKKEYBOARD key)", "bool IsKeyDown(CKKEYBOARD key)", asFUNCTION(BMLAS_IsKeyDown), asCALL_CDECL},
    {"bool IsKeyUp(CKKEYBOARD key)", "bool IsKeyUp(CKKEYBOARD key)", asFUNCTION(BMLAS_IsKeyUp), asCALL_CDECL},
    {"bool IsKeyPressed(CKKEYBOARD key)", "bool IsKeyPressed(CKKEYBOARD key)", asFUNCTION(BMLAS_IsKeyPressed), asCALL_CDECL},
    {"bool IsKeyReleased(CKKEYBOARD key)", "bool IsKeyReleased(CKKEYBOARD key)", asFUNCTION(BMLAS_IsKeyReleased), asCALL_CDECL},
    {"bool IsKeyToggled(CKKEYBOARD key)", "bool IsKeyToggled(CKKEYBOARD key)", asFUNCTION(BMLAS_IsKeyToggled), asCALL_CDECL},
    {"string GetKeyName(CKKEYBOARD key)", "string GetKeyName(CKKEYBOARD key)", asFUNCTION(BMLAS_GetKeyName), asCALL_CDECL},
    {"int GetKeyFromName(const string &in name)", "int GetKeyFromName(const string &in name)", asFUNCTION(BMLAS_GetKeyFromName), asCALL_CDECL},
    {"bool IsMouseButtonDown(CK_MOUSEBUTTON button)", "bool IsMouseButtonDown(CK_MOUSEBUTTON button)", asFUNCTION(BMLAS_IsMouseButtonDown), asCALL_CDECL},
    {"bool IsMouseClicked(CK_MOUSEBUTTON button)", "bool IsMouseClicked(CK_MOUSEBUTTON button)", asFUNCTION(BMLAS_IsMouseClicked), asCALL_CDECL},
    {"bool IsMouseToggled(CK_MOUSEBUTTON button)", "bool IsMouseToggled(CK_MOUSEBUTTON button)", asFUNCTION(BMLAS_IsMouseToggled), asCALL_CDECL},
    {"Vx2DVector GetMousePosition(bool absolute = true)", "Vx2DVector GetMousePosition(bool absolute = true)", asFUNCTION(BMLAS_GetMousePosition), asCALL_CDECL},
    {"Vx2DVector GetLastMousePosition()", "Vx2DVector GetLastMousePosition()", asFUNCTION(BMLAS_GetLastMousePosition), asCALL_CDECL},
    {"VxVector GetMouseRelativePosition()", "VxVector GetMouseRelativePosition()", asFUNCTION(BMLAS_GetMouseRelativePosition), asCALL_CDECL},
    {"bool IsObjectValid(CKObject@ object)", "bool IsObjectValid(CKObject@ object)", asFUNCTION(BMLAS_IsObjectValid), asCALL_CDECL},
    {"int GetObjectId(CKObject@ object)", "int GetObjectId(CKObject@ object)", asFUNCTION(BMLAS_GetObjectId), asCALL_CDECL},
    {"string GetObjectName(CKObject@ object)", "string GetObjectName(CKObject@ object)", asFUNCTION(BMLAS_GetObjectName), asCALL_CDECL},
    {"int GetObjectClassId(CKObject@ object)", "int GetObjectClassId(CKObject@ object)", asFUNCTION(BMLAS_GetObjectClassId), asCALL_CDECL},
    {"bool IsObjectVisible(CKObject@ object)", "bool IsObjectVisible(CKObject@ object)", asFUNCTION(BMLAS_IsObjectVisible), asCALL_CDECL},
    {"bool IsObjectDynamic(CKObject@ object)", "bool IsObjectDynamic(CKObject@ object)", asFUNCTION(BMLAS_IsObjectDynamic), asCALL_CDECL},
    {"int GetBeObjectPriority(CKBeObject@ object)", "int GetBeObjectPriority(CKBeObject@ object)", asFUNCTION(BMLAS_GetBeObjectPriority), asCALL_CDECL},
    {"int GetBeObjectScriptCount(CKBeObject@ object)", "int GetBeObjectScriptCount(CKBeObject@ object)", asFUNCTION(BMLAS_GetBeObjectScriptCount), asCALL_CDECL},
    {"int GetBeObjectAttributeCount(CKBeObject@ object)", "int GetBeObjectAttributeCount(CKBeObject@ object)", asFUNCTION(BMLAS_GetBeObjectAttributeCount), asCALL_CDECL},
    {"VxVector Get3dEntityPosition(CK3dEntity@ entity)", "VxVector Get3dEntityPosition(CK3dEntity@ entity)", asFUNCTION(BMLAS_Get3dEntityPosition), asCALL_CDECL},
    {"VxVector Get3dEntityScale(CK3dEntity@ entity, bool local = true)", "VxVector Get3dEntityScale(CK3dEntity@ entity, bool local = true)", asFUNCTION(BMLAS_Get3dEntityScale), asCALL_CDECL},
    {"int Get3dEntityChildCount(CK3dEntity@ entity)", "int Get3dEntityChildCount(CK3dEntity@ entity)", asFUNCTION(BMLAS_Get3dEntityChildCount), asCALL_CDECL},
    {"CK3dEntity@ Get3dEntityParent(CK3dEntity@ entity)", "CK3dEntity@ Get3dEntityParent(CK3dEntity@ entity)", asFUNCTION(BMLAS_Get3dEntityParent), asCALL_CDECL},
    {"CKDataArray@ GetArrayByName(const string &in name)", "CKDataArray@ GetArrayByName(const string &in name)", asFUNCTION(BMLAS_GetArrayByName), asCALL_CDECL},
    {"CKGroup@ GetGroupByName(const string &in name)", "CKGroup@ GetGroupByName(const string &in name)", asFUNCTION(BMLAS_GetGroupByName), asCALL_CDECL},
    {"CKMaterial@ GetMaterialByName(const string &in name)", "CKMaterial@ GetMaterialByName(const string &in name)", asFUNCTION(BMLAS_GetMaterialByName), asCALL_CDECL},
    {"CKMesh@ GetMeshByName(const string &in name)", "CKMesh@ GetMeshByName(const string &in name)", asFUNCTION(BMLAS_GetMeshByName), asCALL_CDECL},
    {"CK2dEntity@ Get2dEntityByName(const string &in name)", "CK2dEntity@ Get2dEntityByName(const string &in name)", asFUNCTION(BMLAS_Get2dEntityByName), asCALL_CDECL},
    {"CK3dEntity@ Get3dEntityByName(const string &in name)", "CK3dEntity@ Get3dEntityByName(const string &in name)", asFUNCTION(BMLAS_Get3dEntityByName), asCALL_CDECL},
    {"CK3dObject@ Get3dObjectByName(const string &in name)", "CK3dObject@ Get3dObjectByName(const string &in name)", asFUNCTION(BMLAS_Get3dObjectByName), asCALL_CDECL},
    {"CKCamera@ GetCameraByName(const string &in name)", "CKCamera@ GetCameraByName(const string &in name)", asFUNCTION(BMLAS_GetCameraByName), asCALL_CDECL},
    {"CKTargetCamera@ GetTargetCameraByName(const string &in name)", "CKTargetCamera@ GetTargetCameraByName(const string &in name)", asFUNCTION(BMLAS_GetTargetCameraByName), asCALL_CDECL},
    {"CKLight@ GetLightByName(const string &in name)", "CKLight@ GetLightByName(const string &in name)", asFUNCTION(BMLAS_GetLightByName), asCALL_CDECL},
    {"CKTargetLight@ GetTargetLightByName(const string &in name)", "CKTargetLight@ GetTargetLightByName(const string &in name)", asFUNCTION(BMLAS_GetTargetLightByName), asCALL_CDECL},
    {"CKSound@ GetSoundByName(const string &in name)", "CKSound@ GetSoundByName(const string &in name)", asFUNCTION(BMLAS_GetSoundByName), asCALL_CDECL},
    {"CKTexture@ GetTextureByName(const string &in name)", "CKTexture@ GetTextureByName(const string &in name)", asFUNCTION(BMLAS_GetTextureByName), asCALL_CDECL},
    {"CKBehavior@ GetScriptByName(const string &in name)", "CKBehavior@ GetScriptByName(const string &in name)", asFUNCTION(BMLAS_GetScriptByName), asCALL_CDECL},
    {"void SetIC(CKBeObject@ object, bool hierarchy = false)", "void SetIC(CKBeObject@ object, bool hierarchy = false)", asFUNCTION(BMLAS_SetIC), asCALL_CDECL},
    {"void RestoreIC(CKBeObject@ object, bool hierarchy = false)", "void RestoreIC(CKBeObject@ object, bool hierarchy = false)", asFUNCTION(BMLAS_RestoreIC), asCALL_CDECL},
    {"void Show(CKBeObject@ object, CK_OBJECT_SHOWOPTION show = CKSHOW, bool hierarchy = false)", "void Show(CKBeObject@ object, CK_OBJECT_SHOWOPTION show = CKSHOW, bool hierarchy = false)", asFUNCTION(BMLAS_Show), asCALL_CDECL},
    {"TimerRef@ AddTimer(Timer@+ timer)", "TimerRef@ AddTimer(Timer@+ timer)", asFUNCTION(BMLAS_AddTimer), asCALL_CDECL},
    {"CommandRef@ RegisterCommand(Command@+ command)", "CommandRef@ RegisterCommand(Command@+ command)", asFUNCTION(BMLAS_RegisterCommand), asCALL_CDECL},
    {"bool UnregisterCommand(const string &in name)", "bool UnregisterCommand(const string &in name)", asFUNCTION(BMLAS_UnregisterCommand), asCALL_CDECL},
    {"bool RegisterBallType(const string &in ballFile, const string &in ballId, const string &in ballName, const string &in objName, float friction, float elasticity, float mass, const string &in collGroup, float linearDamp, float rotDamp, float force, float radius)", "bool RegisterBallType(...)", asFUNCTION(BMLAS_RegisterBallType), asCALL_CDECL},
    {"bool RegisterFloorType(const string &in floorName, float friction, float elasticity, float mass, const string &in collGroup, bool enableColl)", "bool RegisterFloorType(...)", asFUNCTION(BMLAS_RegisterFloorType), asCALL_CDECL},
    {"bool RegisterModulBall(const string &in modulName, bool fixed, float friction, float elasticity, float mass, const string &in collGroup, bool frozen, bool enableColl, bool calcMassCenter, float linearDamp, float rotDamp, float radius)", "bool RegisterModulBall(...)", asFUNCTION(BMLAS_RegisterModulBall), asCALL_CDECL},
    {"bool RegisterModulConvex(const string &in modulName, bool fixed, float friction, float elasticity, float mass, const string &in collGroup, bool frozen, bool enableColl, bool calcMassCenter, float linearDamp, float rotDamp)", "bool RegisterModulConvex(...)", asFUNCTION(BMLAS_RegisterModulConvex), asCALL_CDECL},
    {"bool RegisterTrafo(const string &in modulName)", "bool RegisterTrafo(const string &in modulName)", asFUNCTION(BMLAS_RegisterTrafo), asCALL_CDECL},
    {"bool RegisterModul(const string &in modulName)", "bool RegisterModul(const string &in modulName)", asFUNCTION(BMLAS_RegisterModul), asCALL_CDECL},
    {"bool FileExistsUtf8(const string &in path)", "bool FileExistsUtf8(const string &in path)", asFUNCTION(BMLAS_FileExistsUtf8), asCALL_CDECL},
    {"bool DirectoryExistsUtf8(const string &in path)", "bool DirectoryExistsUtf8(const string &in path)", asFUNCTION(BMLAS_DirectoryExistsUtf8), asCALL_CDECL},
    {"bool PathExistsUtf8(const string &in path)", "bool PathExistsUtf8(const string &in path)", asFUNCTION(BMLAS_PathExistsUtf8), asCALL_CDECL},
    {"bool IsPathValidUtf8(const string &in path)", "bool IsPathValidUtf8(const string &in path)", asFUNCTION(BMLAS_IsPathValidUtf8), asCALL_CDECL},
    {"bool IsAbsolutePathUtf8(const string &in path)", "bool IsAbsolutePathUtf8(const string &in path)", asFUNCTION(BMLAS_IsAbsolutePathUtf8), asCALL_CDECL},
    {"bool IsRelativePathUtf8(const string &in path)", "bool IsRelativePathUtf8(const string &in path)", asFUNCTION(BMLAS_IsRelativePathUtf8), asCALL_CDECL},
    {"string CombinePathUtf8(const string &in left, const string &in right)", "string CombinePathUtf8(const string &in left, const string &in right)", asFUNCTION(BMLAS_CombinePathUtf8), asCALL_CDECL},
    {"string NormalizePathUtf8(const string &in path)", "string NormalizePathUtf8(const string &in path)", asFUNCTION(BMLAS_NormalizePathUtf8), asCALL_CDECL},
    {"string GetFileNameUtf8(const string &in path)", "string GetFileNameUtf8(const string &in path)", asFUNCTION(BMLAS_GetFileNameUtf8), asCALL_CDECL},
    {"string GetExtensionUtf8(const string &in path)", "string GetExtensionUtf8(const string &in path)", asFUNCTION(BMLAS_GetExtensionUtf8), asCALL_CDECL},
    {"string RemoveExtensionUtf8(const string &in path)", "string RemoveExtensionUtf8(const string &in path)", asFUNCTION(BMLAS_RemoveExtensionUtf8), asCALL_CDECL},
    {"string ReadTextFileUtf8(const string &in path)", "string ReadTextFileUtf8(const string &in path)", asFUNCTION(BMLAS_ReadTextFileUtf8), asCALL_CDECL},
    {"bool DataShareSetString(const string &in key, const string &in value, const string &in name = \"BML\")", "bool DataShareSetString(const string &in key, const string &in value, const string &in name = \"BML\")", asFUNCTION(BMLAS_DataShareSetString), asCALL_CDECL},
    {"string DataShareGetString(const string &in key, const string &in defaultValue = \"\", const string &in name = \"BML\")", "string DataShareGetString(const string &in key, const string &in defaultValue = \"\", const string &in name = \"BML\")", asFUNCTION(BMLAS_DataShareGetString), asCALL_CDECL},
    {"bool DataShareSetBool(const string &in key, bool value, const string &in name = \"BML\")", "bool DataShareSetBool(const string &in key, bool value, const string &in name = \"BML\")", asFUNCTION(BMLAS_DataShareSetBool), asCALL_CDECL},
    {"bool DataShareGetBool(const string &in key, bool defaultValue = false, const string &in name = \"BML\")", "bool DataShareGetBool(const string &in key, bool defaultValue = false, const string &in name = \"BML\")", asFUNCTION(BMLAS_DataShareGetBool), asCALL_CDECL},
    {"bool DataShareSetInt(const string &in key, int value, const string &in name = \"BML\")", "bool DataShareSetInt(const string &in key, int value, const string &in name = \"BML\")", asFUNCTION(BMLAS_DataShareSetInt), asCALL_CDECL},
    {"int DataShareGetInt(const string &in key, int defaultValue = 0, const string &in name = \"BML\")", "int DataShareGetInt(const string &in key, int defaultValue = 0, const string &in name = \"BML\")", asFUNCTION(BMLAS_DataShareGetInt), asCALL_CDECL},
    {"bool DataShareSetFloat(const string &in key, float value, const string &in name = \"BML\")", "bool DataShareSetFloat(const string &in key, float value, const string &in name = \"BML\")", asFUNCTION(BMLAS_DataShareSetFloat), asCALL_CDECL},
    {"float DataShareGetFloat(const string &in key, float defaultValue = 0.0f, const string &in name = \"BML\")", "float DataShareGetFloat(const string &in key, float defaultValue = 0.0f, const string &in name = \"BML\")", asFUNCTION(BMLAS_DataShareGetFloat), asCALL_CDECL},
    {"bool DataShareHas(const string &in key, const string &in name = \"BML\")", "bool DataShareHas(const string &in key, const string &in name = \"BML\")", asFUNCTION(BMLAS_DataShareHas), asCALL_CDECL},
    {"void DataShareRemove(const string &in key, const string &in name = \"BML\")", "void DataShareRemove(const string &in key, const string &in name = \"BML\")", asFUNCTION(BMLAS_DataShareRemove), asCALL_CDECL},
    {"int DataShareSizeOf(const string &in key, const string &in name = \"BML\")", "int DataShareSizeOf(const string &in key, const string &in name = \"BML\")", asFUNCTION(BMLAS_DataShareSizeOf), asCALL_CDECL},
    {"DataShareRequestRef@ RequestDataShare(DataShareRequest@+ request)", "DataShareRequestRef@ RequestDataShare(DataShareRequest@+ request)", asFUNCTION(BMLAS_RequestDataShare), asCALL_CDECL},
};

bool CheckScriptApiContractRegistrationSurface() {
    for (const BML::ScriptCallbackContract &callback : BML::ScriptApiContract::Callbacks()) {
        if (!CheckRegistrationSurfaceText("callback name", nullptr, callback.Name) ||
            !CheckRegistrationSurfaceText("callback declaration", callback.Name, callback.Declaration) ||
            !CheckRegistrationSurfaceText("callback diagnostic", callback.Name, callback.FailurePrefix)) {
            return false;
        }
    }

    for (const BML::ScriptTypedefContract &type : BML::ScriptApiContract::Typedefs()) {
        if (!CheckRegistrationSurfaceText("typedef name", nullptr, type.Name) ||
            !CheckRegistrationSurfaceText("typedef target", type.Name, type.TargetType) ||
            !CheckRegistrationSurfaceText("typedef declaration", type.Name, type.Declaration)) {
            return false;
        }
    }

    for (const BML::ScriptIntegerConstantContract &constant : BML::ScriptApiContract::GameEventConstants()) {
        if (!CheckRegistrationSurfaceText("integer constant", nullptr, constant.Declaration))
            return false;
    }

    for (const BML::ScriptEnumContract &enumInfo : BML::ScriptApiContract::Enums()) {
        if (!CheckRegistrationSurfaceText("enum name", nullptr, enumInfo.Name) ||
            !CheckRegistrationSurfaceText("enum declaration", enumInfo.Name, enumInfo.Declaration)) {
            return false;
        }
        for (const BML::ScriptEnumValueContract &value : BML::ScriptContractSpan<BML::ScriptEnumValueContract>{enumInfo.Values, enumInfo.ValueCount}) {
            if (!CheckRegistrationSurfaceText("enum value", enumInfo.Name, value.Name) ||
                !CheckRegistrationSurfaceText("enum value diagnostic", enumInfo.Name, value.DiagnosticName)) {
                return false;
            }
        }
    }

    for (const BML::ScriptEventTypeContract &eventType : BML::ScriptApiContract::EventTypes()) {
        if (!CheckRegistrationSurfaceText("event type name", nullptr, eventType.Name) ||
            !CheckRegistrationSurfaceText("event type declaration", eventType.Name, eventType.Declaration)) {
            return false;
        }
        for (const BML::ScriptEventMemberContract &member : BML::ScriptContractSpan<BML::ScriptEventMemberContract>{eventType.Members, eventType.MemberCount}) {
            if (!CheckRegistrationSurfaceText("event member declaration", eventType.Name, member.Declaration) ||
                !CheckRegistrationSurfaceText("event member diagnostic", eventType.Name, member.DiagnosticName)) {
                return false;
            }
        }
    }

    return true;
}

bool CheckScriptFacadeRegistrationSurface() {
    if (!CheckScriptApiContractRegistrationSurface())
        return false;

    for (const ScriptObjectTypeRegistration &registration : kObjectTypeRegistrations) {
        if (!CheckRegistrationSurfaceText("object type name", nullptr, registration.Name) ||
            !CheckRegistrationSurfaceText("object type declaration", registration.Name, registration.Declaration)) {
            return false;
        }
    }

    for (const ScriptInterfaceRegistration &registration : kInterfaceRegistrations) {
        if (!CheckRegistrationSurfaceText("interface name", nullptr, registration.Name) ||
            !CheckRegistrationSurfaceText("interface declaration", registration.Name, registration.DiagnosticName)) {
            return false;
        }
    }

    for (const ScriptInterfaceMethodRegistration &registration : kInterfaceMethodRegistrations) {
        if (!CheckRegistrationSurfaceText("interface method interface", registration.DiagnosticName, registration.InterfaceName) ||
            !CheckRegistrationSurfaceText("interface method declaration", registration.InterfaceName, registration.Declaration) ||
            !CheckRegistrationSurfaceText("interface method diagnostic", registration.InterfaceName, registration.DiagnosticName)) {
            return false;
        }
    }

    for (const ScriptObjectBehaviourRegistration &registration : kObjectBehaviourRegistrations) {
        if (!CheckRegistrationSurfaceText("object behaviour type", registration.DiagnosticName, registration.TypeName) ||
            !CheckRegistrationSurfaceText("object behaviour declaration", registration.TypeName, registration.Declaration) ||
            !CheckRegistrationSurfaceText("object behaviour diagnostic", registration.TypeName, registration.DiagnosticName)) {
            return false;
        }
    }

    for (const ScriptObjectMethodRegistration &registration : kObjectMethodRegistrations) {
        if (!CheckRegistrationSurfaceText("object method type", registration.DiagnosticName, registration.TypeName) ||
            !CheckRegistrationSurfaceText("object method declaration", registration.TypeName, registration.Declaration) ||
            !CheckRegistrationSurfaceText("object method diagnostic", registration.TypeName, registration.DiagnosticName)) {
            return false;
        }
    }

    for (const ScriptGlobalFunctionRegistration &registration : kGlobalFunctionRegistrations) {
        if (!CheckRegistrationSurfaceText("global function declaration", nullptr, registration.Declaration) ||
            !CheckRegistrationSurfaceText("global function diagnostic", nullptr, registration.DiagnosticName)) {
            return false;
        }
    }

    for (const ScriptUiEnumValueRegistration &registration : kUiButtonTypeRegistrations) {
        if (!CheckRegistrationSurfaceText("UI enum value", "BML::UI::ButtonType", registration.Name) ||
            !CheckRegistrationSurfaceText("UI enum diagnostic", "BML::UI::ButtonType", registration.DiagnosticName)) {
            return false;
        }
    }

    for (const ScriptUiFunctionRegistration &registration : kUiFunctionRegistrations) {
        if (!CheckRegistrationSurfaceText("UI function declaration", nullptr, registration.Declaration) ||
            !CheckRegistrationSurfaceText("UI function diagnostic", nullptr, registration.DiagnosticName)) {
            return false;
        }
    }

    return true;
}

int RegisterScriptEnumsAndConstants(asIScriptEngine *engine, const char **errorMessage) {
    for (const BML::ScriptTypedefContract &type : BML::ScriptApiContract::Typedefs()) {
        BML_AS_REGISTER(engine->RegisterTypedef(type.Name, type.TargetType), type.Declaration);
    }
    for (const BML::ScriptIntegerConstantContract &constant : BML::ScriptApiContract::GameEventConstants()) {
        BML_AS_REGISTER(engine->RegisterGlobalProperty(constant.Declaration, const_cast<int *>(&constant.Value)), constant.Declaration);
    }
    for (const BML::ScriptIntegerConstantContract &constant : BML::ScriptApiContract::ErrorConstants()) {
        BML_AS_REGISTER(engine->RegisterGlobalProperty(constant.Declaration, const_cast<int *>(&constant.Value)), constant.Declaration);
    }
    for (const BML::ScriptEnumContract &enumInfo : BML::ScriptApiContract::Enums()) {
        BML_AS_REGISTER(engine->RegisterEnum(enumInfo.Name), enumInfo.Declaration);
        for (const BML::ScriptEnumValueContract &value : BML::ScriptContractSpan<BML::ScriptEnumValueContract>{enumInfo.Values, enumInfo.ValueCount}) {
            BML_AS_REGISTER(engine->RegisterEnumValue(enumInfo.Name, value.Name, value.Value), value.DiagnosticName);
        }
    }
    return asSUCCESS;
}

int RegisterScriptObjectTypes(asIScriptEngine *engine, const char **errorMessage) {
    for (const ScriptObjectTypeRegistration &registration : kObjectTypeRegistrations) {
        BML_AS_REGISTER(engine->RegisterObjectType(registration.Name, registration.Size, registration.Flags), registration.Declaration);
    }
    return asSUCCESS;
}

int RegisterScriptInterfaces(asIScriptEngine *engine, const char **errorMessage) {
    for (const ScriptInterfaceRegistration &registration : kInterfaceRegistrations) {
        BML_AS_REGISTER(engine->RegisterInterface(registration.Name), registration.DiagnosticName);
    }
    for (const ScriptInterfaceMethodRegistration &registration : kInterfaceMethodRegistrations) {
        BML_AS_REGISTER(engine->RegisterInterfaceMethod(registration.InterfaceName, registration.Declaration), registration.DiagnosticName);
    }
    return asSUCCESS;
}

int RegisterScriptObjectBehaviours(asIScriptEngine *engine, const char **errorMessage) {
    for (const ScriptObjectBehaviourRegistration &registration : kObjectBehaviourRegistrations) {
        BML_AS_REGISTER(engine->RegisterObjectBehaviour(registration.TypeName, registration.Behaviour, registration.Declaration, registration.Function, registration.CallConvention), registration.DiagnosticName);
    }
    return asSUCCESS;
}

int RegisterScriptObjectMethods(asIScriptEngine *engine, const char **errorMessage) {
    for (const ScriptObjectMethodRegistration &registration : kObjectMethodRegistrations) {
        BML_AS_REGISTER(engine->RegisterObjectMethod(registration.TypeName, registration.Declaration, registration.Function, registration.CallConvention), registration.DiagnosticName);
    }
    return asSUCCESS;
}

int RegisterScriptGlobalFunctions(asIScriptEngine *engine, const char **errorMessage) {
    for (const ScriptGlobalFunctionRegistration &registration : kGlobalFunctionRegistrations) {
        BML_AS_REGISTER(engine->RegisterGlobalFunction(registration.Declaration, registration.Function, registration.CallConvention), registration.DiagnosticName);
    }
    return asSUCCESS;
}

int RegisterScriptUiFacade(asIScriptEngine *engine, const char **errorMessage) {
    BML_AS_REGISTER(engine->SetDefaultNamespace("BML::UI"), "namespace BML::UI");
    BML_AS_REGISTER(engine->RegisterEnum("ButtonType"), "enum BML::UI::ButtonType");
    for (const ScriptUiEnumValueRegistration &registration : kUiButtonTypeRegistrations) {
        BML_AS_REGISTER(engine->RegisterEnumValue("ButtonType", registration.Name, registration.Value), registration.DiagnosticName);
    }
    for (const ScriptUiFunctionRegistration &registration : kUiFunctionRegistrations) {
        BML_AS_REGISTER(engine->RegisterGlobalFunction(registration.Declaration, registration.Function, registration.CallConvention), registration.DiagnosticName);
    }
    BML_AS_REGISTER(engine->SetDefaultNamespace("BML"), "namespace BML");
    return asSUCCESS;
}

int RegisterScriptFacade(asIScriptEngine *engine, const char **errorMessage) {
    if (!CheckScriptFacadeRegistrationSurface()) {
        engine->SetDefaultNamespace("");
        if (errorMessage)
            *errorMessage = g_LastRegistrationError.c_str();
        return asERROR;
    }

    const int enumsResult = RegisterScriptEnumsAndConstants(engine, errorMessage);
    if (enumsResult < 0)
        return enumsResult;
    const int typesResult = RegisterScriptObjectTypes(engine, errorMessage);
    if (typesResult < 0)
        return typesResult;
    const int interfacesResult = RegisterScriptInterfaces(engine, errorMessage);
    if (interfacesResult < 0)
        return interfacesResult;
    const int behavioursResult = RegisterScriptObjectBehaviours(engine, errorMessage);
    if (behavioursResult < 0)
        return behavioursResult;
    const int methodsResult = RegisterScriptObjectMethods(engine, errorMessage);
    if (methodsResult < 0)
        return methodsResult;
    const int globalsResult = RegisterScriptGlobalFunctions(engine, errorMessage);
    if (globalsResult < 0)
        return globalsResult;
    return RegisterScriptUiFacade(engine, errorMessage);
}

int RegisterImGuiBindings(asIScriptEngine *engine, const char **errorMessage) {
    BML_AS_REGISTER(engine->SetDefaultNamespace(""), "SetDefaultNamespace(empty)");
    const int imguiResult = RegisterBMLImGuiAngelScriptBindings(engine, errorMessage);
    if (imguiResult < 0) {
        engine->SetDefaultNamespace("");
        if (errorMessage && *errorMessage) {
            g_LastRegistrationError = *errorMessage;
        } else {
            CheckRegistration(imguiResult, "RegisterBMLImGuiAngelScriptBindings(engine)");
            if (errorMessage)
                *errorMessage = g_LastRegistrationError.c_str();
        }
        return imguiResult;
    }

    return asSUCCESS;
}

int RegisterBMLAngelScript(asIScriptEngine *engine,
                           CKAngelScript *,
                           void *userData,
                           const char **errorMessage) {
    auto *context = static_cast<ModContext *>(userData);
    if (!engine || !context) {
        g_LastRegistrationError = "BML AngelScript registration received a null engine or context.";
        if (errorMessage)
            *errorMessage = g_LastRegistrationError.c_str();
        return asERROR;
    }

    context->SetAngelScriptBindingsRegistered(false);

    const char *requiredTypes[] = {
        "CKContext",
        "CKRenderContext",
        "CKAttributeManager",
        "CKBehaviorManager",
        "CKCollisionManager",
        "CKMessageManager",
        "CKPathManager",
        "CKParameterManager",
        "CKRenderManager",
        "CKSoundManager",
        "CKTimeManager",
        "CKObject",
        "CKDataArray",
        "CKGroup",
        "CKMaterial",
        "CKMesh",
        "CK2dEntity",
        "CK3dEntity",
        "CK3dObject",
        "CKCamera",
        "CKTargetCamera",
        "CKLight",
        "CKTargetLight",
        "CKSound",
        "CKTexture",
        "CKBehavior",
        "CKBeObject",
        "CKKEYBOARD",
        "CK_MOUSEBUTTON",
        "CK_OBJECT_SHOWOPTION",
        "Vx2DVector",
        "VxVector",
    };

    for (const char *typeName : requiredTypes) {
        if (!RequireType(engine, typeName, errorMessage))
            return asERROR;
    }

    BML_AS_REGISTER(engine->SetDefaultNamespace("BML"), "namespace BML");

    const int contractResult = RegisterScriptFacade(engine, errorMessage);
    if (contractResult < 0) {
        engine->SetDefaultNamespace("");
        return contractResult;
    }

    const int imguiResult = RegisterImGuiBindings(engine, errorMessage);
    if (imguiResult < 0) {
        engine->SetDefaultNamespace("");
        return imguiResult;
    }

    engine->SetDefaultNamespace("");
    context->SetAngelScriptBindingsRegistered(true);
    return asSUCCESS;
}

#undef BML_AS_REGISTER

bool BML_TryRegisterAngelScriptBindings(ModContext *context) {
    if (!context || !context->IsInited() || context->IsAngelScriptExtensionRegistered())
        return false;

    CKTimeManager *timeManager = context->GetTimeManager();
    const CKDWORD now = timeManager ? timeManager->GetMainTickCount() : 0;
    if (timeManager && g_NextRegistrationAttemptTick != 0 && now < g_NextRegistrationAttemptTick)
        return false;

    if (!g_AngelScriptHost.Refresh(context->GetCKContext())) {
        if (context->GetLogger() &&
            ShouldLogAngelScriptUnavailable(g_AngelScriptHost.GetState(), g_AngelScriptHost.GetDiagnostic())) {
            context->GetLogger()->Warn("BML AngelScript bindings unavailable: %s",
                                       g_AngelScriptHost.GetDiagnostic().c_str());
        }
        if (timeManager)
            g_NextRegistrationAttemptTick = now + kRegistrationRetryTicks;
        return false;
    }

    CKAngelScript *angelScript = g_AngelScriptHost.GetAngelScript();
    const CKAngelScriptAdapter::Api &api = g_AngelScriptHost.GetApi();

    CKAngelScriptEngineExtension extension = {};
    if (api.InitEngineExtension)
        api.InitEngineExtension(&extension);
    else
        extension.Size = sizeof(extension);
    extension.Name = kExtensionName;
    extension.Register = RegisterBMLAngelScript;
    extension.UserData = context;
    extension.Flags = CKAS_ENGINEEXTENSION_DEFAULT;

    CKAngelScriptResult result = {};
    if (api.InitResult)
        api.InitResult(&result);
    else
        result.Size = sizeof(result);
    const CKAS_STATUS status = api.RegisterEngineExtension(angelScript, &extension, &result);
    if (status == CKAS_OK) {
        ResetAngelScriptUnavailableLog();
        g_NextRegistrationAttemptTick = 0;
        context->SetAngelScriptExtensionRegistered(true);
        context->SetAngelScriptBindingsRegistered(true);
        if (context->GetLogger())
            context->GetLogger()->Info("Registered BML AngelScript bindings");
        return true;
    }

    if (status == CKAS_EXECUTIONFAILED) {
        CKAngelScriptResult unregisterResult = {};
        if (api.InitResult)
            api.InitResult(&unregisterResult);
        else
            unregisterResult.Size = sizeof(unregisterResult);
        api.UnregisterEngineExtension(angelScript, kExtensionName, &unregisterResult);
        context->SetAngelScriptExtensionRegistered(false);
        context->SetAngelScriptBindingsRegistered(false);
        if (context->GetLogger()) {
            const char *message = !g_LastRegistrationError.empty()
                                      ? g_LastRegistrationError.c_str()
                                      : result.ErrorMessage;
            context->GetLogger()->Warn("Failed to register BML AngelScript declarations: code=%d %s",
                                       result.AngelScriptCode,
                                       message ? message : "");
        }
        if (timeManager)
            g_NextRegistrationAttemptTick = now + kRegistrationRetryTicks;
        return false;
    }

    if (context->GetLogger()) {
        context->GetLogger()->Warn("Failed to register BML AngelScript bindings: %s",
                                   CKAngelScriptAdapter::FormatResult(status, result).c_str());
    }
    if (timeManager)
        g_NextRegistrationAttemptTick = now + kRegistrationRetryTicks;
    return false;
}

void BML_UnregisterAngelScriptBindings(ModContext *context) {
    if (!context || !context->IsAngelScriptExtensionRegistered())
        return;

    if (g_AngelScriptHost.Refresh(context->GetCKContext())) {
        CKAngelScriptResult result = {};
        const CKAngelScriptAdapter::Api &api = g_AngelScriptHost.GetApi();
        if (api.InitResult)
            api.InitResult(&result);
        else
            result.Size = sizeof(result);
        api.UnregisterEngineExtension(g_AngelScriptHost.GetAngelScript(), kExtensionName, &result);
    }

    context->SetAngelScriptExtensionRegistered(false);
    context->SetAngelScriptBindingsRegistered(false);
    g_NextRegistrationAttemptTick = 0;
}

#else

bool BML_TryRegisterAngelScriptBindings(ModContext *) {
    return false;
}

void BML_UnregisterAngelScriptBindings(ModContext *) {}

#endif

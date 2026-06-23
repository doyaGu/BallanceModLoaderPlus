#include "AngelScriptBindings.h"

#include "ModContext.h"

#include <string>

#include "BML/BML.h"
#include "BML/Bui.h"
#include "BML/Core.h"
#include "BML/DataShare.h"
#include "BML/IConfig.h"
#include "BML/InputHook.h"
#include "BML/Interop.h"
#include "BML/ILogger.h"
#include "CallFrameInternal.h"

#if BML_ENABLE_ANGELSCRIPT

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <new>
#include <vector>
#include <utility>

#include <angelscript.h>

#include "AngelScript/generated/BMLImGuiAngelScriptBindings.h"
#include "AngelScriptImGuiBindings.h"
#include "BML/ExecuteBB.h"
#include "BMLMod.h"
#include "CKAngelScriptAdapter.h"
#include "ScriptApiContract.h"
#include "ScriptCallbackEvents.h"
#include "ScriptFacadeAccess.h"
#include "ScriptHookBlockService.h"
#include "ScriptMod.h"
#include "ScriptModContextView.h"
#include "ScriptModRuntime.h"
#include "ScriptStateBag.h"
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

static bool RejectScriptObjectConstructionHostCall(const char *apiName) {
    return BML::ScriptModRuntime::RecordConstructionHostCallViolation(apiName);
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

static BML::ScriptStateBag *BMLAS_CreateStateBag() {
    return new (std::nothrow) BML::ScriptStateBag();
}

std::string BMLAS_GetGameEventName(int event) {
    return BML::GetScriptGameEventName(event);
}

bool BMLAS_IsInitialized() { return GetActiveContext() != nullptr; }
bool BMLAS_IsIngame() { ModContext *ctx = nullptr; return RequireContext(ctx) && ctx->IsIngame(); }
bool BMLAS_IsInLevel() { ModContext *ctx = nullptr; return RequireContext(ctx) && ctx->IsInLevel(); }
bool BMLAS_IsPaused() { ModContext *ctx = nullptr; return RequireContext(ctx) && ctx->IsPaused(); }
bool BMLAS_IsPlaying() { ModContext *ctx = nullptr; return RequireContext(ctx) && ctx->IsPlaying(); }
bool BMLAS_IsCheatEnabled() { ModContext *ctx = nullptr; return RequireContext(ctx) && ctx->IsCheatEnabled(); }

void BMLAS_EnableCheat(bool enable) {
    if (RejectScriptObjectConstructionHostCall("BML::EnableCheat"))
        return;
    ModContext *ctx = nullptr;
    if (RequireContext(ctx))
        ctx->EnableCheat(enable);
}

void BMLAS_SendIngameMessage(const std::string &message) {
    if (RejectScriptObjectConstructionHostCall("BML::SendIngameMessage"))
        return;
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->SendIngameMessage(message.c_str());
}

void BMLAS_ExecuteCommand(const std::string &command) {
    if (RejectScriptObjectConstructionHostCall("BML::ExecuteCommand"))
        return;
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->ExecuteCommand(command.c_str());
}

void BMLAS_OpenModsMenu() {
    if (RejectScriptObjectConstructionHostCall("BML::OpenModsMenu"))
        return;
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->OpenModsMenu();
}

void BMLAS_CloseModsMenu() {
    if (RejectScriptObjectConstructionHostCall("BML::CloseModsMenu"))
        return;
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->CloseModsMenu();
}

void BMLAS_OpenMapMenu() {
    if (RejectScriptObjectConstructionHostCall("BML::OpenMapMenu"))
        return;
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->OpenMapMenu();
}

void BMLAS_CloseMapMenu() {
    if (RejectScriptObjectConstructionHostCall("BML::CloseMapMenu"))
        return;
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->CloseMapMenu();
}

void BMLAS_ClearIngameMessages() {
    if (RejectScriptObjectConstructionHostCall("BML::ClearIngameMessages"))
        return;
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
    if (RejectScriptObjectConstructionHostCall("BML::SetHUD"))
        return;
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->SetHUD(mode);
}

void BMLAS_ShowTitle(bool show) {
    if (RejectScriptObjectConstructionHostCall("BML::ShowTitle"))
        return;
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->ShowTitle(show);
}

void BMLAS_ShowFPS(bool show) {
    if (RejectScriptObjectConstructionHostCall("BML::ShowFPS"))
        return;
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->ShowFPS(show);
}

void BMLAS_ShowSRTimer(bool show) {
    if (RejectScriptObjectConstructionHostCall("BML::ShowSRTimer"))
        return;
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->ShowSRTimer(show);
}

void BMLAS_StartSRTimer() {
    if (RejectScriptObjectConstructionHostCall("BML::StartSRTimer"))
        return;
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->StartSRTimer();
}

void BMLAS_PauseSRTimer() {
    if (RejectScriptObjectConstructionHostCall("BML::PauseSRTimer"))
        return;
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->PauseSRTimer();
}

void BMLAS_ResetSRTimer() {
    if (RejectScriptObjectConstructionHostCall("BML::ResetSRTimer"))
        return;
    ModContext *ctx = nullptr;
    if (RequireLoadedContext(ctx))
        ctx->ResetSRTimer();
}

float BMLAS_GetSRTime() {
    ModContext *ctx = nullptr;
    return RequireLoadedContext(ctx) ? ctx->GetSRTime() : 0.0f;
}

void BMLAS_CoreUI_SendMessage(const std::string &message) {
    if (RejectScriptObjectConstructionHostCall("BML::Core::UI::SendMessage"))
        return;
    BML::Core::UI::SendMessage(message.c_str());
}

void BMLAS_CoreUI_ClearMessages() {
    if (RejectScriptObjectConstructionHostCall("BML::Core::UI::ClearMessages"))
        return;
    BML::Core::UI::ClearMessages();
}

void BMLAS_CoreMenu_OpenModsMenu() {
    if (RejectScriptObjectConstructionHostCall("BML::Core::Menu::OpenModsMenu"))
        return;
    BML::Core::Menu::OpenModsMenu();
}

void BMLAS_CoreMenu_CloseModsMenu() {
    if (RejectScriptObjectConstructionHostCall("BML::Core::Menu::CloseModsMenu"))
        return;
    BML::Core::Menu::CloseModsMenu();
}

void BMLAS_CoreMenu_OpenMapMenu() {
    if (RejectScriptObjectConstructionHostCall("BML::Core::Menu::OpenMapMenu"))
        return;
    BML::Core::Menu::OpenMapMenu();
}

void BMLAS_CoreMenu_CloseMapMenu() {
    if (RejectScriptObjectConstructionHostCall("BML::Core::Menu::CloseMapMenu"))
        return;
    BML::Core::Menu::CloseMapMenu();
}

int BMLAS_CoreHUD_GetMode() {
    return BML::Core::HUD::GetMode();
}

void BMLAS_CoreHUD_SetMode(int mode) {
    if (RejectScriptObjectConstructionHostCall("BML::Core::HUD::SetMode"))
        return;
    BML::Core::HUD::SetMode(mode);
}

void BMLAS_CoreHUD_ShowTitle(bool show) {
    if (RejectScriptObjectConstructionHostCall("BML::Core::HUD::ShowTitle"))
        return;
    BML::Core::HUD::ShowTitle(show);
}

void BMLAS_CoreHUD_ShowFPS(bool show) {
    if (RejectScriptObjectConstructionHostCall("BML::Core::HUD::ShowFPS"))
        return;
    BML::Core::HUD::ShowFPS(show);
}

void BMLAS_CoreHUD_ShowSRTimer(bool show) {
    if (RejectScriptObjectConstructionHostCall("BML::Core::HUD::ShowSRTimer"))
        return;
    BML::Core::HUD::ShowSRTimer(show);
}

void BMLAS_CoreHUD_StartSRTimer() {
    if (RejectScriptObjectConstructionHostCall("BML::Core::HUD::StartSRTimer"))
        return;
    BML::Core::HUD::StartSRTimer();
}

void BMLAS_CoreHUD_PauseSRTimer() {
    if (RejectScriptObjectConstructionHostCall("BML::Core::HUD::PauseSRTimer"))
        return;
    BML::Core::HUD::PauseSRTimer();
}

void BMLAS_CoreHUD_ResetSRTimer() {
    if (RejectScriptObjectConstructionHostCall("BML::Core::HUD::ResetSRTimer"))
        return;
    BML::Core::HUD::ResetSRTimer();
}

float BMLAS_CoreHUD_GetSRTime() {
    return BML::Core::HUD::GetSRTime();
}

void BMLAS_SkipRenderForNextTick() {
    if (RejectScriptObjectConstructionHostCall("BML::SkipRenderForNextTick"))
        return;
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
    if (RejectScriptObjectConstructionHostCall("BML::ExitGame"))
        return;
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

static bool BMLAS_InputHookIsValid(InputHook *input) { return BML::ScriptFacadeAccess::IsInputValid(input); }
static void BMLAS_InputHookEnableKeyboardRepetition(InputHook *input, bool enable) {
    if (RejectScriptObjectConstructionHostCall("InputHook::EnableKeyboardRepetition"))
        return;
    BML::ScriptFacadeAccess::EnableKeyboardRepetition(input, enable);
}
static bool BMLAS_InputHookIsKeyboardRepetitionEnabled(InputHook *input) { return BML::ScriptFacadeAccess::IsKeyboardRepetitionEnabled(input); }
static bool BMLAS_InputHookIsKeyboardAttached(InputHook *input) { return BML::ScriptFacadeAccess::IsKeyboardAttached(input); }
static bool BMLAS_InputHookIsMouseAttached(InputHook *input) { return BML::ScriptFacadeAccess::IsMouseAttached(input); }
static bool BMLAS_InputHookIsJoystickAttached(InputHook *input, int joystick) { return BML::ScriptFacadeAccess::IsJoystickAttached(input, joystick); }
static bool BMLAS_InputHookIsKeyDown(InputHook *input, CKKEYBOARD key) { return BML::ScriptFacadeAccess::IsKeyDown(input, key); }
static bool BMLAS_InputHookIsKeyDownWithStamp(InputHook *input, CKKEYBOARD key, unsigned int &stamp) { return BML::ScriptFacadeAccess::IsKeyDown(input, key, stamp); }
static bool BMLAS_InputHookIsKeyUp(InputHook *input, CKKEYBOARD key) { return BML::ScriptFacadeAccess::IsKeyUp(input, key); }
static bool BMLAS_InputHookIsKeyPressed(InputHook *input, CKKEYBOARD key) { return BML::ScriptFacadeAccess::IsKeyPressed(input, key); }
static bool BMLAS_InputHookIsKeyReleased(InputHook *input, CKKEYBOARD key) { return BML::ScriptFacadeAccess::IsKeyReleased(input, key); }
static bool BMLAS_InputHookIsKeyToggled(InputHook *input, CKKEYBOARD key) { return BML::ScriptFacadeAccess::IsKeyToggled(input, key); }
static bool BMLAS_InputHookIsKeyToggledWithStamp(InputHook *input, CKKEYBOARD key, unsigned int &stamp) { return BML::ScriptFacadeAccess::IsKeyToggled(input, key, stamp); }
static std::string BMLAS_InputHookGetKeyName(InputHook *input, CKKEYBOARD key) {
    return BML::ScriptFacadeAccess::GetKeyName(input, key);
}
static int BMLAS_InputHookGetKeyFromName(InputHook *input, const std::string &name) {
    return BML::ScriptFacadeAccess::GetKeyFromName(input, name);
}
static int BMLAS_InputHookGetKeyboardState(InputHook *input, CKKEYBOARD key) { return BML::ScriptFacadeAccess::GetKeyboardState(input, key); }
static bool BMLAS_InputHookIsKeyboardStateDown(InputHook *input, CKKEYBOARD key) { return BML::ScriptFacadeAccess::IsKeyboardStateDown(input, key); }
static int BMLAS_InputHookGetNumberOfKeyInBuffer(InputHook *input) { return BML::ScriptFacadeAccess::GetNumberOfKeyInBuffer(input); }
static int BMLAS_InputHookGetKeyFromBuffer(InputHook *input, int index, CKKEYBOARD &key, unsigned int &timestamp) {
    return BML::ScriptFacadeAccess::GetKeyFromBuffer(input, index, key, timestamp);
}
static bool BMLAS_InputHookIsMouseButtonDown(InputHook *input, CK_MOUSEBUTTON button) { return BML::ScriptFacadeAccess::IsMouseButtonDown(input, button); }
static bool BMLAS_InputHookIsMouseClicked(InputHook *input, CK_MOUSEBUTTON button) { return BML::ScriptFacadeAccess::IsMouseClicked(input, button); }
static bool BMLAS_InputHookIsMouseToggled(InputHook *input, CK_MOUSEBUTTON button) { return BML::ScriptFacadeAccess::IsMouseToggled(input, button); }
static int BMLAS_InputHookGetMouseButtonState(InputHook *input, CK_MOUSEBUTTON button) { return BML::ScriptFacadeAccess::GetMouseButtonState(input, button); }
static Vx2DVector BMLAS_InputHookGetMousePosition(InputHook *input, bool absolute) {
    return BML::ScriptFacadeAccess::GetMousePosition(input, absolute);
}
static Vx2DVector BMLAS_InputHookGetLastMousePosition(InputHook *input) {
    return BML::ScriptFacadeAccess::GetLastMousePosition(input);
}
static VxVector BMLAS_InputHookGetMouseRelativePosition(InputHook *input) {
    return BML::ScriptFacadeAccess::GetMouseRelativePosition(input);
}
static VxVector BMLAS_InputHookGetJoystickPosition(InputHook *input, int joystick) {
    return BML::ScriptFacadeAccess::GetJoystickPosition(input, joystick);
}
static VxVector BMLAS_InputHookGetJoystickRotation(InputHook *input, int joystick) {
    return BML::ScriptFacadeAccess::GetJoystickRotation(input, joystick);
}
static Vx2DVector BMLAS_InputHookGetJoystickSliders(InputHook *input, int joystick) {
    return BML::ScriptFacadeAccess::GetJoystickSliders(input, joystick);
}
static float BMLAS_InputHookGetJoystickPointOfViewAngle(InputHook *input, int joystick) {
    return BML::ScriptFacadeAccess::GetJoystickPointOfViewAngle(input, joystick);
}
static unsigned int BMLAS_InputHookGetJoystickButtonsState(InputHook *input, int joystick) {
    return BML::ScriptFacadeAccess::GetJoystickButtonsState(input, joystick);
}
static bool BMLAS_InputHookIsJoystickButtonDown(InputHook *input, int joystick, int button) {
    return BML::ScriptFacadeAccess::IsJoystickButtonDown(input, joystick, button);
}
static void BMLAS_InputHookPause(InputHook *input, bool pause) {
    if (RejectScriptObjectConstructionHostCall("InputHook::Pause"))
        return;
    BML::ScriptFacadeAccess::PauseInput(input, pause);
}
static void BMLAS_InputHookShowCursor(InputHook *input, bool show) {
    if (RejectScriptObjectConstructionHostCall("InputHook::ShowCursor"))
        return;
    BML::ScriptFacadeAccess::ShowCursor(input, show);
}
static bool BMLAS_InputHookGetCursorVisibility(InputHook *input) { return BML::ScriptFacadeAccess::GetCursorVisibility(input); }
static int BMLAS_InputHookGetSystemCursor(InputHook *input) { return BML::ScriptFacadeAccess::GetSystemCursor(input); }
static void BMLAS_InputHookSetSystemCursor(InputHook *input, int cursor) {
    if (RejectScriptObjectConstructionHostCall("InputHook::SetSystemCursor"))
        return;
    BML::ScriptFacadeAccess::SetSystemCursor(input, cursor);
}
static bool BMLAS_InputHookIsBlock(InputHook *input) { return BML::ScriptFacadeAccess::IsBlock(input); }
static void BMLAS_InputHookSetBlock(InputHook *input, bool block) {
    if (RejectScriptObjectConstructionHostCall("InputHook::SetBlock"))
        return;
    BML::ScriptFacadeAccess::SetBlock(input, block);
}
static int BMLAS_InputHookIsBlocked(InputHook *input, int device) { return BML::ScriptFacadeAccess::IsBlocked(input, device); }
static void BMLAS_InputHookBlock(InputHook *input, int device) {
    if (RejectScriptObjectConstructionHostCall("InputHook::Block"))
        return;
    BML::ScriptFacadeAccess::Block(input, device);
}
static void BMLAS_InputHookUnblock(InputHook *input, int device) {
    if (RejectScriptObjectConstructionHostCall("InputHook::Unblock"))
        return;
    BML::ScriptFacadeAccess::Unblock(input, device);
}
static uint64_t BMLAS_InputHookAcquireBlock(InputHook *input, unsigned int mask) {
    if (RejectScriptObjectConstructionHostCall("InputHook::AcquireBlock"))
        return 0;
    return BML::ScriptFacadeAccess::AcquireBlock(input, mask);
}
static void BMLAS_InputHookReleaseBlock(InputHook *input, uint64_t token) {
    if (RejectScriptObjectConstructionHostCall("InputHook::ReleaseBlock"))
        return;
    BML::ScriptFacadeAccess::ReleaseBlock(input, token);
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
    if (RejectScriptObjectConstructionHostCall("BML::SetIC"))
        return;
    ModContext *ctx = nullptr;
    if (RequireContext(ctx) && object)
        ctx->SetIC(object, hierarchy);
}

void BMLAS_RestoreIC(CKBeObject *object, bool hierarchy) {
    if (RejectScriptObjectConstructionHostCall("BML::RestoreIC"))
        return;
    ModContext *ctx = nullptr;
    if (RequireContext(ctx) && object)
        ctx->RestoreIC(object, hierarchy);
}

void BMLAS_Show(CKBeObject *object, CK_OBJECT_SHOWOPTION show, bool hierarchy) {
    if (RejectScriptObjectConstructionHostCall("BML::Show"))
        return;
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
    if (RejectScriptObjectConstructionHostCall("BML::DataShareSetString"))
        return false;
    BML_DataShare *share = BML_GetDataShare(name.empty() ? nullptr : name.c_str());
    if (!share)
        return false;
    const int result = BML_DataShare_Set(share, key.c_str(), value.c_str(), value.size() + 1);
    BML_DataShare_Release(share);
    return result != 0;
}

template <typename T>
bool BMLAS_DataShareSetValue(const std::string &key, T value, const std::string &name) {
    if (RejectScriptObjectConstructionHostCall("BML::DataShareSet"))
        return false;
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
    if (RejectScriptObjectConstructionHostCall("BML::DataShareRemove"))
        return;
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

struct BMLAS_PhysicalizeDefinition {
    bool Fixed = false;
    float Friction = 0.7f;
    float Elasticity = 0.4f;
    float Mass = 1.0f;
    std::string CollisionGroup;
    bool StartFrozen = false;
    bool EnableCollision = true;
    bool CalcMassCenter = false;
    float LinearDamp = 0.1f;
    float RotDamp = 0.1f;
    std::string CollisionSurface;
    VxVector MassCenter = VxVector(0.0f, 0.0f, 0.0f);
};

struct BMLAS_VxRect {
    float Left = 0.0f;
    float Top = 0.0f;
    float Right = 0.0f;
    float Bottom = 0.0f;

    VxRect ToNative() const {
        return VxRect(Left, Top, Right, Bottom);
    }
};

struct BMLAS_ObjectLoadOptions {
    std::string File;
    bool Rename = true;
    std::string MasterName;
    int FilterClass = CKCID_3DOBJECT;
    bool AddToScene = true;
    bool ReuseMeshes = true;
    bool ReuseMaterials = true;
    bool Dynamic = true;
};

struct BMLAS_Text2DDefinition {
    int Font = ExecuteBB::NOFONT;
    std::string Text;
    int Align = ALIGN_CENTER;
    BMLAS_VxRect Margin = {2.0f, 2.0f, 2.0f, 2.0f};
    Vx2DVector Offset = Vx2DVector(0.0f, 0.0f);
    Vx2DVector ParagraphIndent = Vx2DVector(0.0f, 0.0f);
    float CaretSize = 0.1f;
    int Flags = TEXT_SCREEN;
};

struct BMLAS_BallTypeDefinition {
    std::string BallFile;
    std::string BallId;
    std::string BallName;
    std::string ObjectName;
    float Friction = 0.0f;
    float Elasticity = 0.0f;
    float Mass = 0.0f;
    std::string CollisionGroup;
    float LinearDamp = 0.0f;
    float RotDamp = 0.0f;
    float Force = 0.0f;
    float Radius = 0.0f;
};

struct BMLAS_FloorTypeDefinition {
    std::string Name;
    float Friction = 0.0f;
    float Elasticity = 0.0f;
    float Mass = 0.0f;
    std::string CollisionGroup;
    bool EnableCollision = true;
};

struct BMLAS_ModuleBallDefinition {
    std::string Name;
    bool Fixed = false;
    float Friction = 0.0f;
    float Elasticity = 0.0f;
    float Mass = 0.0f;
    std::string CollisionGroup;
    bool StartFrozen = false;
    bool EnableCollision = true;
    bool CalcMassCenter = false;
    float LinearDamp = 0.0f;
    float RotDamp = 0.0f;
    float Radius = 0.0f;
};

struct BMLAS_ModuleConvexDefinition {
    std::string Name;
    bool Fixed = false;
    float Friction = 0.0f;
    float Elasticity = 0.0f;
    float Mass = 0.0f;
    std::string CollisionGroup;
    bool StartFrozen = false;
    bool EnableCollision = true;
    bool CalcMassCenter = false;
    float LinearDamp = 0.0f;
    float RotDamp = 0.0f;
};

struct BMLAS_TrafoDefinition {
    std::string Name;
};

struct BMLAS_ModuleDefinition {
    std::string Name;
};

static void BMLAS_ConstructPhysicalizeDefinition(BMLAS_PhysicalizeDefinition *self) {
    new (self) BMLAS_PhysicalizeDefinition();
}

static void BMLAS_CopyConstructPhysicalizeDefinition(const BMLAS_PhysicalizeDefinition &other,
                                                     BMLAS_PhysicalizeDefinition *self) {
    new (self) BMLAS_PhysicalizeDefinition(other);
}

static void BMLAS_DestructPhysicalizeDefinition(BMLAS_PhysicalizeDefinition *self) {
    self->~BMLAS_PhysicalizeDefinition();
}

static BMLAS_PhysicalizeDefinition &BMLAS_AssignPhysicalizeDefinition(
    const BMLAS_PhysicalizeDefinition &other,
    BMLAS_PhysicalizeDefinition *self) {
    *self = other;
    return *self;
}

using BMLAS_TimerEvent = BML::ScriptTimerEventView;
using BMLAS_RenderEvent = BML::ScriptRenderEventView;
using BMLAS_CheatEvent = BML::ScriptCheatEventView;
using BMLAS_LoadObjectEvent = BML::ScriptLoadObjectEventView;
using BMLAS_LoadScriptEvent = BML::ScriptLoadScriptEventView;
using BMLAS_CommandEvent = BML::ScriptCommandEventView;
using BMLAS_CommandDefinition = BML::ScriptCommandDefinition;
using BMLAS_ConfigEvent = BML::ScriptConfigEventView;
using BMLAS_DataShareEvent = BML::ScriptDataShareEventView;
using BMLAS_PhysicalizeEvent = BML::ScriptPhysicalizeEventView;
using BMLAS_ObjectEvent = BML::ScriptObjectEventView;
using BMLAS_HookBlockEvent = BML::ScriptHookBlockEventView;

#define BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(CppType, Suffix)                         \
    static void BMLAS_Construct##Suffix(CppType *self) { new (self) CppType(); }   \
    static void BMLAS_CopyConstruct##Suffix(const CppType &other, CppType *self) { \
        new (self) CppType(other);                                                 \
    }                                                                              \
    static void BMLAS_Destruct##Suffix(CppType *self) { self->~CppType(); }        \
    static CppType &BMLAS_Assign##Suffix(const CppType &other, CppType *self) {    \
        *self = other;                                                             \
        return *self;                                                              \
    }

BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_VxRect, VxRect)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_ObjectLoadOptions, ObjectLoadOptions)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_Text2DDefinition, Text2DDefinition)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_BallTypeDefinition, BallTypeDefinition)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_FloorTypeDefinition, FloorTypeDefinition)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_ModuleBallDefinition, ModuleBallDefinition)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_ModuleConvexDefinition, ModuleConvexDefinition)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_TrafoDefinition, TrafoDefinition)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_ModuleDefinition, ModuleDefinition)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_HookBlockEvent, HookBlockEvent)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_TimerEvent, TimerEvent)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_RenderEvent, RenderEvent)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_CheatEvent, CheatEvent)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_LoadObjectEvent, LoadObjectEvent)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_LoadScriptEvent, LoadScriptEvent)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_CommandEvent, CommandEvent)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_CommandDefinition, CommandDefinition)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_ConfigEvent, ConfigEvent)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_DataShareEvent, DataShareEvent)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_PhysicalizeEvent, PhysicalizeEvent)
BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS(BMLAS_ObjectEvent, ObjectEvent)

#undef BMLAS_DEFINE_VALUE_TYPE_FUNCTIONS

static BML::ScriptModContextView *BMLAS_CreateInvalidModContext() {
    return new (std::nothrow) BML::ScriptModContextView();
}

static void BMLAS_ReleaseModContext(BML::ScriptModContextView *view) {
    delete view;
}

static BML::ScriptModContextView &BMLAS_AssignModContext(const BML::ScriptModContextView &other,
                                                         BML::ScriptModContextView *self) {
    *self = other;
    return *self;
}

static bool BMLAS_BorrowCurrentContext(BML::ScriptModContextView &outContext) {
    outContext = BML::ScriptModContextView();
    if (RejectScriptObjectConstructionHostCall("BML::BorrowCurrentContext"))
        return false;
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    BML::ScriptModContextView *view = owner ? owner->BorrowContextView() : nullptr;
    if (!view || !view->HasContext())
        return false;
    outContext = *view;
    return true;
}

class BMLAS_ObjectLoadResult {
public:
    BMLAS_ObjectLoadResult(CKContext *context, bool success, CK_ID mainObjectId, std::vector<CK_ID> objectIds)
        : m_Context(context), m_Success(success), m_MainObjectId(mainObjectId), m_ObjectIds(std::move(objectIds)) {}

    void AddRef() { ++m_RefCount; }

    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    bool IsSuccess() const { return m_Success; }

    int GetCount() const { return static_cast<int>(m_ObjectIds.size()); }

    CKObject *BorrowMainObject() const {
        return Resolve(m_MainObjectId);
    }

    CKObject *BorrowObject(int index) const {
        if (index < 0 || index >= GetCount())
            return nullptr;
        return Resolve(m_ObjectIds[static_cast<std::size_t>(index)]);
    }

private:
    CKObject *Resolve(CK_ID id) const {
        return m_Context && id != 0 ? m_Context->GetObject(id) : nullptr;
    }

    int m_RefCount = 1;
    CKContext *m_Context = nullptr;
    bool m_Success = false;
    CK_ID m_MainObjectId = 0;
    std::vector<CK_ID> m_ObjectIds;
};

static BMLAS_ObjectLoadResult *BMLAS_CreateObjectLoadResult(CKContext *context,
                                                            bool success,
                                                            CK_ID mainObjectId,
                                                            std::vector<CK_ID> objectIds) {
    return new (std::nothrow) BMLAS_ObjectLoadResult(context, success, mainObjectId, std::move(objectIds));
}

static BML::ScriptMod *BMLAS_ResolveScriptModOwner(const std::string &modId) {
    ModContext *ctx = nullptr;
    if (!RequireContext(ctx))
        return nullptr;
    IMod *mod = ctx->FindMod(modId.c_str());
    return dynamic_cast<BML::ScriptMod *>(mod);
}

class BMLAS_LoggerRef {
public:
    explicit BMLAS_LoggerRef(std::string modId) : m_ModId(std::move(modId)) {}

    void AddRef() { ++m_RefCount; }

    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    bool IsValid() const { return Resolve() != nullptr; }

    void Info(const std::string &message) const {
        if (BML::ScriptMod *owner = Resolve())
            owner->LogInfo(message);
    }

    void Warn(const std::string &message) const {
        if (BML::ScriptMod *owner = Resolve())
            owner->LogWarn(message);
    }

    void Error(const std::string &message) const {
        if (BML::ScriptMod *owner = Resolve())
            owner->LogError(message);
    }

private:
    BML::ScriptMod *Resolve() const { return BMLAS_ResolveScriptModOwner(m_ModId); }

    int m_RefCount = 1;
    std::string m_ModId;
};

class BMLAS_ConfigPropertyRef {
public:
    BMLAS_ConfigPropertyRef(std::string modId, std::string category, std::string key)
        : m_ModId(std::move(modId)), m_Category(std::move(category)), m_Key(std::move(key)) {}

    void AddRef() { ++m_RefCount; }

    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    bool IsValid() const { return ResolveProperty() != nullptr; }

    int GetType() const {
        IProperty *property = ResolveProperty();
        return property ? static_cast<int>(property->GetType()) : static_cast<int>(IProperty::NONE);
    }

    std::string GetString(const std::string &defaultValue) const {
        IProperty *property = ResolveProperty();
        if (!property || property->GetType() != IProperty::STRING)
            return defaultValue;
        const char *value = property->GetString();
        return value ? value : "";
    }

    bool GetBoolean(bool defaultValue) const {
        IProperty *property = ResolveProperty();
        return property && property->GetType() == IProperty::BOOLEAN ? property->GetBoolean() : defaultValue;
    }

    int GetInteger(int defaultValue) const {
        IProperty *property = ResolveProperty();
        return property && property->GetType() == IProperty::INTEGER ? property->GetInteger() : defaultValue;
    }

    float GetFloat(float defaultValue) const {
        IProperty *property = ResolveProperty();
        return property && property->GetType() == IProperty::FLOAT ? property->GetFloat() : defaultValue;
    }

    CKKEYBOARD GetKey(CKKEYBOARD defaultValue) const {
        IProperty *property = ResolveProperty();
        return property && property->GetType() == IProperty::KEY ? property->GetKey() : defaultValue;
    }

    void SetString(const std::string &value) const {
        if (IProperty *property = ResolveProperty())
            property->SetString(value.c_str());
    }

    void SetBoolean(bool value) const {
        if (IProperty *property = ResolveProperty())
            property->SetBoolean(value);
    }

    void SetInteger(int value) const {
        if (IProperty *property = ResolveProperty())
            property->SetInteger(value);
    }

    void SetFloat(float value) const {
        if (IProperty *property = ResolveProperty())
            property->SetFloat(value);
    }

    void SetKey(CKKEYBOARD value) const {
        if (IProperty *property = ResolveProperty())
            property->SetKey(value);
    }

    void SetComment(const std::string &comment) const {
        if (IProperty *property = ResolveProperty())
            property->SetComment(comment.c_str());
    }

    void SetDefaultString(const std::string &value) const {
        if (IProperty *property = ResolveProperty())
            property->SetDefaultString(value.c_str());
    }

    void SetDefaultBoolean(bool value) const {
        if (IProperty *property = ResolveProperty())
            property->SetDefaultBoolean(value);
    }

    void SetDefaultInteger(int value) const {
        if (IProperty *property = ResolveProperty())
            property->SetDefaultInteger(value);
    }

    void SetDefaultFloat(float value) const {
        if (IProperty *property = ResolveProperty())
            property->SetDefaultFloat(value);
    }

    void SetDefaultKey(CKKEYBOARD value) const {
        if (IProperty *property = ResolveProperty())
            property->SetDefaultKey(value);
    }

private:
    IProperty *ResolveProperty() const {
        BML::ScriptMod *owner = BMLAS_ResolveScriptModOwner(m_ModId);
        return owner ? owner->GetConfigProperty(m_Category, m_Key) : nullptr;
    }

    int m_RefCount = 1;
    std::string m_ModId;
    std::string m_Category;
    std::string m_Key;
};

class BMLAS_ConfigRef {
public:
    explicit BMLAS_ConfigRef(std::string modId) : m_ModId(std::move(modId)) {}

    void AddRef() { ++m_RefCount; }

    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    bool IsValid() const { return Resolve() != nullptr; }

    bool HasCategory(const std::string &category) const {
        BML::ScriptMod *owner = Resolve();
        return owner && owner->HasConfigCategory(category);
    }

    bool HasKey(const std::string &category, const std::string &key) const {
        BML::ScriptMod *owner = Resolve();
        return owner && owner->HasConfigKey(category, key);
    }

    BMLAS_ConfigPropertyRef *GetProperty(const std::string &category, const std::string &key) const {
        BML::ScriptMod *owner = Resolve();
        if (!owner || !owner->GetConfigProperty(category, key))
            return nullptr;
        return new (std::nothrow) BMLAS_ConfigPropertyRef(m_ModId, category, key);
    }

    void SetCategoryComment(const std::string &category, const std::string &comment) const {
        if (BML::ScriptMod *owner = Resolve())
            owner->SetConfigCategoryComment(category, comment);
    }

private:
    BML::ScriptMod *Resolve() const { return BMLAS_ResolveScriptModOwner(m_ModId); }

    int m_RefCount = 1;
    std::string m_ModId;
};

static BMLAS_LoggerRef *BMLAS_ContextBorrowLogger(BML::ScriptModContextView *view) {
    if (!view || !view->HasContext())
        return nullptr;
    BML::ScriptMod *owner = BMLAS_ResolveScriptModOwner(view->GetModId());
    return owner ? new (std::nothrow) BMLAS_LoggerRef(owner->GetID()) : nullptr;
}

static BMLAS_ConfigRef *BMLAS_ContextBorrowConfig(BML::ScriptModContextView *view) {
    if (!view || !view->HasContext())
        return nullptr;
    BML::ScriptMod *owner = BMLAS_ResolveScriptModOwner(view->GetModId());
    return owner ? new (std::nothrow) BMLAS_ConfigRef(owner->GetID()) : nullptr;
}

static BMLAS_ConfigPropertyRef *BMLAS_ConfigEventBorrowProperty(const BML::ScriptConfigEventView *event) {
    if (!event || !event->HasProperty())
        return nullptr;
    return new (std::nothrow) BMLAS_ConfigPropertyRef(event->GetModId(), event->GetCategory(), event->GetKey());
}

static BMLAS_ObjectLoadResult *BMLAS_CK_LoadObject(const BMLAS_ObjectLoadOptions &options) {
    if (RejectScriptObjectConstructionHostCall("CK::LoadObject"))
        return BMLAS_CreateObjectLoadResult(nullptr, false, 0, {});
    ModContext *ctx = nullptr;
    if (!RequireLoadedContext(ctx))
        return BMLAS_CreateObjectLoadResult(nullptr, false, 0, {});

    std::pair<XObjectArray *, CKObject *> result = ExecuteBB::ObjectLoad(options.File.c_str(),
                                                                         options.Rename,
                                                                         options.MasterName.c_str(),
                                                                         static_cast<CK_CLASSID>(options.FilterClass),
                                                                         options.AddToScene ? TRUE : FALSE,
                                                                         options.ReuseMeshes ? TRUE : FALSE,
                                                                         options.ReuseMaterials ? TRUE : FALSE,
                                                                         options.Dynamic ? TRUE : FALSE);
    std::vector<CK_ID> objectIds;
    if (result.first) {
        objectIds.reserve(static_cast<std::size_t>(result.first->Size()));
        for (CK_ID *id = result.first->Begin(); id != result.first->End(); ++id) {
            objectIds.push_back(*id);
        }
    }

    const CK_ID mainObjectId = result.second ? result.second->GetID() : 0;
    const bool success = result.first != nullptr || result.second != nullptr;
    return BMLAS_CreateObjectLoadResult(ctx->GetCKContext(), success, mainObjectId, std::move(objectIds));
}

static ExecuteBB::FontType BMLAS_ToFontType(int value) {
    if (value < ExecuteBB::NOFONT || value > ExecuteBB::GAMEFONT_CREDITS_BIG)
        return ExecuteBB::NOFONT;
    return static_cast<ExecuteBB::FontType>(value);
}

static CKBehavior *BMLAS_Text_Create2DText(CKBehavior *ownerScript,
                                           CK2dEntity *target,
                                           const BMLAS_Text2DDefinition &definition,
                                           CKMaterial *backgroundMaterial,
                                           CKMaterial *caretMaterial) {
    if (RejectScriptObjectConstructionHostCall("Text::Create2DText"))
        return nullptr;
    ModContext *ctx = nullptr;
    if (!ownerScript || !target || !RequireLoadedContext(ctx))
        return nullptr;

    return ExecuteBB::Create2DText(ownerScript,
                                   target,
                                   BMLAS_ToFontType(definition.Font),
                                   definition.Text.c_str(),
                                   definition.Align,
                                   definition.Margin.ToNative(),
                                   definition.Offset,
                                   definition.ParagraphIndent,
                                   backgroundMaterial,
                                   definition.CaretSize,
                                   caretMaterial,
                                   definition.Flags);
}

static CKBehavior *BMLAS_Text_Create2DTextDefaultMaterials(CKBehavior *ownerScript,
                                                          CK2dEntity *target,
                                                          const BMLAS_Text2DDefinition &definition) {
    return BMLAS_Text_Create2DText(ownerScript, target, definition, nullptr, nullptr);
}

static bool BMLAS_ContextRegisterBallType(const BML::ScriptModContextView *view,
                                          const BMLAS_BallTypeDefinition &definition) {
    return view && view->RegisterBallType(definition.BallFile, definition.BallId, definition.BallName,
                                          definition.ObjectName, definition.Friction, definition.Elasticity,
                                          definition.Mass, definition.CollisionGroup, definition.LinearDamp,
                                          definition.RotDamp, definition.Force, definition.Radius);
}

static bool BMLAS_ContextRegisterFloorType(const BML::ScriptModContextView *view,
                                           const BMLAS_FloorTypeDefinition &definition) {
    return view && view->RegisterFloorType(definition.Name, definition.Friction, definition.Elasticity,
                                           definition.Mass, definition.CollisionGroup,
                                           definition.EnableCollision);
}

static bool BMLAS_ContextRegisterModuleBall(const BML::ScriptModContextView *view,
                                            const BMLAS_ModuleBallDefinition &definition) {
    return view && view->RegisterModulBall(definition.Name, definition.Fixed, definition.Friction,
                                           definition.Elasticity, definition.Mass, definition.CollisionGroup,
                                           definition.StartFrozen, definition.EnableCollision,
                                           definition.CalcMassCenter, definition.LinearDamp, definition.RotDamp,
                                           definition.Radius);
}

static bool BMLAS_ContextRegisterModuleConvex(const BML::ScriptModContextView *view,
                                              const BMLAS_ModuleConvexDefinition &definition) {
    return view && view->RegisterModulConvex(definition.Name, definition.Fixed, definition.Friction,
                                             definition.Elasticity, definition.Mass, definition.CollisionGroup,
                                             definition.StartFrozen, definition.EnableCollision,
                                             definition.CalcMassCenter, definition.LinearDamp, definition.RotDamp);
}

static bool BMLAS_ContextRegisterTrafo(const BML::ScriptModContextView *view,
                                       const BMLAS_TrafoDefinition &definition) {
    return view && view->RegisterTrafo(definition.Name);
}

static bool BMLAS_ContextRegisterModule(const BML::ScriptModContextView *view,
                                        const BMLAS_ModuleDefinition &definition) {
    return view && view->RegisterModul(definition.Name);
}

static void BMLAS_CK_Set3dEntityPosition(CK3dEntity *entity, const VxVector &position) {
    if (RejectScriptObjectConstructionHostCall("CK3dEntity::SetPosition"))
        return;
    if (entity)
        entity->SetPosition(&position);
}

static void BMLAS_CK_Set3dEntityScale(CK3dEntity *entity, const VxVector &scale, bool local) {
    if (RejectScriptObjectConstructionHostCall("CK3dEntity::SetScale"))
        return;
    if (entity)
        entity->SetScale(&scale, FALSE, local ? TRUE : FALSE);
}

static CK3dEntity *BMLAS_CK_Borrow3dEntityChild(CK3dEntity *entity, int index) {
    if (!entity || index < 0 || index >= entity->GetChildrenCount())
        return nullptr;
    return entity->GetChild(index);
}

static bool BMLAS_CK_HasDataArrayCell(CKDataArray *array, int row, int column) {
    return array && row >= 0 && column >= 0 && row < array->GetRowCount() && column < array->GetColumnCount();
}

static int BMLAS_CK_GetDataArrayRowCount(CKDataArray *array) {
    return array ? array->GetRowCount() : 0;
}

static int BMLAS_CK_GetDataArrayColumnCount(CKDataArray *array) {
    return array ? array->GetColumnCount() : 0;
}

static std::string BMLAS_CK_GetDataArrayColumnName(CKDataArray *array, int column) {
    if (!array || column < 0 || column >= array->GetColumnCount())
        return "";
    char *name = array->GetColumnName(column);
    return name ? name : "";
}

static int BMLAS_CK_FindDataArrayColumn(CKDataArray *array, const std::string &name) {
    if (!array)
        return -1;
    const int count = array->GetColumnCount();
    for (int i = 0; i < count; ++i) {
        char *columnName = array->GetColumnName(i);
        if (columnName && name == columnName)
            return i;
    }
    return -1;
}

static std::string BMLAS_CK_GetDataArrayString(CKDataArray *array,
                                               int row,
                                               int column,
                                               const std::string &defaultValue) {
    if (!BMLAS_CK_HasDataArrayCell(array, row, column))
        return defaultValue;

    char buffer[4096] = {};
    const int result = array->GetElementStringValue(row, column, buffer);
    if (result <= 0 && buffer[0] == '\0')
        return defaultValue;
    return buffer;
}

static bool BMLAS_CK_GetDataArrayBool(CKDataArray *array, int row, int column, bool defaultValue) {
    if (!BMLAS_CK_HasDataArrayCell(array, row, column))
        return defaultValue;
    int value = defaultValue ? 1 : 0;
    return array->GetElementValue(row, column, &value) ? value != 0 : defaultValue;
}

static int BMLAS_CK_GetDataArrayInt(CKDataArray *array, int row, int column, int defaultValue) {
    if (!BMLAS_CK_HasDataArrayCell(array, row, column))
        return defaultValue;
    int value = defaultValue;
    return array->GetElementValue(row, column, &value) ? value : defaultValue;
}

static float BMLAS_CK_GetDataArrayFloat(CKDataArray *array, int row, int column, float defaultValue) {
    if (!BMLAS_CK_HasDataArrayCell(array, row, column))
        return defaultValue;
    float value = defaultValue;
    return array->GetElementValue(row, column, &value) ? value : defaultValue;
}

static bool BMLAS_CK_SetDataArrayString(CKDataArray *array,
                                        int row,
                                        int column,
                                        const std::string &value) {
    if (RejectScriptObjectConstructionHostCall("CKDataArray::SetString"))
        return false;
    if (!BMLAS_CK_HasDataArrayCell(array, row, column))
        return false;
    std::string copy = value;
    return array->SetElementStringValue(row, column, copy.empty() ? const_cast<char *>("") : &copy[0]) != 0;
}

static bool BMLAS_CK_SetDataArrayBool(CKDataArray *array, int row, int column, bool value) {
    if (RejectScriptObjectConstructionHostCall("CKDataArray::SetBool"))
        return false;
    if (!BMLAS_CK_HasDataArrayCell(array, row, column))
        return false;
    int stored = value ? 1 : 0;
    return array->SetElementValue(row, column, &stored) != 0;
}

static bool BMLAS_CK_SetDataArrayInt(CKDataArray *array, int row, int column, int value) {
    if (RejectScriptObjectConstructionHostCall("CKDataArray::SetInt"))
        return false;
    return BMLAS_CK_HasDataArrayCell(array, row, column) && array->SetElementValue(row, column, &value) != 0;
}

static bool BMLAS_CK_SetDataArrayFloat(CKDataArray *array, int row, int column, float value) {
    if (RejectScriptObjectConstructionHostCall("CKDataArray::SetFloat"))
        return false;
    return BMLAS_CK_HasDataArrayCell(array, row, column) && array->SetElementValue(row, column, &value) != 0;
}

static bool BMLAS_Physics_HasTarget(CK3dEntity *target) {
    ModContext *ctx = nullptr;
    return target && RequireLoadedContext(ctx);
}

static bool BMLAS_Physics_PhysicalizeConvex(CK3dEntity *target,
                                            const BMLAS_PhysicalizeDefinition &definition,
                                            CKMesh *mesh) {
    if (RejectScriptObjectConstructionHostCall("Physics::PhysicalizeConvex"))
        return false;
    if (!BMLAS_Physics_HasTarget(target))
        return false;
    ExecuteBB::PhysicalizeConvex(target, definition.Fixed, definition.Friction, definition.Elasticity,
                                 definition.Mass, definition.CollisionGroup.c_str(), definition.StartFrozen,
                                 definition.EnableCollision, definition.CalcMassCenter, definition.LinearDamp,
                                 definition.RotDamp, definition.CollisionSurface.c_str(), definition.MassCenter,
                                 mesh);
    return true;
}

static bool BMLAS_Physics_PhysicalizeBall(CK3dEntity *target,
                                          const BMLAS_PhysicalizeDefinition &definition,
                                          const VxVector &center,
                                          float radius) {
    if (RejectScriptObjectConstructionHostCall("Physics::PhysicalizeBall"))
        return false;
    if (!BMLAS_Physics_HasTarget(target))
        return false;
    ExecuteBB::PhysicalizeBall(target, definition.Fixed, definition.Friction, definition.Elasticity,
                               definition.Mass, definition.CollisionGroup.c_str(), definition.StartFrozen,
                               definition.EnableCollision, definition.CalcMassCenter, definition.LinearDamp,
                               definition.RotDamp, definition.CollisionSurface.c_str(), definition.MassCenter,
                               center, radius);
    return true;
}

static bool BMLAS_Physics_PhysicalizeConcave(CK3dEntity *target,
                                             const BMLAS_PhysicalizeDefinition &definition,
                                             CKMesh *mesh) {
    if (RejectScriptObjectConstructionHostCall("Physics::PhysicalizeConcave"))
        return false;
    if (!BMLAS_Physics_HasTarget(target))
        return false;
    ExecuteBB::PhysicalizeConcave(target, definition.Fixed, definition.Friction, definition.Elasticity,
                                  definition.Mass, definition.CollisionGroup.c_str(), definition.StartFrozen,
                                  definition.EnableCollision, definition.CalcMassCenter, definition.LinearDamp,
                                  definition.RotDamp, definition.CollisionSurface.c_str(), definition.MassCenter,
                                  mesh);
    return true;
}

static bool BMLAS_Physics_Unphysicalize(CK3dEntity *target) {
    if (RejectScriptObjectConstructionHostCall("Physics::Unphysicalize"))
        return false;
    if (!BMLAS_Physics_HasTarget(target))
        return false;
    ExecuteBB::Unphysicalize(target);
    return true;
}

static bool BMLAS_Physics_SetForce(CK3dEntity *target,
                                   const VxVector &position,
                                   CK3dEntity *positionReference,
                                   const VxVector &direction,
                                   CK3dEntity *directionReference,
                                   float force) {
    if (RejectScriptObjectConstructionHostCall("Physics::SetForce"))
        return false;
    if (!BMLAS_Physics_HasTarget(target))
        return false;
    ExecuteBB::SetPhysicsForce(target, position, positionReference, direction, directionReference, force);
    return true;
}

static bool BMLAS_Physics_ClearForce(CK3dEntity *target) {
    if (RejectScriptObjectConstructionHostCall("Physics::ClearForce"))
        return false;
    if (!BMLAS_Physics_HasTarget(target))
        return false;
    ExecuteBB::UnsetPhysicsForce(target);
    return true;
}

static bool BMLAS_Physics_Impulse(CK3dEntity *target,
                                  const VxVector &position,
                                  CK3dEntity *positionReference,
                                  const VxVector &direction,
                                  CK3dEntity *directionReference,
                                  float impulse) {
    if (RejectScriptObjectConstructionHostCall("Physics::Impulse"))
        return false;
    if (!BMLAS_Physics_HasTarget(target))
        return false;
    ExecuteBB::PhysicsImpulse(target, position, positionReference, direction, directionReference, impulse);
    return true;
}

static bool BMLAS_Physics_WakeUp(CK3dEntity *target) {
    if (RejectScriptObjectConstructionHostCall("Physics::WakeUp"))
        return false;
    if (!BMLAS_Physics_HasTarget(target))
        return false;
    ExecuteBB::PhysicsWakeUp(target);
    return true;
}

static BML::ScriptTimerRef *BMLAS_AddTimer(asIScriptObject *timer) {
    if (RejectScriptObjectConstructionHostCall("BML::AddTimer"))
        return nullptr;
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    if (owner)
        return owner->AddScriptTimer(timer);
    return nullptr;
}

static BML::ScriptCommandRef *BMLAS_RegisterCommand(asIScriptObject *command) {
    if (RejectScriptObjectConstructionHostCall("BML::RegisterCommand"))
        return nullptr;
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    if (owner)
        return owner->RegisterScriptCommand(command);
    return nullptr;
}

static BML::ScriptCommandRef *BMLAS_ContextRegisterCommandDelegate(
    const BML::ScriptModContextView *context,
    const BMLAS_CommandDefinition &definition,
    asIScriptFunction *execute,
    asIScriptFunction *complete) {
    if (RejectScriptObjectConstructionHostCall("BML::ModContext::RegisterCommand"))
        return nullptr;
    return context ? context->RegisterCommand(definition, execute, complete) : nullptr;
}

static bool BMLAS_UnregisterCommand(const std::string &name) {
    if (RejectScriptObjectConstructionHostCall("BML::UnregisterCommand"))
        return false;
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    return owner && owner->UnregisterScriptCommand(name);
}

static BML::ScriptDataShareRequestRef *BMLAS_RequestDataShare(asIScriptObject *request) {
    if (RejectScriptObjectConstructionHostCall("BML::RequestDataShare"))
        return nullptr;
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    if (owner)
        return owner->RequestScriptDataShare(request);
    return nullptr;
}

static BML::ScriptDataShareRequestRef *BMLAS_ContextRequestDataShareDelegate(
    const BML::ScriptModContextView *context,
    const std::string &key,
    int type,
    asIScriptFunction *callback,
    const std::string &name) {
    if (RejectScriptObjectConstructionHostCall("BML::ModContext::RequestDataShare"))
        return nullptr;
    return context ? context->RequestDataShare(key, type, callback, name) : nullptr;
}

static BML::ScriptHookBlockRef *BMLAS_Hook_Create(CKBehavior *ownerScript,
                                                  asIScriptFunction *callback,
                                                  const std::string &name,
                                                  int inputCount,
                                                  int outputCount) {
    if (RejectScriptObjectConstructionHostCall("BML::Hook::Create"))
        return nullptr;
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    return owner ? owner->CreateScriptHookBlock(ownerScript, callback, name, inputCount, outputCount) : nullptr;
}

static BML::ScriptHookBlockRef *BMLAS_Hook_InsertAfter(CKBehavior *ownerScript,
                                                       CKBehavior *source,
                                                       asIScriptFunction *callback,
                                                       const std::string &name,
                                                       int sourceOutput,
                                                       int targetInput) {
    if (RejectScriptObjectConstructionHostCall("BML::Hook::InsertAfter"))
        return nullptr;
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    return owner ? owner->InsertScriptHookBlockAfter(ownerScript, source, callback, name, sourceOutput, targetInput) : nullptr;
}

static BML::ScriptHookBlockRef *BMLAS_Hook_InsertBefore(CKBehavior *ownerScript,
                                                        CKBehavior *target,
                                                        asIScriptFunction *callback,
                                                        const std::string &name,
                                                        int sourceOutput,
                                                        int targetInput) {
    if (RejectScriptObjectConstructionHostCall("BML::Hook::InsertBefore"))
        return nullptr;
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    return owner ? owner->InsertScriptHookBlockBefore(ownerScript, target, callback, name, sourceOutput, targetInput) : nullptr;
}

static BML::ScriptHookBlockRef *BMLAS_Hook_InsertBetween(CKBehavior *ownerScript,
                                                         CKBehavior *source,
                                                         CKBehavior *target,
                                                         asIScriptFunction *callback,
                                                         const std::string &name,
                                                         int sourceOutput,
                                                         int targetInput) {
    if (RejectScriptObjectConstructionHostCall("BML::Hook::InsertBetween"))
        return nullptr;
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    return owner ? owner->InsertScriptHookBlockBetween(ownerScript, source, target, callback, name, sourceOutput, targetInput) : nullptr;
}

static void BMLAS_HookBlockRefSetEnabled(BML::ScriptHookBlockRef *self, bool enabled) {
    if (self)
        self->SetEnabled(enabled);
}

static void BMLAS_HookBlockRefSetAutoActivateOutputs(BML::ScriptHookBlockRef *self, bool enabled) {
    if (self)
        self->SetAutoActivateOutputs(enabled);
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
    if (RejectScriptObjectConstructionHostCall("BML::RegisterBallType"))
        return false;
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
    if (RejectScriptObjectConstructionHostCall("BML::RegisterFloorType"))
        return false;
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
    if (RejectScriptObjectConstructionHostCall("BML::RegisterModulBall"))
        return false;
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
    if (RejectScriptObjectConstructionHostCall("BML::RegisterModulConvex"))
        return false;
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    return owner && owner->RegisterScriptModulConvex(modulName, fixed, friction, elasticity, mass, collGroup,
                                                     frozen, enableColl, calcMassCenter, linearDamp, rotDamp);
}

static bool BMLAS_RegisterTrafo(const std::string &modulName) {
    if (RejectScriptObjectConstructionHostCall("BML::RegisterTrafo"))
        return false;
    BML::ScriptMod *owner = BMLAS_CurrentScriptMod();
    return owner && owner->RegisterScriptTrafo(modulName);
}

static bool BMLAS_RegisterModul(const std::string &modulName) {
    if (RejectScriptObjectConstructionHostCall("BML::RegisterModul"))
        return false;
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

    int SetBoolArray(unsigned int index, void *values) {
        return SetContiguousArray<BoolArrayTraits>(index, values);
    }

    int GetBoolArray(unsigned int index, void *&values) const {
        return GetContiguousArray<BoolArrayTraits>(index, values);
    }

    int SetIntArray(unsigned int index, void *values) {
        return SetContiguousArray<IntArrayTraits>(index, values);
    }

    int GetIntArray(unsigned int index, void *&values) const {
        return GetContiguousArray<IntArrayTraits>(index, values);
    }

    int SetFloatArray(unsigned int index, void *values) {
        return SetContiguousArray<FloatArrayTraits>(index, values);
    }

    int GetFloatArray(unsigned int index, void *&values) const {
        return GetContiguousArray<FloatArrayTraits>(index, values);
    }

    int SetStringArray(unsigned int index, void *values) {
        if (!m_Frame || !values)
            return BML_ERROR_INVALID_PARAMETER;
        std::vector<std::string> copy;
        int status = CopyStringArrayFromScript(values, copy);
        if (status != BML_OK)
            return status;
        std::vector<const char *> pointers(copy.size());
        for (size_t i = 0; i < copy.size(); ++i)
            pointers[i] = copy[i].c_str();
        return BML_CallFrame_SetValue(m_Frame, index, BML_CALL_VALUE_STRING_ARRAY, pointers.data(), pointers.size());
    }

    int GetStringArray(unsigned int index, void *&values) const {
        values = nullptr;
        if (!m_Frame)
            return BML_ERROR_INTEROP_BAD_CALL_FRAME;
        const void *data = nullptr;
        size_t count = 0;
        size_t elementSize = 0;
        const int status = BML_CallFrame_BorrowValue(m_Frame,
                                                     index,
                                                     BML_CALL_VALUE_STRING_ARRAY,
                                                     &data,
                                                     &count,
                                                     &elementSize);
        if (status != BML_OK)
            return status;
        if (elementSize != sizeof(const char *))
            return BML_ERROR_INTEROP_BAD_CALL_FRAME;
        return CreateStringArrayFromPointers(static_cast<const char *const *>(data), count, values);
    }

    int SetBuffer(unsigned int index, void *values) {
        return SetContiguousArray<BufferArrayTraits>(index, values);
    }

    int GetBuffer(unsigned int index, void *&values) const {
        return GetContiguousArray<BufferArrayTraits>(index, values);
    }

    int SetObjectId(unsigned int index, int objectId) {
        return m_Frame ? BML_CallFrame_SetObjectId(m_Frame, index, objectId) : BML_ERROR_INVALID_PARAMETER;
    }

    int GetObjectId(unsigned int index, int &objectId) const {
        return m_Frame ? BML_CallFrame_GetObjectId(m_Frame, index, &objectId) : BML_ERROR_INTEROP_BAD_CALL_FRAME;
    }

    int SetObject(unsigned int index, CKObject *object) {
        return SetObjectId(index, object ? static_cast<int>(object->GetID()) : 0);
    }

    int GetObject(unsigned int index, CKObject *&object) const {
        object = nullptr;
        int objectId = 0;
        const int status = GetObjectId(index, objectId);
        if (status != BML_OK)
            return status;
        if (objectId == 0)
            return BML_OK;
        ModContext *ctx = GetActiveContext();
        object = ctx && ctx->GetCKContext() ? ctx->GetCKContext()->GetObject(static_cast<CK_ID>(objectId)) : nullptr;
        return object ? BML_OK : BML_ERROR_INTEROP_HANDLE_STALE;
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

    int SetResultBoolArray(void *values) {
        return SetResultContiguousArray<BoolArrayTraits>(values);
    }

    int GetResultBoolArray(void *&values) const {
        return GetResultContiguousArray<BoolArrayTraits>(values);
    }

    int SetResultIntArray(void *values) {
        return SetResultContiguousArray<IntArrayTraits>(values);
    }

    int GetResultIntArray(void *&values) const {
        return GetResultContiguousArray<IntArrayTraits>(values);
    }

    int SetResultFloatArray(void *values) {
        return SetResultContiguousArray<FloatArrayTraits>(values);
    }

    int GetResultFloatArray(void *&values) const {
        return GetResultContiguousArray<FloatArrayTraits>(values);
    }

    int SetResultStringArray(void *values) {
        if (!m_Frame || !values)
            return BML_ERROR_INVALID_PARAMETER;
        std::vector<std::string> copy;
        int status = CopyStringArrayFromScript(values, copy);
        if (status != BML_OK)
            return status;
        std::vector<const char *> pointers(copy.size());
        for (size_t i = 0; i < copy.size(); ++i)
            pointers[i] = copy[i].c_str();
        return BML_CallFrame_SetResultValue(m_Frame, BML_CALL_VALUE_STRING_ARRAY, pointers.data(), pointers.size());
    }

    int GetResultStringArray(void *&values) const {
        values = nullptr;
        if (!m_Frame)
            return BML_ERROR_INTEROP_BAD_CALL_FRAME;
        const void *data = nullptr;
        size_t count = 0;
        size_t elementSize = 0;
        const int status = BML_CallFrame_BorrowResultValue(m_Frame,
                                                           BML_CALL_VALUE_STRING_ARRAY,
                                                           &data,
                                                           &count,
                                                           &elementSize);
        if (status != BML_OK)
            return status;
        if (elementSize != sizeof(const char *))
            return BML_ERROR_INTEROP_BAD_CALL_FRAME;
        return CreateStringArrayFromPointers(static_cast<const char *const *>(data), count, values);
    }

    int SetResultBuffer(void *values) {
        return SetResultContiguousArray<BufferArrayTraits>(values);
    }

    int GetResultBuffer(void *&values) const {
        return GetResultContiguousArray<BufferArrayTraits>(values);
    }

    int SetResultObjectId(int objectId) {
        return m_Frame ? BML_CallFrame_SetResultObjectId(m_Frame, objectId) : BML_ERROR_INVALID_PARAMETER;
    }

    int GetResultObjectId(int &objectId) const {
        return m_Frame ? BML_CallFrame_GetResultObjectId(m_Frame, &objectId) : BML_ERROR_INTEROP_BAD_CALL_FRAME;
    }

    int SetResultObject(CKObject *object) {
        return SetResultObjectId(object ? static_cast<int>(object->GetID()) : 0);
    }

    int GetResultObject(CKObject *&object) const {
        object = nullptr;
        int objectId = 0;
        const int status = GetResultObjectId(objectId);
        if (status != BML_OK)
            return status;
        if (objectId == 0)
            return BML_OK;
        ModContext *ctx = GetActiveContext();
        object = ctx && ctx->GetCKContext() ? ctx->GetCKContext()->GetObject(static_cast<CK_ID>(objectId)) : nullptr;
        return object ? BML_OK : BML_ERROR_INTEROP_HANDLE_STALE;
    }

    BML_CallFrame *GetNativeFrame() const { return m_Frame; }

private:
    struct BoolArrayTraits {
        using FrameElement = int;
        using ScriptElement = bool;
        static constexpr BML_CALL_VALUE_TYPE FrameType = BML_CALL_VALUE_BOOL_ARRAY;
        static constexpr int ScriptTypeId = asTYPEID_BOOL;
        static const char *Decl() { return "array<bool>"; }
        static FrameElement ToFrame(ScriptElement value) { return value ? 1 : 0; }
        static ScriptElement ToScript(FrameElement value) { return value != 0; }
    };

    struct IntArrayTraits {
        using FrameElement = int;
        using ScriptElement = int;
        static constexpr BML_CALL_VALUE_TYPE FrameType = BML_CALL_VALUE_INT_ARRAY;
        static constexpr int ScriptTypeId = asTYPEID_INT32;
        static const char *Decl() { return "array<int>"; }
        static FrameElement ToFrame(ScriptElement value) { return value; }
        static ScriptElement ToScript(FrameElement value) { return value; }
    };

    struct FloatArrayTraits {
        using FrameElement = float;
        using ScriptElement = float;
        static constexpr BML_CALL_VALUE_TYPE FrameType = BML_CALL_VALUE_FLOAT_ARRAY;
        static constexpr int ScriptTypeId = asTYPEID_FLOAT;
        static const char *Decl() { return "array<float>"; }
        static FrameElement ToFrame(ScriptElement value) { return value; }
        static ScriptElement ToScript(FrameElement value) { return value; }
    };

    struct BufferArrayTraits {
        using FrameElement = std::uint8_t;
        using ScriptElement = std::uint8_t;
        static constexpr BML_CALL_VALUE_TYPE FrameType = BML_CALL_VALUE_BUFFER;
        static constexpr int ScriptTypeId = asTYPEID_UINT8;
        static const char *Decl() { return "array<uint8>"; }
        static FrameElement ToFrame(ScriptElement value) { return value; }
        static ScriptElement ToScript(FrameElement value) { return value; }
    };

    static int MapArrayStatus(CKAS_STATUS status) {
        switch (status) {
        case CKAS_OK:
            return BML_OK;
        case CKAS_TYPEMISMATCH:
            return BML_ERROR_INTEROP_TYPE_MISMATCH;
        case CKAS_UNSUPPORTED:
            return BML_ERROR_INTEROP_UNSUPPORTED;
        default:
            return BML_ERROR_INTEROP_BAD_CALL_FRAME;
        }
    }

    static const CKAngelScriptAdapter::Api *GetArrayApi(CKAngelScript **angelScript = nullptr) {
        if (angelScript)
            *angelScript = nullptr;
        if (!g_AngelScriptHost.IsAvailable() || !g_AngelScriptHost.GetAngelScript()) {
            ModContext *context = GetActiveContext();
            if (context && context->GetCKContext())
                g_AngelScriptHost.Refresh(context->GetCKContext());
        }
        if (!g_AngelScriptHost.IsAvailable() || !g_AngelScriptHost.GetAngelScript())
            return nullptr;
        if (angelScript)
            *angelScript = g_AngelScriptHost.GetAngelScript();
        return &g_AngelScriptHost.GetApi();
    }

    static int GetArraySize(void *values, CKDWORD &count) {
        count = 0;
        if (!values)
            return BML_ERROR_INVALID_PARAMETER;
        const CKAngelScriptAdapter::Api *api = GetArrayApi();
        if (!api || !api->ArrayGetSize)
            return BML_ERROR_INTEROP_UNSUPPORTED;
        return MapArrayStatus(api->ArrayGetSize(values, &count));
    }

    static int RequireArrayElementType(void *values, int expectedTypeId) {
        if (!values)
            return BML_ERROR_INVALID_PARAMETER;
        const CKAngelScriptAdapter::Api *api = GetArrayApi();
        if (!api || !api->ArrayGetElementTypeId)
            return BML_ERROR_INTEROP_UNSUPPORTED;
        int actualTypeId = 0;
        const int status = MapArrayStatus(api->ArrayGetElementTypeId(values, &actualTypeId));
        if (status != BML_OK)
            return status;
        return actualTypeId == expectedTypeId ? BML_OK : BML_ERROR_INTEROP_TYPE_MISMATCH;
    }

    static int GetStringTypeId(void *values, int &typeId) {
        typeId = 0;
        if (!values)
            return BML_ERROR_INVALID_PARAMETER;
        const CKAngelScriptAdapter::Api *api = GetArrayApi();
        if (!api || !api->ArrayGetArrayType)
            return BML_ERROR_INTEROP_UNSUPPORTED;
        asITypeInfo *arrayType = nullptr;
        int status = MapArrayStatus(api->ArrayGetArrayType(values, &arrayType));
        if (status != BML_OK)
            return status;
        asIScriptEngine *engine = arrayType ? arrayType->GetEngine() : nullptr;
        if (!engine)
            return BML_ERROR_INTEROP_BAD_CALL_FRAME;
        typeId = engine->GetTypeIdByDecl("string");
        return typeId >= 0 ? BML_OK : BML_ERROR_INTEROP_UNSUPPORTED;
    }

    static int RequireStringArray(void *values) {
        int stringTypeId = 0;
        int status = GetStringTypeId(values, stringTypeId);
        if (status != BML_OK)
            return status;
        return RequireArrayElementType(values, stringTypeId);
    }

    static void ReleaseArray(void *&values) {
        if (!values)
            return;
        const CKAngelScriptAdapter::Api *api = GetArrayApi();
        if (api && api->ArrayRelease)
            api->ArrayRelease(values);
        values = nullptr;
    }

    static int CreateArray(const char *decl, size_t count, void *&array) {
        array = nullptr;
        CKAngelScript *angelScript = nullptr;
        const CKAngelScriptAdapter::Api *api = GetArrayApi(&angelScript);
        if (!api || !api->CreateArray || !angelScript)
            return BML_ERROR_INTEROP_UNSUPPORTED;
        return MapArrayStatus(api->CreateArray(angelScript, decl, static_cast<CKDWORD>(count), &array));
    }

    static int AssignArrayHandle(void *array, void *&values) {
        values = nullptr;
        if (!array)
            return BML_ERROR_INVALID_PARAMETER;
        CKAngelScript *angelScript = nullptr;
        const CKAngelScriptAdapter::Api *api = GetArrayApi(&angelScript);
        if (!api || !api->AssignObjectHandle || !api->ArrayGetArrayType || !angelScript)
            return BML_ERROR_INTEROP_UNSUPPORTED;

        asITypeInfo *arrayType = nullptr;
        int status = MapArrayStatus(api->ArrayGetArrayType(array, &arrayType));
        if (status != BML_OK)
            return status;
        return MapArrayStatus(api->AssignObjectHandle(&values, array, arrayType));
    }

    template <typename Traits>
    static int CopyContiguousArrayFromScript(void *values, std::vector<typename Traits::FrameElement> &copy) {
        CKDWORD count = 0;
        int status = GetArraySize(values, count);
        if (status != BML_OK)
            return status;
        status = RequireArrayElementType(values, Traits::ScriptTypeId);
        if (status != BML_OK)
            return status;
        const CKAngelScriptAdapter::Api *api = GetArrayApi();
        if (!api || !api->ArrayGetElementAddress)
            return BML_ERROR_INTEROP_UNSUPPORTED;
        copy.resize(count);
        for (CKDWORD i = 0; i < count; ++i) {
            void *address = nullptr;
            status = MapArrayStatus(api->ArrayGetElementAddress(values, i, &address));
            if (status != BML_OK)
                return status;
            const auto *value = static_cast<const typename Traits::ScriptElement *>(address);
            copy[i] = value ? Traits::ToFrame(*value) : typename Traits::FrameElement();
        }
        return BML_OK;
    }

    template <typename Traits>
    int SetContiguousArray(unsigned int index, void *values) {
        if (!m_Frame || !values)
            return BML_ERROR_INVALID_PARAMETER;
        std::vector<typename Traits::FrameElement> copy;
        const int status = CopyContiguousArrayFromScript<Traits>(values, copy);
        if (status != BML_OK)
            return status;
        return BML_CallFrame_SetValue(m_Frame, index, Traits::FrameType, copy.data(), copy.size());
    }

    template <typename Traits>
    int SetResultContiguousArray(void *values) {
        if (!m_Frame || !values)
            return BML_ERROR_INVALID_PARAMETER;
        std::vector<typename Traits::FrameElement> copy;
        const int status = CopyContiguousArrayFromScript<Traits>(values, copy);
        if (status != BML_OK)
            return status;
        return BML_CallFrame_SetResultValue(m_Frame, Traits::FrameType, copy.data(), copy.size());
    }

    template <typename Traits>
    static int CreateContiguousArrayFromData(const void *data, size_t count, size_t elementSize, void *&values) {
        values = nullptr;
        if (elementSize != sizeof(typename Traits::FrameElement))
            return BML_ERROR_INTEROP_BAD_CALL_FRAME;

        void *array = nullptr;
        int status = CreateArray(Traits::Decl(), count, array);
        if (status != BML_OK)
            return status;
        const CKAngelScriptAdapter::Api *api = GetArrayApi();
        if (!api || !api->ArraySetElementValue) {
            ReleaseArray(array);
            return BML_ERROR_INTEROP_UNSUPPORTED;
        }

        const auto *source = static_cast<const typename Traits::FrameElement *>(data);
        for (CKDWORD i = 0; i < count; ++i) {
            const typename Traits::ScriptElement value = source ? Traits::ToScript(source[i])
                                                                : typename Traits::ScriptElement();
            status = MapArrayStatus(api->ArraySetElementValue(array, i, &value));
            if (status != BML_OK) {
                ReleaseArray(array);
                return status;
            }
        }
        status = AssignArrayHandle(array, values);
        ReleaseArray(array);
        return status;
    }

    template <typename Traits>
    int GetContiguousArray(unsigned int index, void *&values) const {
        values = nullptr;
        if (!m_Frame)
            return BML_ERROR_INTEROP_BAD_CALL_FRAME;
        const void *data = nullptr;
        size_t count = 0;
        size_t elementSize = 0;
        const int status = BML_CallFrame_BorrowValue(m_Frame, index, Traits::FrameType, &data, &count, &elementSize);
        if (status != BML_OK)
            return status;
        return CreateContiguousArrayFromData<Traits>(data, count, elementSize, values);
    }

    template <typename Traits>
    int GetResultContiguousArray(void *&values) const {
        values = nullptr;
        if (!m_Frame)
            return BML_ERROR_INTEROP_BAD_CALL_FRAME;
        const void *data = nullptr;
        size_t count = 0;
        size_t elementSize = 0;
        const int status = BML_CallFrame_BorrowResultValue(m_Frame, Traits::FrameType, &data, &count, &elementSize);
        if (status != BML_OK)
            return status;
        return CreateContiguousArrayFromData<Traits>(data, count, elementSize, values);
    }

    static int CopyStringArrayFromScript(void *values, std::vector<std::string> &copy) {
        CKDWORD count = 0;
        int status = GetArraySize(values, count);
        if (status != BML_OK)
            return status;
        status = RequireStringArray(values);
        if (status != BML_OK)
            return status;
        const CKAngelScriptAdapter::Api *api = GetArrayApi();
        if (!api || !api->ArrayGetElementAddress)
            return BML_ERROR_INTEROP_UNSUPPORTED;
        copy.resize(count);
        for (CKDWORD i = 0; i < count; ++i) {
            void *address = nullptr;
            status = MapArrayStatus(api->ArrayGetElementAddress(values, i, &address));
            if (status != BML_OK)
                return status;
            const auto *value = static_cast<const std::string *>(address);
            copy[i] = value ? *value : "";
        }
        return BML_OK;
    }

    static int CreateStringArrayFromPointers(const char *const *items, size_t count, void *&values) {
        void *array = nullptr;
        int status = CreateArray("array<string>", count, array);
        if (status != BML_OK)
            return status;
        const CKAngelScriptAdapter::Api *api = GetArrayApi();
        if (!api || !api->ArraySetElementValue) {
            ReleaseArray(array);
            return BML_ERROR_INTEROP_UNSUPPORTED;
        }
        for (CKDWORD i = 0; i < count; ++i) {
            const std::string value(items && items[i] ? items[i] : "");
            status = MapArrayStatus(api->ArraySetElementValue(array, i, &value));
            if (status != BML_OK) {
                ReleaseArray(array);
                return status;
            }
        }
        status = AssignArrayHandle(array, values);
        ReleaseArray(array);
        return status;
    }

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

bool BMLAS_UI_OkButton(const std::string &label) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    return Bui::OkButton(label.c_str());
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

bool BMLAS_UI_KeyButton(const std::string &label, bool &toggled, int &keyChord) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    ImGuiKeyChord nativeChord = static_cast<ImGuiKeyChord>(keyChord);
    const bool changed = Bui::KeyButton(label.c_str(), &toggled, &nativeChord);
    keyChord = static_cast<int>(nativeChord);
    return changed;
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

bool BMLAS_UI_RadioButtonText(const std::string &label, int &currentItem, const std::string &itemsText) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    std::vector<std::string> items;
    std::size_t start = 0;
    while (start <= itemsText.size()) {
        const std::size_t end = itemsText.find('\n', start);
        std::string item = end == std::string::npos
                               ? itemsText.substr(start)
                               : itemsText.substr(start, end - start);
        if (!item.empty() && item.back() == '\r')
            item.pop_back();
        if (!item.empty())
            items.push_back(std::move(item));
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    if (items.empty())
        return false;

    std::vector<const char *> itemPointers;
    itemPointers.reserve(items.size());
    for (const std::string &item : items)
        itemPointers.push_back(item.c_str());
    return Bui::RadioButton(label.c_str(), &currentItem, itemPointers.data(), static_cast<int>(itemPointers.size()));
}

bool BMLAS_UI_InputTextButton(const std::string &label, std::string &text, int maxLength) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    if (maxLength < 1)
        maxLength = 1;
    if (maxLength > 4096)
        maxLength = 4096;
    std::vector<char> buffer(static_cast<std::size_t>(maxLength) + 1, '\0');
    std::strncpy(buffer.data(), text.c_str(), buffer.size() - 1);
    const bool changed = Bui::InputTextButton(label.c_str(), buffer.data(), buffer.size());
    if (changed)
        text = buffer.data();
    return changed;
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

int BMLAS_UI_CKKeyToImGuiKey(CKKEYBOARD key) {
    return static_cast<int>(Bui::CKKeyToImGuiKey(key));
}

CKKEYBOARD BMLAS_UI_ImGuiKeyToCKKey(int key) {
    return Bui::ImGuiKeyToCKKey(static_cast<ImGuiKey>(key));
}

std::string BMLAS_UI_KeyChordToString(int keyChord) {
    if (!BMLAS_UI_BeginRenderCall())
        return {};
    Bui::ImGuiContextScope scope;
    char buffer[128] = {};
    return Bui::KeyChordToString(static_cast<ImGuiKeyChord>(keyChord), buffer, sizeof(buffer)) ? buffer : "";
}

bool BMLAS_UI_SetKeyChordFromIO(int &keyChord) {
    if (!BMLAS_UI_BeginRenderCall())
        return false;
    Bui::ImGuiContextScope scope;
    ImGuiKeyChord nativeChord = static_cast<ImGuiKeyChord>(keyChord);
    const bool changed = Bui::SetKeyChordFromIO(&nativeChord);
    keyChord = static_cast<int>(nativeChord);
    return changed;
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

struct ScriptObjectPropertyRegistration {
    const char *TypeName;
    const char *Declaration;
    const char *DiagnosticName;
    int ByteOffset;
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

struct ScriptFuncdefRegistration {
    const char *Declaration;
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
    {"VxRect", "class VxRect", sizeof(BMLAS_VxRect), asOBJ_VALUE | asGetTypeTraits<BMLAS_VxRect>()},
    {"PhysicalizeDefinition", "class PhysicalizeDefinition", sizeof(BMLAS_PhysicalizeDefinition), asOBJ_VALUE | asGetTypeTraits<BMLAS_PhysicalizeDefinition>()},
    {"ObjectLoadOptions", "class ObjectLoadOptions", sizeof(BMLAS_ObjectLoadOptions), asOBJ_VALUE | asGetTypeTraits<BMLAS_ObjectLoadOptions>()},
    {"ObjectLoadResult", "class ObjectLoadResult", 0, asOBJ_REF},
    {"Logger", "class Logger", 0, asOBJ_REF},
    {"Config", "class Config", 0, asOBJ_REF},
    {"ConfigProperty", "class ConfigProperty", 0, asOBJ_REF},
    {"Text2DDefinition", "class Text2DDefinition", sizeof(BMLAS_Text2DDefinition), asOBJ_VALUE | asGetTypeTraits<BMLAS_Text2DDefinition>()},
    {"BallTypeDefinition", "class BallTypeDefinition", sizeof(BMLAS_BallTypeDefinition), asOBJ_VALUE | asGetTypeTraits<BMLAS_BallTypeDefinition>()},
    {"FloorTypeDefinition", "class FloorTypeDefinition", sizeof(BMLAS_FloorTypeDefinition), asOBJ_VALUE | asGetTypeTraits<BMLAS_FloorTypeDefinition>()},
    {"ModuleBallDefinition", "class ModuleBallDefinition", sizeof(BMLAS_ModuleBallDefinition), asOBJ_VALUE | asGetTypeTraits<BMLAS_ModuleBallDefinition>()},
    {"ModuleConvexDefinition", "class ModuleConvexDefinition", sizeof(BMLAS_ModuleConvexDefinition), asOBJ_VALUE | asGetTypeTraits<BMLAS_ModuleConvexDefinition>()},
    {"TrafoDefinition", "class TrafoDefinition", sizeof(BMLAS_TrafoDefinition), asOBJ_VALUE | asGetTypeTraits<BMLAS_TrafoDefinition>()},
    {"ModuleDefinition", "class ModuleDefinition", sizeof(BMLAS_ModuleDefinition), asOBJ_VALUE | asGetTypeTraits<BMLAS_ModuleDefinition>()},
    {"InputHook", "class InputHook", 0, asOBJ_REF | asOBJ_NOCOUNT},
    {"TimerRef", "class TimerRef", 0, asOBJ_REF},
    {"TimerEvent", "class TimerEvent", sizeof(BML::ScriptTimerEventView), asOBJ_VALUE | asGetTypeTraits<BML::ScriptTimerEventView>()},
    {"RenderEvent", "class RenderEvent", sizeof(BML::ScriptRenderEventView), asOBJ_VALUE | asGetTypeTraits<BML::ScriptRenderEventView>()},
    {"CheatEvent", "class CheatEvent", sizeof(BML::ScriptCheatEventView), asOBJ_VALUE | asGetTypeTraits<BML::ScriptCheatEventView>()},
    {"LoadObjectEvent", "class LoadObjectEvent", sizeof(BML::ScriptLoadObjectEventView), asOBJ_VALUE | asGetTypeTraits<BML::ScriptLoadObjectEventView>()},
    {"LoadScriptEvent", "class LoadScriptEvent", sizeof(BML::ScriptLoadScriptEventView), asOBJ_VALUE | asGetTypeTraits<BML::ScriptLoadScriptEventView>()},
    {"CommandEvent", "class CommandEvent", sizeof(BML::ScriptCommandEventView), asOBJ_VALUE | asGetTypeTraits<BML::ScriptCommandEventView>()},
    {"CommandDefinition", "class CommandDefinition", sizeof(BMLAS_CommandDefinition), asOBJ_VALUE | asGetTypeTraits<BMLAS_CommandDefinition>()},
    {"ConfigEvent", "class ConfigEvent", sizeof(BML::ScriptConfigEventView), asOBJ_VALUE | asGetTypeTraits<BML::ScriptConfigEventView>()},
    {"CommandCompletion", "class CommandCompletion", 0, asOBJ_REF | asOBJ_SCOPED},
    {"CommandRef", "class CommandRef", 0, asOBJ_REF},
    {"DataShareEvent", "class DataShareEvent", sizeof(BML::ScriptDataShareEventView), asOBJ_VALUE | asGetTypeTraits<BML::ScriptDataShareEventView>()},
    {"DataShareRequestRef", "class DataShareRequestRef", 0, asOBJ_REF},
    {"HookBlockEvent", "class HookBlockEvent", sizeof(BMLAS_HookBlockEvent), asOBJ_VALUE | asGetTypeTraits<BMLAS_HookBlockEvent>()},
    {"HookBlockRef", "class HookBlockRef", 0, asOBJ_REF},
    {"PhysicalizeEvent", "class PhysicalizeEvent", sizeof(BML::ScriptPhysicalizeEventView), asOBJ_VALUE | asGetTypeTraits<BML::ScriptPhysicalizeEventView>()},
    {"ObjectEvent", "class ObjectEvent", sizeof(BML::ScriptObjectEventView), asOBJ_VALUE | asGetTypeTraits<BML::ScriptObjectEventView>()},
    {"ModRef", "class ModRef", 0, asOBJ_REF},
    {"ExportRef", "class ExportRef", 0, asOBJ_REF},
    {"CallFrame", "class CallFrame", 0, asOBJ_REF},
    {"StateBag", "class StateBag", 0, asOBJ_REF},
};

static const ScriptInterfaceRegistration kInterfaceRegistrations[] = {
    {"Command", "interface Command"},
    {"Timer", "interface Timer"},
    {"DataShareRequest", "interface DataShareRequest"},
};

static const ScriptFuncdefRegistration kFuncdefRegistrations[] = {
    {"void TimerCallback(const BML::ModContext &in, const BML::TimerEvent &in)", "funcdef TimerCallback"},
    {"bool TimerLoopCallback(const BML::ModContext &in, const BML::TimerEvent &in)", "funcdef TimerLoopCallback"},
    {"void CommandCallback(const BML::ModContext &in, const BML::CommandEvent &in)", "funcdef CommandCallback"},
    {"void CommandCompletionCallback(const BML::ModContext &in, const BML::CommandEvent &in, BML::CommandCompletion &inout)", "funcdef CommandCompletionCallback"},
    {"void DataShareCallback(const BML::ModContext &in, const BML::DataShareEvent &in)", "funcdef DataShareCallback"},
    {"int HookBlockCallback(const BML::ModContext &in, const BML::HookBlockEvent &in)", "funcdef HookBlockCallback"},
};

static const ScriptInterfaceMethodRegistration kInterfaceMethodRegistrations[] = {
    {"Command", "string get_Name() const", "string Command::get_Name() const"},
    {"Command", "void Execute(const BML::ModContext &in, const BML::CommandEvent &in)", "void Command::Execute(...)"},
    {"Timer", "bool Tick(const BML::ModContext &in, const BML::TimerEvent &in)", "bool Timer::Tick(...)"},
    {"DataShareRequest", "string get_Key() const", "string DataShareRequest::get_Key() const"},
    {"DataShareRequest", "int get_Type() const", "int DataShareRequest::get_Type() const"},
    {"DataShareRequest", "void Receive(const BML::ModContext &in, const BML::DataShareEvent &in)", "void DataShareRequest::Receive(...)"},
};

static const ScriptObjectPropertyRegistration kObjectPropertyRegistrations[] = {
    {"VxRect", "float Left", "float VxRect::Left", asOFFSET(BMLAS_VxRect, Left)},
    {"VxRect", "float Top", "float VxRect::Top", asOFFSET(BMLAS_VxRect, Top)},
    {"VxRect", "float Right", "float VxRect::Right", asOFFSET(BMLAS_VxRect, Right)},
    {"VxRect", "float Bottom", "float VxRect::Bottom", asOFFSET(BMLAS_VxRect, Bottom)},
    {"PhysicalizeDefinition", "bool Fixed", "bool PhysicalizeDefinition::Fixed", asOFFSET(BMLAS_PhysicalizeDefinition, Fixed)},
    {"PhysicalizeDefinition", "float Friction", "float PhysicalizeDefinition::Friction", asOFFSET(BMLAS_PhysicalizeDefinition, Friction)},
    {"PhysicalizeDefinition", "float Elasticity", "float PhysicalizeDefinition::Elasticity", asOFFSET(BMLAS_PhysicalizeDefinition, Elasticity)},
    {"PhysicalizeDefinition", "float Mass", "float PhysicalizeDefinition::Mass", asOFFSET(BMLAS_PhysicalizeDefinition, Mass)},
    {"PhysicalizeDefinition", "string CollisionGroup", "string PhysicalizeDefinition::CollisionGroup", asOFFSET(BMLAS_PhysicalizeDefinition, CollisionGroup)},
    {"PhysicalizeDefinition", "bool StartFrozen", "bool PhysicalizeDefinition::StartFrozen", asOFFSET(BMLAS_PhysicalizeDefinition, StartFrozen)},
    {"PhysicalizeDefinition", "bool EnableCollision", "bool PhysicalizeDefinition::EnableCollision", asOFFSET(BMLAS_PhysicalizeDefinition, EnableCollision)},
    {"PhysicalizeDefinition", "bool CalcMassCenter", "bool PhysicalizeDefinition::CalcMassCenter", asOFFSET(BMLAS_PhysicalizeDefinition, CalcMassCenter)},
    {"PhysicalizeDefinition", "float LinearDamp", "float PhysicalizeDefinition::LinearDamp", asOFFSET(BMLAS_PhysicalizeDefinition, LinearDamp)},
    {"PhysicalizeDefinition", "float RotDamp", "float PhysicalizeDefinition::RotDamp", asOFFSET(BMLAS_PhysicalizeDefinition, RotDamp)},
    {"PhysicalizeDefinition", "string CollisionSurface", "string PhysicalizeDefinition::CollisionSurface", asOFFSET(BMLAS_PhysicalizeDefinition, CollisionSurface)},
    {"PhysicalizeDefinition", "VxVector MassCenter", "VxVector PhysicalizeDefinition::MassCenter", asOFFSET(BMLAS_PhysicalizeDefinition, MassCenter)},
    {"ObjectLoadOptions", "string File", "string ObjectLoadOptions::File", asOFFSET(BMLAS_ObjectLoadOptions, File)},
    {"ObjectLoadOptions", "bool Rename", "bool ObjectLoadOptions::Rename", asOFFSET(BMLAS_ObjectLoadOptions, Rename)},
    {"ObjectLoadOptions", "string MasterName", "string ObjectLoadOptions::MasterName", asOFFSET(BMLAS_ObjectLoadOptions, MasterName)},
    {"ObjectLoadOptions", "int FilterClass", "int ObjectLoadOptions::FilterClass", asOFFSET(BMLAS_ObjectLoadOptions, FilterClass)},
    {"ObjectLoadOptions", "bool AddToScene", "bool ObjectLoadOptions::AddToScene", asOFFSET(BMLAS_ObjectLoadOptions, AddToScene)},
    {"ObjectLoadOptions", "bool ReuseMeshes", "bool ObjectLoadOptions::ReuseMeshes", asOFFSET(BMLAS_ObjectLoadOptions, ReuseMeshes)},
    {"ObjectLoadOptions", "bool ReuseMaterials", "bool ObjectLoadOptions::ReuseMaterials", asOFFSET(BMLAS_ObjectLoadOptions, ReuseMaterials)},
    {"ObjectLoadOptions", "bool Dynamic", "bool ObjectLoadOptions::Dynamic", asOFFSET(BMLAS_ObjectLoadOptions, Dynamic)},
    {"Text2DDefinition", "FontType Font", "FontType Text2DDefinition::Font", asOFFSET(BMLAS_Text2DDefinition, Font)},
    {"Text2DDefinition", "string Text", "string Text2DDefinition::Text", asOFFSET(BMLAS_Text2DDefinition, Text)},
    {"Text2DDefinition", "int Align", "int Text2DDefinition::Align", asOFFSET(BMLAS_Text2DDefinition, Align)},
    {"Text2DDefinition", "VxRect Margin", "VxRect Text2DDefinition::Margin", asOFFSET(BMLAS_Text2DDefinition, Margin)},
    {"Text2DDefinition", "Vx2DVector Offset", "Vx2DVector Text2DDefinition::Offset", asOFFSET(BMLAS_Text2DDefinition, Offset)},
    {"Text2DDefinition", "Vx2DVector ParagraphIndent", "Vx2DVector Text2DDefinition::ParagraphIndent", asOFFSET(BMLAS_Text2DDefinition, ParagraphIndent)},
    {"Text2DDefinition", "float CaretSize", "float Text2DDefinition::CaretSize", asOFFSET(BMLAS_Text2DDefinition, CaretSize)},
    {"Text2DDefinition", "int Flags", "int Text2DDefinition::Flags", asOFFSET(BMLAS_Text2DDefinition, Flags)},
    {"BallTypeDefinition", "string BallFile", "string BallTypeDefinition::BallFile", asOFFSET(BMLAS_BallTypeDefinition, BallFile)},
    {"BallTypeDefinition", "string BallId", "string BallTypeDefinition::BallId", asOFFSET(BMLAS_BallTypeDefinition, BallId)},
    {"BallTypeDefinition", "string BallName", "string BallTypeDefinition::BallName", asOFFSET(BMLAS_BallTypeDefinition, BallName)},
    {"BallTypeDefinition", "string ObjectName", "string BallTypeDefinition::ObjectName", asOFFSET(BMLAS_BallTypeDefinition, ObjectName)},
    {"BallTypeDefinition", "float Friction", "float BallTypeDefinition::Friction", asOFFSET(BMLAS_BallTypeDefinition, Friction)},
    {"BallTypeDefinition", "float Elasticity", "float BallTypeDefinition::Elasticity", asOFFSET(BMLAS_BallTypeDefinition, Elasticity)},
    {"BallTypeDefinition", "float Mass", "float BallTypeDefinition::Mass", asOFFSET(BMLAS_BallTypeDefinition, Mass)},
    {"BallTypeDefinition", "string CollisionGroup", "string BallTypeDefinition::CollisionGroup", asOFFSET(BMLAS_BallTypeDefinition, CollisionGroup)},
    {"BallTypeDefinition", "float LinearDamp", "float BallTypeDefinition::LinearDamp", asOFFSET(BMLAS_BallTypeDefinition, LinearDamp)},
    {"BallTypeDefinition", "float RotDamp", "float BallTypeDefinition::RotDamp", asOFFSET(BMLAS_BallTypeDefinition, RotDamp)},
    {"BallTypeDefinition", "float Force", "float BallTypeDefinition::Force", asOFFSET(BMLAS_BallTypeDefinition, Force)},
    {"BallTypeDefinition", "float Radius", "float BallTypeDefinition::Radius", asOFFSET(BMLAS_BallTypeDefinition, Radius)},
    {"FloorTypeDefinition", "string Name", "string FloorTypeDefinition::Name", asOFFSET(BMLAS_FloorTypeDefinition, Name)},
    {"FloorTypeDefinition", "float Friction", "float FloorTypeDefinition::Friction", asOFFSET(BMLAS_FloorTypeDefinition, Friction)},
    {"FloorTypeDefinition", "float Elasticity", "float FloorTypeDefinition::Elasticity", asOFFSET(BMLAS_FloorTypeDefinition, Elasticity)},
    {"FloorTypeDefinition", "float Mass", "float FloorTypeDefinition::Mass", asOFFSET(BMLAS_FloorTypeDefinition, Mass)},
    {"FloorTypeDefinition", "string CollisionGroup", "string FloorTypeDefinition::CollisionGroup", asOFFSET(BMLAS_FloorTypeDefinition, CollisionGroup)},
    {"FloorTypeDefinition", "bool EnableCollision", "bool FloorTypeDefinition::EnableCollision", asOFFSET(BMLAS_FloorTypeDefinition, EnableCollision)},
    {"ModuleBallDefinition", "string Name", "string ModuleBallDefinition::Name", asOFFSET(BMLAS_ModuleBallDefinition, Name)},
    {"ModuleBallDefinition", "bool Fixed", "bool ModuleBallDefinition::Fixed", asOFFSET(BMLAS_ModuleBallDefinition, Fixed)},
    {"ModuleBallDefinition", "float Friction", "float ModuleBallDefinition::Friction", asOFFSET(BMLAS_ModuleBallDefinition, Friction)},
    {"ModuleBallDefinition", "float Elasticity", "float ModuleBallDefinition::Elasticity", asOFFSET(BMLAS_ModuleBallDefinition, Elasticity)},
    {"ModuleBallDefinition", "float Mass", "float ModuleBallDefinition::Mass", asOFFSET(BMLAS_ModuleBallDefinition, Mass)},
    {"ModuleBallDefinition", "string CollisionGroup", "string ModuleBallDefinition::CollisionGroup", asOFFSET(BMLAS_ModuleBallDefinition, CollisionGroup)},
    {"ModuleBallDefinition", "bool StartFrozen", "bool ModuleBallDefinition::StartFrozen", asOFFSET(BMLAS_ModuleBallDefinition, StartFrozen)},
    {"ModuleBallDefinition", "bool EnableCollision", "bool ModuleBallDefinition::EnableCollision", asOFFSET(BMLAS_ModuleBallDefinition, EnableCollision)},
    {"ModuleBallDefinition", "bool CalcMassCenter", "bool ModuleBallDefinition::CalcMassCenter", asOFFSET(BMLAS_ModuleBallDefinition, CalcMassCenter)},
    {"ModuleBallDefinition", "float LinearDamp", "float ModuleBallDefinition::LinearDamp", asOFFSET(BMLAS_ModuleBallDefinition, LinearDamp)},
    {"ModuleBallDefinition", "float RotDamp", "float ModuleBallDefinition::RotDamp", asOFFSET(BMLAS_ModuleBallDefinition, RotDamp)},
    {"ModuleBallDefinition", "float Radius", "float ModuleBallDefinition::Radius", asOFFSET(BMLAS_ModuleBallDefinition, Radius)},
    {"ModuleConvexDefinition", "string Name", "string ModuleConvexDefinition::Name", asOFFSET(BMLAS_ModuleConvexDefinition, Name)},
    {"ModuleConvexDefinition", "bool Fixed", "bool ModuleConvexDefinition::Fixed", asOFFSET(BMLAS_ModuleConvexDefinition, Fixed)},
    {"ModuleConvexDefinition", "float Friction", "float ModuleConvexDefinition::Friction", asOFFSET(BMLAS_ModuleConvexDefinition, Friction)},
    {"ModuleConvexDefinition", "float Elasticity", "float ModuleConvexDefinition::Elasticity", asOFFSET(BMLAS_ModuleConvexDefinition, Elasticity)},
    {"ModuleConvexDefinition", "float Mass", "float ModuleConvexDefinition::Mass", asOFFSET(BMLAS_ModuleConvexDefinition, Mass)},
    {"ModuleConvexDefinition", "string CollisionGroup", "string ModuleConvexDefinition::CollisionGroup", asOFFSET(BMLAS_ModuleConvexDefinition, CollisionGroup)},
    {"ModuleConvexDefinition", "bool StartFrozen", "bool ModuleConvexDefinition::StartFrozen", asOFFSET(BMLAS_ModuleConvexDefinition, StartFrozen)},
    {"ModuleConvexDefinition", "bool EnableCollision", "bool ModuleConvexDefinition::EnableCollision", asOFFSET(BMLAS_ModuleConvexDefinition, EnableCollision)},
    {"ModuleConvexDefinition", "bool CalcMassCenter", "bool ModuleConvexDefinition::CalcMassCenter", asOFFSET(BMLAS_ModuleConvexDefinition, CalcMassCenter)},
    {"ModuleConvexDefinition", "float LinearDamp", "float ModuleConvexDefinition::LinearDamp", asOFFSET(BMLAS_ModuleConvexDefinition, LinearDamp)},
    {"ModuleConvexDefinition", "float RotDamp", "float ModuleConvexDefinition::RotDamp", asOFFSET(BMLAS_ModuleConvexDefinition, RotDamp)},
    {"TrafoDefinition", "string Name", "string TrafoDefinition::Name", asOFFSET(BMLAS_TrafoDefinition, Name)},
    {"ModuleDefinition", "string Name", "string ModuleDefinition::Name", asOFFSET(BMLAS_ModuleDefinition, Name)},
    {"CommandDefinition", "string Name", "string CommandDefinition::Name", asOFFSET(BMLAS_CommandDefinition, Name)},
    {"CommandDefinition", "string Alias", "string CommandDefinition::Alias", asOFFSET(BMLAS_CommandDefinition, Alias)},
    {"CommandDefinition", "string Description", "string CommandDefinition::Description", asOFFSET(BMLAS_CommandDefinition, Description)},
    {"CommandDefinition", "string Usage", "string CommandDefinition::Usage", asOFFSET(BMLAS_CommandDefinition, Usage)},
    {"CommandDefinition", "string Category", "string CommandDefinition::Category", asOFFSET(BMLAS_CommandDefinition, Category)},
    {"CommandDefinition", "bool Cheat", "bool CommandDefinition::Cheat", asOFFSET(BMLAS_CommandDefinition, Cheat)},
    {"CommandDefinition", "bool Hidden", "bool CommandDefinition::Hidden", asOFFSET(BMLAS_CommandDefinition, Hidden)},
    {"CommandDefinition", "bool Enabled", "bool CommandDefinition::Enabled", asOFFSET(BMLAS_CommandDefinition, Enabled)},
};

static const ScriptObjectBehaviourRegistration kObjectBehaviourRegistrations[] = {
    {"ModContext", asBEHAVE_FACTORY, "ModContext@ f()", "ModContext@ ModContext factory", asFUNCTION(BMLAS_CreateInvalidModContext), asCALL_CDECL},
    {"ModContext", asBEHAVE_RELEASE, "void f()", "void ModContext release", asFUNCTION(BMLAS_ReleaseModContext), asCALL_CDECL_OBJLAST},
    {"VxRect", asBEHAVE_CONSTRUCT, "void f()", "void VxRect default construct", asFUNCTION(BMLAS_ConstructVxRect), asCALL_CDECL_OBJLAST},
    {"VxRect", asBEHAVE_CONSTRUCT, "void f(const VxRect &in)", "void VxRect copy construct", asFUNCTION(BMLAS_CopyConstructVxRect), asCALL_CDECL_OBJLAST},
    {"VxRect", asBEHAVE_DESTRUCT, "void f()", "void VxRect destruct", asFUNCTION(BMLAS_DestructVxRect), asCALL_CDECL_OBJLAST},
    {"PhysicalizeDefinition", asBEHAVE_CONSTRUCT, "void f()", "void PhysicalizeDefinition default construct", asFUNCTION(BMLAS_ConstructPhysicalizeDefinition), asCALL_CDECL_OBJLAST},
    {"PhysicalizeDefinition", asBEHAVE_CONSTRUCT, "void f(const PhysicalizeDefinition &in)", "void PhysicalizeDefinition copy construct", asFUNCTION(BMLAS_CopyConstructPhysicalizeDefinition), asCALL_CDECL_OBJLAST},
    {"PhysicalizeDefinition", asBEHAVE_DESTRUCT, "void f()", "void PhysicalizeDefinition destruct", asFUNCTION(BMLAS_DestructPhysicalizeDefinition), asCALL_CDECL_OBJLAST},
    {"ObjectLoadOptions", asBEHAVE_CONSTRUCT, "void f()", "void ObjectLoadOptions default construct", asFUNCTION(BMLAS_ConstructObjectLoadOptions), asCALL_CDECL_OBJLAST},
    {"ObjectLoadOptions", asBEHAVE_CONSTRUCT, "void f(const ObjectLoadOptions &in)", "void ObjectLoadOptions copy construct", asFUNCTION(BMLAS_CopyConstructObjectLoadOptions), asCALL_CDECL_OBJLAST},
    {"ObjectLoadOptions", asBEHAVE_DESTRUCT, "void f()", "void ObjectLoadOptions destruct", asFUNCTION(BMLAS_DestructObjectLoadOptions), asCALL_CDECL_OBJLAST},
    {"ObjectLoadResult", asBEHAVE_ADDREF, "void f()", "void ObjectLoadResult addref", asMETHOD(BMLAS_ObjectLoadResult, AddRef), asCALL_THISCALL},
    {"ObjectLoadResult", asBEHAVE_RELEASE, "void f()", "void ObjectLoadResult release", asMETHOD(BMLAS_ObjectLoadResult, Release), asCALL_THISCALL},
    {"Logger", asBEHAVE_ADDREF, "void f()", "void Logger addref", asMETHOD(BMLAS_LoggerRef, AddRef), asCALL_THISCALL},
    {"Logger", asBEHAVE_RELEASE, "void f()", "void Logger release", asMETHOD(BMLAS_LoggerRef, Release), asCALL_THISCALL},
    {"Config", asBEHAVE_ADDREF, "void f()", "void Config addref", asMETHOD(BMLAS_ConfigRef, AddRef), asCALL_THISCALL},
    {"Config", asBEHAVE_RELEASE, "void f()", "void Config release", asMETHOD(BMLAS_ConfigRef, Release), asCALL_THISCALL},
    {"ConfigProperty", asBEHAVE_ADDREF, "void f()", "void ConfigProperty addref", asMETHOD(BMLAS_ConfigPropertyRef, AddRef), asCALL_THISCALL},
    {"ConfigProperty", asBEHAVE_RELEASE, "void f()", "void ConfigProperty release", asMETHOD(BMLAS_ConfigPropertyRef, Release), asCALL_THISCALL},
    {"Text2DDefinition", asBEHAVE_CONSTRUCT, "void f()", "void Text2DDefinition default construct", asFUNCTION(BMLAS_ConstructText2DDefinition), asCALL_CDECL_OBJLAST},
    {"Text2DDefinition", asBEHAVE_CONSTRUCT, "void f(const Text2DDefinition &in)", "void Text2DDefinition copy construct", asFUNCTION(BMLAS_CopyConstructText2DDefinition), asCALL_CDECL_OBJLAST},
    {"Text2DDefinition", asBEHAVE_DESTRUCT, "void f()", "void Text2DDefinition destruct", asFUNCTION(BMLAS_DestructText2DDefinition), asCALL_CDECL_OBJLAST},
    {"BallTypeDefinition", asBEHAVE_CONSTRUCT, "void f()", "void BallTypeDefinition default construct", asFUNCTION(BMLAS_ConstructBallTypeDefinition), asCALL_CDECL_OBJLAST},
    {"BallTypeDefinition", asBEHAVE_CONSTRUCT, "void f(const BallTypeDefinition &in)", "void BallTypeDefinition copy construct", asFUNCTION(BMLAS_CopyConstructBallTypeDefinition), asCALL_CDECL_OBJLAST},
    {"BallTypeDefinition", asBEHAVE_DESTRUCT, "void f()", "void BallTypeDefinition destruct", asFUNCTION(BMLAS_DestructBallTypeDefinition), asCALL_CDECL_OBJLAST},
    {"FloorTypeDefinition", asBEHAVE_CONSTRUCT, "void f()", "void FloorTypeDefinition default construct", asFUNCTION(BMLAS_ConstructFloorTypeDefinition), asCALL_CDECL_OBJLAST},
    {"FloorTypeDefinition", asBEHAVE_CONSTRUCT, "void f(const FloorTypeDefinition &in)", "void FloorTypeDefinition copy construct", asFUNCTION(BMLAS_CopyConstructFloorTypeDefinition), asCALL_CDECL_OBJLAST},
    {"FloorTypeDefinition", asBEHAVE_DESTRUCT, "void f()", "void FloorTypeDefinition destruct", asFUNCTION(BMLAS_DestructFloorTypeDefinition), asCALL_CDECL_OBJLAST},
    {"ModuleBallDefinition", asBEHAVE_CONSTRUCT, "void f()", "void ModuleBallDefinition default construct", asFUNCTION(BMLAS_ConstructModuleBallDefinition), asCALL_CDECL_OBJLAST},
    {"ModuleBallDefinition", asBEHAVE_CONSTRUCT, "void f(const ModuleBallDefinition &in)", "void ModuleBallDefinition copy construct", asFUNCTION(BMLAS_CopyConstructModuleBallDefinition), asCALL_CDECL_OBJLAST},
    {"ModuleBallDefinition", asBEHAVE_DESTRUCT, "void f()", "void ModuleBallDefinition destruct", asFUNCTION(BMLAS_DestructModuleBallDefinition), asCALL_CDECL_OBJLAST},
    {"ModuleConvexDefinition", asBEHAVE_CONSTRUCT, "void f()", "void ModuleConvexDefinition default construct", asFUNCTION(BMLAS_ConstructModuleConvexDefinition), asCALL_CDECL_OBJLAST},
    {"ModuleConvexDefinition", asBEHAVE_CONSTRUCT, "void f(const ModuleConvexDefinition &in)", "void ModuleConvexDefinition copy construct", asFUNCTION(BMLAS_CopyConstructModuleConvexDefinition), asCALL_CDECL_OBJLAST},
    {"ModuleConvexDefinition", asBEHAVE_DESTRUCT, "void f()", "void ModuleConvexDefinition destruct", asFUNCTION(BMLAS_DestructModuleConvexDefinition), asCALL_CDECL_OBJLAST},
    {"TrafoDefinition", asBEHAVE_CONSTRUCT, "void f()", "void TrafoDefinition default construct", asFUNCTION(BMLAS_ConstructTrafoDefinition), asCALL_CDECL_OBJLAST},
    {"TrafoDefinition", asBEHAVE_CONSTRUCT, "void f(const TrafoDefinition &in)", "void TrafoDefinition copy construct", asFUNCTION(BMLAS_CopyConstructTrafoDefinition), asCALL_CDECL_OBJLAST},
    {"TrafoDefinition", asBEHAVE_DESTRUCT, "void f()", "void TrafoDefinition destruct", asFUNCTION(BMLAS_DestructTrafoDefinition), asCALL_CDECL_OBJLAST},
    {"ModuleDefinition", asBEHAVE_CONSTRUCT, "void f()", "void ModuleDefinition default construct", asFUNCTION(BMLAS_ConstructModuleDefinition), asCALL_CDECL_OBJLAST},
    {"ModuleDefinition", asBEHAVE_CONSTRUCT, "void f(const ModuleDefinition &in)", "void ModuleDefinition copy construct", asFUNCTION(BMLAS_CopyConstructModuleDefinition), asCALL_CDECL_OBJLAST},
    {"ModuleDefinition", asBEHAVE_DESTRUCT, "void f()", "void ModuleDefinition destruct", asFUNCTION(BMLAS_DestructModuleDefinition), asCALL_CDECL_OBJLAST},
    {"CommandDefinition", asBEHAVE_CONSTRUCT, "void f()", "void CommandDefinition default construct", asFUNCTION(BMLAS_ConstructCommandDefinition), asCALL_CDECL_OBJLAST},
    {"CommandDefinition", asBEHAVE_CONSTRUCT, "void f(const CommandDefinition &in)", "void CommandDefinition copy construct", asFUNCTION(BMLAS_CopyConstructCommandDefinition), asCALL_CDECL_OBJLAST},
    {"CommandDefinition", asBEHAVE_DESTRUCT, "void f()", "void CommandDefinition destruct", asFUNCTION(BMLAS_DestructCommandDefinition), asCALL_CDECL_OBJLAST},
    {"TimerEvent", asBEHAVE_CONSTRUCT, "void f()", "void TimerEvent default construct", asFUNCTION(BMLAS_ConstructTimerEvent), asCALL_CDECL_OBJLAST},
    {"TimerEvent", asBEHAVE_CONSTRUCT, "void f(const TimerEvent &in)", "void TimerEvent copy construct", asFUNCTION(BMLAS_CopyConstructTimerEvent), asCALL_CDECL_OBJLAST},
    {"TimerEvent", asBEHAVE_DESTRUCT, "void f()", "void TimerEvent destruct", asFUNCTION(BMLAS_DestructTimerEvent), asCALL_CDECL_OBJLAST},
    {"RenderEvent", asBEHAVE_CONSTRUCT, "void f()", "void RenderEvent default construct", asFUNCTION(BMLAS_ConstructRenderEvent), asCALL_CDECL_OBJLAST},
    {"RenderEvent", asBEHAVE_CONSTRUCT, "void f(const RenderEvent &in)", "void RenderEvent copy construct", asFUNCTION(BMLAS_CopyConstructRenderEvent), asCALL_CDECL_OBJLAST},
    {"RenderEvent", asBEHAVE_DESTRUCT, "void f()", "void RenderEvent destruct", asFUNCTION(BMLAS_DestructRenderEvent), asCALL_CDECL_OBJLAST},
    {"CheatEvent", asBEHAVE_CONSTRUCT, "void f()", "void CheatEvent default construct", asFUNCTION(BMLAS_ConstructCheatEvent), asCALL_CDECL_OBJLAST},
    {"CheatEvent", asBEHAVE_CONSTRUCT, "void f(const CheatEvent &in)", "void CheatEvent copy construct", asFUNCTION(BMLAS_CopyConstructCheatEvent), asCALL_CDECL_OBJLAST},
    {"CheatEvent", asBEHAVE_DESTRUCT, "void f()", "void CheatEvent destruct", asFUNCTION(BMLAS_DestructCheatEvent), asCALL_CDECL_OBJLAST},
    {"LoadObjectEvent", asBEHAVE_CONSTRUCT, "void f()", "void LoadObjectEvent default construct", asFUNCTION(BMLAS_ConstructLoadObjectEvent), asCALL_CDECL_OBJLAST},
    {"LoadObjectEvent", asBEHAVE_CONSTRUCT, "void f(const LoadObjectEvent &in)", "void LoadObjectEvent copy construct", asFUNCTION(BMLAS_CopyConstructLoadObjectEvent), asCALL_CDECL_OBJLAST},
    {"LoadObjectEvent", asBEHAVE_DESTRUCT, "void f()", "void LoadObjectEvent destruct", asFUNCTION(BMLAS_DestructLoadObjectEvent), asCALL_CDECL_OBJLAST},
    {"LoadScriptEvent", asBEHAVE_CONSTRUCT, "void f()", "void LoadScriptEvent default construct", asFUNCTION(BMLAS_ConstructLoadScriptEvent), asCALL_CDECL_OBJLAST},
    {"LoadScriptEvent", asBEHAVE_CONSTRUCT, "void f(const LoadScriptEvent &in)", "void LoadScriptEvent copy construct", asFUNCTION(BMLAS_CopyConstructLoadScriptEvent), asCALL_CDECL_OBJLAST},
    {"LoadScriptEvent", asBEHAVE_DESTRUCT, "void f()", "void LoadScriptEvent destruct", asFUNCTION(BMLAS_DestructLoadScriptEvent), asCALL_CDECL_OBJLAST},
    {"CommandEvent", asBEHAVE_CONSTRUCT, "void f()", "void CommandEvent default construct", asFUNCTION(BMLAS_ConstructCommandEvent), asCALL_CDECL_OBJLAST},
    {"CommandEvent", asBEHAVE_CONSTRUCT, "void f(const CommandEvent &in)", "void CommandEvent copy construct", asFUNCTION(BMLAS_CopyConstructCommandEvent), asCALL_CDECL_OBJLAST},
    {"CommandEvent", asBEHAVE_DESTRUCT, "void f()", "void CommandEvent destruct", asFUNCTION(BMLAS_DestructCommandEvent), asCALL_CDECL_OBJLAST},
    {"ConfigEvent", asBEHAVE_CONSTRUCT, "void f()", "void ConfigEvent default construct", asFUNCTION(BMLAS_ConstructConfigEvent), asCALL_CDECL_OBJLAST},
    {"ConfigEvent", asBEHAVE_CONSTRUCT, "void f(const ConfigEvent &in)", "void ConfigEvent copy construct", asFUNCTION(BMLAS_CopyConstructConfigEvent), asCALL_CDECL_OBJLAST},
    {"ConfigEvent", asBEHAVE_DESTRUCT, "void f()", "void ConfigEvent destruct", asFUNCTION(BMLAS_DestructConfigEvent), asCALL_CDECL_OBJLAST},
    {"DataShareEvent", asBEHAVE_CONSTRUCT, "void f()", "void DataShareEvent default construct", asFUNCTION(BMLAS_ConstructDataShareEvent), asCALL_CDECL_OBJLAST},
    {"DataShareEvent", asBEHAVE_CONSTRUCT, "void f(const DataShareEvent &in)", "void DataShareEvent copy construct", asFUNCTION(BMLAS_CopyConstructDataShareEvent), asCALL_CDECL_OBJLAST},
    {"DataShareEvent", asBEHAVE_DESTRUCT, "void f()", "void DataShareEvent destruct", asFUNCTION(BMLAS_DestructDataShareEvent), asCALL_CDECL_OBJLAST},
    {"HookBlockEvent", asBEHAVE_CONSTRUCT, "void f()", "void HookBlockEvent default construct", asFUNCTION(BMLAS_ConstructHookBlockEvent), asCALL_CDECL_OBJLAST},
    {"HookBlockEvent", asBEHAVE_CONSTRUCT, "void f(const HookBlockEvent &in)", "void HookBlockEvent copy construct", asFUNCTION(BMLAS_CopyConstructHookBlockEvent), asCALL_CDECL_OBJLAST},
    {"HookBlockEvent", asBEHAVE_DESTRUCT, "void f()", "void HookBlockEvent destruct", asFUNCTION(BMLAS_DestructHookBlockEvent), asCALL_CDECL_OBJLAST},
    {"PhysicalizeEvent", asBEHAVE_CONSTRUCT, "void f()", "void PhysicalizeEvent default construct", asFUNCTION(BMLAS_ConstructPhysicalizeEvent), asCALL_CDECL_OBJLAST},
    {"PhysicalizeEvent", asBEHAVE_CONSTRUCT, "void f(const PhysicalizeEvent &in)", "void PhysicalizeEvent copy construct", asFUNCTION(BMLAS_CopyConstructPhysicalizeEvent), asCALL_CDECL_OBJLAST},
    {"PhysicalizeEvent", asBEHAVE_DESTRUCT, "void f()", "void PhysicalizeEvent destruct", asFUNCTION(BMLAS_DestructPhysicalizeEvent), asCALL_CDECL_OBJLAST},
    {"ObjectEvent", asBEHAVE_CONSTRUCT, "void f()", "void ObjectEvent default construct", asFUNCTION(BMLAS_ConstructObjectEvent), asCALL_CDECL_OBJLAST},
    {"ObjectEvent", asBEHAVE_CONSTRUCT, "void f(const ObjectEvent &in)", "void ObjectEvent copy construct", asFUNCTION(BMLAS_CopyConstructObjectEvent), asCALL_CDECL_OBJLAST},
    {"ObjectEvent", asBEHAVE_DESTRUCT, "void f()", "void ObjectEvent destruct", asFUNCTION(BMLAS_DestructObjectEvent), asCALL_CDECL_OBJLAST},
    {"CommandCompletion", asBEHAVE_FACTORY, "CommandCompletion@ f()", "CommandCompletion@ factory", asFUNCTION(BMLAS_CreateInvalidCommandCompletion), asCALL_CDECL},
    {"CommandCompletion", asBEHAVE_RELEASE, "void f()", "void CommandCompletion release", asFUNCTION(BMLAS_ReleaseCommandCompletion), asCALL_CDECL_OBJLAST},
    {"CommandRef", asBEHAVE_ADDREF, "void f()", "void CommandRef addref", asMETHOD(BML::ScriptCommandRef, AddRef), asCALL_THISCALL},
    {"CommandRef", asBEHAVE_RELEASE, "void f()", "void CommandRef release", asMETHOD(BML::ScriptCommandRef, Release), asCALL_THISCALL},
    {"DataShareRequestRef", asBEHAVE_ADDREF, "void f()", "void DataShareRequestRef addref", asMETHOD(BML::ScriptDataShareRequestRef, AddRef), asCALL_THISCALL},
    {"DataShareRequestRef", asBEHAVE_RELEASE, "void f()", "void DataShareRequestRef release", asMETHOD(BML::ScriptDataShareRequestRef, Release), asCALL_THISCALL},
    {"HookBlockRef", asBEHAVE_ADDREF, "void f()", "void HookBlockRef addref", asMETHOD(BML::ScriptHookBlockRef, AddRef), asCALL_THISCALL},
    {"HookBlockRef", asBEHAVE_RELEASE, "void f()", "void HookBlockRef release", asMETHOD(BML::ScriptHookBlockRef, Release), asCALL_THISCALL},
    {"TimerRef", asBEHAVE_ADDREF, "void f()", "void TimerRef addref", asMETHOD(BML::ScriptTimerRef, AddRef), asCALL_THISCALL},
    {"TimerRef", asBEHAVE_RELEASE, "void f()", "void TimerRef release", asMETHOD(BML::ScriptTimerRef, Release), asCALL_THISCALL},
    {"CallFrame", asBEHAVE_FACTORY, "CallFrame@ f()", "CallFrame@ CallFrame factory", asFUNCTION(BMLAS_CreateCallFrame), asCALL_CDECL},
    {"CallFrame", asBEHAVE_ADDREF, "void f()", "void CallFrame addref", asMETHOD(BMLAS_CallFrame, AddRef), asCALL_THISCALL},
    {"CallFrame", asBEHAVE_RELEASE, "void f()", "void CallFrame release", asMETHOD(BMLAS_CallFrame, Release), asCALL_THISCALL},
    {"StateBag", asBEHAVE_FACTORY, "StateBag@ f()", "StateBag@ StateBag factory", asFUNCTION(BMLAS_CreateStateBag), asCALL_CDECL},
    {"StateBag", asBEHAVE_ADDREF, "void f()", "void StateBag addref", asMETHOD(BML::ScriptStateBag, AddRef), asCALL_THISCALL},
    {"StateBag", asBEHAVE_RELEASE, "void f()", "void StateBag release", asMETHOD(BML::ScriptStateBag, Release), asCALL_THISCALL},
    {"ModRef", asBEHAVE_ADDREF, "void f()", "void ModRef addref", asMETHOD(BMLAS_ModRef, AddRef), asCALL_THISCALL},
    {"ModRef", asBEHAVE_RELEASE, "void f()", "void ModRef release", asMETHOD(BMLAS_ModRef, Release), asCALL_THISCALL},
    {"ExportRef", asBEHAVE_ADDREF, "void f()", "void ExportRef addref", asMETHOD(BMLAS_ExportRef, AddRef), asCALL_THISCALL},
    {"ExportRef", asBEHAVE_RELEASE, "void f()", "void ExportRef release", asMETHOD(BMLAS_ExportRef, Release), asCALL_THISCALL},
};

static const ScriptObjectMethodRegistration kObjectMethodRegistrations[] = {
    {"VxRect", "VxRect &opAssign(const VxRect &in)", "VxRect &VxRect::opAssign(const VxRect &in)", asFUNCTION(BMLAS_AssignVxRect), asCALL_CDECL_OBJLAST},
    {"PhysicalizeDefinition", "PhysicalizeDefinition &opAssign(const PhysicalizeDefinition &in)", "PhysicalizeDefinition &PhysicalizeDefinition::opAssign(const PhysicalizeDefinition &in)", asFUNCTION(BMLAS_AssignPhysicalizeDefinition), asCALL_CDECL_OBJLAST},
    {"ObjectLoadOptions", "ObjectLoadOptions &opAssign(const ObjectLoadOptions &in)", "ObjectLoadOptions &ObjectLoadOptions::opAssign(const ObjectLoadOptions &in)", asFUNCTION(BMLAS_AssignObjectLoadOptions), asCALL_CDECL_OBJLAST},
    {"TimerEvent", "TimerEvent &opAssign(const TimerEvent &in)", "TimerEvent &TimerEvent::opAssign(const TimerEvent &in)", asFUNCTION(BMLAS_AssignTimerEvent), asCALL_CDECL_OBJLAST},
    {"RenderEvent", "RenderEvent &opAssign(const RenderEvent &in)", "RenderEvent &RenderEvent::opAssign(const RenderEvent &in)", asFUNCTION(BMLAS_AssignRenderEvent), asCALL_CDECL_OBJLAST},
    {"CheatEvent", "CheatEvent &opAssign(const CheatEvent &in)", "CheatEvent &CheatEvent::opAssign(const CheatEvent &in)", asFUNCTION(BMLAS_AssignCheatEvent), asCALL_CDECL_OBJLAST},
    {"LoadObjectEvent", "LoadObjectEvent &opAssign(const LoadObjectEvent &in)", "LoadObjectEvent &LoadObjectEvent::opAssign(const LoadObjectEvent &in)", asFUNCTION(BMLAS_AssignLoadObjectEvent), asCALL_CDECL_OBJLAST},
    {"LoadScriptEvent", "LoadScriptEvent &opAssign(const LoadScriptEvent &in)", "LoadScriptEvent &LoadScriptEvent::opAssign(const LoadScriptEvent &in)", asFUNCTION(BMLAS_AssignLoadScriptEvent), asCALL_CDECL_OBJLAST},
    {"CommandEvent", "CommandEvent &opAssign(const CommandEvent &in)", "CommandEvent &CommandEvent::opAssign(const CommandEvent &in)", asFUNCTION(BMLAS_AssignCommandEvent), asCALL_CDECL_OBJLAST},
    {"ConfigEvent", "ConfigEvent &opAssign(const ConfigEvent &in)", "ConfigEvent &ConfigEvent::opAssign(const ConfigEvent &in)", asFUNCTION(BMLAS_AssignConfigEvent), asCALL_CDECL_OBJLAST},
    {"DataShareEvent", "DataShareEvent &opAssign(const DataShareEvent &in)", "DataShareEvent &DataShareEvent::opAssign(const DataShareEvent &in)", asFUNCTION(BMLAS_AssignDataShareEvent), asCALL_CDECL_OBJLAST},
    {"HookBlockEvent", "HookBlockEvent &opAssign(const HookBlockEvent &in)", "HookBlockEvent &HookBlockEvent::opAssign(const HookBlockEvent &in)", asFUNCTION(BMLAS_AssignHookBlockEvent), asCALL_CDECL_OBJLAST},
    {"PhysicalizeEvent", "PhysicalizeEvent &opAssign(const PhysicalizeEvent &in)", "PhysicalizeEvent &PhysicalizeEvent::opAssign(const PhysicalizeEvent &in)", asFUNCTION(BMLAS_AssignPhysicalizeEvent), asCALL_CDECL_OBJLAST},
    {"ObjectEvent", "ObjectEvent &opAssign(const ObjectEvent &in)", "ObjectEvent &ObjectEvent::opAssign(const ObjectEvent &in)", asFUNCTION(BMLAS_AssignObjectEvent), asCALL_CDECL_OBJLAST},
    {"ObjectLoadResult", "bool get_Success() const", "bool ObjectLoadResult::get_Success() const", asMETHOD(BMLAS_ObjectLoadResult, IsSuccess), asCALL_THISCALL},
    {"ObjectLoadResult", "int get_Count() const", "int ObjectLoadResult::get_Count() const", asMETHOD(BMLAS_ObjectLoadResult, GetCount), asCALL_THISCALL},
    {"ObjectLoadResult", "CKObject@ BorrowMainObject() const", "CKObject@ ObjectLoadResult::BorrowMainObject() const", asMETHOD(BMLAS_ObjectLoadResult, BorrowMainObject), asCALL_THISCALL},
    {"ObjectLoadResult", "CKObject@ BorrowObject(int index) const", "CKObject@ ObjectLoadResult::BorrowObject(int index) const", asMETHOD(BMLAS_ObjectLoadResult, BorrowObject), asCALL_THISCALL},
    {"Logger", "bool get_IsValid() const", "bool Logger::get_IsValid() const", asMETHOD(BMLAS_LoggerRef, IsValid), asCALL_THISCALL},
    {"Logger", "bool IsValid() const", "bool Logger::IsValid() const", asMETHOD(BMLAS_LoggerRef, IsValid), asCALL_THISCALL},
    {"Logger", "void Info(const string &in message) const", "void Logger::Info(const string &in message) const", asMETHOD(BMLAS_LoggerRef, Info), asCALL_THISCALL},
    {"Logger", "void Warn(const string &in message) const", "void Logger::Warn(const string &in message) const", asMETHOD(BMLAS_LoggerRef, Warn), asCALL_THISCALL},
    {"Logger", "void Error(const string &in message) const", "void Logger::Error(const string &in message) const", asMETHOD(BMLAS_LoggerRef, Error), asCALL_THISCALL},
    {"Config", "bool get_IsValid() const", "bool Config::get_IsValid() const", asMETHOD(BMLAS_ConfigRef, IsValid), asCALL_THISCALL},
    {"Config", "bool IsValid() const", "bool Config::IsValid() const", asMETHOD(BMLAS_ConfigRef, IsValid), asCALL_THISCALL},
    {"Config", "bool HasCategory(const string &in category) const", "bool Config::HasCategory(const string &in category) const", asMETHOD(BMLAS_ConfigRef, HasCategory), asCALL_THISCALL},
    {"Config", "bool HasKey(const string &in category, const string &in key) const", "bool Config::HasKey(const string &in category, const string &in key) const", asMETHOD(BMLAS_ConfigRef, HasKey), asCALL_THISCALL},
    {"Config", "ConfigProperty@ GetProperty(const string &in category, const string &in key) const", "ConfigProperty@ Config::GetProperty(const string &in category, const string &in key) const", asMETHOD(BMLAS_ConfigRef, GetProperty), asCALL_THISCALL},
    {"Config", "void SetCategoryComment(const string &in category, const string &in comment) const", "void Config::SetCategoryComment(const string &in category, const string &in comment) const", asMETHOD(BMLAS_ConfigRef, SetCategoryComment), asCALL_THISCALL},
    {"ConfigProperty", "bool get_IsValid() const", "bool ConfigProperty::get_IsValid() const", asMETHOD(BMLAS_ConfigPropertyRef, IsValid), asCALL_THISCALL},
    {"ConfigProperty", "bool IsValid() const", "bool ConfigProperty::IsValid() const", asMETHOD(BMLAS_ConfigPropertyRef, IsValid), asCALL_THISCALL},
    {"ConfigProperty", "ConfigPropertyType get_Type() const", "ConfigPropertyType ConfigProperty::get_Type() const", asMETHOD(BMLAS_ConfigPropertyRef, GetType), asCALL_THISCALL},
    {"ConfigProperty", "ConfigPropertyType GetType() const", "ConfigPropertyType ConfigProperty::GetType() const", asMETHOD(BMLAS_ConfigPropertyRef, GetType), asCALL_THISCALL},
    {"ConfigProperty", "string GetString(const string &in defaultValue = \"\") const", "string ConfigProperty::GetString(const string &in defaultValue) const", asMETHOD(BMLAS_ConfigPropertyRef, GetString), asCALL_THISCALL},
    {"ConfigProperty", "bool GetBoolean(bool defaultValue = false) const", "bool ConfigProperty::GetBoolean(bool defaultValue) const", asMETHOD(BMLAS_ConfigPropertyRef, GetBoolean), asCALL_THISCALL},
    {"ConfigProperty", "int GetInteger(int defaultValue = 0) const", "int ConfigProperty::GetInteger(int defaultValue) const", asMETHOD(BMLAS_ConfigPropertyRef, GetInteger), asCALL_THISCALL},
    {"ConfigProperty", "float GetFloat(float defaultValue = 0.0f) const", "float ConfigProperty::GetFloat(float defaultValue) const", asMETHOD(BMLAS_ConfigPropertyRef, GetFloat), asCALL_THISCALL},
    {"ConfigProperty", "CKKEYBOARD GetKey(CKKEYBOARD defaultValue = 0) const", "CKKEYBOARD ConfigProperty::GetKey(CKKEYBOARD defaultValue) const", asMETHOD(BMLAS_ConfigPropertyRef, GetKey), asCALL_THISCALL},
    {"ConfigProperty", "void SetString(const string &in value) const", "void ConfigProperty::SetString(const string &in value) const", asMETHOD(BMLAS_ConfigPropertyRef, SetString), asCALL_THISCALL},
    {"ConfigProperty", "void SetBoolean(bool value) const", "void ConfigProperty::SetBoolean(bool value) const", asMETHOD(BMLAS_ConfigPropertyRef, SetBoolean), asCALL_THISCALL},
    {"ConfigProperty", "void SetInteger(int value) const", "void ConfigProperty::SetInteger(int value) const", asMETHOD(BMLAS_ConfigPropertyRef, SetInteger), asCALL_THISCALL},
    {"ConfigProperty", "void SetFloat(float value) const", "void ConfigProperty::SetFloat(float value) const", asMETHOD(BMLAS_ConfigPropertyRef, SetFloat), asCALL_THISCALL},
    {"ConfigProperty", "void SetKey(CKKEYBOARD value) const", "void ConfigProperty::SetKey(CKKEYBOARD value) const", asMETHOD(BMLAS_ConfigPropertyRef, SetKey), asCALL_THISCALL},
    {"ConfigProperty", "void SetComment(const string &in comment) const", "void ConfigProperty::SetComment(const string &in comment) const", asMETHOD(BMLAS_ConfigPropertyRef, SetComment), asCALL_THISCALL},
    {"ConfigProperty", "void SetDefaultString(const string &in value) const", "void ConfigProperty::SetDefaultString(const string &in value) const", asMETHOD(BMLAS_ConfigPropertyRef, SetDefaultString), asCALL_THISCALL},
    {"ConfigProperty", "void SetDefaultBoolean(bool value) const", "void ConfigProperty::SetDefaultBoolean(bool value) const", asMETHOD(BMLAS_ConfigPropertyRef, SetDefaultBoolean), asCALL_THISCALL},
    {"ConfigProperty", "void SetDefaultInteger(int value) const", "void ConfigProperty::SetDefaultInteger(int value) const", asMETHOD(BMLAS_ConfigPropertyRef, SetDefaultInteger), asCALL_THISCALL},
    {"ConfigProperty", "void SetDefaultFloat(float value) const", "void ConfigProperty::SetDefaultFloat(float value) const", asMETHOD(BMLAS_ConfigPropertyRef, SetDefaultFloat), asCALL_THISCALL},
    {"ConfigProperty", "void SetDefaultKey(CKKEYBOARD value) const", "void ConfigProperty::SetDefaultKey(CKKEYBOARD value) const", asMETHOD(BMLAS_ConfigPropertyRef, SetDefaultKey), asCALL_THISCALL},
    {"Text2DDefinition", "Text2DDefinition &opAssign(const Text2DDefinition &in)", "Text2DDefinition &Text2DDefinition::opAssign(const Text2DDefinition &in)", asFUNCTION(BMLAS_AssignText2DDefinition), asCALL_CDECL_OBJLAST},
    {"BallTypeDefinition", "BallTypeDefinition &opAssign(const BallTypeDefinition &in)", "BallTypeDefinition &BallTypeDefinition::opAssign(const BallTypeDefinition &in)", asFUNCTION(BMLAS_AssignBallTypeDefinition), asCALL_CDECL_OBJLAST},
    {"FloorTypeDefinition", "FloorTypeDefinition &opAssign(const FloorTypeDefinition &in)", "FloorTypeDefinition &FloorTypeDefinition::opAssign(const FloorTypeDefinition &in)", asFUNCTION(BMLAS_AssignFloorTypeDefinition), asCALL_CDECL_OBJLAST},
    {"ModuleBallDefinition", "ModuleBallDefinition &opAssign(const ModuleBallDefinition &in)", "ModuleBallDefinition &ModuleBallDefinition::opAssign(const ModuleBallDefinition &in)", asFUNCTION(BMLAS_AssignModuleBallDefinition), asCALL_CDECL_OBJLAST},
    {"ModuleConvexDefinition", "ModuleConvexDefinition &opAssign(const ModuleConvexDefinition &in)", "ModuleConvexDefinition &ModuleConvexDefinition::opAssign(const ModuleConvexDefinition &in)", asFUNCTION(BMLAS_AssignModuleConvexDefinition), asCALL_CDECL_OBJLAST},
    {"TrafoDefinition", "TrafoDefinition &opAssign(const TrafoDefinition &in)", "TrafoDefinition &TrafoDefinition::opAssign(const TrafoDefinition &in)", asFUNCTION(BMLAS_AssignTrafoDefinition), asCALL_CDECL_OBJLAST},
    {"ModuleDefinition", "ModuleDefinition &opAssign(const ModuleDefinition &in)", "ModuleDefinition &ModuleDefinition::opAssign(const ModuleDefinition &in)", asFUNCTION(BMLAS_AssignModuleDefinition), asCALL_CDECL_OBJLAST},
    {"CommandDefinition", "CommandDefinition &opAssign(const CommandDefinition &in)", "CommandDefinition &CommandDefinition::opAssign(const CommandDefinition &in)", asFUNCTION(BMLAS_AssignCommandDefinition), asCALL_CDECL_OBJLAST},
    {"ModContext", "ModContext &opAssign(const ModContext &in)", "ModContext &ModContext::opAssign(const ModContext &in)", asFUNCTION(BMLAS_AssignModContext), asCALL_CDECL_OBJLAST},
    {"ModContext", "bool get_HasContext() const", "bool ModContext::get_HasContext() const", asMETHOD(BML::ScriptModContextView, HasContext), asCALL_THISCALL},
    {"ModContext", "bool HasContext() const", "bool ModContext::HasContext() const", asMETHOD(BML::ScriptModContextView, HasContext), asCALL_THISCALL},
    {"ModContext", "string get_ModId() const", "string ModContext::get_ModId() const", asMETHOD(BML::ScriptModContextView, GetModId), asCALL_THISCALL},
    {"ModContext", "string GetModId() const", "string ModContext::GetModId() const", asMETHOD(BML::ScriptModContextView, GetModId), asCALL_THISCALL},
    {"ModContext", "string get_ModName() const", "string ModContext::get_ModName() const", asMETHOD(BML::ScriptModContextView, GetModName), asCALL_THISCALL},
    {"ModContext", "string GetModName() const", "string ModContext::GetModName() const", asMETHOD(BML::ScriptModContextView, GetModName), asCALL_THISCALL},
    {"ModContext", "bool get_IsReloading() const", "bool ModContext::get_IsReloading() const", asMETHOD(BML::ScriptModContextView, IsReloading), asCALL_THISCALL},
    {"ModContext", "bool IsReloading() const", "bool ModContext::IsReloading() const", asMETHOD(BML::ScriptModContextView, IsReloading), asCALL_THISCALL},
    {"ModContext", "ReloadPhase get_ReloadPhase() const", "ReloadPhase ModContext::get_ReloadPhase() const", asMETHOD(BML::ScriptModContextView, GetReloadPhase), asCALL_THISCALL},
    {"ModContext", "ReloadPhase GetReloadPhase() const", "ReloadPhase ModContext::GetReloadPhase() const", asMETHOD(BML::ScriptModContextView, GetReloadPhase), asCALL_THISCALL},
    {"ModContext", "uint get_ReloadAttemptId() const", "uint ModContext::get_ReloadAttemptId() const", asMETHOD(BML::ScriptModContextView, GetReloadAttemptId), asCALL_THISCALL},
    {"ModContext", "uint GetReloadAttemptId() const", "uint ModContext::GetReloadAttemptId() const", asMETHOD(BML::ScriptModContextView, GetReloadAttemptId), asCALL_THISCALL},
    {"ModContext", "uint get_ModGeneration() const", "uint ModContext::get_ModGeneration() const", asMETHOD(BML::ScriptModContextView, GetModGeneration), asCALL_THISCALL},
    {"ModContext", "uint GetModGeneration() const", "uint ModContext::GetModGeneration() const", asMETHOD(BML::ScriptModContextView, GetModGeneration), asCALL_THISCALL},
    {"ModContext", "uint get_RuntimeGeneration() const", "uint ModContext::get_RuntimeGeneration() const", asMETHOD(BML::ScriptModContextView, GetRuntimeGeneration), asCALL_THISCALL},
    {"ModContext", "uint GetRuntimeGeneration() const", "uint ModContext::GetRuntimeGeneration() const", asMETHOD(BML::ScriptModContextView, GetRuntimeGeneration), asCALL_THISCALL},
    {"ModContext", "CKContext@ BorrowCKContext() const", "CKContext@ ModContext::BorrowCKContext() const", asMETHOD(BML::ScriptModContextView, GetCKContext), asCALL_THISCALL},
    {"ModContext", "CKRenderContext@ BorrowRenderContext() const", "CKRenderContext@ ModContext::BorrowRenderContext() const", asMETHOD(BML::ScriptModContextView, GetRenderContext), asCALL_THISCALL},
    {"ModContext", "CKAttributeManager@ BorrowAttributeManager() const", "CKAttributeManager@ ModContext::BorrowAttributeManager() const", asMETHOD(BML::ScriptModContextView, GetAttributeManager), asCALL_THISCALL},
    {"ModContext", "CKBehaviorManager@ BorrowBehaviorManager() const", "CKBehaviorManager@ ModContext::BorrowBehaviorManager() const", asMETHOD(BML::ScriptModContextView, GetBehaviorManager), asCALL_THISCALL},
    {"ModContext", "CKCollisionManager@ BorrowCollisionManager() const", "CKCollisionManager@ ModContext::BorrowCollisionManager() const", asMETHOD(BML::ScriptModContextView, GetCollisionManager), asCALL_THISCALL},
    {"ModContext", "InputHook@ BorrowInputManager() const", "InputHook@ ModContext::BorrowInputManager() const", asMETHOD(BML::ScriptModContextView, GetInputManager), asCALL_THISCALL},
    {"ModContext", "CKMessageManager@ BorrowMessageManager() const", "CKMessageManager@ ModContext::BorrowMessageManager() const", asMETHOD(BML::ScriptModContextView, GetMessageManager), asCALL_THISCALL},
    {"ModContext", "CKPathManager@ BorrowPathManager() const", "CKPathManager@ ModContext::BorrowPathManager() const", asMETHOD(BML::ScriptModContextView, GetPathManager), asCALL_THISCALL},
    {"ModContext", "CKParameterManager@ BorrowParameterManager() const", "CKParameterManager@ ModContext::BorrowParameterManager() const", asMETHOD(BML::ScriptModContextView, GetParameterManager), asCALL_THISCALL},
    {"ModContext", "CKRenderManager@ BorrowRenderManager() const", "CKRenderManager@ ModContext::BorrowRenderManager() const", asMETHOD(BML::ScriptModContextView, GetRenderManager), asCALL_THISCALL},
    {"ModContext", "CKSoundManager@ BorrowSoundManager() const", "CKSoundManager@ ModContext::BorrowSoundManager() const", asMETHOD(BML::ScriptModContextView, GetSoundManager), asCALL_THISCALL},
    {"ModContext", "CKTimeManager@ BorrowTimeManager() const", "CKTimeManager@ ModContext::BorrowTimeManager() const", asMETHOD(BML::ScriptModContextView, GetTimeManager), asCALL_THISCALL},
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
    {"ModContext", "CKDataArray@ BorrowDataArrayByName(const string &in name) const", "CKDataArray@ ModContext::BorrowDataArrayByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetArrayByName), asCALL_THISCALL},
    {"ModContext", "CKGroup@ BorrowGroupByName(const string &in name) const", "CKGroup@ ModContext::BorrowGroupByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetGroupByName), asCALL_THISCALL},
    {"ModContext", "CKMaterial@ BorrowMaterialByName(const string &in name) const", "CKMaterial@ ModContext::BorrowMaterialByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetMaterialByName), asCALL_THISCALL},
    {"ModContext", "CKMesh@ BorrowMeshByName(const string &in name) const", "CKMesh@ ModContext::BorrowMeshByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetMeshByName), asCALL_THISCALL},
    {"ModContext", "CK2dEntity@ Borrow2dEntityByName(const string &in name) const", "CK2dEntity@ ModContext::Borrow2dEntityByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, Get2dEntityByName), asCALL_THISCALL},
    {"ModContext", "CK3dEntity@ Borrow3dEntityByName(const string &in name) const", "CK3dEntity@ ModContext::Borrow3dEntityByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, Get3dEntityByName), asCALL_THISCALL},
    {"ModContext", "CK3dObject@ Borrow3dObjectByName(const string &in name) const", "CK3dObject@ ModContext::Borrow3dObjectByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, Get3dObjectByName), asCALL_THISCALL},
    {"ModContext", "CKCamera@ BorrowCameraByName(const string &in name) const", "CKCamera@ ModContext::BorrowCameraByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetCameraByName), asCALL_THISCALL},
    {"ModContext", "CKTargetCamera@ BorrowTargetCameraByName(const string &in name) const", "CKTargetCamera@ ModContext::BorrowTargetCameraByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetTargetCameraByName), asCALL_THISCALL},
    {"ModContext", "CKLight@ BorrowLightByName(const string &in name) const", "CKLight@ ModContext::BorrowLightByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetLightByName), asCALL_THISCALL},
    {"ModContext", "CKTargetLight@ BorrowTargetLightByName(const string &in name) const", "CKTargetLight@ ModContext::BorrowTargetLightByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetTargetLightByName), asCALL_THISCALL},
    {"ModContext", "CKSound@ BorrowSoundByName(const string &in name) const", "CKSound@ ModContext::BorrowSoundByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetSoundByName), asCALL_THISCALL},
    {"ModContext", "CKTexture@ BorrowTextureByName(const string &in name) const", "CKTexture@ ModContext::BorrowTextureByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetTextureByName), asCALL_THISCALL},
    {"ModContext", "CKBehavior@ BorrowScriptByName(const string &in name) const", "CKBehavior@ ModContext::BorrowScriptByName(const string &in name) const", asMETHOD(BML::ScriptModContextView, GetScriptByName), asCALL_THISCALL},
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
    {"ModContext", "TimerRef@ SetTimeoutTicks(uint delayTicks, TimerCallback@+ callback, const string &in name = \"\") const", "TimerRef@ ModContext::SetTimeoutTicks(uint delayTicks, TimerCallback@+ callback, const string &in name = \"\") const", asMETHOD(BML::ScriptModContextView, SetTimeoutTicks), asCALL_THISCALL},
    {"ModContext", "TimerRef@ SetTimeout(float delayMs, TimerCallback@+ callback, const string &in name = \"\") const", "TimerRef@ ModContext::SetTimeout(float delayMs, TimerCallback@+ callback, const string &in name = \"\") const", asMETHOD(BML::ScriptModContextView, SetTimeout), asCALL_THISCALL},
    {"ModContext", "TimerRef@ SetIntervalTicks(uint delayTicks, TimerLoopCallback@+ callback, const string &in name = \"\") const", "TimerRef@ ModContext::SetIntervalTicks(uint delayTicks, TimerLoopCallback@+ callback, const string &in name = \"\") const", asMETHOD(BML::ScriptModContextView, SetIntervalTicks), asCALL_THISCALL},
    {"ModContext", "TimerRef@ SetInterval(float delayMs, TimerLoopCallback@+ callback, const string &in name = \"\") const", "TimerRef@ ModContext::SetInterval(float delayMs, TimerLoopCallback@+ callback, const string &in name = \"\") const", asMETHOD(BML::ScriptModContextView, SetInterval), asCALL_THISCALL},
    {"ModContext", "CommandRef@ RegisterCommand(Command@+ command) const", "CommandRef@ ModContext::RegisterCommand(Command@+ command) const", asMETHODPR(BML::ScriptModContextView, RegisterCommand, (asIScriptObject *) const, BML::ScriptCommandRef *), asCALL_THISCALL},
    {"ModContext", "CommandRef@ RegisterCommand(const CommandDefinition &in definition, CommandCallback@+ execute, CommandCompletionCallback@+ complete = null) const", "CommandRef@ ModContext::RegisterCommand(const CommandDefinition &in, CommandCallback@+, CommandCompletionCallback@+) const", asFUNCTION(BMLAS_ContextRegisterCommandDelegate), asCALL_CDECL_OBJFIRST},
    {"ModContext", "bool UnregisterCommand(const string &in name) const", "bool ModContext::UnregisterCommand(const string &in name) const", asMETHOD(BML::ScriptModContextView, UnregisterCommand), asCALL_THISCALL},
    {"ModContext", "DataShareRequestRef@ RequestDataShare(DataShareRequest@+ request) const", "DataShareRequestRef@ ModContext::RequestDataShare(DataShareRequest@+ request) const", asMETHODPR(BML::ScriptModContextView, RequestDataShare, (asIScriptObject *) const, BML::ScriptDataShareRequestRef *), asCALL_THISCALL},
    {"ModContext", "DataShareRequestRef@ RequestDataShare(const string &in key, int type, DataShareCallback@+ callback, const string &in name = \"\") const", "DataShareRequestRef@ ModContext::RequestDataShare(const string &in, int, DataShareCallback@+, const string &in) const", asFUNCTION(BMLAS_ContextRequestDataShareDelegate), asCALL_CDECL_OBJFIRST},
    {"ModContext", "HookBlockRef@ CreateHookBlock(CKBehavior@ ownerScript, HookBlockCallback@+ callback, const string &in name = \"\", int inputCount = 1, int outputCount = 1) const", "HookBlockRef@ ModContext::CreateHookBlock(CKBehavior@, HookBlockCallback@+) const", asMETHOD(BML::ScriptModContextView, CreateHookBlock), asCALL_THISCALL},
    {"ModContext", "HookBlockRef@ InsertHookBlockAfter(CKBehavior@ ownerScript, CKBehavior@ source, HookBlockCallback@+ callback, const string &in name = \"\", int sourceOutput = 0, int targetInput = -1) const", "HookBlockRef@ ModContext::InsertHookBlockAfter(CKBehavior@, CKBehavior@, HookBlockCallback@+) const", asMETHOD(BML::ScriptModContextView, InsertHookBlockAfter), asCALL_THISCALL},
    {"ModContext", "HookBlockRef@ InsertHookBlockBefore(CKBehavior@ ownerScript, CKBehavior@ target, HookBlockCallback@+ callback, const string &in name = \"\", int sourceOutput = -1, int targetInput = 0) const", "HookBlockRef@ ModContext::InsertHookBlockBefore(CKBehavior@, CKBehavior@, HookBlockCallback@+) const", asMETHOD(BML::ScriptModContextView, InsertHookBlockBefore), asCALL_THISCALL},
    {"ModContext", "HookBlockRef@ InsertHookBlockBetween(CKBehavior@ ownerScript, CKBehavior@ source, CKBehavior@ target, HookBlockCallback@+ callback, const string &in name = \"\", int sourceOutput = 0, int targetInput = 0) const", "HookBlockRef@ ModContext::InsertHookBlockBetween(CKBehavior@, CKBehavior@, CKBehavior@, HookBlockCallback@+) const", asMETHOD(BML::ScriptModContextView, InsertHookBlockBetween), asCALL_THISCALL},
    {"ModContext", "bool RegisterBallType(const BallTypeDefinition &in definition) const", "bool ModContext::RegisterBallType(const BallTypeDefinition &in definition) const", asFUNCTION(BMLAS_ContextRegisterBallType), asCALL_CDECL_OBJFIRST},
    {"ModContext", "bool RegisterFloorType(const FloorTypeDefinition &in definition) const", "bool ModContext::RegisterFloorType(const FloorTypeDefinition &in definition) const", asFUNCTION(BMLAS_ContextRegisterFloorType), asCALL_CDECL_OBJFIRST},
    {"ModContext", "bool RegisterModule(const ModuleBallDefinition &in definition) const", "bool ModContext::RegisterModule(const ModuleBallDefinition &in definition) const", asFUNCTION(BMLAS_ContextRegisterModuleBall), asCALL_CDECL_OBJFIRST},
    {"ModContext", "bool RegisterModule(const ModuleConvexDefinition &in definition) const", "bool ModContext::RegisterModule(const ModuleConvexDefinition &in definition) const", asFUNCTION(BMLAS_ContextRegisterModuleConvex), asCALL_CDECL_OBJFIRST},
    {"ModContext", "bool RegisterModule(const TrafoDefinition &in definition) const", "bool ModContext::RegisterModule(const TrafoDefinition &in definition) const", asFUNCTION(BMLAS_ContextRegisterTrafo), asCALL_CDECL_OBJFIRST},
    {"ModContext", "bool RegisterModule(const ModuleDefinition &in definition) const", "bool ModContext::RegisterModule(const ModuleDefinition &in definition) const", asFUNCTION(BMLAS_ContextRegisterModule), asCALL_CDECL_OBJFIRST},
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
    {"ModContext", "Logger@ BorrowLogger() const", "Logger@ ModContext::BorrowLogger() const", asFUNCTION(BMLAS_ContextBorrowLogger), asCALL_CDECL_OBJFIRST},
    {"ModContext", "Config@ BorrowConfig() const", "Config@ ModContext::BorrowConfig() const", asFUNCTION(BMLAS_ContextBorrowConfig), asCALL_CDECL_OBJFIRST},
    {"ModContext", "void SendIngameMessage(const string &in message) const", "void ModContext::SendIngameMessage(const string &in message) const", asMETHOD(BML::ScriptModContextView, SendIngameMessage), asCALL_THISCALL},
    {"ModContext", "void ClearIngameMessages() const", "void ModContext::ClearIngameMessages() const", asMETHOD(BML::ScriptModContextView, ClearIngameMessages), asCALL_THISCALL},
    {"ModContext", "void ExecuteCommand(const string &in command) const", "void ModContext::ExecuteCommand(const string &in command) const", asMETHOD(BML::ScriptModContextView, ExecuteCommand), asCALL_THISCALL},
    {"ModContext", "void SkipRenderForNextTick() const", "void ModContext::SkipRenderForNextTick() const", asMETHOD(BML::ScriptModContextView, SkipRenderForNextTick), asCALL_THISCALL},
    {"ModContext", "void OpenModsMenu() const", "void ModContext::OpenModsMenu() const", asMETHOD(BML::ScriptModContextView, OpenModsMenu), asCALL_THISCALL},
    {"ModContext", "void CloseModsMenu() const", "void ModContext::CloseModsMenu() const", asMETHOD(BML::ScriptModContextView, CloseModsMenu), asCALL_THISCALL},
    {"ModContext", "void OpenMapMenu() const", "void ModContext::OpenMapMenu() const", asMETHOD(BML::ScriptModContextView, OpenMapMenu), asCALL_THISCALL},
    {"ModContext", "void CloseMapMenu() const", "void ModContext::CloseMapMenu() const", asMETHOD(BML::ScriptModContextView, CloseMapMenu), asCALL_THISCALL},
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
    {"CallFrame", "int SetArray(uint index, const array<bool> &in values)", "int CallFrame::SetArray(uint index, const array<bool> &in values)", asMETHOD(BMLAS_CallFrame, SetBoolArray), asCALL_THISCALL},
    {"CallFrame", "int GetArray(uint index, array<bool>@ &out values) const", "int CallFrame::GetArray(uint index, array<bool>@ &out values) const", asMETHOD(BMLAS_CallFrame, GetBoolArray), asCALL_THISCALL},
    {"CallFrame", "int SetArray(uint index, const array<int> &in values)", "int CallFrame::SetArray(uint index, const array<int> &in values)", asMETHOD(BMLAS_CallFrame, SetIntArray), asCALL_THISCALL},
    {"CallFrame", "int GetArray(uint index, array<int>@ &out values) const", "int CallFrame::GetArray(uint index, array<int>@ &out values) const", asMETHOD(BMLAS_CallFrame, GetIntArray), asCALL_THISCALL},
    {"CallFrame", "int SetArray(uint index, const array<float> &in values)", "int CallFrame::SetArray(uint index, const array<float> &in values)", asMETHOD(BMLAS_CallFrame, SetFloatArray), asCALL_THISCALL},
    {"CallFrame", "int GetArray(uint index, array<float>@ &out values) const", "int CallFrame::GetArray(uint index, array<float>@ &out values) const", asMETHOD(BMLAS_CallFrame, GetFloatArray), asCALL_THISCALL},
    {"CallFrame", "int SetArray(uint index, const array<string> &in values)", "int CallFrame::SetArray(uint index, const array<string> &in values)", asMETHOD(BMLAS_CallFrame, SetStringArray), asCALL_THISCALL},
    {"CallFrame", "int GetArray(uint index, array<string>@ &out values) const", "int CallFrame::GetArray(uint index, array<string>@ &out values) const", asMETHOD(BMLAS_CallFrame, GetStringArray), asCALL_THISCALL},
    {"CallFrame", "int SetArray(uint index, const array<uint8> &in values)", "int CallFrame::SetArray(uint index, const array<uint8> &in values)", asMETHOD(BMLAS_CallFrame, SetBuffer), asCALL_THISCALL},
    {"CallFrame", "int GetArray(uint index, array<uint8>@ &out values) const", "int CallFrame::GetArray(uint index, array<uint8>@ &out values) const", asMETHOD(BMLAS_CallFrame, GetBuffer), asCALL_THISCALL},
    {"CallFrame", "int SetObjectId(uint index, int objectId)", "int CallFrame::SetObjectId(uint index, int objectId)", asMETHOD(BMLAS_CallFrame, SetObjectId), asCALL_THISCALL},
    {"CallFrame", "int GetObjectId(uint index, int &out objectId) const", "int CallFrame::GetObjectId(uint index, int &out objectId) const", asMETHOD(BMLAS_CallFrame, GetObjectId), asCALL_THISCALL},
    {"CallFrame", "int SetObject(uint index, CKObject@ object)", "int CallFrame::SetObject(uint index, CKObject@ object)", asMETHOD(BMLAS_CallFrame, SetObject), asCALL_THISCALL},
    {"CallFrame", "int GetObject(uint index, CKObject@ &out object) const", "int CallFrame::GetObject(uint index, CKObject@ &out object) const", asMETHOD(BMLAS_CallFrame, GetObject), asCALL_THISCALL},
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
    {"CallFrame", "int SetResultArray(const array<bool> &in values)", "int CallFrame::SetResultArray(const array<bool> &in values)", asMETHOD(BMLAS_CallFrame, SetResultBoolArray), asCALL_THISCALL},
    {"CallFrame", "int GetResultArray(array<bool>@ &out values) const", "int CallFrame::GetResultArray(array<bool>@ &out values) const", asMETHOD(BMLAS_CallFrame, GetResultBoolArray), asCALL_THISCALL},
    {"CallFrame", "int SetResultArray(const array<int> &in values)", "int CallFrame::SetResultArray(const array<int> &in values)", asMETHOD(BMLAS_CallFrame, SetResultIntArray), asCALL_THISCALL},
    {"CallFrame", "int GetResultArray(array<int>@ &out values) const", "int CallFrame::GetResultArray(array<int>@ &out values) const", asMETHOD(BMLAS_CallFrame, GetResultIntArray), asCALL_THISCALL},
    {"CallFrame", "int SetResultArray(const array<float> &in values)", "int CallFrame::SetResultArray(const array<float> &in values)", asMETHOD(BMLAS_CallFrame, SetResultFloatArray), asCALL_THISCALL},
    {"CallFrame", "int GetResultArray(array<float>@ &out values) const", "int CallFrame::GetResultArray(array<float>@ &out values) const", asMETHOD(BMLAS_CallFrame, GetResultFloatArray), asCALL_THISCALL},
    {"CallFrame", "int SetResultArray(const array<string> &in values)", "int CallFrame::SetResultArray(const array<string> &in values)", asMETHOD(BMLAS_CallFrame, SetResultStringArray), asCALL_THISCALL},
    {"CallFrame", "int GetResultArray(array<string>@ &out values) const", "int CallFrame::GetResultArray(array<string>@ &out values) const", asMETHOD(BMLAS_CallFrame, GetResultStringArray), asCALL_THISCALL},
    {"CallFrame", "int SetResultArray(const array<uint8> &in values)", "int CallFrame::SetResultArray(const array<uint8> &in values)", asMETHOD(BMLAS_CallFrame, SetResultBuffer), asCALL_THISCALL},
    {"CallFrame", "int GetResultArray(array<uint8>@ &out values) const", "int CallFrame::GetResultArray(array<uint8>@ &out values) const", asMETHOD(BMLAS_CallFrame, GetResultBuffer), asCALL_THISCALL},
    {"CallFrame", "int SetResultObjectId(int objectId)", "int CallFrame::SetResultObjectId(int objectId)", asMETHOD(BMLAS_CallFrame, SetResultObjectId), asCALL_THISCALL},
    {"CallFrame", "int GetResultObjectId(int &out objectId) const", "int CallFrame::GetResultObjectId(int &out objectId) const", asMETHOD(BMLAS_CallFrame, GetResultObjectId), asCALL_THISCALL},
    {"CallFrame", "int SetResultObject(CKObject@ object)", "int CallFrame::SetResultObject(CKObject@ object)", asMETHOD(BMLAS_CallFrame, SetResultObject), asCALL_THISCALL},
    {"CallFrame", "int GetResultObject(CKObject@ &out object) const", "int CallFrame::GetResultObject(CKObject@ &out object) const", asMETHOD(BMLAS_CallFrame, GetResultObject), asCALL_THISCALL},
    {"StateBag", "bool Has(const string &in key) const", "bool StateBag::Has(const string &in) const", asMETHOD(BML::ScriptStateBag, Has), asCALL_THISCALL},
    {"StateBag", "bool Remove(const string &in key)", "bool StateBag::Remove(const string &in)", asMETHOD(BML::ScriptStateBag, Remove), asCALL_THISCALL},
    {"StateBag", "void Clear()", "void StateBag::Clear()", asMETHOD(BML::ScriptStateBag, Clear), asCALL_THISCALL},
    {"StateBag", "int get_Count() const", "int StateBag::get_Count() const", asMETHOD(BML::ScriptStateBag, GetCount), asCALL_THISCALL},
    {"StateBag", "int GetCount() const", "int StateBag::GetCount() const", asMETHOD(BML::ScriptStateBag, GetCount), asCALL_THISCALL},
    {"StateBag", "string GetKey(int index) const", "string StateBag::GetKey(int) const", asMETHOD(BML::ScriptStateBag, GetKey), asCALL_THISCALL},
    {"StateBag", "StateValueType GetType(const string &in key) const", "StateValueType StateBag::GetType(const string &in) const", asMETHOD(BML::ScriptStateBag, GetType), asCALL_THISCALL},
    {"StateBag", "void SetBool(const string &in key, bool value)", "void StateBag::SetBool(const string &in, bool)", asMETHOD(BML::ScriptStateBag, SetBool), asCALL_THISCALL},
    {"StateBag", "bool GetBool(const string &in key, bool defaultValue = false) const", "bool StateBag::GetBool(const string &in, bool) const", asMETHOD(BML::ScriptStateBag, GetBool), asCALL_THISCALL},
    {"StateBag", "void SetInt(const string &in key, int value)", "void StateBag::SetInt(const string &in, int)", asMETHOD(BML::ScriptStateBag, SetInt), asCALL_THISCALL},
    {"StateBag", "int GetInt(const string &in key, int defaultValue = 0) const", "int StateBag::GetInt(const string &in, int) const", asMETHOD(BML::ScriptStateBag, GetInt), asCALL_THISCALL},
    {"StateBag", "void SetFloat(const string &in key, float value)", "void StateBag::SetFloat(const string &in, float)", asMETHOD(BML::ScriptStateBag, SetFloat), asCALL_THISCALL},
    {"StateBag", "float GetFloat(const string &in key, float defaultValue = 0.0f) const", "float StateBag::GetFloat(const string &in, float) const", asMETHOD(BML::ScriptStateBag, GetFloat), asCALL_THISCALL},
    {"StateBag", "void SetString(const string &in key, const string &in value)", "void StateBag::SetString(const string &in, const string &in)", asMETHOD(BML::ScriptStateBag, SetString), asCALL_THISCALL},
    {"StateBag", "string GetString(const string &in key, const string &in defaultValue = \"\") const", "string StateBag::GetString(const string &in, const string &in) const", asMETHOD(BML::ScriptStateBag, GetString), asCALL_THISCALL},
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
    {"HookBlockEvent", "bool get_IsValid() const", "bool HookBlockEvent::get_IsValid() const", asMETHOD(BMLAS_HookBlockEvent, IsValid), asCALL_THISCALL},
    {"HookBlockEvent", "int get_BlockId() const", "int HookBlockEvent::get_BlockId() const", asMETHOD(BMLAS_HookBlockEvent, GetBlockId), asCALL_THISCALL},
    {"HookBlockEvent", "string get_BlockName() const", "string HookBlockEvent::get_BlockName() const", asMETHOD(BMLAS_HookBlockEvent, GetBlockName), asCALL_THISCALL},
    {"HookBlockEvent", "float get_DeltaTime() const", "float HookBlockEvent::get_DeltaTime() const", asMETHOD(BMLAS_HookBlockEvent, GetDeltaTime), asCALL_THISCALL},
    {"HookBlockEvent", "int get_InputCount() const", "int HookBlockEvent::get_InputCount() const", asMETHOD(BMLAS_HookBlockEvent, GetInputCount), asCALL_THISCALL},
    {"HookBlockEvent", "int get_OutputCount() const", "int HookBlockEvent::get_OutputCount() const", asMETHOD(BMLAS_HookBlockEvent, GetOutputCount), asCALL_THISCALL},
    {"HookBlockEvent", "CKBehavior@ BorrowBlock() const", "CKBehavior@ HookBlockEvent::BorrowBlock() const", asMETHOD(BMLAS_HookBlockEvent, BorrowBlock), asCALL_THISCALL},
    {"HookBlockEvent", "CKBehavior@ BorrowOwnerScript() const", "CKBehavior@ HookBlockEvent::BorrowOwnerScript() const", asMETHOD(BMLAS_HookBlockEvent, BorrowOwnerScript), asCALL_THISCALL},
    {"HookBlockEvent", "bool ActivateOutput(int index) const", "bool HookBlockEvent::ActivateOutput(int index) const", asMETHOD(BMLAS_HookBlockEvent, ActivateOutput), asCALL_THISCALL},
    {"HookBlockEvent", "void ActivateAllOutputs() const", "void HookBlockEvent::ActivateAllOutputs() const", asMETHOD(BMLAS_HookBlockEvent, ActivateAllOutputs), asCALL_THISCALL},
    {"HookBlockRef", "bool get_IsValid() const", "bool HookBlockRef::get_IsValid() const", asMETHOD(BML::ScriptHookBlockRef, IsValid), asCALL_THISCALL},
    {"HookBlockRef", "bool get_IsInstalled() const", "bool HookBlockRef::get_IsInstalled() const", asMETHOD(BML::ScriptHookBlockRef, IsInstalled), asCALL_THISCALL},
    {"HookBlockRef", "bool get_Enabled() const", "bool HookBlockRef::get_Enabled() const", asMETHOD(BML::ScriptHookBlockRef, IsEnabled), asCALL_THISCALL},
    {"HookBlockRef", "void set_Enabled(bool enabled)", "void HookBlockRef::set_Enabled(bool enabled)", asFUNCTION(BMLAS_HookBlockRefSetEnabled), asCALL_CDECL_OBJFIRST},
    {"HookBlockRef", "bool SetEnabled(bool enabled)", "bool HookBlockRef::SetEnabled(bool enabled)", asMETHOD(BML::ScriptHookBlockRef, SetEnabled), asCALL_THISCALL},
    {"HookBlockRef", "bool get_AutoActivateOutputs() const", "bool HookBlockRef::get_AutoActivateOutputs() const", asMETHOD(BML::ScriptHookBlockRef, GetAutoActivateOutputs), asCALL_THISCALL},
    {"HookBlockRef", "void set_AutoActivateOutputs(bool enabled)", "void HookBlockRef::set_AutoActivateOutputs(bool enabled)", asFUNCTION(BMLAS_HookBlockRefSetAutoActivateOutputs), asCALL_CDECL_OBJFIRST},
    {"HookBlockRef", "bool SetAutoActivateOutputs(bool enabled)", "bool HookBlockRef::SetAutoActivateOutputs(bool enabled)", asMETHOD(BML::ScriptHookBlockRef, SetAutoActivateOutputs), asCALL_THISCALL},
    {"HookBlockRef", "int get_BlockId() const", "int HookBlockRef::get_BlockId() const", asMETHOD(BML::ScriptHookBlockRef, GetBlockId), asCALL_THISCALL},
    {"HookBlockRef", "string get_Name() const", "string HookBlockRef::get_Name() const", asMETHOD(BML::ScriptHookBlockRef, GetName), asCALL_THISCALL},
    {"HookBlockRef", "CKBehavior@ BorrowBlock() const", "CKBehavior@ HookBlockRef::BorrowBlock() const", asMETHOD(BML::ScriptHookBlockRef, BorrowBlock), asCALL_THISCALL},
    {"HookBlockRef", "CKBehavior@ BorrowOwnerScript() const", "CKBehavior@ HookBlockRef::BorrowOwnerScript() const", asMETHOD(BML::ScriptHookBlockRef, BorrowOwnerScript), asCALL_THISCALL},
    {"HookBlockRef", "bool Uninstall()", "bool HookBlockRef::Uninstall()", asMETHOD(BML::ScriptHookBlockRef, Uninstall), asCALL_THISCALL},
    {"InputHook", "bool get_IsValid() const", "bool InputHook::get_IsValid() const", asFUNCTION(BMLAS_InputHookIsValid), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsValid() const", "bool InputHook::IsValid() const", asFUNCTION(BMLAS_InputHookIsValid), asCALL_CDECL_OBJFIRST},
    {"InputHook", "void EnableKeyboardRepetition(bool enable = true) const", "void InputHook::EnableKeyboardRepetition(bool enable) const", asFUNCTION(BMLAS_InputHookEnableKeyboardRepetition), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsKeyboardRepetitionEnabled() const", "bool InputHook::IsKeyboardRepetitionEnabled() const", asFUNCTION(BMLAS_InputHookIsKeyboardRepetitionEnabled), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsKeyboardAttached() const", "bool InputHook::IsKeyboardAttached() const", asFUNCTION(BMLAS_InputHookIsKeyboardAttached), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsMouseAttached() const", "bool InputHook::IsMouseAttached() const", asFUNCTION(BMLAS_InputHookIsMouseAttached), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsJoystickAttached(int joystick) const", "bool InputHook::IsJoystickAttached(int joystick) const", asFUNCTION(BMLAS_InputHookIsJoystickAttached), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsKeyDown(CKKEYBOARD key) const", "bool InputHook::IsKeyDown(CKKEYBOARD key) const", asFUNCTION(BMLAS_InputHookIsKeyDown), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsKeyDown(CKKEYBOARD key, uint &out stamp) const", "bool InputHook::IsKeyDown(CKKEYBOARD key, uint &out stamp) const", asFUNCTION(BMLAS_InputHookIsKeyDownWithStamp), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsKeyUp(CKKEYBOARD key) const", "bool InputHook::IsKeyUp(CKKEYBOARD key) const", asFUNCTION(BMLAS_InputHookIsKeyUp), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsKeyPressed(CKKEYBOARD key) const", "bool InputHook::IsKeyPressed(CKKEYBOARD key) const", asFUNCTION(BMLAS_InputHookIsKeyPressed), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsKeyReleased(CKKEYBOARD key) const", "bool InputHook::IsKeyReleased(CKKEYBOARD key) const", asFUNCTION(BMLAS_InputHookIsKeyReleased), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsKeyToggled(CKKEYBOARD key) const", "bool InputHook::IsKeyToggled(CKKEYBOARD key) const", asFUNCTION(BMLAS_InputHookIsKeyToggled), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsKeyToggled(CKKEYBOARD key, uint &out stamp) const", "bool InputHook::IsKeyToggled(CKKEYBOARD key, uint &out stamp) const", asFUNCTION(BMLAS_InputHookIsKeyToggledWithStamp), asCALL_CDECL_OBJFIRST},
    {"InputHook", "string GetKeyName(CKKEYBOARD key) const", "string InputHook::GetKeyName(CKKEYBOARD key) const", asFUNCTION(BMLAS_InputHookGetKeyName), asCALL_CDECL_OBJFIRST},
    {"InputHook", "int GetKeyFromName(const string &in name) const", "int InputHook::GetKeyFromName(const string &in name) const", asFUNCTION(BMLAS_InputHookGetKeyFromName), asCALL_CDECL_OBJFIRST},
    {"InputHook", "int GetKeyboardState(CKKEYBOARD key) const", "int InputHook::GetKeyboardState(CKKEYBOARD key) const", asFUNCTION(BMLAS_InputHookGetKeyboardState), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsKeyboardStateDown(CKKEYBOARD key) const", "bool InputHook::IsKeyboardStateDown(CKKEYBOARD key) const", asFUNCTION(BMLAS_InputHookIsKeyboardStateDown), asCALL_CDECL_OBJFIRST},
    {"InputHook", "int GetNumberOfKeyInBuffer() const", "int InputHook::GetNumberOfKeyInBuffer() const", asFUNCTION(BMLAS_InputHookGetNumberOfKeyInBuffer), asCALL_CDECL_OBJFIRST},
    {"InputHook", "int GetKeyFromBuffer(int index, CKKEYBOARD &out key, uint &out timestamp) const", "int InputHook::GetKeyFromBuffer(int index, CKKEYBOARD &out key, uint &out timestamp) const", asFUNCTION(BMLAS_InputHookGetKeyFromBuffer), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsMouseButtonDown(CK_MOUSEBUTTON button) const", "bool InputHook::IsMouseButtonDown(CK_MOUSEBUTTON button) const", asFUNCTION(BMLAS_InputHookIsMouseButtonDown), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsMouseClicked(CK_MOUSEBUTTON button) const", "bool InputHook::IsMouseClicked(CK_MOUSEBUTTON button) const", asFUNCTION(BMLAS_InputHookIsMouseClicked), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsMouseToggled(CK_MOUSEBUTTON button) const", "bool InputHook::IsMouseToggled(CK_MOUSEBUTTON button) const", asFUNCTION(BMLAS_InputHookIsMouseToggled), asCALL_CDECL_OBJFIRST},
    {"InputHook", "int GetMouseButtonState(CK_MOUSEBUTTON button) const", "int InputHook::GetMouseButtonState(CK_MOUSEBUTTON button) const", asFUNCTION(BMLAS_InputHookGetMouseButtonState), asCALL_CDECL_OBJFIRST},
    {"InputHook", "Vx2DVector GetMousePosition(bool absolute = true) const", "Vx2DVector InputHook::GetMousePosition(bool absolute) const", asFUNCTION(BMLAS_InputHookGetMousePosition), asCALL_CDECL_OBJFIRST},
    {"InputHook", "Vx2DVector GetLastMousePosition() const", "Vx2DVector InputHook::GetLastMousePosition() const", asFUNCTION(BMLAS_InputHookGetLastMousePosition), asCALL_CDECL_OBJFIRST},
    {"InputHook", "VxVector GetMouseRelativePosition() const", "VxVector InputHook::GetMouseRelativePosition() const", asFUNCTION(BMLAS_InputHookGetMouseRelativePosition), asCALL_CDECL_OBJFIRST},
    {"InputHook", "VxVector GetJoystickPosition(int joystick) const", "VxVector InputHook::GetJoystickPosition(int joystick) const", asFUNCTION(BMLAS_InputHookGetJoystickPosition), asCALL_CDECL_OBJFIRST},
    {"InputHook", "VxVector GetJoystickRotation(int joystick) const", "VxVector InputHook::GetJoystickRotation(int joystick) const", asFUNCTION(BMLAS_InputHookGetJoystickRotation), asCALL_CDECL_OBJFIRST},
    {"InputHook", "Vx2DVector GetJoystickSliders(int joystick) const", "Vx2DVector InputHook::GetJoystickSliders(int joystick) const", asFUNCTION(BMLAS_InputHookGetJoystickSliders), asCALL_CDECL_OBJFIRST},
    {"InputHook", "float GetJoystickPointOfViewAngle(int joystick) const", "float InputHook::GetJoystickPointOfViewAngle(int joystick) const", asFUNCTION(BMLAS_InputHookGetJoystickPointOfViewAngle), asCALL_CDECL_OBJFIRST},
    {"InputHook", "uint GetJoystickButtonsState(int joystick) const", "uint InputHook::GetJoystickButtonsState(int joystick) const", asFUNCTION(BMLAS_InputHookGetJoystickButtonsState), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsJoystickButtonDown(int joystick, int button) const", "bool InputHook::IsJoystickButtonDown(int joystick, int button) const", asFUNCTION(BMLAS_InputHookIsJoystickButtonDown), asCALL_CDECL_OBJFIRST},
    {"InputHook", "void Pause(bool pause) const", "void InputHook::Pause(bool pause) const", asFUNCTION(BMLAS_InputHookPause), asCALL_CDECL_OBJFIRST},
    {"InputHook", "void ShowCursor(bool show) const", "void InputHook::ShowCursor(bool show) const", asFUNCTION(BMLAS_InputHookShowCursor), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool GetCursorVisibility() const", "bool InputHook::GetCursorVisibility() const", asFUNCTION(BMLAS_InputHookGetCursorVisibility), asCALL_CDECL_OBJFIRST},
    {"InputHook", "int GetSystemCursor() const", "int InputHook::GetSystemCursor() const", asFUNCTION(BMLAS_InputHookGetSystemCursor), asCALL_CDECL_OBJFIRST},
    {"InputHook", "void SetSystemCursor(int cursor) const", "void InputHook::SetSystemCursor(int cursor) const", asFUNCTION(BMLAS_InputHookSetSystemCursor), asCALL_CDECL_OBJFIRST},
    {"InputHook", "bool IsBlock() const", "bool InputHook::IsBlock() const", asFUNCTION(BMLAS_InputHookIsBlock), asCALL_CDECL_OBJFIRST},
    {"InputHook", "void SetBlock(bool block) const", "void InputHook::SetBlock(bool block) const", asFUNCTION(BMLAS_InputHookSetBlock), asCALL_CDECL_OBJFIRST},
    {"InputHook", "int IsBlocked(InputDevice device) const", "int InputHook::IsBlocked(InputDevice device) const", asFUNCTION(BMLAS_InputHookIsBlocked), asCALL_CDECL_OBJFIRST},
    {"InputHook", "void Block(InputDevice device) const", "void InputHook::Block(InputDevice device) const", asFUNCTION(BMLAS_InputHookBlock), asCALL_CDECL_OBJFIRST},
    {"InputHook", "void Unblock(InputDevice device) const", "void InputHook::Unblock(InputDevice device) const", asFUNCTION(BMLAS_InputHookUnblock), asCALL_CDECL_OBJFIRST},
    {"InputHook", "uint64 AcquireBlock(uint mask) const", "uint64 InputHook::AcquireBlock(uint mask) const", asFUNCTION(BMLAS_InputHookAcquireBlock), asCALL_CDECL_OBJFIRST},
    {"InputHook", "void ReleaseBlock(uint64 token) const", "void InputHook::ReleaseBlock(uint64 token) const", asFUNCTION(BMLAS_InputHookReleaseBlock), asCALL_CDECL_OBJFIRST},
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
    {"LoadObjectEvent", "int get_ObjectCount() const", "int LoadObjectEvent::get_ObjectCount() const", asMETHOD(BML::ScriptLoadObjectEventView, GetObjectCount), asCALL_THISCALL},
    {"LoadObjectEvent", "int GetObjectId(int index) const", "int LoadObjectEvent::GetObjectId(int index) const", asMETHOD(BML::ScriptLoadObjectEventView, GetObjectId), asCALL_THISCALL},
    {"LoadObjectEvent", "CKObject@ BorrowObject(int index) const", "CKObject@ LoadObjectEvent::BorrowObject(int index) const", asMETHOD(BML::ScriptLoadObjectEventView, BorrowObject), asCALL_THISCALL},
    {"LoadObjectEvent", "CKObject@ BorrowMasterObject() const", "CKObject@ LoadObjectEvent::BorrowMasterObject() const", asMETHOD(BML::ScriptLoadObjectEventView, BorrowMasterObject), asCALL_THISCALL},
    {"LoadScriptEvent", "string get_Filename() const", "string LoadScriptEvent::get_Filename() const", asMETHOD(BML::ScriptLoadScriptEventView, GetFilename), asCALL_THISCALL},
    {"LoadScriptEvent", "int get_ScriptId() const", "int LoadScriptEvent::get_ScriptId() const", asMETHOD(BML::ScriptLoadScriptEventView, GetScriptId), asCALL_THISCALL},
    {"LoadScriptEvent", "CKBehavior@ BorrowScript() const", "CKBehavior@ LoadScriptEvent::BorrowScript() const", asMETHOD(BML::ScriptLoadScriptEventView, BorrowScript), asCALL_THISCALL},
    {"CommandEvent", "CommandEventPhase get_Phase() const", "CommandEventPhase CommandEvent::get_Phase() const", asMETHOD(BML::ScriptCommandEventView, GetPhase), asCALL_THISCALL},
    {"CommandEvent", "bool get_IsPre() const", "bool CommandEvent::get_IsPre() const", asMETHOD(BML::ScriptCommandEventView, IsPre), asCALL_THISCALL},
    {"CommandEvent", "bool get_IsPost() const", "bool CommandEvent::get_IsPost() const", asMETHOD(BML::ScriptCommandEventView, IsPost), asCALL_THISCALL},
    {"CommandEvent", "bool get_IsExecute() const", "bool CommandEvent::get_IsExecute() const", asMETHOD(BML::ScriptCommandEventView, IsExecute), asCALL_THISCALL},
    {"CommandEvent", "bool get_IsComplete() const", "bool CommandEvent::get_IsComplete() const", asMETHOD(BML::ScriptCommandEventView, IsComplete), asCALL_THISCALL},
    {"CommandEvent", "string get_CommandName() const", "string CommandEvent::get_CommandName() const", asMETHOD(BML::ScriptCommandEventView, GetCommandName), asCALL_THISCALL},
    {"CommandEvent", "int get_ArgCount() const", "int CommandEvent::get_ArgCount() const", asMETHOD(BML::ScriptCommandEventView, GetArgCount), asCALL_THISCALL},
    {"CommandEvent", "string GetArg(int index) const", "string CommandEvent::GetArg(int index) const", asMETHOD(BML::ScriptCommandEventView, GetArg), asCALL_THISCALL},
    {"CommandEvent", "string get_ArgsText() const", "string CommandEvent::get_ArgsText() const", asMETHOD(BML::ScriptCommandEventView, GetArgsText), asCALL_THISCALL},
    {"CommandEvent", "bool get_IsCheat() const", "bool CommandEvent::get_IsCheat() const", asMETHOD(BML::ScriptCommandEventView, IsCheat), asCALL_THISCALL},
    {"ConfigEvent", "string get_ModId() const", "string ConfigEvent::get_ModId() const", asMETHOD(BML::ScriptConfigEventView, GetModId), asCALL_THISCALL},
    {"ConfigEvent", "string get_Category() const", "string ConfigEvent::get_Category() const", asMETHOD(BML::ScriptConfigEventView, GetCategory), asCALL_THISCALL},
    {"ConfigEvent", "string get_Key() const", "string ConfigEvent::get_Key() const", asMETHOD(BML::ScriptConfigEventView, GetKey), asCALL_THISCALL},
    {"ConfigEvent", "ConfigPropertyType get_Type() const", "ConfigPropertyType ConfigEvent::get_Type() const", asMETHOD(BML::ScriptConfigEventView, GetType), asCALL_THISCALL},
    {"ConfigEvent", "bool get_HasProperty() const", "bool ConfigEvent::get_HasProperty() const", asMETHOD(BML::ScriptConfigEventView, HasProperty), asCALL_THISCALL},
    {"ConfigEvent", "ConfigProperty@ BorrowProperty() const", "ConfigProperty@ ConfigEvent::BorrowProperty() const", asFUNCTION(BMLAS_ConfigEventBorrowProperty), asCALL_CDECL_OBJFIRST},
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
    {"PhysicalizeEvent", "CK3dEntity@ BorrowTarget() const", "CK3dEntity@ PhysicalizeEvent::BorrowTarget() const", asMETHOD(BML::ScriptPhysicalizeEventView, BorrowTarget), asCALL_THISCALL},
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
    {"PhysicalizeEvent", "VxVector get_MassCenter() const", "VxVector PhysicalizeEvent::get_MassCenter() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetMassCenter), asCALL_THISCALL},
    {"PhysicalizeEvent", "int get_ConvexCount() const", "int PhysicalizeEvent::get_ConvexCount() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetConvexCount), asCALL_THISCALL},
    {"PhysicalizeEvent", "CKMesh@ BorrowConvexMesh(int index) const", "CKMesh@ PhysicalizeEvent::BorrowConvexMesh(int index) const", asMETHOD(BML::ScriptPhysicalizeEventView, BorrowConvexMesh), asCALL_THISCALL},
    {"PhysicalizeEvent", "int get_BallCount() const", "int PhysicalizeEvent::get_BallCount() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetBallCount), asCALL_THISCALL},
    {"PhysicalizeEvent", "VxVector GetBallCenter(int index) const", "VxVector PhysicalizeEvent::GetBallCenter(int index) const", asMETHOD(BML::ScriptPhysicalizeEventView, GetBallCenter), asCALL_THISCALL},
    {"PhysicalizeEvent", "float GetBallRadius(int index) const", "float PhysicalizeEvent::GetBallRadius(int index) const", asMETHOD(BML::ScriptPhysicalizeEventView, GetBallRadius), asCALL_THISCALL},
    {"PhysicalizeEvent", "int get_ConcaveCount() const", "int PhysicalizeEvent::get_ConcaveCount() const", asMETHOD(BML::ScriptPhysicalizeEventView, GetConcaveCount), asCALL_THISCALL},
    {"PhysicalizeEvent", "CKMesh@ BorrowConcaveMesh(int index) const", "CKMesh@ PhysicalizeEvent::BorrowConcaveMesh(int index) const", asMETHOD(BML::ScriptPhysicalizeEventView, BorrowConcaveMesh), asCALL_THISCALL},
    {"ObjectEvent", "int get_TargetId() const", "int ObjectEvent::get_TargetId() const", asMETHOD(BML::ScriptObjectEventView, GetTargetId), asCALL_THISCALL},
    {"ObjectEvent", "string get_TargetName() const", "string ObjectEvent::get_TargetName() const", asMETHOD(BML::ScriptObjectEventView, GetTargetName), asCALL_THISCALL},
    {"ObjectEvent", "CK3dEntity@ BorrowTarget() const", "CK3dEntity@ ObjectEvent::BorrowTarget() const", asMETHOD(BML::ScriptObjectEventView, BorrowTarget), asCALL_THISCALL},
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

static const ScriptUiEnumValueRegistration kFontTypeRegistrations[] = {
    {"FONT_NONE", ExecuteBB::NOFONT, "BML::FONT_NONE"},
    {"FONT_GAME_NORMAL", ExecuteBB::GAMEFONT_01, "BML::FONT_GAME_NORMAL"},
    {"FONT_GAME_LARGE", ExecuteBB::GAMEFONT_02, "BML::FONT_GAME_LARGE"},
    {"FONT_GAME_SMALL", ExecuteBB::GAMEFONT_03, "BML::FONT_GAME_SMALL"},
    {"FONT_GAME_SMALL_GRAY", ExecuteBB::GAMEFONT_03A, "BML::FONT_GAME_SMALL_GRAY"},
    {"FONT_GAME_HUGE", ExecuteBB::GAMEFONT_04, "BML::FONT_GAME_HUGE"},
    {"FONT_CREDITS_SMALL", ExecuteBB::GAMEFONT_CREDITS_SMALL, "BML::FONT_CREDITS_SMALL"},
    {"FONT_CREDITS_BIG", ExecuteBB::GAMEFONT_CREDITS_BIG, "BML::FONT_CREDITS_BIG"},
};

static const ScriptUiFunctionRegistration kUiFunctionRegistrations[] = {
    {"void SendMessage(const string &in message)", "BML::UI::SendMessage", asFUNCTION(BMLAS_CoreUI_SendMessage), asCALL_CDECL},
    {"void ClearMessages()", "BML::UI::ClearMessages", asFUNCTION(BMLAS_CoreUI_ClearMessages), asCALL_CDECL},
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
    {"bool OkButton(const string &in label)", "BML::UI::OkButton", asFUNCTION(BMLAS_UI_OkButton), asCALL_CDECL},
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
    {"bool KeyButton(const string &in label, bool &inout toggled, int &inout keyChord)", "BML::UI::KeyButton", asFUNCTION(BMLAS_UI_KeyButton), asCALL_CDECL},
    {"void Title(const string &in text, float y = 0.13f, float scale = 1.5f)", "BML::UI::Title", asFUNCTION(BMLAS_UI_Title), asCALL_CDECL},
    {"void WrappedText(const string &in text, float width, float baseX = 0.0f, float scale = 1.0f)", "BML::UI::WrappedText", asFUNCTION(BMLAS_UI_WrappedText), asCALL_CDECL},
    {"bool NavLeft(float x = 0.36f, float y = 0.124f)", "BML::UI::NavLeft", asFUNCTION(BMLAS_UI_NavLeft), asCALL_CDECL},
    {"bool NavRight(float x = 0.6038f, float y = 0.124f)", "BML::UI::NavRight", asFUNCTION(BMLAS_UI_NavRight), asCALL_CDECL},
    {"bool NavBack(float x = 0.4031f, float y = 0.85f)", "BML::UI::NavBack", asFUNCTION(BMLAS_UI_NavBack), asCALL_CDECL},
    {"bool YesNoButton(const string &in label, bool &inout value)", "BML::UI::YesNoButton", asFUNCTION(BMLAS_UI_YesNoButton), asCALL_CDECL},
    {"bool RadioButtonText(const string &in label, int &inout currentItem, const string &in items)", "BML::UI::RadioButtonText", asFUNCTION(BMLAS_UI_RadioButtonText), asCALL_CDECL},
    {"bool InputTextButton(const string &in label, string &inout value, int maxLength = 256)", "BML::UI::InputTextButton", asFUNCTION(BMLAS_UI_InputTextButton), asCALL_CDECL},
    {"bool InputIntButton(const string &in label, int &inout value, int step = 1, int stepFast = 100)", "BML::UI::InputIntButton", asFUNCTION(BMLAS_UI_InputIntButton), asCALL_CDECL},
    {"bool InputFloatButton(const string &in label, float &inout value, float step = 0.0f, float stepFast = 0.0f)", "BML::UI::InputFloatButton", asFUNCTION(BMLAS_UI_InputFloatButton), asCALL_CDECL},
    {"bool SearchBar(string &inout text, float x = 0.4f, float y = 0.18f, float width = 0.2f)", "BML::UI::SearchBar", asFUNCTION(BMLAS_UI_SearchBar), asCALL_CDECL},
    {"void PlayMenuClickSound()", "BML::UI::PlayMenuClickSound", asFUNCTION(BMLAS_UI_PlayMenuClickSound), asCALL_CDECL},
    {"int CKKeyToImGuiKey(CKKEYBOARD key)", "BML::UI::CKKeyToImGuiKey", asFUNCTION(BMLAS_UI_CKKeyToImGuiKey), asCALL_CDECL},
    {"CKKEYBOARD ImGuiKeyToCKKey(int key)", "BML::UI::ImGuiKeyToCKKey", asFUNCTION(BMLAS_UI_ImGuiKeyToCKKey), asCALL_CDECL},
    {"string KeyChordToString(int keyChord)", "BML::UI::KeyChordToString", asFUNCTION(BMLAS_UI_KeyChordToString), asCALL_CDECL},
    {"bool SetKeyChordFromIO(int &inout keyChord)", "BML::UI::SetKeyChordFromIO", asFUNCTION(BMLAS_UI_SetKeyChordFromIO), asCALL_CDECL},
    {"float GetButtonSizeX(ButtonType type)", "BML::UI::GetButtonSizeX", asFUNCTION(BMLAS_UI_GetButtonSizeX), asCALL_CDECL},
    {"float GetButtonSizeY(ButtonType type)", "BML::UI::GetButtonSizeY", asFUNCTION(BMLAS_UI_GetButtonSizeY), asCALL_CDECL},
    {"float GetButtonIndent(ButtonType type)", "BML::UI::GetButtonIndent", asFUNCTION(BMLAS_UI_GetButtonIndent), asCALL_CDECL},
    {"float GetButtonSizeCoordX(ButtonType type)", "BML::UI::GetButtonSizeCoordX", asFUNCTION(BMLAS_UI_GetButtonSizeCoordX), asCALL_CDECL},
    {"float GetButtonSizeCoordY(ButtonType type)", "BML::UI::GetButtonSizeCoordY", asFUNCTION(BMLAS_UI_GetButtonSizeCoordY), asCALL_CDECL},
    {"float GetButtonIndentCoord(ButtonType type)", "BML::UI::GetButtonIndentCoord", asFUNCTION(BMLAS_UI_GetButtonIndentCoord), asCALL_CDECL},
};

static const ScriptGlobalFunctionRegistration kGlobalFunctionRegistrations[] = {
    {"string GetVersion()", "BML::GetVersion", asFUNCTION(BMLAS_GetVersion), asCALL_CDECL},
    {"int GetVersionMajor()", "BML::GetVersionMajor", asFUNCTION(BMLAS_GetVersionMajor), asCALL_CDECL},
    {"int GetVersionMinor()", "BML::GetVersionMinor", asFUNCTION(BMLAS_GetVersionMinor), asCALL_CDECL},
    {"int GetVersionPatch()", "BML::GetVersionPatch", asFUNCTION(BMLAS_GetVersionPatch), asCALL_CDECL},
    {"string GetErrorString(int errorCode)", "BML::GetErrorString", asFUNCTION(BMLAS_GetErrorString), asCALL_CDECL},
    {"string GetGameEventName(GameEvent event)", "BML::GetGameEventName", asFUNCTION(BMLAS_GetGameEventName), asCALL_CDECL},
    {"bool BorrowCurrentContext(ModContext &out context)", "BML::BorrowCurrentContext", asFUNCTION(BMLAS_BorrowCurrentContext), asCALL_CDECL},
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
};

static const ScriptGlobalFunctionRegistration kPathFunctionRegistrations[] = {
    {"bool Exists(const string &in path)", "BML::Path::Exists", asFUNCTION(BMLAS_PathExistsUtf8), asCALL_CDECL},
    {"bool IsFile(const string &in path)", "BML::Path::IsFile", asFUNCTION(BMLAS_FileExistsUtf8), asCALL_CDECL},
    {"bool IsDirectory(const string &in path)", "BML::Path::IsDirectory", asFUNCTION(BMLAS_DirectoryExistsUtf8), asCALL_CDECL},
    {"bool IsValid(const string &in path)", "BML::Path::IsValid", asFUNCTION(BMLAS_IsPathValidUtf8), asCALL_CDECL},
    {"bool IsAbsolute(const string &in path)", "BML::Path::IsAbsolute", asFUNCTION(BMLAS_IsAbsolutePathUtf8), asCALL_CDECL},
    {"bool IsRelative(const string &in path)", "BML::Path::IsRelative", asFUNCTION(BMLAS_IsRelativePathUtf8), asCALL_CDECL},
    {"string Combine(const string &in left, const string &in right)", "BML::Path::Combine", asFUNCTION(BMLAS_CombinePathUtf8), asCALL_CDECL},
    {"string Normalize(const string &in path)", "BML::Path::Normalize", asFUNCTION(BMLAS_NormalizePathUtf8), asCALL_CDECL},
    {"string FileName(const string &in path)", "BML::Path::FileName", asFUNCTION(BMLAS_GetFileNameUtf8), asCALL_CDECL},
    {"string Extension(const string &in path)", "BML::Path::Extension", asFUNCTION(BMLAS_GetExtensionUtf8), asCALL_CDECL},
    {"string RemoveExtension(const string &in path)", "BML::Path::RemoveExtension", asFUNCTION(BMLAS_RemoveExtensionUtf8), asCALL_CDECL},
};

static const ScriptGlobalFunctionRegistration kMenuFunctionRegistrations[] = {
    {"void OpenModsMenu()", "BML::Menu::OpenModsMenu", asFUNCTION(BMLAS_CoreMenu_OpenModsMenu), asCALL_CDECL},
    {"void CloseModsMenu()", "BML::Menu::CloseModsMenu", asFUNCTION(BMLAS_CoreMenu_CloseModsMenu), asCALL_CDECL},
    {"void OpenMapMenu()", "BML::Menu::OpenMapMenu", asFUNCTION(BMLAS_CoreMenu_OpenMapMenu), asCALL_CDECL},
    {"void CloseMapMenu()", "BML::Menu::CloseMapMenu", asFUNCTION(BMLAS_CoreMenu_CloseMapMenu), asCALL_CDECL},
};

static const ScriptGlobalFunctionRegistration kHudFunctionRegistrations[] = {
    {"int GetMode()", "BML::HUD::GetMode", asFUNCTION(BMLAS_CoreHUD_GetMode), asCALL_CDECL},
    {"void SetMode(int mode)", "BML::HUD::SetMode", asFUNCTION(BMLAS_CoreHUD_SetMode), asCALL_CDECL},
    {"void ShowTitle(bool show)", "BML::HUD::ShowTitle", asFUNCTION(BMLAS_CoreHUD_ShowTitle), asCALL_CDECL},
    {"void ShowFPS(bool show)", "BML::HUD::ShowFPS", asFUNCTION(BMLAS_CoreHUD_ShowFPS), asCALL_CDECL},
    {"void ShowSRTimer(bool show)", "BML::HUD::ShowSRTimer", asFUNCTION(BMLAS_CoreHUD_ShowSRTimer), asCALL_CDECL},
    {"void StartSRTimer()", "BML::HUD::StartSRTimer", asFUNCTION(BMLAS_CoreHUD_StartSRTimer), asCALL_CDECL},
    {"void PauseSRTimer()", "BML::HUD::PauseSRTimer", asFUNCTION(BMLAS_CoreHUD_PauseSRTimer), asCALL_CDECL},
    {"void ResetSRTimer()", "BML::HUD::ResetSRTimer", asFUNCTION(BMLAS_CoreHUD_ResetSRTimer), asCALL_CDECL},
    {"float GetSRTime()", "BML::HUD::GetSRTime", asFUNCTION(BMLAS_CoreHUD_GetSRTime), asCALL_CDECL},
};

static const ScriptGlobalFunctionRegistration kCkFunctionRegistrations[] = {
    {"bool IsValid(CKObject@ object)", "BML::CK::IsValid", asFUNCTION(BMLAS_IsObjectValid), asCALL_CDECL},
    {"int GetId(CKObject@ object)", "BML::CK::GetId", asFUNCTION(BMLAS_GetObjectId), asCALL_CDECL},
    {"string GetName(CKObject@ object)", "BML::CK::GetName", asFUNCTION(BMLAS_GetObjectName), asCALL_CDECL},
    {"int GetClassId(CKObject@ object)", "BML::CK::GetClassId", asFUNCTION(BMLAS_GetObjectClassId), asCALL_CDECL},
    {"bool IsVisible(CKObject@ object)", "BML::CK::IsVisible", asFUNCTION(BMLAS_IsObjectVisible), asCALL_CDECL},
    {"bool IsDynamic(CKObject@ object)", "BML::CK::IsDynamic", asFUNCTION(BMLAS_IsObjectDynamic), asCALL_CDECL},
    {"int GetPriority(CKBeObject@ object)", "BML::CK::GetPriority", asFUNCTION(BMLAS_GetBeObjectPriority), asCALL_CDECL},
    {"int GetScriptCount(CKBeObject@ object)", "BML::CK::GetScriptCount", asFUNCTION(BMLAS_GetBeObjectScriptCount), asCALL_CDECL},
    {"int GetAttributeCount(CKBeObject@ object)", "BML::CK::GetAttributeCount", asFUNCTION(BMLAS_GetBeObjectAttributeCount), asCALL_CDECL},
    {"void SetIC(CKBeObject@ object, bool hierarchy = false)", "BML::CK::SetIC", asFUNCTION(BMLAS_SetIC), asCALL_CDECL},
    {"void RestoreIC(CKBeObject@ object, bool hierarchy = false)", "BML::CK::RestoreIC", asFUNCTION(BMLAS_RestoreIC), asCALL_CDECL},
    {"void Show(CKBeObject@ object, CK_OBJECT_SHOWOPTION show = CKSHOW, bool hierarchy = false)", "BML::CK::Show", asFUNCTION(BMLAS_Show), asCALL_CDECL},
    {"VxVector GetPosition(CK3dEntity@ entity)", "BML::CK::GetPosition", asFUNCTION(BMLAS_Get3dEntityPosition), asCALL_CDECL},
    {"void SetPosition(CK3dEntity@ entity, const VxVector &in position)", "BML::CK::SetPosition", asFUNCTION(BMLAS_CK_Set3dEntityPosition), asCALL_CDECL},
    {"VxVector GetScale(CK3dEntity@ entity, bool local = true)", "BML::CK::GetScale", asFUNCTION(BMLAS_Get3dEntityScale), asCALL_CDECL},
    {"void SetScale(CK3dEntity@ entity, const VxVector &in scale, bool local = true)", "BML::CK::SetScale", asFUNCTION(BMLAS_CK_Set3dEntityScale), asCALL_CDECL},
    {"int GetChildCount(CK3dEntity@ entity)", "BML::CK::GetChildCount", asFUNCTION(BMLAS_Get3dEntityChildCount), asCALL_CDECL},
    {"CK3dEntity@ BorrowChild(CK3dEntity@ entity, int index)", "BML::CK::BorrowChild", asFUNCTION(BMLAS_CK_Borrow3dEntityChild), asCALL_CDECL},
    {"CK3dEntity@ BorrowParent(CK3dEntity@ entity)", "BML::CK::BorrowParent", asFUNCTION(BMLAS_Get3dEntityParent), asCALL_CDECL},
    {"int GetRowCount(CKDataArray@ array)", "BML::CK::GetRowCount", asFUNCTION(BMLAS_CK_GetDataArrayRowCount), asCALL_CDECL},
    {"int GetColumnCount(CKDataArray@ array)", "BML::CK::GetColumnCount", asFUNCTION(BMLAS_CK_GetDataArrayColumnCount), asCALL_CDECL},
    {"string GetColumnName(CKDataArray@ array, int column)", "BML::CK::GetColumnName", asFUNCTION(BMLAS_CK_GetDataArrayColumnName), asCALL_CDECL},
    {"int FindColumn(CKDataArray@ array, const string &in name)", "BML::CK::FindColumn", asFUNCTION(BMLAS_CK_FindDataArrayColumn), asCALL_CDECL},
    {"string GetString(CKDataArray@ array, int row, int column, const string &in defaultValue = \"\")", "BML::CK::GetString", asFUNCTION(BMLAS_CK_GetDataArrayString), asCALL_CDECL},
    {"bool GetBool(CKDataArray@ array, int row, int column, bool defaultValue = false)", "BML::CK::GetBool", asFUNCTION(BMLAS_CK_GetDataArrayBool), asCALL_CDECL},
    {"int GetInt(CKDataArray@ array, int row, int column, int defaultValue = 0)", "BML::CK::GetInt", asFUNCTION(BMLAS_CK_GetDataArrayInt), asCALL_CDECL},
    {"float GetFloat(CKDataArray@ array, int row, int column, float defaultValue = 0.0f)", "BML::CK::GetFloat", asFUNCTION(BMLAS_CK_GetDataArrayFloat), asCALL_CDECL},
    {"bool SetString(CKDataArray@ array, int row, int column, const string &in value)", "BML::CK::SetString", asFUNCTION(BMLAS_CK_SetDataArrayString), asCALL_CDECL},
    {"bool SetBool(CKDataArray@ array, int row, int column, bool value)", "BML::CK::SetBool", asFUNCTION(BMLAS_CK_SetDataArrayBool), asCALL_CDECL},
    {"bool SetInt(CKDataArray@ array, int row, int column, int value)", "BML::CK::SetInt", asFUNCTION(BMLAS_CK_SetDataArrayInt), asCALL_CDECL},
    {"bool SetFloat(CKDataArray@ array, int row, int column, float value)", "BML::CK::SetFloat", asFUNCTION(BMLAS_CK_SetDataArrayFloat), asCALL_CDECL},
    {"ObjectLoadResult@ LoadObject(const BML::ObjectLoadOptions &in options)", "BML::CK::LoadObject", asFUNCTION(BMLAS_CK_LoadObject), asCALL_CDECL},
};

static const ScriptGlobalFunctionRegistration kPhysicsFunctionRegistrations[] = {
    {"bool PhysicalizeConvex(CK3dEntity@ target, const BML::PhysicalizeDefinition &in definition, CKMesh@ mesh = null)", "BML::Physics::PhysicalizeConvex", asFUNCTION(BMLAS_Physics_PhysicalizeConvex), asCALL_CDECL},
    {"bool PhysicalizeBall(CK3dEntity@ target, const BML::PhysicalizeDefinition &in definition, const VxVector &in center, float radius)", "BML::Physics::PhysicalizeBall", asFUNCTION(BMLAS_Physics_PhysicalizeBall), asCALL_CDECL},
    {"bool PhysicalizeConcave(CK3dEntity@ target, const BML::PhysicalizeDefinition &in definition, CKMesh@ mesh = null)", "BML::Physics::PhysicalizeConcave", asFUNCTION(BMLAS_Physics_PhysicalizeConcave), asCALL_CDECL},
    {"bool Unphysicalize(CK3dEntity@ target)", "BML::Physics::Unphysicalize", asFUNCTION(BMLAS_Physics_Unphysicalize), asCALL_CDECL},
    {"bool SetForce(CK3dEntity@ target, const VxVector &in position, CK3dEntity@ positionReference, const VxVector &in direction, CK3dEntity@ directionReference, float force)", "BML::Physics::SetForce", asFUNCTION(BMLAS_Physics_SetForce), asCALL_CDECL},
    {"bool ClearForce(CK3dEntity@ target)", "BML::Physics::ClearForce", asFUNCTION(BMLAS_Physics_ClearForce), asCALL_CDECL},
    {"bool Impulse(CK3dEntity@ target, const VxVector &in position, CK3dEntity@ positionReference, const VxVector &in direction, CK3dEntity@ directionReference, float impulse)", "BML::Physics::Impulse", asFUNCTION(BMLAS_Physics_Impulse), asCALL_CDECL},
    {"bool WakeUp(CK3dEntity@ target)", "BML::Physics::WakeUp", asFUNCTION(BMLAS_Physics_WakeUp), asCALL_CDECL},
};

static const ScriptGlobalFunctionRegistration kTextFunctionRegistrations[] = {
    {"CKBehavior@ Create2DText(CKBehavior@ ownerScript, CK2dEntity@ target, const BML::Text2DDefinition &in definition)", "BML::Text::Create2DText", asFUNCTION(BMLAS_Text_Create2DTextDefaultMaterials), asCALL_CDECL},
    {"CKBehavior@ Create2DText(CKBehavior@ ownerScript, CK2dEntity@ target, const BML::Text2DDefinition &in definition, CKMaterial@ backgroundMaterial, CKMaterial@ caretMaterial)", "BML::Text::Create2DText(materials)", asFUNCTION(BMLAS_Text_Create2DText), asCALL_CDECL},
};

static const ScriptGlobalFunctionRegistration kHookFunctionRegistrations[] = {
    {"HookBlockRef@ Create(CKBehavior@ ownerScript, HookBlockCallback@+ callback, const string &in name = \"\", int inputCount = 1, int outputCount = 1)", "BML::Hook::Create", asFUNCTION(BMLAS_Hook_Create), asCALL_CDECL},
    {"HookBlockRef@ InsertAfter(CKBehavior@ ownerScript, CKBehavior@ source, HookBlockCallback@+ callback, const string &in name = \"\", int sourceOutput = 0, int targetInput = -1)", "BML::Hook::InsertAfter", asFUNCTION(BMLAS_Hook_InsertAfter), asCALL_CDECL},
    {"HookBlockRef@ InsertBefore(CKBehavior@ ownerScript, CKBehavior@ target, HookBlockCallback@+ callback, const string &in name = \"\", int sourceOutput = -1, int targetInput = 0)", "BML::Hook::InsertBefore", asFUNCTION(BMLAS_Hook_InsertBefore), asCALL_CDECL},
    {"HookBlockRef@ InsertBetween(CKBehavior@ ownerScript, CKBehavior@ source, CKBehavior@ target, HookBlockCallback@+ callback, const string &in name = \"\", int sourceOutput = 0, int targetInput = 0)", "BML::Hook::InsertBetween", asFUNCTION(BMLAS_Hook_InsertBetween), asCALL_CDECL},
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

    for (const ScriptObjectPropertyRegistration &registration : kObjectPropertyRegistrations) {
        if (!CheckRegistrationSurfaceText("object property type", registration.DiagnosticName, registration.TypeName) ||
            !CheckRegistrationSurfaceText("object property declaration", registration.TypeName, registration.Declaration) ||
            !CheckRegistrationSurfaceText("object property diagnostic", registration.TypeName, registration.DiagnosticName)) {
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

    for (const ScriptUiEnumValueRegistration &registration : kFontTypeRegistrations) {
        if (!CheckRegistrationSurfaceText("font enum value", "BML::FontType", registration.Name) ||
            !CheckRegistrationSurfaceText("font enum diagnostic", "BML::FontType", registration.DiagnosticName)) {
            return false;
        }
    }

    const ScriptGlobalFunctionRegistration *globalFunctionGroups[] = {
        kGlobalFunctionRegistrations,
        kPathFunctionRegistrations,
        kMenuFunctionRegistrations,
        kHudFunctionRegistrations,
        kCkFunctionRegistrations,
        kPhysicsFunctionRegistrations,
        kTextFunctionRegistrations,
        kHookFunctionRegistrations,
    };
    const std::size_t globalFunctionGroupSizes[] = {
        std::size(kGlobalFunctionRegistrations),
        std::size(kPathFunctionRegistrations),
        std::size(kMenuFunctionRegistrations),
        std::size(kHudFunctionRegistrations),
        std::size(kCkFunctionRegistrations),
        std::size(kPhysicsFunctionRegistrations),
        std::size(kTextFunctionRegistrations),
        std::size(kHookFunctionRegistrations),
    };

    for (std::size_t group = 0; group < std::size(globalFunctionGroups); ++group) {
        for (std::size_t index = 0; index < globalFunctionGroupSizes[group]; ++index) {
            const ScriptGlobalFunctionRegistration &registration = globalFunctionGroups[group][index];
        if (!CheckRegistrationSurfaceText("global function declaration", nullptr, registration.Declaration) ||
            !CheckRegistrationSurfaceText("global function diagnostic", nullptr, registration.DiagnosticName)) {
            return false;
        }
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
    BML_AS_REGISTER(engine->RegisterEnum("FontType"), "enum FontType");
    for (const ScriptUiEnumValueRegistration &registration : kFontTypeRegistrations) {
        BML_AS_REGISTER(engine->RegisterEnumValue("FontType", registration.Name, registration.Value), registration.DiagnosticName);
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

int RegisterScriptFuncdefs(asIScriptEngine *engine, const char **errorMessage) {
    for (const ScriptFuncdefRegistration &registration : kFuncdefRegistrations) {
        BML_AS_REGISTER(engine->RegisterFuncdef(registration.Declaration), registration.DiagnosticName);
    }
    return asSUCCESS;
}

int RegisterScriptObjectProperties(asIScriptEngine *engine, const char **errorMessage) {
    for (const ScriptObjectPropertyRegistration &registration : kObjectPropertyRegistrations) {
        BML_AS_REGISTER(engine->RegisterObjectProperty(registration.TypeName, registration.Declaration, registration.ByteOffset), registration.DiagnosticName);
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

template <std::size_t Count>
int RegisterScriptGlobalFunctionList(asIScriptEngine *engine,
                                     const char **errorMessage,
                                     const char *namespaceName,
                                     const ScriptGlobalFunctionRegistration (&registrations)[Count]) {
    BML_AS_REGISTER(engine->SetDefaultNamespace(namespaceName), namespaceName);
    for (const ScriptGlobalFunctionRegistration &registration : registrations) {
        BML_AS_REGISTER(engine->RegisterGlobalFunction(registration.Declaration, registration.Function, registration.CallConvention), registration.DiagnosticName);
    }
    BML_AS_REGISTER(engine->SetDefaultNamespace("BML"), "namespace BML");
    return asSUCCESS;
}

int RegisterScriptGlobalFunctions(asIScriptEngine *engine, const char **errorMessage) {
    const int bmlResult = RegisterScriptGlobalFunctionList(engine, errorMessage, "BML", kGlobalFunctionRegistrations);
    if (bmlResult < 0)
        return bmlResult;
    const int pathResult = RegisterScriptGlobalFunctionList(engine, errorMessage, "BML::Path", kPathFunctionRegistrations);
    if (pathResult < 0)
        return pathResult;
    const int menuResult = RegisterScriptGlobalFunctionList(engine, errorMessage, "BML::Menu", kMenuFunctionRegistrations);
    if (menuResult < 0)
        return menuResult;
    const int hudResult = RegisterScriptGlobalFunctionList(engine, errorMessage, "BML::HUD", kHudFunctionRegistrations);
    if (hudResult < 0)
        return hudResult;
    const int ckResult = RegisterScriptGlobalFunctionList(engine, errorMessage, "BML::CK", kCkFunctionRegistrations);
    if (ckResult < 0)
        return ckResult;
    const int physicsResult = RegisterScriptGlobalFunctionList(engine, errorMessage, "BML::Physics", kPhysicsFunctionRegistrations);
    if (physicsResult < 0)
        return physicsResult;
    const int textResult = RegisterScriptGlobalFunctionList(engine, errorMessage, "BML::Text", kTextFunctionRegistrations);
    if (textResult < 0)
        return textResult;
    return RegisterScriptGlobalFunctionList(engine, errorMessage, "BML::Hook", kHookFunctionRegistrations);
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
    const int propertiesResult = RegisterScriptObjectProperties(engine, errorMessage);
    if (propertiesResult < 0)
        return propertiesResult;
    const int interfacesResult = RegisterScriptInterfaces(engine, errorMessage);
    if (interfacesResult < 0)
        return interfacesResult;
    const int funcdefsResult = RegisterScriptFuncdefs(engine, errorMessage);
    if (funcdefsResult < 0)
        return funcdefsResult;
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

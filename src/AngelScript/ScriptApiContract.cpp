#include "ScriptApiContract.h"

#include "BML/Interop.h"
#include "BML/IConfig.h"
#include "BML/InputHook.h"
#include "BML/Timer.h"
#include "ScriptCallbackEvents.h"

namespace BML {

namespace {

static const ScriptCallbackContract kCallbacks[] = {
    {ScriptCallbackOnLoad, "OnLoad", "void OnLoad(const BML::ModContext &in)", "OnLoad failed", ScriptCallbackPayloadKind::None},
    {ScriptCallbackOnUnload, "OnUnload", "void OnUnload(const BML::ModContext &in)", "OnUnload failed", ScriptCallbackPayloadKind::None},
    {ScriptCallbackOnProcess, "OnProcess", "void OnProcess(const BML::ModContext &in)", "OnProcess failed", ScriptCallbackPayloadKind::None},
    {ScriptCallbackOnRender, "OnRender", "void OnRender(const BML::ModContext &in, const BML::RenderEvent &in)", "OnRender failed", ScriptCallbackPayloadKind::EventObject},
    {ScriptCallbackOnGameEvent, "OnGameEvent", "void OnGameEvent(const BML::ModContext &in, BML::GameEvent event)", "OnGameEvent failed", ScriptCallbackPayloadKind::GameEventInt},
    {ScriptCallbackOnCheatEnabled, "OnCheatEnabled", "void OnCheatEnabled(const BML::ModContext &in, const BML::CheatEvent &in)", "OnCheatEnabled failed", ScriptCallbackPayloadKind::EventObject},
    {ScriptCallbackOnLoadObject, "OnLoadObject", "void OnLoadObject(const BML::ModContext &in, const BML::LoadObjectEvent &in)", "OnLoadObject failed", ScriptCallbackPayloadKind::EventObject},
    {ScriptCallbackOnLoadScript, "OnLoadScript", "void OnLoadScript(const BML::ModContext &in, const BML::LoadScriptEvent &in)", "OnLoadScript failed", ScriptCallbackPayloadKind::EventObject},
    {ScriptCallbackOnCommandEvent, "OnCommandEvent", "void OnCommandEvent(const BML::ModContext &in, const BML::CommandEvent &in)", "OnCommandEvent failed", ScriptCallbackPayloadKind::EventObject},
    {ScriptCallbackOnModifyConfig, "OnModifyConfig", "void OnModifyConfig(const BML::ModContext &in, const BML::ConfigEvent &in)", "OnModifyConfig failed", ScriptCallbackPayloadKind::EventObject},
    {ScriptCallbackOnPhysicalize, "OnPhysicalize", "void OnPhysicalize(const BML::ModContext &in, const BML::PhysicalizeEvent &in)", "OnPhysicalize failed", ScriptCallbackPayloadKind::EventObject},
    {ScriptCallbackOnUnphysicalize, "OnUnphysicalize", "void OnUnphysicalize(const BML::ModContext &in, const BML::ObjectEvent &in)", "OnUnphysicalize failed", ScriptCallbackPayloadKind::EventObject},
};

static const ScriptIntegerConstantContract kErrorConstants[] = {
    {"const int ERROR_OK", BML_OK},
    {"const int ERROR_FAIL", BML_ERROR_FAIL},
    {"const int ERROR_NOT_FOUND", BML_ERROR_NOT_FOUND},
    {"const int ERROR_INVALID_PARAMETER", BML_ERROR_INVALID_PARAMETER},
    {"const int ERROR_INTEROP_TARGET_NOT_FOUND", BML_ERROR_INTEROP_TARGET_NOT_FOUND},
    {"const int ERROR_INTEROP_TARGET_FAILED", BML_ERROR_INTEROP_TARGET_FAILED},
    {"const int ERROR_INTEROP_EXPORT_NOT_FOUND", BML_ERROR_INTEROP_EXPORT_NOT_FOUND},
    {"const int ERROR_INTEROP_EXPORT_AMBIGUOUS", BML_ERROR_INTEROP_EXPORT_AMBIGUOUS},
    {"const int ERROR_INTEROP_BAD_SIGNATURE", BML_ERROR_INTEROP_BAD_SIGNATURE},
    {"const int ERROR_INTEROP_SIGNATURE_MISMATCH", BML_ERROR_INTEROP_SIGNATURE_MISMATCH},
    {"const int ERROR_INTEROP_BAD_CALL_FRAME", BML_ERROR_INTEROP_BAD_CALL_FRAME},
    {"const int ERROR_INTEROP_TYPE_MISMATCH", BML_ERROR_INTEROP_TYPE_MISMATCH},
    {"const int ERROR_INTEROP_TARGET_EXECUTION_FAILED", BML_ERROR_INTEROP_TARGET_EXECUTION_FAILED},
    {"const int ERROR_INTEROP_HANDLE_STALE", BML_ERROR_INTEROP_HANDLE_STALE},
};

static const ScriptEnumValueContract kGameEventValues[] = {
    {"GAME_EVENT_PRE_START_MENU", ScriptGameEventPreStartMenu, "GameEvent::GAME_EVENT_PRE_START_MENU"},
    {"GAME_EVENT_POST_START_MENU", ScriptGameEventPostStartMenu, "GameEvent::GAME_EVENT_POST_START_MENU"},
    {"GAME_EVENT_EXIT_GAME", ScriptGameEventExitGame, "GameEvent::GAME_EVENT_EXIT_GAME"},
    {"GAME_EVENT_PRE_LOAD_LEVEL", ScriptGameEventPreLoadLevel, "GameEvent::GAME_EVENT_PRE_LOAD_LEVEL"},
    {"GAME_EVENT_POST_LOAD_LEVEL", ScriptGameEventPostLoadLevel, "GameEvent::GAME_EVENT_POST_LOAD_LEVEL"},
    {"GAME_EVENT_START_LEVEL", ScriptGameEventStartLevel, "GameEvent::GAME_EVENT_START_LEVEL"},
    {"GAME_EVENT_PRE_RESET_LEVEL", ScriptGameEventPreResetLevel, "GameEvent::GAME_EVENT_PRE_RESET_LEVEL"},
    {"GAME_EVENT_POST_RESET_LEVEL", ScriptGameEventPostResetLevel, "GameEvent::GAME_EVENT_POST_RESET_LEVEL"},
    {"GAME_EVENT_PAUSE_LEVEL", ScriptGameEventPauseLevel, "GameEvent::GAME_EVENT_PAUSE_LEVEL"},
    {"GAME_EVENT_UNPAUSE_LEVEL", ScriptGameEventUnpauseLevel, "GameEvent::GAME_EVENT_UNPAUSE_LEVEL"},
    {"GAME_EVENT_PRE_EXIT_LEVEL", ScriptGameEventPreExitLevel, "GameEvent::GAME_EVENT_PRE_EXIT_LEVEL"},
    {"GAME_EVENT_POST_EXIT_LEVEL", ScriptGameEventPostExitLevel, "GameEvent::GAME_EVENT_POST_EXIT_LEVEL"},
    {"GAME_EVENT_PRE_NEXT_LEVEL", ScriptGameEventPreNextLevel, "GameEvent::GAME_EVENT_PRE_NEXT_LEVEL"},
    {"GAME_EVENT_POST_NEXT_LEVEL", ScriptGameEventPostNextLevel, "GameEvent::GAME_EVENT_POST_NEXT_LEVEL"},
    {"GAME_EVENT_DEAD", ScriptGameEventDead, "GameEvent::GAME_EVENT_DEAD"},
    {"GAME_EVENT_PRE_END_LEVEL", ScriptGameEventPreEndLevel, "GameEvent::GAME_EVENT_PRE_END_LEVEL"},
    {"GAME_EVENT_POST_END_LEVEL", ScriptGameEventPostEndLevel, "GameEvent::GAME_EVENT_POST_END_LEVEL"},
    {"GAME_EVENT_COUNTER_ACTIVE", ScriptGameEventCounterActive, "GameEvent::GAME_EVENT_COUNTER_ACTIVE"},
    {"GAME_EVENT_COUNTER_INACTIVE", ScriptGameEventCounterInactive, "GameEvent::GAME_EVENT_COUNTER_INACTIVE"},
    {"GAME_EVENT_BALL_NAV_ACTIVE", ScriptGameEventBallNavActive, "GameEvent::GAME_EVENT_BALL_NAV_ACTIVE"},
    {"GAME_EVENT_BALL_NAV_INACTIVE", ScriptGameEventBallNavInactive, "GameEvent::GAME_EVENT_BALL_NAV_INACTIVE"},
    {"GAME_EVENT_CAM_NAV_ACTIVE", ScriptGameEventCamNavActive, "GameEvent::GAME_EVENT_CAM_NAV_ACTIVE"},
    {"GAME_EVENT_CAM_NAV_INACTIVE", ScriptGameEventCamNavInactive, "GameEvent::GAME_EVENT_CAM_NAV_INACTIVE"},
    {"GAME_EVENT_BALL_OFF", ScriptGameEventBallOff, "GameEvent::GAME_EVENT_BALL_OFF"},
    {"GAME_EVENT_PRE_CHECKPOINT_REACHED", ScriptGameEventPreCheckpointReached, "GameEvent::GAME_EVENT_PRE_CHECKPOINT_REACHED"},
    {"GAME_EVENT_POST_CHECKPOINT_REACHED", ScriptGameEventPostCheckpointReached, "GameEvent::GAME_EVENT_POST_CHECKPOINT_REACHED"},
    {"GAME_EVENT_LEVEL_FINISH", ScriptGameEventLevelFinish, "GameEvent::GAME_EVENT_LEVEL_FINISH"},
    {"GAME_EVENT_GAME_OVER", ScriptGameEventGameOver, "GameEvent::GAME_EVENT_GAME_OVER"},
    {"GAME_EVENT_EXTRA_POINT", ScriptGameEventExtraPoint, "GameEvent::GAME_EVENT_EXTRA_POINT"},
    {"GAME_EVENT_PRE_SUB_LIFE", ScriptGameEventPreSubLife, "GameEvent::GAME_EVENT_PRE_SUB_LIFE"},
    {"GAME_EVENT_POST_SUB_LIFE", ScriptGameEventPostSubLife, "GameEvent::GAME_EVENT_POST_SUB_LIFE"},
    {"GAME_EVENT_PRE_LIFE_UP", ScriptGameEventPreLifeUp, "GameEvent::GAME_EVENT_PRE_LIFE_UP"},
    {"GAME_EVENT_POST_LIFE_UP", ScriptGameEventPostLifeUp, "GameEvent::GAME_EVENT_POST_LIFE_UP"},
};

static const ScriptEnumValueContract kDirectoryTypeValues[] = {
    {"DIR_WORKING", 0, "DirectoryType::DIR_WORKING"},
    {"DIR_TEMP", 1, "DirectoryType::DIR_TEMP"},
    {"DIR_GAME", 2, "DirectoryType::DIR_GAME"},
    {"DIR_LOADER", 3, "DirectoryType::DIR_LOADER"},
    {"DIR_CONFIG", 4, "DirectoryType::DIR_CONFIG"},
};

static const ScriptEnumValueContract kModKindValues[] = {
    {"MOD_KIND_UNKNOWN", 0, "ModKind::MOD_KIND_UNKNOWN"},
    {"MOD_KIND_NATIVE", 1, "ModKind::MOD_KIND_NATIVE"},
    {"MOD_KIND_SCRIPT", 2, "ModKind::MOD_KIND_SCRIPT"},
};

static const ScriptEnumValueContract kModStateValues[] = {
    {"MOD_STATE_NOT_FOUND", 0, "ModState::MOD_STATE_NOT_FOUND"},
    {"MOD_STATE_REGISTERED", 1, "ModState::MOD_STATE_REGISTERED"},
    {"MOD_STATE_LOADED", 2, "ModState::MOD_STATE_LOADED"},
    {"MOD_STATE_FAILED", 3, "ModState::MOD_STATE_FAILED"},
};

static const ScriptEnumValueContract kHudFlagValues[] = {
    {"HUD_TITLE", 1, "HudFlag::HUD_TITLE"},
    {"HUD_FPS", 2, "HudFlag::HUD_FPS"},
    {"HUD_SR", 4, "HudFlag::HUD_SR"},
};

static const ScriptEnumValueContract kInputDeviceValues[] = {
    {"INPUT_DEVICE_KEYBOARD", CK_INPUT_DEVICE_KEYBOARD, "InputDevice::INPUT_DEVICE_KEYBOARD"},
    {"INPUT_DEVICE_MOUSE", CK_INPUT_DEVICE_MOUSE, "InputDevice::INPUT_DEVICE_MOUSE"},
    {"INPUT_DEVICE_JOYSTICK", CK_INPUT_DEVICE_JOYSTICK, "InputDevice::INPUT_DEVICE_JOYSTICK"},
    {"INPUT_DEVICE_COUNT", CK_INPUT_DEVICE_COUNT, "InputDevice::INPUT_DEVICE_COUNT"},
};

static const ScriptEnumValueContract kInputKeyEventValues[] = {
    {"INPUT_KEY_NONE", NO_KEY, "InputKeyEvent::INPUT_KEY_NONE"},
    {"INPUT_KEY_PRESSED", KEY_PRESSED, "InputKeyEvent::INPUT_KEY_PRESSED"},
    {"INPUT_KEY_RELEASED", KEY_RELEASED, "InputKeyEvent::INPUT_KEY_RELEASED"},
};

static const ScriptEnumValueContract kInputButtonStateValues[] = {
    {"INPUT_BUTTON_IDLE", KS_IDLE, "InputButtonState::INPUT_BUTTON_IDLE"},
    {"INPUT_BUTTON_PRESSED", KS_PRESSED, "InputButtonState::INPUT_BUTTON_PRESSED"},
    {"INPUT_BUTTON_RELEASED", KS_RELEASED, "InputButtonState::INPUT_BUTTON_RELEASED"},
};

static const ScriptEnumValueContract kCursorPointerValues[] = {
    {"CURSOR_NORMALSELECT", VXCURSOR_NORMALSELECT, "CursorPointer::CURSOR_NORMALSELECT"},
    {"CURSOR_BUSY", VXCURSOR_BUSY, "CursorPointer::CURSOR_BUSY"},
    {"CURSOR_MOVE", VXCURSOR_MOVE, "CursorPointer::CURSOR_MOVE"},
    {"CURSOR_LINKSELECT", VXCURSOR_LINKSELECT, "CursorPointer::CURSOR_LINKSELECT"},
};

static const ScriptEnumValueContract kCommandEventPhaseValues[] = {
    {"COMMAND_EVENT_PRE", ScriptCommandEventPre, "CommandEventPhase::COMMAND_EVENT_PRE"},
    {"COMMAND_EVENT_POST", ScriptCommandEventPost, "CommandEventPhase::COMMAND_EVENT_POST"},
    {"COMMAND_EVENT_EXECUTE", ScriptCommandEventExecute, "CommandEventPhase::COMMAND_EVENT_EXECUTE"},
    {"COMMAND_EVENT_COMPLETE", ScriptCommandEventComplete, "CommandEventPhase::COMMAND_EVENT_COMPLETE"},
};

static const ScriptEnumValueContract kConfigPropertyTypeValues[] = {
    {"CONFIG_PROPERTY_STRING", IProperty::STRING, "ConfigPropertyType::CONFIG_PROPERTY_STRING"},
    {"CONFIG_PROPERTY_BOOLEAN", IProperty::BOOLEAN, "ConfigPropertyType::CONFIG_PROPERTY_BOOLEAN"},
    {"CONFIG_PROPERTY_INTEGER", IProperty::INTEGER, "ConfigPropertyType::CONFIG_PROPERTY_INTEGER"},
    {"CONFIG_PROPERTY_KEY", IProperty::KEY, "ConfigPropertyType::CONFIG_PROPERTY_KEY"},
    {"CONFIG_PROPERTY_FLOAT", IProperty::FLOAT, "ConfigPropertyType::CONFIG_PROPERTY_FLOAT"},
    {"CONFIG_PROPERTY_NONE", IProperty::NONE, "ConfigPropertyType::CONFIG_PROPERTY_NONE"},
};

static const ScriptEnumValueContract kTimerStateValues[] = {
    {"TIMER_IDLE", Timer::IDLE, "TimerState::TIMER_IDLE"},
    {"TIMER_RUNNING", Timer::RUNNING, "TimerState::TIMER_RUNNING"},
    {"TIMER_PAUSED", Timer::PAUSED, "TimerState::TIMER_PAUSED"},
    {"TIMER_COMPLETED", Timer::COMPLETED, "TimerState::TIMER_COMPLETED"},
    {"TIMER_CANCELLED", Timer::CANCELLED, "TimerState::TIMER_CANCELLED"},
};

static const ScriptEnumValueContract kTimerTypeValues[] = {
    {"TIMER_ONCE", Timer::ONCE, "TimerType::TIMER_ONCE"},
    {"TIMER_LOOP", Timer::LOOP, "TimerType::TIMER_LOOP"},
    {"TIMER_REPEAT", Timer::REPEAT, "TimerType::TIMER_REPEAT"},
    {"TIMER_INTERVAL", Timer::INTERVAL, "TimerType::TIMER_INTERVAL"},
    {"TIMER_DEBOUNCE", Timer::DEBOUNCE, "TimerType::TIMER_DEBOUNCE"},
    {"TIMER_THROTTLE", Timer::THROTTLE, "TimerType::TIMER_THROTTLE"},
};

static const ScriptEnumValueContract kTimerTimeBaseValues[] = {
    {"TIMER_TIMEBASE_TICK", Timer::TICK, "TimerTimeBase::TIMER_TIMEBASE_TICK"},
    {"TIMER_TIMEBASE_TIME", Timer::TIME, "TimerTimeBase::TIMER_TIMEBASE_TIME"},
    {"TIMER_TIMEBASE_REALTIME", Timer::REALTIME, "TimerTimeBase::TIMER_TIMEBASE_REALTIME"},
};

static const ScriptEnumValueContract kDataShareValueTypeValues[] = {
    {"DATASHARE_STRING", 0, "DataShareValueType::DATASHARE_STRING"},
    {"DATASHARE_BOOL", 1, "DataShareValueType::DATASHARE_BOOL"},
    {"DATASHARE_INT", 2, "DataShareValueType::DATASHARE_INT"},
    {"DATASHARE_FLOAT", 3, "DataShareValueType::DATASHARE_FLOAT"},
};

static const ScriptEnumValueContract kCallValueTypeValues[] = {
    {"CALL_VALUE_EMPTY", BML_CALL_VALUE_EMPTY, "CallValueType::CALL_VALUE_EMPTY"},
    {"CALL_VALUE_BOOL", BML_CALL_VALUE_BOOL, "CallValueType::CALL_VALUE_BOOL"},
    {"CALL_VALUE_INT", BML_CALL_VALUE_INT, "CallValueType::CALL_VALUE_INT"},
    {"CALL_VALUE_FLOAT", BML_CALL_VALUE_FLOAT, "CallValueType::CALL_VALUE_FLOAT"},
    {"CALL_VALUE_STRING", BML_CALL_VALUE_STRING, "CallValueType::CALL_VALUE_STRING"},
};

static const ScriptEnumContract kEnums[] = {
    {"GameEvent", "enum GameEvent", kGameEventValues, sizeof(kGameEventValues) / sizeof(kGameEventValues[0])},
    {"DirectoryType", "enum DirectoryType", kDirectoryTypeValues, sizeof(kDirectoryTypeValues) / sizeof(kDirectoryTypeValues[0])},
    {"ModKind", "enum ModKind", kModKindValues, sizeof(kModKindValues) / sizeof(kModKindValues[0])},
    {"ModState", "enum ModState", kModStateValues, sizeof(kModStateValues) / sizeof(kModStateValues[0])},
    {"HudFlag", "enum HudFlag", kHudFlagValues, sizeof(kHudFlagValues) / sizeof(kHudFlagValues[0])},
    {"InputDevice", "enum InputDevice", kInputDeviceValues, sizeof(kInputDeviceValues) / sizeof(kInputDeviceValues[0])},
    {"InputKeyEvent", "enum InputKeyEvent", kInputKeyEventValues, sizeof(kInputKeyEventValues) / sizeof(kInputKeyEventValues[0])},
    {"InputButtonState", "enum InputButtonState", kInputButtonStateValues, sizeof(kInputButtonStateValues) / sizeof(kInputButtonStateValues[0])},
    {"CursorPointer", "enum CursorPointer", kCursorPointerValues, sizeof(kCursorPointerValues) / sizeof(kCursorPointerValues[0])},
    {"CommandEventPhase", "enum CommandEventPhase", kCommandEventPhaseValues, sizeof(kCommandEventPhaseValues) / sizeof(kCommandEventPhaseValues[0])},
    {"ConfigPropertyType", "enum ConfigPropertyType", kConfigPropertyTypeValues, sizeof(kConfigPropertyTypeValues) / sizeof(kConfigPropertyTypeValues[0])},
    {"TimerState", "enum TimerState", kTimerStateValues, sizeof(kTimerStateValues) / sizeof(kTimerStateValues[0])},
    {"TimerType", "enum TimerType", kTimerTypeValues, sizeof(kTimerTypeValues) / sizeof(kTimerTypeValues[0])},
    {"TimerTimeBase", "enum TimerTimeBase", kTimerTimeBaseValues, sizeof(kTimerTimeBaseValues) / sizeof(kTimerTimeBaseValues[0])},
    {"DataShareValueType", "enum DataShareValueType", kDataShareValueTypeValues, sizeof(kDataShareValueTypeValues) / sizeof(kDataShareValueTypeValues[0])},
    {"CallValueType", "enum CallValueType", kCallValueTypeValues, sizeof(kCallValueTypeValues) / sizeof(kCallValueTypeValues[0])},
};

static const ScriptEventMemberContract kRenderEventMembers[] = {
    {"int get_Flags() const", "int RenderEvent::get_Flags() const"},
    {"int GetFlags() const", "int RenderEvent::GetFlags() const"},
};

static const ScriptEventMemberContract kCheatEventMembers[] = {
    {"bool get_Enabled() const", "bool CheatEvent::get_Enabled() const"},
    {"bool IsEnabled() const", "bool CheatEvent::IsEnabled() const"},
};

static const ScriptEventMemberContract kLoadObjectEventMembers[] = {
    {"string get_Filename() const", "string LoadObjectEvent::get_Filename() const"},
    {"bool get_IsMap() const", "bool LoadObjectEvent::get_IsMap() const"},
    {"string get_MasterName() const", "string LoadObjectEvent::get_MasterName() const"},
    {"int get_FilterClass() const", "int LoadObjectEvent::get_FilterClass() const"},
    {"bool get_AddToScene() const", "bool LoadObjectEvent::get_AddToScene() const"},
    {"bool get_ReuseMeshes() const", "bool LoadObjectEvent::get_ReuseMeshes() const"},
    {"bool get_ReuseMaterials() const", "bool LoadObjectEvent::get_ReuseMaterials() const"},
    {"bool get_IsDynamic() const", "bool LoadObjectEvent::get_IsDynamic() const"},
    {"int get_ObjectCount() const", "int LoadObjectEvent::get_ObjectCount() const"},
    {"int GetObjectId(int index) const", "int LoadObjectEvent::GetObjectId(int index) const"},
    {"CKObject@ BorrowObject(int index) const", "CKObject@ LoadObjectEvent::BorrowObject(int index) const"},
    {"CKObject@ BorrowMasterObject() const", "CKObject@ LoadObjectEvent::BorrowMasterObject() const"},
};

static const ScriptEventMemberContract kLoadScriptEventMembers[] = {
    {"string get_Filename() const", "string LoadScriptEvent::get_Filename() const"},
    {"int get_ScriptId() const", "int LoadScriptEvent::get_ScriptId() const"},
    {"CKBehavior@ BorrowScript() const", "CKBehavior@ LoadScriptEvent::BorrowScript() const"},
};

static const ScriptEventMemberContract kCommandEventMembers[] = {
    {"CommandEventPhase get_Phase() const", "CommandEventPhase CommandEvent::get_Phase() const"},
    {"bool get_IsPre() const", "bool CommandEvent::get_IsPre() const"},
    {"bool get_IsPost() const", "bool CommandEvent::get_IsPost() const"},
    {"bool get_IsExecute() const", "bool CommandEvent::get_IsExecute() const"},
    {"bool get_IsComplete() const", "bool CommandEvent::get_IsComplete() const"},
    {"string get_CommandName() const", "string CommandEvent::get_CommandName() const"},
    {"int get_ArgCount() const", "int CommandEvent::get_ArgCount() const"},
    {"string GetArg(int index) const", "string CommandEvent::GetArg(int index) const"},
    {"string get_ArgsText() const", "string CommandEvent::get_ArgsText() const"},
    {"bool get_IsCheat() const", "bool CommandEvent::get_IsCheat() const"},
};

static const ScriptEventMemberContract kConfigEventMembers[] = {
    {"string get_ModId() const", "string ConfigEvent::get_ModId() const"},
    {"string get_Category() const", "string ConfigEvent::get_Category() const"},
    {"string get_Key() const", "string ConfigEvent::get_Key() const"},
    {"ConfigPropertyType get_Type() const", "ConfigPropertyType ConfigEvent::get_Type() const"},
    {"bool get_HasProperty() const", "bool ConfigEvent::get_HasProperty() const"},
    {"ConfigProperty@ BorrowProperty() const", "ConfigProperty@ ConfigEvent::BorrowProperty() const"},
};

static const ScriptEventMemberContract kTimerEventMembers[] = {
    {"bool get_IsValid() const", "bool TimerEvent::get_IsValid() const"},
    {"int get_Id() const", "int TimerEvent::get_Id() const"},
    {"string get_Name() const", "string TimerEvent::get_Name() const"},
    {"int get_State() const", "int TimerEvent::get_State() const"},
    {"int get_Type() const", "int TimerEvent::get_Type() const"},
    {"int get_TimeBase() const", "int TimerEvent::get_TimeBase() const"},
    {"int get_CompletedIterations() const", "int TimerEvent::get_CompletedIterations() const"},
    {"int get_RemainingIterations() const", "int TimerEvent::get_RemainingIterations() const"},
    {"float get_Progress() const", "float TimerEvent::get_Progress() const"},
};

static const ScriptEventMemberContract kDataShareEventMembers[] = {
    {"bool get_Exists() const", "bool DataShareEvent::get_Exists() const"},
    {"string get_Key() const", "string DataShareEvent::get_Key() const"},
    {"int get_Type() const", "int DataShareEvent::get_Type() const"},
    {"string get_StringValue() const", "string DataShareEvent::get_StringValue() const"},
    {"bool get_BoolValue() const", "bool DataShareEvent::get_BoolValue() const"},
    {"int get_IntValue() const", "int DataShareEvent::get_IntValue() const"},
    {"float get_FloatValue() const", "float DataShareEvent::get_FloatValue() const"},
};

static const ScriptEventMemberContract kPhysicalizeEventMembers[] = {
    {"int get_TargetId() const", "int PhysicalizeEvent::get_TargetId() const"},
    {"string get_TargetName() const", "string PhysicalizeEvent::get_TargetName() const"},
    {"CK3dEntity@ BorrowTarget() const", "CK3dEntity@ PhysicalizeEvent::BorrowTarget() const"},
    {"bool get_Fixed() const", "bool PhysicalizeEvent::get_Fixed() const"},
    {"float get_Friction() const", "float PhysicalizeEvent::get_Friction() const"},
    {"float get_Elasticity() const", "float PhysicalizeEvent::get_Elasticity() const"},
    {"float get_Mass() const", "float PhysicalizeEvent::get_Mass() const"},
    {"string get_CollisionGroup() const", "string PhysicalizeEvent::get_CollisionGroup() const"},
    {"bool get_StartFrozen() const", "bool PhysicalizeEvent::get_StartFrozen() const"},
    {"bool get_EnableCollision() const", "bool PhysicalizeEvent::get_EnableCollision() const"},
    {"bool get_AutoCalcMassCenter() const", "bool PhysicalizeEvent::get_AutoCalcMassCenter() const"},
    {"float get_LinearDamp() const", "float PhysicalizeEvent::get_LinearDamp() const"},
    {"float get_RotDamp() const", "float PhysicalizeEvent::get_RotDamp() const"},
    {"string get_CollisionSurface() const", "string PhysicalizeEvent::get_CollisionSurface() const"},
    {"float get_MassCenterX() const", "float PhysicalizeEvent::get_MassCenterX() const"},
    {"float get_MassCenterY() const", "float PhysicalizeEvent::get_MassCenterY() const"},
    {"float get_MassCenterZ() const", "float PhysicalizeEvent::get_MassCenterZ() const"},
    {"VxVector get_MassCenter() const", "VxVector PhysicalizeEvent::get_MassCenter() const"},
    {"int get_ConvexCount() const", "int PhysicalizeEvent::get_ConvexCount() const"},
    {"CKMesh@ BorrowConvexMesh(int index) const", "CKMesh@ PhysicalizeEvent::BorrowConvexMesh(int index) const"},
    {"int get_BallCount() const", "int PhysicalizeEvent::get_BallCount() const"},
    {"VxVector GetBallCenter(int index) const", "VxVector PhysicalizeEvent::GetBallCenter(int index) const"},
    {"float GetBallRadius(int index) const", "float PhysicalizeEvent::GetBallRadius(int index) const"},
    {"int get_ConcaveCount() const", "int PhysicalizeEvent::get_ConcaveCount() const"},
    {"CKMesh@ BorrowConcaveMesh(int index) const", "CKMesh@ PhysicalizeEvent::BorrowConcaveMesh(int index) const"},
};

static const ScriptEventMemberContract kObjectEventMembers[] = {
    {"int get_TargetId() const", "int ObjectEvent::get_TargetId() const"},
    {"string get_TargetName() const", "string ObjectEvent::get_TargetName() const"},
    {"CK3dEntity@ BorrowTarget() const", "CK3dEntity@ ObjectEvent::BorrowTarget() const"},
};

static const ScriptEventTypeContract kEventTypes[] = {
    {"RenderEvent", "class RenderEvent", kRenderEventMembers, sizeof(kRenderEventMembers) / sizeof(kRenderEventMembers[0])},
    {"CheatEvent", "class CheatEvent", kCheatEventMembers, sizeof(kCheatEventMembers) / sizeof(kCheatEventMembers[0])},
    {"LoadObjectEvent", "class LoadObjectEvent", kLoadObjectEventMembers, sizeof(kLoadObjectEventMembers) / sizeof(kLoadObjectEventMembers[0])},
    {"LoadScriptEvent", "class LoadScriptEvent", kLoadScriptEventMembers, sizeof(kLoadScriptEventMembers) / sizeof(kLoadScriptEventMembers[0])},
    {"CommandEvent", "class CommandEvent", kCommandEventMembers, sizeof(kCommandEventMembers) / sizeof(kCommandEventMembers[0])},
    {"ConfigEvent", "class ConfigEvent", kConfigEventMembers, sizeof(kConfigEventMembers) / sizeof(kConfigEventMembers[0])},
    {"TimerEvent", "class TimerEvent", kTimerEventMembers, sizeof(kTimerEventMembers) / sizeof(kTimerEventMembers[0])},
    {"DataShareEvent", "class DataShareEvent", kDataShareEventMembers, sizeof(kDataShareEventMembers) / sizeof(kDataShareEventMembers[0])},
    {"PhysicalizeEvent", "class PhysicalizeEvent", kPhysicalizeEventMembers, sizeof(kPhysicalizeEventMembers) / sizeof(kPhysicalizeEventMembers[0])},
    {"ObjectEvent", "class ObjectEvent", kObjectEventMembers, sizeof(kObjectEventMembers) / sizeof(kObjectEventMembers[0])},
};

} // namespace

ScriptContractSpan<ScriptCallbackContract> ScriptApiContract::Callbacks() {
    return {kCallbacks, sizeof(kCallbacks) / sizeof(kCallbacks[0])};
}

ScriptContractSpan<ScriptTypedefContract> ScriptApiContract::Typedefs() {
    return {nullptr, 0};
}

ScriptContractSpan<ScriptIntegerConstantContract> ScriptApiContract::GameEventConstants() {
    return {nullptr, 0};
}

ScriptContractSpan<ScriptIntegerConstantContract> ScriptApiContract::ErrorConstants() {
    return {kErrorConstants, sizeof(kErrorConstants) / sizeof(kErrorConstants[0])};
}

ScriptContractSpan<ScriptEnumContract> ScriptApiContract::Enums() {
    return {kEnums, sizeof(kEnums) / sizeof(kEnums[0])};
}

ScriptContractSpan<ScriptEventTypeContract> ScriptApiContract::EventTypes() {
    return {kEventTypes, sizeof(kEventTypes) / sizeof(kEventTypes[0])};
}

} // namespace BML

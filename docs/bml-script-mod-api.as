// BML Script Mod API reference stub.
// This file documents the script-facing contract implemented by BML.
// It is not loaded by Player and should not be included at runtime.

namespace BML {

enum DirectoryType {
  DIR_WORKING = 0,
  DIR_TEMP = 1,
  DIR_GAME = 2,
  DIR_LOADER = 3,
  DIR_CONFIG = 4
}

enum ModKind {
  MOD_KIND_UNKNOWN = 0,
  MOD_KIND_NATIVE = 1,
  MOD_KIND_SCRIPT = 2
}

enum ModState {
  MOD_STATE_NOT_FOUND = 0,
  MOD_STATE_REGISTERED = 1,
  MOD_STATE_LOADED = 2,
  MOD_STATE_FAILED = 3
}

enum HudFlag {
  HUD_TITLE = 1,
  HUD_FPS = 2,
  HUD_SR = 4
}

typedef int GameEvent;
const GameEvent GAME_EVENT_PRE_START_MENU;
const GameEvent GAME_EVENT_POST_START_MENU;
const GameEvent GAME_EVENT_EXIT_GAME;
const GameEvent GAME_EVENT_PRE_LOAD_LEVEL;
const GameEvent GAME_EVENT_POST_LOAD_LEVEL;
const GameEvent GAME_EVENT_START_LEVEL;
const GameEvent GAME_EVENT_PRE_RESET_LEVEL;
const GameEvent GAME_EVENT_POST_RESET_LEVEL;
const GameEvent GAME_EVENT_PAUSE_LEVEL;
const GameEvent GAME_EVENT_UNPAUSE_LEVEL;
const GameEvent GAME_EVENT_PRE_EXIT_LEVEL;
const GameEvent GAME_EVENT_POST_EXIT_LEVEL;
const GameEvent GAME_EVENT_PRE_NEXT_LEVEL;
const GameEvent GAME_EVENT_POST_NEXT_LEVEL;
const GameEvent GAME_EVENT_DEAD;
const GameEvent GAME_EVENT_PRE_END_LEVEL;
const GameEvent GAME_EVENT_POST_END_LEVEL;
const GameEvent GAME_EVENT_COUNTER_ACTIVE;
const GameEvent GAME_EVENT_COUNTER_INACTIVE;
const GameEvent GAME_EVENT_BALL_NAV_ACTIVE;
const GameEvent GAME_EVENT_BALL_NAV_INACTIVE;
const GameEvent GAME_EVENT_CAM_NAV_ACTIVE;
const GameEvent GAME_EVENT_CAM_NAV_INACTIVE;
const GameEvent GAME_EVENT_BALL_OFF;
const GameEvent GAME_EVENT_PRE_CHECKPOINT_REACHED;
const GameEvent GAME_EVENT_POST_CHECKPOINT_REACHED;
const GameEvent GAME_EVENT_LEVEL_FINISH;
const GameEvent GAME_EVENT_GAME_OVER;
const GameEvent GAME_EVENT_EXTRA_POINT;
const GameEvent GAME_EVENT_PRE_SUB_LIFE;
const GameEvent GAME_EVENT_POST_SUB_LIFE;
const GameEvent GAME_EVENT_PRE_LIFE_UP;
const GameEvent GAME_EVENT_POST_LIFE_UP;

const int ERROR_OK;
const int ERROR_FAIL;
const int ERROR_NOT_FOUND;
const int ERROR_INVALID_PARAMETER;
const int ERROR_INTEROP_TARGET_NOT_FOUND;
const int ERROR_INTEROP_TARGET_FAILED;
const int ERROR_INTEROP_EXPORT_NOT_FOUND;
const int ERROR_INTEROP_EXPORT_AMBIGUOUS;
const int ERROR_INTEROP_BAD_SIGNATURE;
const int ERROR_INTEROP_SIGNATURE_MISMATCH;
const int ERROR_INTEROP_BAD_CALL_FRAME;
const int ERROR_INTEROP_TYPE_MISMATCH;
const int ERROR_INTEROP_TARGET_EXECUTION_FAILED;
const int ERROR_INTEROP_HANDLE_STALE;

enum CommandEventPhase {
  COMMAND_EVENT_PRE = 0,
  COMMAND_EVENT_POST = 1,
  COMMAND_EVENT_EXECUTE = 2,
  COMMAND_EVENT_COMPLETE = 3
}

enum TimerState {
  TIMER_IDLE = 0,
  TIMER_RUNNING = 1,
  TIMER_PAUSED = 2,
  TIMER_COMPLETED = 3,
  TIMER_CANCELLED = 4
}

enum TimerType {
  TIMER_ONCE = 0,
  TIMER_LOOP = 1,
  TIMER_REPEAT = 2,
  TIMER_INTERVAL = 3,
  TIMER_DEBOUNCE = 4,
  TIMER_THROTTLE = 5
}

enum TimerTimeBase {
  TIMER_TIMEBASE_TICK = 0,
  TIMER_TIMEBASE_TIME = 1,
  TIMER_TIMEBASE_REALTIME = 2
}

enum DataShareValueType {
  DATASHARE_STRING = 0,
  DATASHARE_BOOL = 1,
  DATASHARE_INT = 2,
  DATASHARE_FLOAT = 3
}

enum CallValueType {
  CALL_VALUE_EMPTY = 0,
  CALL_VALUE_BOOL = 1,
  CALL_VALUE_INT = 2,
  CALL_VALUE_FLOAT = 3,
  CALL_VALUE_STRING = 4
}

namespace UI {

enum ButtonType {
  BUTTON_MAIN = 0,
  BUTTON_BACK = 1,
  BUTTON_OPTION = 2,
  BUTTON_LEVEL = 3,
  BUTTON_KEY = 4,
  BUTTON_SMALL = 5,
  BUTTON_LEFT = 6,
  BUTTON_RIGHT = 7,
  BUTTON_PLUS = 8,
  BUTTON_MINUS = 9
}

void SetCursorCoord(float x, float y);
float CoordToPixelX(float x);
float CoordToPixelY(float y);
float GetMenuPosX();
float GetMenuPosY();
float GetMenuSizeX();
float GetMenuSizeY();
int CalcPageCount(int totalCount, int pageSize);
bool CanPrevPage(int pageIndex);
bool CanNextPage(int pageIndex, int totalCount, int pageSize);

bool MainButton(const string &in label);
bool BackButton(const string &in label);
bool OptionButton(const string &in label);
bool LevelButton(const string &in label);
bool LevelButton(const string &in label, bool &inout selected);
bool SmallButton(const string &in label);
bool SmallButton(const string &in label, bool &inout selected);
bool LeftButton(const string &in label);
bool RightButton(const string &in label);
bool PlusButton(const string &in label);
bool MinusButton(const string &in label);

void Title(const string &in text, float y = 0.13f, float scale = 1.5f);
void WrappedText(const string &in text, float width, float baseX = 0.0f, float scale = 1.0f);
bool NavLeft(float x = 0.36f, float y = 0.124f);
bool NavRight(float x = 0.6038f, float y = 0.124f);
bool NavBack(float x = 0.4031f, float y = 0.85f);

bool YesNoButton(const string &in label, bool &inout value);
bool InputIntButton(const string &in label, int &inout value, int step = 1, int stepFast = 100);
bool InputFloatButton(const string &in label, float &inout value, float step = 0.0f, float stepFast = 0.0f);
bool SearchBar(string &inout text, float x = 0.4f, float y = 0.18f, float width = 0.2f);

void PlayMenuClickSound();
float GetButtonSizeX(ButtonType type);
float GetButtonSizeY(ButtonType type);
float GetButtonIndent(ButtonType type);
float GetButtonSizeCoordX(ButtonType type);
float GetButtonSizeCoordY(ButtonType type);
float GetButtonIndentCoord(ButtonType type);

}

class RenderEvent {
  int get_Flags() const;
  int GetFlags() const;
}

class CheatEvent {
  bool get_Enabled() const;
  bool IsEnabled() const;
}

class LoadObjectEvent {
  string get_Filename() const;
  bool get_IsMap() const;
  string get_MasterName() const;
  int get_FilterClass() const;
  bool get_AddToScene() const;
  bool get_ReuseMeshes() const;
  bool get_ReuseMaterials() const;
  bool get_IsDynamic() const;
}

class LoadScriptEvent {
  string get_Filename() const;
  int get_ScriptId() const;
}

class CommandEvent {
  int get_Phase() const;
  bool get_IsPre() const;
  bool get_IsPost() const;
  bool get_IsExecute() const;
  bool get_IsComplete() const;
  string get_CommandName() const;
  int get_ArgCount() const;
  string GetArg(int index) const;
  string get_ArgsText() const;
  bool get_IsCheat() const;
}

interface Command {
  string get_Name() const;
  void Execute(const BML::ModContext &in, const BML::CommandEvent &in);
}

class CommandCompletion {
  void Add(const string &in value) const;
  int get_Count() const;
  string At(int index) const;
}

class CommandRef {
  bool get_IsValid() const;
  string get_Name() const;
  string get_Alias() const;
  bool get_IsCheat() const;
  bool get_IsEnabled() const;
  bool SetEnabled(bool enabled);
  bool Unregister();
}

interface Timer {
  bool Tick(const BML::ModContext &in ctx, const BML::TimerEvent &in event);
}

interface DataShareRequest {
  string get_Key() const;
  int get_Type() const;
  void Receive(const BML::ModContext &in ctx, const BML::DataShareEvent &in event);
}

class DataShareEvent {
  bool get_Exists() const;
  string get_Key() const;
  int get_Type() const;
  string get_StringValue() const;
  bool get_BoolValue() const;
  int get_IntValue() const;
  float get_FloatValue() const;
}

class DataShareRequestRef {
  bool get_IsValid() const;
  string get_Key() const;
  int get_Type() const;
  bool Cancel();
}

class TimerEvent {
  bool get_IsValid() const;
  int get_Id() const;
  string get_Name() const;
  int get_State() const;
  int get_Type() const;
  int get_TimeBase() const;
  int get_CompletedIterations() const;
  int get_RemainingIterations() const;
  float get_Progress() const;
}

class PhysicalizeEvent {
  int get_TargetId() const;
  string get_TargetName() const;
  bool get_Fixed() const;
  float get_Friction() const;
  float get_Elasticity() const;
  float get_Mass() const;
  string get_CollisionGroup() const;
  bool get_StartFrozen() const;
  bool get_EnableCollision() const;
  bool get_AutoCalcMassCenter() const;
  float get_LinearDamp() const;
  float get_RotDamp() const;
  string get_CollisionSurface() const;
  float get_MassCenterX() const;
  float get_MassCenterY() const;
  float get_MassCenterZ() const;
  int get_ConvexCount() const;
  int get_BallCount() const;
  int get_ConcaveCount() const;
}

class ObjectEvent {
  int get_TargetId() const;
  string get_TargetName() const;
}

class ModContext {
  string get_ModId() const;
  string GetModId() const;
  string GetModId(int index) const;
  string get_ModName() const;
  string GetModName() const;
  CKContext@ GetCKContext() const;
  CKRenderContext@ GetRenderContext() const;
  CKAttributeManager@ GetAttributeManager() const;
  CKBehaviorManager@ GetBehaviorManager() const;
  CKCollisionManager@ GetCollisionManager() const;
  InputHook@ GetInputManager() const;
  CKMessageManager@ GetMessageManager() const;
  CKPathManager@ GetPathManager() const;
  CKParameterManager@ GetParameterManager() const;
  CKRenderManager@ GetRenderManager() const;
  CKSoundManager@ GetSoundManager() const;
  CKTimeManager@ GetTimeManager() const;
  bool get_IsInGame() const;
  bool GetIsInGame() const;
  bool get_IsInLevel() const;
  bool GetIsInLevel() const;
  bool get_IsPaused() const;
  bool GetIsPaused() const;
  bool get_IsPlaying() const;
  bool GetIsPlaying() const;
  bool get_IsCheatEnabled() const;
  bool GetIsCheatEnabled() const;
  void EnableCheat(bool enable) const;
  void ExitGame() const;
  float GetSRScore() const;
  int GetHSScore() const;
  string GetDirectoryUtf8(int type) const;
  float GetTimeMs() const;
  float GetAbsoluteTimeMs() const;
  float GetDeltaTimeMs() const;
  uint GetFrameCount() const;
  bool IsKeyboardAttached() const;
  bool IsMouseAttached() const;
  bool IsKeyDown(CKKEYBOARD key) const;
  bool IsKeyUp(CKKEYBOARD key) const;
  bool IsKeyPressed(CKKEYBOARD key) const;
  bool IsKeyReleased(CKKEYBOARD key) const;
  bool IsKeyToggled(CKKEYBOARD key) const;
  string GetKeyName(CKKEYBOARD key) const;
  int GetKeyFromName(const string &in name) const;
  bool IsMouseButtonDown(CK_MOUSEBUTTON button) const;
  bool IsMouseClicked(CK_MOUSEBUTTON button) const;
  bool IsMouseToggled(CK_MOUSEBUTTON button) const;
  Vx2DVector GetMousePosition(bool absolute = true) const;
  Vx2DVector GetLastMousePosition() const;
  VxVector GetMouseRelativePosition() const;
  bool IsObjectValid(CKObject@ object) const;
  int GetObjectId(CKObject@ object) const;
  string GetObjectName(CKObject@ object) const;
  int GetObjectClassId(CKObject@ object) const;
  bool IsObjectVisible(CKObject@ object) const;
  bool IsObjectDynamic(CKObject@ object) const;
  int GetBeObjectPriority(CKBeObject@ object) const;
  int GetBeObjectScriptCount(CKBeObject@ object) const;
  int GetBeObjectAttributeCount(CKBeObject@ object) const;
  VxVector Get3dEntityPosition(CK3dEntity@ entity) const;
  VxVector Get3dEntityScale(CK3dEntity@ entity, bool local = true) const;
  int Get3dEntityChildCount(CK3dEntity@ entity) const;
  CK3dEntity@ Get3dEntityParent(CK3dEntity@ entity) const;
  CKDataArray@ GetArrayByName(const string &in name) const;
  CKGroup@ GetGroupByName(const string &in name) const;
  CKMaterial@ GetMaterialByName(const string &in name) const;
  CKMesh@ GetMeshByName(const string &in name) const;
  CK2dEntity@ Get2dEntityByName(const string &in name) const;
  CK3dEntity@ Get3dEntityByName(const string &in name) const;
  CK3dObject@ Get3dObjectByName(const string &in name) const;
  CKCamera@ GetCameraByName(const string &in name) const;
  CKTargetCamera@ GetTargetCameraByName(const string &in name) const;
  CKLight@ GetLightByName(const string &in name) const;
  CKTargetLight@ GetTargetLightByName(const string &in name) const;
  CKSound@ GetSoundByName(const string &in name) const;
  CKTexture@ GetTextureByName(const string &in name) const;
  CKBehavior@ GetScriptByName(const string &in name) const;
  void SetIC(CKBeObject@ object, bool hierarchy = false) const;
  void RestoreIC(CKBeObject@ object, bool hierarchy = false) const;
  void Show(CKBeObject@ object, CK_OBJECT_SHOWOPTION show = CKSHOW, bool hierarchy = false) const;
  int GetHUD() const;
  void SetHUD(int mode) const;
  void ShowTitle(bool show) const;
  void ShowFPS(bool show) const;
  void ShowSRTimer(bool show) const;
  void StartSRTimer() const;
  void PauseSRTimer() const;
  void ResetSRTimer() const;
  float GetSRTime() const;
  TimerRef@ AddTimer(Timer@+ timer) const;
  CommandRef@ RegisterCommand(Command@+ command) const;
  bool UnregisterCommand(const string &in name) const;
  DataShareRequestRef@ RequestDataShare(DataShareRequest@+ request) const;
  bool RegisterBallType(const string &in ballFile, const string &in ballId, const string &in ballName, const string &in objName, float friction, float elasticity, float mass, const string &in collGroup, float linearDamp, float rotDamp, float force, float radius) const;
  bool RegisterFloorType(const string &in floorName, float friction, float elasticity, float mass, const string &in collGroup, bool enableColl) const;
  bool RegisterModulBall(const string &in modulName, bool fixed, float friction, float elasticity, float mass, const string &in collGroup, bool frozen, bool enableColl, bool calcMassCenter, float linearDamp, float rotDamp, float radius) const;
  bool RegisterModulConvex(const string &in modulName, bool fixed, float friction, float elasticity, float mass, const string &in collGroup, bool frozen, bool enableColl, bool calcMassCenter, float linearDamp, float rotDamp) const;
  bool RegisterTrafo(const string &in modulName) const;
  bool RegisterModul(const string &in modulName) const;
  string GetModRootUtf8() const;
  string ResolveModPathUtf8(const string &in relativePath) const;
  bool ModFileExistsUtf8(const string &in relativePath) const;
  bool ModDirectoryExistsUtf8(const string &in relativePath) const;
  string ReadModTextFileUtf8(const string &in relativePath, const string &in defaultValue = "") const;
  int GetCommandCount() const;
  string GetCommandName(int index) const;
  string GetCommandAlias(int index) const;
  string GetCommandDescription(int index) const;
  bool HasCommand(const string &in name) const;
  bool IsCommandCheat(const string &in name) const;
  ModRef@ FindMod(const string &in id) const;
  int GetModCount() const;
  ModRef@ GetMod(int index) const;
  void SendIngameMessage(const string &in message) const;
  void ClearIngameMessages() const;
  void ExecuteCommand(const string &in command) const;
  void SkipRenderForNextTick() const;
  void OpenModsMenu() const;
  void CloseModsMenu() const;
  void OpenMapMenu() const;
  void CloseMapMenu() const;
  string GetConfigString(const string &in key, const string &in defaultValue = "") const;
  void SetConfigString(const string &in key, const string &in value) const;
  bool GetConfigBool(const string &in key, bool defaultValue = false) const;
  void SetConfigBool(const string &in key, bool value) const;
  int GetConfigInt(const string &in key, int defaultValue = 0) const;
  void SetConfigInt(const string &in key, int value) const;
  float GetConfigFloat(const string &in key, float defaultValue = 0.0f) const;
  void SetConfigFloat(const string &in key, float value) const;
  void LogInfo(const string &in message) const;
  void LogWarn(const string &in message) const;
  void LogError(const string &in message) const;
}

class ModRef {
  bool get_IsValid() const;
  bool get_IsScript() const;
  bool get_IsFailed() const;
  // Same diagnostic exposed by native BML_GetModDiagnostic.
  string get_Diagnostic() const;
  string GetDiagnostic() const;
  string get_Id() const;
  string GetId() const;
  string get_Name() const;
  string get_Version() const;
  string get_Author() const;
  string get_Description() const;
  string get_BMLVersion() const;
  int get_BMLVersionMajor() const;
  int get_BMLVersionMinor() const;
  int get_BMLVersionPatch() const;
  int get_Kind() const;
  int get_State() const;
  int CheckDependencies() const;
  int GetDependencyCount() const;
  string GetDependencyId(int index) const;
  string GetDependencyVersion(int index) const;
  int GetDependencyVersionMajor(int index) const;
  int GetDependencyVersionMinor(int index) const;
  int GetDependencyVersionPatch(int index) const;
  bool IsDependencyOptional(int index) const;
  int GetExportCount() const;
  string GetExportName(int index) const;
  string GetExportSignature(int index) const;
  ExportRef@ GetExport(int index) const;
  ExportRef@ FindExport(const string &in name, const string &in signature = "") const;
  int TryFindExport(const string &in name, ExportRef@ &out exportRef, const string &in signature = "") const;
}

class ExportRef {
  bool get_IsValid() const;
  string get_ModId() const;
  string get_Name() const;
  string get_Signature() const;
  int Call(CallFrame@ frame) const;
  int CallVoid() const;
  int CallString(const string &in argument, string &out result) const;
  int CallString(string &out result) const;
  int CallBool(bool argument, bool &out result) const;
  int CallBool(bool &out result) const;
  int CallInt(int argument, int &out result) const;
  int CallInt(int &out result) const;
  int CallFloat(float argument, float &out result) const;
  int CallFloat(float &out result) const;
}

class CallFrame {
  bool get_IsValid() const;
  void Clear();
  int get_ArgCount() const;
  int GetArgCount() const;
  int GetArgType(uint index) const;
  int ClearArg(uint index);
  int SetBool(uint index, bool value);
  int GetBool(uint index, bool &out value) const;
  int SetInt(uint index, int value);
  int GetInt(uint index, int &out value) const;
  int SetFloat(uint index, float value);
  int GetFloat(uint index, float &out value) const;
  int SetString(uint index, const string &in value);
  int GetString(uint index, string &out value) const;
  int SetResultBool(bool value);
  int get_ResultType() const;
  int GetResultType() const;
  int ClearResult();
  int GetResultBool(bool &out value) const;
  int SetResultInt(int value);
  int GetResultInt(int &out value) const;
  int SetResultFloat(float value);
  int GetResultFloat(float &out value) const;
  int SetResultString(const string &in value);
  int GetResultString(string &out value) const;
}

class TimerRef {
  bool get_IsValid() const;
  bool IsValid() const;
  int get_Id() const;
  string get_Name() const;
  int get_State() const;
  int get_CompletedIterations() const;
  int get_RemainingIterations() const;
  float get_Progress() const;
  void Pause();
  void Resume();
  void Cancel();
}

class InputHook {
  bool IsKeyboardAttached() const;
  bool IsMouseAttached() const;
  bool IsKeyDown(CKKEYBOARD key) const;
  bool IsKeyUp(CKKEYBOARD key) const;
  bool IsKeyPressed(CKKEYBOARD key) const;
  bool IsKeyReleased(CKKEYBOARD key) const;
  bool IsKeyToggled(CKKEYBOARD key) const;
  string GetKeyName(CKKEYBOARD key) const;
  int GetKeyFromName(const string &in name) const;
  bool IsMouseButtonDown(CK_MOUSEBUTTON button) const;
  bool IsMouseClicked(CK_MOUSEBUTTON button) const;
  bool IsMouseToggled(CK_MOUSEBUTTON button) const;
  Vx2DVector GetMousePosition(bool absolute = true) const;
  Vx2DVector GetLastMousePosition() const;
  VxVector GetMouseRelativePosition() const;
}

string GetVersion();
int GetVersionMajor();
int GetVersionMinor();
int GetVersionPatch();
string GetErrorString(int errorCode);

bool IsInitialized();
bool IsIngame();
bool IsInLevel();
bool IsPaused();
bool IsPlaying();
bool IsCheatEnabled();
void EnableCheat(bool enable);
void SendIngameMessage(const string &in message);
void ExecuteCommand(const string &in command);
void ExitGame();
void OpenModsMenu();
void CloseModsMenu();
void OpenMapMenu();
void CloseMapMenu();
void ClearIngameMessages();
float GetSRScore();
int GetHSScore();
int GetHUD();
void SetHUD(int mode);
void ShowTitle(bool show);
void ShowFPS(bool show);
void ShowSRTimer(bool show);
void StartSRTimer();
void PauseSRTimer();
void ResetSRTimer();
float GetSRTime();
void SkipRenderForNextTick();

// Borrowed CKAS object wrappers. Do not retain these across level unload, reset, or mod unload.
CKContext@ GetCKContext();
CKRenderContext@ GetRenderContext();
CKAttributeManager@ GetAttributeManager();
CKBehaviorManager@ GetBehaviorManager();
CKCollisionManager@ GetCollisionManager();
InputHook@ GetInputManager();
CKMessageManager@ GetMessageManager();
CKPathManager@ GetPathManager();
CKParameterManager@ GetParameterManager();
CKRenderManager@ GetRenderManager();
CKSoundManager@ GetSoundManager();
CKTimeManager@ GetTimeManager();
float GetTimeMs();
float GetAbsoluteTimeMs();
float GetDeltaTimeMs();
uint GetFrameCount();

bool IsKeyboardAttached();
bool IsMouseAttached();
bool IsKeyDown(CKKEYBOARD key);
bool IsKeyUp(CKKEYBOARD key);
bool IsKeyPressed(CKKEYBOARD key);
bool IsKeyReleased(CKKEYBOARD key);
bool IsKeyToggled(CKKEYBOARD key);
string GetKeyName(CKKEYBOARD key);
int GetKeyFromName(const string &in name);
bool IsMouseButtonDown(CK_MOUSEBUTTON button);
bool IsMouseClicked(CK_MOUSEBUTTON button);
bool IsMouseToggled(CK_MOUSEBUTTON button);
Vx2DVector GetMousePosition(bool absolute = true);
Vx2DVector GetLastMousePosition();
VxVector GetMouseRelativePosition();

bool IsObjectValid(CKObject@ object);
int GetObjectId(CKObject@ object);
string GetObjectName(CKObject@ object);
int GetObjectClassId(CKObject@ object);
bool IsObjectVisible(CKObject@ object);
bool IsObjectDynamic(CKObject@ object);
int GetBeObjectPriority(CKBeObject@ object);
int GetBeObjectScriptCount(CKBeObject@ object);
int GetBeObjectAttributeCount(CKBeObject@ object);
VxVector Get3dEntityPosition(CK3dEntity@ entity);
VxVector Get3dEntityScale(CK3dEntity@ entity, bool local = true);
int Get3dEntityChildCount(CK3dEntity@ entity);
CK3dEntity@ Get3dEntityParent(CK3dEntity@ entity);

CKDataArray@ GetArrayByName(const string &in name);
CKGroup@ GetGroupByName(const string &in name);
CKMaterial@ GetMaterialByName(const string &in name);
CKMesh@ GetMeshByName(const string &in name);
CK2dEntity@ Get2dEntityByName(const string &in name);
CK3dEntity@ Get3dEntityByName(const string &in name);
CK3dObject@ Get3dObjectByName(const string &in name);
CKCamera@ GetCameraByName(const string &in name);
CKTargetCamera@ GetTargetCameraByName(const string &in name);
CKLight@ GetLightByName(const string &in name);
CKTargetLight@ GetTargetLightByName(const string &in name);
CKSound@ GetSoundByName(const string &in name);
CKTexture@ GetTextureByName(const string &in name);
CKBehavior@ GetScriptByName(const string &in name);
void SetIC(CKBeObject@ object, bool hierarchy = false);
void RestoreIC(CKBeObject@ object, bool hierarchy = false);
void Show(CKBeObject@ object, CK_OBJECT_SHOWOPTION show = CKSHOW, bool hierarchy = false);
TimerRef@ AddTimer(Timer@+ timer);
CommandRef@ RegisterCommand(Command@+ command);
bool UnregisterCommand(const string &in name);
bool RegisterBallType(const string &in ballFile, const string &in ballId, const string &in ballName, const string &in objName, float friction, float elasticity, float mass, const string &in collGroup, float linearDamp, float rotDamp, float force, float radius);
bool RegisterFloorType(const string &in floorName, float friction, float elasticity, float mass, const string &in collGroup, bool enableColl);
bool RegisterModulBall(const string &in modulName, bool fixed, float friction, float elasticity, float mass, const string &in collGroup, bool frozen, bool enableColl, bool calcMassCenter, float linearDamp, float rotDamp, float radius);
bool RegisterModulConvex(const string &in modulName, bool fixed, float friction, float elasticity, float mass, const string &in collGroup, bool frozen, bool enableColl, bool calcMassCenter, float linearDamp, float rotDamp);
bool RegisterTrafo(const string &in modulName);
bool RegisterModul(const string &in modulName);

string GetDirectoryUtf8(DirectoryType type);
bool FileExistsUtf8(const string &in path);
bool DirectoryExistsUtf8(const string &in path);
bool PathExistsUtf8(const string &in path);
bool IsPathValidUtf8(const string &in path);
bool IsAbsolutePathUtf8(const string &in path);
bool IsRelativePathUtf8(const string &in path);
string CombinePathUtf8(const string &in left, const string &in right);
string NormalizePathUtf8(const string &in path);
string GetFileNameUtf8(const string &in path);
string GetExtensionUtf8(const string &in path);
string RemoveExtensionUtf8(const string &in path);
string ReadTextFileUtf8(const string &in path);

bool DataShareSetString(const string &in key, const string &in value, const string &in name = "BML");
string DataShareGetString(const string &in key, const string &in defaultValue = "", const string &in name = "BML");
bool DataShareSetBool(const string &in key, bool value, const string &in name = "BML");
bool DataShareGetBool(const string &in key, bool defaultValue = false, const string &in name = "BML");
bool DataShareSetInt(const string &in key, int value, const string &in name = "BML");
int DataShareGetInt(const string &in key, int defaultValue = 0, const string &in name = "BML");
bool DataShareSetFloat(const string &in key, float value, const string &in name = "BML");
float DataShareGetFloat(const string &in key, float defaultValue = 0.0f, const string &in name = "BML");
bool DataShareHas(const string &in key, const string &in name = "BML");
void DataShareRemove(const string &in key, const string &in name = "BML");
int DataShareSizeOf(const string &in key, const string &in name = "BML");
DataShareRequestRef@ RequestDataShare(DataShareRequest@+ request);

ModRef@ FindMod(const string &in id);
int GetModCount();
string GetModId(int index);
ModRef@ GetMod(int index);

} // namespace BML

// Advanced ImGui declarations are generated in docs/bml-imgui-api.as.
// Prefer BML::UI for normal menu scripts; use ImGui only from render callbacks.

// Optional script mod class methods. BML caches these exact declarations at OnLoad time.
class BMLScriptModContract {
  string Echo(const string &in value);

  void OnLoad(const BML::ModContext &in);
  void OnUnload(const BML::ModContext &in);
  void OnProcess(const BML::ModContext &in);
  void OnRender(const BML::ModContext &in, const BML::RenderEvent &in);
  void OnGameEvent(const BML::ModContext &in, BML::GameEvent event);
  void OnCheatEnabled(const BML::ModContext &in, const BML::CheatEvent &in);
  void OnLoadObject(const BML::ModContext &in, const BML::LoadObjectEvent &in);
  void OnLoadScript(const BML::ModContext &in, const BML::LoadScriptEvent &in);
  void OnCommandEvent(const BML::ModContext &in, const BML::CommandEvent &in);
  void OnPhysicalize(const BML::ModContext &in, const BML::PhysicalizeEvent &in);
  void OnUnphysicalize(const BML::ModContext &in, const BML::ObjectEvent &in);
}

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

enum ReloadPhase {
  RELOAD_NONE = 0,
  RELOAD_UNLOAD = 1,
  RELOAD_LOAD = 2,
  RELOAD_ROLLBACK = 3,
  RELOAD_RECOVERY = 4,
  RELOAD_CLEANUP = 5,
  RELOAD_SAVE_STATE = 6,
  RELOAD_MIGRATE_STATE = 7,
  RELOAD_RESTORE_STATE = 8
}

enum HudFlag {
  HUD_TITLE = 1,
  HUD_FPS = 2,
  HUD_SR = 4
}

enum InputDevice {
  INPUT_DEVICE_KEYBOARD = 0,
  INPUT_DEVICE_MOUSE = 1,
  INPUT_DEVICE_JOYSTICK = 2,
  INPUT_DEVICE_COUNT = 3
}

enum InputKeyEvent {
  INPUT_KEY_NONE = 0,
  INPUT_KEY_PRESSED = 1,
  INPUT_KEY_RELEASED = 2
}

enum InputButtonState {
  INPUT_BUTTON_IDLE = 0,
  INPUT_BUTTON_PRESSED = 1,
  INPUT_BUTTON_RELEASED = 2
}

enum CursorPointer {
  CURSOR_NORMALSELECT = 1,
  CURSOR_BUSY = 2,
  CURSOR_MOVE = 3,
  CURSOR_LINKSELECT = 4
}

enum GameEvent {
  GAME_EVENT_PRE_START_MENU = 0,
  GAME_EVENT_POST_START_MENU = 1,
  GAME_EVENT_EXIT_GAME = 2,
  GAME_EVENT_PRE_LOAD_LEVEL = 3,
  GAME_EVENT_POST_LOAD_LEVEL = 4,
  GAME_EVENT_START_LEVEL = 5,
  GAME_EVENT_PRE_RESET_LEVEL = 6,
  GAME_EVENT_POST_RESET_LEVEL = 7,
  GAME_EVENT_PAUSE_LEVEL = 8,
  GAME_EVENT_UNPAUSE_LEVEL = 9,
  GAME_EVENT_PRE_EXIT_LEVEL = 10,
  GAME_EVENT_POST_EXIT_LEVEL = 11,
  GAME_EVENT_PRE_NEXT_LEVEL = 12,
  GAME_EVENT_POST_NEXT_LEVEL = 13,
  GAME_EVENT_DEAD = 14,
  GAME_EVENT_PRE_END_LEVEL = 15,
  GAME_EVENT_POST_END_LEVEL = 16,
  GAME_EVENT_COUNTER_ACTIVE = 17,
  GAME_EVENT_COUNTER_INACTIVE = 18,
  GAME_EVENT_BALL_NAV_ACTIVE = 19,
  GAME_EVENT_BALL_NAV_INACTIVE = 20,
  GAME_EVENT_CAM_NAV_ACTIVE = 21,
  GAME_EVENT_CAM_NAV_INACTIVE = 22,
  GAME_EVENT_BALL_OFF = 23,
  GAME_EVENT_PRE_CHECKPOINT_REACHED = 24,
  GAME_EVENT_POST_CHECKPOINT_REACHED = 25,
  GAME_EVENT_LEVEL_FINISH = 26,
  GAME_EVENT_GAME_OVER = 27,
  GAME_EVENT_EXTRA_POINT = 28,
  GAME_EVENT_PRE_SUB_LIFE = 29,
  GAME_EVENT_POST_SUB_LIFE = 30,
  GAME_EVENT_PRE_LIFE_UP = 31,
  GAME_EVENT_POST_LIFE_UP = 32
}

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
const int ERROR_INTEROP_UNSUPPORTED;

enum CommandEventPhase {
  COMMAND_EVENT_PRE = 0,
  COMMAND_EVENT_POST = 1,
  COMMAND_EVENT_EXECUTE = 2,
  COMMAND_EVENT_COMPLETE = 3
}

enum ConfigPropertyType {
  CONFIG_PROPERTY_STRING = 0,
  CONFIG_PROPERTY_BOOLEAN = 1,
  CONFIG_PROPERTY_INTEGER = 2,
  CONFIG_PROPERTY_KEY = 3,
  CONFIG_PROPERTY_FLOAT = 4,
  CONFIG_PROPERTY_NONE = 5
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
  CALL_VALUE_STRING = 4,
  CALL_VALUE_BOOL_ARRAY = 16,
  CALL_VALUE_INT_ARRAY = 17,
  CALL_VALUE_FLOAT_ARRAY = 18,
  CALL_VALUE_STRING_ARRAY = 19,
  CALL_VALUE_BUFFER = 20,
  CALL_VALUE_OBJECT_ID = 21
}

enum FontType {
  FONT_NONE = 0,
  FONT_GAME_NORMAL = 1,
  FONT_GAME_LARGE = 2,
  FONT_GAME_SMALL = 3,
  FONT_GAME_SMALL_GRAY = 4,
  FONT_GAME_HUGE = 5,
  FONT_CREDITS_SMALL = 6,
  FONT_CREDITS_BIG = 7
}

class VxRect {
  float Left;
  float Top;
  float Right;
  float Bottom;
}

class PhysicalizeDefinition {
  bool Fixed;
  float Friction;
  float Elasticity;
  float Mass;
  string CollisionGroup;
  bool StartFrozen;
  bool EnableCollision;
  bool CalcMassCenter;
  float LinearDamp;
  float RotDamp;
  string CollisionSurface;
  VxVector MassCenter;
}

class ObjectLoadOptions {
  string File;
  bool Rename;
  string MasterName;
  int FilterClass;
  bool AddToScene;
  bool ReuseMeshes;
  bool ReuseMaterials;
  bool Dynamic;
}

class ObjectLoadResult {
  bool Success;
  int Count;
  CKObject@ BorrowMainObject() const;
  CKObject@ BorrowObject(int index) const;
}

class Text2DDefinition {
  FontType Font;
  string Text;
  int Align;
  VxRect Margin;
  Vx2DVector Offset;
  Vx2DVector ParagraphIndent;
  float CaretSize;
  int Flags;
}

class BallTypeDefinition {
  string BallFile;
  string BallId;
  string BallName;
  string ObjectName;
  float Friction;
  float Elasticity;
  float Mass;
  string CollisionGroup;
  float LinearDamp;
  float RotDamp;
  float Force;
  float Radius;
}

class FloorTypeDefinition {
  string Name;
  float Friction;
  float Elasticity;
  float Mass;
  string CollisionGroup;
  bool EnableCollision;
}

class ModuleBallDefinition {
  string Name;
  bool Fixed;
  float Friction;
  float Elasticity;
  float Mass;
  string CollisionGroup;
  bool StartFrozen;
  bool EnableCollision;
  bool CalcMassCenter;
  float LinearDamp;
  float RotDamp;
  float Radius;
}

class ModuleConvexDefinition {
  string Name;
  bool Fixed;
  float Friction;
  float Elasticity;
  float Mass;
  string CollisionGroup;
  bool StartFrozen;
  bool EnableCollision;
  bool CalcMassCenter;
  float LinearDamp;
  float RotDamp;
}

class TrafoDefinition {
  string Name;
}

class ModuleDefinition {
  string Name;
}

namespace Path {
bool Exists(const string &in path);
bool IsFile(const string &in path);
bool IsDirectory(const string &in path);
bool IsValid(const string &in path);
bool IsAbsolute(const string &in path);
bool IsRelative(const string &in path);
string Combine(const string &in left, const string &in right);
string Normalize(const string &in path);
string FileName(const string &in path);
string Extension(const string &in path);
string RemoveExtension(const string &in path);
}

namespace CK {
bool IsValid(CKObject@ object);
int GetId(CKObject@ object);
string GetName(CKObject@ object);
int GetClassId(CKObject@ object);
bool IsVisible(CKObject@ object);
bool IsDynamic(CKObject@ object);

int GetPriority(CKBeObject@ object);
int GetScriptCount(CKBeObject@ object);
int GetAttributeCount(CKBeObject@ object);
void SetIC(CKBeObject@ object, bool hierarchy = false);
void RestoreIC(CKBeObject@ object, bool hierarchy = false);
void Show(CKBeObject@ object, CK_OBJECT_SHOWOPTION show = CKSHOW, bool hierarchy = false);

VxVector GetPosition(CK3dEntity@ entity);
void SetPosition(CK3dEntity@ entity, const VxVector &in position);
VxVector GetScale(CK3dEntity@ entity, bool local = true);
void SetScale(CK3dEntity@ entity, const VxVector &in scale, bool local = true);
int GetChildCount(CK3dEntity@ entity);
CK3dEntity@ BorrowChild(CK3dEntity@ entity, int index);
CK3dEntity@ BorrowParent(CK3dEntity@ entity);

int GetRowCount(CKDataArray@ array);
int GetColumnCount(CKDataArray@ array);
string GetColumnName(CKDataArray@ array, int column);
int FindColumn(CKDataArray@ array, const string &in name);
string GetString(CKDataArray@ array, int row, int column, const string &in defaultValue = "");
bool GetBool(CKDataArray@ array, int row, int column, bool defaultValue = false);
int GetInt(CKDataArray@ array, int row, int column, int defaultValue = 0);
float GetFloat(CKDataArray@ array, int row, int column, float defaultValue = 0.0f);
bool SetString(CKDataArray@ array, int row, int column, const string &in value);
bool SetBool(CKDataArray@ array, int row, int column, bool value);
bool SetInt(CKDataArray@ array, int row, int column, int value);
bool SetFloat(CKDataArray@ array, int row, int column, float value);
ObjectLoadResult@ LoadObject(const BML::ObjectLoadOptions &in options);
}

namespace Physics {
bool PhysicalizeConvex(CK3dEntity@ target, const BML::PhysicalizeDefinition &in definition, CKMesh@ mesh = null);
bool PhysicalizeBall(CK3dEntity@ target, const BML::PhysicalizeDefinition &in definition, const VxVector &in center, float radius);
bool PhysicalizeConcave(CK3dEntity@ target, const BML::PhysicalizeDefinition &in definition, CKMesh@ mesh = null);
bool Unphysicalize(CK3dEntity@ target);
bool SetForce(CK3dEntity@ target, const VxVector &in position, CK3dEntity@ positionReference, const VxVector &in direction, CK3dEntity@ directionReference, float force);
bool ClearForce(CK3dEntity@ target);
bool Impulse(CK3dEntity@ target, const VxVector &in position, CK3dEntity@ positionReference, const VxVector &in direction, CK3dEntity@ directionReference, float impulse);
bool WakeUp(CK3dEntity@ target);
}

namespace Text {
CKBehavior@ Create2DText(CKBehavior@ ownerScript, CK2dEntity@ target, const BML::Text2DDefinition &in definition);
CKBehavior@ Create2DText(CKBehavior@ ownerScript, CK2dEntity@ target, const BML::Text2DDefinition &in definition, CKMaterial@ backgroundMaterial, CKMaterial@ caretMaterial);
}

namespace Hook {
HookBlockRef@ Create(CKBehavior@ ownerScript, HookBlockCallback@+ callback, const string &in name = "", int inputCount = 1, int outputCount = 1);
HookBlockRef@ InsertAfter(CKBehavior@ ownerScript, CKBehavior@ source, HookBlockCallback@+ callback, const string &in name = "", int sourceOutput = 0, int targetInput = -1);
HookBlockRef@ InsertBefore(CKBehavior@ ownerScript, CKBehavior@ target, HookBlockCallback@+ callback, const string &in name = "", int sourceOutput = -1, int targetInput = 0);
HookBlockRef@ InsertBetween(CKBehavior@ ownerScript, CKBehavior@ source, CKBehavior@ target, HookBlockCallback@+ callback, const string &in name = "", int sourceOutput = 0, int targetInput = 0);
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

void SendMessage(const string &in message);
void ClearMessages();

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
bool OkButton(const string &in label);
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
bool KeyButton(const string &in label, bool &inout toggled, int &inout keyChord);

void Title(const string &in text, float y = 0.13f, float scale = 1.5f);
void WrappedText(const string &in text, float width, float baseX = 0.0f, float scale = 1.0f);
bool NavLeft(float x = 0.36f, float y = 0.124f);
bool NavRight(float x = 0.6038f, float y = 0.124f);
bool NavBack(float x = 0.4031f, float y = 0.85f);

bool YesNoButton(const string &in label, bool &inout value);
bool RadioButtonText(const string &in label, int &inout currentItem, const string &in items);
bool InputTextButton(const string &in label, string &inout value, int maxLength = 256);
bool InputIntButton(const string &in label, int &inout value, int step = 1, int stepFast = 100);
bool InputFloatButton(const string &in label, float &inout value, float step = 0.0f, float stepFast = 0.0f);
bool SearchBar(string &inout text, float x = 0.4f, float y = 0.18f, float width = 0.2f);

void PlayMenuClickSound();
int CKKeyToImGuiKey(CKKEYBOARD key);
CKKEYBOARD ImGuiKeyToCKKey(int key);
string KeyChordToString(int keyChord);
bool SetKeyChordFromIO(int &inout keyChord);
float GetButtonSizeX(ButtonType type);
float GetButtonSizeY(ButtonType type);
float GetButtonIndent(ButtonType type);
float GetButtonSizeCoordX(ButtonType type);
float GetButtonSizeCoordY(ButtonType type);
float GetButtonIndentCoord(ButtonType type);
}

namespace Menu {
void OpenModsMenu();
void CloseModsMenu();
void OpenMapMenu();
void CloseMapMenu();
}

namespace HUD {
int GetMode();
void SetMode(int mode);
void ShowTitle(bool show);
void ShowFPS(bool show);
void ShowSRTimer(bool show);
void StartSRTimer();
void PauseSRTimer();
void ResetSRTimer();
float GetSRTime();
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
  int get_ObjectCount() const;
  int GetObjectId(int index) const;
  CKObject@ BorrowObject(int index) const;
  CKObject@ BorrowMasterObject() const;
}

class LoadScriptEvent {
  string get_Filename() const;
  int get_ScriptId() const;
  CKBehavior@ BorrowScript() const;
}

class CommandEvent {
  CommandEventPhase get_Phase() const;
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

class ConfigEvent {
  string get_ModId() const;
  string get_Category() const;
  string get_Key() const;
  ConfigPropertyType get_Type() const;
  bool get_HasProperty() const;
  ConfigProperty@ BorrowProperty() const;
}

interface Command {
  string get_Name() const;
  void Execute(const BML::ModContext &in, const BML::CommandEvent &in);
}

funcdef void CommandCallback(const BML::ModContext &in ctx, const BML::CommandEvent &in event);
funcdef void CommandCompletionCallback(const BML::ModContext &in ctx, const BML::CommandEvent &in event, BML::CommandCompletion &inout completions);

class CommandDefinition {
  string Name;
  string Alias;
  string Description;
  string Usage;
  string Category;
  bool Cheat;
  bool Hidden;
  bool Enabled;
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

funcdef void TimerCallback(const BML::ModContext &in ctx, const BML::TimerEvent &in event);
funcdef bool TimerLoopCallback(const BML::ModContext &in ctx, const BML::TimerEvent &in event);

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

interface DataShareRequest {
  string get_Key() const;
  int get_Type() const;
  void Receive(const BML::ModContext &in ctx, const BML::DataShareEvent &in event);
}

funcdef void DataShareCallback(const BML::ModContext &in ctx, const BML::DataShareEvent &in event);

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

class HookBlockEvent {
  bool get_IsValid() const;
  int get_BlockId() const;
  string get_BlockName() const;
  float get_DeltaTime() const;
  int get_InputCount() const;
  int get_OutputCount() const;
  CKBehavior@ BorrowBlock() const;
  CKBehavior@ BorrowOwnerScript() const;
  bool ActivateOutput(int index) const;
  void ActivateAllOutputs() const;
}

funcdef int HookBlockCallback(const BML::ModContext &in ctx, const BML::HookBlockEvent &in event);

class HookBlockRef {
  bool get_IsValid() const;
  bool get_IsInstalled() const;
  bool get_Enabled() const;
  void set_Enabled(bool enabled);
  bool SetEnabled(bool enabled);
  bool get_AutoActivateOutputs() const;
  void set_AutoActivateOutputs(bool enabled);
  bool SetAutoActivateOutputs(bool enabled);
  int get_BlockId() const;
  string get_Name() const;
  CKBehavior@ BorrowBlock() const;
  CKBehavior@ BorrowOwnerScript() const;
  bool Uninstall();
}

class PhysicalizeEvent {
  int get_TargetId() const;
  string get_TargetName() const;
  CK3dEntity@ BorrowTarget() const;
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
  VxVector get_MassCenter() const;
  int get_ConvexCount() const;
  CKMesh@ BorrowConvexMesh(int index) const;
  int get_BallCount() const;
  VxVector GetBallCenter(int index) const;
  float GetBallRadius(int index) const;
  int get_ConcaveCount() const;
  CKMesh@ BorrowConcaveMesh(int index) const;
}

class ObjectEvent {
  int get_TargetId() const;
  string get_TargetName() const;
  CK3dEntity@ BorrowTarget() const;
}

class InputHook {
  bool get_IsValid() const;
  bool IsValid() const;
  void EnableKeyboardRepetition(bool enable = true) const;
  bool IsKeyboardRepetitionEnabled() const;
  bool IsKeyboardAttached() const;
  bool IsMouseAttached() const;
  bool IsJoystickAttached(int joystick) const;
  bool IsKeyDown(CKKEYBOARD key) const;
  bool IsKeyDown(CKKEYBOARD key, uint &out stamp) const;
  bool IsKeyUp(CKKEYBOARD key) const;
  bool IsKeyPressed(CKKEYBOARD key) const;
  bool IsKeyReleased(CKKEYBOARD key) const;
  bool IsKeyToggled(CKKEYBOARD key) const;
  bool IsKeyToggled(CKKEYBOARD key, uint &out stamp) const;
  string GetKeyName(CKKEYBOARD key) const;
  int GetKeyFromName(const string &in name) const;
  int GetKeyboardState(CKKEYBOARD key) const;
  bool IsKeyboardStateDown(CKKEYBOARD key) const;
  int GetNumberOfKeyInBuffer() const;
  int GetKeyFromBuffer(int index, CKKEYBOARD &out key, uint &out timestamp) const;
  bool IsMouseButtonDown(CK_MOUSEBUTTON button) const;
  bool IsMouseClicked(CK_MOUSEBUTTON button) const;
  bool IsMouseToggled(CK_MOUSEBUTTON button) const;
  int GetMouseButtonState(CK_MOUSEBUTTON button) const;
  Vx2DVector GetMousePosition(bool absolute = true) const;
  Vx2DVector GetLastMousePosition() const;
  VxVector GetMouseRelativePosition() const;
  VxVector GetJoystickPosition(int joystick) const;
  VxVector GetJoystickRotation(int joystick) const;
  Vx2DVector GetJoystickSliders(int joystick) const;
  float GetJoystickPointOfViewAngle(int joystick) const;
  uint GetJoystickButtonsState(int joystick) const;
  bool IsJoystickButtonDown(int joystick, int button) const;
  void Pause(bool pause) const;
  void ShowCursor(bool show) const;
  bool GetCursorVisibility() const;
  int GetSystemCursor() const;
  void SetSystemCursor(int cursor) const;
  bool IsBlock() const;
  void SetBlock(bool block) const;
  int IsBlocked(InputDevice device) const;
  void Block(InputDevice device) const;
  void Unblock(InputDevice device) const;
}

class Logger {
  bool get_IsValid() const;
  bool IsValid() const;
  void Info(const string &in message) const;
  void Warn(const string &in message) const;
  void Error(const string &in message) const;
}

class ConfigProperty {
  bool get_IsValid() const;
  bool IsValid() const;
  ConfigPropertyType get_Type() const;
  ConfigPropertyType GetType() const;
  string GetString(const string &in defaultValue = "") const;
  bool GetBoolean(bool defaultValue = false) const;
  int GetInteger(int defaultValue = 0) const;
  float GetFloat(float defaultValue = 0.0f) const;
  CKKEYBOARD GetKey(CKKEYBOARD defaultValue = 0) const;
  void SetString(const string &in value) const;
  void SetBoolean(bool value) const;
  void SetInteger(int value) const;
  void SetFloat(float value) const;
  void SetKey(CKKEYBOARD value) const;
  void SetComment(const string &in comment) const;
  void SetDefaultString(const string &in value) const;
  void SetDefaultBoolean(bool value) const;
  void SetDefaultInteger(int value) const;
  void SetDefaultFloat(float value) const;
  void SetDefaultKey(CKKEYBOARD value) const;
}

class Config {
  bool get_IsValid() const;
  bool IsValid() const;
  bool HasCategory(const string &in category) const;
  bool HasKey(const string &in category, const string &in key) const;
  ConfigProperty@ GetProperty(const string &in category, const string &in key) const;
  void SetCategoryComment(const string &in category, const string &in comment) const;
}

class ModContext {
  bool get_HasContext() const;
  bool HasContext() const;
  string get_ModId() const;
  string GetModId() const;
  string GetModId(int index) const;
  string get_ModName() const;
  string GetModName() const;
  bool get_IsReloading() const;
  bool IsReloading() const;
  ReloadPhase get_ReloadPhase() const;
  ReloadPhase GetReloadPhase() const;
  uint get_ReloadAttemptId() const;
  uint GetReloadAttemptId() const;
  uint get_ModGeneration() const;
  uint GetModGeneration() const;
  uint get_RuntimeGeneration() const;
  uint GetRuntimeGeneration() const;

  CKContext@ BorrowCKContext() const;
  CKRenderContext@ BorrowRenderContext() const;
  CKAttributeManager@ BorrowAttributeManager() const;
  CKBehaviorManager@ BorrowBehaviorManager() const;
  CKCollisionManager@ BorrowCollisionManager() const;
  InputHook@ BorrowInputManager() const;
  CKMessageManager@ BorrowMessageManager() const;
  CKPathManager@ BorrowPathManager() const;
  CKParameterManager@ BorrowParameterManager() const;
  CKRenderManager@ BorrowRenderManager() const;
  CKSoundManager@ BorrowSoundManager() const;
  CKTimeManager@ BorrowTimeManager() const;
  CKDataArray@ BorrowDataArrayByName(const string &in name) const;
  CKGroup@ BorrowGroupByName(const string &in name) const;
  CKMaterial@ BorrowMaterialByName(const string &in name) const;
  CKMesh@ BorrowMeshByName(const string &in name) const;
  CK2dEntity@ Borrow2dEntityByName(const string &in name) const;
  CK3dEntity@ Borrow3dEntityByName(const string &in name) const;
  CK3dObject@ Borrow3dObjectByName(const string &in name) const;
  CKCamera@ BorrowCameraByName(const string &in name) const;
  CKTargetCamera@ BorrowTargetCameraByName(const string &in name) const;
  CKLight@ BorrowLightByName(const string &in name) const;
  CKTargetLight@ BorrowTargetLightByName(const string &in name) const;
  CKSound@ BorrowSoundByName(const string &in name) const;
  CKTexture@ BorrowTextureByName(const string &in name) const;
  CKBehavior@ BorrowScriptByName(const string &in name) const;
  Logger@ BorrowLogger() const;
  Config@ BorrowConfig() const;

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
  void SendIngameMessage(const string &in message) const;
  void ClearIngameMessages() const;
  void ExecuteCommand(const string &in command) const;
  void SkipRenderForNextTick() const;

  float GetSRScore() const;
  int GetHSScore() const;
  int GetHUD() const;
  void SetHUD(int mode) const;
  void ShowTitle(bool show) const;
  void ShowFPS(bool show) const;
  void ShowSRTimer(bool show) const;
  void StartSRTimer() const;
  void PauseSRTimer() const;
  void ResetSRTimer() const;
  float GetSRTime() const;
  void OpenModsMenu() const;
  void CloseModsMenu() const;
  void OpenMapMenu() const;
  void CloseMapMenu() const;

  string GetDirectoryUtf8(int type) const;
  string GetModRootUtf8() const;
  string ResolveModPathUtf8(const string &in relativePath) const;
  bool ModFileExistsUtf8(const string &in relativePath) const;
  bool ModDirectoryExistsUtf8(const string &in relativePath) const;
  string ReadModTextFileUtf8(const string &in relativePath, const string &in defaultValue = "") const;

  TimerRef@ AddTimer(Timer@+ timer) const;
  TimerRef@ SetTimeoutTicks(uint delayTicks, TimerCallback@+ callback, const string &in name = "") const;
  TimerRef@ SetTimeout(float delayMs, TimerCallback@+ callback, const string &in name = "") const;
  TimerRef@ SetIntervalTicks(uint delayTicks, TimerLoopCallback@+ callback, const string &in name = "") const;
  TimerRef@ SetInterval(float delayMs, TimerLoopCallback@+ callback, const string &in name = "") const;
  CommandRef@ RegisterCommand(Command@+ command) const;
  CommandRef@ RegisterCommand(const CommandDefinition &in definition, CommandCallback@+ execute, CommandCompletionCallback@+ complete = null) const;
  bool UnregisterCommand(const string &in name) const;
  DataShareRequestRef@ RequestDataShare(DataShareRequest@+ request) const;
  DataShareRequestRef@ RequestDataShare(const string &in key, int type, DataShareCallback@+ callback, const string &in name = "") const;
  HookBlockRef@ CreateHookBlock(CKBehavior@ ownerScript, HookBlockCallback@+ callback, const string &in name = "", int inputCount = 1, int outputCount = 1) const;
  HookBlockRef@ InsertHookBlockAfter(CKBehavior@ ownerScript, CKBehavior@ source, HookBlockCallback@+ callback, const string &in name = "", int sourceOutput = 0, int targetInput = -1) const;
  HookBlockRef@ InsertHookBlockBefore(CKBehavior@ ownerScript, CKBehavior@ target, HookBlockCallback@+ callback, const string &in name = "", int sourceOutput = -1, int targetInput = 0) const;
  HookBlockRef@ InsertHookBlockBetween(CKBehavior@ ownerScript, CKBehavior@ source, CKBehavior@ target, HookBlockCallback@+ callback, const string &in name = "", int sourceOutput = 0, int targetInput = 0) const;
  bool RegisterBallType(const BallTypeDefinition &in definition) const;
  bool RegisterFloorType(const FloorTypeDefinition &in definition) const;
  bool RegisterModule(const ModuleBallDefinition &in definition) const;
  bool RegisterModule(const ModuleConvexDefinition &in definition) const;
  bool RegisterModule(const TrafoDefinition &in definition) const;
  bool RegisterModule(const ModuleDefinition &in definition) const;

  int GetCommandCount() const;
  string GetCommandName(int index) const;
  string GetCommandAlias(int index) const;
  string GetCommandDescription(int index) const;
  bool HasCommand(const string &in name) const;
  bool IsCommandCheat(const string &in name) const;
  ModRef@ FindMod(const string &in id) const;
  int GetModCount() const;
  ModRef@ GetMod(int index) const;
}

class ModRef {
  bool get_IsValid() const;
  bool get_IsScript() const;
  bool get_IsFailed() const;
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
  int SetArray(uint index, const array<bool> &in values);
  int GetArray(uint index, array<bool>@ &out values) const;
  int SetArray(uint index, const array<int> &in values);
  int GetArray(uint index, array<int>@ &out values) const;
  int SetArray(uint index, const array<float> &in values);
  int GetArray(uint index, array<float>@ &out values) const;
  int SetArray(uint index, const array<string> &in values);
  int GetArray(uint index, array<string>@ &out values) const;
  int SetArray(uint index, const array<uint8> &in values);
  int GetArray(uint index, array<uint8>@ &out values) const;
  int SetObjectId(uint index, int objectId);
  int GetObjectId(uint index, int &out objectId) const;
  int SetObject(uint index, CKObject@ object);
  int GetObject(uint index, CKObject@ &out object) const;
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
  int SetResultArray(const array<bool> &in values);
  int GetResultArray(array<bool>@ &out values) const;
  int SetResultArray(const array<int> &in values);
  int GetResultArray(array<int>@ &out values) const;
  int SetResultArray(const array<float> &in values);
  int GetResultArray(array<float>@ &out values) const;
  int SetResultArray(const array<string> &in values);
  int GetResultArray(array<string>@ &out values) const;
  int SetResultArray(const array<uint8> &in values);
  int GetResultArray(array<uint8>@ &out values) const;
  int SetResultObjectId(int objectId);
  int GetResultObjectId(int &out objectId) const;
  int SetResultObject(CKObject@ object);
  int GetResultObject(CKObject@ &out object) const;
}

string GetVersion();
int GetVersionMajor();
int GetVersionMinor();
int GetVersionPatch();
string GetErrorString(int errorCode);
string GetGameEventName(GameEvent event);
bool BorrowCurrentContext(ModContext &out context);

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

} // namespace BML

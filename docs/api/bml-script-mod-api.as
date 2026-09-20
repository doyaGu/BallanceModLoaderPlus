// BML Script Mod API reference stub.
// This file documents the script-facing API implemented by BML.
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

enum StateValueType {
  STATE_VALUE_EMPTY = 0,
  STATE_VALUE_BOOL = 1,
  STATE_VALUE_INT = 2,
  STATE_VALUE_FLOAT = 3,
  STATE_VALUE_STRING = 4
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
const int ERROR_FROZEN;
const int ERROR_NOT_FOUND;
const int ERROR_NOT_IMPLEMENTED;
const int ERROR_OUT_OF_MEMORY;
const int ERROR_INVALID_PARAMETER;
const int ERROR_ACCESS_DENIED;
const int ERROR_TIMEOUT;
const int ERROR_BUSY;
const int ERROR_ALREADY_EXISTS;
const int ERROR_INVALID_HANDLE;
const int ERROR_WOULD_BLOCK;
const int ERROR_CANCELLED;
const int ERROR_WRONG_THREAD;
const int ERROR_MALFORMED_MESSAGE;
const int ERROR_TYPE_MISMATCH;
const int ERROR_VERSION_MISMATCH;
const int ERROR_IMC_ENDPOINT_NOT_FOUND;
const int ERROR_IMC_HANDLE_STALE;
const int ERROR_IMC_UNSUPPORTED;
const int ERROR_IMC_API_MISMATCH;
const int ERROR_IMC_PROVIDER_UNLOADED;
const int ERROR_UNAVAILABLE;
const int ERROR_OBJECT_INVALID;
const int ERROR_BUFFER_TOO_SMALL;
const int ERROR_IMC_SCHEMA_MISMATCH;
const int ERROR_IMC_TARGET_EXECUTION_FAILED;

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

enum ConfigPropertyEditor {
  CONFIG_EDITOR_DEFAULT = 0,
  CONFIG_EDITOR_COLOR = 1
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
  bool get_Success() const;
  int get_Count() const;
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
void AddMessage(const string &in message);
void ClearMessages();
void OpenModsMenu();
void CloseModsMenu();
void OpenMapMenu();
void CloseMapMenu();
int GetHUDMode();
void SetHUDMode(int mode);
void ShowTitle(bool show);
void ShowFPS(bool show);
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
bool ColorButton(const string &in label, ImVec4 &inout color);
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

namespace Speedrun {
void SetTimerVisible(bool visible);
void StartTimer();
void PauseTimer();
void ResetTimer();
float GetElapsedTime();
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

class ImcRequestRef {
  bool get_IsValid() const;
  bool get_IsComplete() const;
  int get_Status() const;
  int Cancel();
}

class ImcSubscriptionRef {
  bool get_IsValid() const;
  int get_Status() const;
  int GetDroppedCount(uint64 &out count) const;
  int Cancel();
}

// Generated IMC facades use this bridge. Handwritten scripts should use the
// typed records, functions, Handlers, and Provider emitted from their .imc.
namespace Detail {
class ImcRecord {
  ImcRecord();
  int get_Status() const;
  bool Has(uint id) const;
  void WriteBool(uint id, bool value);
  void WriteInt(uint id, int value);
  void WriteFloat(uint id, float value);
  void WriteInt64(uint id, int64 value);
  void WriteUInt64(uint id, uint64 value);
  void WriteDouble(uint id, double value);
  void WriteString(uint id, const string &in value);
  void WriteBytes(uint id, const array<uint8> &in value);
  void WriteObject(uint id, CKObject@ value);
  void WriteVec2(uint id, const BML::Vec2 &in value);
  void WriteVec3(uint id, const BML::Vec3 &in value);
  void WriteMat4(uint id, const BML::Mat4 &in value);
  void WriteBoolArray(uint id, const array<bool> &in value);
  void WriteIntArray(uint id, const array<int> &in value);
  void WriteFloatArray(uint id, const array<float> &in value);
  void WriteInt64Array(uint id, const array<int64> &in value);
  void WriteUInt64Array(uint id, const array<uint64> &in value);
  void WriteDoubleArray(uint id, const array<double> &in value);
  void WriteStringArray(uint id, const array<string> &in value);
  void WriteObjectArray(uint id, const array<CKObject@> &in value);
  // Host registrations take ? so AngelScript does not instantiate array<VecN>
  // inside the BML config group. Pass array<BML::Vec2/Vec3/Mat4>.
  void WriteVec2Array(uint id, const array<BML::Vec2> &in value);
  void WriteVec3Array(uint id, const array<BML::Vec3> &in value);
  void WriteMat4Array(uint id, const array<BML::Mat4> &in value);
  int ReadBool(uint id, bool &out value) const;
  int ReadInt(uint id, int &out value) const;
  int ReadFloat(uint id, float &out value) const;
  int ReadInt64(uint id, int64 &out value) const;
  int ReadUInt64(uint id, uint64 &out value) const;
  int ReadDouble(uint id, double &out value) const;
  int ReadString(uint id, string &out value) const;
  int ReadBytes(uint id, array<uint8> &out value) const;
  int ReadObject(uint id, CKObject@ &out value) const;
  int ReadVec2(uint id, BML::Vec2 &out value) const;
  int ReadVec3(uint id, BML::Vec3 &out value) const;
  int ReadMat4(uint id, BML::Mat4 &out value) const;
  int ReadBoolArray(uint id, array<bool> &out value) const;
  int ReadIntArray(uint id, array<int> &out value) const;
  int ReadFloatArray(uint id, array<float> &out value) const;
  int ReadInt64Array(uint id, array<int64> &out value) const;
  int ReadUInt64Array(uint id, array<uint64> &out value) const;
  int ReadDoubleArray(uint id, array<double> &out value) const;
  int ReadStringArray(uint id, array<string> &out value) const;
  int ReadObjectArray(uint id, array<CKObject@> &out value) const;
  int ReadVec2Array(uint id, array<BML::Vec2> &out value) const;
  int ReadVec3Array(uint id, array<BML::Vec3> &out value) const;
  int ReadMat4Array(uint id, array<BML::Mat4> &out value) const;
}
class ImcReply {
  void Complete(int status);
  void Complete(int status, const ImcRecord &in record);
}
funcdef void ImcCompletion(int status, const ImcRecord &in record);
funcdef void ImcTopicCallback(int status, const ImcRecord &in record);
funcdef void ImcRpcHandler(const ImcRecord &in request, ImcReply &inout reply);
class ImcProviderRef {
  bool get_IsOpen() const;
  int get_Status() const;
  int _RegisterRpc(const string &in route, const string &in requestPayload,
                   const string &in responsePayload, ImcRpcHandler@+ handler);
  int _UnregisterRpc(const string &in route);
  int _Publish(const string &in topic, const string &in payload,
               const ImcRecord &in message, uint64 &out delivered);
  int _GetSubscriberCount(const string &in topic, uint64 &out count) const;
  int Close();
}
} // namespace Detail

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
  ConfigPropertyEditor get_Editor() const;
  ConfigPropertyEditor GetEditor() const;
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
  void SetEditor(ConfigPropertyEditor editor) const;
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
  void LogInfo(const string &in message) const;
  void LogWarn(const string &in message) const;
  void LogError(const string &in message) const;
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
  void ExecuteCommand(const string &in command) const;
  void SkipRenderForNextTick() const;

  float GetSRScore() const;
  int GetHSScore() const;
  string GetDirectoryUtf8(int type) const;
  float GetTimeMs() const;
  float GetAbsoluteTimeMs() const;
  float GetDeltaTimeMs() const;
  uint GetFrameCount() const;
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
  int _IsImcRpcAvailable(const string &in route, bool &out available) const;
  ImcRequestRef@ _CallImc(const string &in route, const string &in requestPayload, const string &in responsePayload, const BML::Detail::ImcRecord &in request, BML::Detail::ImcCompletion@+ callback, uint timeoutMs = 5000) const;
  ImcSubscriptionRef@ _SubscribeImc(const string &in topic, const string &in payload, BML::Detail::ImcTopicCallback@+ callback, uint capacity = 256) const;
  int _PublishImc(const string &in topic, const string &in payload, const BML::Detail::ImcRecord &in message, uint64 &out delivered) const;
  int _GetImcSubscriberCount(const string &in topic, uint64 &out count) const;
  BML::Detail::ImcProviderRef@ _OpenImcProvider() const;
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
}

class StateBag {
  bool get_IsReloadState() const;
  bool IsReloadState() const;
  bool Has(const string &in key) const;
  bool Remove(const string &in key);
  void Clear();
  int get_Count() const;
  int GetCount() const;
  string GetKey(int index) const;
  StateValueType GetType(const string &in key) const;
  void SetBool(const string &in key, bool value);
  bool GetBool(const string &in key, bool defaultValue = false) const;
  void SetInt(const string &in key, int value);
  int GetInt(const string &in key, int defaultValue = 0) const;
  void SetFloat(const string &in key, float value);
  float GetFloat(const string &in key, float defaultValue = 0.0f) const;
  void SetString(const string &in key, const string &in value);
  string GetString(const string &in key, const string &in defaultValue = "") const;
}

class Vec2 { float x; float y; }
class Vec3 { float x; float y; float z; }
class Mat4 {
  float m00; float m01; float m02; float m03;
  float m10; float m11; float m12; float m13;
  float m20; float m21; float m22; float m23;
  float m30; float m31; float m32; float m33;
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

// Owner-scoped projection of the native Behavior authoring model. Handles
// created here are retired before the current physical Script Mod unloads.
namespace Behavior {
enum ValueKind {
  Bool = 1, Int = 2, Float = 3, String = 4, Vec2 = 5, Vec3 = 6,
  Quaternion = 7, Euler = 8, Rect = 9, Color = 10, Box = 11,
  Matrix = 12, Object = 13, ObjectList = 14
}
enum SlotKind { In = 1, Out = 2, Pin = 3, Pout = 4, Setting = 5, Local = 6, Target = 7 }
enum LayoutOrigin { Declared = 1, Live = 2 }
enum BehaviorKind { Function = 1, Callback = 2, Graph = 3 }
enum Relation { Stored = 1, Direct = 2, Shared = 3, Operation = 4 }
enum HookResult { Error = -1, Ok = 0, AgainNextFrame = 1, Fault = 2 }
enum ChangeKind { Graph = 1, Layout = 2, SampledValue = 3 }
enum Error {
  None = 0, OwnerUnavailable = 1, PrototypeNotFound = 2,
  RequiredManagerMissing = 3, CreationFailed = 4, InitializationFailed = 5,
  TargetInvalid = 6, CallbackFailed = 7, SlotNotFound = 8,
  SlotAmbiguous = 9, LayoutChanged = 10, TypeMismatch = 11,
  ValueInvalid = 12, StateInvalid = 13, NativeError = 14,
  BreakUnsupported = 15, PoutUnsupported = 16, PoutUnavailable = 17,
  FrameQueueFull = 18, Cancelled = 19, PrototypeChanged = 20,
  PrototypeLoadFailed = 21, LayoutUnavailable = 22,
  ParameterTypeUnavailable = 23, ParameterTypeUnsupported = 24,
  DetachedUnsupported = 25, ObserverUnavailable = 26, GraphChanged = 27,
  GraphLocalityInvalid = 28, DelayInvalid = 29, SameFrameCycle = 30,
  SharedSourceCycle = 31, PushCycle = 32, InterfaceUnsupported = 33,
  SourceConflict = 34, SourceOrderCycle = 35, OrderingTargetMismatch = 36,
  OverlayOrderCycle = 37, LinkNotFound = 38, PathAmbiguous = 39,
  PathCycle = 40, QueryNotFound = 41, QueryAmbiguous = 42,
  WorldBoundValue = 43, RevertConflict = 44, TargetCardinality = 45,
  SourceInvalid = 46, OperationInvalid = 47, Busy = 48, Unavailable = 49,
  WrongThread = 50, RedirectConflict = 51
}
enum RunState { Ready = 1, Pending = 2, Failed = 3 }
enum PulseResult { Ran = 1, Queued = 2 }
enum Continuation { None = 0, Native = 1, QueuedInput = 2 }
enum View { Logical = 1, Live = 2 }
enum ObservationState { Available = 1, Indeterminate = 2, Unsupported = 3 }
enum WatchState { Active = 1, Failed = 2 }
enum PatchState {
  Pending = 1, Active = 2, Disabled = 3, Closing = 4,
  Conflicted = 5, Closed = 6, Failed = 7
}
enum PlanState {
  Reconciling = 1, Active = 2, Partial = 4, Unsatisfied = 3,
  Disabled = 5, Conflicted = 6, Retiring = 7
}
enum ScriptState { Ready = 1, Closing = 2, Failed = 3 }
enum CloseState { Closing = 0, Closed = 1 }

class Selector {}
Selector Only();
Selector At(int index);
Selector Named(const string &in name, int occurrence);
Selector Unique(const string &in name);

class FramePolicy {
  FramePolicy Pouts(bool include = true) const;
}
FramePolicy Signals(uint limit = 64);
FramePolicy EachFrame(uint limit);
FramePolicy Latest();
FramePolicy Ignore();

class Value {
  Value(); Value(bool value); Value(int value); Value(float value);
  Value(const string &in value); Value(const Vx2DVector &in value);
  Value(const VxVector &in value); Value(const VxQuaternion &in value);
  Value(const VxRect &in value); Value(const VxColor &in value);
  Value(const VxBbox &in value); Value(const VxMatrix &in value);
  Value(const CKGUID &in type, CKObject@ object);
  CKGUID get_Type() const; ValueKind get_Kind() const; bool get_IsNull() const;
}
Value Euler(const VxVector &in value);

class SlotValue {
  SlotValue();
  SlotValue(const Selector &in slot, const Value &in value);
  SlotValue(const string &in slot, const Value &in value);
}

class NodePattern {
  NodePattern(); NodePattern(const Selector &in selector);
  NodePattern(const string &in name);
  NodePattern &Prototype(const CKGUID &in prototype);
  NodePattern &Kind(BehaviorKind kind);
  NodePattern &Ins(int count); NodePattern &Outs(int count);
  NodePattern &Pins(int count); NodePattern &Pouts(int count);
  NodePattern &Settings(int count); NodePattern &Locals(int count);
  NodePattern &Pin(const Selector &in slot, const Value &in value);
  NodePattern &Pout(const Selector &in slot, const Value &in value);
  NodePattern &Setting(const Selector &in slot, const Value &in value);
  NodePattern &Local(const Selector &in slot, const Value &in value);
  NodePattern &Target(const Value &in value);
}

class Slot {
  bool get_IsValid() const; SlotKind get_Kind() const;
  uint64 get_Generation() const; bool get_Dynamic() const;
  int get_Index() const; int get_Occurrence() const;
  CKGUID get_Type() const; ValueKind get_ValueKind() const;
  string get_Name() const; string get_TypeName() const;
}
class Layout {
  bool get_IsValid() const; LayoutOrigin get_Origin() const;
  uint64 get_Generation() const; BehaviorKind get_Kind() const;
  int get_CompatibleClass() const; uint get_PrototypeFlags() const;
  uint get_BehaviorFlags() const; CKGUID get_Prototype() const;
  uint64 get_PrototypeGeneration() const; CKGUID get_TargetType() const;
  string get_Name() const; string get_Category() const;
  string get_Provider() const; string get_Author() const;
  string get_Description() const; int get_SlotCount() const;
  Slot@ opIndex(int index) const;
  Slot@ Find(SlotKind kind, const Selector &in selector) const;
  Slot@ Find(SlotKind kind, const string &in name) const;
}

class Frame {
  uint64 get_Sequence() const; uint64 get_GameFrame() const;
  int get_NativeResult() const; Continuation get_Continuation() const;
  Error get_Error() const; int get_OutCount() const;
  int get_PoutCount() const; int get_StatusCount() const;
  bool HasOut(const Selector &in selector) const;
  string OutName(int index) const; string PoutName(int index) const;
  string Status(int index) const;
  bool Read(const Selector &in selector, bool &out value) const;
  bool Read(const Selector &in selector, int &out value) const;
  bool Read(const Selector &in selector, float &out value) const;
  bool Read(const Selector &in selector, string &out value) const;
  bool Read(const Selector &in selector, Vx2DVector &out value) const;
  bool Read(const Selector &in selector, VxVector &out value) const;
  bool Read(const Selector &in selector, VxQuaternion &out value) const;
  bool ReadEuler(const Selector &in selector, VxVector &out value) const;
  bool Read(const Selector &in selector, VxRect &out value) const;
  bool Read(const Selector &in selector, VxColor &out value) const;
  bool Read(const Selector &in selector, VxBbox &out value) const;
  bool Read(const Selector &in selector, VxMatrix &out value) const;
  ObjectRef@ ReadObject(const Selector &in selector) const;
  ObjectList@ ReadObjects(const Selector &in selector) const;
}
class Frames {
  int get_Count() const; bool get_Empty() const; Frame@ opIndex(int index) const;
}
class ObjectRef {
  bool get_IsNull() const; bool get_IsValid() const; bool get_IsStale() const;
  uint get_Domain() const; uint get_Slot() const; uint get_Generation() const;
  CKObject@ Borrow() const;
}
class ObjectList {
  int get_Count() const; bool get_Empty() const; ObjectRef@ opIndex(int index) const;
}

class Port {
  bool get_IsValid() const; SlotKind get_Kind() const; int get_Index() const;
  int get_Occurrence() const; uint64 get_NodeId() const;
  uint64 get_LayoutGeneration() const; CKGUID get_Type() const;
  bool get_Dynamic() const; bool get_Active() const; string get_Name() const;
}
class Node {
  bool get_IsValid() const; uint64 get_Id() const; int get_Index() const;
  int get_Occurrence() const; uint64 get_LayoutGeneration() const;
  BehaviorKind get_Kind() const; bool get_IsGraph() const; CKGUID get_Prototype() const;
  int get_Priority() const; bool get_Active() const; string get_Name() const;
  int get_PortCount() const; Port@ PortAt(int index) const;
  Port@ In(const Selector &in slot) const; Port@ Out(const Selector &in slot) const;
  Port@ Pin(const Selector &in slot) const; Port@ Pout(const Selector &in slot) const;
  Port@ Setting(const Selector &in slot) const; Port@ Local(const Selector &in slot) const;
  Port@ In(const string &in name) const; Port@ Out(const string &in name) const;
  Port@ Pin(const string &in name) const; Port@ Pout(const string &in name) const;
  Port@ Setting(const string &in name) const; Port@ Local(const string &in name) const;
  Port@ Target() const;
}
class Link {
  bool get_IsValid() const; uint64 get_Id() const;
  int get_InitialDelay() const; int get_RemainingDelay() const;
  int get_Pending() const; Port@ Source() const; Port@ Target() const;
}
class ObservedValue {
  bool get_IsAvailable() const; ObservationState get_State() const;
  Relation get_Relation() const; ValueKind get_Kind() const; CKGUID get_Type() const;
  bool Read(bool &out value) const; bool Read(int &out value) const;
  bool Read(float &out value) const; bool Read(string &out value) const;
  bool Read(Vx2DVector &out value) const; bool Read(VxVector &out value) const;
  bool Read(VxQuaternion &out value) const; bool ReadEuler(VxVector &out value) const;
  bool Read(VxRect &out value) const; bool Read(VxColor &out value) const;
  bool Read(VxBbox &out value) const; bool Read(VxMatrix &out value) const;
  ObjectRef@ ReadObject() const;
}

class Change {
  ChangeKind get_Kind() const; uint64 get_Sequence() const; uint64 get_GameFrame() const;
  uint64 get_Before() const; uint64 get_After() const;
  ObservedValue@ Previous() const; ObservedValue@ Current() const;
}
class HookEvent {
  float get_DeltaTime() const; CKBehavior@ BorrowBlock() const;
  CKBehavior@ BorrowScript() const; CKBeObject@ BorrowOwner() const;
}
funcdef HookResult HookCallback(const BML::ModContext &in, const HookEvent &in);
funcdef void WatchCallback(const BML::ModContext &in, const Change &in);
class Watch {
  bool get_IsValid() const; WatchState get_State() const;
  string get_Error() const; CloseState Close();
}

class Graph {
  bool get_IsValid() const; View get_View() const; uint64 get_Generation() const;
  uint64 get_Fingerprint() const; int get_NodeCount() const; int get_LinkCount() const;
  Node@ Root() const; Node@ NodeAt(int index) const; Link@ LinkAt(int index) const;
  Node@ Find(const Selector &in selector, const CKGUID &in prototype) const;
  int IncomingCount(Node@ node) const; int IncomingCount(Port@ port) const;
  int OutgoingCount(Node@ node) const; int OutgoingCount(Port@ port) const;
  Link@ Incoming(Node@ node, int index) const; Link@ Incoming(Port@ port, int index) const;
  Link@ Outgoing(Node@ node, int index) const; Link@ Outgoing(Port@ port, int index) const;
  Link@ Entering(Node@ node) const; Link@ Entering(Port@ port) const;
  Link@ Leaving(Node@ node) const; Link@ Leaving(Port@ port) const;
  Node@ Previous(Node@ node) const; Node@ Previous(Port@ port) const;
  Node@ Next(Node@ node) const; Node@ Next(Port@ port) const;
  ObservedValue@ Read(Port@ port) const; Graph@ Inspect(Node@ node) const;
  Graph@ Logical() const; Graph@ Live() const;
  Patch@ Apply(const string &in name, Edit@ edit) const;
  Watch@ Watch(WatchCallback@+ callback) const;
  Watch@ Watch(Node@ node, WatchCallback@+ callback) const;
  Watch@ Watch(Port@ port, WatchCallback@+ callback) const;
}

class EditPort { bool get_IsValid() const; }
class EditPorts { bool get_IsValid() const; }
class EditLink { bool get_IsValid() const; }
class EditPath { bool get_IsValid() const; }
class Operation {
  bool get_IsValid() const; EditPort@ Input(int index) const; EditPort@ Result() const;
}
class EditNode {
  bool get_IsValid() const;
  EditPort@ In(const Selector &in slot) const; EditPort@ Out(const Selector &in slot) const;
  EditPort@ Pin(const Selector &in slot) const; EditPort@ Pout(const Selector &in slot) const;
  EditPort@ Local(const Selector &in slot) const; EditPort@ Target() const;
  EditGraph@ Graph() const;
}
class EditNodes {
  bool get_IsValid() const;
  EditPorts@ In(const Selector &in slot) const; EditPorts@ Out(const Selector &in slot) const;
  EditPorts@ Pin(const Selector &in slot) const; EditPorts@ Pout(const Selector &in slot) const;
  EditPorts@ Local(const Selector &in slot) const; EditPorts@ Target() const;
}
class EditGraph {
  bool get_IsValid() const; EditNode@ Root() const;
  EditNode@ Require(const Selector &in selector, const CKGUID &in prototype) const;
  EditNode@ Require(const NodePattern &in pattern) const;
  EditNodes@ Each(const NodePattern &in pattern) const;
  EditNode@ Require(Node@ node) const; EditLink@ Require(Link@ link) const;
  EditNode@ Use(Node@ node) const; EditLink@ Use(Link@ link) const;
  EditLink@ Between(EditPort@ source, EditPort@ sink, int delay = 0) const;
  EditNode@ Next(EditPort@ source) const; EditNode@ Next(EditNode@ source) const;
  EditNode@ Previous(EditPort@ sink) const; EditNode@ Previous(EditNode@ sink) const;
  EditLink@ Leaving(EditPort@ source) const; EditLink@ Leaving(EditNode@ source) const;
  EditLink@ Entering(EditPort@ sink) const; EditLink@ Entering(EditNode@ sink) const;
  EditLink@ To(EditPort@ source, EditNode@ target) const; EditPath@ Follow(EditPort@ source) const;
  EditNode@ Add(Block@ block) const; EditNode@ AddGraph(const string &in name, int priority = 0) const;
  EditNode@ Replace(EditNode@ target, Block@ block) const; bool Remove(EditNode@ node) const;
  bool Flow(EditPort@ source, EditPort@ sink, int delay = 0) const;
  bool Flow(EditPorts@ sources, EditPort@ sink, int delay = 0) const;
  bool Flow(EditPort@ source, EditPorts@ sinks, int delay = 0) const;
  bool FlowCycle(EditPort@ source, EditPort@ sink, int delay = 0) const;
  bool FlowCycle(EditPorts@ sources, EditPort@ sink, int delay = 0) const;
  bool FlowCycle(EditPort@ source, EditPorts@ sinks, int delay = 0) const;
  bool Bind(EditPort@ sink, EditPort@ source) const;
  bool Bind(EditPort@ sink, bool value) const;
  bool Bind(EditPort@ sink, int value) const;
  bool Bind(EditPort@ sink, float value) const;
  bool Bind(EditPort@ sink, const string &in value) const;
  bool Bind(EditPort@ sink, const Vx2DVector &in value) const;
  bool Bind(EditPort@ sink, const VxVector &in value) const;
  bool Bind(EditPort@ sink, const Value &in value) const;
  bool Bind(EditPorts@ sinks, EditPort@ source) const;
  bool Bind(EditPorts@ sinks, const Value &in value) const;
  bool Share(EditPort@ sink, EditPort@ source) const;
  bool Share(EditPorts@ sinks, EditPort@ source) const;
  bool Push(EditPort@ source, EditPort@ sink) const;
  bool Push(EditPorts@ sources, EditPort@ sink) const;
  bool Push(EditPort@ source, EditPorts@ sinks) const;
  Operation@ AddOperation(const CKGUID &in operation, const CKGUID &in result,
                          const CKGUID &in input1 = CKGUID(0, 0),
                          const CKGUID &in input2 = CKGUID(0, 0)) const;
  EditPort@ AppendIn(const string &in name) const; EditPort@ AppendOut(const string &in name) const;
  EditPort@ AppendPin(const string &in name, const CKGUID &in type) const;
  EditPort@ AppendPout(const string &in name, const CKGUID &in type) const;
  EditPort@ AppendLocal(const string &in name, const CKGUID &in type) const;
  EditPort@ AppendIn(EditNode@ owner, const string &in name) const;
  EditPort@ AppendOut(EditNode@ owner, const string &in name) const;
  EditPort@ AppendPin(EditNode@ owner, const string &in name,
                      const CKGUID &in type) const;
  EditPort@ AppendPout(EditNode@ owner, const string &in name,
                       const CKGUID &in type) const;
  EditPort@ AppendLocal(EditNode@ owner, const string &in name,
                        const CKGUID &in type) const;
  bool Splice(EditLink@ link, EditNode@ through) const;
  bool Splice(EditLink@ link, EditPort@ sink, EditPort@ source) const;
  bool Redirect(EditLink@ link, EditPort@ sink) const;
  bool Redirect(EditLink@ link, EditLink@ destination) const;
  bool Reconnect(EditLink@ link, EditPort@ source, EditPort@ sink) const;
  bool ReconnectCycle(EditLink@ link, EditPort@ source, EditPort@ sink) const;
  bool Tap(EditPort@ source, HookCallback@+ callback) const;
  bool Tap(EditPorts@ sources, HookCallback@+ callback) const;
  bool Before(EditLink@ link, HookCallback@+ callback) const;
  bool After(EditPath@ path, HookCallback@+ callback) const;
  bool After(EditPort@ source, HookCallback@+ callback) const;
}
class Edit {
  bool get_IsValid() const; EditGraph@ Root() const;
  Plan@ Plan(const string &in name, const string &in script, bool each = false);
  Script@ CreateScript(CKBeObject@ owner, const string &in name, int priority = 0);
}
Edit@ Edit();

class Block {
  bool get_IsValid() const; Block@ Clone() const; Block@ TargetOwner();
  Block@ Target(const CKGUID &in type, CKObject@ object);
  Block@ NullTarget(const CKGUID &in type);
  // Host registrations take ? so AngelScript does not instantiate
  // array<SlotValue> inside the BML config group. Pass array<SlotValue>.
  Block@ Settings(const array<SlotValue> &in values);
  Block@ Pins(const array<SlotValue> &in values);
  Block@ Locals(const array<SlotValue> &in values);
  Block@ Setting(const Selector &in slot, const Value &in value);
  Block@ Setting(const string &in slot, const Value &in value);
  Block@ Setting(const string &in slot, bool value);
  Block@ Setting(const Selector &in slot, bool value);
  Block@ Setting(const string &in slot, int value);
  Block@ Setting(const Selector &in slot, int value);
  Block@ Setting(const string &in slot, float value);
  Block@ Setting(const Selector &in slot, float value);
  Block@ Setting(const string &in slot, const string &in value);
  Block@ Setting(const Selector &in slot, const string &in value);
  Block@ Setting(const string &in slot, const Vx2DVector &in value);
  Block@ Setting(const Selector &in slot, const Vx2DVector &in value);
  Block@ Setting(const string &in slot, const VxVector &in value);
  Block@ Setting(const Selector &in slot, const VxVector &in value);
  Block@ Setting(const string &in slot, const CKGUID &in type, CKObject@ value);
  Block@ Setting(const Selector &in slot, const CKGUID &in type, CKObject@ value);
  Block@ Pin(const Selector &in slot, const Value &in value);
  Block@ Pin(const string &in slot, const Value &in value);
  Block@ Pin(const string &in slot, bool value);
  Block@ Pin(const Selector &in slot, bool value);
  Block@ Pin(const string &in slot, int value);
  Block@ Pin(const Selector &in slot, int value);
  Block@ Pin(const string &in slot, float value);
  Block@ Pin(const Selector &in slot, float value);
  Block@ Pin(const string &in slot, const string &in value);
  Block@ Pin(const Selector &in slot, const string &in value);
  Block@ Pin(const string &in slot, const Vx2DVector &in value);
  Block@ Pin(const Selector &in slot, const Vx2DVector &in value);
  Block@ Pin(const string &in slot, const VxVector &in value);
  Block@ Pin(const Selector &in slot, const VxVector &in value);
  Block@ Pin(const string &in slot, const CKGUID &in type, CKObject@ value);
  Block@ Pin(const Selector &in slot, const CKGUID &in type, CKObject@ value);
  Block@ Local(const Selector &in slot, const Value &in value);
  Block@ Local(const string &in slot, const Value &in value);
  Block@ Local(const string &in slot, bool value);
  Block@ Local(const Selector &in slot, bool value);
  Block@ Local(const string &in slot, int value);
  Block@ Local(const Selector &in slot, int value);
  Block@ Local(const string &in slot, float value);
  Block@ Local(const Selector &in slot, float value);
  Block@ Local(const string &in slot, const string &in value);
  Block@ Local(const Selector &in slot, const string &in value);
  Block@ Local(const string &in slot, const Vx2DVector &in value);
  Block@ Local(const Selector &in slot, const Vx2DVector &in value);
  Block@ Local(const string &in slot, const VxVector &in value);
  Block@ Local(const Selector &in slot, const VxVector &in value);
  Block@ Local(const string &in slot, const CKGUID &in type, CKObject@ value);
  Block@ Local(const Selector &in slot, const CKGUID &in type, CKObject@ value);
  Block@ PinType(const string &in slot, const CKGUID &in type);
  Block@ PinType(const Selector &in slot, const CKGUID &in type);
  Block@ PoutType(const string &in slot, const CKGUID &in type);
  Block@ PoutType(const Selector &in slot, const CKGUID &in type);
  bool Validate();
  Call@ Call(const Selector &in input);
  Call@ Call(const Selector &in input, const FramePolicy &in frames);
  Call@ Call(CKBeObject@ owner, const Selector &in input, const FramePolicy &in frames);
  Task@ Start(const Selector &in input);
  Task@ Start(const Selector &in input, const FramePolicy &in frames);
  Task@ Start(CKBeObject@ owner, const Selector &in input, const FramePolicy &in frames);
  Instance@ Spawn(); Instance@ Spawn(const FramePolicy &in frames);
  Instance@ Spawn(CKBeObject@ owner, const FramePolicy &in frames);
  Instance@ SpawnIn(CKBehavior@ graph);
  Instance@ SpawnIn(CKBehavior@ graph, const FramePolicy &in frames);
}
class Call {
  bool get_IsValid() const; RunState get_State() const; string get_Error() const;
  Frames@ TakeFrames(); Layout@ Layout(); Graph@ Inspect(bool live = false);
  bool Set(Slot@ slot, const Value &in value);
  bool Bind(Slot@ slot, Port@ source, Relation relation = Direct);
  bool Settings(const array<SlotValue> &in values);
  bool Pin(const string &in slot, bool value); bool Pin(const Selector &in slot, bool value);
  bool Pin(const string &in slot, int value); bool Pin(const Selector &in slot, int value);
  bool Pin(const string &in slot, float value); bool Pin(const Selector &in slot, float value);
  bool Pin(const string &in slot, const string &in value); bool Pin(const Selector &in slot, const string &in value);
  bool Pin(const string &in slot, const Vx2DVector &in value); bool Pin(const Selector &in slot, const Vx2DVector &in value);
  bool Pin(const string &in slot, const VxVector &in value); bool Pin(const Selector &in slot, const VxVector &in value);
  bool Pin(const Selector &in slot, const Value &in value);
  bool Pin(const string &in slot, const CKGUID &in type, CKObject@ value);
  bool Pin(const Selector &in slot, const CKGUID &in type, CKObject@ value);
  bool Local(const string &in slot, bool value); bool Local(const Selector &in slot, bool value);
  bool Local(const string &in slot, int value); bool Local(const Selector &in slot, int value);
  bool Local(const string &in slot, float value); bool Local(const Selector &in slot, float value);
  bool Local(const string &in slot, const string &in value); bool Local(const Selector &in slot, const string &in value);
  bool Local(const string &in slot, const Vx2DVector &in value); bool Local(const Selector &in slot, const Vx2DVector &in value);
  bool Local(const string &in slot, const VxVector &in value); bool Local(const Selector &in slot, const VxVector &in value);
  bool Local(const Selector &in slot, const Value &in value);
  bool Setting(const string &in slot, bool value); bool Setting(const Selector &in slot, bool value);
  bool Setting(const string &in slot, int value); bool Setting(const Selector &in slot, int value);
  bool Setting(const string &in slot, float value); bool Setting(const Selector &in slot, float value);
  bool Setting(const string &in slot, const string &in value); bool Setting(const Selector &in slot, const string &in value);
  bool Setting(const string &in slot, const Vx2DVector &in value); bool Setting(const Selector &in slot, const Vx2DVector &in value);
  bool Setting(const string &in slot, const VxVector &in value); bool Setting(const Selector &in slot, const VxVector &in value);
  bool Setting(const Selector &in slot, const Value &in value);
  bool Close(); Task@ Continue();
}
class Task {
  bool get_IsValid() const; RunState get_State() const; string get_Error() const;
  Frames@ TakeFrames(); Layout@ Layout(); Graph@ Inspect(bool live = false);
  bool Set(Slot@ slot, const Value &in value);
  bool Bind(Slot@ slot, Port@ source, Relation relation = Direct);
  bool Settings(const array<SlotValue> &in values);
  bool Pin(const string &in slot, bool value); bool Pin(const Selector &in slot, bool value);
  bool Pin(const string &in slot, int value); bool Pin(const Selector &in slot, int value);
  bool Pin(const string &in slot, float value); bool Pin(const Selector &in slot, float value);
  bool Pin(const string &in slot, const string &in value); bool Pin(const Selector &in slot, const string &in value);
  bool Pin(const string &in slot, const Vx2DVector &in value); bool Pin(const Selector &in slot, const Vx2DVector &in value);
  bool Pin(const string &in slot, const VxVector &in value); bool Pin(const Selector &in slot, const VxVector &in value);
  bool Pin(const Selector &in slot, const Value &in value);
  bool Pin(const string &in slot, const CKGUID &in type, CKObject@ value);
  bool Pin(const Selector &in slot, const CKGUID &in type, CKObject@ value);
  bool Local(const string &in slot, bool value); bool Local(const Selector &in slot, bool value);
  bool Local(const string &in slot, int value); bool Local(const Selector &in slot, int value);
  bool Local(const string &in slot, float value); bool Local(const Selector &in slot, float value);
  bool Local(const string &in slot, const string &in value); bool Local(const Selector &in slot, const string &in value);
  bool Local(const string &in slot, const Vx2DVector &in value); bool Local(const Selector &in slot, const Vx2DVector &in value);
  bool Local(const string &in slot, const VxVector &in value); bool Local(const Selector &in slot, const VxVector &in value);
  bool Local(const Selector &in slot, const Value &in value);
  bool Setting(const string &in slot, bool value); bool Setting(const Selector &in slot, bool value);
  bool Setting(const string &in slot, int value); bool Setting(const Selector &in slot, int value);
  bool Setting(const string &in slot, float value); bool Setting(const Selector &in slot, float value);
  bool Setting(const string &in slot, const string &in value); bool Setting(const Selector &in slot, const string &in value);
  bool Setting(const string &in slot, const Vx2DVector &in value); bool Setting(const Selector &in slot, const Vx2DVector &in value);
  bool Setting(const string &in slot, const VxVector &in value); bool Setting(const Selector &in slot, const VxVector &in value);
  bool Setting(const Selector &in slot, const Value &in value);
  bool Close();
  PulseResult Pulse(const Selector &in input);
}
class Instance {
  bool get_IsValid() const; RunState get_State() const; string get_Error() const;
  Frames@ TakeFrames(); Layout@ Layout(); Graph@ Inspect(bool live = false);
  bool Set(Slot@ slot, const Value &in value);
  bool Bind(Slot@ slot, Port@ source, Relation relation = Direct);
  bool Settings(const array<SlotValue> &in values);
  bool Pin(const string &in slot, bool value); bool Pin(const Selector &in slot, bool value);
  bool Pin(const string &in slot, int value); bool Pin(const Selector &in slot, int value);
  bool Pin(const string &in slot, float value); bool Pin(const Selector &in slot, float value);
  bool Pin(const string &in slot, const string &in value); bool Pin(const Selector &in slot, const string &in value);
  bool Pin(const string &in slot, const Vx2DVector &in value); bool Pin(const Selector &in slot, const Vx2DVector &in value);
  bool Pin(const string &in slot, const VxVector &in value); bool Pin(const Selector &in slot, const VxVector &in value);
  bool Pin(const Selector &in slot, const Value &in value);
  bool Pin(const string &in slot, const CKGUID &in type, CKObject@ value);
  bool Pin(const Selector &in slot, const CKGUID &in type, CKObject@ value);
  bool Local(const string &in slot, bool value); bool Local(const Selector &in slot, bool value);
  bool Local(const string &in slot, int value); bool Local(const Selector &in slot, int value);
  bool Local(const string &in slot, float value); bool Local(const Selector &in slot, float value);
  bool Local(const string &in slot, const string &in value); bool Local(const Selector &in slot, const string &in value);
  bool Local(const string &in slot, const Vx2DVector &in value); bool Local(const Selector &in slot, const Vx2DVector &in value);
  bool Local(const string &in slot, const VxVector &in value); bool Local(const Selector &in slot, const VxVector &in value);
  bool Local(const Selector &in slot, const Value &in value);
  bool Setting(const string &in slot, bool value); bool Setting(const Selector &in slot, bool value);
  bool Setting(const string &in slot, int value); bool Setting(const Selector &in slot, int value);
  bool Setting(const string &in slot, float value); bool Setting(const Selector &in slot, float value);
  bool Setting(const string &in slot, const string &in value); bool Setting(const Selector &in slot, const string &in value);
  bool Setting(const string &in slot, const Vx2DVector &in value); bool Setting(const Selector &in slot, const Vx2DVector &in value);
  bool Setting(const string &in slot, const VxVector &in value); bool Setting(const Selector &in slot, const VxVector &in value);
  bool Setting(const Selector &in slot, const Value &in value);
  bool Close();
  PulseResult Pulse(const Selector &in input);
}
class Patch {
  bool get_IsValid() const; PatchState get_State() const; string get_Error() const;
  bool Enable(); bool Disable(); bool Replace(Graph@ graph, Edit@ edit);
  ObjectRef@ Resolve(EditNode@ node) const; CloseState Close();
}
class Plan {
  bool get_IsValid() const; PlanState get_State() const; string get_Error() const;
  bool Enable(); bool Disable();
  bool Replace(const string &in script, Edit@ edit, bool each = false); CloseState Close();
}
class Script {
  bool get_IsValid() const; ScriptState get_State() const; string get_Error() const;
  bool Activate(); bool Restart(); bool Deactivate();
  Graph@ Inspect(bool live = false) const;
  Patch@ Apply(const string &in name, Edit@ edit) const; CloseState Close();
}

Block@ Use(const CKGUID &in prototype);
Block@ Find(const string &in name, const string &in category = "",
            const string &in provider = "");
Layout@ Describe(const CKGUID &in prototype);
Graph@ Inspect(CKBehavior@ graph, bool live = false);
Script@ CreateScript(CKBeObject@ owner, const string &in name,
                     Edit@ body, int priority = 0);
} // namespace Behavior

// Typed built-in capability facades. Runtime snapshots are small values returned
// directly. Borrow* methods create a host-owned, non-retained CK handle and are
// valid only while the object still belongs to the current game scene.
namespace Runtime {
class State {
  bool InGame; bool InLevel; bool Paused; bool Playing; bool CheatEnabled;
}
class Clock { float TimeMs; float AbsoluteMs; float DeltaMs; int Frame; }
class Score { float SR; int HS; }
State GetState();
Clock GetClock();
Score GetScore();
} // namespace Runtime

namespace Gameplay {
class LevelState {
  int Id; BML::Mat4 ResetMatrix; int Points;
  CKObject@ BorrowActiveBall() const;
}
class EnergyState {
  int Points; int Lives; int StartPoints; int StartLives; float TimeFactor; int LifeBonus;
}
class CatalogEntry {
  int Bonus; int Music;
  bool FileTruncated; bool StartBallTruncated; bool SkyTruncated;
  string get_File() const; string get_StartBall() const; string get_Sky() const;
}
class Checkpoint { BML::Mat4 Matrix; CKObject@ BorrowObject() const; }
class Resetpoint { CKObject@ BorrowObject() const; }
int ReadLevel(LevelState &out state);
int ReadEnergy(EnergyState &out state);
int ReadCatalogCount(int &out count);
int ReadCatalogEntry(int index, CatalogEntry &out entry);
int ReadCheckpointCount(int &out count);
int ReadCheckpoint(int index, Checkpoint &out checkpoint);
int ReadResetpointCount(int &out count);
int ReadResetpoint(int index, Resetpoint &out resetpoint);
} // namespace Gameplay

namespace Events {
// Event is an immutable stream snapshot.  Category-specific fields are
// meaningful only when the corresponding Is* flag is true.
class Event {
  bool get_IsValid() const; int get_Status() const; int get_Kind() const;
  uint64 get_Sequence() const; uint64 get_Timestamp() const;
  bool get_IsLoad() const; bool get_IsPhysics() const; bool get_IsCommand() const;
  bool get_IsConfig() const; bool get_IsCheat() const;
  string get_Filename() const; bool get_IsMap() const; string get_MasterName() const;
  int get_FilterClass() const; bool get_AddToScene() const; bool get_ReuseMeshes() const;
  bool get_ReuseMaterials() const; bool get_Dynamic() const;
  CKObject@ BorrowMasterObject() const; CKObject@ BorrowScript() const;
  int get_ObjectCount() const; CKObject@ BorrowObject(int index) const;
  CKObject@ BorrowTarget() const;
  bool get_Fixed() const; float get_Friction() const; float get_Elasticity() const;
  float get_Mass() const; string get_CollisionGroup() const; bool get_StartFrozen() const;
  bool get_EnableCollision() const; bool get_AutoCalculateMassCenter() const;
  float get_LinearDamp() const; float get_RotDamp() const;
  string get_CollisionSurface() const; BML::Vec3 get_MassCenter() const;
  int get_ConvexMeshCount() const; CKObject@ BorrowConvexMesh(int index) const;
  int get_BallCount() const; BML::Vec3 GetBallCenter(int index) const;
  float GetBallRadius(int index) const; int get_ConcaveMeshCount() const;
  CKObject@ BorrowConcaveMesh(int index) const;
  string get_Command() const; int get_CommandArgumentCount() const;
  string GetCommandArgument(int index) const; string get_ConfigCategory() const;
  string get_ConfigKey() const; int get_ConfigType() const; string get_ConfigValue() const;
  bool get_CheatEnabled() const;
}
class Stream {
  bool get_IsOpen() const; int Close();
  int GetDroppedCount(int &out count) const;
  int Poll(Event@ &out event);
}
int Open(Stream@ &out stream, int capacity = 256);
} // namespace Events

} // namespace BML

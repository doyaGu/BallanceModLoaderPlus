# ModContext 方法

## Mod 信息

```angelscript
string GetModId() const;
string GetModName() const;
```

## 日志

```angelscript
BML::Logger@ BorrowLogger() const;
// logger.Info(msg), logger.Warn(msg), logger.Error(msg)
```

## 配置

```angelscript
BML::Config@ BorrowConfig() const;
// config.SetCategoryComment(category, comment)
// config.GetProperty(category, key) -> BML::ConfigProperty@
// prop.SetDefaultBoolean/String/Integer/Float(value)
// prop.SetComment(text)
// prop.GetBoolean/String/Integer/Float(defaultValue)
// prop.SetBoolean/String/Integer/Float(value)
```

## 输入

```angelscript
BML::InputHook@ BorrowInputManager() const;
// input.IsKeyDown(CKKEY_X) — true every frame while held
// input.IsKeyPressed(CKKEY_X) — true only first frame
```

## 命令

```angelscript
BML::CommandRef@ RegisterCommand(const BML::CommandDefinition &in def,
    BML::CommandCallback@+ execute,
    BML::CommandCompletionCallback@+ complete = null) const;
bool UnregisterCommand(const string &in name) const;
bool HasCommand(const string &in name) const;
void ExecuteCommand(const string &in command) const;
```

## 定时器

```angelscript
BML::TimerRef@ SetTimeout(float delayMs, BML::TimerCallback@+ callback, const string &in name = "") const;
BML::TimerRef@ SetInterval(float delayMs, BML::TimerLoopCallback@+ callback, const string &in name = "") const;
```

## 游戏状态

```angelscript
bool IsInGame;    // 属性访问器
bool IsInLevel;
bool IsPaused;
bool IsPlaying;
bool IsCheatEnabled;
bool IsReloading;
BML::ReloadPhase ReloadPhase;
```

用法：`ctx.IsInLevel`、`ctx.IsPaused`、`ctx.ReloadPhase`（不是方法调用，不加括号）。

`IsReloading` 只在热重载生命周期回调里为 true。`ReloadPhase` 常用来区分正常启动/退出和热重载：

| 值 | 含义 |
| --- | --- |
| `BML::RELOAD_NONE` | 正常启动、正常运行、正常退出 |
| `BML::RELOAD_UNLOAD` | 热重载正在卸载旧 runtime |
| `BML::RELOAD_LOAD` | 热重载正在加载新 runtime |
| `BML::RELOAD_ROLLBACK` | 新 runtime 失败，BML 正在恢复旧 runtime |
| `BML::RELOAD_RECOVERY` | 启动时失败的占位 mod 正在恢复 |
| `BML::RELOAD_CLEANUP` | 热重载清理阶段 |
| `BML::RELOAD_SAVE_STATE` | 正在调用旧 runtime 的 `SaveState` |
| `BML::RELOAD_MIGRATE_STATE` | 正在调用新 runtime 的 `MigrateState` |
| `BML::RELOAD_RESTORE_STATE` | 正在调用 `RestoreState` |

## UI 与 Speedrun

消息、菜单、HUD 和 BML 风格控件由 `BML::UI` 提供；Speedrun 计时由
`BML::Speedrun` 提供。它们不是 `ModContext` 成员。完整声明和使用规则见
[UI、计时与资源](ref-hud-resources.md)。

## 时间

```angelscript
float GetTimeMs() const;          // 游戏时间，毫秒
float GetAbsoluteTimeMs() const;  // 绝对时间，毫秒
float GetDeltaTimeMs() const;     // 上一帧到当前帧的间隔，毫秒
uint GetFrameCount() const;       // 当前帧计数
```

需要按时间间隔执行逻辑时优先用 Timer。`GetDeltaTimeMs()` 适合平滑显示、插值和一次性调试统计，不要用它绕过 Timer 写复杂调度器。

## 文件与资源

```angelscript
string GetModRootUtf8() const;
string ResolveModPathUtf8(const string &in relativePath) const;
bool ModFileExistsUtf8(const string &in relativePath) const;
string ReadModTextFileUtf8(const string &in relativePath, const string &in defaultValue = "") const;
```

## Mod 列表

```angelscript
int GetModCount() const;
BML::ModRef@ GetMod(int index) const;
BML::ModRef@ FindMod(const string &in id) const;
```

## CKAS / Virtools 入口

```angelscript
CKContext@ BorrowCKContext() const;
CKRenderContext@ BorrowRenderContext() const;
CKDataArray@ BorrowDataArrayByName(const string &in name) const;
CKGroup@ BorrowGroupByName(const string &in name) const;
CK3dEntity@ Borrow3dEntityByName(const string &in name) const;
CKBehavior@ BorrowScriptByName(const string &in name) const;
```

## DataShare

```angelscript
BML::DataShareRequestRef@ RequestDataShare(BML::DataShareRequest@+ request) const;
BML::DataShareRequestRef@ RequestDataShare(const string &in key, int type,
    BML::DataShareCallback@+ callback, const string &in name = "") const;
```

## 注册 API (仅 OnLoad 可调用)

```angelscript
bool RegisterBallType(const BML::BallTypeDefinition &in ball) const;
bool RegisterFloorType(const BML::FloorTypeDefinition &in floor) const;
bool RegisterModule(const BML::ModuleDefinition &in module) const;
```

## Borrow 规则

1. 所有 `Borrow*` 返回值都必须判空。
2. 原始 CK 句柄只在对应 CK 对象仍存在时有效。只应缓存生命周期明确、关卡内稳定
   的对象，并在对象可能被删除或退出关卡前清空。
3. 跨回调保存 CK 对象身份时，优先使用 CKAS 的可重新验证引用类型；每次使用前
   检查 `valid`。否则保存 `CK_ID` 或确实唯一且稳定的名称，并在操作前重新查找。
4. 不要把名称默认当成唯一标识，也不要在关卡切换后继续使用旧句柄或旧
   `CK_ID`。

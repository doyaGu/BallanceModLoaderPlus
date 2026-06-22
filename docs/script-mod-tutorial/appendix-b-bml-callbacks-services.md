# 附录 B：常用 BML 回调和服务

本附录用于快速查 BML 层的入口。正文里每章会慢慢讲原因；这里把常用回调和服务集中放在一页。

先记住一条线：

```text
BML 加载脚本
  -> 创建带 [bml.mod] 的类
  -> 在固定时机调用回调
  -> 通过 BML::ModContext 提供服务
```

大多数脚本功能都从 `ctx` 开始。看到一个 API 时，先判断它属于 BML 层，还是已经进入 CKAS / Virtools 层。

## 固定回调

固定回调是写在 `[bml.mod]` 类里的方法。函数名和参数要照抄。

| 回调 | 什么时候调用 | 常见用途 |
| --- | --- | --- |
| `OnLoad` | mod 加载成功后 | 读配置、注册命令、启动 Timer、初始化成员变量 |
| `OnUnload` | mod 卸载前 | 清理对象、取消物理化、注销命令、释放脚本状态 |
| `OnProcess` | 每帧 | 更新 ImGui 调试窗口、读取输入、刷新轻量状态 |
| `OnGameEvent` | BML 发出游戏事件时 | 关卡开始后扫描对象，退出关卡前清理缓存 |
| `OnRender` | Virtools 渲染窗口期 | 观察渲染事件，处理与渲染流程有关的轻量逻辑 |
| `OnCommandEvent` | 命令流程变化时 | 观察命令执行前后、记录参数和阶段 |
| `OnModifyConfig` | 配置项变化时 | 同步脚本成员变量、记录配置变化 |
| `OnLoadObject` | Object Load 发生时 | 观察资源文件和对象进入运行时 |
| `OnLoadScript` | Virtools 行为脚本加载时 | 观察行为脚本来源和 id |
| `OnPhysicalize` | 3D 对象物理化时 | 记录目标、摩擦、质量、碰撞组等参数 |
| `OnUnphysicalize` | 3D 对象取消物理化时 | 记录目标，清理脚本保存的相关状态 |
| `OnCheatEnabled` | Cheat 状态变化时 | 同步和调试功能有关的开关 |

常用签名：

```angelscript
void OnLoad(const BML::ModContext &in ctx)
void OnUnload(const BML::ModContext &in ctx)
void OnProcess(const BML::ModContext &in ctx)
void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event)
```

进阶回调签名：

```angelscript
void OnRender(const BML::ModContext &in ctx, const BML::RenderEvent &in event)
void OnCommandEvent(const BML::ModContext &in ctx, const BML::CommandEvent &in event)
void OnModifyConfig(const BML::ModContext &in ctx, const BML::ConfigEvent &in event)
void OnLoadObject(const BML::ModContext &in ctx, const BML::LoadObjectEvent &in event)
void OnLoadScript(const BML::ModContext &in ctx, const BML::LoadScriptEvent &in event)
void OnPhysicalize(const BML::ModContext &in ctx, const BML::PhysicalizeEvent &in event)
void OnUnphysicalize(const BML::ModContext &in ctx, const BML::ObjectEvent &in event)
void OnCheatEnabled(const BML::ModContext &in ctx, const BML::CheatEvent &in event)
```

签名错了，BML 就找不到这个回调。常见错误包括少写 `const`、少写 `&in`、把事件对象类型写错、函数名拼错。

## 游戏事件

`OnGameEvent` 的第二个参数是枚举：

```angelscript
void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event)
```

常用事件：

| 事件 | 适合做什么 |
| --- | --- |
| `GAME_EVENT_PRE_LOAD_LEVEL` | 清掉上一关缓存，准备进入新关 |
| `GAME_EVENT_POST_LOAD_LEVEL` | 记录关卡文件加载完成 |
| `GAME_EVENT_START_LEVEL` | 查找关卡对象、组、DataArray |
| `GAME_EVENT_PRE_RESET_LEVEL` | 重置前保存或清理临时状态 |
| `GAME_EVENT_POST_RESET_LEVEL` | 重置后重新扫描对象 |
| `GAME_EVENT_PRE_EXIT_LEVEL` | 退出关卡前清理自建对象和缓存 |
| `GAME_EVENT_POST_EXIT_LEVEL` | 确认关卡相关状态已经释放 |
| `GAME_EVENT_EXIT_GAME` | 游戏退出时收尾 |
| `GAME_EVENT_EXTRA_POINT` | 观察加分事件后的分数状态 |
| `GAME_EVENT_PRE_CHECKPOINT_REACHED` | 检查点触发前观察状态 |
| `GAME_EVENT_POST_CHECKPOINT_REACHED` | 检查点触发后重新读运行时表 |

事件名可以转成文本：

```angelscript
string name = BML::GetGameEventName(event);
```

入门阶段最常用的是：

```text
GAME_EVENT_START_LEVEL      进入关卡后查对象
GAME_EVENT_PRE_EXIT_LEVEL   离开关卡前清理
GAME_EVENT_EXIT_GAME        退出游戏时清理
```

## `BML::ModContext`

`ctx` 是当前 mod 的服务入口。常用函数按用途分：

| 用途 | 函数 |
| --- | --- |
| 当前 mod 信息 | `GetModId()`、`GetModName()` |
| 日志 | `BorrowLogger()` |
| 配置 | `BorrowConfig()` |
| 输入 | `BorrowInputManager()` |
| 游戏状态 | `GetIsInGame()`、`GetIsInLevel()`、`GetIsPaused()`、`GetIsPlaying()` |
| 命令 | `RegisterCommand(...)`、`UnregisterCommand(...)`、`HasCommand(...)`、`ExecuteCommand(...)` |
| Timer | `SetTimeout(...)`、`SetInterval(...)`、`AddTimer(...)` |
| 文件和资源 | `GetModRootUtf8()`、`ResolveModPathUtf8(...)`、`ModFileExistsUtf8(...)`、`ReadModTextFileUtf8(...)` |
| mod 列表 | `GetModCount()`、`GetMod(index)`、`FindMod(id)` |
| DataShare | `RequestDataShare(...)` |
| HUD / 菜单 | `SetHUD(...)`、`ShowFPS(...)`、`OpenModsMenu()`、`OpenMapMenu()` |
| CKAS / Virtools 入口 | `BorrowCKContext()`、`BorrowRenderContext()`、`BorrowDataArrayByName(...)`、`BorrowGroupByName(...)`、`Borrow3dEntityByName(...)` |

`Borrow*` 返回值先判空：

```angelscript
BML::Logger@ logger = ctx.BorrowLogger();
if (logger !is null) {
    logger.Info("ready");
}
```

关卡对象类的 `Borrow*` 更要谨慎。菜单阶段、关卡还没开始、关卡已经退出时，结果可能为空。

## 日志

日志服务：

```angelscript
BML::Logger@ logger = ctx.BorrowLogger();
```

常用方法：

```angelscript
logger.Info("message");
logger.Warn("message");
logger.Error("message");
```

用法建议：

| 等级 | 用法 |
| --- | --- |
| `Info` | 正常流程，例如加载成功、命令执行、状态摘要 |
| `Warn` | 功能跳过，例如资源缺失、对象没找到、配置不可用 |
| `Error` | 功能失败，例如无法继续执行的初始化错误 |

不要在 `OnProcess` 里每帧写日志。每帧日志会淹没真正有用的信息。

## 配置

配置入口：

```angelscript
BML::Config@ config = ctx.BorrowConfig();
```

常用对象：

```text
BML::Config
BML::ConfigProperty
```

典型写法：

```angelscript
BML::ConfigProperty@ prop = config.GetProperty("Window", "Show");
if (prop !is null) {
    prop.SetDefaultBoolean(true);
    prop.SetComment("Show the window.");
    bool show = prop.GetBoolean(true);
}
```

配置适合保存脚本自己的长期偏好。不要把关卡对象 id、临时句柄、当前帧状态写进配置。

## 命令

命令常用两种注册方式。教程主要使用 `CommandDefinition`：

```angelscript
BML::CommandDefinition def;
def.Name = "hello";
def.Alias = "h";
def.Description = "Hello command";
def.Usage = "hello [status|toggle]";

BML::CommandCallback@ execute = BML::CommandCallback(this.OnHelloCommand);
BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompleteHelloCommand);
BML::CommandRef@ ref = ctx.RegisterCommand(def, execute, complete);
```

执行回调：

```angelscript
void OnHelloCommand(const BML::ModContext &in ctx, const BML::CommandEvent &in event)
```

补全回调：

```angelscript
void CompleteHelloCommand(const BML::ModContext &in ctx,
                          const BML::CommandEvent &in event,
                          BML::CommandCompletion &inout completions)
```

命令事件里常用：

| 字段或函数 | 含义 |
| --- | --- |
| `event.CommandName` | 命令名 |
| `event.ArgCount` | 参数数量 |
| `event.GetArg(0)` | 第一个参数 |
| `event.ArgsText` | 参数原文 |
| `event.Phase` | 命令流程阶段 |

命令适合手动触发动作：扫描、刷新、清理、切换窗口、打印状态。

## Timer

Timer 适合延迟或低频执行：

```angelscript
BML::TimerCallback@ once = BML::TimerCallback(this.OnOnce);
ctx.SetTimeout(1000.0f, once, "one-shot");

BML::TimerLoopCallback@ loop = BML::TimerLoopCallback(this.OnLoop);
ctx.SetInterval(500.0f, loop, "loop");
```

一次性回调：

```angelscript
void OnOnce(const BML::ModContext &in ctx, const BML::TimerEvent &in event)
```

循环回调：

```angelscript
bool OnLoop(const BML::ModContext &in ctx, const BML::TimerEvent &in event)
```

循环回调返回：

| 返回值 | 含义 |
| --- | --- |
| `true` | 继续下一次 |
| `false` | 停止循环 |

Timer 适合做：

```text
延迟提示
低频刷新状态
分批扫描
节流日志
```

不适合做需要每帧响应的输入处理。每帧输入还是放在 `OnProcess`，但要避免重复触发。

## DataShare

DataShare 是 BML 层的轻量共享状态。

直接读写：

```angelscript
BML::DataShareSetString("status", "ready", "my.mod");
string status = BML::DataShareGetString("status", "none", "my.mod");
```

常用类型：

```text
string
bool
int
float
```

也可以请求某个 key 的变化：

```angelscript
BML::DataShareCallback@ callback = BML::DataShareCallback(this.OnShareChanged);
ctx.RequestDataShare("status", BML::DATASHARE_STRING, callback, "my.mod");
```

DataShare 适合共享轻量状态，例如：

```text
当前模式
是否启用某个功能
一个状态文本
一个计数值
```

不要用 DataShare 保存复杂对象句柄。

## UI、HUD 和菜单

BML 层有几类显示入口：

| 类型 | 用途 |
| --- | --- |
| ImGui | 调试窗口、状态面板、工具窗口 |
| `ctx.SendIngameMessage(...)` | 游戏内短消息 |
| `BML::UI::SendMessage(...)` | BML 消息区域短提示 |
| HUD | 标题、FPS、SR Timer 等 HUD 开关 |
| 菜单 | 打开 Mods Menu、Map Menu |

常用函数：

```angelscript
ctx.SendIngameMessage("done");
BML::UI::SendMessage("done");
ctx.ShowFPS(true);
ctx.OpenModsMenu();
```

ImGui 窗口一般写在 `OnProcess` 里，状态放成员变量。短消息只放重要结果，不要当成长文本日志。

## CKAS / Virtools 入口

这些函数会接触 Virtools 运行时对象：

```angelscript
CKDataArray@ BorrowDataArrayByName(const string &in name) const;
CKGroup@ BorrowGroupByName(const string &in name) const;
CK3dEntity@ Borrow3dEntityByName(const string &in name) const;
CKBehavior@ BorrowScriptByName(const string &in name) const;
```

使用规则：

```text
先等关卡时机合适
Borrow 返回值先判空
短期使用 raw CK handle
跨关卡不要继续使用旧句柄
需要长期身份时保存名字、id 或 CKAS 引用包装
```

查对象、组、DataArray 属于进入 CKAS / Virtools 层。它和日志、命令、配置分属不同层。

## 常用搭配

| 需求 | 推荐入口 |
| --- | --- |
| 脚本加载时初始化 | `OnLoad` + `BorrowLogger` + `BorrowConfig` |
| 每帧显示调试窗口 | `OnProcess` + ImGui |
| 用户手动触发动作 | `RegisterCommand` + 命令回调 |
| 延迟或低频执行 | `SetTimeout` / `SetInterval` |
| 进入关卡后查对象 | `OnGameEvent` + `GAME_EVENT_START_LEVEL` |
| 离开关卡前清理 | `OnGameEvent` + `GAME_EVENT_PRE_EXIT_LEVEL` |
| 观察配置变化 | `OnModifyConfig` |
| 观察资源加载 | `OnLoadObject` |
| 观察物理化 | `OnPhysicalize` |
| 跨 mod 共享轻量状态 | DataShare |

遇到问题时，先用日志确认三件事：

```text
回调有没有被调用
服务句柄是否为 null
分支条件有没有走到
```

这比直接改对象更容易定位问题。

# 参考 C：ModContext 服务地图

BML 脚本 mod 的常见回调都会收到一个 `ctx`：

```angelscript
void OnLoad(const BML::ModContext &in ctx)
void OnProcess(const BML::ModContext &in ctx)
void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event)
```

`ctx` 的完整类型是 `BML::ModContext`。

可以先把它理解成：

```text
BML 交给当前脚本的一张服务地图
```

脚本不直接凭空写日志、找配置、读输入、找对象。大多数入口都从 `ctx` 开始。

## `ctx` 分成几类服务

先按用途分：

| 类别 | 常用函数 |
| --- | --- |
| 当前 mod 身份 | `GetModId()`、`GetModName()` |
| 日志和配置 | `BorrowLogger()`、`BorrowConfig()` |
| 输入和每帧状态 | `BorrowInputManager()`，以及 ImGui 的 `GetIO().DeltaTime` |
| 命令和 Timer | `RegisterCommand(...)`、`SetTimeout(...)`、`SetInterval(...)` |
| 游戏状态 | `GetIsInGame()`、`GetIsInLevel()`、`GetIsPaused()`、`GetIsPlaying()` |
| 菜单和 HUD | `SendIngameMessage(...)`、`ShowFPS(...)`、`OpenModsMenu()` |
| 文件和资源 | `GetModRootUtf8()`、`ResolveModPathUtf8(...)`、`ReadModTextFileUtf8(...)` |
| mod 列表 | `GetModCount()`、`GetMod(...)`、`FindMod(...)` |
| CKAS / Virtools 入口 | `BorrowCKContext()`、`BorrowRenderContext()`、`BorrowDataArrayByName(...)`、`Borrow3dEntityByName(...)` |

基础模板用过日志、配置、输入、命令、Timer。


## 用小脚本打印服务状态

新建脚本：

```text
ModLoader/Mods/CtxMapMod.mod.as
```

写入：

```angelscript
[bml.mod id="ctxmap.script" name="Context Map Demo" version="1.0.0" author="Tutorial" bml="0.3.12" description="Inspect ModContext services"]
class CtxMapMod {
    void OnLoad(const BML::ModContext &in ctx) {
        LogInfo(ctx, "CtxMap mod id=" + ctx.GetModId() + " name=" + ctx.GetModName());
        LogInfo(ctx, "CtxMap mod count=" + ctx.GetModCount());

        BML::Config@ config = ctx.BorrowConfig();
        BML::InputHook@ input = ctx.BorrowInputManager();
        CKContext@ ck = ctx.BorrowCKContext();
        CKRenderContext@ render = ctx.BorrowRenderContext();

        LogInfo(ctx, "CtxMap services config=" + BoolText(config !is null) +
                     " input=" + BoolText(input !is null) +
                     " ck=" + BoolText(ck !is null) +
                     " render=" + BoolText(render !is null));

        LogInfo(ctx, "CtxMap game inGame=" + BoolText(ctx.GetIsInGame()) +
                     " inLevel=" + BoolText(ctx.GetIsInLevel()) +
                     " paused=" + BoolText(ctx.GetIsPaused()) +
                     " playing=" + BoolText(ctx.GetIsPlaying()));

        LogInfo(ctx, "CtxMap root=" + ctx.GetModRootUtf8());
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        string name = BML::GetGameEventName(event);

        if (event == BML::GAME_EVENT_START_LEVEL) {
            LogInfo(ctx, "CtxMap level started");
            LogLevelEntryPoints(ctx);
            return;
        }

        if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL ||
            event == BML::GAME_EVENT_POST_EXIT_LEVEL ||
            event == BML::GAME_EVENT_EXIT_GAME) {
            LogInfo(ctx, "CtxMap leaving level or game: " + name);
        }
    }

    private void LogLevelEntryPoints(const BML::ModContext &in ctx) {
        CKDataArray@ currentLevel = ctx.BorrowDataArrayByName("CurrentLevel");
        CK3dEntity@ ball = ctx.Borrow3dEntityByName("Ball");

        LogInfo(ctx, "CtxMap level entries CurrentLevel=" + BoolText(currentLevel !is null) +
                     " Ball=" + BoolText(ball !is null));
    }

    private void LogInfo(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info(message);
        }
    }

    private string BoolText(bool value) {
        return value ? "true" : "false";
    }
}
```

这份脚本只打印状态，不修改游戏对象。

## `Borrow*` 要按临时借用理解

很多函数都叫 `Borrow...`：

```angelscript
BorrowLogger()
BorrowConfig()
BorrowInputManager()
BorrowCKContext()
BorrowRenderContext()
BorrowDataArrayByName(...)
Borrow3dEntityByName(...)
```

`Borrow` 先按“借来用一下”理解。

拿到句柄后，先检查：

```angelscript
CKContext@ ck = ctx.BorrowCKContext();
if (ck !is null) {
    // 在这里使用
}
```

不要在入门阶段把 `CKContext@`、`CK3dEntity@`、`CKDataArray@` 长期保存成成员变量。关卡切换、退出关卡、对象销毁后，这些句柄可能不再适合继续用。

短规则：

```text
用的时候借
借到先判空
关卡对象不要长期缓存
```

如果确实需要跨多次调用保存对象身份，再引入 CKAS 的 `ObjectRef@`、`Entity3DRef@` 这类引用层。

## BML 层服务

这些服务属于 BML 脚本 mod 自己：

```text
日志
配置
命令
Timer
路径
mod 列表
游戏内消息
菜单和 HUD
```

例如日志：

```angelscript
BML::Logger@ logger = ctx.BorrowLogger();
if (logger !is null) {
    logger.Info("message");
}
```

例如配置：

```angelscript
BML::Config@ config = ctx.BorrowConfig();
if (config !is null) {
    BML::ConfigProperty@ prop = config.GetProperty("Base", "ShowMessage");
}
```

例如 mod 身份：

```angelscript
string id = ctx.GetModId();
string name = ctx.GetModName();
int count = ctx.GetModCount();
```

这些不要求理解 Virtools 对象。

## CKAS / Virtools 入口

这些入口开始接近游戏运行时：

```angelscript
CKContext@ BorrowCKContext() const;
CKRenderContext@ BorrowRenderContext() const;
CKDataArray@ BorrowDataArrayByName(const string &in name) const;
CKGroup@ BorrowGroupByName(const string &in name) const;
CK3dEntity@ Borrow3dEntityByName(const string &in name) const;
CKBehavior@ BorrowScriptByName(const string &in name) const;
```

这些返回的是 CKAS / Virtools 侧的句柄。

例如：

```angelscript
CKDataArray@ currentLevel = ctx.BorrowDataArrayByName("CurrentLevel");
```

这表示从当前运行时环境里按名字找一个 DataArray。

如果当前还在菜单，或者关卡还没开始，它可能是 `null`。所以参考 B说过，读关卡对象通常等到：

```angelscript
event == BML::GAME_EVENT_START_LEVEL
```

## 路径和资源

路径相关函数先记三个：

```angelscript
string GetModRootUtf8() const;
string ResolveModPathUtf8(const string &in relativePath) const;
bool ModFileExistsUtf8(const string &in relativePath) const;
```

它们用于访问当前 mod 自己的文件。

例如：

```angelscript
string root = ctx.GetModRootUtf8();
string path = ctx.ResolveModPathUtf8("data/settings.txt");
bool exists = ctx.ModFileExistsUtf8("data/settings.txt");
```

文档和示例不写本机绝对路径。脚本资源应当放在 mod 自己目录下面，再用相对路径访问。

路径、资源和日志诊断见参考 D。

## 游戏状态查询

这些函数用来判断当前处于什么状态：

```angelscript
ctx.GetIsInGame()
ctx.GetIsInLevel()
ctx.GetIsPaused()
ctx.GetIsPlaying()
```

它们适合做保护条件：

```angelscript
if (!ctx.GetIsInLevel()) {
    return;
}
```

但它们不能替代 `OnGameEvent`。

判断“现在是不是在关卡里”用状态查询。
判断“刚刚进入关卡”用 `GAME_EVENT_START_LEVEL`。

## 运行后看日志

保存脚本，重启 Player。

在开始菜单阶段，可以看到：

```text
[ctxmap.script/INFO]: CtxMap mod id=ctxmap.script name=Context Map Demo
[ctxmap.script/INFO]: CtxMap mod count=3
[ctxmap.script/INFO]: CtxMap services config=true input=true ck=true render=true
[ctxmap.script/INFO]: CtxMap game inGame=false inLevel=false paused=false playing=false
[ctxmap.script/INFO]: CtxMap root=<mod 根目录>
[ModLoader/INFO]: BML script mod summary: loaded=1 failed=0
```

如果进入关卡，`GAME_EVENT_START_LEVEL` 之后还会看到：

```text
[ctxmap.script/INFO]: CtxMap level started
[ctxmap.script/INFO]: CtxMap level entries CurrentLevel=true Ball=true
```

不同启动阶段、窗口焦点、游戏状态可能导致布尔值不同。先看日志形状，不要把某个布尔值当成所有机器都固定的结果。

`root` 实际会输出当前 mod 的完整目录。文档中用 `<mod 根目录>` 代替本机路径。

## 速查结论

可将 `ctx` 看成一张服务地图：

```text
BML 层
  日志、配置、命令、Timer、路径、mod 身份、游戏状态

CKAS / Virtools 入口
  CKContext、RenderContext、DataArray、Group、Entity、Behavior
```

使用原则：

```text
先判断自己要做的是 BML 事情，还是关卡运行时事情
从 ctx 找入口
Borrow 返回值先判空
关卡对象等 GAME_EVENT_START_LEVEL
退出关卡后清掉自己保存的关卡状态
```

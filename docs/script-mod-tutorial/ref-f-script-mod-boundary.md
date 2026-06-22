# 参考 F：脚本 mod 的边界

相关基础中已经会写日志、窗口、命令、配置、Timer、事件、资源路径。
这些都属于 BML 脚本 mod 自己的运行框架。

使用 CKAS 和 Virtools 对象前，先把边界摆清楚。

## 先看三层

可以先用这张图记位置：

```text
Ballance Player
  BMLPlus
    BML 脚本 mod
      BML::ModContext
      日志、命令、配置、Timer、资源、DataShare、BML 菜单和 HUD

  CKAngelScript
    CKAS runtime script
      ScriptContext
      Runtime、Scene、Message、Async 等 CKAS 服务

    AngelScript Component
      CKBehaviorContext
      行为图里的脚本组件、BB 配置、行为实例上下文

  Virtools 运行时
    CKContext、CKRenderContext、CKObject、CKGroup、CKDataArray、CKBehavior
```

BML 脚本 mod 是由 CKAS 执行的 AngelScript 代码，同时有自己的 BML mod 身份。它和 CKAS runtime script、AngelScript Component 是三种入口。

判断自己站在哪一层，看入口参数就够了：

```angelscript
void OnLoad(const BML::ModContext &in ctx)
```

这里拿到的是 `BML::ModContext`，所以这份脚本是 BML 脚本 mod。

## 三种上下文不要混用

先记这张表：

| 脚本形态 | 常见入口 | 主要用途 |
| --- | --- | --- |
| BML 脚本 mod | `BML::ModContext` | mod 身份、加载顺序、日志、配置、命令、Timer、资源、DataShare、BML UI |
| CKAS runtime script | `ScriptContext` | CKAS 长期脚本、Runtime、Scene、Message、Async |
| AngelScript Component | `CKBehaviorContext` | 挂在 Virtools 行为图里的脚本组件 |

不能把一个上下文当成另一个上下文传。

例如，BML 回调里有：

```angelscript
void OnLoad(const BML::ModContext &in ctx)
```

这时 `ctx` 的类型只有 `BML::ModContext`。
如果某个 CKAS 函数声明要求的是：

```angelscript
SomeFunction(const ScriptContext &in ctx)
```

BML 的 `ctx` 不能直接传进去。

遇到 CKAS API 时，先看函数声明要求哪种上下文。声明写 `CKContext@`，BML 脚本 mod 才能考虑用 `ctx.BorrowCKContext()` 取得入口。

## BML 层负责什么

BML 层管的是“mod 怎么作为 mod 运行”：

```text
mod id、名称、版本
加载和卸载
命令
配置
Timer
日志
资源路径
DataShare
BML 菜单、HUD、游戏内消息
BML 事件回调
```

这些事情从 `BML::ModContext` 开始。

例如：

```angelscript
ctx.GetModId()
ctx.BorrowLogger()
ctx.BorrowConfig()
ctx.RegisterCommand(...)
ctx.SetInterval(...)
ctx.ResolveModPathUtf8("data/message.txt")
```

这些 API 不要求先理解 Virtools 对象。

## CKAS / Virtools 层负责什么

CKAS / Virtools 层管的是游戏运行时里的对象和逻辑：

```text
场景对象
组
DataArray
材质、贴图、网格
行为图
Building Block
参数
物理对象和物理事件
```

BML 脚本 mod 可以通过 `ModContext` 借到几个入口：

```angelscript
CKContext@ BorrowCKContext() const;
CKRenderContext@ BorrowRenderContext() const;
CKDataArray@ BorrowDataArrayByName(const string &in name) const;
CKGroup@ BorrowGroupByName(const string &in name) const;
CK3dEntity@ Borrow3dEntityByName(const string &in name) const;
CKBehavior@ BorrowScriptByName(const string &in name) const;
```

这些返回的是 CKAS / Virtools 侧的句柄。拿到以后先判空，只在当前函数附近使用。

## 用小脚本确认边界

新建脚本：

```text
ModLoader/Mods/BoundaryMod.mod.as
```

写入：

```angelscript
[bml.mod id="boundary.script" name="Boundary Demo" version="1.0.0" author="Tutorial" bml="0.3.12" description="Inspect BML and CKAS boundary"]
class BoundaryMod {
    void OnLoad(const BML::ModContext &in ctx) {
        LogInfo(ctx, "Boundary id=" + ctx.GetModId() + " name=" + ctx.GetModName());
        LogInfo(ctx, "Boundary bml mods=" + ctx.GetModCount() +
                     " inGame=" + BoolText(ctx.GetIsInGame()) +
                     " inLevel=" + BoolText(ctx.GetIsInLevel()));

        ProbeBmlServices(ctx);
        ProbeBorrowedCkEntrances(ctx);

        LogInfo(ctx, "Boundary loaded");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event != BML::GAME_EVENT_START_LEVEL) {
            return;
        }

        LogInfo(ctx, "Boundary level started");
        ProbeLevelObjects(ctx);
    }

    void OnUnload(const BML::ModContext &in ctx) {
        LogInfo(ctx, "Boundary unload");
    }

    private void ProbeBmlServices(const BML::ModContext &in ctx) {
        BML::Logger@ logger = ctx.BorrowLogger();
        BML::Config@ config = ctx.BorrowConfig();
        BML::InputHook@ input = ctx.BorrowInputManager();

        LogInfo(ctx, "Boundary bml services logger=" + BoolText(logger !is null) +
                     " config=" + BoolText(config !is null) +
                     " input=" + BoolText(input !is null));
    }

    private void ProbeBorrowedCkEntrances(const BML::ModContext &in ctx) {
        // 这里只确认入口是否存在，不保存 CKContext@ 和 CKRenderContext@。
        CKContext@ ck = ctx.BorrowCKContext();
        CKRenderContext@ render = ctx.BorrowRenderContext();

        LogInfo(ctx, "Boundary ck entrances ck=" + BoolText(ck !is null) +
                     " render=" + BoolText(render !is null));
    }

    private void ProbeLevelObjects(const BML::ModContext &in ctx) {
        // 关卡对象等关卡开始后再找。找到后也只在当前函数里使用。
        CKDataArray@ currentLevel = ctx.BorrowDataArrayByName("CurrentLevel");
        CK3dEntity@ ball = ctx.Borrow3dEntityByName("Ball");

        LogInfo(ctx, "Boundary level objects CurrentLevel=" + BoolText(currentLevel !is null) +
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

这份脚本做三件事：

```text
确认 BML 层服务能借到
确认 CKContext / RenderContext 入口能借到
等关卡开始后再尝试找关卡对象
```

它不保存 `CKContext@`，不保存 `CKRenderContext@`，也不保存 `CKDataArray@` 或 `CK3dEntity@`。

## `Borrow*` 的含义

这里反复出现 `Borrow`：

```angelscript
ctx.BorrowLogger()
ctx.BorrowConfig()
ctx.BorrowInputManager()
ctx.BorrowCKContext()
ctx.BorrowRenderContext()
ctx.BorrowDataArrayByName("CurrentLevel")
ctx.Borrow3dEntityByName("Ball")
```

先按“借来用一下”理解。

借来的 CK/Virtools 句柄要遵守三条：

```text
拿到后先判空
尽量在当前函数附近使用
不要当成长期身份保存
```

这样写可以：

```angelscript
CK3dEntity@ ball = ctx.Borrow3dEntityByName("Ball");
if (ball is null) {
    return;
}
```

不要在入门阶段这样写：

```angelscript
private CK3dEntity@ cachedBall;
```

球、机关、DataArray、行为图都属于关卡运行时。退出关卡、重新加载关卡、切换地图后，旧句柄可能已经不适合继续用。

需要长期记住对象时，优先保存更稳定的信息：

```text
对象名
对象 id
```

## 关卡对象要等时机

`OnLoad` 只说明 mod 加载了，不说明关卡已经开始。

所以这段代码放在 `GAME_EVENT_START_LEVEL` 后：

```angelscript
void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
    if (event != BML::GAME_EVENT_START_LEVEL) {
        return;
    }

    ProbeLevelObjects(ctx);
}
```

如果在开始菜单阶段找 `CurrentLevel` 或 `Ball`，结果可能是 `null`。常见原因是时机还没到。

## 运行后看日志

保存脚本，重启 Player。

开始菜单阶段可以看到：

```text
[ModLoader/INFO]: Loading Mod boundary.script[Boundary Demo] v1.0.0 by Tutorial
[boundary.script/INFO]: Boundary id=boundary.script name=Boundary Demo
[boundary.script/INFO]: Boundary bml mods=3 inGame=false inLevel=false
[boundary.script/INFO]: Boundary bml services logger=true config=true input=true
[boundary.script/INFO]: Boundary ck entrances ck=true render=true
[boundary.script/INFO]: Boundary loaded
[ModLoader/INFO]: BML script mod summary: loaded=1 failed=0
```

退出 Player 时会看到：

```text
[boundary.script/INFO]: Boundary unload
```

如果进入关卡，`GAME_EVENT_START_LEVEL` 之后还会看到：

```text
[boundary.script/INFO]: Boundary level started
[boundary.script/INFO]: Boundary level objects CurrentLevel=true Ball=true
```

`bml mods=3` 是这次测试环境里的结果。实际启用的 mod 数量不同，这个数字会变。

## 什么时候用哪一层

按目标选入口：

| 目标 | 先考虑 |
| --- | --- |
| 做一个可发布的 BML mod，有配置、命令、日志、资源、版本号 | BML 脚本 mod |
| 查找或观察 Virtools 对象、组、DataArray | BML 脚本 mod 借 CKAS 入口，或 CKAS runtime script |
| 给某个行为图节点挂脚本逻辑 | AngelScript Component |
| 读行为图、参数、Building Block 信息 | CKAS 的 `Behavior`、`BB`、`Param` API |
| 脚本之间通信、做协作式异步任务 | CKAS `Message` / `Async`，或 BML DataShare，取决于通信对象是谁 |
| 改底层渲染、输入 hook、性能敏感逻辑 | 原生插件，再通过 CKAS 暴露安全接口 |

一句话：

```text
BML 负责 mod 语义
CKAS 负责脚本运行时和 Virtools 能力
Virtools 负责游戏对象和行为图
```

## 速查结论

边界速查：

```text
BML::ModContext、ScriptContext、CKBehaviorContext 是三种上下文
BorrowCKContext() 只是借 CKContext 入口
Borrow* 返回的 CK 句柄不代表脚本拥有它
关卡对象等关卡开始后再找
长期状态优先保存名字、id 或引用包装
```

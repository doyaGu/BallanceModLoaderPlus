# 第 4 章：BML 怎么调用脚本

第 3 章的脚本能加载，也能写日志。

现在要补上一个关键模型：脚本文件放进目录后，不会像普通程序那样从第一行一路执行到最后。BMLPlus 会加载脚本、创建带 `[bml.mod ...]` 的类，然后在固定时机调用这个类里的函数。

这种“由 BMLPlus 在指定时机调用的函数”，叫回调。

本章先只建立三个点：

```text
脚本写函数
BMLPlus 按时机调用函数
ctx 是每次调用时传进来的 BML 入口
```

## 先看调用关系

可以先按这个顺序理解：

```text
脚本文件
  写好 class HelloMod
    写好 OnLoad / OnProcess 这些函数

BMLPlus
  加载脚本
  创建 HelloMod 对象
  到时间后调用对应函数
```

脚本负责把函数写出来。  
BMLPlus 负责决定什么时候调用。

第 3 章已经用过 `OnLoad`：

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
    BML::Logger@ logger = ctx.BorrowLogger();
    if (logger !is null) {
        logger.Info("HelloMod loaded from ModLoader/Mods/HelloMod.mod.as");
    }
}
```

这段函数本身不会自动执行。BMLPlus 在脚本加载完成后调用它一次。

## 固定回调和注册回调

前几章会遇到两类回调。

```text
固定回调    函数名由 BML 规定，写对名字和参数就会被调用
注册回调    函数名可以自己取，但要先把函数交给 BML
```

`OnLoad`、`OnProcess` 属于固定回调。

```angelscript
void OnLoad(const BML::ModContext &in ctx)
void OnProcess(const BML::ModContext &in ctx)
```

命令、Timer 属于注册回调。函数名可以叫 `RunHelloCommand`、`OnHelloCommand`、`TickStatus`，但注册时必须把函数交给 BML。

```text
OnLoad 里注册命令
  玩家输入命令
    BML 调用命令回调

OnLoad 里启动 Timer
  时间到了
    BML 调用 Timer 回调
```

第 6 章会正式写命令，第 8 章会正式写 Timer。本章先把调用关系看懂。

## 固定回调一览

当前 BML 脚本 mod 可以识别这些固定回调。

```angelscript
void OnLoad(const BML::ModContext &in ctx)
void OnUnload(const BML::ModContext &in ctx)
void OnProcess(const BML::ModContext &in ctx)
void OnRender(const BML::ModContext &in ctx, const BML::RenderEvent &in event)
void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event)
void OnCheatEnabled(const BML::ModContext &in ctx, const BML::CheatEvent &in event)
void OnLoadObject(const BML::ModContext &in ctx, const BML::LoadObjectEvent &in event)
void OnLoadScript(const BML::ModContext &in ctx, const BML::LoadScriptEvent &in event)
void OnCommandEvent(const BML::ModContext &in ctx, const BML::CommandEvent &in event)
void OnModifyConfig(const BML::ModContext &in ctx, const BML::ConfigEvent &in event)
void OnPhysicalize(const BML::ModContext &in ctx, const BML::PhysicalizeEvent &in event)
void OnUnphysicalize(const BML::ModContext &in ctx, const BML::ObjectEvent &in event)
```

先不用背，也不用现在都写出来。第一篇只会真正用到 `OnLoad` 和 `OnProcess`，命令和 Timer 走注册回调。其他固定回调先认名字，后面用到时再回来看。

可以先按用途分组：

| 回调 | 先记用途 |
| --- | --- |
| `OnLoad` | 脚本加载后初始化 |
| `OnUnload` | 脚本卸载前清理 |
| `OnProcess` | 每帧更新轻量状态 |
| `OnRender` | 渲染阶段的回调 |
| `OnGameEvent` | 收到 BML 游戏事件 |
| `OnCheatEnabled` | Cheat 状态变化 |
| `OnLoadObject` | Virtools 对象加载后通知脚本 |
| `OnLoadScript` | Virtools 行为脚本加载后通知脚本 |
| `OnCommandEvent` | 观察命令执行前后 |
| `OnModifyConfig` | 配置项被修改时通知脚本 |
| `OnPhysicalize` | 对象物理化时通知脚本 |
| `OnUnphysicalize` | 对象取消物理化时通知脚本 |

## `OnLoad`

`OnLoad` 在脚本 mod 加载完成后调用一次。

适合放这些事情：

- 写一条启动日志；
- 读取配置；
- 注册命令；
- 启动 Timer；
- 初始化成员变量。

不适合放每帧都要更新的事情。

签名要照写：

```angelscript
void OnLoad(const BML::ModContext &in ctx)
```

下面这些都不会被当成正确的 `OnLoad`：

```angelscript
void OnLoaded(const BML::ModContext &in ctx)
void OnLoad()
int OnLoad(const BML::ModContext &in ctx)
```

BML 找的是完整函数声明。名字、返回值、参数类型有一个对不上，就不是同一个回调。

## `OnProcess`

`OnProcess` 在游戏运行时反复调用。可以先把它理解成“每帧更新”。

如果游戏大约 60 FPS，`OnProcess` 一秒大约会执行 60 次。这里适合处理轻量的每帧状态，例如：

- 检查某个按键这一帧有没有按下；
- 更新 FPS 计算；
- 根据成员变量刷新调试窗口状态。

不要在 `OnProcess` 里每帧写日志。日志会很快被刷满。

需要确认 `OnProcess` 是否被调用时，可以只记录第一次。

把下面代码加到 `HelloMod` 类里面，和 `OnLoad` 同一级：

```angelscript
private bool firstProcess = true;

void OnProcess(const BML::ModContext &in ctx) {
    if (!firstProcess) {
        return;
    }

    firstProcess = false;

    BML::Logger@ logger = ctx.BorrowLogger();
    if (logger !is null) {
        logger.Info("OnProcess reached first frame");
    }
}
```

代码里有两个新点。

`firstProcess` 是成员变量。它属于 `HelloMod` 对象，不会每帧重新变回 `true`。

`return` 会提前离开函数。第一次之后，`firstProcess` 已经变成 `false`，后面的帧会直接返回，不再写日志。

## `ctx` 每次都会传进来

这些固定回调都会收到 `ctx`：

```angelscript
void OnLoad(const BML::ModContext &in ctx)
void OnProcess(const BML::ModContext &in ctx)
```

`ctx` 是脚本和 BMLPlus 之间的入口。要写日志、读配置、注册命令、启动 Timer，都会从它开始。

可以把它当成当前 mod 的工具箱：

```text
ctx.BorrowLogger()      日志
ctx.BorrowConfig()      配置
ctx.RegisterCommand()   命令
ctx.SetTimeout()        一次性 Timer
ctx.SetInterval()       循环 Timer
```

前几章会逐个使用这些工具。现在先记住：回调函数收到 `ctx`，脚本通过 `ctx` 访问 BML 提供的服务。

## 命令回调

命令回调的函数名由脚本自己决定。

固定回调是这样：

```text
BML 看到 OnLoad 这个名字
  加载完成后调用 OnLoad
```

命令回调是这样：

```text
脚本注册一个命令
  注册时把某个函数交给 BML
    玩家执行命令时，BML 调用这个函数
```

命令回调要看注册关系。函数名只需要和注册时交出去的函数一致。

第 6 章会写完整命令。本章先看一眼形状：

```angelscript
BML::CommandCallback@ callback = BML::CommandCallback(this.OnHelloCommand);
```

这行的意思是：把 `OnHelloCommand` 包成一个命令回调，交给 BML 使用。

## Timer 回调

Timer 回调的函数名也由脚本自己决定。

Timer 的调用关系是：

```text
脚本启动 Timer
  BML 记住 Timer 和回调函数
    时间到了
      BML 调用回调函数
```

第 8 章会写完整 Timer。本章先看一眼形状：

```angelscript
BML::TimerCallback@ callback = BML::TimerCallback(this.OnDelayedMessage);
ctx.SetTimeout(1000.0f, callback, "hello-delay");
```

`1000.0f` 表示 1000 毫秒。时间到了以后，BML 调用 `OnDelayedMessage`。

Timer 适合延迟执行或低频执行。不要把所有定时逻辑都塞进 `OnProcess` 自己数帧。

## 本章结果

现在可以用这张图理解前几章和后几章：

```text
BMLPlus 加载脚本
  调用 OnLoad
    写日志
    后面会注册命令、读取配置、启动 Timer

游戏每帧运行
  调用 OnProcess
    后面会读取输入、刷新 FPS、更新调试窗口

玩家执行命令
  调用命令回调

Timer 到时间
  调用 Timer 回调
```

下一章会让脚本在游戏里显示一个调试窗口。窗口状态保存在成员变量里，每帧由 `OnProcess` 更新。

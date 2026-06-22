# 第 9 章：第一篇排错流程

脚本出问题时，先按顺序缩小范围。

这一章不新增 API，只整理排错顺序。

不要同时改文件名、代码、配置和命令。一次只确认一层：

```text
文件是否被 BML 找到
  脚本是否编译通过
    OnLoad 是否执行
      OnProcess 是否执行
        命令、配置、Timer 是否各自工作
```

前一层不成立，后一层通常没有意义。

## 先看日志

前面章节已经多次用过日志。排错时先打开：

```text
ModLoader/ModLoader.log
```

正常加载时，能看到这样的内容：

```text
[BML/INFO]: Loading Mod hello.script[Hello Mod] v1.0.0 by Tutorial
[hello.script/INFO]: HelloMod command registered: hello
[hello.script/INFO]: HelloMod loaded from ModLoader/Mods/HelloMod.mod.as messagesEnabled=true
[ModLoader/INFO]: BML script mod summary: loaded=3 failed=0
```

先找三类信息：

| 日志内容 | 说明 |
| --- | --- |
| `Loading Mod hello.script` | BML 找到了脚本，并读到了 mod 信息 |
| `hello.script/INFO` | 脚本里的日志已经执行 |
| `failed=0` | 本轮脚本加载没有失败项 |

如果日志里没有 `Loading Mod hello.script`，先查文件和 mod 信息。  
如果有 `Loading Mod hello.script`，但没有脚本自己的日志，继续查编译和回调。  
如果已经有脚本日志，再查具体功能。

## 脚本没被找到

先查文件名。

正确：

```text
HelloMod.mod.as
```

常见错误：

```text
HelloMod.as
HelloMod.mod.txt
HelloMod.mod.as.txt
```

Windows 资源管理器可能隐藏扩展名。看到文件名像 `HelloMod.mod.as`，实际可能是 `HelloMod.mod.as.txt`。排错时建议打开扩展名显示。

再查位置：

```text
ModLoader/Mods/HelloMod.mod.as
```

脚本文件要放在 `ModLoader/Mods` 下面。

然后查入口类前面的 mod 信息：

```angelscript
[bml.mod id="hello.script" name="Hello Mod" version="1.0.0" author="Tutorial" bml="0.3.12"]
class HelloMod {
}
```

`[bml.mod ...]` 是 BMLPlus 读取脚本 mod 信息用的标记。它不是 AngelScript 语言本身的语法，也不是注释。不要删掉，也不要放到别的类前面。

## 脚本编译失败

编译失败时，日志会给出错误位置或错误说明。先看第一条错误，不要从最后一条开始猜。

最常见的是这些：

| 错误类型 | 先查什么 |
| --- | --- |
| 少分号 | 上一行结尾有没有 `;` |
| 大括号不配对 | `{` 和 `}` 是否成对 |
| 名字拼错 | 函数名、变量名、类型名是否和教程一致 |
| 回调签名不对 | 返回值、参数、`const`、`&in` 是否照抄 |
| 句柄为空 | `xxx !is null` 之后再调用方法 |

例如日志对象可能为空，写日志时用这种结构：

```angelscript
private void LogInfo(const BML::ModContext &in ctx, const string &in message) {
    BML::Logger@ logger = ctx.BorrowLogger();
    if (logger !is null) {
        logger.Info(message);
    }
}
```

这里的关键点有两个：

```text
BML::Logger@ logger     logger 是对象句柄
logger !is null         确认句柄有效
```

后面看到 `CommandRef@`、`TimerRef@`、`Config@`，也按这个思路处理。

## `OnLoad` 没执行

`OnLoad` 是固定回调，名字和签名都要对。

正确：

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
    LogInfo(ctx, "HelloMod loaded.");
}
```

下面这些都不对：

```angelscript
void OnLoaded(const BML::ModContext &in ctx)
void OnLoad()
int OnLoad(const BML::ModContext &in ctx)
```

排错时可以先把 `OnLoad` 缩到最小：

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
    LogInfo(ctx, "OnLoad reached");
}
```

日志出现 `OnLoad reached`，说明脚本加载和 `OnLoad` 都已经通了。再把命令、配置、Timer 加回来。

## `OnProcess` 没执行

`OnProcess` 也是固定回调。

正确：

```angelscript
void OnProcess(const BML::ModContext &in ctx) {
}
```

看不到窗口时，先确认 `OnProcess` 是否真的在跑。可以加一个只写一次的日志：

```angelscript
private bool firstProcess = true;

void OnProcess(const BML::ModContext &in ctx) {
    if (firstProcess) {
        firstProcess = false;
        LogInfo(ctx, "OnProcess first frame");
    }

    DrawWindow();
}
```

如果日志里有：

```text
[hello.script/INFO]: OnProcess first frame
```

说明 `OnProcess` 没问题。接下来查窗口开关和 ImGui 代码。

## 看不到 ImGui 窗口

先查窗口是否被隐藏：

```angelscript
private bool showWindow = true;
```

再查 `OnProcess` 里有没有调用窗口函数：

```angelscript
void OnProcess(const BML::ModContext &in ctx) {
    HandleWindowToggle(ctx);
    UpdateFps();
    DrawWindow();
}
```

再查 `DrawWindow()` 是否因为开关提前返回：

```angelscript
private void DrawWindow() {
    if (!showWindow) {
        return;
    }

    if (ImGui::Begin("Hello")) {
        ImGui::TextUnformatted("HelloMod is running.");
    }
    ImGui::End();
}
```

如果按过 `F9`，窗口可能已经被隐藏。再按一次 `F9`，或者临时把 `showWindow` 改回 `true`。

## 命令没有反应

命令先看注册，再看输入。

`OnLoad` 里要调用：

```angelscript
RegisterCommands(ctx);
```

注册函数里要能拿到有效引用：

```angelscript
@helloCommand = ctx.RegisterCommand(def, execute, complete);

if (helloCommand is null || !helloCommand.IsValid) {
    LogWarn(ctx, "hello command registration failed");
    return;
}

LogInfo(ctx, "HelloMod command registered: " + helloCommand.Name);
```

正常日志：

```text
[hello.script/INFO]: HelloMod command registered: hello
```

命令栏输入的是命令名或别名：

```text
hello
h
```

不要输入函数名：

```text
OnHelloCommand
```

函数名只给脚本内部使用。命令栏只认识 `def.Name` 和 `def.Alias`。

## 命令补全没有出现

补全要分两步看。

第一步，注册命令时传入补全回调：

```angelscript
BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompleteHelloCommand);
@helloCommand = ctx.RegisterCommand(def, execute, complete);
```

第二步，补全函数签名要完整：

```angelscript
private void CompleteHelloCommand(const BML::ModContext &in ctx,
                                  const BML::CommandEvent &in event,
                                  BML::CommandCompletion &inout completions) {
    completions.Add("status");
    completions.Add("toggle");
    completions.Add("messages");
}
```

`BML::CommandCompletion &inout completions` 表示 BML 把补全列表交给函数，函数往里面加候选。

## 配置没生成

配置先看 `OnLoad` 里有没有调用：

```angelscript
LoadConfig(ctx);
```

再看是否拿到了配置对象：

```angelscript
BML::Config@ config = ctx.BorrowConfig();
if (config is null) {
    LogWarn(ctx, "config is not available");
    return;
}
```

再看是否访问了配置项：

```angelscript
@messagesEnabledProp = config.GetProperty("Hello", "MessagesEnabled");
messagesEnabledProp.SetDefaultBoolean(true);
messagesEnabled = messagesEnabledProp.GetBoolean();
```

配置文件通常在脚本成功加载并访问配置项后生成。修改脚本后没有看到配置变化时，先确认脚本这次真的加载成功。

## 配置改了但脚本没按配置走

配置项和成员变量是两件事。

配置项保存在文件里：

```angelscript
private BML::ConfigProperty@ messagesEnabledProp;
```

成员变量保存在当前脚本运行中：

```angelscript
private bool messagesEnabled = true;
```

读取配置时，要把配置值放进成员变量：

```angelscript
messagesEnabled = messagesEnabledProp.GetBoolean();
```

命令修改配置时，也要同时更新成员变量和配置项：

```angelscript
messagesEnabled = !messagesEnabled;
messagesEnabledProp.SetBoolean(messagesEnabled);
```

只改其中一个，会出现“配置文件里变了，但窗口里没变”或“窗口里变了，下次启动又恢复”的情况。

## Timer 没执行

Timer 先看启动，再看回调。

`OnLoad` 里要调用：

```angelscript
StartTimers(ctx);
```

启动后应该能看到：

```text
[hello.script/INFO]: HelloMod timers started: command=true status=true
```

如果这里是 `false`，先查 `SetTimeout` / `SetInterval` 是否返回了有效 `TimerRef`。

一次性 Timer 回调签名：

```angelscript
private void OnCommandTimer(const BML::ModContext &in ctx, const BML::TimerEvent &in event)
```

循环 Timer 回调签名：

```angelscript
private bool OnStatusTimer(const BML::ModContext &in ctx, const BML::TimerEvent &in event)
```

循环 Timer 的返回值决定是否继续：

```text
true   继续下一次
false  停止
```

第 8 章的例子第 3 次返回 `false`，所以只会看到三次：

```text
[hello.script/INFO]: HelloMod timer tick 1 from hello-status
[hello.script/INFO]: HelloMod timer tick 2 from hello-status
[hello.script/INFO]: HelloMod timer tick 3 from hello-status
```

另外，`SetTimeout(1000.0f, ...)` 和 `SetInterval(2000.0f, ...)` 的时间单位是毫秒。

## 改了脚本但游戏里没变化

修改 `.as` 后，重启 Player。

第一篇里的脚本写法不依赖热重载。保存文件后如果只切回游戏窗口，旧脚本可能还在运行。

重启后先看日志时间，再看脚本自己的日志：

```text
[hello.script/INFO]: HelloMod loaded from ModLoader/Mods/HelloMod.mod.as messagesEnabled=true
```

如果日志时间没变，通常是 Player 没重启。  
如果日志时间变了但内容还是旧的，通常是改错文件或文件没保存。

## 每次只改一层

排错时把范围缩小到一件事。

例如窗口看不到：

```text
先确认脚本加载
再确认 OnLoad 日志
再确认 OnProcess 日志
再确认 showWindow
最后查 ImGui::Begin / ImGui::End
```

命令没有反应：

```text
先确认 RegisterCommands(ctx)
再确认 command registered 日志
再确认命令栏输入 hello 或 h
再确认 OnHelloCommand 日志
最后查命令参数
```

Timer 没执行：

```text
先确认 StartTimers(ctx)
再确认 timers started 日志
再确认回调签名
再确认时间单位
最后查循环回调返回值
```

## 本章结果

第一篇写出的脚本已经有这些入口：

```text
OnLoad       启动时执行
OnProcess    每帧执行
Command      命令栏手动触发
Config       保存和读取设置
Timer        延迟或低频触发
```

排错时按这个顺序查：

```text
文件名和位置
[bml.mod ...]
编译日志
OnLoad 日志
OnProcess 日志
命令注册日志
配置项读取
Timer 启动日志
```

下一章讲怎么查脚本 API。看到没用过的类型、函数和回调签名时，先判断它属于 BML、CKAS 还是 CK/Virtools 绑定，再查对应资料。

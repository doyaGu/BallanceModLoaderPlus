# 第 6 章：命令

第 5 章已经能在画面里显示脚本状态。

这一章加一个手动入口：在 BML 命令栏输入 `hello`，让 BML 调用脚本里的函数。

先把命令看成三件东西：

```text
命令定义    名字、别名、说明、用法
执行回调    按回车后执行什么
补全回调    输入命令时给出候选词
```

本章注册一个命令：

```text
hello
```

它有一个别名：

```text
h
```

命令栏输入 `hello status` 后，流程是这样：

```text
玩家输入 hello status
  BML 找到 hello 命令
    BML 把 status 作为参数交给脚本
      脚本执行 OnHelloCommand
```

命令还是 BML 层的能力。它给脚本多了一个运行时入口，还没有进入 CKAS，也没有直接修改 Virtools 行为图。

## 改哪里

继续修改：

```text
ModLoader/Mods/HelloMod.mod.as
```

在第 5 章脚本基础上改四处：

```text
1. 类开头增加命令状态
2. OnLoad 里注册命令
3. DrawWindow 里显示最近一次命令
4. 类里增加 RegisterCommands、OnHelloCommand、CompleteHelloCommand
```

## 增加成员变量

在 `HelloMod` 类开头，放到 `fps` 后面：

```angelscript
// 保存注册后的命令引用，用来确认命令是否仍然有效。
private BML::CommandRef@ helloCommand;

// 显示最近一次执行的命令。
private string lastCommand = "none";
```

`helloCommand` 是 BML 返回的命令引用。注册成功后，可以从它读到命令名、别名、是否有效。

`lastCommand` 用来显示在 ImGui 窗口里。

## 在 `OnLoad` 里注册命令

把第 5 章的 `OnLoad` 改成这样：

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
    RegisterCommands(ctx);
    LogInfo(ctx, "HelloMod loaded from ModLoader/Mods/HelloMod.mod.as");
    ctx.SendIngameMessage("HelloMod loaded.");
}
```

`RegisterCommands(ctx)` 放在 `OnLoad` 里。脚本加载时注册一次就够了，不要放进 `OnProcess` 每帧注册。

## 在窗口里显示命令状态

在 `DrawWindow()` 里加一行：

```angelscript
ImGui::TextUnformatted("Last command: " + lastCommand);
```

放在 FPS 后面即可：

```angelscript
if (ImGui::Begin("Hello")) {
    ImGui::TextUnformatted("HelloMod is running.");
    ImGui::TextUnformatted("FPS: " + fps);
    ImGui::TextUnformatted("Last command: " + lastCommand);
    ImGui::TextUnformatted("Press F9 to hide or show this window.");
}
ImGui::End();
```

## 注册命令

把下面函数加到 `HelloMod` 类里，和 `UpdateFps`、`DrawWindow` 同一级。

```angelscript
private void RegisterCommands(const BML::ModContext &in ctx) {
    // CommandDefinition 描述命令本身。
    BML::CommandDefinition def;
    def.Name = "hello";
    def.Alias = "h";
    def.Description = "Show HelloMod status";
    def.Usage = "hello [status|toggle]";
    def.Category = "Tutorial";
    def.Enabled = true;

    // 执行命令时调用 OnHelloCommand。
    BML::CommandCallback@ execute = BML::CommandCallback(this.OnHelloCommand);

    // 命令栏需要补全时调用 CompleteHelloCommand。
    BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompleteHelloCommand);

    @helloCommand = ctx.RegisterCommand(def, execute, complete);
    if (helloCommand is null || !helloCommand.IsValid) {
        LogWarn(ctx, "HelloMod command registration failed");
        return;
    }

    LogInfo(ctx, "HelloMod command registered: " + helloCommand.Name);
}
```

这里有三个对象。

`CommandDefinition` 只描述命令：

```text
Name         命令名
Alias        别名
Description 说明
Usage        用法提示
Category     分类
Enabled      是否启用
```

`CommandCallback` 指向执行函数。

`CommandCompletionCallback` 指向补全函数。

本章走 `CommandDefinition` 这条路。`BML::Command` 接口先放下，直接用定义加回调更容易看清楚流程。

`helloCommand.Name` 是属性写法。API 里会看到 `get_Name() const`，脚本里通常写成 `.Name`，不用手写 `get_Name()`。

## 执行回调

继续加这个函数：

```angelscript
void OnHelloCommand(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
    string action = "status";
    if (event.ArgCount > 0) {
        action = event.GetArg(0);
    }

    if (event.ArgsText == "") {
        lastCommand = event.CommandName;
    } else {
        lastCommand = event.CommandName + " " + event.ArgsText;
    }

    LogInfo(ctx, "HelloMod command executed: " + lastCommand);
    ctx.SendIngameMessage("Command: " + lastCommand);

    if (action == "toggle") {
        showWindow = !showWindow;
        LogInfo(ctx, "HelloMod window visible=" + BoolText(showWindow));
    }
}
```

命令执行后，BML 会把 `CommandEvent` 传进来。

常用内容如下：

| 写法 | 含义 |
| --- | --- |
| `event.CommandName` | 注册命令名 |
| `event.ArgCount` | 参数数量 |
| `event.GetArg(0)` | 第一个参数 |
| `event.ArgsText` | 所有参数的原文 |

输入：

```text
hello status now
```

可以按下面这样读：

```text
CommandName  hello
ArgCount     2
GetArg(0)    status
GetArg(1)    now
ArgsText     status now
```

本章只处理第一个参数：

```text
hello          显示状态
hello status   显示状态
hello toggle   切换窗口显示状态
h toggle       用别名触发同一个 hello 命令
```

用别名触发时，`event.CommandName` 仍然是注册时的命令名 `hello`。

## 补全回调

再加补全函数：

```angelscript
void CompleteHelloCommand(const BML::ModContext &in ctx,
                          const BML::CommandEvent &in event,
                          BML::CommandCompletion &inout completions) {
    completions.Add("status");
    completions.Add("toggle");
}
```

命令栏里输入：

```text
hello 
```

注意 `hello` 后面有一个空格。此时命令栏会请求补全，脚本把候选词加到 `completions` 里。

本章固定返回两个候选：

```text
status
toggle
```

后面命令变复杂时，可以根据 `event.ArgCount` 和 `event.GetArg(...)` 决定补什么。本章先保持简单。

## 增加 `LogWarn`

第 5 章已经有 `LogInfo`。本章注册失败时还要写警告，所以加一个 `LogWarn`：

```angelscript
private void LogWarn(const BML::ModContext &in ctx, const string &in message) {
    BML::Logger@ logger = ctx.BorrowLogger();
    if (logger !is null) {
        logger.Warn(message);
    }
}
```

## 运行

保存脚本，重新启动 Player。

先看日志里有没有：

```text
HelloMod command registered: hello
```

这行出现，说明命令已经注册。

打开 BML 命令栏，输入：

```text
hello
```

再试：

```text
h status
```

`h` 是别名。命令执行后，日志里会出现这些内容：

```text
[ModLoader/INFO]: Execute Command: hello
HelloMod command executed: hello
[ModLoader/INFO]: Execute Command: h status
HelloMod command executed: hello status
```

调试窗口里的 `Last command` 也会更新。

输入：

```text
hello toggle
```

窗口会隐藏或显示。这个命令只是为了确认命令能改变脚本状态。

## 命令栏和 `/`

BML 命令栏用 `/` 打开。

`/` 只是打开命令栏的按键，不是命令名的一部分。打开命令栏后，输入的是：

```text
hello
```

不要把命令写成：

```text
/hello
```

## 常见检查项

日志里没有 `HelloMod command registered: hello`：

- `RegisterCommands(ctx)` 是否放进了 `OnLoad`；
- `def.Name` 是否是 `"hello"`；
- `ctx.RegisterCommand(...)` 的三个参数是否齐全；
- `OnHelloCommand` 签名是否是 `void OnHelloCommand(const BML::ModContext &in ctx, const BML::CommandEvent &in event)`。

命令能执行，但没有补全：

- 是否传入了 `complete`；
- `CompleteHelloCommand` 第三个参数是否是 `BML::CommandCompletion &inout completions`；
- 是否输入了 `hello `，命令名后要有空格。

提示命令名已存在：

- 换一个命令名；
- 或检查脚本是不是重复注册了同名命令。

不要把 `RegisterCommands(ctx)` 放到 `OnProcess`。`OnProcess` 每帧都会执行，会不断尝试注册同一个命令。

## 本章结果

现在脚本有了一个手动入口：

```text
hello
h
```

命令可以执行，也可以补全。下一章讲配置，把脚本自己的开关保存到配置文件里。

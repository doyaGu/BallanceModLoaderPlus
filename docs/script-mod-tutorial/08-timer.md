# 第 8 章：Timer

命令需要手动输入。

有些事情不适合手动触发，也不适合每帧都检查。例如：

```text
1 秒后做一次
每 2 秒做一次
做 3 次后停止
```

这类事情用 Timer。

Timer 的模型很简单：

```text
脚本启动 Timer
  BML 记住回调函数
    时间到了
      BML 调用回调函数
```

Timer 不是新线程。它只是让 BML 在未来某个时间点再调用脚本函数。

本章加两个 Timer：

```text
一次性 Timer    1 秒后自动执行 hello status
循环 Timer      每 2 秒写一次状态，写 3 次后停止
```

## Timer 和 `OnProcess`

`OnProcess` 每帧执行，适合处理每帧都要更新的事情：

- 读取这一帧按键；
- 计算 FPS；
- 绘制 ImGui 窗口。

Timer 按时间执行，适合：

- 延迟提示；
- 定时刷新状态；
- 低频写日志。

本章不会在 `OnProcess` 里自己数帧。时间到了没有，由 BML Timer 负责。

## 增加成员变量

继续修改：

```text
ModLoader/Mods/HelloMod.mod.as
```

在 `messagesEnabledProp` 后面加：

```angelscript
// 保存 Timer 引用，用来确认 Timer 是否有效。
private BML::TimerRef@ statusTimer;
private BML::TimerRef@ commandTimer;

// 循环 Timer 已经执行了几次。
private int timerTicks = 0;

// 显示在调试窗口里的 Timer 状态。
private string lastTimer = "not started";
```

`TimerRef` 和前面的 `CommandRef` 类似，都是 BML 返回的引用。它可以用来检查 Timer 是否有效，也可以取消 Timer。

## 在 `OnLoad` 里启动 Timer

把 `OnLoad` 改成这样：

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
    LoadConfig(ctx);
    RegisterCommands(ctx);
    StartTimers(ctx);

    LogInfo(ctx, "HelloMod loaded from ModLoader/Mods/HelloMod.mod.as messagesEnabled=" + BoolText(messagesEnabled));
    if (messagesEnabled) {
        ctx.SendIngameMessage("HelloMod loaded.");
    }
}
```

`StartTimers(ctx)` 放在 `RegisterCommands(ctx)` 后面。因为一次性 Timer 会自动执行 `hello status`，命令要先注册好。

## 启动两个 Timer

把下面函数加到 `HelloMod` 类里：

```angelscript
private void StartTimers(const BML::ModContext &in ctx) {
    // 一次性 Timer：1 秒后执行一次。
    BML::TimerCallback@ commandCallback = BML::TimerCallback(this.OnCommandTimer);
    @commandTimer = ctx.SetTimeout(1000.0f, commandCallback, "hello-command");

    // 循环 Timer：每 2 秒执行一次。
    BML::TimerLoopCallback@ statusCallback = BML::TimerLoopCallback(this.OnStatusTimer);
    @statusTimer = ctx.SetInterval(2000.0f, statusCallback, "hello-status");

    LogInfo(ctx, "HelloMod timers started: command=" + BoolText(commandTimer !is null && commandTimer.IsValid) +
                 " status=" + BoolText(statusTimer !is null && statusTimer.IsValid));
}
```

`SetTimeout` 只执行一次。

`SetInterval` 会循环执行。

这里的 `1000.0f` 和 `2000.0f` 单位都是毫秒：

```text
1000.0f  1 秒
2000.0f  2 秒
```

## 一次性 Timer 回调

继续加这个函数：

```angelscript
private void OnCommandTimer(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
    lastTimer = "timeout: " + event.Name;
    LogInfo(ctx, "HelloMod timer " + lastTimer);
    ctx.ExecuteCommand("hello status");
}
```

`TimerCallback` 对应的函数返回 `void`：

```angelscript
private void OnCommandTimer(const BML::ModContext &in ctx, const BML::TimerEvent &in event)
```

这个 Timer 到时间后会执行：

```angelscript
ctx.ExecuteCommand("hello status");
```

也就是让 BML 命令系统去执行上一章注册的命令。

## 循环 Timer 回调

再加循环回调：

```angelscript
private bool OnStatusTimer(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
    timerTicks++;
    lastTimer = "tick " + timerTicks + " from " + event.Name;
    LogInfo(ctx, "HelloMod timer " + lastTimer);

    return timerTicks < 3;
}
```

`TimerLoopCallback` 对应的函数返回 `bool`：

```angelscript
private bool OnStatusTimer(const BML::ModContext &in ctx, const BML::TimerEvent &in event)
```

返回值决定 Timer 是否继续：

```text
true   继续等下一次
false  停止
```

本章代码第 1 次、第 2 次返回 `true`，第 3 次返回 `false`。

所以日志里会有：

```text
HelloMod timer tick 1 from hello-status
HelloMod timer tick 2 from hello-status
HelloMod timer tick 3 from hello-status
```

第 3 次之后不再继续。

## 在窗口里显示 Timer 状态

在 `DrawWindow()` 里加一行：

```angelscript
ImGui::TextUnformatted("Timer: " + lastTimer);
```

放在 `Messages enabled` 后面：

```angelscript
if (ImGui::Begin("Hello")) {
    ImGui::TextUnformatted("HelloMod is running.");
    ImGui::TextUnformatted("FPS: " + fps);
    ImGui::TextUnformatted("Last command: " + lastCommand);
    ImGui::TextUnformatted("Messages enabled: " + BoolText(messagesEnabled));
    ImGui::TextUnformatted("Timer: " + lastTimer);
    ImGui::TextUnformatted("Press F9 to hide or show this window.");
}
ImGui::End();
```

## `TimerEvent` 先看哪些字段

回调里收到的 `event` 是 `BML::TimerEvent`。

本章只用：

```angelscript
event.Name
```

它来自启动 Timer 时传入的名字：

```angelscript
ctx.SetTimeout(1000.0f, commandCallback, "hello-command");
ctx.SetInterval(2000.0f, statusCallback, "hello-status");
```

API 里会看到 `get_Name() const`，脚本里通常写成 `.Name`。

后面需要调试 Timer 时，还可以看：

| 字段 | 用途 |
| --- | --- |
| `event.Id` | Timer 的运行时编号 |
| `event.State` | 当前状态 |
| `event.CompletedIterations` | 已完成次数 |
| `event.Progress` | 当前进度 |

第一篇先不展开这些字段。

## `TimerRef` 能做什么

`SetTimeout` 和 `SetInterval` 会返回 `BML::TimerRef@`：

```angelscript
@commandTimer = ctx.SetTimeout(1000.0f, commandCallback, "hello-command");
@statusTimer = ctx.SetInterval(2000.0f, statusCallback, "hello-status");
```

本章只检查它是否有效：

```angelscript
commandTimer !is null && commandTimer.IsValid
```

以后需要取消 Timer 时，可以调用：

```angelscript
if (statusTimer !is null && statusTimer.IsValid) {
    statusTimer.Cancel();
}
```

## 运行

保存脚本，重新启动 Player。

启动后日志里会先出现：

```text
[hello.script/INFO]: HelloMod command registered: hello
[hello.script/INFO]: HelloMod timers started: command=true status=true
[hello.script/INFO]: HelloMod loaded from ModLoader/Mods/HelloMod.mod.as messagesEnabled=true
```

约 1 秒后，一次性 Timer 会触发：

```text
[hello.script/INFO]: HelloMod timer timeout: hello-command
[ModLoader/INFO]: Execute Command: hello status
[hello.script/INFO]: HelloMod command executed: hello status
```

循环 Timer 会继续写 3 次：

```text
[hello.script/INFO]: HelloMod timer tick 1 from hello-status
[hello.script/INFO]: HelloMod timer tick 2 from hello-status
[hello.script/INFO]: HelloMod timer tick 3 from hello-status
```

窗口里的 `Timer` 行也会更新。

## 常见检查项

没有看到 `HelloMod timers started`：

- `StartTimers(ctx)` 是否放进了 `OnLoad`；
- 是否写在 `RegisterCommands(ctx)` 后面；
- `TimerCallback` / `TimerLoopCallback` 是否创建成功。

一次性 Timer 没执行：

- `OnCommandTimer` 签名是否返回 `void`；
- `ctx.SetTimeout(...)` 的时间是否大于 `0`；
- `ctx.ExecuteCommand("hello status")` 里的命令是否已经注册。

循环 Timer 没继续：

- `OnStatusTimer` 签名是否返回 `bool`；
- 回调最后是否返回了 `true`；
- 本章第 3 次返回 `false`，所以只执行 3 次。

日志里提示 Timer 回调签名错误时，直接对照这两个签名：

```angelscript
private void OnCommandTimer(const BML::ModContext &in ctx, const BML::TimerEvent &in event)
private bool OnStatusTimer(const BML::ModContext &in ctx, const BML::TimerEvent &in event)
```

## 本章结果

现在脚本有五种基础能力：

```text
OnLoad       初始化
OnProcess    每帧更新
Command      手动触发
Config       保存设置
Timer        延迟或低频触发
```

下一章整理第一篇常见错误和排查顺序。

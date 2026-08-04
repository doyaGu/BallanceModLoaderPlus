# 07 配置与 Timer

成员变量只活在这一轮运行里。关掉 Player 再打开，`messagesEnabled` 又回到代码里写死的默认值。如果你做了一个 mod 让玩家关闭某个提示，结果每次启动都要重新关一遍，这不是 mod 应有的体验。

配置系统把脚本的设置写到磁盘上的文件里，下次启动 BML 自动读回来。Timer 让 BML 在指定时间后调用脚本函数，不需要在 `OnProcess` 里自己数帧算时间。

本章把这两个能力合在一起：用配置保存一个开关，用 Timer 做延迟和定时动作。

---

## Part 1：配置

### 配置是什么

Ballance 玩家下载了你的 mod，运行一次后想调整某个行为，比如关掉某条反复出现的消息。如果他必须打开 `.as` 文件找到 `messagesEnabled = true` 改成 `false`，门槛太高了。

配置系统提供了更友好的出口：BML 为每个 mod 生成一个 `.cfg` 文件，内容是纯文本键值对。玩家用记事本就能编辑，不需要懂代码。

mod id 是 `hello.script`，对应的配置文件是：

```text
ModLoader/Configs/hello.script.cfg
```

第一次运行时 BML 自动创建这个文件并填入默认值。之后每次启动，BML 读取文件里的当前值交给脚本。

### 配置的生命周期

配置项的完整流程分三步：

1. **声明并设默认值**（`SetDefaultBoolean`）， 告诉 BML "这个配置项是布尔型，默认是 true"
2. **读取当前值**（`GetBoolean`）， BML 返回文件里保存的值，如果文件里没有就返回默认值
3. **运行时写回**（`SetBoolean`）， 脚本修改了配置，BML 写入文件，下次启动生效

理解这个流程后，下面的代码就不会感到陌生。

### 增加成员变量

```angelscript
private bool messagesEnabled = true;
private BML::ConfigProperty@ messagesEnabledProp;
```

`messagesEnabled` 是运行时用的开关。`messagesEnabledProp` 是配置项的句柄，写回时需要它。

### 读取配置

在 `OnLoad` 最前面读取配置：

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
    LoadConfig(ctx);
    RegisterCommands(ctx);
    StartTimers(ctx);

    LogInfo(ctx, "HelloMod loaded messagesEnabled=" + BoolText(messagesEnabled));
    if (messagesEnabled) {
        ctx.SendIngameMessage("HelloMod loaded.");
    }
}

private void LoadConfig(const BML::ModContext &in ctx) {
    BML::Config@ config = ctx.BorrowConfig();
    if (config is null) {
        LogWarn(ctx, "HelloMod config is not available");
        return;
    }

    config.SetCategoryComment("Hello", "HelloMod settings");

    @messagesEnabledProp = config.GetProperty("Hello", "MessagesEnabled");
    if (messagesEnabledProp is null) {
        return;
    }

    messagesEnabledProp.SetDefaultBoolean(true);
    messagesEnabledProp.SetComment("Show HelloMod in-game messages.");
    messagesEnabled = messagesEnabledProp.GetBoolean(true);
}
```

逐行解读：

- `ctx.BorrowConfig()` ， 借当前 mod 的配置对象。每个 mod 有自己独立的配置空间，不会互相干扰。
- `config.SetCategoryComment("Hello", "HelloMod settings")` ， 在配置文件里创建一个 `[Hello]` 分类，并写上注释。玩家打开 `.cfg` 文件时会看到这个注释，帮助理解这一段是什么。
- `config.GetProperty("Hello", "MessagesEnabled")` ， 在 `Hello` 分类下取名为 `MessagesEnabled` 的配置项。如果文件里还没有，BML 会新建。
- `messagesEnabledProp.SetDefaultBoolean(true)` ， 声明默认值。第一次运行时文件里会写入 `MessagesEnabled = true`。
- `messagesEnabledProp.GetBoolean(true)` ， 读取当前值。参数 `true` 是兜底默认值，当配置文件损坏或配置项不存在时返回它。

运行一次后，打开 `ModLoader/Configs/hello.script.cfg`，会看到类似：

```ini
[Hello]
; HelloMod settings
; Show HelloMod in-game messages.
MessagesEnabled = true
```

### 写回配置

当玩家用命令切换开关时，要同时修改两个地方：

```angelscript
private void ToggleMessages(const BML::ModContext &in ctx) {
    messagesEnabled = !messagesEnabled;

    if (messagesEnabledProp !is null && messagesEnabledProp.IsValid) {
        messagesEnabledProp.SetBoolean(messagesEnabled);
    }

    LogInfo(ctx, "HelloMod messages=" + BoolText(messagesEnabled));
    if (messagesEnabled) {
        ctx.SendIngameMessage("HelloMod messages=" + BoolText(messagesEnabled));
    }
}
```

为什么两边都要改？

- 只改 `messagesEnabled` 不调 `SetBoolean`：本次运行里逻辑正确，但重启后丢失修改。
- 只调 `SetBoolean` 不改 `messagesEnabled`：文件里的值变了，但本次运行里脚本还在用旧值，要到下次启动才生效。

两边同步修改，才能保证"改完立刻生效，下次启动也记住"。

### 命令里调用

在 `OnHelloCommand` 里增加 `messages` 分支：

```angelscript
private void OnHelloCommand(const BML::ModContext &in ctx,
                            const BML::CommandEvent &in event) {
    string action = "status";
    if (event.ArgCount > 0) {
        action = event.GetArg(0);
    }

    if (event.ArgsText == "") {
        lastCommand = event.CommandName;
    } else {
        lastCommand = event.CommandName + " " + event.ArgsText;
    }

    if (action == "messages") {
        ToggleMessages(ctx);
        return;
    }

    if (action == "toggle") {
        showWindow = !showWindow;
        if (messagesEnabled) {
            ctx.SendIngameMessage("HelloMod window toggled by command.");
        }
        return;
    }

    if (messagesEnabled) {
        ctx.SendIngameMessage("Command: " + lastCommand);
    }
}
```

补全也加上 `messages`：

```angelscript
private void CompleteHelloCommand(const BML::ModContext &in ctx,
                                  const BML::CommandEvent &in event,
                                  BML::CommandCompletion &inout completions) {
    completions.Add("status");
    completions.Add("toggle");
    completions.Add("messages");
}
```

### BoolText 辅助函数

```angelscript
private string BoolText(bool value) {
    return value ? "true" : "false";
}
```

AngelScript 不能直接把 `bool` 拼进字符串（`"enabled=" + true` 会报类型错误），所以需要这个辅助函数把布尔值转成 `"true"` 或 `"false"` 字符串。

---

## Part 2：Timer

### 为什么不在 OnProcess 里自己计时

假设你想在加载后 3 秒显示一条消息。一种做法是在 `OnProcess` 里用变量累加：

```text
if (elapsed >= 3.0) { show message; }
```

问题：

- 你需要自己记住开始时间
- 帧率波动时计算容易出错
- 多个定时任务交织在一起，代码变得难以维护

Timer 把"到时间再调用我"这件事交给 BML 管理。你只需要说"1000 毫秒后调用这个函数"，不用关心帧率和累加逻辑。

### Timer 的两种类型

| 类型 | 行为 | 用途举例 |
| --- | --- | --- |
| `SetTimeout` | 到时间执行一次，然后结束 | 延迟显示欢迎消息 |
| `SetInterval` | 每隔固定时间重复执行 | 定期刷新状态、循环检测 |

两者都不会创建新线程。BML 在游戏主循环里检查时间，到了就调用脚本函数，和 `OnProcess` 在同一个线程。

### 增加成员变量

```angelscript
private BML::TimerRef@ statusTimer;
private BML::TimerRef@ commandTimer;
private int timerTicks = 0;
private string lastTimer = "not started";
```

`TimerRef` 是 Timer 的句柄，可以用来取消尚未触发的 Timer。

### 启动 Timer

```angelscript
private void StartTimers(const BML::ModContext &in ctx) {
    // 一次性：1 秒后执行一次
    BML::TimerCallback@ commandCallback = BML::TimerCallback(this.OnCommandTimer);
    @commandTimer = ctx.SetTimeout(1000.0f, commandCallback, "hello-command");

    // 循环：每 2 秒执行一次
    BML::TimerLoopCallback@ statusCallback = BML::TimerLoopCallback(this.OnStatusTimer);
    @statusTimer = ctx.SetInterval(2000.0f, statusCallback, "hello-status");

    LogInfo(ctx, "HelloMod timers started");
}
```

参数说明：

- 第一个参数：时间，单位毫秒。`1000.0f` = 1 秒，`2000.0f` = 2 秒。
- 第二个参数：回调函数的委托。注意一次性用 `BML::TimerCallback`，循环用 `BML::TimerLoopCallback`，两者签名不同，不能混用。
- 第三个参数：名字字符串，出现在 `TimerEvent.Name` 里，用于调试日志区分哪个 Timer 触发了。

### 一次性 Timer 回调

```angelscript
private void OnCommandTimer(const BML::ModContext &in ctx,
                            const BML::TimerEvent &in event) {
    lastTimer = "timeout: " + event.Name;
    LogInfo(ctx, "HelloMod timer " + lastTimer);
    ctx.ExecuteCommand("hello status");
}
```

签名要求：返回 `void`，接受 `ModContext` 和 `TimerEvent`。`event.Name` 就是注册时传入的 `"hello-command"`。

执行完后 Timer 自动销毁，不需要手动清理。

### 循环 Timer 回调

```angelscript
private bool OnStatusTimer(const BML::ModContext &in ctx,
                           const BML::TimerEvent &in event) {
    timerTicks++;
    lastTimer = "tick " + timerTicks + " from " + event.Name;
    LogInfo(ctx, "HelloMod timer " + lastTimer);

    return timerTicks < 3;
}
```

和一次性回调的关键区别：**返回值是 `bool`**。

- 返回 `true`：继续，下一个周期还会调用
- 返回 `false`：停止，Timer 被销毁

本例在第 3 次调用时 `timerTicks < 3` 为 `false`，Timer 停止。这比在外部取消更自然，回调自己决定何时结束。

如果你想让循环永远执行，始终返回 `true` 即可。

### 取消 Timer

有时候需要在回调触发前取消 Timer。比如玩家离开了某个菜单，不需要定时刷新了：

```angelscript
if (statusTimer !is null && statusTimer.IsValid) {
    statusTimer.Cancel();
}
```

`IsValid` 检查 Timer 是否还活着（已触发完的一次性 Timer，或已返回 `false` 的循环 Timer，`IsValid` 会变成 `false`）。对一个已经结束的 Timer 调用 `Cancel()` 无害，但养成先判断的习惯更安全。

### 窗口里显示 Timer 状态

在 `DrawWindow()` 里加入 Timer 相关信息：

```angelscript
if (ImGui::Begin("Hello")) {
    ImGui::TextUnformatted("HelloMod is running.");
    ImGui::TextUnformatted("FPS: " + fps);
    ImGui::TextUnformatted("Last command: " + lastCommand);
    ImGui::TextUnformatted("Messages: " + BoolText(messagesEnabled));
    ImGui::TextUnformatted("Timer: " + lastTimer);
    ImGui::TextUnformatted("Press F9 to hide or show this window.");
}
ImGui::End();
```

启动后窗口里 `Timer` 一行会从 `"not started"` 变成 `"timeout: hello-command"`，然后变成 `"tick 1 from hello-status"`、`"tick 2 from hello-status"`、`"tick 3 from hello-status"`，之后不再变化（循环结束了）。

---

## 运行验证

启动 Player 后看日志，应该依次出现：

```text
[hello.script/INFO]: HelloMod timers started
[hello.script/INFO]: HelloMod loaded messagesEnabled=true
```

约 1 秒后一次性 Timer 触发：

```text
[hello.script/INFO]: HelloMod timer timeout: hello-command
```

之后循环 Timer 每 2 秒触发一次，共 3 次：

```text
[hello.script/INFO]: HelloMod timer tick 1 from hello-status
[hello.script/INFO]: HelloMod timer tick 2 from hello-status
[hello.script/INFO]: HelloMod timer tick 3 from hello-status
```

然后用命令栏测试配置：

1. 按 `/` 打开命令栏，输入 `hello messages`，回车
2. 日志出现 `HelloMod messages=false`
3. 关闭 Player，打开 `ModLoader/Configs/hello.script.cfg`
4. 确认 `MessagesEnabled = false`
5. 重新启动 Player，日志显示 `messagesEnabled=false`，设置被记住了

---

## 常见问题诊断

### 配置问题

| 现象 | 检查方向 |
| --- | --- |
| 没有生成 `.cfg` 文件 | 确认 `LoadConfig(ctx)` 在 `OnLoad` 里被调用了 |
| 命令改了但重启后丢失 | 确认 `SetBoolean` 被调用，检查是否漏写了那一行 |
| `config is null` | mod 文件名或 id 格式有误，BML 无法创建配置空间 |
| 配置值读出来总是默认值 | 确认 `SetDefaultBoolean` 在 `GetBoolean` 之前调用 |

### Timer 问题

| 现象 | 检查方向 |
| --- | --- |
| 没有任何 timer 日志 | 确认 `StartTimers(ctx)` 在 `OnLoad` 里被调用了 |
| 循环 Timer 只执行了一次 | 检查回调是否返回了 `true`。如果第一次就返回 `false`，后续不会执行 |
| 编译报签名不匹配 | 一次性回调必须返回 `void`，循环回调必须返回 `bool`。混用会导致委托绑定失败 |
| Timer 时间不准 | Timer 精度取决于帧率。60fps 下精度约 16ms，低帧率时会有偏差。不要用于需要精确计时的场景 |

---

## 完成状态

脚本现在有六种基础能力：

- `OnLoad` 做初始化
- `OnProcess` 每帧更新
- Command 做手动触发
- Config 保存持久设置
- Timer 做延迟和定时触发
- ImGui 显示调试面板

这些都是 BML 提供的服务。下一章将离开 BML 自身的 API，进入游戏世界，通过 CK 绑定查找 Ballance 的游戏对象。

下一步：[08 查找对象](tutorial-08-find-objects.md)

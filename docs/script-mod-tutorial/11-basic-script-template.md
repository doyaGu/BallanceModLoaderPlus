# 第 11 章：第一篇收束：基础脚本模板

到这里，BML 脚本 mod 的基础已经够写一个小工具。

下面这份模板只保留第一篇讲过的能力：

```text
加载时初始化
写日志
显示一个 ImGui 窗口
注册一个命令
读取一个配置项
启动一个 Timer
```

它不是后面所有章节的总容器。后面查对象、读 DataArray、看行为图时，可以单独写小脚本验证，也可以从这个模板里取几段使用。

## 文件位置

新建脚本文件：

```text
ModLoader/Mods/BaseMod.mod.as
```

如果继续沿用前几章的 `HelloMod.mod.as`，也可以只参考结构，不必改文件名。

## 模板代码

```angelscript
[bml.mod id="base.script" name="Base Script" version="1.0.0" author="Tutorial" bml="0.3.12" description="Basic script mod template"]
class BaseMod {
    // 当前运行状态。ImGui 窗口、命令和 Timer 都会读写这些值。
    private bool showWindow = true;
    private bool showMessage = true;
    private string lastCommand = "none";
    private string timerStatus = "not started";

    // BML 返回的引用。保存下来后，可以检查配置项、命令和 Timer 是否有效。
    private BML::ConfigProperty@ showMessageProp;
    private BML::CommandRef@ baseCommand;
    private BML::TimerRef@ startupTimer;

    // 固定回调：脚本加载后执行一次。
    void OnLoad(const BML::ModContext &in ctx) {
        // 启动阶段先准备脚本自己的功能入口。
        LoadConfig(ctx);
        RegisterCommands(ctx);
        StartTimers(ctx);

        LogInfo(ctx, "BaseMod loaded showMessage=" + BoolText(showMessage));
        if (showMessage) {
            ctx.SendIngameMessage("BaseMod loaded.");
        }
    }

    // 固定回调：每帧执行。这里保持轻量，只处理输入和窗口。
    void OnProcess(const BML::ModContext &in ctx) {
        HandleWindowToggle(ctx);
        DrawWindow();
    }

    private void LoadConfig(const BML::ModContext &in ctx) {
        BML::Config@ config = ctx.BorrowConfig();
        if (config is null) {
            LogWarn(ctx, "BaseMod config is not available");
            return;
        }

        config.SetCategoryComment("Base", "BaseMod settings");

        @showMessageProp = config.GetProperty("Base", "ShowMessage");
        if (showMessageProp is null) {
            LogWarn(ctx, "BaseMod ShowMessage config is not available");
            return;
        }

        showMessageProp.SetDefaultBoolean(true);
        showMessageProp.SetComment("Show in-game messages from BaseMod.");
        showMessage = showMessageProp.GetBoolean(true);
    }

    private void RegisterCommands(const BML::ModContext &in ctx) {
        // CommandDefinition 描述命令栏里能看到和输入的命令。
        BML::CommandDefinition def;
        def.Name = "base";
        def.Alias = "b";
        def.Description = "Show BaseMod status";
        def.Usage = "base [message]";
        def.Category = "Tutorial";
        def.Enabled = true;

        BML::CommandCallback@ execute = BML::CommandCallback(this.OnBaseCommand);
        BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompleteBaseCommand);

        @baseCommand = ctx.RegisterCommand(def, execute, complete);
        if (baseCommand is null || !baseCommand.IsValid) {
            LogWarn(ctx, "BaseMod command registration failed");
            return;
        }

        LogInfo(ctx, "BaseMod command registered: " + baseCommand.Name);
    }

    private void StartTimers(const BML::ModContext &in ctx) {
        // 1 秒后调用 OnStartupTimer，一次后结束。
        BML::TimerCallback@ callback = BML::TimerCallback(this.OnStartupTimer);
        @startupTimer = ctx.SetTimeout(1000.0f, callback, "base-startup");

        bool valid = startupTimer !is null && startupTimer.IsValid;
        LogInfo(ctx, "BaseMod startup timer valid=" + BoolText(valid));
    }

    void OnBaseCommand(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        // 记录完整输入，方便在窗口和日志里确认命令是否执行。
        lastCommand = event.CommandName;
        if (event.ArgsText != "") {
            lastCommand += " " + event.ArgsText;
        }

        LogInfo(ctx, "BaseMod command executed: " + lastCommand);

        if (event.ArgCount > 0 && event.GetArg(0) == "message") {
            ToggleMessage(ctx);
            return;
        }

        if (showMessage) {
            ctx.SendIngameMessage("BaseMod status: " + timerStatus);
        }
    }

    void CompleteBaseCommand(const BML::ModContext &in ctx,
                             const BML::CommandEvent &in event,
                             BML::CommandCompletion &inout completions) {
        // 命令栏补全候选词。
        completions.Add("message");
    }

    void OnStartupTimer(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
        // Timer 回调里更新状态，再按配置决定是否发游戏内消息。
        timerStatus = "done: " + event.Name;
        LogInfo(ctx, "BaseMod timer " + timerStatus);

        if (showMessage) {
            ctx.SendIngameMessage("BaseMod timer finished.");
        }
    }

    private void HandleWindowToggle(const BML::ModContext &in ctx) {
        BML::InputHook@ input = ctx.BorrowInputManager();
        if (input is null || !input.IsKeyPressed(CKKEY_F9)) {
            return;
        }

        showWindow = !showWindow;
        LogInfo(ctx, "BaseMod window visible=" + BoolText(showWindow));
    }

    private void DrawWindow() {
        if (!showWindow) {
            return;
        }

        ImGui::SetNextWindowPos(ImVec2(80.0f, 80.0f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Once);

        if (ImGui::Begin("Base Script")) {
            ImGui::TextUnformatted("BaseMod is running.");
            ImGui::TextUnformatted("Last command: " + lastCommand);
            ImGui::TextUnformatted("Show message: " + BoolText(showMessage));
            ImGui::TextUnformatted("Timer: " + timerStatus);
            ImGui::TextUnformatted("Press F9 to hide or show this window.");
        }
        ImGui::End();
    }

    private void ToggleMessage(const BML::ModContext &in ctx) {
        showMessage = !showMessage;

        if (showMessageProp !is null && showMessageProp.IsValid) {
            showMessageProp.SetBoolean(showMessage);
        }

        string message = "BaseMod showMessage=" + BoolText(showMessage);
        LogInfo(ctx, message);

        if (showMessage) {
            ctx.SendIngameMessage(message);
        }
    }

    private void LogInfo(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info(message);
        }
    }

    private void LogWarn(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Warn(message);
        }
    }

    private string BoolText(bool value) {
        return value ? "true" : "false";
    }
}
```

## 这份模板怎么读

先看 `OnLoad`：

```angelscript
LoadConfig(ctx);
RegisterCommands(ctx);
StartTimers(ctx);
```

这三行分别做启动阶段的三件事：

```text
读取配置
注册命令
启动 Timer
```

再看 `OnProcess`：

```angelscript
HandleWindowToggle(ctx);
DrawWindow();
```

这两行每帧执行。一个处理按键，一个绘制 ImGui 窗口。

命令和 Timer 不在 `OnProcess` 里手动检查。命令由 BML 命令系统触发，Timer 由 BML Timer 触发。

## 可以删掉哪些部分

只想写日志：

```text
保留 OnLoad
保留 LogInfo / LogWarn
删除 ImGui、命令、配置、Timer
```

只想写一个游戏内调试窗口：

```text
保留 OnLoad
保留 OnProcess
保留 DrawWindow
保留 LogInfo / LogWarn
删除命令、配置、Timer
```

只想做命令工具：

```text
保留 OnLoad
保留 RegisterCommands
保留 OnBaseCommand
保留 CompleteBaseCommand
保留 LogInfo / LogWarn
按需要保留配置
```

不要把后面章节的所有代码都堆进这个模板。功能变多以后，先按职责拆函数；再往后可以拆成多个脚本文件或脚本库。

## 改名时要一起改

复制模板后，通常要改这些地方：

| 位置 | 示例 |
| --- | --- |
| 文件名 | `BaseMod.mod.as` |
| mod id | `base.script` |
| mod name | `Base Script` |
| 类名 | `BaseMod` |
| 命令名 | `base` |
| 配置分类 | `Base` |
| 日志前缀 | `BaseMod ...` |

命令名和类名不是一回事。命令栏输入的是：

```text
base
b
```

不是：

```text
BaseMod
OnBaseCommand
```

## 运行后看什么

正常加载后，日志里应当能看到：

```text
[base.script/INFO]: BaseMod command registered: base
[base.script/INFO]: BaseMod startup timer valid=true
[base.script/INFO]: BaseMod loaded showMessage=true
```

约 1 秒后：

```text
[base.script/INFO]: BaseMod timer done: base-startup
```

命令栏输入：

```text
base
```

日志里会出现：

```text
[base.script/INFO]: BaseMod command executed: base
```

输入：

```text
base message
```

会切换配置项：

```text
[base.script/INFO]: BaseMod command executed: base message
[base.script/INFO]: BaseMod showMessage=false
```

## 第一篇到这里结束

第一篇只解决 BML 脚本 mod 的起步问题。

现在应该能看懂这些东西：

```text
[bml.mod ...]      BML 读取 mod 信息
class BaseMod      BML 创建脚本对象
OnLoad             启动初始化
OnProcess          每帧更新
ImGui              画调试窗口
Command            从命令栏触发脚本函数
Config             保存设置
Timer              延迟执行
Log                判断脚本是否真的跑到某一步
```

第二篇开始，用这个脚本外壳去观察 Ballance 的运行时世界。

# 第 7 章：配置

第 6 章已经能用命令改变脚本状态。

但成员变量只活在这一轮运行里。重启 Player 后，成员变量会回到代码里的默认值。

配置解决的是另一件事：把脚本自己的设置写到配置文件里，下次启动还能读回来。

本章保存一个布尔开关：

```text
MessagesEnabled
```

它控制脚本是否发送游戏内消息。日志仍然照常写，配置只影响 `ctx.SendIngameMessage(...)`。

配置保存的是脚本自己的偏好。它不保存关卡进度，也不保存游戏里的对象状态。

## 配置文件在哪里

本教程的 mod id 是：

```text
hello.script
```

BML 会给它生成配置文件：

```text
ModLoader/Configs/hello.script.cfg
```

配置文件属于脚本 mod 自己。它不修改关卡文件，也不写原版 DataArray。

## 增加成员变量

在 `HelloMod` 类开头，放到 `lastCommand` 后面：

```angelscript
// 是否发送游戏内消息。日志不受这个开关影响。
private bool messagesEnabled = true;

// 指向配置文件里的 MessagesEnabled 项。
private BML::ConfigProperty@ messagesEnabledProp;
```

这里有两层状态：

```text
messagesEnabled       当前运行中使用的值
messagesEnabledProp   配置文件里的配置项
```

脚本加载时，从配置项读到成员变量。  
命令修改开关时，先改成员变量，再写回配置项。

## 在 `OnLoad` 里读取配置

把 `OnLoad` 改成这样：

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
    LoadConfig(ctx);
    RegisterCommands(ctx);

    LogInfo(ctx, "HelloMod loaded from ModLoader/Mods/HelloMod.mod.as messagesEnabled=" + BoolText(messagesEnabled));
    if (messagesEnabled) {
        ctx.SendIngameMessage("HelloMod loaded.");
    }
}
```

`LoadConfig(ctx)` 放在最前面。后面的命令和消息会用到 `messagesEnabled`。

## 读取配置项

把下面函数加到 `HelloMod` 类里：

```angelscript
private void LoadConfig(const BML::ModContext &in ctx) {
    // BorrowConfig 取到的是当前 mod 自己的配置对象。
    BML::Config@ config = ctx.BorrowConfig();
    if (config is null) {
        LogWarn(ctx, "HelloMod config is not available");
        return;
    }

    // 分类注释会写进配置文件。
    config.SetCategoryComment("Hello", "HelloMod settings");

    // Hello 是分类，MessagesEnabled 是配置项。
    @messagesEnabledProp = config.GetProperty("Hello", "MessagesEnabled");
    if (messagesEnabledProp is null) {
        LogWarn(ctx, "HelloMod MessagesEnabled config is not available");
        return;
    }

    // 第一次生成配置文件时使用默认值。
    messagesEnabledProp.SetDefaultBoolean(true);
    messagesEnabledProp.SetComment("Show HelloMod in-game messages.");

    // 读取配置文件里的当前值。没有值时用 true。
    messagesEnabled = messagesEnabledProp.GetBoolean(true);
}
```

`BorrowConfig()` 和前面见过的 `BorrowLogger()` 一样，都是从 `ctx` 借用 BML 提供的服务。

`GetProperty("Hello", "MessagesEnabled")` 取得一个配置项：

```text
Hello             分类
MessagesEnabled   配置项
```

`SetDefaultBoolean(true)` 是默认值。第一次生成配置时，会使用这个值。

`GetBoolean(true)` 是读取当前值。配置项还不存在时，也会返回这里给的默认值。

## 写回配置项

再加一个切换函数：

```angelscript
private void ToggleMessages(const BML::ModContext &in ctx) {
    messagesEnabled = !messagesEnabled;

    if (messagesEnabledProp !is null && messagesEnabledProp.IsValid) {
        messagesEnabledProp.SetBoolean(messagesEnabled);
    }

    string message = "HelloMod messagesEnabled=" + BoolText(messagesEnabled);
    LogInfo(ctx, message);
    if (messagesEnabled) {
        ctx.SendIngameMessage(message);
    }
}
```

先改成员变量：

```angelscript
messagesEnabled = !messagesEnabled;
```

再写回配置文件：

```angelscript
messagesEnabledProp.SetBoolean(messagesEnabled);
```

如果 `messagesEnabledProp` 是空的，脚本仍然能继续运行，只是这次改动不能保存。

## 修改命令

第 6 章的 `hello toggle` 已经用来切换窗口。本章增加一个参数：

```text
hello messages
```

它用来切换 `MessagesEnabled`。

先改命令用法：

```angelscript
def.Usage = "hello [status|toggle|messages]";
```

再改补全函数：

```angelscript
void CompleteHelloCommand(const BML::ModContext &in ctx,
                          const BML::CommandEvent &in event,
                          BML::CommandCompletion &inout completions) {
    completions.Add("status");
    completions.Add("toggle");
    completions.Add("messages");
}
```

最后改 `OnHelloCommand`：

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

    if (action == "messages") {
        ToggleMessages(ctx);
        return;
    }

    if (action == "toggle") {
        showWindow = !showWindow;
        string message = "HelloMod window visible=" + BoolText(showWindow);
        LogInfo(ctx, message);
        if (messagesEnabled) {
            ctx.SendIngameMessage(message);
        }
        return;
    }

    if (messagesEnabled) {
        ctx.SendIngameMessage("Command: " + lastCommand);
    }
}
```

现在三个参数分工清楚：

```text
hello             输出命令状态
hello status      输出命令状态
hello toggle      切换调试窗口
hello messages    切换游戏内消息
```

日志始终会写。游戏内消息受 `messagesEnabled` 控制。

## 窗口里显示配置

在 `DrawWindow()` 里加一行：

```angelscript
ImGui::TextUnformatted("Messages enabled: " + BoolText(messagesEnabled));
```

放在 `Last command` 后面即可：

```angelscript
if (ImGui::Begin("Hello")) {
    ImGui::TextUnformatted("HelloMod is running.");
    ImGui::TextUnformatted("FPS: " + fps);
    ImGui::TextUnformatted("Last command: " + lastCommand);
    ImGui::TextUnformatted("Messages enabled: " + BoolText(messagesEnabled));
    ImGui::TextUnformatted("Press F9 to hide or show this window.");
}
ImGui::End();
```

## 运行

保存脚本，重新启动 Player。

启动后先看日志：

```text
[hello.script/INFO]: HelloMod command registered: hello
[hello.script/INFO]: HelloMod loaded from ModLoader/Mods/HelloMod.mod.as messagesEnabled=true
[hello.script/INFO]: HelloMod ImGui window first process
```

配置文件会生成在：

```text
ModLoader/Configs/hello.script.cfg
```

第一次生成时，内容大致如下：

```text
# Configuration File for Mod: Hello Mod - 1.0.0

# HelloMod settings
Hello {

    # Show HelloMod in-game messages.
    B MessagesEnabled 1

}
```

`B` 表示布尔值。  
`1` 表示 `true`。  
`0` 表示 `false`。

打开命令栏，输入：

```text
hello messages
```

日志里会出现：

```text
[ModLoader/INFO]: Execute Command: hello messages
[hello.script/INFO]: HelloMod command executed: hello messages
[hello.script/INFO]: HelloMod messagesEnabled=false
```

配置文件里的值会变成：

```text
B MessagesEnabled 0
```

再输入一次：

```text
hello messages
```

日志会变成：

```text
[ModLoader/INFO]: Execute Command: hello messages
[hello.script/INFO]: HelloMod command executed: hello messages
[hello.script/INFO]: HelloMod messagesEnabled=true
[BML/INFO]: HelloMod messagesEnabled=true
```

配置文件里的值会变回：

```text
B MessagesEnabled 1
```

## 配置和成员变量

这两个东西不要混在一起：

```text
配置文件     保存在磁盘上，重启后还在
成员变量     保存在脚本对象里，本次运行中使用
```

脚本加载时：

```text
配置文件 -> messagesEnabled
```

命令修改时：

```text
messagesEnabled -> 配置文件
```

只改成员变量，不写回配置项，下次启动就会丢失。  
只改配置项，不改成员变量，本次运行里的逻辑不会立刻跟着变。

## 常见检查项

没有生成 `hello.script.cfg`：

- `LoadConfig(ctx)` 是否放进了 `OnLoad`；
- `ctx.BorrowConfig()` 是否返回空；
- 有没有调用 `config.GetProperty("Hello", "MessagesEnabled")`；
- 脚本是否成功加载。

命令能执行，但配置没有变化：

- `ToggleMessages(ctx)` 是否调用了 `messagesEnabledProp.SetBoolean(messagesEnabled)`；
- `messagesEnabledProp` 是否为空；
- `messagesEnabledProp.IsValid` 是否为 `true`。

游戏内看不到 `HelloMod messagesEnabled=false`：

- 这是正常结果。切到 `false` 后，游戏内消息被关闭；
- 看日志确认状态变化。

## 本章结果

现在脚本有了自己的持久设置：

```text
MessagesEnabled
```

它会写入 `ModLoader/Configs/hello.script.cfg`，下次启动还能读回来。

下一章讲 Timer，用 BML 在指定时间后调用脚本函数。

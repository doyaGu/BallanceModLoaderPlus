# 05 命令系统

游戏运行时，你有时需要手动触发某个功能：检查当前变量值、切换某个开关、测试一段逻辑是否正常。如果每次都要改脚本再重启 Player，开发效率会很低。

BML 的命令栏（按 `/` 打开）提供了运行时入口。本章为 HelloMod 注册一个 `hello` 命令，让你在游戏中随时操作脚本状态。

---

## 命令能做什么

在实际开发中，命令常用于以下场景：

- **运行时调试**：输出当前变量值，确认脚本状态是否符合预期
- **测试功能**：不重启 Player 就能切换开关、修改参数
- **开发迭代**：每次调整完逻辑后直接在游戏内验证效果

本章的 `hello` 命令会演示两个操作：查看状态（status）和切换窗口显示（toggle）。

---

## 命令的三个组成部分

一个完整命令包含：

1. **定义（CommandDefinition）**：描述命令的名字、别名、说明、用法
2. **执行回调（CommandExecute）**：玩家回车后执行的逻辑
3. **补全回调（CommandComplete）**：输入过程中提供候选词

三者在 `OnLoad` 里组装并注册到 BML。

---

## 为什么在 OnLoad 注册，而不是 OnProcess

`OnLoad` 在脚本生命周期中只执行一次，适合做初始化工作。`OnProcess` 每帧执行一次（每秒几十到几百次）。如果把注册逻辑放进 `OnProcess`，BML 会每帧尝试注册同名命令，第二帧起就会因名字冲突而失败，日志会被错误信息淹没。

规则：注册类操作（命令、配置、计时器）放 `OnLoad`；持续性逻辑（检测按键、更新状态）放 `OnProcess`。

---

## 注册命令

在类中添加成员变量和注册函数：

```angelscript
private string lastCommand = "none";

void OnLoad(const BML::ModContext &in ctx) {
    RegisterCommands(ctx);
    ctx.LogInfo("HelloMod loaded from ModLoader/Mods/HelloMod.mod.as");
    BML::UI::AddMessage("HelloMod loaded.");
}

private void RegisterCommands(const BML::ModContext &in ctx) {
    BML::CommandDefinition def;
    def.Name = "hello";
    def.Alias = "h";
    def.Description = "Hello Mod commands";
    def.Usage = "hello [status|toggle]";
    def.Category = "Tutorial";
    def.Enabled = true;

    BML::CommandExecute@ execute = BML::CommandExecute(this.OnHelloCommand);
    BML::CommandComplete@ complete = BML::CommandComplete(this.CompleteHelloCommand);

    BML::CommandRef@ command;
    int code = ctx.Commands().Register(def, execute, command, complete);
    if (code != BML::ERROR_OK) {
        ctx.LogWarn("HelloMod command registration failed");
        return;
    }
    BML::CommandInfo info;
    if (command.Read(info) == BML::ERROR_OK)
        ctx.LogInfo("HelloMod command registered: " + info.Name);
}
```

### CommandDefinition 各字段含义

| 字段 | 作用 |
| --- | --- |
| `Name` | 命令主名称，玩家输入 `/hello` 时匹配这个字符串 |
| `Alias` | 短别名，`/h` 等价于 `/hello` |
| `Description` | 命令栏帮助列表中显示的说明文字 |
| `Usage` | 用法提示，告诉玩家有哪些参数可用 |
| `Category` | 分类标签，方便命令列表按类别归组 |
| `Enabled` | 是否启用；设为 `false` 则注册后不响应输入 |

### 理解 funcdef 与回调

```angelscript
BML::CommandExecute@ execute = BML::CommandExecute(this.OnHelloCommand);
```

这一行涉及 AngelScript 的 **funcdef（函数类型定义）** 概念。`BML::CommandExecute` 是 BML 预定义的函数类型，它规定了"执行回调必须长什么样"（参数列表和返回值）。

`@` 表示这是一个函数句柄。注册命令时，脚本把 `this.OnHelloCommand` 交给 BML；玩家执行命令时，BML 再调用这个函数。

补全回调 `BML::CommandComplete` 同理，只是参数列表多了一个 `completions`，用于写入候选词。

### CommandRef 返回值

Register 返回错误码，CommandRef 通过 out 接收。只有 code == BML::ERROR_OK 才成功；
失败 out 为 null。后续使用 IsValid() 判活，Read(CommandInfo &out) 读取拥有数据的元信息。
丢弃脚本 Ref 不注销，显式 Unregister 或由运行期退休清理。

---

## 执行回调

```angelscript
private int OnHelloCommand(const BML::ModContext &in ctx,
                           const BML::CommandInvocation &in call) {
    string action = "status";
    if (call.ArgumentCount > 0) call.ReadArgument(0, action);
    lastCommand = call.InvokedAs;
    for (uint i = 0; i < call.ArgumentCount; ++i) {
        string argument;
        if (call.ReadArgument(i, argument) != BML::ERROR_OK) return 1;
        lastCommand += " " + argument;
    }
    if (action == "toggle") showWindow = !showWindow;
    BML::UI::AddMessage("Command: " + lastCommand);
    return 0;
}
```

### CommandInvocation 字段

Name 是注册的规范名，InvokedAs 是输入的名称或别名。ArgumentCount 与 ReadArgument
使用不含命令名的零基参数。Input/HasInput() 区分管道输入与空输入，Write 输出到当前
管道。回调返回非负业务状态，0 通常表示成功；API 调用的错误码另行检查。保存副本
不能延长 sink 或请求的回调寿命。OnCommandEvent 仍是独立的前后通知协议。

---

## 补全回调

```angelscript
private int CompleteHelloCommand(const BML::ModContext &in ctx,
                                 const BML::CommandCompletionRequest &in request,
                                 const BML::CommandCompletion &in completions) {
    int code = completions.Add("status");
    return code == BML::ERROR_OK ? completions.Add("toggle") : code;
}
```

### 补全触发时机

当玩家在命令栏中输入 `hello `（命令名后跟一个空格）时，BML 判断玩家开始输入参数，此时向脚本请求补全。脚本通过 `completions.Add(...)` 添加候选词，命令栏会以列表形式展示 `status` 和 `toggle` 供玩家选择。

如果玩家已经输入了部分文字（如 `hello t`），BML 会自动过滤候选词，只显示以 `t` 开头的选项（即 `toggle`）。

---

## 在窗口中显示命令

在上一章的 `DrawWindow()` 里加一行，让窗口实时反映最近执行的命令：

```angelscript
ImGui::TextUnformatted("Last command: " + lastCommand);
```

每次执行命令后，`lastCommand` 被更新，窗口内容随之变化。

---

## 运行验证

启动 Player 后检查日志：

```text
[hello.script/INFO]: HelloMod command registered: hello
```

看到这行说明注册成功。然后在游戏内：

1. 按 `/` 打开命令栏
2. 输入 `hello` 回车：窗口中 `Last command` 显示 `hello`
3. 输入 `hello toggle` 回车：窗口切换显示/隐藏
4. 输入 `h toggle` 回车：别名同样生效

---

## 故障诊断

| 现象 | 排查方向 |
| --- | --- |
| 日志中没有注册成功的信息 | 确认 `RegisterCommands(ctx)` 在 `OnLoad` 中被调用；检查是否有编译错误导致脚本未加载 |
| 日志显示 "registration failed" | `command is null` 或 `!command.IsValid()` 为真。检查是否有其他 Mod 已注册同名命令 |
| 命令栏输入后无反应 | 确认输入格式正确（`hello` 而非 `/hello`，命令栏已自动添加 `/`）；确认 `def.Enabled = true` |
| 没有补全候选词出现 | 确认命令名后输入了空格；确认 `complete` 句柄传给了 `RegisterCommand` 的第三个参数 |
| 补全列表为空但不报错 | 确认 `CompleteHelloCommand` 内部调用了 `completions.Add(...)`；检查函数签名是否匹配 funcdef |
| toggle 无效果但无报错 | 确认 `showWindow` 变量在类中声明为 `private bool showWindow = true`（上一章内容） |

---

## 完成状态

脚本现在拥有运行时手动入口 `/hello`（别名 `/h`），支持参数解析和自动补全。你可以在游戏过程中随时检查状态或切换功能，无需重启 Player。

命令系统让脚本从"加载后自动运行"变成了"可以交互"。下一章会介绍 BML 的回调模型，脚本如何在游戏的各个时机（进入关卡、暂停、退出）获得执行机会。

下一步：[06 回调模型](tutorial-06-callbacks.md)

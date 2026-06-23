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
2. **执行回调（CommandCallback）**：玩家回车后执行的逻辑
3. **补全回调（CommandCompletionCallback）**：输入过程中提供候选词

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
    LogInfo(ctx, "HelloMod loaded from ModLoader/Mods/HelloMod.mod.as");
    ctx.SendIngameMessage("HelloMod loaded.");
}

private void RegisterCommands(const BML::ModContext &in ctx) {
    BML::CommandDefinition def;
    def.Name = "hello";
    def.Alias = "h";
    def.Description = "Hello Mod commands";
    def.Usage = "hello [status|toggle]";
    def.Category = "Tutorial";
    def.Enabled = true;

    BML::CommandCallback@ execute = BML::CommandCallback(this.OnHelloCommand);
    BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompleteHelloCommand);

    BML::CommandRef@ command = ctx.RegisterCommand(def, execute, complete);
    if (command is null || !command.IsValid) {
        LogWarn(ctx, "HelloMod command registration failed");
        return;
    }
    LogInfo(ctx, "HelloMod command registered: " + command.Name);
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
BML::CommandCallback@ execute = BML::CommandCallback(this.OnHelloCommand);
```

这一行涉及 AngelScript 的 **funcdef（函数类型定义）** 概念。`BML::CommandCallback` 是 BML 预定义的函数类型，它规定了"执行回调必须长什么样"（参数列表和返回值）。

`@` 表示这是一个函数句柄。注册命令时，脚本把 `this.OnHelloCommand` 交给 BML；玩家执行命令时，BML 再调用这个函数。

补全回调 `BML::CommandCompletionCallback` 同理，只是参数列表多了一个 `completions`，用于写入候选词。

### CommandRef 返回值

`ctx.RegisterCommand(...)` 返回 `BML::CommandRef@`，它是注册结果的句柄。检查方式：

- `command is null`：注册过程出现内部错误
- `!command.IsValid`：注册被拒绝（例如命令名已被占用）

两种情况都意味着命令不可用，应记录警告并提前返回。如果注册成功，`command.Name` 会返回注册后的命令名（此处为 `"hello"`）。

---

## 执行回调

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

    if (action == "toggle") {
        showWindow = !showWindow;
        ctx.SendIngameMessage("HelloMod window toggled by command.");
        return;
    }

    ctx.SendIngameMessage("Command: " + lastCommand);
}
```

### CommandEvent 字段

| 写法 | 含义 |
| --- | --- |
| `event.CommandName` | 注册命令名 |
| `event.ArgCount` | 参数数量 |
| `event.GetArg(0)` | 第一个参数 |
| `event.ArgsText` | 所有参数原文 |

注意：即使玩家用别名 `/h toggle` 触发，`event.CommandName` 仍然是 `"hello"`（注册时的主名称）。

### 逐步追踪：玩家输入 `/hello toggle` 时发生了什么

1. 玩家按 `/` 打开命令栏，输入 `hello toggle`，按回车
2. BML 解析输入，将第一个空格前的部分 `hello` 作为命令名查找
3. BML 在注册表中找到名为 `hello` 的条目
4. BML 构造 `CommandEvent` 对象：`CommandName = "hello"`，`ArgCount = 1`，`GetArg(0) = "toggle"`，`ArgsText = "toggle"`
5. BML 通过之前保存的 `execute` 句柄调用 `OnHelloCommand`
6. 回调内部读取 `action = "toggle"`，执行 `showWindow = !showWindow`
7. 屏幕上出现提示信息 "HelloMod window toggled by command."

---

## 补全回调

```angelscript
private void CompleteHelloCommand(const BML::ModContext &in ctx,
                                  const BML::CommandEvent &in event,
                                  BML::CommandCompletion &inout completions) {
    completions.Add("status");
    completions.Add("toggle");
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
| 日志显示 "registration failed" | `command is null` 或 `!command.IsValid` 为真。检查是否有其他 Mod 已注册同名命令 |
| 命令栏输入后无反应 | 确认输入格式正确（`hello` 而非 `/hello`，命令栏已自动添加 `/`）；确认 `def.Enabled = true` |
| 没有补全候选词出现 | 确认命令名后输入了空格；确认 `complete` 句柄传给了 `RegisterCommand` 的第三个参数 |
| 补全列表为空但不报错 | 确认 `CompleteHelloCommand` 内部调用了 `completions.Add(...)`；检查函数签名是否匹配 funcdef |
| toggle 无效果但无报错 | 确认 `showWindow` 变量在类中声明为 `private bool showWindow = true`（上一章内容） |

---

## 完成状态

脚本现在拥有运行时手动入口 `/hello`（别名 `/h`），支持参数解析和自动补全。你可以在游戏过程中随时检查状态或切换功能，无需重启 Player。

命令系统让脚本从"加载后自动运行"变成了"可以交互"。下一章会介绍 BML 的回调模型，脚本如何在游戏的各个时机（进入关卡、暂停、退出）获得执行机会。

下一步：[06 回调模型](tutorial-06-callbacks.md)

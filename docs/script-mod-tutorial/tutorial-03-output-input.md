# 03 输出与输入

上一节脚本只写了日志。日志要打开文件才能看到，玩家在游戏画面里看不到任何变化。本节做两件事：在游戏画面里显示一条消息，以及读取键盘按键让脚本对玩家操作做出反应。

做完以后，能看到两个现象：游戏加载时画面上出现一条文字消息，按下 F9 时画面上再出现一条消息。

## 两种输出位置

脚本有两种途径把信息展示给用户：

| 方式 | 输出位置 | 什么时候看到 |
| --- | --- | --- |
| `logger.Info(...)` | `ModLoader/ModLoader.log` 文件 | 打开日志文件时 |
| `ctx.SendIngameMessage(...)` | 游戏画面左下方的 BML 短消息区域 | 消息发出的瞬间，在画面里 |

日志适合记录"之后回头查"的信息，比如初始化步骤、错误原因、调试数据。它不会打扰游戏画面，也不会因为消息太快而看不清。

游戏内消息适合"马上让玩家看到"的反馈，比如"mod 已加载"、"按键触发了"、"配置已切换"。它显示几秒后会自动消失。

实际开发中，经常两边都写。日志里记详细信息方便排查，游戏内消息给一个简短的反馈让玩家知道发生了什么。

## 在 OnLoad 里发送游戏内消息

把 `HelloMod.mod.as` 的 `OnLoad` 改成：

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
    ctx.SendIngameMessage("HelloMod loaded.");

    BML::Logger@ logger = ctx.BorrowLogger();
    if (logger !is null) {
        logger.Info("HelloMod loaded from ModLoader/Mods/HelloMod.mod.as");
    }
}
```

重启游戏后，游戏画面上会出现 `HelloMod loaded.` 的消息。同时日志文件里也有对应的记录。

`ctx.SendIngameMessage(...)` 接收一个字符串参数，直接显示在画面里。不需要获取句柄，不需要 null 检查，调用一次就发送一条消息。

## Logger helper 模式

上一节用到 Logger 时，每次都要写 `BorrowLogger`、null 检查、再调用方法。代码量虽然不多，但如果每个回调里都重复写，会让代码变得冗长。

一种常见的做法是把日志调用封装成类的私有方法：

```angelscript
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
```

把这两个方法放在 `HelloMod` 类里面。`private` 表示它们只能在类内部使用，外部看不到。

之后 `OnLoad` 可以写成：

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
    ctx.SendIngameMessage("HelloMod loaded.");
    LogInfo(ctx, "HelloMod loaded from ModLoader/Mods/HelloMod.mod.as");
}
```

这样的好处是：null 检查只写一次，以后所有地方用 `LogInfo` / `LogWarn` 就行。如果将来需要统一给日志加前缀或者改格式，只改 helper 方法就够了。

## 键盘输入：OnProcess 回调

到目前为止，脚本只在游戏启动时执行一次（`OnLoad`）。要响应键盘，需要每帧检查按键状态。BML 提供了 `OnProcess` 回调，每一帧调用一次。

在 `HelloMod` 类里加一个新方法：

```angelscript
void OnProcess(const BML::ModContext &in ctx) {
    BML::InputHook@ input = ctx.BorrowInputManager();
    if (input is null) {
        return;
    }

    if (input.IsKeyPressed(CKKEY_F9)) {
        ctx.SendIngameMessage("F9 pressed!");
        LogInfo(ctx, "F9 pressed");
    }
}
```

重启游戏后按 F9，画面上出现 `F9 pressed!`。

### 轮询模型

BML 的输入系统是轮询（polling）模型，不是事件（event）模型。这个区别很重要：

- **事件模型**：按下按键时，系统主动通知你"F9 被按下了"。你写一个处理函数等着被调用。
- **轮询模型**：你每帧主动去问"F9 现在是什么状态？"。如果是按下的状态，你自己决定做什么。

BML 用的是轮询模型。所以检查按键的代码必须放在 `OnProcess` 里，因为它每帧都执行。如果放在 `OnLoad` 里只执行一次，那一次检查之后就再也不检查了。

### BorrowInputManager

`ctx.BorrowInputManager()` 返回一个 `BML::InputHook@` 句柄。和 Logger 一样，用 `@` 句柄并检查是否为空。InputHook 提供了检查键盘和鼠标状态的方法。

## IsKeyDown 和 IsKeyPressed 的区别

InputHook 提供了两个检查按键的方法，它们的行为不同：

| 方法 | 含义 | 按住 F9 不放时的行为 |
| --- | --- | --- |
| `IsKeyPressed(key)` | 按键刚按下的那一帧 | 只在第一帧返回 true，后续帧全部返回 false |
| `IsKeyDown(key)` | 按键当前是否处于按下状态 | 每一帧都返回 true，直到松开 |

用一个具体的场景来理解：假设你按住 F9 不放，持续了 60 帧（大约 1 秒），然后松开。

```text
帧:  1    2    3    4    5   ...  60   61
     v按下                          ^松开
IsKeyPressed:  true  false false false false ... false false
IsKeyDown:     true  true  true  true  true  ... true  false
```

### 什么时候用哪个

**IsKeyPressed** 适合"执行一次"的动作：

- 按 F9 打开/关闭一个窗口
- 按 F9 执行一条命令
- 按 F9 切换一个开关

如果用 `IsKeyDown` 做这些事情，玩家按住稍微久一点，动作就会重复执行很多次（窗口反复开关、命令重复触发）。

**IsKeyDown** 适合"持续进行"的动作：

- 按住 W 向前移动
- 按住空格施加力
- 按住某个键让数值持续增长

如果用 `IsKeyPressed` 做这些事情，玩家按住按键时只有第一帧有效果，后续帧没有反应。

### 实验

把代码改成 `IsKeyDown`：

```angelscript
if (input.IsKeyDown(CKKEY_F9)) {
    ctx.SendIngameMessage("F9 is held!");
}
```

重启后按住 F9 不放，会看到消息疯狂刷屏。这就是 `IsKeyDown` 在每帧触发的效果。改回 `IsKeyPressed` 后，同样按住 F9，消息只出现一次。

## 按键常量

BML 使用 Virtools 的键盘常量命名。常用的几个：

```text
CKKEY_F1 ~ CKKEY_F12     功能键
CKKEY_A ~ CKKEY_Z        字母键
CKKEY_0 ~ CKKEY_9        数字键（主键盘）
CKKEY_SPACE              空格
CKKEY_RETURN             回车
CKKEY_ESCAPE             Esc
CKKEY_LSHIFT             左 Shift
CKKEY_LCONTROL           左 Ctrl
```

这些常量定义在 `as.predefined` 里，编辑器能识别和补全。如果输入 `CKKEY_` 后没有补全提示，检查 `as.predefined` 文件是否在工作区里。

## 当前完整代码

把本节的内容整合起来，`HelloMod.mod.as` 现在应该是：

```angelscript
[bml.mod id="hello.script" name="Hello Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="Minimal tutorial script mod"]
class HelloMod {
    void OnLoad(const BML::ModContext &in ctx) {
        ctx.SendIngameMessage("HelloMod loaded.");
        LogInfo(ctx, "HelloMod loaded from ModLoader/Mods/HelloMod.mod.as");
    }

    void OnProcess(const BML::ModContext &in ctx) {
        BML::InputHook@ input = ctx.BorrowInputManager();
        if (input is null) {
            return;
        }

        if (input.IsKeyPressed(CKKEY_F9)) {
            ctx.SendIngameMessage("F9 pressed!");
            LogInfo(ctx, "F9 pressed");
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
}
```

## 排查

| 现象 | 检查 |
| --- | --- |
| 游戏画面里没有消息 | `OnLoad` 有没有执行（看日志）；是否忘记重启游戏 |
| 按 F9 没反应 | `OnProcess` 是否在 class 内部；方法签名是否正确 |
| 一按键消息刷很多条 | 是否错用了 `IsKeyDown`，应该换成 `IsKeyPressed` |
| 编译提示找不到 `CKKEY_F9` | `as.predefined` 是否在 VS Code 工作区里（编辑器报错）；实际运行中这个常量由 BML 注入，不会找不到 |
| 编译提示 `BorrowInputManager` 不存在 | 检查 `as.predefined` 版本是否和 BML 版本匹配 |

## 完成状态

脚本能在游戏画面显示消息（`SendIngameMessage`），能写日志（`Logger.Info`），并能响应键盘按键（`IsKeyPressed`）。已经掌握了两种输出方式的区别和两种按键检测的区别。

-> 下一节：[04 ImGui 窗口](tutorial-04-imgui-window.md)

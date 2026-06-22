# 第 5 章：让脚本在游戏里可见

前面几章只能从 `ModLoader.log` 确认脚本有没有运行。

日志适合排错，但每次都切出去看日志很麻烦。本章做一个游戏画面里的调试窗口，让脚本状态直接显示出来。

本章完成三件事：

- 显示一个 ImGui 窗口；
- 在窗口里显示 FPS；
- 按 `F9` 隐藏或显示窗口。

本章仍然站在 BML 层。ImGui 窗口只是脚本画出来的调试面板，不是关卡对象，也不会保存进关卡。

## ImGui 是什么

ImGui 可以先理解成“每帧画出来的调试面板”。

它不会创建 Ballance 关卡对象，也不会写入关卡文件。窗口内容来自脚本当前这一帧执行的代码。

流程是这样：

```text
OnProcess 每帧执行
  脚本更新状态
  脚本调用 ImGui 函数描述窗口
  BMLPlus 把窗口显示到游戏画面上
```

所以，窗口里要显示什么，就先把状态存在成员变量里，再在每帧把这些状态画出来。

本章只用三个 ImGui 概念：

| 函数 | 用途 |
| --- | --- |
| `SetNextWindowPos` | 设置窗口第一次出现的位置 |
| `Begin` / `End` | 开始和结束一个窗口 |
| `TextUnformatted` | 显示一行文字 |

## 修改脚本

打开：

```text
ModLoader/Mods/HelloMod.mod.as
```

把脚本改成下面这样。

```angelscript
[bml.mod id="hello.script" name="Hello Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="Minimal tutorial script mod"]
class HelloMod {
    // 只让 OnProcess 的确认日志写一次。
    private bool firstProcess = true;

    // 调试窗口当前是否显示。按 F9 会切换这个值。
    private bool showWindow = true;

    // 当前显示在窗口里的 FPS。
    private float fps = 0.0f;

    void OnLoad(const BML::ModContext &in ctx) {
        LogInfo(ctx, "HelloMod loaded from ModLoader/Mods/HelloMod.mod.as");
        ctx.SendIngameMessage("HelloMod loaded.");
    }

    void OnProcess(const BML::ModContext &in ctx) {
        HandleWindowToggle(ctx);
        UpdateFps(ctx);
        LogFirstProcess(ctx);
        DrawWindow();
    }

    private void HandleWindowToggle(const BML::ModContext &in ctx) {
        // F9 是本章调试窗口的开关。
        BML::InputHook@ input = ctx.BorrowInputManager();
        if (input is null || !input.IsKeyPressed(CKKEY_F9)) {
            return;
        }

        showWindow = !showWindow;

        string message = "HelloMod window visible=" + BoolText(showWindow);
        LogInfo(ctx, message);
        ctx.SendIngameMessage(message);
    }

    private void UpdateFps(const BML::ModContext &in ctx) {
        // ImGui::GetIO().DeltaTime 返回上一帧耗时，单位是秒。
        ImGuiIO@ io = ImGui::GetIO();
        if (io !is null && io.DeltaTime > 0.0f) {
            fps = 1.0f / io.DeltaTime;
        }
    }

    private void LogFirstProcess(const BML::ModContext &in ctx) {
        if (!firstProcess) {
            return;
        }

        firstProcess = false;
        LogInfo(ctx, "HelloMod ImGui window first process");
    }

    private void DrawWindow() {
        if (!showWindow) {
            return;
        }

        // ImGui 窗口每帧重新提交一次。
        ImGui::SetNextWindowPos(ImVec2(80.0f, 80.0f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Once);
        if (ImGui::Begin("Hello")) {
            ImGui::TextUnformatted("HelloMod is running.");
            ImGui::TextUnformatted("FPS: " + fps);
            ImGui::TextUnformatted("Press F9 to hide or show this window.");
        }
        ImGui::End();
    }

    private void LogInfo(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info(message);
        }
    }

    private string BoolText(bool value) {
        return value ? "true" : "false";
    }
}
```

## 运行结果

保存脚本，重新启动 Player。

进入游戏后，画面左上方会出现一个标题为 `Hello` 的窗口：

```text
HelloMod is running.
FPS: ...
Press F9 to hide or show this window.
```

按 `F9`，窗口隐藏。再按一次 `F9`，窗口重新显示。

本章测试时使用的是相对位置：

```text
<Ballance 游戏目录>\ModLoader\Mods\HelloMod.mod.as
```

测试日志中出现了这些行：

```text
[ModLoader/INFO]: Loading Mod hello.script[Hello Mod] v1.0.0 by Tutorial
[hello.script/INFO]: HelloMod loaded from ModLoader/Mods/HelloMod.mod.as
[BML/INFO]: HelloMod loaded.
[ModLoader/INFO]: BML script mod summary: loaded=1 failed=0
[hello.script/INFO]: HelloMod ImGui window first process
[hello.script/INFO]: HelloMod window visible=false
[BML/INFO]: HelloMod window visible=false
[hello.script/INFO]: HelloMod window visible=true
[BML/INFO]: HelloMod window visible=true
```

看到 `HelloMod ImGui window first process`，说明 `OnProcess` 已经开始执行。

看到 `visible=false` 和 `visible=true`，说明 `F9` 输入被脚本收到了。

## `OnProcess` 现在做什么

第 4 章讲过，`OnProcess` 会反复执行。本章把每帧逻辑拆成四步：

```angelscript
void OnProcess(const BML::ModContext &in ctx) {
    HandleWindowToggle(ctx);
    UpdateFps(ctx);
    LogFirstProcess(ctx);
    DrawWindow();
}
```

按顺序读：

```text
HandleWindowToggle   检查 F9
UpdateFps            更新 FPS
LogFirstProcess      第一次进入 OnProcess 时写日志
DrawWindow           绘制调试窗口
```

函数可以写在 `OnProcess` 后面。AngelScript 会先编译整个类，再运行回调。

## 成员变量保存状态

类开头有三个成员变量：

```angelscript
private bool firstProcess = true;
private bool showWindow = true;
private float fps = 0.0f;
```

它们写在函数外面，属于 `HelloMod` 对象。

`OnProcess` 每帧都会重新进入，但这些成员变量不会每帧重新创建。

所以跨帧保存状态时，放成员变量里：

```text
firstProcess   是否已经写过第一帧日志
showWindow     窗口当前是否显示
fps            当前显示的 FPS
```

## FPS 怎么算

这几行读取上一帧耗时：

```angelscript
ImGuiIO@ io = ImGui::GetIO();
float deltaSeconds = io.DeltaTime;
```

`DeltaTime` 的单位是秒。FPS 的意思是一秒能跑多少帧，所以用 `1` 除以上一帧耗时：

```angelscript
fps = 1.0f / deltaSeconds;
```

例子：

```text
16.67 毫秒一帧  大约 60 FPS
10 毫秒一帧     大约 100 FPS
33.33 毫秒一帧  大约 30 FPS
```

代码先判断 `deltaMs > 0.0f`，避免除以 0。

## 窗口怎么画

窗口从 `Begin` 开始，到 `End` 结束：

```angelscript
if (ImGui::Begin("Hello")) {
    ImGui::TextUnformatted("HelloMod is running.");
    ImGui::TextUnformatted("FPS: " + fps);
}
ImGui::End();
```

`Begin("Hello")` 里的 `Hello` 是窗口标题。

`TextUnformatted(...)` 显示一行文字。本章用它显示固定文字和 FPS。

`End()` 要和 `Begin(...)` 配对。代码里即使窗口被折叠，也会执行到 `End()`。

窗口第一次出现的位置由这行控制：

```angelscript
ImGui::SetNextWindowPos(ImVec2(80.0f, 80.0f), ImGuiCond_Once);
```

`80.0f, 80.0f` 是屏幕上的初始坐标。`ImGuiCond_Once` 表示只设置第一次。之后拖动窗口，脚本不会每帧把窗口拉回去。

## F9 怎么读取

读取按键从输入管理器开始：

```angelscript
BML::InputHook@ input = ctx.BorrowInputManager();
```

可能拿不到输入管理器，所以先判断：

```angelscript
if (input is null || !input.IsKeyPressed(CKKEY_F9)) {
    return;
}
```

这句话的意思是：

```text
没有输入管理器，结束函数
这一帧没有按下 F9，结束函数
```

能继续往下走，就说明这一帧按下了 `F9`。

窗口开关只需要一行：

```angelscript
showWindow = !showWindow;
```

`!` 会把布尔值反过来：

```text
true  变成 false
false 变成 true
```

所以每按一次 `F9`，`showWindow` 就切换一次。

## 常见检查项

没有看到窗口时，先看 `ModLoader.log`。

如果没有这行：

```text
HelloMod ImGui window first process
```

说明脚本还没有跑到 `OnProcess`。检查：

- 文件名是不是 `HelloMod.mod.as`；
- 文件是不是放在 `ModLoader/Mods`；
- `OnProcess` 签名是不是 `void OnProcess(const BML::ModContext &in ctx)`；
- 第 3 章的加载日志有没有出现。

如果编译失败并提示找不到 `ImGui::Text`，改用本章的 `ImGui::TextUnformatted`。

如果窗口出现过，后来不见了，先按一次 `F9`。

## 本章结果

现在脚本已经能在游戏里显示状态：

```text
OnLoad      写加载日志，发一条游戏内消息
OnProcess   检查 F9，更新 FPS，绘制窗口
ImGui       显示调试窗口
InputHook   读取按键
```

下一章讲命令。命令可以让脚本在运行时执行一个动作，不需要每次都改代码、重启 Player。

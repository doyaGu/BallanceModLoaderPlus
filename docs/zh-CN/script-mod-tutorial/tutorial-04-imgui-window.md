# 04 ImGui 窗口和 FPS

前面已经能用日志和游戏内消息确认脚本运行。这一章在游戏画面里显示一个调试窗口：先显示一行文字，再加 FPS，最后用 F9 切换窗口的显示与隐藏。

## 即时模式 UI

游戏中的 ImGui 窗口和 Windows 窗体程序里的按钮不同。Windows 窗体（以及 WPF、Qt 等传统 GUI 框架）属于「保留模式」：你创建一个按钮对象，它会一直存在于屏幕上，直到你主动销毁它。按钮的文字、位置、颜色都存储在按钮对象内部。

ImGui 是「即时模式」UI：每一帧你都要重新描述这一帧想显示什么。如果某一帧你没有调用 `ImGui::Begin("Hello")`，那么 Hello 窗口在这一帧就不会出现。窗口不会作为持久对象存在，它是你每帧提交的一组绘制指令。

这意味着 `OnProcess` 回调里的 ImGui 代码，每帧都会执行一遍。游戏跑 60 帧，你的窗口描述代码就执行 60 次。

## 最小窗口

先确认窗口能出现。把 `OnProcess` 改成：

```angelscript
void OnProcess(const BML::ModContext &in ctx) {
    DrawWindow();
}

private void DrawWindow() {
    ImGui::SetNextWindowPos(ImVec2(80.0f, 80.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Once);

    if (ImGui::Begin("Hello")) {
        ImGui::TextUnformatted("HelloMod is running.");
    }
    ImGui::End();
}
```

### SetNextWindowPos 和 SetNextWindowSize

`ImVec2(80.0f, 80.0f)` 表示窗口左上角距离屏幕左上角的像素偏移，水平 80 像素、垂直 80 像素。如果你的游戏分辨率是 1024x768，这个位置在左上方偏内侧。

`ImVec2(360.0f, 0.0f)` 是窗口尺寸。第一个值 360 表示宽度为 360 像素。第二个值 0 表示高度自动适应内容，窗口会根据内部控件的数量自动调整高度。

### ImGuiCond_Once 的含义

第二个参数 `ImGuiCond_Once` 告诉 ImGui：这个位置/尺寸只在窗口第一次出现时设置一次。之后用户可以用鼠标拖动窗口、调整大小，ImGui 不会强制把窗口拉回 (80, 80) 的位置。

如果你改成 `ImGuiCond_Always`，窗口每帧都会被强制设置到 (80, 80)，用户拖动窗口后下一帧又会弹回去，导致窗口无法被移动。

### Begin/End 的配对规则

`ImGui::Begin("Hello")` 返回值为 `true` 时表示窗口内容可见（窗口未被折叠、未被裁剪）。返回 `false` 表示窗口被折叠了，但窗口本身依然存在。

`ImGui::End()` 必须放在 `if` 语句的外面，而不是放在 `if` 的内部。因为即使 `Begin` 返回 `false`（窗口被折叠），ImGui 内部也已经开始了一个窗口的绘制上下文，需要用 `End()` 来关闭这个上下文。如果你把 `End()` 放进 `if` 里面，当窗口折叠时 `End()` 不会执行，ImGui 的内部状态就会错乱。

保存后启动 Player，画面左上方应当出现一个窗口，显示 "HelloMod is running."。

## 加 FPS

FPS（每秒帧数）需要知道每帧花了多长时间。ImGui 提供了一个 `ImGuiIO` 对象，里面的 `DeltaTime` 字段记录了上一帧到当前帧经过的时间（单位：秒）。如果上一帧耗时 0.016 秒，`1.0 / 0.016` 约等于 62.5，这就是当前的 FPS。

在类里增加成员变量和更新函数：

```angelscript
private float fps = 0.0f;

private void UpdateFps() {
    ImGuiIO@ io = ImGui::GetIO();
    if (io !is null && io.DeltaTime > 0.0f) {
        fps = 1.0f / io.DeltaTime;
    }
}
```

### 成员变量是跨帧状态

`fps` 是一个成员变量（声明在类的顶层，而不是某个函数内部）。因为 ImGui 是即时模式，每帧重新绘制，函数执行完毕后局部变量就消失了。如果你把 `fps` 声明为 `UpdateFps()` 的局部变量，每次 `DrawWindow()` 执行时读不到那个值。

成员变量在对象的整个生命周期内存在，帧与帧之间保持数据。任何需要跨帧记忆的数据（计数器、开关、计算结果），都应该放在成员变量里。

### DeltaTime 的含义

`DeltaTime` 是「上一帧到这一帧经过的秒数」。游戏运行越流畅，这个值越小。60 FPS 时约为 0.0167 秒，30 FPS 时约为 0.0333 秒。`1.0f / DeltaTime` 就是 FPS。检查 `DeltaTime > 0.0f` 是为了避免除以零（在游戏初始化的第一帧，DeltaTime 可能为 0）。

把 `OnProcess` 改成：

```angelscript
void OnProcess(const BML::ModContext &in ctx) {
    UpdateFps();
    DrawWindow();
}
```

在窗口里加一行：

```angelscript
if (ImGui::Begin("Hello")) {
    ImGui::TextUnformatted("HelloMod is running.");
    ImGui::TextUnformatted("FPS: " + fps);
}
ImGui::End();
```

保存后启动，窗口里应该出现实时变动的 FPS 数值。

## 按 F9 显示/隐藏

加一个成员变量和按键检测函数：

```angelscript
private bool showWindow = true;

private void HandleWindowToggle(const BML::ModContext &in ctx) {
    BML::InputHook@ input = ctx.BorrowInputManager();
    if (input is null || !input.IsKeyPressed(CKKEY_F9)) {
        return;
    }
    showWindow = !showWindow;
}
```

### BorrowInputManager 和 IsKeyPressed

`ctx.BorrowInputManager()` 从 BML 上下文中借出输入管理器。返回值是一个句柄（`@` 表示句柄类型），如果系统尚未初始化完毕，可能返回 `null`，所以需要检查。

`IsKeyPressed(CKKEY_F9)` 检测 F9 键是否在这一帧被「按下」。注意区分「按下」和「按住」：`IsKeyPressed` 在按键从未按到按下的那一帧返回 `true`，后续持续按住的帧返回 `false`。这样按一次 F9 只触发一次切换，不会出现快速反复切换的情况。

`showWindow = !showWindow` 将布尔值取反。`true` 变 `false`，`false` 变 `true`。

把 `OnProcess` 改成：

```angelscript
void OnProcess(const BML::ModContext &in ctx) {
    HandleWindowToggle(ctx);
    UpdateFps();
    DrawWindow();
}
```

`DrawWindow()` 开头加判断：

```angelscript
private void DrawWindow() {
    if (!showWindow) {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(80.0f, 80.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Once);

    if (ImGui::Begin("Hello")) {
        ImGui::TextUnformatted("HelloMod is running.");
        ImGui::TextUnformatted("FPS: " + fps);
        ImGui::TextUnformatted("Press F9 to hide or show this window.");
    }
    ImGui::End();
}
```

当 `showWindow` 为 `false` 时，`DrawWindow` 直接返回，不调用任何 ImGui 函数，窗口在这一帧就不存在。按 F9 切换回 `true`，下一帧又开始调用 ImGui 函数，窗口重新出现。

## 故障排查

| 现象 | 可能原因 | 处理方法 |
|------|----------|----------|
| 窗口不出现，日志无报错 | `OnProcess` 拼写错误或签名不对 | 确认方法签名为 `void OnProcess(const BML::ModContext &in ctx)` |
| 窗口闪烁后消失 | `DrawWindow` 没有每帧被调用 | 确认 `DrawWindow()` 在 `OnProcess` 内部，没有被条件跳过 |
| 窗口出现但无法拖动 | 使用了 `ImGuiCond_Always` 而不是 `ImGuiCond_Once` | 检查 `SetNextWindowPos` 的第二个参数 |
| F9 无反应 | `HandleWindowToggle` 没有被调用，或 `BorrowInputManager` 返回 null | 在 `HandleWindowToggle` 开头加日志确认是否执行到 |
| FPS 一直显示 0 | `UpdateFps` 没有被调用，或 `ImGui::GetIO()` 返回 null | 确认 `UpdateFps()` 在 `OnProcess` 中，且在 `DrawWindow()` 之前 |
| 折叠窗口后崩溃 | `End()` 被放在了 `if (Begin(...))` 内部 | 将 `End()` 移到 `if` 语句外面 |

## 完整脚本

```angelscript
[bml.mod id="hello.script" name="Hello Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="Minimal tutorial script mod"]
class HelloMod {
    private bool showWindow = true;
    private float fps = 0.0f;

    void OnLoad(const BML::ModContext &in ctx) {
        LogInfo(ctx, "HelloMod loaded from ModLoader/Mods/HelloMod.mod.as");
        ctx.SendIngameMessage("HelloMod loaded.");
    }

    void OnProcess(const BML::ModContext &in ctx) {
        HandleWindowToggle(ctx);
        UpdateFps();
        DrawWindow();
    }

    private void HandleWindowToggle(const BML::ModContext &in ctx) {
        BML::InputHook@ input = ctx.BorrowInputManager();
        if (input is null || !input.IsKeyPressed(CKKEY_F9)) {
            return;
        }
        showWindow = !showWindow;
        ctx.SendIngameMessage("HelloMod window toggled.");
    }

    private void UpdateFps() {
        ImGuiIO@ io = ImGui::GetIO();
        if (io !is null && io.DeltaTime > 0.0f) {
            fps = 1.0f / io.DeltaTime;
        }
    }

    private void DrawWindow() {
        if (!showWindow) {
            return;
        }

        ImGui::SetNextWindowPos(ImVec2(80.0f, 80.0f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Once);

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
}
```

## 完成状态

脚本现在能在游戏画面里显示调试窗口，窗口包含 FPS，按 F9 可以切换显示。`OnProcess` 每帧执行，ImGui 窗口每帧重新提交（即时模式），成员变量跨帧保持状态（跨帧存储）。`Begin`/`End` 必须配对，`End` 必须在 `if` 外面。

下一章将利用这个窗口，实现通过命令系统与 Mod 交互。

下一步：[05 命令系统](tutorial-05-commands.md)

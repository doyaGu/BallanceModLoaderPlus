# 参考 E：输入、UI 和每帧状态

第 5 章已经做过一个 ImGui 窗口。那时重点是“画面上能看到脚本”。

这一章换一个角度看同一类代码：

```text
OnProcess 每帧进来
输入函数判断这一帧有没有按键
成员变量保存跨帧状态
ImGui 把当前状态画出来
低频事情交给 Timer
```

这就是 BML 脚本 mod 里最常见的调试面板模型。

## 每帧是什么

Ballance 运行时，画面会一帧一帧更新。BML 会在这个循环里调用脚本的 `OnProcess`：

```angelscript
void OnProcess(const BML::ModContext &in ctx)
```

可以先按下面理解：

```text
一帧
  -> BML 调用 OnProcess
  -> 脚本读取输入
  -> 脚本更新自己的状态
  -> 脚本提交 ImGui 窗口
  -> 游戏继续渲染
```

`OnProcess` 会被频繁调用。60 FPS 时，一秒大约调用 60 次。  
所以它适合做轻量的每帧更新，不适合每帧刷一堆日志，也不适合每帧扫描大量对象。

## 这章的小脚本

新建脚本：

```text
ModLoader/Mods/LoopMod.mod.as
```

写入：

```angelscript
[bml.mod id="loop.script" name="Loop UI" version="1.0.0" author="Tutorial" bml="0.3.12" description="Show frame state, input and ImGui UI"]
class LoopMod {
    private bool showWindow = true;
    private bool firstProcess = true;

    private int frameCount = 0;
    private int toggleCount = 0;
    private int f9DownFrames = 0;
    private int timerTicks = 0;

    private float fps = 0.0f;
    private string lastAction = "loaded";
    private string timerStatus = "waiting";

    private BML::TimerRef@ statusTimer;

    void OnLoad(const BML::ModContext &in ctx) {
        StartStatusTimer(ctx);
        LogInfo(ctx, "LoopMod loaded");
    }

    void OnProcess(const BML::ModContext &in ctx) {
        frameCount++;

        UpdateFps(ctx);
        HandleInput(ctx);
        LogFirstProcess(ctx);
        DrawWindow();
    }

    void OnUnload(const BML::ModContext &in ctx) {
        LogInfo(ctx, "LoopMod unload frames=" + frameCount + " toggles=" + toggleCount);
    }

    private void StartStatusTimer(const BML::ModContext &in ctx) {
        // 低频状态更新交给 Timer，不放进每一帧里刷日志。
        BML::TimerLoopCallback@ callback = BML::TimerLoopCallback(this.OnStatusTimer);
        @statusTimer = ctx.SetInterval(1000.0f, callback, "loop-status");

        bool valid = statusTimer !is null && statusTimer.IsValid;
        LogInfo(ctx, "LoopMod status timer valid=" + BoolText(valid));
    }

    private bool OnStatusTimer(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
        timerTicks++;
        timerStatus = event.Name + " tick " + timerTicks;
        LogInfo(ctx, "LoopMod " + timerStatus);

        // 返回 true 表示循环 Timer 继续运行。
        return true;
    }

    private void UpdateFps(const BML::ModContext &in ctx) {
        float deltaMs = ctx.GetDeltaTimeMs();
        if (deltaMs > 0.0f) {
            fps = 1000.0f / deltaMs;
        }
    }

    private void HandleInput(const BML::ModContext &in ctx) {
        BML::InputHook@ input = ctx.BorrowInputManager();
        if (input is null) {
            f9DownFrames = 0;
            return;
        }

        if (input.IsKeyDown(CKKEY_F9)) {
            f9DownFrames++;
        } else {
            f9DownFrames = 0;
        }

        // IsKeyPressed 只在按下的那一帧返回 true，适合做开关。
        if (input.IsKeyPressed(CKKEY_F9)) {
            showWindow = !showWindow;
            toggleCount++;
            lastAction = "F9 toggle " + toggleCount + " visible=" + BoolText(showWindow);
            LogInfo(ctx, "LoopMod " + lastAction);
        }
    }

    private void LogFirstProcess(const BML::ModContext &in ctx) {
        if (!firstProcess) {
            return;
        }

        firstProcess = false;
        LogInfo(ctx, "LoopMod first process");
    }

    private void DrawWindow() {
        if (!showWindow) {
            return;
        }

        ImGui::SetNextWindowPos(ImVec2(80.0f, 80.0f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(400.0f, 0.0f), ImGuiCond_Once);

        if (ImGui::Begin("Loop UI")) {
            ImGui::TextUnformatted("Frame: " + frameCount);
            ImGui::TextUnformatted("FPS: " + fps);
            ImGui::TextUnformatted("F9 down frames: " + f9DownFrames);
            ImGui::TextUnformatted("Toggles: " + toggleCount);
            ImGui::TextUnformatted("Last action: " + lastAction);
            ImGui::TextUnformatted("Timer: " + timerStatus);
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

代码分成四块看：

```text
OnLoad      启动低频 Timer
OnProcess   每帧更新输入、FPS 和窗口
OnUnload    退出时写一次统计日志
私有函数    把每一步拆开，避免 OnProcess 变成一大团
```

## `IsKeyDown` 和 `IsKeyPressed`

这章同时用了两个输入函数：

```angelscript
input.IsKeyDown(CKKEY_F9)
input.IsKeyPressed(CKKEY_F9)
```

区别很重要：

| 函数 | 按住 F9 不放时的结果 |
| --- | --- |
| `IsKeyDown` | 每一帧都是 `true` |
| `IsKeyPressed` | 只有刚按下的那一帧是 `true` |

开关窗口要用 `IsKeyPressed`：

```angelscript
if (input.IsKeyPressed(CKKEY_F9)) {
    showWindow = !showWindow;
}
```

如果用 `IsKeyDown` 做开关，按住 F9 的几十帧里会反复切换，窗口会闪，最后停在哪个状态也难判断。

`IsKeyDown` 适合做“按住期间持续生效”的状态。例如本章用它统计：

```angelscript
f9DownFrames++;
```

这只是显示用，不改变游戏状态。

## 状态为什么放成员变量

脚本类开头这些变量都写在函数外：

```angelscript
private bool showWindow = true;
private int frameCount = 0;
private string lastAction = "loaded";
```

它们属于 `LoopMod` 对象，会跨帧保留。

如果把 `frameCount` 写在 `OnProcess` 里面：

```angelscript
void OnProcess(const BML::ModContext &in ctx) {
    int frameCount = 0;
    frameCount++;
}
```

每一帧都会重新创建一个新的 `frameCount`，结果永远只会加到 1。  
跨帧状态要放在成员变量里。

本章保存的状态有：

| 状态 | 用途 |
| --- | --- |
| `showWindow` | 当前窗口是否显示 |
| `frameCount` | `OnProcess` 进来过多少次 |
| `f9DownFrames` | F9 连续按住了多少帧 |
| `toggleCount` | 用 F9 切换过几次窗口 |
| `timerStatus` | 最近一次低频 Timer 状态 |

## ImGui 窗口每帧重画

`DrawWindow()` 每帧都会被调用：

```angelscript
private void DrawWindow() {
    if (!showWindow) {
        return;
    }

    if (ImGui::Begin("Loop UI")) {
        ImGui::TextUnformatted("Frame: " + frameCount);
        ImGui::TextUnformatted("Timer: " + timerStatus);
    }
    ImGui::End();
}
```

ImGui 不保存一份窗口内容等着下次显示。每一帧都要重新提交这一帧的窗口内容。  
窗口里显示的数字能变化，是因为成员变量先被更新，然后这一帧又画了一次。

可以按这个顺序记：

```text
先改状态
再画状态
下一帧重复
```

## 低频事情交给 Timer

`OnProcess` 一秒会执行很多次。下面这些事情不要每帧做：

```text
写日志
扫描大量对象
刷新不需要实时变化的统计信息
读取较大的资源文件
```

本章用 `SetInterval` 每秒更新一次状态：

```angelscript
BML::TimerLoopCallback@ callback = BML::TimerLoopCallback(this.OnStatusTimer);
@statusTimer = ctx.SetInterval(1000.0f, callback, "loop-status");
```

回调返回 `true`，Timer 会继续循环：

```angelscript
private bool OnStatusTimer(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
    timerTicks++;
    timerStatus = event.Name + " tick " + timerTicks;
    return true;
}
```

这样 `OnProcess` 负责每帧显示，Timer 负责低频更新。两边分工清楚，日志也不会被刷爆。

## 运行后看什么

保存脚本，重启 Player。

画面上会出现 `Loop UI` 窗口，里面的 `Frame` 会持续增加，`FPS` 会变化。按 `F9` 可以隐藏窗口，再按一次重新显示。

日志里可以看到：

```text
[ModLoader/INFO]: Loading Mod loop.script[Loop UI] v1.0.0 by Tutorial
[loop.script/INFO]: LoopMod status timer valid=true
[loop.script/INFO]: LoopMod loaded
[ModLoader/INFO]: BML script mod summary: loaded=1 failed=0
[loop.script/INFO]: LoopMod first process
[loop.script/INFO]: LoopMod loop-status tick 1
```

退出 Player 时还会有：

```text
[loop.script/INFO]: LoopMod unload frames=... toggles=...
```

如果看不到窗口，先查三件事：

```text
脚本是否 loaded=1 failed=0
日志里有没有 LoopMod first process
showWindow 初始值是不是 true
```

如果按 F9 没反应，先查：

```text
BorrowInputManager() 是否返回 null
是否用了 IsKeyPressed(CKKEY_F9)
游戏窗口是否处于焦点状态
```

## 哪些 UI 适合放在这里

ImGui 面板适合做调试和观察：

```text
显示 FPS、当前事件、最后一次命令
显示找到的对象名、组大小、DataArray 摘要
放几个调试开关
临时显示脚本内部状态
```

这些 UI 不会写入关卡文件，也不会自动成为 Ballance 的原生菜单。  
它更像开发时的仪表盘。

正式玩法提示、长期存在的 HUD、菜单页面，后面会单独讲 BML 的 HUD 和菜单服务。不要一开始就把所有界面都堆进 ImGui 窗口。

## 本章结果

到这里，`OnProcess` 的心智模型应该是：

```text
每帧进来一次
只做轻量工作
读取这一帧输入
更新成员变量
提交这一帧 ImGui
低频事情交给 Timer 或事件
```

下一章讲脚本 mod 的边界。重点是分清 BML 脚本 mod、CKAS 运行时脚本、Virtools 行为上下文分别站在哪一层。

# 参考 J：BML HUD、菜单和消息服务

相关基础中已经用过两类界面：

```text
ImGui 调试窗口
BML 的游戏内消息
```

它们解决的问题不同。

ImGui 适合做脚本自己的调试面板：显示变量、按钮、开关、列表。
BML 自带的 UI/HUD/Menu 服务适合接到游戏已有界面里：发一条消息、显示 FPS、显示 SR Timer、打开 BML 菜单。

本参考只讲 BML 自带服务。

## 先分清三组能力

BML 暴露给脚本的界面相关能力，可以先按三组记：

| 组 | 常用 API | 用途 |
| --- | --- | --- |
| 消息 | `SendIngameMessage`、`ClearIngameMessages`、`BML::UI::SendMessage` | 在游戏画面里显示短文本 |
| HUD | `GetHUD`、`SetHUD`、`ShowFPS`、`ShowSRTimer`、SR Timer | 控制 BML 已有 HUD 项 |
| 菜单 | `OpenModsMenu`、`CloseModsMenu`、`OpenMapMenu`、`CloseMapMenu` | 打开或关闭 BML 已有菜单 |

这些 API 不是画按钮、画窗口的工具。
要画自己的按钮和窗口，继续用 ImGui。

## `ctx` 方法和命名空间方法

同一个功能经常有两种写法。

在 mod 回调里，通常用 `ctx`：

```angelscript
ctx.SendIngameMessage("hello");
ctx.ShowFPS(true);
ctx.OpenModsMenu();
```

也可以用命名空间方法：

```angelscript
BML::UI::SendMessage("hello");
BML::HUD::ShowFPS(true);
BML::Menu::OpenModsMenu();
```

多数示例脚本都在 BML 回调里执行，已经有 `ctx`，所以优先用 `ctx`。
命名空间写法适合放在不方便传 `ctx` 的辅助代码里，或者临时确认某个 BML 全局服务能不能工作。

## HUD 的心智模型

HUD 可以先理解成几个开关合在一起：

```text
Title
FPS
SR Timer
```

`ShowFPS(true)` 只打开 FPS。
`ShowFPS(false)` 只关掉 FPS。

`ShowSRTimer(true)` 和 `ShowSRTimer(false)` 控制 SR Timer 是否显示。

`GetHUD()` 读出当前 HUD 组合。
`SetHUD(mode)` 一次性恢复到某个组合。

这就是为什么示例会在加载时保存原来的 HUD：

```text
加载脚本
  记住原来的 HUD
  打开示例要演示的 HUD 项
卸载脚本
  把 HUD 恢复回去
```

脚本不应该永久占用玩家的 HUD 状态。

## SR Timer 的心智模型

SR Timer 是 BML 的内置计时器。

这一组 API 只管这只计时器：

| API | 含义 |
| --- | --- |
| `ShowSRTimer(true)` | 显示计时器 |
| `ResetSRTimer()` | 归零 |
| `StartSRTimer()` | 开始走 |
| `PauseSRTimer()` | 暂停 |
| `GetSRTime()` | 读取当前时间 |

它不是脚本 Timer。

脚本 Timer 是“过一段时间调用一个函数”。
SR Timer 是“HUD 上显示的一只计时表”。

示例会用脚本 Timer 等一秒，然后读取 SR Timer 的值。

## 新建脚本

新建：

```text
ModLoader/Mods/HudMenuMod.mod.as
```

写入：

```angelscript
[bml.mod id="hud.menu.demo" name="HUD Menu Demo" version="1.0.0" author="Tutorial" bml="0.3.12" description="Shows BML HUD, menu, and message services"]
class HudMenuMod {
    private int originalHud = 0;
    private bool hasOriginalHud = false;

    private bool fpsShown = true;
    private bool srShown = true;
    private BML::CommandRef@ commandRef;

    void OnLoad(const BML::ModContext &in ctx) {
        // 保存玩家原来的 HUD 组合，卸载时恢复。
        originalHud = ctx.GetHUD();
        hasOriginalHud = true;

        RegisterCommand(ctx);

        // 两种消息入口都会显示到 BML 的游戏内消息区域。
        ctx.SendIngameMessage("HUD Menu Demo loaded.");
        BML::UI::SendMessage("Message sent through BML::UI.");

        // 打开 FPS 和 SR Timer，演示 BML 内置 HUD。
        ctx.ShowFPS(true);
        ctx.ShowSRTimer(true);
        ctx.ResetSRTimer();
        ctx.StartSRTimer();

        // 一秒后读取 HUD 和 SR Timer 状态。
        BML::TimerCallback@ statusTimer = BML::TimerCallback(this.OnStatusTimer);
        ctx.SetTimeout(1000.0f, statusTimer, "hud-status");

        // 再过一小段时间暂停 SR Timer，避免一直占用计时器。
        BML::TimerCallback@ pauseTimer = BML::TimerCallback(this.OnPauseTimer);
        ctx.SetTimeout(1600.0f, pauseTimer, "hud-pause");

        LogInfo(ctx, "HUD Menu Demo loaded originalHud=" + originalHud);
    }

    void OnUnload(const BML::ModContext &in ctx) {
        ctx.PauseSRTimer();
        ctx.CloseModsMenu();
        ctx.CloseMapMenu();

        if (hasOriginalHud) {
            ctx.SetHUD(originalHud);
        }

        LogInfo(ctx, "HUD Menu Demo unloaded");
    }

    private void RegisterCommand(const BML::ModContext &in ctx) {
        BML::CommandDefinition def;
        def.Name = "hm";
        def.Alias = "hmenu";
        def.Description = "Control HUD Menu Demo";
        def.Usage = "hm [message|clear|fps|sr|mods|map|restore]";
        def.Category = "Tutorial";
        def.Enabled = true;

        BML::CommandCallback@ execute = BML::CommandCallback(this.OnHudCommand);
        BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompleteHudCommand);

        @commandRef = ctx.RegisterCommand(def, execute, complete);
        bool valid = commandRef !is null && commandRef.IsValid;
        LogInfo(ctx, "HUD command registered=" + BoolText(valid));
    }

    void OnHudCommand(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        string action = "message";
        if (event.ArgCount > 0) {
            action = event.GetArg(0);
        }

        if (action == "message") {
            ctx.SendIngameMessage("HUD command message.");
            LogInfo(ctx, "HUD command message");
        } else if (action == "clear") {
            ctx.ClearIngameMessages();
            LogInfo(ctx, "HUD messages cleared");
        } else if (action == "fps") {
            fpsShown = !fpsShown;
            ctx.ShowFPS(fpsShown);
            LogInfo(ctx, "HUD fpsShown=" + BoolText(fpsShown));
        } else if (action == "sr") {
            srShown = !srShown;
            ctx.ShowSRTimer(srShown);
            LogInfo(ctx, "HUD srShown=" + BoolText(srShown));
        } else if (action == "mods") {
            ctx.OpenModsMenu();
            LogInfo(ctx, "HUD opened mods menu");
        } else if (action == "map") {
            ctx.OpenMapMenu();
            LogInfo(ctx, "HUD opened map menu");
        } else if (action == "restore") {
            Restore(ctx);
            LogInfo(ctx, "HUD restored");
        } else {
            ctx.SendIngameMessage("Usage: hm [message|clear|fps|sr|mods|map|restore]");
            LogWarn(ctx, "Unknown HUD action: " + action);
        }
    }

    void CompleteHudCommand(const BML::ModContext &in ctx,
                            const BML::CommandEvent &in event,
                            BML::CommandCompletion &inout completions) {
        completions.Add("message");
        completions.Add("clear");
        completions.Add("fps");
        completions.Add("sr");
        completions.Add("mods");
        completions.Add("map");
        completions.Add("restore");
    }

    void OnStatusTimer(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
        LogInfo(ctx, "HUD status mode=" + ctx.GetHUD() + " srTime=" + ctx.GetSRTime());
        ctx.SendIngameMessage("HUD status logged.");
    }

    void OnPauseTimer(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
        ctx.PauseSRTimer();
        LogInfo(ctx, "HUD SR timer paused at " + ctx.GetSRTime());
    }

    private void Restore(const BML::ModContext &in ctx) {
        ctx.PauseSRTimer();
        ctx.CloseModsMenu();
        ctx.CloseMapMenu();

        if (hasOriginalHud) {
            ctx.SetHUD(originalHud);
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
        if (value) {
            return "true";
        }
        return "false";
    }
}
```

## 代码按四段看

第一段保存和恢复 HUD：

```angelscript
originalHud = ctx.GetHUD();
hasOriginalHud = true;
```

卸载时：

```angelscript
ctx.SetHUD(originalHud);
```

这样做是为了不把玩家原来的 FPS、标题、SR Timer 显示习惯留成脚本演示状态。

第二段发送消息：

```angelscript
ctx.SendIngameMessage("HUD Menu Demo loaded.");
BML::UI::SendMessage("Message sent through BML::UI.");
```

`ctx.SendIngameMessage(...)` 是回调内更常用的写法。
`BML::UI::SendMessage(...)` 展示命名空间入口。

清理消息用：

```angelscript
ctx.ClearIngameMessages();
BML::UI::ClearMessages();
```

清理消息会清掉当前 BML 消息区域里的文本。完整 mod 里不要随手清，因为其他 mod 也可能正在显示消息。

第三段控制 HUD 和 SR Timer：

```angelscript
ctx.ShowFPS(true);
ctx.ShowSRTimer(true);
ctx.ResetSRTimer();
ctx.StartSRTimer();
```

脚本加载后，画面上应该能看到 FPS 和 SR Timer。
一秒后 `OnStatusTimer` 读取状态：

```angelscript
ctx.GetHUD()
ctx.GetSRTime()
```

第四段注册命令：

```text
hm message
hm clear
hm fps
hm sr
hm mods
hm map
hm restore
```

菜单相关操作放到命令里。
如果脚本每次加载都自动打开菜单，会打断正常进游戏。

## 运行后看什么

保存脚本，重新启动 Player。

画面上应该能看到：

```text
HUD Menu Demo loaded.
Message sent through BML::UI.
FPS
SR Timer
```

日志里应该能看到：

```text
HUD command registered=true
HUD Menu Demo loaded originalHud=...
HUD status mode=... srTime=...
HUD SR timer paused at ...
```

打开 BML 命令栏，输入：

```text
hm message
```

画面会再出现一条消息。

输入：

```text
hm fps
```

FPS 会切换显示状态。

输入：

```text
hm sr
```

SR Timer 会切换显示状态。

输入：

```text
hm mods
```

BML Mods 菜单会打开。

输入：

```text
hm restore
```

脚本会关闭菜单，并恢复加载前保存的 HUD 组合。

## 命名空间入口怎么对应

示例主要用 `ctx`，下面是对应关系：

| `ctx` 写法 | 命名空间写法 |
| --- | --- |
| `ctx.SendIngameMessage(text)` | `BML::UI::SendMessage(text)` |
| `ctx.ClearIngameMessages()` | `BML::UI::ClearMessages()` |
| `ctx.ShowFPS(show)` | `BML::HUD::ShowFPS(show)` |
| `ctx.ShowSRTimer(show)` | `BML::HUD::ShowSRTimer(show)` |
| `ctx.StartSRTimer()` | `BML::HUD::StartSRTimer()` |
| `ctx.PauseSRTimer()` | `BML::HUD::PauseSRTimer()` |
| `ctx.ResetSRTimer()` | `BML::HUD::ResetSRTimer()` |
| `ctx.GetSRTime()` | `BML::HUD::GetSRTime()` |
| `ctx.OpenModsMenu()` | `BML::Menu::OpenModsMenu()` |
| `ctx.CloseModsMenu()` | `BML::Menu::CloseModsMenu()` |
| `ctx.OpenMapMenu()` | `BML::Menu::OpenMapMenu()` |
| `ctx.CloseMapMenu()` | `BML::Menu::CloseMapMenu()` |

`GetHUD()` 和 `SetHUD(mode)` 也有命名空间入口：

```angelscript
BML::HUD::GetMode();
BML::HUD::SetMode(mode);
```

## 常见问题

消息没出现：

- 确认脚本已经加载；
- 看日志里有没有 `HUD Menu Demo loaded`；
- 如果刚执行过 `hm clear`，消息区域可能已经被清掉。

FPS 或 SR Timer 没变化：

- 确认没有马上执行 `hm restore`；
- 确认别的 mod 没有在同一时间改 HUD；
- 看日志里的 `HUD status mode=...` 是否变化。

菜单命令没反应：

- 确认日志里有 `HUD command registered=true`；
- 确认输入的是 `hm mods` 或 `hm map`；
- 如果正在某些过场或菜单状态，BML 菜单打开结果可能和关卡内不同。

命令补全里没有候选：

- 命令名后面留一个空格再补全；
- 确认 `RegisterCommand` 传入了 `complete`；
- 补全函数第三个参数要写成 `BML::CommandCompletion &inout completions`。

## 速查结论

BML 运行框架可以按这几块归类：

```text
生命周期
游戏事件
ModContext 服务
路径和日志
输入、ImGui、每帧状态
脚本边界
进阶回调
mod 信息、依赖、导出
DataShare
BML HUD、菜单和消息
```

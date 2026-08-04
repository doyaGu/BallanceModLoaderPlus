# 12 状态面板 Mod

到这一步，你已经掌握了：ImGui 窗口绘制（第 4 章）、命令注册（第 5 章）、游戏事件响应（第 6 章）、对象查找（第 8 章）、DataArray 读取（第 11 章）。现在把它们全部组合起来，做第二个完整的实用 mod。

目标：一个运行时状态面板，集中显示游戏信息，FPS、关卡号、活动球、复活点、分数、球的坐标。玩自定义地图或调试其他 mod 时，这个面板能让你随时了解游戏内部发生了什么。

## 为什么需要状态面板

开发 mod 的过程中，你经常需要确认某个值是否正确：

- "我的分数修改 mod 真的改了分数吗？" -> 看 Points
- "球现在是什么材质？" -> 看 ActiveBall
- "过了检查点没有？" -> 看 Resetpoint 是否更新
- "游戏在跑多少帧？" -> 看 FPS

每次都写临时的日志输出太麻烦。一个常驻的状态面板就像汽车仪表盘，抬眼就能看到关键信息。

## 设计

### 面板显示的内容

| 数据 | 来源 | 刷新频率 |
| --- | --- | --- |
| FPS | ImGui DeltaTime | 每帧 |
| 关卡编号 | CurrentLevel 列 0 | 每帧 |
| 活动球 | CurrentLevel 列 1 | 每帧 |
| 当前复活点 | CurrentLevel 列 3 | 每帧 |
| 当前分数 | CurrentLevel 列 5 | 每帧 |

"每帧"不意味着这些值每帧都变，大部分时间它们是稳定的。但每帧读取确保面板能立即反映变化，不会有延迟。

### 生命周期管理

```text
OnLoad              -> 注册 panel 命令，打印加载消息
GAME_EVENT_START    -> 获取 CurrentLevel 句柄
OnProcess           -> 计算 FPS，读取数据，绘制 ImGui 面板
GAME_EVENT_EXIT     -> 清空句柄
```

这个模式和坐标显示器完全一致。唯一的新内容是命令注册，让用户能通过输入 `panel show/hide` 控制面板显示。

### 命令设计

面板的显示控制通过命令完成：

```text
panel show     -> 显示面板
panel hide     -> 隐藏面板
panel toggle   -> 切换显示/隐藏（默认行为）
```

三个子命令就够了。不需要设计更复杂的功能，面板的职责是显示信息，命令的职责只是控制可见性。

## 完整脚本

保存为 `ModLoader/Mods/PanelMod.mod.as`：

```angelscript
[bml.mod id="panel.script" name="Status Panel" version="1.0.0" author="Tutorial" bml="0.3.12" description="Runtime status panel with command toggle"]
class PanelMod {
    private CKDataArray@ currentLevel = null;
    private bool showPanel = true;
    private float fps = 0.0f;

    void OnLoad(const BML::ModContext &in ctx) {
        RegisterCommand(ctx);
        LogInfo(ctx, "PanelMod loaded");
        ctx.SendIngameMessage("PanelMod loaded. Command: panel show/hide");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_START_LEVEL) {
            @currentLevel = ctx.BorrowDataArrayByName("CurrentLevel");
        } else if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL) {
            @currentLevel = null;
        }
    }

    void OnProcess(const BML::ModContext &in ctx) {
        UpdateFps();
        if (showPanel) {
            DrawPanel(ctx);
        }
    }

    private void UpdateFps() {
        ImGuiIO@ io = ImGui::GetIO();
        if (io !is null && io.DeltaTime > 0.0f) {
            fps = 1.0f / io.DeltaTime;
        }
    }

    private void DrawPanel(const BML::ModContext &in ctx) {
        ImGui::SetNextWindowPos(ImVec2(10.0f, 60.0f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f), ImGuiCond_Once);

        if (ImGui::Begin("Status Panel")) {
            ImGui::TextUnformatted("FPS: " + int(fps));

            if (currentLevel is null) {
                ImGui::TextUnformatted("Not in level");
            } else {
                int rows = BML::CK::GetRowCount(currentLevel);
                if (rows > 0) {
                    int levelId = BML::CK::GetInt(currentLevel, 0, 0, -1);
                    string activeBall = BML::CK::GetString(currentLevel, 0, 1, "?");
                    string resetpoint = BML::CK::GetString(currentLevel, 0, 3, "?");
                    int points = BML::CK::GetInt(currentLevel, 0, 5, 0);

                    ImGui::Separator();
                    ImGui::TextUnformatted("Level: " + levelId);
                    ImGui::TextUnformatted("Ball: " + activeBall);
                    ImGui::TextUnformatted("Resetpoint: " + resetpoint);
                    ImGui::TextUnformatted("Points: " + points);
                } else {
                    ImGui::TextUnformatted("CurrentLevel empty");
                }
            }
        }
        ImGui::End();
    }

    // --- 命令 ---

    private void RegisterCommand(const BML::ModContext &in ctx) {
        BML::CommandDefinition def;
        def.Name = "panel";
        def.Description = "Toggle status panel";
        def.Usage = "panel [show|hide|toggle]";
        def.Category = "Tutorial";
        def.Enabled = true;

        BML::CommandCallback@ execute = BML::CommandCallback(this.OnPanelCommand);
        BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompletePanelCommand);

        BML::CommandRef@ commandRef = ctx.RegisterCommand(def, execute, complete);
        if (commandRef is null || !commandRef.IsValid) {
            LogInfo(ctx, "panel command registration failed");
        }
    }

    private void OnPanelCommand(const BML::ModContext &in ctx,
                                const BML::CommandEvent &in event) {
        string action = "toggle";
        if (event.ArgCount > 0) {
            action = event.GetArg(0);
        }

        if (action == "show") {
            showPanel = true;
            ctx.SendIngameMessage("Panel shown.");
        } else if (action == "hide") {
            showPanel = false;
            ctx.SendIngameMessage("Panel hidden.");
        } else if (action == "toggle") {
            showPanel = !showPanel;
            ctx.SendIngameMessage(showPanel ? "Panel shown." : "Panel hidden.");
        } else {
            ctx.SendIngameMessage("Usage: panel [show|hide|toggle]");
        }
    }

    private void CompletePanelCommand(const BML::ModContext &in ctx,
                                      const BML::CommandEvent &in event,
                                      BML::CommandCompletion &inout completions) {
        completions.Add("show");
        completions.Add("hide");
        completions.Add("toggle");
    }

    // --- 工具 ---

    private void LogInfo(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info(message);
        }
    }
}
```

## 代码分段解析

### 成员变量

```angelscript
private CKDataArray@ currentLevel = null;
private bool showPanel = true;
private float fps = 0.0f;
```

三个状态：

- `currentLevel`：缓存的表句柄，关卡中有效，菜单中为 null
- `showPanel`：面板是否可见，由命令控制
- `fps`：每帧更新的帧率值

### FPS 计算

```angelscript
private void UpdateFps() {
    ImGuiIO@ io = ImGui::GetIO();
    if (io !is null && io.DeltaTime > 0.0f) {
        fps = 1.0f / io.DeltaTime;
    }
}
```

`DeltaTime` 是上一帧到这一帧的时间间隔（秒）。1 除以时间间隔就是帧率。比如两帧间隔 0.0167 秒，帧率就是 60。

检查 `DeltaTime > 0.0f` 是为了避免除以零，游戏刚启动的第一帧 DeltaTime 可能是 0。

### 面板绘制

`DrawPanel` 函数的结构：

1. 设置窗口初始位置和大小（`ImGuiCond_Once` 表示只在第一次生效，之后用户可以拖动）
2. `ImGui::Begin` 开始窗口
3. 先显示 FPS（不需要关卡数据）
4. 如果 `currentLevel` 为 null -> 显示"Not in level"
5. 如果表有数据 -> 逐列读取并显示
6. `ImGui::End` 结束窗口

注意 `ImGui::Separator()` 在 FPS 和关卡数据之间画一条分隔线，视觉上区分两个区域。

### 命令注册和处理

命令部分和第 5 章学过的完全一样：

- `RegisterCommand` 在 `OnLoad` 里调用一次
- `OnPanelCommand` 处理用户输入
- `CompletePanelCommand` 提供 Tab 补全

命令处理的逻辑：

```text
无参数 -> 默认执行 toggle
"show" -> showPanel = true
"hide" -> showPanel = false
"toggle" -> showPanel = !showPanel
其他 -> 显示用法提示
```

### 关卡事件处理

```angelscript
if (event == BML::GAME_EVENT_START_LEVEL) {
    @currentLevel = ctx.BorrowDataArrayByName("CurrentLevel");
} else if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL) {
    @currentLevel = null;
}
```

两行代码，但缺一不可：

- 不获取 -> 面板没数据可显示
- 不清空 -> 退出关卡后句柄悬空，访问会崩溃

## 运行效果

菜单中面板只显示 FPS 和"Not in level"。进入第一关后：

```text
┌─ Status Panel ─────────────────────────────────────────┐
│ FPS: 60                                               │
│ ─────────────────────────────────────────────────────  │
│ Level: 1                                              │
│ Ball: Ball_Wood                                       │
│ Resetpoint: [-1,0,0,0][0,1,0,0][0,0,-1,0][35,8,0,1]│
│ Points: 0                                             │
└────────────────────────────────────────────────────────┘
```

游戏过程中，面板实时反映变化：经过变球器 Ball 更新，吃到加分球 Points 递增，过检查点 Resetpoint 变为新矩阵。退出到菜单后回到"Not in level"。

按 `/` 打开命令栏输入 `panel hide` 隐藏面板，`panel show` 重新显示，`panel` 切换状态。Tab 键自动补全子命令。

## 这个 mod 用到的所有技术

回顾一下每项技术的来源：

| 技术 | 学习章节 | 在面板里的用途 |
| --- | --- | --- |
| ImGui 窗口 | 第 4 章 | `Begin/End`、`TextUnformatted`、`Separator` |
| FPS 计算 | 第 4 章 | `ImGui::GetIO().DeltaTime` |
| 命令注册 | 第 5 章 | `RegisterCommand`、回调、Tab 补全 |
| 游戏事件 | 第 6 章 | `OnGameEvent` 管理句柄生命周期 |
| DataArray 读取 | 第 11 章 | `BorrowDataArrayByName`、`GetInt`、`GetString` |
| 句柄缓存 | 第 8-9 章 | 进入时获取、退出时清空 |

如果你对某个部分感到不确定，回头翻对应的章节复习。面板 mod 是这些技术的综合练习。

## 常见问题

**"面板位置挡住了游戏画面"**

拖动面板标题栏移动位置。`ImGuiCond_Once` 只设置初始位置，之后可以自由拖动。如果想改默认位置，修改 `SetNextWindowPos` 里的坐标。

**"退出关卡后面板崩溃了"**

检查是否在 `GAME_EVENT_PRE_EXIT_LEVEL` 里把 `@currentLevel` 设为 null。如果没有，`DrawPanel` 会尝试读取已销毁的表。

**"面板命令不响应"**

确认 `RegisterCommand` 在 `OnLoad` 里被调用了。检查日志里是否有"panel command registration failed"。如果有，可能和另一个 mod 的命令名冲突。

**"FPS 显示为 0"**

`ImGui::GetIO()` 在极早期可能返回 null。代码里已经有 null 检查，不会崩溃。如果持续显示 0，说明 ImGui 没有正常初始化，确认 BML 版本是否正确。

**"Points 不更新"**

确认你确实吃到了加分机关（金色球形）。注意不是所有看起来像道具的对象都会加分，只有金色加分球才影响 Points。

## 扩展方向

这个面板是你的开发工具。后续章节可以继续往里面加东西：

- **球坐标**：结合第 9 章，用 `ctx.Borrow3dEntityByName` 获取球，`BML::CK::GetPosition` 取坐标，在面板里多显示一行 XYZ
- **关卡计时**：在 `GAME_EVENT_START_LEVEL` 重置一个 `elapsed` 变量，`OnProcess` 里累加 `DeltaTime`，显示经过的秒数
- **Sector 信息**：根据球的位置判断当前在哪个小节
- **调试入口**：每次开发新 mod 时，先在面板里加一行显示相关数据，确认值正确后再写正式逻辑

---

**完成状态**：第二个实用 mod 完成，状态面板能显示 FPS 和关卡数据，命令可以控制显隐

---

下一步：[13 Levelinit 如何处理占位符](tutorial-13-levelinit-placeholders.md)

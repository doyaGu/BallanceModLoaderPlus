# 14 Gameplay 状态机

上一章看了 Levelinit 如何整理关卡内容：建组、填表、设初始状态。Levelinit 完成后退出，接下来 `Gameplay.nmo` 启动，接管整个游戏会话。

Levelinit 做的是一次性初始化。Gameplay 做的是持续运行的状态维护。它每一帧都在检查：球在哪里、该激活哪个 Sector、玩家有没有掉落、是不是该播放变球器动画。它的工作方式更接近状态机，根据当前状态决定下一步动作。

## Gameplay 的职责

进入关卡后，以下功能全部由 Gameplay 驱动：

```text
球的导航控制（键盘输入 -> 球移动）
相机的导航控制（跟随球旋转和平移）
变球器管理（触发动画、切换 ActiveBall）
Sector 激活和关闭（只渲染球附近的关卡段落）
检查点推进（经过双火焰柱时更新复活点）
分数计算（吃到加分机关时累加）
死亡和复活流程
关卡结束判定
游戏事件消息转发
```

所有这些逻辑在三个行为图子系统中协作运行。

## 三个子系统

Gameplay.nmo 内部分成三个主要行为图，各有明确职责：

| 行为图 | 规模 | 职责 |
| --- | --- | --- |
| `Gameplay_Ingame` | 788 个节点 | 总控：球控制、相机控制、变球器、初始化状态 |
| `Gameplay_SectorManager` | 138 个节点 | 小节切换：根据 IngameParameter 激活/关闭 Sector 组 |
| `Gameplay_Events` | 234 个节点 | 事件响应：检查点、按键事件、流式加载事件 |

它们之间通过 Virtools 消息（Send Message / Wait Message）通信，形成松耦合的协作关系。脚本 mod 不需要介入这些消息，但理解它们的存在有助于解释"为什么某些状态变化看起来有延迟"。

### Gameplay_Ingame：总控制器

这是最大的子系统，负责：

- **Init Ingame**：开局时初始化球和相机
- **Ball Navigation**：把键盘输入转换为球的物理力
- **Cam Navigation**：让相机跟随球移动
- **Trafo Manager**：管理变球器的触发和动画
- **BallNav On/Off** 和 **CamNav On/Off**：在特定时机锁定或解锁球/相机的控制（比如过变球器时球不能动）
- **DepthTest**：判断球是否掉出关卡（触发死亡）
- **Binary Switch**：根据状态分支选择不同的逻辑路径

### Gameplay_SectorManager：小节管理

Ballance 把每关分成若干个 Sector。第一关有 4 个 Sector，高难度关卡更多。SectorManager 做的事很简单：

1. 从 `IngameParameter` 读取 `Activate Sector` 和 `Deactivate Sector` 的值
2. 用 `CurrentLevel.Activation Phase?` 判断是否需要执行切换
3. 调用 Set Cell / Get Cell 操作 DataArray
4. 激活新 Sector 组里的对象（使其可见并参与物理）
5. 关闭旧 Sector 组里的对象（使其不可见并暂停物理）

这让游戏在任意时刻只渲染和计算当前区域附近的对象，节省性能。

### Gameplay_Events：事件分发

Events 监听特定触发条件并执行响应动作：

- **activate next Checkpoint**：玩家到达检查点时推进复活点
- **activate Sektor**：触发 Sector 切换请求
- **set Resetpoint**：把当前球位置写入 CurrentLevel 的 CurrentResetpoint
- **Key Event**：处理暂停等非移动按键
- **Streaming Event**：远处对象的流式加载/卸载

## IngameParameter：运行时控制面板

`IngameParameter` 只有 1 行 5 列。它是 Gameplay 各子系统之间共享状态的主要表格。

| 列号 | 列名 | 类型 | 含义 | 第一关开局值 |
| --- | --- | --- | --- | --- |
| 0 | Activetrafo | object | 当前变球器对象（正在变球时有值） | 空 |
| 1 | Activate Sector | int | 下一个要激活的 Sector 编号 | 1 |
| 2 | Deactivate Sector | int | 下一个要关闭的 Sector 编号 | 0 |
| 3 | Tutorial activ? | parameter | 教程提示是否激活 | TRUE |
| 4 | RollSound activate? | parameter | 滚动音效是否激活 | FALSE |

`Activetrafo`、`Tutorial activ?` 和 `RollSound activate?` 的底层类型不是字符串。脚本示例用 `GetString` 读取它们，是为了在窗口里显示可读文本。

这张表的工作方式：Gameplay_Events 在检查点触发时修改 `Activate Sector` 和 `Deactivate Sector` 的值，SectorManager 每帧检查这些值并执行对应的激活/关闭操作。完成后更新值表示已处理。

一个典型的 Sector 切换流程：

```text
1. 球到达检查点
2. Gameplay_Events 把 Activate Sector 设为 2, Deactivate Sector 设为 0
3. SectorManager 检测到需要切换
4. SectorManager 激活 Sector_02 组, 关闭 Sector_00（如果有）
5. CurrentLevel.Activation Phase? 被设为 FALSE, 表示切换完成
```

## CurrentLevel 在 Gameplay 中的角色

第 11 章你已经读过 CurrentLevel。在 Gameplay 的上下文里，它的每一列都有对应的子系统负责读写：

| 列名 | 谁写入 | 谁读取 |
| --- | --- | --- |
| Level ID | Levelinit（一次） | Gameplay_Ingame |
| ActiveBall | Trafo Manager（过变球器时） | Ball Navigation |
| Ball_Pos_Frame | Levelinit / 复活流程 | 球生成和复活流程 |
| CurrentResetpoint | Gameplay_Events（过检查点时） | 死亡复活流程 |
| Activation Phase? | SectorManager | SectorManager |
| Points | 加分逻辑 | HUD 显示 |

脚本 mod 可以随时读取这些列来了解游戏当前状态。你的脚本是观察者，Gameplay 的行为图是写入者。

## Sector 切换的完整图景

把 IngameParameter 和 CurrentLevel 结合来看 Sector 切换的完整过程：

```text
球到达 Sector 1 和 Sector 2 的边界
       |
       v
Gameplay_Events: activate Sektor
  -> IngameParameter.Activate Sector = 2
  -> IngameParameter.Deactivate Sector = 0
  -> CurrentLevel.Activation Phase? = TRUE
       |
       v
Gameplay_SectorManager: 检测到 Activation Phase? == TRUE
  -> 激活组 "Sector_02" 里的所有对象
  -> 关闭组 "Sector_00" 里的所有对象（如果编号大于 0）
  -> CurrentLevel.Activation Phase? = FALSE
       |
       v
对象可见性和物理仿真状态已更新
```

为什么 Deactivate Sector 的初始值是 0？因为游戏不会关闭 Sector_01（球的起始区域），0 表示"不关闭任何 Sector"。

## 消息系统

三个子系统之间用 Virtools 消息通信。消息是命名字符串，发送方用 `Send Message` 发出，接收方用 `Wait Message` 等待。

常见的内部消息：

| 消息 | 发送方 | 接收方 | 含义 |
| --- | --- | --- | --- |
| Ball Nav On | Gameplay_Ingame | 球导航 | 恢复球的控制 |
| Ball Nav Off | Gameplay_Ingame | 球导航 | 锁定球的控制 |
| Cam Nav On | Gameplay_Ingame | 相机导航 | 恢复相机跟随 |
| Cam Nav Off | Gameplay_Ingame | 相机导航 | 锁定相机 |

这些消息对脚本 mod 来说是不可见的。你无法拦截或发送 Virtools 内部消息。但 BML 提供了 `GameEvent` 作为脚本层的等价物，让你在关键时机（关卡开始、死亡、过关）得到通知。

## 实践：监控 Gameplay 状态面板

下面这个脚本每帧读取 IngameParameter 和 CurrentLevel，用 ImGui 显示当前 Gameplay 状态。保存为 `ModLoader/Mods/GameplayMonitor.mod.as`：

```angelscript
[bml.mod id="gameplay.monitor" name="Gameplay Monitor" version="1.0.0" author="Tutorial" bml="0.3.12" description="Monitor Gameplay state machine"]
class GameplayMonitor {
    private CKDataArray@ ingameParam = null;
    private CKDataArray@ currentLevel = null;
    private bool showWindow = true;

    void OnLoad(const BML::ModContext &in ctx) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info("GameplayMonitor loaded");
        }
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_START_LEVEL) {
            @ingameParam = ctx.BorrowDataArrayByName("IngameParameter");
            @currentLevel = ctx.BorrowDataArrayByName("CurrentLevel");

            BML::Logger@ logger = ctx.BorrowLogger();
            if (logger !is null) {
                string ipStatus = (ingameParam is null) ? "not found" : "ok";
                string clStatus = (currentLevel is null) ? "not found" : "ok";
                logger.Info("IngameParameter=" + ipStatus + " CurrentLevel=" + clStatus);
            }
        } else if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL) {
            @ingameParam = null;
            @currentLevel = null;
        }
    }

    void OnProcess(const BML::ModContext &in ctx) {
        if (!showWindow) return;
        if (ingameParam is null && currentLevel is null) return;

        ImGui::SetNextWindowPos(ImVec2(10.0f, 200.0f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_Once);

        if (ImGui::Begin("Gameplay State")) {
            DrawIngameParameter();
            ImGui::Separator();
            DrawCurrentLevel();
        }
        ImGui::End();
    }

    private void DrawIngameParameter() {
        if (ingameParam is null) {
            ImGui::TextUnformatted("IngameParameter: not loaded");
            return;
        }

        ImGui::TextUnformatted("-- IngameParameter --");

        string trafo = BML::CK::GetString(ingameParam, 0, 0, "");
        int activeSec = BML::CK::GetInt(ingameParam, 0, 1, -1);
        int deactiveSec = BML::CK::GetInt(ingameParam, 0, 2, -1);
        string tutorial = BML::CK::GetString(ingameParam, 0, 3, "?");
        string rollSound = BML::CK::GetString(ingameParam, 0, 4, "?");

        string trafoDisplay = trafo;
        if (trafo == "") {
            trafoDisplay = "(none)";
        }

        ImGui::TextUnformatted("Activetrafo: " + trafoDisplay);
        ImGui::TextUnformatted("Activate Sector: " + activeSec);
        ImGui::TextUnformatted("Deactivate Sector: " + deactiveSec);
        ImGui::TextUnformatted("Tutorial: " + tutorial);
        ImGui::TextUnformatted("RollSound: " + rollSound);
    }

    private void DrawCurrentLevel() {
        if (currentLevel is null) {
            ImGui::TextUnformatted("CurrentLevel: not loaded");
            return;
        }

        ImGui::TextUnformatted("-- CurrentLevel --");

        int levelId = BML::CK::GetInt(currentLevel, 0, 0, -1);
        string ball = BML::CK::GetString(currentLevel, 0, 1, "?");
        string resetpoint = BML::CK::GetString(currentLevel, 0, 3, "");
        string phase = BML::CK::GetString(currentLevel, 0, 4, "?");
        int points = BML::CK::GetInt(currentLevel, 0, 5, 0);

        ImGui::TextUnformatted("Level ID: " + levelId);
        ImGui::TextUnformatted("ActiveBall: " + ball);
        ImGui::TextUnformatted("Activation Phase?: " + phase);
        ImGui::TextUnformatted("Points: " + points);

        // Resetpoint 矩阵太长，只显示前 30 字符
        string rpDisplay = resetpoint;
        if (rpDisplay.length() > 30) {
            rpDisplay = rpDisplay.substr(0, 30) + "...";
        }
        ImGui::TextUnformatted("Resetpoint: " + rpDisplay);
    }
}
```

### 运行结果

进入第一关后，窗口显示：

```text
+-- Gameplay State -----------+
| -- IngameParameter --       |
| Activetrafo: (none)         |
| Activate Sector: 1          |
| Deactivate Sector: 0        |
| Tutorial: TRUE              |
| RollSound: FALSE            |
| --------------------------- |
| -- CurrentLevel --          |
| Level ID: 1                 |
| ActiveBall: Ball_Wood       |
| Activation Phase?: FALSE    |
| Points: 0                   |
| Resetpoint: 1.0 0.0 0.0... |
+-----------------------------+
```

做以下操作观察面板变化：

- 推球到第一个检查点 -> `Activate Sector` 变为 2, `Activation Phase?` 短暂变为 TRUE 然后回到 FALSE
- 经过变球器 -> `Activetrafo` 短暂出现变球器名字, `ActiveBall` 从 `Ball_Wood` 变成新球名
- 吃到加分点 -> `Points` 递增
- 掉落死亡后复活 -> `Resetpoint` 不变（复活到上次检查点的位置）

### 代码逻辑

这个脚本的结构和第 11、12 章的模式一致：

1. `GAME_EVENT_START_LEVEL` 时获取两张表的句柄
2. `OnProcess` 每帧读取并绘制 ImGui
3. `GAME_EVENT_PRE_EXIT_LEVEL` 时清空句柄

新内容在于同时读两张表，把 Gameplay 的控制状态（IngameParameter）和游戏状态（CurrentLevel）放在一起观察。当你看到 `Activate Sector` 和 `Activation Phase?` 同时变化时，就是 SectorManager 正在工作。

## DataArray 是共享黑板

这里有一个固定模式：Gameplay 的三个子系统之间，通信方式除了 Virtools 消息之外，主要就是通过 DataArray 传递状态。

```text
Gameplay_Events 写入 IngameParameter
                       |
                       v
Gameplay_SectorManager 读取 IngameParameter, 写入 CurrentLevel
                       |
                       v
Gameplay_Ingame 读取 CurrentLevel, 控制球和相机
```

DataArray 在这里扮演的角色类似"共享黑板"：多个独立运行的子系统，不直接调用彼此，而是把状态写到表格里，让其他子系统自己去读。这种设计让各子系统可以独立运行，只通过数据耦合。

对脚本 mod 而言，这意味着：

- **读取任何一张表，就能观察到对应子系统的当前状态**
- **写入某张表的某列，会改变其他子系统下一次读到的状态**
- **不需要理解行为图的内部连线，只需要知道表的列名和含义**

## 写入 DataArray 的最低规则

写入比读取危险，因为行为图也在读写同一张表。动手前先满足三个条件：

1. 确认表、行、列都存在。
2. 保存旧值，退出关卡前恢复。
3. 只改能承受回滚失败的小状态，不碰关卡加载、对象生成、检查点推进这类关键流程。

下面是一个最小模板：临时给当前分数加 100，离开关卡时恢复原值。它只演示写入模式，不建议当作正式功能。

```angelscript
private CKDataArray@ currentLevel = null;
private int oldPoints = 0;
private bool pointsChanged = false;

void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
    if (event == BML::GAME_EVENT_START_LEVEL) {
        @currentLevel = ctx.BorrowDataArrayByName("CurrentLevel");
        pointsChanged = false;
    } else if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL ||
               event == BML::GAME_EVENT_PRE_LOAD_LEVEL) {
        RestorePoints();
        @currentLevel = null;
    }
}

private void AddDebugPoints() {
    if (currentLevel is null) return;

    int col = BML::CK::FindColumn(currentLevel, "Points");
    if (col < 0) return;

    oldPoints = BML::CK::GetInt(currentLevel, 0, col, 0);
    pointsChanged = BML::CK::SetInt(currentLevel, 0, col, oldPoints + 100);
}

private void RestorePoints() {
    if (!pointsChanged || currentLevel is null) return;

    int col = BML::CK::FindColumn(currentLevel, "Points");
    if (col >= 0) {
        BML::CK::SetInt(currentLevel, 0, col, oldPoints);
    }
    pointsChanged = false;
}
```

这个例子只改 `Points`，因为它容易观察，也容易恢复。像 `ActiveBall`、`CurrentResetpoint`、`Activation Phase?` 这样的列会参与运行时流程，随便写入可能让复活、变球、小节切换出现异常。需要写入时先查 [DataArray 参考](ref-dataarray.md)，再按第 19 章的安全检查清单判断风险。

## 常见问题

**"Activate Sector 的值一直不变"**

Sector 切换只在球到达检查点时发生。如果你在第一个 Sector 里来回走，这个值不会变。推球前进到足够远的检查点。

**"Activation Phase? 总是 FALSE"**

切换发生得很快（通常在一帧内完成）。你在 ImGui 面板里几乎看不到 TRUE 的瞬间。如果想确认切换确实发生了，可以在 `OnProcess` 里加一个条件判断：当值为 TRUE 时打印日志。

**"Activetrafo 总是空的"**

变球器只在球经过变球器的瞬间短暂写入值。经过变球器后它会被清空。正常的。

**"Energy 表呢？"**

`Energy` 也是 Gameplay 使用的表，包含分数、生命和计时参数。本章聚焦 IngameParameter 和 CurrentLevel 是因为它们直接关联 Sector 切换这一核心流程。Energy 的读取方式相同，可以按第 11 章的读取流程加入面板。

**"脚本能拦截 Virtools 消息吗？"**

不能。Virtools 消息是行为图内部的通信机制，脚本层无法介入。但 BML 的 `GameEvent` 覆盖了所有你需要响应的时机。

## 实验建议

1. 在面板里加上 `Energy` 表的显示（Points, Lifes, StartLifes），观察死亡时 Lifes 怎么变化
2. 在 `OnProcess` 里检测 `Activate Sector` 的值是否和上一帧不同，如果不同就打印日志，记录完整的 Sector 切换序列
3. 把 `Activation Phase?` 的历史值记录到一个数组里，关卡结束时打印，看整关一共发生了几次 Sector 切换

---

**完成状态**：能用 ImGui 面板实时观察 Gameplay 状态机的两张核心表，理解 IngameParameter 和 CurrentLevel 在 Sector 切换中的协作方式

---

下一步：[15 行为图阅读法](tutorial-15-behavior-graph-reading.md)

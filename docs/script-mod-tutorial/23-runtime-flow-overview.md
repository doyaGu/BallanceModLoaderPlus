# 第 23 章：从 base.cmo 到一局游戏

前面已经能读对象、组和几张 DataArray。

现在把这些结果放回一局游戏流程里。

先用一条线记：

```text
base.cmo
  启动、菜单、关卡加载调度

Levelinit.nmo
  准备关卡运行时状态

Level_XX.NMO
  提供具体关卡内容

Gameplay.nmo
  关卡开始后的运行逻辑
```

这一章先讲总流程。  
不修改这些文件，也不追行为图内部每一根线。

## 前面已经拿到的线索

前面几章看到的是局部。

对象和组：

```text
PC_Checkpoints
PR_Resetpoints
PE_Levelende
PS_Levelstart
Sector_01
```

当前状态表：

```text
CurrentLevel
Level ID: 1
ActiveBall: Ball_Wood
Points: 0
```

关卡配置表：

```text
AllLevel
Level 1: Level_01.nmo start=Ball_Wood bonus=100 music=1
```

球物理参数表：

```text
Physicalize_Balls
P_Ball_Wood: friction=0.6 elasticity=0.2 mass=2 radius=2
P_Ball_Stone: friction=0.7 elasticity=0.1 mass=10 radius=2
```

这些结果不是四件互不相关的事。  
它们都出现在“进入一关并开始运行”的流程里。

## 新建脚本

这一章用一个小脚本记录流程边界。

新建：

```text
ModLoader/Mods/FlowMod.mod.as
```

写入：

```angelscript
[bml.mod id="flow.script" name="Flow Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="Observe level runtime flow"]
class FlowMod {
    private BML::CommandRef@ commandRef;
    private int eventCount = 0;

    void OnLoad(const BML::ModContext &in ctx) {
        RegisterCommand(ctx);
        LogInfo(ctx, "FlowMod loaded");
        ctx.SendIngameMessage("FlowMod loaded. Use ckflow snapshot after entering a level.");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        eventCount++;

        string eventName = BML::GetGameEventName(event);
        LogInfo(ctx, "FlowMod event " + eventCount + ": " + eventName);

        if (event == BML::GAME_EVENT_START_LEVEL) {
            SnapshotLevelState(ctx, "level-start");
        }
    }

    void OnUnload(const BML::ModContext &in ctx) {
        LogInfo(ctx, "FlowMod unloaded");
    }

    private void RegisterCommand(const BML::ModContext &in ctx) {
        BML::CommandDefinition def;
        def.Name = "ckflow";
        def.Description = "Observe level runtime flow";
        def.Usage = "ckflow snapshot";
        def.Category = "Tutorial";
        def.Enabled = true;

        BML::CommandCallback@ execute = BML::CommandCallback(this.OnFlowCommand);
        BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompleteFlowCommand);

        @commandRef = ctx.RegisterCommand(def, execute, complete);
        bool valid = commandRef !is null && commandRef.IsValid;
        LogInfo(ctx, "FlowMod command registered=" + BoolText(valid));
    }

    void OnFlowCommand(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        string action = "snapshot";
        if (event.ArgCount > 0) {
            action = event.GetArg(0);
        }

        if (action == "snapshot") {
            SnapshotLevelState(ctx, "command");
            return;
        }

        ctx.SendIngameMessage("Usage: ckflow snapshot");
        LogWarn(ctx, "FlowMod unknown command action: " + action);
    }

    void CompleteFlowCommand(const BML::ModContext &in ctx,
                             const BML::CommandEvent &in event,
                             BML::CommandCompletion &inout completions) {
        completions.Add("snapshot");
    }

    private void SnapshotLevelState(const BML::ModContext &in ctx, const string &in reason) {
        LogInfo(ctx, "FlowMod snapshot begin reason=" + reason);

        // CurrentLevel 是当前这一局的状态。
        CKDataArray@ currentLevel = ctx.BorrowDataArrayByName("CurrentLevel");
        if (currentLevel is null) {
            LogWarn(ctx, "FlowMod CurrentLevel not found");
        } else {
            LogInfo(ctx, "FlowMod CurrentLevel rows=" +
                         BML::CK::GetRowCount(currentLevel) +
                         " columns=" + BML::CK::GetColumnCount(currentLevel));
            LogInfo(ctx, "FlowMod " + ReadInt(currentLevel, "Level ID"));
            LogInfo(ctx, "FlowMod " + ReadString(currentLevel, "ActiveBall"));
        }

        // AllLevel 是关卡起始配置。
        CKDataArray@ allLevel = ctx.BorrowDataArrayByName("AllLevel");
        if (allLevel is null) {
            LogWarn(ctx, "FlowMod AllLevel not found");
        } else {
            LogInfo(ctx, "FlowMod AllLevel rows=" +
                         BML::CK::GetRowCount(allLevel) +
                         " columns=" + BML::CK::GetColumnCount(allLevel));
            LogInfo(ctx, "FlowMod AllLevel row0 " + ReadStringAt(allLevel, 0, "Levelfile") +
                         " start=" + ReadStringAt(allLevel, 0, "StartBall"));
        }

        // Physicalize_Balls 是球形物理参数来源之一。
        CKDataArray@ balls = ctx.BorrowDataArrayByName("Physicalize_Balls");
        if (balls is null) {
            LogWarn(ctx, "FlowMod Physicalize_Balls not found");
        } else {
            LogInfo(ctx, "FlowMod Physicalize_Balls rows=" +
                         BML::CK::GetRowCount(balls) +
                         " columns=" + BML::CK::GetColumnCount(balls));
        }

        LogInfo(ctx, "FlowMod snapshot end");
        ctx.SendIngameMessage("Flow snapshot finished. Check ModLoader log.");
    }

    private string ReadString(CKDataArray@ table, const string &in columnName) {
        return columnName + ": " + ReadStringAt(table, 0, columnName);
    }

    private string ReadStringAt(CKDataArray@ table, int row, const string &in columnName) {
        int column = BML::CK::FindColumn(table, columnName);
        if (column < 0) {
            return columnName + " column not found";
        }

        return BML::CK::GetString(table, row, column, "");
    }

    private string ReadInt(CKDataArray@ table, const string &in columnName) {
        int column = BML::CK::FindColumn(table, columnName);
        if (column < 0) {
            return columnName + ": column not found";
        }

        int value = BML::CK::GetInt(table, 0, column, 0);
        return columnName + ": " + value;
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

## 这份脚本看什么

`OnGameEvent` 记录游戏事件：

```angelscript
void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
    eventCount++;

    string eventName = BML::GetGameEventName(event);
    LogInfo(ctx, "FlowMod event " + eventCount + ": " + eventName);

    if (event == BML::GAME_EVENT_START_LEVEL) {
        SnapshotLevelState(ctx, "level-start");
    }
}
```

它只能看到 BML 暴露出来的流程边界。  
它看不到 `base.cmo` 里每个行为图节点怎么执行。

这一点很重要：

```text
BML 游戏事件    适合判断脚本什么时候能观察
Virtools 行为图  负责原版流程内部怎么跑
```

第 26 章以后再讲行为图阅读法。

## 从菜单到关卡开始

启动 Player 后，脚本会先加载：

```text
FlowMod command registered=true
FlowMod loaded
```

进入第一关时，日志通常会出现这组事件：

```text
FlowMod event 1: GAME_EVENT_PRE_START_MENU
FlowMod event 2: GAME_EVENT_POST_START_MENU
FlowMod event 3: GAME_EVENT_PRE_LOAD_LEVEL
FlowMod event 4: GAME_EVENT_POST_LOAD_LEVEL
FlowMod event 5: GAME_EVENT_START_LEVEL
FlowMod event 6: GAME_EVENT_CAM_NAV_ACTIVE
FlowMod event 7: GAME_EVENT_BALL_NAV_ACTIVE
FlowMod event 8: GAME_EVENT_COUNTER_ACTIVE
```

先把它们理解成：

| 事件 | 说明 |
| --- | --- |
| `GAME_EVENT_PRE_START_MENU` | 准备进入开始菜单 |
| `GAME_EVENT_POST_START_MENU` | 开始菜单已经进入 |
| `GAME_EVENT_PRE_LOAD_LEVEL` | 准备加载关卡 |
| `GAME_EVENT_POST_LOAD_LEVEL` | 关卡加载完成一段 |
| `GAME_EVENT_START_LEVEL` | 关卡开始运行 |
| `GAME_EVENT_CAM_NAV_ACTIVE` | 相机控制激活 |
| `GAME_EVENT_BALL_NAV_ACTIVE` | 球控制激活 |
| `GAME_EVENT_COUNTER_ACTIVE` | 计时/计数流程激活 |

脚本在 `GAME_EVENT_START_LEVEL` 做快照，因为这时前面几章读过的表已经可用。
从实测顺序看，相机控制、球控制和计数流程还会继续在后面激活。

## 快照里有什么

`SnapshotLevelState` 只读三张表的摘要。

第一张是 `CurrentLevel`：

```text
FlowMod CurrentLevel rows=1 columns=6
FlowMod Level ID: 1
FlowMod ActiveBall: Ball_Wood
```

它说明当前这一局已经是第一关，当前活动球是木球。

第二张是 `AllLevel`：

```text
FlowMod AllLevel rows=13 columns=8
FlowMod AllLevel row0 Level_01.nmo start=Ball_Wood
```

它说明 13 关的起始配置表已经存在，第 0 行对应第一关。

第三张是 `Physicalize_Balls`：

```text
FlowMod Physicalize_Balls rows=2 columns=12
```

它说明球形物理参数表已经存在。  
本章只看表是否在流程里出现，完整参数前一章已经读过。

## 四个文件各自负责什么

现在把流程放回文件分工。

| 名字 | 先这样理解 | 这一章看到的证据 |
| --- | --- | --- |
| `base.cmo` | 启动、菜单、关卡加载调度 | 收到菜单、加载、开始关卡事件 |
| `Levelinit.nmo` | 准备运行时状态 | `AllLevel`、`Physicalize_Balls` 已经可读 |
| `Level_XX.NMO` | 提供具体关卡内容 | 前面能找到第一关对象和组 |
| `Gameplay.nmo` | 关卡开始后的运行逻辑 | `CurrentLevel` 进入可观察状态 |

可以先按这个顺序理解一局游戏：

```text
Player 启动
base.cmo 进入菜单
选择关卡
base.cmo 发起关卡加载
Levelinit.nmo 准备运行时状态
Level_XX.NMO 提供关卡内容
Gameplay.nmo 开始接管运行
相机、球控制、计时/计数流程陆续激活
```

真实流程会交叉配合。  
这张图只用来建立第一版心智模型。

## 为什么要先等 START_LEVEL

前面读对象、组和 DataArray 时，基本都等到：

```angelscript
if (event == BML::GAME_EVENT_START_LEVEL) {
    ...
}
```

原因就是这一章看到的流程顺序。

在 `OnLoad` 里，脚本只说明自己加载了。  
在开始菜单阶段，关卡内容还没准备好。  
在关卡加载过程中，一部分对象和表可能还在准备。

`GAME_EVENT_START_LEVEL` 不是所有内部逻辑的终点，但它是入门阶段观察关卡内容的稳妥起点。

## 对写脚本的意义

写脚本时，先判断自己碰的是哪一层：

| 想做的事 | 更接近哪一层 |
| --- | --- |
| 显示窗口、注册命令、Timer | BML |
| 按名字找对象和组 | 关卡内容 / Virtools 对象 |
| 读 `AllLevel` | 初始化配置 |
| 读 `CurrentLevel` | 当前运行状态 |
| 看物理参数 | 物理化参数来源 |
| 改球、复活点、检查点流程 | 原版运行逻辑 |

现在仍然只做观察：

```text
记录事件
找对象
读表
写日志
```

修改放到后面。  
先知道原版流程把东西放在哪里，再决定从哪个点下手。

## 本章结果

现在可以把前几章串起来：

```text
事件顺序说明游戏从菜单进入关卡
对象和组说明关卡内容已经进入运行时
AllLevel 说明关卡起始配置已经可读
Physicalize_Balls 说明物理参数来源已经可读
CurrentLevel 说明当前这一局的状态已经可读
```

下一章看 `Levelinit.nmo` 如何处理作者放在关卡里的对象和组。

# 第 25 章：Gameplay 如何维持运行中状态

第 24 章看的是 `Levelinit`。

`Levelinit` 把关卡内容整理好，让一局游戏可以开始。  
这一章看 `Gameplay`，也就是关卡开始后的持续运行。

先把它理解成一套运行中的控制流程：

```text
球控制
相机控制
小节切换
教程提示
滚动声音
分数和生命
检查点、复活、过关
```

这些状态不会只放在某一个 3D 对象里。  
原版流程会用 DataArray 保存和传递状态。

这一章看三张表：

```text
IngameParameter
Energy
CurrentLevel
```

## Gameplay 和 Levelinit 的区别

先把第 24 章和这一章分开：

| 阶段 | 先这样理解 |
| --- | --- |
| `Levelinit` | 开局前后，把关卡内容整理成运行时状态 |
| `Gameplay` | 开局之后，持续维护玩法状态 |

第 24 章看到：

```text
PH_Groups
Checkpoints
ResetPoints
```

这一章看到：

```text
IngameParameter
Energy
CurrentLevel
```

`CurrentLevel` 在第 20 章已经读过。  
这里再读一次，是为了把它和 `Gameplay` 放在一起看。

## 新建脚本

新建：

```text
ModLoader/Mods/PlayMod.mod.as
```

写入：

```angelscript
[bml.mod id="play.script" name="Play Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="Observe Gameplay runtime tables"]
class PlayMod {
    private BML::CommandRef@ commandRef;

    void OnLoad(const BML::ModContext &in ctx) {
        RegisterCommand(ctx);
        LogInfo(ctx, "PlayMod loaded");
        ctx.SendIngameMessage("PlayMod loaded. Use ckplay state after entering a level.");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        string eventName = BML::GetGameEventName(event);
        LogInfo(ctx, "PlayMod event " + eventName);

        if (event == BML::GAME_EVENT_START_LEVEL ||
            event == BML::GAME_EVENT_EXTRA_POINT ||
            event == BML::GAME_EVENT_DEAD) {
            ScanGameplayState(ctx, eventName);
        }
    }

    void OnUnload(const BML::ModContext &in ctx) {
        LogInfo(ctx, "PlayMod unloaded");
    }

    private void RegisterCommand(const BML::ModContext &in ctx) {
        BML::CommandDefinition def;
        def.Name = "ckplay";
        def.Description = "Observe Gameplay runtime state";
        def.Usage = "ckplay state";
        def.Category = "Tutorial";
        def.Enabled = true;

        BML::CommandCallback@ execute = BML::CommandCallback(this.OnPlayCommand);
        BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompletePlayCommand);

        @commandRef = ctx.RegisterCommand(def, execute, complete);
        bool valid = commandRef !is null && commandRef.IsValid;
        LogInfo(ctx, "PlayMod command registered=" + BoolText(valid));
    }

    void OnPlayCommand(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        string action = "state";
        if (event.ArgCount > 0) {
            action = event.GetArg(0);
        }

        if (action == "state") {
            ScanGameplayState(ctx, "command");
            return;
        }

        ctx.SendIngameMessage("Usage: ckplay state");
        LogWarn(ctx, "PlayMod unknown command action: " + action);
    }

    void CompletePlayCommand(const BML::ModContext &in ctx,
                             const BML::CommandEvent &in event,
                             BML::CommandCompletion &inout completions) {
        completions.Add("state");
    }

    private void ScanGameplayState(const BML::ModContext &in ctx, const string &in reason) {
        LogInfo(ctx, "PlayMod scan begin reason=" + reason);

        ScanIngameParameter(ctx);
        ScanEnergy(ctx);
        ScanCurrentLevel(ctx);

        LogInfo(ctx, "PlayMod scan end");
        ctx.SendIngameMessage("Gameplay state scan finished. Check ModLoader log.");
    }

    private void ScanIngameParameter(const BML::ModContext &in ctx) {
        CKDataArray@ table = ctx.BorrowDataArrayByName("IngameParameter");
        if (table is null) {
            LogWarn(ctx, "PlayMod IngameParameter not found");
            return;
        }

        LogInfo(ctx, "PlayMod IngameParameter rows=" +
                     BML::CK::GetRowCount(table) +
                     " columns=" + BML::CK::GetColumnCount(table));

        LogInfo(ctx, "PlayMod " + ReadString(table, "IngameParameter", "Activetrafo"));
        LogInfo(ctx, "PlayMod " + ReadInt(table, "IngameParameter", "Activate Sector"));
        LogInfo(ctx, "PlayMod " + ReadInt(table, "IngameParameter", "Deactivate Sector"));
        LogInfo(ctx, "PlayMod " + ReadString(table, "IngameParameter", "Tutorial activ?"));
        LogInfo(ctx, "PlayMod " + ReadString(table, "IngameParameter", "RollSound activate?"));
    }

    private void ScanEnergy(const BML::ModContext &in ctx) {
        CKDataArray@ table = ctx.BorrowDataArrayByName("Energy");
        if (table is null) {
            LogWarn(ctx, "PlayMod Energy not found");
            return;
        }

        LogInfo(ctx, "PlayMod Energy rows=" +
                     BML::CK::GetRowCount(table) +
                     " columns=" + BML::CK::GetColumnCount(table));

        LogInfo(ctx, "PlayMod " + ReadInt(table, "Energy", "Points"));
        LogInfo(ctx, "PlayMod " + ReadInt(table, "Energy", "Lifes"));
        LogInfo(ctx, "PlayMod " + ReadInt(table, "Energy", "StartPoints"));
        LogInfo(ctx, "PlayMod " + ReadInt(table, "Energy", "StartLifes"));
        LogInfo(ctx, "PlayMod " + ReadString(table, "Energy", "Timefactor"));
        LogInfo(ctx, "PlayMod " + ReadInt(table, "Energy", "LifeBonus"));
    }

    private void ScanCurrentLevel(const BML::ModContext &in ctx) {
        CKDataArray@ table = ctx.BorrowDataArrayByName("CurrentLevel");
        if (table is null) {
            LogWarn(ctx, "PlayMod CurrentLevel not found");
            return;
        }

        LogInfo(ctx, "PlayMod CurrentLevel rows=" +
                     BML::CK::GetRowCount(table) +
                     " columns=" + BML::CK::GetColumnCount(table));

        LogInfo(ctx, "PlayMod " + ReadInt(table, "CurrentLevel", "Level ID"));
        LogInfo(ctx, "PlayMod " + ReadString(table, "CurrentLevel", "ActiveBall"));
        LogInfo(ctx, "PlayMod " + ReadString(table, "CurrentLevel", "Activation Phase?"));
        LogInfo(ctx, "PlayMod " + ReadInt(table, "CurrentLevel", "Points"));
    }

    private string ReadInt(CKDataArray@ table, const string &in tableName, const string &in columnName) {
        int column = BML::CK::FindColumn(table, columnName);
        if (column < 0) {
            return tableName + "." + columnName + ": column not found";
        }

        int value = BML::CK::GetInt(table, 0, column, 0);
        return tableName + "." + columnName + ": " + value;
    }

    private string ReadString(CKDataArray@ table, const string &in tableName, const string &in columnName) {
        int column = BML::CK::FindColumn(table, columnName);
        if (column < 0) {
            return tableName + "." + columnName + ": column not found";
        }

        string value = BML::CK::GetString(table, 0, column, "");
        return tableName + "." + columnName + ": " + value;
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

## 这次为什么监听三个事件

脚本在三个事件里重新读表：

```angelscript
if (event == BML::GAME_EVENT_START_LEVEL ||
    event == BML::GAME_EVENT_EXTRA_POINT ||
    event == BML::GAME_EVENT_DEAD) {
    ScanGameplayState(ctx, eventName);
}
```

`GAME_EVENT_START_LEVEL` 用来记录开局状态。  
`GAME_EVENT_EXTRA_POINT` 和 `GAME_EVENT_DEAD` 是后面观察分数、生命、死亡流程时常用的时机。

本章先记录进入关卡后的开局状态。  
后两个事件作为后续观察分数和死亡流程的入口，先放在脚本里。

## IngameParameter

`IngameParameter` 保存运行中的开关和小节相关编号。

进入第一关后，实测结果是：

```text
PlayMod IngameParameter rows=1 columns=5
PlayMod IngameParameter.Activetrafo: 
PlayMod IngameParameter.Activate Sector: 1
PlayMod IngameParameter.Deactivate Sector: 0
PlayMod IngameParameter.Tutorial activ?: TRUE
PlayMod IngameParameter.RollSound activate?: FALSE
```

先这样理解：

| 列 | 含义 |
| --- | --- |
| `Activetrafo` | 当前变球器相关状态 |
| `Activate Sector` | 当前要激活的小节编号 |
| `Deactivate Sector` | 当前要关闭的小节编号 |
| `Tutorial activ?` | 教程提示相关开关 |
| `RollSound activate?` | 滚动声音相关开关 |

列名里的 `Sector` 是原版名字。  
正文里按“关卡小节”理解即可。

## Energy

`Energy` 保存分数、生命和奖励参数。

进入第一关后，实测结果是：

```text
PlayMod Energy rows=1 columns=6
PlayMod Energy.Points: 0
PlayMod Energy.Lifes: 0
PlayMod Energy.StartPoints: 1000
PlayMod Energy.StartLifes: 3
PlayMod Energy.Timefactor: 00m 00s 500ms
PlayMod Energy.LifeBonus: 200
```

先这样理解：

| 列 | 含义 |
| --- | --- |
| `Points` | 当前分数相关值 |
| `Lifes` | 当前生命显示相关值 |
| `StartPoints` | 开局基础分 |
| `StartLifes` | 开局生命数 |
| `Timefactor` | 计时或分数计算相关参数 |
| `LifeBonus` | 生命奖励分 |

`Points` 在 `CurrentLevel` 里也出现过。  
同一种玩法概念可能在多张表里被不同流程使用。

现在只观察，不写入。

## CurrentLevel 再看一次

这章也读 `CurrentLevel`：

```text
PlayMod CurrentLevel rows=1 columns=6
PlayMod CurrentLevel.Level ID: 1
PlayMod CurrentLevel.ActiveBall: Ball_Wood
PlayMod CurrentLevel.Activation Phase?: FALSE
PlayMod CurrentLevel.Points: 0
```

这里不是重复第 20 章。  
它的作用是把当前关卡状态和 `IngameParameter`、`Energy` 放在同一个时机观察。

一局游戏开始后，几个表会一起参与运行：

```text
CurrentLevel      当前关卡和当前球
IngameParameter   运行中的开关和小节编号
Energy            分数、生命和奖励参数
```

## BML 游戏事件和 Virtools 消息

这一章容易混淆两件事：

```text
BML::GameEvent
Virtools 行为图里的 Send Message / Wait Message
```

`BML::GameEvent` 是 BML 给脚本 mod 的回调事件。  
脚本用它判断什么时候观察。

例如：

```angelscript
void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
}
```

`Send Message` / `Wait Message` 是 Virtools 行为图里的通信方式。  
它属于原版行为图内部流程。

这章只用 BML 事件做观察入口。  
后面读行为图时，再看 Virtools 消息如何连接 `Gameplay_Ingame`、`Gameplay_Events`、`Gameplay_SectorManager` 这些流程。

## 和前几章放在一起

到这里已经看到几类运行时表：

| 表 | 主要用途 |
| --- | --- |
| `AllLevel` | 关卡起始配置 |
| `Physicalize_Balls` | 木球、石球物理参数 |
| `PH_Groups` | 作者标记分类 |
| `Checkpoints` | 检查点运行时表 |
| `ResetPoints` | 复活点运行时表 |
| `CurrentLevel` | 当前这一局的状态 |
| `IngameParameter` | 运行中的开关和小节编号 |
| `Energy` | 分数、生命和奖励参数 |

可以先这样记：

```text
Levelinit 准备一局游戏
Gameplay 维持一局游戏
DataArray 是它们共享状态的地方
脚本 mod 先从读取这些表开始理解原版流程
```

## 本章结果

现在可以把第二篇后半段连起来：

```text
第 23 章：看总流程
第 24 章：看 Levelinit 整理作者标记
第 25 章：看 Gameplay 维护运行中状态
```

下一章开始进入行为图和物理。

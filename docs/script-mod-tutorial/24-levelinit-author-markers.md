# 第 24 章：Levelinit 如何处理作者标记

上一章把总流程串起来了：

```text
base.cmo 负责调度
Level_XX.NMO 提供关卡内容
Levelinit.nmo 准备运行时状态
Gameplay.nmo 负责关卡开始后的运行
```

这一章看 `Levelinit.nmo` 的一类工作：把关卡作者放在内容文件里的对象和组，整理成运行时表。

先用检查点和复活点理解：

```text
关卡内容里有检查点组、复活点组
Levelinit 读取这些组
整理出 Checkpoints、ResetPoints 等运行时表
Gameplay 后面再使用这些表
```

这一章仍然只查看，不修改。

## 作者组和运行时表

第 14 章讲过组：

```text
PC_Checkpoints   检查点对象名单
PR_Resetpoints   复活点对象名单
```

这些组来自关卡内容。  
它们更像“作者放在地图里的分类名单”。

`PH_Groups` 里还会出现更多分类名，例如：

```text
P_Extra_Point
P_Extra_Life
P_Ball_Wood
P_Box
```

这些名字在表里有意义。  
不要直接推断每个名字在当前时机都能用 `BorrowGroupByName` 拿到同名 `CKGroup`。

进入关卡后，还能读到几张运行时表：

```text
PH_Groups
Checkpoints
ResetPoints
```

它们不是同一个层次。

| 名字 | 更像什么 |
| --- | --- |
| `PC_Checkpoints` | 关卡内容里的对象组 |
| `Checkpoints` | Levelinit 整理出的检查点表 |
| `PR_Resetpoints` | 关卡内容里的对象组 |
| `ResetPoints` | Levelinit 整理出的复活点表 |
| `PH_Groups` | 可物理化对象分类表 |

注意大小写：表名是 `ResetPoints`，不是 `Resetpoints`。

## 新建脚本

新建：

```text
ModLoader/Mods/InitMod.mod.as
```

写入：

```angelscript
[bml.mod id="init.script" name="Init Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="Observe Levelinit runtime tables"]
class InitMod {
    private BML::CommandRef@ commandRef;

    void OnLoad(const BML::ModContext &in ctx) {
        RegisterCommand(ctx);
        LogInfo(ctx, "InitMod loaded");
        ctx.SendIngameMessage("InitMod loaded. Use ckinit scan after entering a level.");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_START_LEVEL) {
            ScanLevelinitState(ctx, "level-start");
        }
    }

    void OnUnload(const BML::ModContext &in ctx) {
        LogInfo(ctx, "InitMod unloaded");
    }

    private void RegisterCommand(const BML::ModContext &in ctx) {
        BML::CommandDefinition def;
        def.Name = "ckinit";
        def.Description = "Observe Levelinit tables";
        def.Usage = "ckinit scan";
        def.Category = "Tutorial";
        def.Enabled = true;

        BML::CommandCallback@ execute = BML::CommandCallback(this.OnInitCommand);
        BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompleteInitCommand);

        @commandRef = ctx.RegisterCommand(def, execute, complete);
        bool valid = commandRef !is null && commandRef.IsValid;
        LogInfo(ctx, "InitMod command registered=" + BoolText(valid));
    }

    void OnInitCommand(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        string action = "scan";
        if (event.ArgCount > 0) {
            action = event.GetArg(0);
        }

        if (action == "scan") {
            ScanLevelinitState(ctx, "command");
            return;
        }

        ctx.SendIngameMessage("Usage: ckinit scan");
        LogWarn(ctx, "InitMod unknown command action: " + action);
    }

    void CompleteInitCommand(const BML::ModContext &in ctx,
                             const BML::CommandEvent &in event,
                             BML::CommandCompletion &inout completions) {
        completions.Add("scan");
    }

    private void ScanLevelinitState(const BML::ModContext &in ctx, const string &in reason) {
        LogInfo(ctx, "InitMod scan begin reason=" + reason);

        // 这些是关卡内容里的作者组。
        DescribeGroup(ctx, "PC_Checkpoints");
        DescribeGroup(ctx, "PR_Resetpoints");

        // 这些是 Levelinit 整理后的运行时表。
        ScanPHGroups(ctx);
        ScanCheckpoints(ctx);
        ScanResetPoints(ctx);

        LogInfo(ctx, "InitMod scan end");
        ctx.SendIngameMessage("Levelinit scan finished. Check ModLoader log.");
    }

    private void DescribeGroup(const BML::ModContext &in ctx, const string &in name) {
        CKGroup@ group = ctx.BorrowGroupByName(name);
        if (group is null) {
            LogWarn(ctx, "InitMod group not found: " + name);
            return;
        }

        LogInfo(ctx, "InitMod group " + name +
                     " id=" + BML::CK::GetId(group) +
                     " class=" + BML::CK::GetClassId(group));
    }

    private void ScanPHGroups(const BML::ModContext &in ctx) {
        CKDataArray@ table = ctx.BorrowDataArrayByName("PH_Groups");
        if (table is null) {
            LogWarn(ctx, "InitMod PH_Groups not found");
            return;
        }

        int rows = BML::CK::GetRowCount(table);
        int columns = BML::CK::GetColumnCount(table);
        LogInfo(ctx, "InitMod PH_Groups rows=" + rows + " columns=" + columns);

        int nameColumn = BML::CK::FindColumn(table, "Group Names");
        int amountColumn = BML::CK::FindColumn(table, "Amount");
        int activationColumn = BML::CK::FindColumn(table, "Activation");
        int resetColumn = BML::CK::FindColumn(table, "Reset");

        if (nameColumn < 0 || amountColumn < 0 || activationColumn < 0 || resetColumn < 0) {
            LogWarn(ctx, "InitMod PH_Groups required column not found");
            return;
        }

        for (int row = 0; row < rows; row++) {
            string groupName = BML::CK::GetString(table, row, nameColumn, "");
            if (ShouldShowPHGroup(groupName)) {
                LogInfo(ctx, "InitMod " + DescribePHGroupRow(table, row,
                    nameColumn, amountColumn, activationColumn, resetColumn));
            }
        }
    }

    private bool ShouldShowPHGroup(const string &in name) {
        return name == "P_Extra_Life" ||
            name == "P_Extra_Point" ||
            name == "P_Trafo_Stone" ||
            name == "P_Trafo_Wood" ||
            name == "P_Ball_Stone" ||
            name == "P_Ball_Wood" ||
            name == "P_Box";
    }

    private string DescribePHGroupRow(CKDataArray@ table, int row,
                                      int nameColumn, int amountColumn,
                                      int activationColumn, int resetColumn) {
        string name = BML::CK::GetString(table, row, nameColumn, "");
        int amount = BML::CK::GetInt(table, row, amountColumn, 0);
        int activation = BML::CK::GetInt(table, row, activationColumn, 0);
        int reset = BML::CK::GetInt(table, row, resetColumn, 0);

        return "PH_Groups " + name +
            ": amount=" + amount +
            " activation=" + activation +
            " reset=" + reset;
    }

    private void ScanCheckpoints(const BML::ModContext &in ctx) {
        CKDataArray@ table = ctx.BorrowDataArrayByName("Checkpoints");
        if (table is null) {
            LogWarn(ctx, "InitMod Checkpoints not found");
            return;
        }

        int rows = BML::CK::GetRowCount(table);
        int columns = BML::CK::GetColumnCount(table);
        LogInfo(ctx, "InitMod Checkpoints rows=" + rows + " columns=" + columns);

        int matrixColumn = BML::CK::FindColumn(table, "Matrix");
        int objectColumn = BML::CK::FindColumn(table, "Object");
        if (matrixColumn < 0 || objectColumn < 0) {
            LogWarn(ctx, "InitMod Checkpoints required column not found");
            return;
        }

        for (int row = 0; row < rows; row++) {
            string objectName = BML::CK::GetString(table, row, objectColumn, "");
            string matrix = BML::CK::GetString(table, row, matrixColumn, "");
            LogInfo(ctx, "InitMod Checkpoints row=" + row +
                         " object=" + objectName +
                         " matrix=" + matrix);
        }
    }

    private void ScanResetPoints(const BML::ModContext &in ctx) {
        CKDataArray@ table = ctx.BorrowDataArrayByName("ResetPoints");
        if (table is null) {
            LogWarn(ctx, "InitMod ResetPoints not found");
            return;
        }

        int rows = BML::CK::GetRowCount(table);
        int columns = BML::CK::GetColumnCount(table);
        LogInfo(ctx, "InitMod ResetPoints rows=" + rows + " columns=" + columns);

        int resetPointColumn = BML::CK::FindColumn(table, "Resetpoint");
        if (resetPointColumn < 0) {
            LogWarn(ctx, "InitMod ResetPoints required column not found");
            return;
        }

        for (int row = 0; row < rows; row++) {
            string matrix = BML::CK::GetString(table, row, resetPointColumn, "");
            LogInfo(ctx, "InitMod ResetPoints row=" + row +
                         " resetpoint=" + matrix);
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

## 脚本分成两类观察

第一类是看作者组是否存在：

```angelscript
DescribeGroup(ctx, "PC_Checkpoints");
DescribeGroup(ctx, "PR_Resetpoints");
```

这些名字属于关卡内容层。  
脚本只记录它们能不能被运行时找到。

第二类是看运行时表：

```angelscript
ScanPHGroups(ctx);
ScanCheckpoints(ctx);
ScanResetPoints(ctx);
```

这些表已经是 `Levelinit` 整理后的结果。

## PH_Groups

`PH_Groups` 有 4 列：

```text
Group Names
Amount
Activation
Reset
```

进入第一关后，实测结果是：

```text
InitMod PH_Groups rows=23 columns=4
InitMod PH_Groups P_Extra_Life: amount=1 activation=1 reset=1
InitMod PH_Groups P_Extra_Point: amount=4 activation=1 reset=1
InitMod PH_Groups P_Trafo_Stone: amount=1 activation=1 reset=0
InitMod PH_Groups P_Trafo_Wood: amount=1 activation=1 reset=0
InitMod PH_Groups P_Ball_Stone: amount=5 activation=3 reset=2
InitMod PH_Groups P_Ball_Wood: amount=2 activation=3 reset=2
InitMod PH_Groups P_Box: amount=3 activation=2 reset=2
```

先这样理解四列：

| 列 | 含义 |
| --- | --- |
| `Group Names` | 关卡内容里的组名 |
| `Amount` | 当前关卡里这一类对象数量 |
| `Activation` | 激活分类编号 |
| `Reset` | 复位分类编号 |

`Amount` 会随关卡变化。  
例如第一关有 4 个 `P_Extra_Point`，另一关可能不同。

`Activation` 和 `Reset` 现在先当分类编号。  
后面看行为图时，再展开这些编号如何参与激活和复位流程。

这里再强调一次：`PH_Groups` 的 `Group Names` 是分类名。  
它和 `BorrowGroupByName` 查到的 `CKGroup` 有关系，但不能简单画等号。

## Checkpoints

`Checkpoints` 是检查点运行时表。

进入第一关后，实测结果是：

```text
InitMod Checkpoints rows=3 columns=2
InitMod Checkpoints row=0 object=PC_TwoFlames_MF matrix=[-1,0,0,0][0,1,0,0][0,0,-1,0][-18.6501,11.6332,-379.104,1]
InitMod Checkpoints row=1 object=PC_TwoFlames_MF001 matrix=[0,0,1,0][0,1,0,0][-1,0,0,0][147.433,-1.63728,-710.489,1]
InitMod Checkpoints row=2 object=PC_TwoFlames_MF matrix=[1,0,0,0][0,1,0,0][0,0,1,0][341.96,-7.39847,-830.591,1]
```

它有两列：

| 列 | 含义 |
| --- | --- |
| `Matrix` | 检查点位置和方向 |
| `Object` | 检查点相关运行时对象 |

这里能看到一个转变：

```text
PC_Checkpoints 组
  ↓
Checkpoints 表
```

组回答“哪些对象属于检查点”。  
表回答“这些检查点在运行时按什么顺序、位置和对象使用”。

## ResetPoints

`ResetPoints` 是复活点运行时表。

进入第一关后，实测结果是：

```text
InitMod ResetPoints rows=4 columns=1
InitMod ResetPoints row=0 resetpoint=[-1,0,0,0][0,1,0,0][0,0,-1,0][54.0956,17.6565,-153.072,1]
InitMod ResetPoints row=1 resetpoint=[-1,0,0,0][0,1,0,0][0,0,-1,0][-18.6501,14.959,-379.104,1]
InitMod ResetPoints row=2 resetpoint=[0,0,1,0][0,1,0,0][-1,0,0,0][147.433,1.68852,-710.489,1]
InitMod ResetPoints row=3 resetpoint=[-1,0,0,0][0,1,0,0][0,0,-1,0][341.96,-4.07268,-830.591,1]
```

它只有一列：

| 列 | 含义 |
| --- | --- |
| `Resetpoint` | 复活点位置和方向 |

这里也能看到同样的转变：

```text
PR_Resetpoints 组
  ↓
ResetPoints 表
```

`CurrentLevel` 里的 `CurrentResetpoint` 会保存当前复活点矩阵。  
第 20 章读到的值，就来自这一类流程。

## 大小写要准确

脚本里要写：

```angelscript
ctx.BorrowDataArrayByName("ResetPoints");
```

不要写成：

```angelscript
ctx.BorrowDataArrayByName("Resetpoints");
```

后者在本章实测中拿不到表。

DataArray 名字和列名都按运行时实际名字写。  
不确定时，先用日志打印列名，不要猜。

## 本章结果

现在可以把 `Levelinit` 的一部分工作看清楚：

```text
关卡内容提供对象和组
Levelinit 读取这些作者组
PH_Groups 记录可物理化对象分类
Checkpoints 记录检查点位置和对象
ResetPoints 记录复活点位置
Gameplay 后面继续使用这些表
```

下一章看 `Gameplay` 如何维持运行中的状态。

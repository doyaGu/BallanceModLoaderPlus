# 第 22 章：物理参数表 Physicalize_Balls

前两章读了两张表：

| 表名 | 作用 |
| --- | --- |
| `CurrentLevel` | 当前这一局的状态 |
| `AllLevel` | 13 个关卡的起始配置 |

这一章继续读 DataArray，不过目标换成物理参数：

```text
Physicalize_Balls
```

它记录木球和石球被物理化时会用到的一部分参数。

这章先看 4 个最直观的值：

| 列名 | 先这样理解 |
| --- | --- |
| `Friction` | 摩擦 |
| `Elasticity` | 弹性 |
| `Mass` | 质量 |
| `Radius` | 球形碰撞半径 |

这些值能解释一个直观问题：

```text
木球和石球不只模型不同，物理参数也不同。
```

## 参数来源和运行中对象

先把边界说清楚。

`Physicalize_Balls` 是参数来源之一。  
表里有数值，不代表当前场景里已经存在对应的物理对象。

一般流程更接近这样：

```text
关卡对象出现
原版流程判断它属于哪类物理对象
行为图读取 Physicalize_Balls
Physicalize 相关 Building Block 创建物理对象
运行中的球才开始受物理系统控制
```

所以本章只读表。  
后面讲 `OnPhysicalize` 时，再看运行中的物理事件。

## 新建脚本

新建：

```text
ModLoader/Mods/PhysMod.mod.as
```

写入：

```angelscript
[bml.mod id="phys.script" name="Phys Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="Read Physicalize_Balls DataArray"]
class PhysMod {
    private BML::CommandRef@ commandRef;

    void OnLoad(const BML::ModContext &in ctx) {
        RegisterCommand(ctx);
        LogInfo(ctx, "PhysMod loaded");
        ctx.SendIngameMessage("PhysMod loaded. Use ckphys balls after entering a level.");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_START_LEVEL) {
            ScanPhysicalizeBalls(ctx, "level-start");
        }
    }

    void OnUnload(const BML::ModContext &in ctx) {
        LogInfo(ctx, "PhysMod unloaded");
    }

    private void RegisterCommand(const BML::ModContext &in ctx) {
        BML::CommandDefinition def;
        def.Name = "ckphys";
        def.Description = "Read physics parameter tables";
        def.Usage = "ckphys balls";
        def.Category = "Tutorial";
        def.Enabled = true;

        BML::CommandCallback@ execute = BML::CommandCallback(this.OnPhysCommand);
        BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompletePhysCommand);

        @commandRef = ctx.RegisterCommand(def, execute, complete);
        bool valid = commandRef !is null && commandRef.IsValid;
        LogInfo(ctx, "PhysMod command registered=" + BoolText(valid));
    }

    void OnPhysCommand(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        string action = "balls";
        if (event.ArgCount > 0) {
            action = event.GetArg(0);
        }

        if (action == "balls") {
            ScanPhysicalizeBalls(ctx, "command");
            return;
        }

        ctx.SendIngameMessage("Usage: ckphys balls");
        LogWarn(ctx, "PhysMod unknown command action: " + action);
    }

    void CompletePhysCommand(const BML::ModContext &in ctx,
                             const BML::CommandEvent &in event,
                             BML::CommandCompletion &inout completions) {
        completions.Add("balls");
    }

    private void ScanPhysicalizeBalls(const BML::ModContext &in ctx, const string &in reason) {
        LogInfo(ctx, "PhysMod scan begin reason=" + reason);

        // Physicalize_Balls 是球形物理参数表。
        CKDataArray@ table = ctx.BorrowDataArrayByName("Physicalize_Balls");
        if (table is null) {
            LogWarn(ctx, "PhysMod Physicalize_Balls not found");
            ctx.SendIngameMessage("Physicalize_Balls not found.");
            return;
        }

        int rows = BML::CK::GetRowCount(table);
        int columns = BML::CK::GetColumnCount(table);
        LogInfo(ctx, "PhysMod Physicalize_Balls rows=" + rows + " columns=" + columns);

        int nameColumn = BML::CK::FindColumn(table, "Group Name");
        int frictionColumn = BML::CK::FindColumn(table, "Friction");
        int elasticityColumn = BML::CK::FindColumn(table, "Elasticity");
        int massColumn = BML::CK::FindColumn(table, "Mass");
        int radiusColumn = BML::CK::FindColumn(table, "Radius");

        if (nameColumn < 0 || frictionColumn < 0 || elasticityColumn < 0 ||
            massColumn < 0 || radiusColumn < 0) {
            LogWarn(ctx, "PhysMod required Physicalize_Balls column not found");
            ctx.SendIngameMessage("Physicalize_Balls column not found.");
            return;
        }

        for (int row = 0; row < rows; row++) {
            LogInfo(ctx, "PhysMod " + DescribeBallPhysicsRow(table, row,
                nameColumn, frictionColumn, elasticityColumn, massColumn, radiusColumn));
        }

        LogInfo(ctx, "PhysMod scan end");
        ctx.SendIngameMessage("Physicalize_Balls scan finished. Check ModLoader log.");
    }

    private string DescribeBallPhysicsRow(CKDataArray@ table, int row,
                                          int nameColumn, int frictionColumn,
                                          int elasticityColumn, int massColumn,
                                          int radiusColumn) {
        string name = BML::CK::GetString(table, row, nameColumn, "");
        float friction = BML::CK::GetFloat(table, row, frictionColumn, 0.0f);
        float elasticity = BML::CK::GetFloat(table, row, elasticityColumn, 0.0f);
        float mass = BML::CK::GetFloat(table, row, massColumn, 0.0f);
        float radius = BML::CK::GetFloat(table, row, radiusColumn, 0.0f);

        return name +
            ": friction=" + friction +
            " elasticity=" + elasticity +
            " mass=" + mass +
            " radius=" + radius;
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

## 找到 Physicalize_Balls

表名是：

```text
Physicalize_Balls
```

脚本这样取得它：

```angelscript
CKDataArray@ table = ctx.BorrowDataArrayByName("Physicalize_Balls");
```

找不到时直接停下：

```angelscript
if (table is null) {
    LogWarn(ctx, "PhysMod Physicalize_Balls not found");
    ctx.SendIngameMessage("Physicalize_Balls not found.");
    return;
}
```

这个检查和前两章一样。  
读任何 DataArray 前都要先确认表存在。

## 表的大小

进入第一关后，日志会看到：

```text
PhysMod Physicalize_Balls rows=2 columns=12
```

这里有两个重点：

```text
2 行：本表里有两类球形物理参数
12 列：本表不只保存摩擦、弹性、质量、半径
```

本章只读最容易理解的 5 列。

```angelscript
int nameColumn = BML::CK::FindColumn(table, "Group Name");
int frictionColumn = BML::CK::FindColumn(table, "Friction");
int elasticityColumn = BML::CK::FindColumn(table, "Elasticity");
int massColumn = BML::CK::FindColumn(table, "Mass");
int radiusColumn = BML::CK::FindColumn(table, "Radius");
```

`Group Name` 用 `GetString` 读。  
后面四个参数用 `GetFloat` 读。

## 第一次使用 GetFloat

前面已经用过：

```text
GetString 读文本
GetInt    读整数
```

这一章新增：

```text
GetFloat  读小数
```

代码里是这样：

```angelscript
float friction = BML::CK::GetFloat(table, row, frictionColumn, 0.0f);
float elasticity = BML::CK::GetFloat(table, row, elasticityColumn, 0.0f);
float mass = BML::CK::GetFloat(table, row, massColumn, 0.0f);
float radius = BML::CK::GetFloat(table, row, radiusColumn, 0.0f);
```

最后的 `0.0f` 是默认值。  
如果行号、列号或单元格读取失败，API 会返回这个默认值。

所以脚本仍然先用 `FindColumn` 检查列号。  
不要直接读一个可能不存在的列。

## 进入关卡后的结果

启动 Player，进入第一关。脚本会自动读取 `Physicalize_Balls`。

也可以打开 BML 命令栏，输入：

```text
ckphys balls
```

日志会像这样：

```text
PhysMod scan begin reason=level-start
PhysMod Physicalize_Balls rows=2 columns=12
PhysMod P_Ball_Wood: friction=0.6 elasticity=0.2 mass=2 radius=2
PhysMod P_Ball_Stone: friction=0.7 elasticity=0.1 mass=10 radius=2
PhysMod scan end
```

这组结果先看两行：

| 行 | 含义 |
| --- | --- |
| `P_Ball_Wood` | 木球物理参数 |
| `P_Ball_Stone` | 石球物理参数 |

再看参数差异：

| 参数 | 木球 | 石球 |
| --- | --- | --- |
| `Friction` | `0.6` | `0.7` |
| `Elasticity` | `0.2` | `0.1` |
| `Mass` | `2` | `10` |
| `Radius` | `2` | `2` |

石球质量更大，弹性更低。  
半径一样，说明这里的球形碰撞半径都按 `2` 配置。

## 为什么表里没有纸球

这一章读到的 `Physicalize_Balls` 只有两行：

```text
P_Ball_Wood
P_Ball_Stone
```

这不代表纸球没有物理参数。  
纸球的处理方式和木球、石球不完全一样，后面看 `Physicalize_Convex` 和物理化事件时再展开。

现在先记住：每张 DataArray 只回答它负责的问题。  
不要把一张表当成全部物理系统。

## 本章结果

现在已经读过三类 DataArray：

```text
CurrentLevel        当前这一局的状态
AllLevel            13 个关卡的起始配置
Physicalize_Balls   木球、石球的球形物理参数
```

这章新增了 `GetFloat`，也建立了后面讲物理事件需要的基础：

```text
表里能看到参数来源
运行中的物理对象要等物理化流程创建
```

下一章开始把 `base.cmo`、`Levelinit.nmo`、`Gameplay.nmo` 串起来看。

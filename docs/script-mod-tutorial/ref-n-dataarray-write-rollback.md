# 参考 N：DataArray 写入和回滚

相关主题一直在读 DataArray。读表不会改变游戏状态，出错时通常只是日志不对。

写 DataArray 时，原版行为图下一帧就可能读到新值，所以必须先建立一个习惯：

```text
先保存旧值
再写入新值
马上读回确认
最后恢复旧值
```

本参考只写一个单元格：

```text
Energy 表
第 0 行
Points 列
```

`Energy.Points` 是当前分数。本参考把它临时加 1，然后立刻恢复。这样能确认 `SetInt` 的调用方式，又不会留下关卡状态。

## 检查目标

此处不追求做出“加分 mod”。
它只验证一个更基础的能力：

```text
脚本能不能安全地写一次原版表，再把值恢复回去？
```

所以命令叫 `pulse`。它像一次很短的脉冲：

```text
读取旧分数
临时写入旧分数 + 1
立刻读回确认
马上恢复旧分数
```

正常结束后，当前分数应该回到原值。
如果这一步都不稳定，写 `CurrentLevel`、`IngameParameter`、检查点表就更危险。

脚本会同时写日志和发一条游戏内短消息。
短消息用来看“命令确实触发了”，日志用来核对旧值、写入值和恢复值。

## DataArray 写入 API

DataArray 的读取函数相关基础中已经用过：

```angelscript
int value = BML::CK::GetInt(table, row, column, 0);
```

写入函数和读取函数一一对应：

| API | 写入的值 |
| --- | --- |
| `BML::CK::SetString(array, row, column, value)` | 字符串 |
| `BML::CK::SetBool(array, row, column, value)` | 布尔值 |
| `BML::CK::SetInt(array, row, column, value)` | 整数 |
| `BML::CK::SetFloat(array, row, column, value)` | 小数 |

这些函数都返回 `bool`：

```text
true   写入成功
false  写入失败
```

失败的常见原因有三类：

```text
表为空
行号不存在
列号不存在
写入类型和单元格类型不匹配
```

所以写入前仍然要按老步骤来：

```text
BorrowDataArrayByName 找表
GetRowCount 确认行数
FindColumn 找列
GetInt 保存旧值
SetInt 写新值
GetInt 读回确认
SetInt 恢复旧值
```

## 完整脚本

脚本入口路径：

```text
ModLoader/Mods/ArrayWriteMod.mod.as
```

```angelscript
[bml.mod id="array.write.script" name="Array Write Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="Write and restore one DataArray cell"]
class ArrayWriteMod {
    private bool hasBackup = false;
    private string backupTableName = "";
    private int backupRow = -1;
    private int backupColumn = -1;
    private int backupValue = 0;

    void OnLoad(const BML::ModContext &in ctx) {
        BML::CommandDefinition def;
        def.Name = "ckarray";
        def.Description = "Inspect or test a reversible DataArray write";
        def.Usage = "ckarray [inspect|pulse|restore]";

        BML::CommandCallback@ execute = BML::CommandCallback(this.OnArrayCommand);
        BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompleteArrayCommand);
        ctx.RegisterCommand(def, execute, complete);

        ctx.BorrowLogger().Info("ArrayWriteMod loaded; use ckarray after level start");
        ctx.SendIngameMessage("ArrayWrite loaded. Use ckarray pulse in a level.");
    }

    void OnUnload(const BML::ModContext &in ctx) {
        // 脚本卸载前，如果还有未恢复的值，先写回旧值。
        RestoreBackup(ctx, "unload");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL ||
            event == BML::GAME_EVENT_PRE_LOAD_LEVEL) {
            // 关卡切换时运行时表会变化，不要把旧备份拖到下一关。
            RestoreBackup(ctx, "level-change");
        }
    }

    private void OnArrayCommand(const BML::ModContext &in ctx,
                                const BML::CommandEvent &in event) {
        string action = "inspect";
        if (event.ArgCount > 0) {
            action = event.GetArg(0);
        }

        if (action == "inspect") {
            InspectPoints(ctx);
        } else if (action == "pulse") {
            PulsePoints(ctx);
        } else if (action == "restore") {
            RestoreBackup(ctx, "command");
        } else {
            ctx.BorrowLogger().Warn("Unknown ckarray action: " + action);
        }
    }

    private void CompleteArrayCommand(const BML::ModContext &in ctx,
                                      const BML::CommandEvent &in event,
                                      BML::CommandCompletion &inout completions) {
        completions.Add("inspect");
        completions.Add("pulse");
        completions.Add("restore");
    }

    private void InspectPoints(const BML::ModContext &in ctx) {
        BML::Logger@ log = ctx.BorrowLogger();
        CKDataArray@ energy = BorrowEnergy(ctx, log);
        if (energy is null) {
            return;
        }

        int column = FindColumn(log, energy, "Points");
        if (column < 0) {
            return;
        }

        int points = BML::CK::GetInt(energy, 0, column, 0);
        log.Info("ArrayWrite Energy.Points=" + points);
        ctx.SendIngameMessage("Energy.Points = " + points);
    }

    private void PulsePoints(const BML::ModContext &in ctx) {
        BML::Logger@ log = ctx.BorrowLogger();
        CKDataArray@ energy = BorrowEnergy(ctx, log);
        if (energy is null) {
            return;
        }

        int column = FindColumn(log, energy, "Points");
        if (column < 0) {
            return;
        }

        int oldValue = BML::CK::GetInt(energy, 0, column, 0);
        // 写入前先保存旧值。之后任何一步失败，都还有机会恢复。
        SaveBackup("Energy", 0, column, oldValue);

        int newValue = oldValue + 1;
        if (!BML::CK::SetInt(energy, 0, column, newValue)) {
            log.Warn("ArrayWrite SetInt failed");
            RestoreBackup(ctx, "set-failed");
            return;
        }

        // 写入后马上读回，确认单元格当前是什么值。
        int afterWrite = BML::CK::GetInt(energy, 0, column, -1);
        log.Info("ArrayWrite wrote Energy.Points: old=" + oldValue +
                 " afterWrite=" + afterWrite);

        // 本参考只验证写入路径，不把分数改动留在关卡里。
        RestoreBackup(ctx, "pulse");
        int restored = BML::CK::GetInt(energy, 0, column, -1);
        log.Info("ArrayWrite restored Energy.Points=" + restored);
        ctx.SendIngameMessage("Points pulse: " + oldValue + " -> " + afterWrite + " -> " + restored);
    }

    private CKDataArray@ BorrowEnergy(const BML::ModContext &in ctx,
                                      BML::Logger@ log) {
        CKDataArray@ energy = ctx.BorrowDataArrayByName("Energy");
        if (energy is null) {
            log.Warn("Energy table not found; enter a level first");
            return null;
        }

        int rows = BML::CK::GetRowCount(energy);
        if (rows <= 0) {
            log.Warn("Energy table has no rows");
            return null;
        }

        return energy;
    }

    private int FindColumn(BML::Logger@ log,
                           CKDataArray@ table,
                           const string &in columnName) {
        int column = BML::CK::FindColumn(table, columnName);
        if (column < 0) {
            log.Warn("Column not found: " + columnName);
        }
        return column;
    }

    private void SaveBackup(const string &in tableName,
                            int row,
                            int column,
                            int value) {
        hasBackup = true;
        backupTableName = tableName;
        backupRow = row;
        backupColumn = column;
        backupValue = value;
    }

    private bool RestoreBackup(const BML::ModContext &in ctx,
                               const string &in reason) {
        if (!hasBackup) {
            return true;
        }

        BML::Logger@ log = ctx.BorrowLogger();
        // 备份只保存表名和坐标，恢复时重新借当前关卡里的表。
        CKDataArray@ table = ctx.BorrowDataArrayByName(backupTableName);
        if (table is null) {
            log.Warn("Backup table missing during restore: " + backupTableName);
            hasBackup = false;
            return false;
        }

        bool ok = BML::CK::SetInt(table, backupRow, backupColumn, backupValue);
        if (ok) {
            log.Info("ArrayWrite restore " + reason + ": " +
                     backupTableName + "[" + backupRow + "," +
                     backupColumn + "]=" + backupValue);
        } else {
            log.Warn("ArrayWrite restore failed: " + reason);
        }

        hasBackup = false;
        return ok;
    }
}
```

## 先看命令

这个脚本注册了一个命令：

```text
ckarray [inspect|pulse|restore]
```

三个动作分别是：

| 命令 | 动作 |
| --- | --- |
| `ckarray inspect` | 只读取 `Energy.Points` |
| `ckarray pulse` | 写入一次，然后立刻恢复 |
| `ckarray restore` | 如果还有备份值，手动恢复 |

补全函数也把这三个动作加进去：

```angelscript
completions.Add("inspect");
completions.Add("pulse");
completions.Add("restore");
```

## 为什么要保存备份

`PulsePoints` 里先读旧值：

```angelscript
int oldValue = BML::CK::GetInt(energy, 0, column, 0);
```

然后保存备份：

```angelscript
SaveBackup("Energy", 0, column, oldValue);
```

备份保存了四件事：

```text
哪张表
哪一行
哪一列
旧值是多少
```

这四件事缺一个，恢复时就不知道该把值写回哪里。

本参考只备份 `int`。如果以后写 `float`、`bool`、`string`，要分别保存旧的 `float`、`bool`、`string`，不能全塞进一个整数备份里。

## 写入后必须读回

写入用：

```angelscript
bool ok = BML::CK::SetInt(energy, 0, column, newValue);
```

`ok` 只能说明 setter 返回成功。为了确认单元格当前值，又读了一次：

```angelscript
int afterWrite = BML::CK::GetInt(energy, 0, column, -1);
```

日志会同时打印旧值和写入后的值：

```text
ArrayWrite wrote Energy.Points: old=0 afterWrite=1
```

如果看到 `afterWrite` 没变，先检查三件事：

```text
列名是否正确
单元格类型是否是整数
写入后是否马上被原版行为图覆盖
```

## 恢复有三个入口

本参考的恢复不只靠 `pulse` 末尾那一行。

脚本准备了三个恢复入口：

```angelscript
RestoreBackup(ctx, "pulse");
RestoreBackup(ctx, "command");
RestoreBackup(ctx, "unload");
```

另外，关卡切换前也会恢复：

```angelscript
if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL ||
    event == BML::GAME_EVENT_PRE_LOAD_LEVEL) {
    RestoreBackup(ctx, "level-change");
}
```

这样写是为了降低 DataArray 写入风险。脚本可能在命令中途出错，也可能在关卡切换时还留着备份。恢复入口多一层，风险就低一层。

## 哪些表不要上来就写

刚开始验证写入能力时，不要碰这些表和字段：

| 表或字段 | 原因 |
| --- | --- |
| `CurrentLevel.ActiveBall` | 会影响当前球类型，常和原版流程联动 |
| `IngameParameter.Activate Sector` | 影响关卡流程判断 |
| `Checkpoints` | 影响检查点状态和复活流程 |
| `ResetPoints` | 影响复活点选择 |
| `Physicalize_Balls` | 影响物理化参数，通常要配合重新物理化 |
| `AllLevel` | 属于关卡配置，不适合拿来做临时运行验证 |

先从这类值开始：

```text
容易读懂
类型明确
改动后能马上恢复
不控制关卡流程
```

`Energy.Points` 也不应该长期改。本参考只用它证明 `SetInt` 路径可用，真正做玩法时要重新设计状态来源、触发时机和恢复策略。

## 运行后看什么

进入第一关后执行：

```text
ckarray inspect
```

应当看到当前分数：

```text
ArrayWrite Energy.Points=0
```

再执行：

```text
ckarray pulse
```

一次实际运行中输出如下：

```text
ArrayWrite wrote Energy.Points: old=0 afterWrite=1
ArrayWrite restore pulse: Energy[0,0]=0
ArrayWrite restored Energy.Points=0
Points pulse: 0 -> 1 -> 0
```

这几行分别对应：

```text
写入成功
恢复函数执行
读回确认已经恢复
游戏内消息也显示了这次变化
```

如果已经吃到分数，再执行同一条命令，旧值会变成当时的分数。例如这次测试中，又执行了一次：

```text
ArrayWrite wrote Energy.Points: old=916 afterWrite=917
ArrayWrite restore pulse: Energy[0,0]=916
ArrayWrite restored Energy.Points=916
Points pulse: 916 -> 917 -> 916
```

代码没有变，变的是 `Energy.Points` 当时的状态。

如果在菜单里运行命令，`Energy` 表可能还没进入本局运行状态。进入关卡后再执行。

## 适用边界

本参考只说明 DataArray 单元格怎么写、怎么恢复。

还没有解决这些问题：

```text
什么时候写
写入后原版行为图什么时候会读到
写入值是否会被原版流程覆盖
写入后是否还要同步对象、物理或 UI
```

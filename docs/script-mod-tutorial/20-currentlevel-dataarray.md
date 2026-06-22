# 第 20 章：DataArray 入门：CurrentLevel

上一章按名字找到了对象和组。

这一章换一个角度看游戏状态：读一张 DataArray。

先记住这个模型：

```text
DataArray 是表

表有名字
表里有行
行里有列
行列交叉的位置是单元格
```

`CurrentLevel` 是这一章要读的表。它记录当前这一局的几个核心状态，例如当前关卡、当前球、当前复活点和分数。

本章只查看，不写入。

## 表、行、列、单元格

可以先把 `CurrentLevel` 想成这样：

| 列名 | 值 |
| --- | --- |
| `Level ID` | 当前关卡编号 |
| `ActiveBall` | 当前活动球 |
| `CurrentResetpoint` | 当前复活点矩阵 |
| `Activation Phase?` | 当前是否处在激活阶段 |
| `Points` | 当前分数 |

它和第 19 章的对象、组不一样。

第 19 章查的是：

```text
这个名字对应哪个对象
这个名字对应哪个组
```

本章查的是：

```text
这张表现在有几行几列
某一列现在是什么值
```

## 新建脚本

新建：

```text
ModLoader/Mods/StateMod.mod.as
```

写入：

```angelscript
[bml.mod id="state.script" name="State Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="Read CurrentLevel DataArray"]
class StateMod {
    private BML::CommandRef@ commandRef;

    void OnLoad(const BML::ModContext &in ctx) {
        RegisterCommand(ctx);
        LogInfo(ctx, "StateMod loaded");
        ctx.SendIngameMessage("StateMod loaded. Use ckstate scan after entering a level.");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_START_LEVEL) {
            ScanCurrentLevel(ctx, "level-start");
        }
    }

    void OnUnload(const BML::ModContext &in ctx) {
        LogInfo(ctx, "StateMod unloaded");
    }

    private void RegisterCommand(const BML::ModContext &in ctx) {
        BML::CommandDefinition def;
        def.Name = "ckstate";
        def.Description = "Read CurrentLevel table";
        def.Usage = "ckstate scan";
        def.Category = "Tutorial";
        def.Enabled = true;

        BML::CommandCallback@ execute = BML::CommandCallback(this.OnStateCommand);
        BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompleteStateCommand);

        @commandRef = ctx.RegisterCommand(def, execute, complete);
        bool valid = commandRef !is null && commandRef.IsValid;
        LogInfo(ctx, "StateMod command registered=" + BoolText(valid));
    }

    void OnStateCommand(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        string action = "scan";
        if (event.ArgCount > 0) {
            action = event.GetArg(0);
        }

        if (action == "scan") {
            ScanCurrentLevel(ctx, "command");
            return;
        }

        ctx.SendIngameMessage("Usage: ckstate scan");
        LogWarn(ctx, "StateMod unknown command action: " + action);
    }

    void CompleteStateCommand(const BML::ModContext &in ctx,
                              const BML::CommandEvent &in event,
                              BML::CommandCompletion &inout completions) {
        completions.Add("scan");
    }

    private void ScanCurrentLevel(const BML::ModContext &in ctx, const string &in reason) {
        LogInfo(ctx, "StateMod scan begin reason=" + reason +
                     " inLevel=" + BoolText(ctx.GetIsInLevel()));

        // CurrentLevel 是运行时表，用 BorrowDataArrayByName 按表名取得。
        // inLevel 日志只辅助判断时机，不能代替下面的 null 检查。
        CKDataArray@ currentLevel = ctx.BorrowDataArrayByName("CurrentLevel");
        if (currentLevel is null) {
            LogWarn(ctx, "StateMod CurrentLevel not found");
            ctx.SendIngameMessage("CurrentLevel not found.");
            return;
        }

        // 先看表的规模。没有行时，后面不能读第 0 行。
        int rows = BML::CK::GetRowCount(currentLevel);
        int columns = BML::CK::GetColumnCount(currentLevel);
        LogInfo(ctx, "StateMod CurrentLevel rows=" + rows + " columns=" + columns);

        if (rows <= 0) {
            LogWarn(ctx, "StateMod CurrentLevel has no rows");
            return;
        }

        // 这一章只读第 0 行的几个字段，不写入表。
        LogInfo(ctx, "StateMod " + ReadInt(currentLevel, "Level ID"));
        LogInfo(ctx, "StateMod " + ReadString(currentLevel, "ActiveBall"));
        LogInfo(ctx, "StateMod " + ReadString(currentLevel, "CurrentResetpoint"));
        LogInfo(ctx, "StateMod " + ReadString(currentLevel, "Activation Phase?"));
        LogInfo(ctx, "StateMod " + ReadInt(currentLevel, "Points"));

        LogInfo(ctx, "StateMod scan end");
        ctx.SendIngameMessage("CurrentLevel scan finished. Check ModLoader log.");
    }

    private string ReadInt(CKDataArray@ array, const string &in columnName) {
        int column = BML::CK::FindColumn(array, columnName);
        if (column < 0) {
            return columnName + ": column not found";
        }

        int value = BML::CK::GetInt(array, 0, column, 0);
        return columnName + ": " + value;
    }

    private string ReadString(CKDataArray@ array, const string &in columnName) {
        int column = BML::CK::FindColumn(array, columnName);
        if (column < 0) {
            return columnName + ": column not found";
        }

        string value = BML::CK::GetString(array, 0, column, "");
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

## 命令入口

这个脚本注册了一个命令：

```text
ckstate scan
```

命令只做一件事：

```text
重新读取 CurrentLevel
把结果写入日志
```

代码入口在这里：

```angelscript
void OnStateCommand(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
    string action = "scan";
    if (event.ArgCount > 0) {
        action = event.GetArg(0);
    }

    if (action == "scan") {
        ScanCurrentLevel(ctx, "command");
        return;
    }

    ctx.SendIngameMessage("Usage: ckstate scan");
    LogWarn(ctx, "StateMod unknown command action: " + action);
}
```

这样写有两个好处。

第一，刚进入关卡时会自动扫一次。

第二，站在关卡里也能手动再扫一次。

## 找到 CurrentLevel

读取 DataArray 的入口是：

```angelscript
CKDataArray@ currentLevel = ctx.BorrowDataArrayByName("CurrentLevel");
```

它按表名查找。找不到时返回 `null`。

所以后面马上判断：

```angelscript
if (currentLevel is null) {
    LogWarn(ctx, "StateMod CurrentLevel not found");
    ctx.SendIngameMessage("CurrentLevel not found.");
    return;
}
```

在菜单阶段，关卡运行状态还没准备好，`CurrentLevel` 可能拿不到。  
进入关卡后再执行 `ckstate scan`，结果才有意义。

脚本也记录了 `ctx.GetIsInLevel()`：

```angelscript
LogInfo(ctx, "StateMod scan begin reason=" + reason +
             " inLevel=" + BoolText(ctx.GetIsInLevel()));
```

这个值只放进日志里看时机。  
不要用它挡住读表流程。

在当前 BMLPlus 中，开始关卡事件回调触发时，`CurrentLevel` 已经可以读，但 `inLevel` 仍可能显示 `false`。
所以这一章的可靠判断顺序是：

```text
先 BorrowDataArrayByName
再判断 null
再检查行数
再 FindColumn
```

## 先看行数和列数

表拿到了，也不要马上读单元格。先看规模：

```angelscript
int rows = BML::CK::GetRowCount(currentLevel);
int columns = BML::CK::GetColumnCount(currentLevel);
```

日志会输出：

```text
StateMod CurrentLevel rows=1 columns=6
```

如果 `rows` 是 `0`，第 0 行不存在。  
所以脚本会停在这里：

```angelscript
if (rows <= 0) {
    LogWarn(ctx, "StateMod CurrentLevel has no rows");
    return;
}
```

这一步很重要。DataArray 是表，读单元格前必须先确认目标行存在。

## 按列名读值

读列之前先找列号：

```angelscript
int column = BML::CK::FindColumn(array, columnName);
```

找不到列时，`FindColumn` 返回小于 `0` 的值。  
这时不要继续读：

```angelscript
if (column < 0) {
    return columnName + ": column not found";
}
```

列存在以后，再按类型读取。

整数列：

```angelscript
int value = BML::CK::GetInt(array, 0, column, 0);
```

字符串形式：

```angelscript
string value = BML::CK::GetString(array, 0, column, "");
```

这里的 `0` 有两个位置：

```text
GetInt(array, 0, column, 0)
              ^         ^
              |         默认值
              第 0 行
```

最后一个 `0` 是默认值。  
如果读取失败，API 会给这个默认值。

所以脚本先用 `FindColumn` 检查列是否存在。否则 `0` 可能表示真实值，也可能只是默认值，日志会变得不好判断。

## 进入关卡后的结果

启动 Player，进入第一关。脚本会在关卡开始时自动扫描。

也可以打开 BML 命令栏，输入：

```text
ckstate scan
```

日志通常会像这样：

```text
StateMod scan begin reason=level-start inLevel=false
StateMod CurrentLevel rows=1 columns=6
StateMod Level ID: 1
StateMod ActiveBall: Ball_Wood
StateMod CurrentResetpoint: [-1,0,0,0][0,1,0,0][0,0,-1,0][54.0956,17.6565,-153.072,1]
StateMod Activation Phase?: FALSE
StateMod Points: 0
StateMod scan end
```

这里先关心三件事：

| 日志 | 说明 |
| --- | --- |
| `rows=1` | 这张表当前有一行 |
| `Level ID: 1` | 当前在第一关 |
| `ActiveBall: Ball_Wood` | 当前活动球是木球 |

这里的 `inLevel=false` 不影响本章结论。  
它说明这个回调触发点比 BML 的关卡状态标志更新更早一点，但表已经能读。

`CurrentResetpoint` 看起来比较长，因为它是一个矩阵。  
现在先把它当成“复活点的位置和方向数据”，后面讲复活点流程时再展开。

## 菜单阶段为什么可能拿不到

`CurrentLevel` 属于关卡运行状态。

在菜单里，Player 还没有进入一局游戏。此时执行：

```text
ckstate scan
```

可能看到：

```text
StateMod scan begin reason=command inLevel=false
StateMod CurrentLevel not found
```

这时重点看 `CurrentLevel not found`。  
脚本没有坏，只是还没有可读取的关卡状态表。

## 本章结果

现在已经可以用脚本读一张 DataArray：

```text
BorrowDataArrayByName 按名字拿表
GetRowCount / GetColumnCount 看表的规模
FindColumn 按列名找列号
GetInt / GetString 读取单元格
先检查 null、行数和列号
```

这一章仍然不修改 `CurrentLevel`。

下一章读 `AllLevel`。那张表保存关卡配置，不保存当前这一局的即时状态。

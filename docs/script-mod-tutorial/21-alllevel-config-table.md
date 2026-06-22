# 第 21 章：关卡配置表 AllLevel

上一章读的是 `CurrentLevel`。

`CurrentLevel` 记录当前这一局正在发生什么。  
这一章读 `AllLevel`，它记录 13 个原版关卡开局时该用什么配置。

先把两张表分开：

| 表名 | 更关心什么 |
| --- | --- |
| `CurrentLevel` | 当前这一局的状态 |
| `AllLevel` | 13 个关卡的起始配置 |

`CurrentLevel` 通常只有当前状态的一行。  
`AllLevel` 有 13 行，每一行对应一个原版关卡。

## AllLevel 里有什么

`AllLevel` 常见列名是：

```text
Levelfile
StartBall
StartResetpoint
Sky
Light
Skytranslation
LevelBonus
Music
```

这些列回答的是开局配置问题：

```text
这一关加载哪个关卡内容文件
开局使用哪种球
开局复活点是哪一个
天空和光照怎么配
通关奖励分是多少
使用哪一首音乐
```

本章先读其中 4 列：

| 列名 | 含义 |
| --- | --- |
| `Levelfile` | 关卡内容文件 |
| `StartBall` | 起始球 |
| `LevelBonus` | 关卡奖励分 |
| `Music` | 音乐编号 |

这四列已经足够看出 `AllLevel` 的作用。

## 行号和关卡号

DataArray 的行号从 `0` 开始。

玩家说的关卡号从 `1` 开始。

所以对应关系是：

| DataArray 行号 | 显示给玩家看的关卡 |
| --- | --- |
| `0` | 第 1 关 |
| `1` | 第 2 关 |
| `12` | 第 13 关 |

代码里循环时用 `row`，日志里显示时用 `row + 1`。

## 新建脚本

新建：

```text
ModLoader/Mods/LevelMod.mod.as
```

写入：

```angelscript
[bml.mod id="level.script" name="Level Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="Read AllLevel DataArray"]
class LevelMod {
    private BML::CommandRef@ commandRef;

    void OnLoad(const BML::ModContext &in ctx) {
        RegisterCommand(ctx);
        LogInfo(ctx, "LevelMod loaded");
        ctx.SendIngameMessage("LevelMod loaded. Use cklevel list after entering a level.");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_START_LEVEL) {
            ScanAllLevel(ctx, "level-start");
        }
    }

    void OnUnload(const BML::ModContext &in ctx) {
        LogInfo(ctx, "LevelMod unloaded");
    }

    private void RegisterCommand(const BML::ModContext &in ctx) {
        BML::CommandDefinition def;
        def.Name = "cklevel";
        def.Description = "Read AllLevel table";
        def.Usage = "cklevel list";
        def.Category = "Tutorial";
        def.Enabled = true;

        BML::CommandCallback@ execute = BML::CommandCallback(this.OnLevelCommand);
        BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompleteLevelCommand);

        @commandRef = ctx.RegisterCommand(def, execute, complete);
        bool valid = commandRef !is null && commandRef.IsValid;
        LogInfo(ctx, "LevelMod command registered=" + BoolText(valid));
    }

    void OnLevelCommand(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        string action = "list";
        if (event.ArgCount > 0) {
            action = event.GetArg(0);
        }

        if (action == "list") {
            ScanAllLevel(ctx, "command");
            return;
        }

        ctx.SendIngameMessage("Usage: cklevel list");
        LogWarn(ctx, "LevelMod unknown command action: " + action);
    }

    void CompleteLevelCommand(const BML::ModContext &in ctx,
                              const BML::CommandEvent &in event,
                              BML::CommandCompletion &inout completions) {
        completions.Add("list");
    }

    private void ScanAllLevel(const BML::ModContext &in ctx, const string &in reason) {
        LogInfo(ctx, "LevelMod scan begin reason=" + reason);

        // AllLevel 是关卡配置表，用表名取得。
        CKDataArray@ allLevel = ctx.BorrowDataArrayByName("AllLevel");
        if (allLevel is null) {
            LogWarn(ctx, "LevelMod AllLevel not found");
            ctx.SendIngameMessage("AllLevel not found.");
            return;
        }

        int rows = BML::CK::GetRowCount(allLevel);
        int columns = BML::CK::GetColumnCount(allLevel);
        LogInfo(ctx, "LevelMod AllLevel rows=" + rows + " columns=" + columns);

        int levelFileColumn = BML::CK::FindColumn(allLevel, "Levelfile");
        int startBallColumn = BML::CK::FindColumn(allLevel, "StartBall");
        int levelBonusColumn = BML::CK::FindColumn(allLevel, "LevelBonus");
        int musicColumn = BML::CK::FindColumn(allLevel, "Music");

        if (levelFileColumn < 0 || startBallColumn < 0 ||
            levelBonusColumn < 0 || musicColumn < 0) {
            LogWarn(ctx, "LevelMod required AllLevel column not found");
            ctx.SendIngameMessage("AllLevel column not found.");
            return;
        }

        // row 从 0 开始；显示时用 row + 1，和玩家看到的关卡号一致。
        for (int row = 0; row < rows; row++) {
            LogInfo(ctx, "LevelMod " + DescribeLevelRow(allLevel, row,
                levelFileColumn, startBallColumn, levelBonusColumn, musicColumn));
        }

        LogInfo(ctx, "LevelMod scan end");
        ctx.SendIngameMessage("AllLevel scan finished. Check ModLoader log.");
    }

    private string DescribeLevelRow(CKDataArray@ array, int row,
                                    int levelFileColumn, int startBallColumn,
                                    int levelBonusColumn, int musicColumn) {
        string levelFile = BML::CK::GetString(array, row, levelFileColumn, "");
        string startBall = BML::CK::GetString(array, row, startBallColumn, "");
        int levelBonus = BML::CK::GetInt(array, row, levelBonusColumn, 0);
        int music = BML::CK::GetInt(array, row, musicColumn, 0);

        return "Level " + (row + 1) + ": " + levelFile +
            " start=" + startBall +
            " bonus=" + levelBonus +
            " music=" + music;
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

## 找到 AllLevel

入口和上一章一样：

```angelscript
CKDataArray@ allLevel = ctx.BorrowDataArrayByName("AllLevel");
```

按名字拿表。找不到时返回 `null`：

```angelscript
if (allLevel is null) {
    LogWarn(ctx, "LevelMod AllLevel not found");
    ctx.SendIngameMessage("AllLevel not found.");
    return;
}
```

`AllLevel` 通常在进入关卡流程后更稳定。  
所以脚本在 `GAME_EVENT_START_LEVEL` 自动扫一次，也提供 `cklevel list` 手动再扫。

## 先看表有多大

```angelscript
int rows = BML::CK::GetRowCount(allLevel);
int columns = BML::CK::GetColumnCount(allLevel);
```

进入第一关后，日志会看到：

```text
LevelMod AllLevel rows=13 columns=8
```

这正好对应：

```text
13 行：13 个原版关卡
8 列：每关的起始配置字段
```

## 先找列，再读值

这一章读 4 列。

代码先把列号找出来：

```angelscript
int levelFileColumn = BML::CK::FindColumn(allLevel, "Levelfile");
int startBallColumn = BML::CK::FindColumn(allLevel, "StartBall");
int levelBonusColumn = BML::CK::FindColumn(allLevel, "LevelBonus");
int musicColumn = BML::CK::FindColumn(allLevel, "Music");
```

只要有一个列名找不到，就停下来：

```angelscript
if (levelFileColumn < 0 || startBallColumn < 0 ||
    levelBonusColumn < 0 || musicColumn < 0) {
    LogWarn(ctx, "LevelMod required AllLevel column not found");
    ctx.SendIngameMessage("AllLevel column not found.");
    return;
}
```

停下来比继续读默认值更好。  
如果列名写错，`GetInt(..., 0)` 可能一直给 `0`，看起来像真实数据，排查会变难。

## 用循环读 13 行

`AllLevel` 不是只读当前关卡那一行。  
这一章把 13 行都打印出来：

```angelscript
for (int row = 0; row < rows; row++) {
    LogInfo(ctx, "LevelMod " + DescribeLevelRow(allLevel, row,
        levelFileColumn, startBallColumn, levelBonusColumn, musicColumn));
}
```

`row` 是 DataArray 的行号。  
`DescribeLevelRow` 负责把一行整理成日志文本：

```angelscript
private string DescribeLevelRow(CKDataArray@ array, int row,
                                int levelFileColumn, int startBallColumn,
                                int levelBonusColumn, int musicColumn) {
    string levelFile = BML::CK::GetString(array, row, levelFileColumn, "");
    string startBall = BML::CK::GetString(array, row, startBallColumn, "");
    int levelBonus = BML::CK::GetInt(array, row, levelBonusColumn, 0);
    int music = BML::CK::GetInt(array, row, musicColumn, 0);

    return "Level " + (row + 1) + ": " + levelFile +
        " start=" + startBall +
        " bonus=" + levelBonus +
        " music=" + music;
}
```

这里用了两种读取函数：

| 列 | 读取函数 |
| --- | --- |
| `Levelfile` | `GetString` |
| `StartBall` | `GetString` |
| `LevelBonus` | `GetInt` |
| `Music` | `GetInt` |

`row + 1` 只用于显示。  
读表时仍然用原来的 `row`。

## 进入关卡后的结果

启动 Player，进入第一关。脚本会自动读取 `AllLevel`。

也可以打开 BML 命令栏，输入：

```text
cklevel list
```

日志会像这样：

```text
LevelMod scan begin reason=level-start
LevelMod AllLevel rows=13 columns=8
LevelMod Level 1: Level_01.nmo start=Ball_Wood bonus=100 music=1
LevelMod Level 2: Level_02.nmo start=Ball_Wood bonus=200 music=5
LevelMod Level 3: Level_03.nmo start=Ball_Wood bonus=300 music=2
LevelMod Level 4: Level_04.nmo start=Ball_Wood bonus=400 music=3
LevelMod Level 5: Level_05.nmo start=Ball_Wood bonus=500 music=1
LevelMod Level 6: Level_06.nmo start=Ball_Wood bonus=600 music=5
LevelMod Level 7: Level_07.nmo start=Ball_Wood bonus=700 music=4
LevelMod Level 8: Level_08.nmo start=Ball_Wood bonus=800 music=2
LevelMod Level 9: Level_09.nmo start=Ball_Wood bonus=900 music=3
LevelMod Level 10: Level_10.nmo start=Ball_Wood bonus=1000 music=1
LevelMod Level 11: Level_11.nmo start=Ball_Wood bonus=1100 music=3
LevelMod Level 12: Level_12.nmo start=Ball_Wood bonus=1200 music=4
LevelMod Level 13: Level_13.nmo start=Ball_Stone bonus=1300 music=5
LevelMod scan end
```

这组结果说明：

```text
第 1 关加载 Level_01.nmo，起始球是 Ball_Wood。
第 13 关加载 Level_13.nmo，起始球是 Ball_Stone。
奖励分从 100 增长到 1300。
音乐编号不是简单递增，它来自配置表。
```

## 和 CurrentLevel 对照

现在可以把第 20 章和第 21 章连起来看。

`AllLevel` 里第 1 行写着：

```text
Level_01.nmo
Ball_Wood
bonus=100
music=1
```

进入第一关后，`CurrentLevel` 里能看到：

```text
Level ID: 1
ActiveBall: Ball_Wood
Points: 0
```

这两张表的关系可以先这样理解：

```text
AllLevel 提供每一关的开局配置
CurrentLevel 记录当前这一局的运行状态
```

`AllLevel` 不是“当前关卡状态”。它有 13 行。  
`CurrentLevel` 也不是“完整关卡清单”。它记录当前这一局。

## 本章结果

现在已经能读取第二张 DataArray：

```text
AllLevel
13 行
8 列
row 0 对应第 1 关
row 12 对应第 13 关
GetString 读文本列
GetInt 读整数列
```

下一章读 `Physicalize_Balls`。那张表开始出现小数参数，所以会用到 `GetFloat`。

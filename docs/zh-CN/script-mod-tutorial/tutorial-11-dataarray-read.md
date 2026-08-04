# 11 读 DataArray

上一章讲到 Levelinit 会创建运行时表格来记录游戏状态。CurrentLevel 表像仪表盘一样实时反映当前关卡的情况，关卡号、活动球、分数、复活点。这一章动手把这些数据读出来。

DataArray 就是游戏内的电子表格。它有行有列，每个单元格可以保存整数、字符串、对象引用、参数值等不同类型的数据。读取的流程固定，先找到表，再定位单元格，然后取值。

## 读取的完整步骤

每次读 DataArray 都遵循同样的流程：

```text
第一步：按名字获取表的句柄
  -> ctx.BorrowDataArrayByName("表名")
  -> 返回 CKDataArray@ 或 null

第二步：确认表存在且有数据
  -> 检查 null
  -> BML::CK::GetRowCount(table) 确认有行

第三步：定位要读的列
  -> BML::CK::FindColumn(table, "列名") 返回列号
  -> 或者直接用已知的列号

第四步：读取单元格的值
  -> BML::CK::GetInt(table, row, col, defaultValue)
  -> BML::CK::GetString(table, row, col, defaultValue)
  -> BML::CK::GetFloat(table, row, col, defaultValue)
```

最后一个参数 `defaultValue` 是安全网：如果读取失败（行号越界、列号错误、类型不匹配），函数返回这个默认值而不是崩溃。

## 第一个例子：读 CurrentLevel

`CurrentLevel` 是最常用的表。它只有 1 行，但包含了当前关卡的全部即时状态。

### CurrentLevel 的列结构

| 列号 | 列名 | 类型 | 含义 | 第一关初始值 |
| --- | --- | --- | --- | --- |
| 0 | Level ID | int | 当前关卡编号（1~13） | 1 |
| 1 | ActiveBall | object | 当前球对象 | `Ball_Wood` |
| 2 | Ball_Pos_Frame | object | 当前球生成位置使用的参考对象 | `Ball_Pos_Frame` |
| 3 | CurrentResetpoint | parameter | 当前复活点的变换矩阵 | （一串数字） |
| 4 | Activation Phase? | parameter | 小节切换中的内部状态标记 | `FALSE` |
| 5 | Points | int | 当前得分 | 0 |

对象和参数列不是字符串列，但用 `BML::CK::GetString` 可以把它们转成适合显示和写日志的文本。后面的代码读 `ActiveBall`、`CurrentResetpoint` 时就是这个用途。

玩第一关时，这些值会随游戏进程变化：

- 经过变球器 -> `ActiveBall` 从 `Ball_Wood` 变成 `Ball_Paper` 或 `Ball_Stone`
- 激活检查点 -> `CurrentResetpoint` 更新为新复活点的矩阵
- 吃到加分机关 -> `Points` 增加

### 最小读取代码

```angelscript
void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
    if (event == BML::GAME_EVENT_START_LEVEL) {
        CKDataArray@ table = ctx.BorrowDataArrayByName("CurrentLevel");
        if (table is null) {
            LogInfo(ctx, "CurrentLevel not found");
            return;
        }

        int rows = BML::CK::GetRowCount(table);
        if (rows < 1) {
            LogInfo(ctx, "CurrentLevel has no rows");
            return;
        }

        // 用列号直接读取
        int levelId = BML::CK::GetInt(table, 0, 0, -1);
        string ball = BML::CK::GetString(table, 0, 1, "?");
        int points = BML::CK::GetInt(table, 0, 5, 0);

        LogInfo(ctx, "Level=" + levelId + " Ball=" + ball + " Points=" + points);
    }
}
```

进入第一关后，日志输出：

```text
[DataReadMod] Level=1 Ball=Ball_Wood Points=0
```

## FindColumn：按列名查找

上面的代码直接用了列号（0、1、5）。这能工作，但有一个风险：如果某个自定义地图的 DataArray 列顺序不同，代码就会读错数据。

更安全的做法是用 `BML::CK::FindColumn` 按名字查找列号：

```angelscript
int colLevelId = BML::CK::FindColumn(table, "Level ID");
int colBall = BML::CK::FindColumn(table, "ActiveBall");
int colPoints = BML::CK::FindColumn(table, "Points");

// FindColumn 找不到时返回 -1
if (colLevelId < 0 || colBall < 0 || colPoints < 0) {
    LogInfo(ctx, "Column not found in CurrentLevel");
    return;
}

int levelId = BML::CK::GetInt(table, 0, colLevelId, -1);
string ball = BML::CK::GetString(table, 0, colBall, "?");
int points = BML::CK::GetInt(table, 0, colPoints, 0);
```

`FindColumn` 返回 -1 表示列不存在。这让你的代码更健壮：即使列名拼错了，也不会崩溃，只是读到默认值。

对于原版关卡，直接用列号没问题，CurrentLevel 的列结构是固定的。但如果你的 mod 需要兼容自定义地图，用 `FindColumn` 更保险。

## 完整示例：ImGui 显示 CurrentLevel

把读取和显示结合起来。保存为 `ModLoader/Mods/DataReadMod.mod.as`：

```angelscript
[bml.mod id="dataread.script" name="DataArray Reader" version="1.0.0" author="Tutorial" bml="0.3.12" description="Read and display CurrentLevel"]
class DataReadMod {
    private CKDataArray@ currentLevel = null;
    private bool showWindow = true;

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_START_LEVEL) {
            @currentLevel = ctx.BorrowDataArrayByName("CurrentLevel");
            if (currentLevel !is null) {
                LogInfo(ctx, "CurrentLevel found, rows=" + BML::CK::GetRowCount(currentLevel));
            }
        } else if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL) {
            @currentLevel = null;
        }
    }

    void OnProcess(const BML::ModContext &in ctx) {
        if (!showWindow || currentLevel is null) return;

        ImGui::SetNextWindowPos(ImVec2(10.0f, 60.0f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f), ImGuiCond_Once);

        if (ImGui::Begin("CurrentLevel")) {
            int rows = BML::CK::GetRowCount(currentLevel);
            if (rows > 0) {
                int levelId = BML::CK::GetInt(currentLevel, 0, 0, -1);
                string activeBall = BML::CK::GetString(currentLevel, 0, 1, "?");
                int points = BML::CK::GetInt(currentLevel, 0, 5, 0);

                ImGui::TextUnformatted("Level: " + levelId);
                ImGui::TextUnformatted("Ball: " + activeBall);
                ImGui::TextUnformatted("Points: " + points);
            } else {
                ImGui::TextUnformatted("No rows in CurrentLevel");
            }
        }
        ImGui::End();
    }

    private void LogInfo(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info(message);
        }
    }
}
```

### 代码逻辑解析

这个脚本的结构应该很熟悉，和上一章的坐标显示器几乎一样：

1. **进入关卡时**：获取 `CurrentLevel` 表的句柄并缓存
2. **每帧**：从缓存的句柄读取数据并用 ImGui 显示
3. **退出关卡时**：清空句柄

区别在于数据来源：坐标 mod 读的是 3D 实体的位置，这里读的是表格的单元格。

### 运行结果

进入第一关后，屏幕左上角出现窗口：

```text
┌─ CurrentLevel ─────────┐
│ Level: 1               │
│ Ball: Ball_Wood        │
│ Points: 0             │
└────────────────────────┘
```

做以下操作观察变化：

- 经过变球器 -> `Ball` 变为 `Ball_Paper` 或 `Ball_Stone`
- 吃到金色加分球 -> `Points` 从 0 开始递增
- 过检查点后变球 -> `Ball` 再次变化

面板实时反映游戏状态的变化，因为你每帧都在重新读取数据。

## 第二个例子：AllLevel 表

`AllLevel` 有 13 行，每行对应一个原版关卡的起始配置。如果说 CurrentLevel 是"现在正在发生什么"，那 AllLevel 就是"每关开始时应该怎样"。

### AllLevel 的列结构

| 列名 | 类型 | 含义 | 第 1 行的值（Level 1） |
| --- | --- | --- | --- |
| Levelfile | string | 关卡文件名 | `Level_01.NMO` |
| StartBall | string | 起始球 | `Ball_Wood` |
| StartResetpoint | int | 起始复活点编号 | 0 |
| Sky | string | 天空对象名称 | `Sky_L` |
| Light | parameter | 光照参数 | （参数值） |
| Skytranslation | parameter | 天空平移参数 | （参数值） |
| LevelBonus | int | 通关奖励分 | 100 |
| Music | int | 背景音乐编号 | 1 |

行号从 0 开始。所以第 1 关是 row=0，第 3 关是 row=2，第 13 关是 row=12。

### 读取 AllLevel

```angelscript
CKDataArray@ allLevel = ctx.BorrowDataArrayByName("AllLevel");
if (allLevel !is null) {
    int rows = BML::CK::GetRowCount(allLevel);  // 通常 13
    for (int row = 0; row < rows; row++) {
        string file = BML::CK::GetString(allLevel, row, 0, "?");
        string ball = BML::CK::GetString(allLevel, row, 1, "?");
        int bonus = BML::CK::GetInt(allLevel, row, 6, 0);
        LogInfo(ctx, "Level " + (row + 1) + ": " + file + " ball=" + ball + " bonus=" + bonus);
    }
}
```

日志输出：

```text
[DataReadMod] Level 1: Level_01.nmo ball=Ball_Wood bonus=100
[DataReadMod] Level 2: Level_02.nmo ball=Ball_Wood bonus=200
[DataReadMod] Level 3: Level_03.nmo ball=Ball_Wood bonus=300
...
[DataReadMod] Level 13: Level_13.nmo ball=Ball_Stone bonus=1300
```

从这些数据你能看出游戏设计：原版关卡的起始球大多是木球，最后一关从石球开始；通关奖励分从 100 逐关递增到 1300。

## 缓存策略

和 3D 实体一样，DataArray 句柄应该配合关卡事件管理生命周期：

```angelscript
// 成员变量
private CKDataArray@ currentLevel = null;

// 进入关卡时获取
if (event == BML::GAME_EVENT_START_LEVEL) {
    @currentLevel = ctx.BorrowDataArrayByName("CurrentLevel");
}

// 退出关卡时清空
if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL) {
    @currentLevel = null;
}
```

为什么要缓存？因为 `BorrowDataArrayByName` 每次调用都要在场景中搜索。对于每帧都要读的表（如 CurrentLevel），缓存句柄后直接用省去了搜索开销。

对于偶尔读一次的表（如 AllLevel，你可能只在关卡开始时读一次），临时获取也可以接受。

## 其他常用的读取函数

除了 `GetInt`、`GetString`、`GetFloat`，还有：

```angelscript
// 读取布尔值
bool value = BML::CK::GetBool(table, row, col, false);

// 获取列数（用于遍历所有列）
int cols = BML::CK::GetColumnCount(table);
```

这些函数的参数模式完全一致：`(表, 行号, 列号, 默认值)`。记住这个模式，用起来就不会混淆。

## 常见问题

**"BorrowDataArrayByName 返回 null"**

你可能还在菜单里。DataArray 是关卡对象，只在关卡运行时存在。确保你在 `GAME_EVENT_START_LEVEL` 之后才读取。

**"读出来的值是默认值（-1 或 '?'）"**

两种可能：
1. 列号写错了。用 `BML::CK::FindColumn` 确认列名对应的列号
2. 行号越界。先用 `GetRowCount` 确认有几行

**"表名拼错了怎么办？"**

`BorrowDataArrayByName` 返回 null，但不会报错。如果你不确定表名，先在 `GAME_EVENT_START_LEVEL` 里把查找结果打日志，确认成功后再写后续逻辑。

**"能写入 DataArray 吗？"**

可以，但这一章只做读取。写入 DataArray 会直接影响游戏状态（比如修改分数）。第 14 章会给出最低限度的写入和恢复规则，完整 API 查 [DataArray 参考](ref-dataarray.md)。

**"GetRowCount 返回 0"**

极少见。如果你确定在关卡中且表名正确，可能是时机问题，试试在 `OnProcess` 的第一帧读取而不是在事件回调里。

## 实验建议

在进入下一章之前，试试这些操作来加深理解：

1. 修改示例代码，把 `CurrentResetpoint` 也显示出来（列号 3，用 `GetString` 显示），观察过检查点时它怎么变化
2. 在 ImGui 窗口里同时显示 CurrentLevel 和 AllLevel 的通关奖励分，对比"当前分数"和"通关能拿多少分"
3. 用 `GetColumnCount` 查看 CurrentLevel 有几列，再和上面的列结构对照

---

**完成状态**：ImGui 里显示 CurrentLevel 表的内容，理解 DataArray 的读取流程

---

下一步：[12 状态面板 Mod](tutorial-12-status-panel-mod.md)

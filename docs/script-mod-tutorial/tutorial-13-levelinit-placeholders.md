# 13 Levelinit 如何处理占位符

上一章做了状态面板，从 CurrentLevel 表读取关卡号、球名、分数。但这些数据是哪里来的？第 10 章提到 Levelinit 负责"整理对象、创建组和运行时表"，当时只是一句话带过。这一节看 Levelinit 的核心流程：Process Placeholder（处理占位符），看看它如何把关卡文件里的原始对象变成运行时可用的状态。

理解这个流程之后，你就能回答："PC_Checkpoints 组是怎么进入运行时的？""PH_Groups 表里那些数字是什么意思？""为什么 GAME_EVENT_START_LEVEL 的时候一切都已经就绪了？"

## Levelinit_build 的整体流程

Levelinit.nmo 被 base.cmo 启动后，执行的主入口是 `Levelinit_build`。这是一个规模很大的行为图（993 个节点、1119 条边），但它的顶层结构可以整理成线性流程：

```text
Levelinit_build 顶层子图（按执行顺序）
---------------------------------------------
1. load Music             加载关卡音乐
2. Load LevelXX           加载关卡内容文件 (Level_01.NMO 等)
3. set CurrentLevel       初始化 CurrentLevel 表
4. Init Arrays            初始化运行时所需的空表
5. set Mapping/Textures   设置材质和纹理映射
6. Process Placeholder    处理占位符 <-- 本章重点
7. init Level             初始化关卡运行参数
8. load Tutorial.nmo      加载教程相关内容
9. Send Message           通知 base.cmo 初始化完成
10. Preload Sound         预加载音效
11. Preload Textures      预加载纹理
```

第 6 步 Process Placeholder 是最大的子图（558 个节点、623 条边），占了 Levelinit 超过一半的复杂度。它的职责是：把关卡文件里以 `P_`、`PC_`、`PR_`、`PS_`、`PE_` 等前缀命名的对象，按分类规则整理成组和表。

## Process Placeholder 做了什么

Process Placeholder 内部的顶层结构：

```text
Process Placeholder 内部子图
---------------------------------------------
1. Count PH                    统计各类占位符的数量
2. init Placeholders           初始化占位符处理所需的数组
3. load Placeholder            加载占位符处理的辅助对象
4. Copy Objects                复制运行时需要的对象副本
5. Set Sector Attribute        设置小节属性
6. Replace PH                  替换占位符为实际对象
7. set MF-Objects in Array     把 MF 对象放入数组
8. init and load Checkpoints   初始化并加载检查点
9. init Resetpoints            初始化复活点
10. init and load Levelstart   初始化起点
11. init and load Levelende    初始化终点
12. fill DepthTest Group       填充深度测试组
```

用日常语言描述这个过程：

- 第 1-3 步是准备工作。统计关卡里有多少个 P_Extra_Point、多少个 P_Box、多少个 PC_Checkpoints，把结果写入 PH_Groups 表。
- 第 4-7 步是核心处理。把占位符对象替换为运行时对象，设置好物理属性和小节归属。
- 第 8-11 步是特殊对象处理。检查点、复活点、起点、终点各有独立的初始化逻辑，处理完后对应的 DataArray（Checkpoints、ResetPoints）就有了数据。
- 第 12 步是收尾。设置渲染相关的分组。

## PH_Groups 表：占位符的分类注册表

PH_Groups 是 Process Placeholder 的入口数据。它记录了所有占位符分类及其处理规则。

### 表结构

| 列号 | 列名 | 类型 | 含义 |
| --- | --- | --- | --- |
| 0 | Group Names | string | 占位符组名（如 P_Extra_Point） |
| 1 | Amount | int | 当前关卡中该类对象的数量 |
| 2 | Activation | int | 激活分类编号 |
| 3 | Reset | int | 复位分类编号 |

### PH_Groups 的典型内容（23 行）

| Group Names | Amount | Activation | Reset | 说明 |
| --- | --- | --- | --- | --- |
| P_Extra_Life | 视关卡 | 1 | 1 | 额外生命 |
| P_Extra_Point | 视关卡 | 1 | 1 | 加分道具 |
| P_Trafo_Paper | 视关卡 | 1 | 0 | 纸球变球器 |
| P_Trafo_Stone | 视关卡 | 1 | 0 | 石球变球器 |
| P_Trafo_Wood | 视关卡 | 1 | 0 | 木球变球器 |
| P_Ball_Paper | 视关卡 | 3 | 2 | 纸球 |
| P_Ball_Stone | 视关卡 | 3 | 2 | 石球 |
| P_Ball_Wood | 视关卡 | 3 | 2 | 木球 |
| P_Box | 视关卡 | 2 | 2 | 可推箱子 |
| P_Dome | 视关卡 | 2 | 2 | 球形机关 |
| P_Modul_01 ~ 41 | 视关卡 | 1 | 1 | 各类关卡模块 |

### Activation 和 Reset 的含义

这两列是分类编号，决定 Gameplay 在小节切换和死亡复位时如何处理该类对象。

Activation 值的含义：

```text
0 = 不参与激活/停用流程
1 = 随小节激活/停用（进入某小节时显示，离开时隐藏）
2 = 随小节激活/停用 + 参与物理（箱子、球体等需要物理仿真的对象）
3 = 球类专用处理
```

Reset 值的含义：

```text
0 = 死亡时不复位（变球器只需要显示在原处，不需要恢复状态）
1 = 死亡时复位到初始状态（道具被吃掉后，死亡会让它重新出现）
2 = 死亡时复位位置和物理状态（箱子被推开后，死亡会让它回到原位）
```

举例：P_Extra_Point 的 Activation=1、Reset=1，意味着加分道具随小节显示/隐藏，死亡时已被吃掉的道具会重新出现。P_Box 的 Activation=2、Reset=2，意味着箱子需要物理仿真，死亡时回到原位。

## 从 PC_Checkpoints 到 Checkpoints 表

PC_Checkpoints 组里的对象（如 PC_TwoFlames_01、PC_TwoFlames_02）是关卡作者放置的检查点标记。Process Placeholder 的第 8 步"init and load Checkpoints"做的事情：

```text
1. 遍历 PC_Checkpoints 组的所有成员
2. 对每个检查点对象，记录它的位置矩阵
3. 加载检查点的可视化辅助对象（火焰柱模型）
4. 把矩阵和对象引用写入 Checkpoints DataArray
5. Gameplay 后续通过读取 Checkpoints 表判断球是否触碰了检查点
```

Checkpoints 表的结构：

| 列 | 内容 |
| --- | --- |
| Matrix | 检查点的位置/方向矩阵 |
| Object | 关联的运行时对象引用 |

类似地，PR_Resetpoints 组经过"init Resetpoints"后变成 ResetPoints 表：

| 列 | 内容 |
| --- | --- |
| Resetpoint | 复活点的位置/方向矩阵 |

当球经过检查点时，Gameplay 更新 CurrentLevel 表的 CurrentResetpoint 列，指向 ResetPoints 表中对应行的矩阵。死亡时球被传送到这个矩阵位置。

## 动手验证：读取 PH_Groups

保存为 `ModLoader/Mods/PHGroupsMod.mod.as`：

```angelscript
[bml.mod id="phgroups.script" name="PH Groups Reader" version="1.0.0" author="Tutorial" bml="0.3.12" description="Read PH_Groups table"]
class PHGroupsMod {

    void OnLoad(const BML::ModContext &in ctx) {
        LogInfo(ctx, "PHGroupsMod loaded");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_START_LEVEL) {
            ReadPHGroups(ctx);
            ReadCheckpoints(ctx);
            ReadResetPoints(ctx);
        }
    }

    private void ReadPHGroups(const BML::ModContext &in ctx) {
        CKDataArray@ table = ctx.BorrowDataArrayByName("PH_Groups");
        if (table is null) {
            LogInfo(ctx, "PH_Groups not found");
            return;
        }

        int rows = BML::CK::GetRowCount(table);
        int cols = BML::CK::GetColumnCount(table);
        LogInfo(ctx, "PH_Groups: rows=" + rows + " columns=" + cols);

        int colName = BML::CK::FindColumn(table, "Group Names");
        int colAmount = BML::CK::FindColumn(table, "Amount");
        int colActivation = BML::CK::FindColumn(table, "Activation");
        int colReset = BML::CK::FindColumn(table, "Reset");

        if (colName < 0 || colAmount < 0 || colActivation < 0 || colReset < 0) {
            LogInfo(ctx, "PH_Groups: column not found");
            return;
        }

        for (int i = 0; i < rows; i++) {
            string name = BML::CK::GetString(table, i, colName, "");
            int amount = BML::CK::GetInt(table, i, colAmount, 0);
            int activation = BML::CK::GetInt(table, i, colActivation, 0);
            int reset = BML::CK::GetInt(table, i, colReset, 0);

            LogInfo(ctx, "  " + name
                + " amount=" + amount
                + " activation=" + activation
                + " reset=" + reset);
        }
    }

    private void ReadCheckpoints(const BML::ModContext &in ctx) {
        CKDataArray@ table = ctx.BorrowDataArrayByName("Checkpoints");
        if (table is null) {
            LogInfo(ctx, "Checkpoints not found");
            return;
        }

        int rows = BML::CK::GetRowCount(table);
        LogInfo(ctx, "Checkpoints: rows=" + rows);
    }

    private void ReadResetPoints(const BML::ModContext &in ctx) {
        CKDataArray@ table = ctx.BorrowDataArrayByName("ResetPoints");
        if (table is null) {
            LogInfo(ctx, "ResetPoints not found");
            return;
        }

        int rows = BML::CK::GetRowCount(table);
        LogInfo(ctx, "ResetPoints: rows=" + rows);
    }

    private void LogInfo(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info(message);
        }
    }
}
```

## 预期输出

进入第一关后，日志应该显示类似内容：

```text
[PHGroupsMod] PH_Groups: rows=23 columns=4
[PHGroupsMod]   P_Extra_Life amount=1 activation=1 reset=1
[PHGroupsMod]   P_Extra_Point amount=11 activation=1 reset=1
[PHGroupsMod]   P_Trafo_Paper amount=1 activation=1 reset=0
[PHGroupsMod]   P_Trafo_Stone amount=1 activation=1 reset=0
[PHGroupsMod]   P_Trafo_Wood amount=0 activation=1 reset=0
[PHGroupsMod]   P_Ball_Paper amount=1 activation=3 reset=2
[PHGroupsMod]   P_Ball_Stone amount=1 activation=3 reset=2
[PHGroupsMod]   P_Ball_Wood amount=1 activation=3 reset=2
[PHGroupsMod]   P_Box amount=8 activation=2 reset=2
[PHGroupsMod]   P_Dome amount=0 activation=2 reset=2
[PHGroupsMod]   P_Modul_01 amount=2 activation=1 reset=1
[PHGroupsMod]   ...
[PHGroupsMod] Checkpoints: rows=3
[PHGroupsMod] ResetPoints: rows=4
```

几个要点：

- Amount 随关卡变化。第一关有 11 个 P_Extra_Point，其他关卡数量不同。
- P_Trafo_Wood 在第一关 amount=0，因为第一关没有木球变球器。但这一行仍然存在于表中，Levelinit 预先注册了所有可能的分类。
- Checkpoints rows=3 对应第一关的 3 个检查点，ResetPoints rows=4 对应 4 个复活点。

## 整体数据流

把从关卡文件到运行时的完整流程画出来：

```text
Level_01.NMO（关卡文件）
  |
  |  包含命名为 P_Extra_Point_01, PC_TwoFlames_01,
  |  PR_Resetpoint_01 等的 3D 对象
  |
  v
Levelinit_build 启动
  |
  |  步骤 1-5: 加载音乐、关卡文件、初始化表
  |
  v
Process Placeholder（步骤 6）
  |
  |-- Count PH -> 统计各 P_ 前缀组的数量 -> 填入 PH_Groups.Amount
  |
  |-- Replace PH -> 用实际运行时对象替换占位符
  |
  |-- init and load Checkpoints
  |     PC_Checkpoints 组 -> 遍历 -> Checkpoints 表
  |
  |-- init Resetpoints
  |     PR_Resetpoints 组 -> 遍历 -> ResetPoints 表
  |
  |-- init and load Levelstart
  |     PS_Levelstart 组 -> 起始位置
  |
  |-- init and load Levelende
  |     PE_Levelende 组 -> 终点位置
  |
  v
Levelinit 完成，通知 base.cmo
  |
  v
Gameplay.nmo 启动
  |
  |  读取 Checkpoints 表、ResetPoints 表、PH_Groups 表
  |  执行球的物理控制、碰撞检测、计分
  |
  v
BML 发出 GAME_EVENT_START_LEVEL
  |
  v
你的脚本开始工作
  所有表已填充，所有组已就绪
```

## 对 mod 开发的意义

理解 Process Placeholder 后，你就明白了几个关键问题：

**为什么 GAME_EVENT_START_LEVEL 时一切已经就绪？**

因为 Levelinit 在你的脚本收到事件之前就完成了全部初始化。PH_Groups、Checkpoints、ResetPoints 都已经有了正确数据。

**为什么 PH_Groups 总是 23 行？**

因为 Levelinit 预注册了所有支持的占位符分类。某一关没有某类对象时，对应行的 Amount 为 0，但行本身仍然存在。

**为什么改 PH_Groups 的 Amount 没效果？**

Amount 是 Levelinit 统计的结果，Gameplay 用的是实际存在的对象。直接修改 Amount 列不会凭空创建或删除对象。

**能不能在运行时动态添加检查点？**

Checkpoints 表是 Levelinit 在初始化时一次性写入的。运行时往表里加行，Gameplay 不一定能正确识别，因为它的行为图逻辑已经按初始行数运行。这类修改属于高风险操作，需要对行为图有更深理解。

## 常见问题

**"PH_Groups 和 PH 表有什么区别？"**

PH_Groups 是分类索引，记录每类占位符的名字和处理规则。PH 是另一张表，保存具体对象的替换结果。初学阶段先掌握 PH_Groups 就够了。

**"Activation=3 和 Activation=2 有什么区别？"**

Activation=2 是普通物理对象（箱子、球形机关），随小节激活/停用。Activation=3 是球类专用，涉及更复杂的球切换和变球器逻辑。

**"我进入第二关，Amount 数字变了"**

正常。Amount 反映的是当前加载的关卡文件里该类对象的实际数量。每关内容不同，数量自然不同。

**"Reset=0 是什么意思？"**

Reset=0 表示死亡时不需要复位这类对象。变球器（P_Trafo_*）就是 Reset=0，因为变球器只是一个固定的装置，球碰到就变身，死亡后它不需要回到某个"初始状态"。

**"我修改了 Activation 列的值会怎样？"**

如果在 GAME_EVENT_START_LEVEL 之后修改，Gameplay 已经按原值设置了处理逻辑，修改不会追溯生效。如果用 hook 在 Levelinit 执行过程中修改（BML C++ mod 才能做到），有可能影响后续处理，但这超出了脚本 mod 的范围。

---

**完成状态**：能解释 Levelinit_build 的顶层流程，能说出 Process Placeholder 如何把 PC_Checkpoints 组变成 Checkpoints 表，能读取 PH_Groups 并解释 Activation/Reset 列的分类含义

---

下一步：[14 Gameplay 状态机](tutorial-14-gameplay-state-machine.md)

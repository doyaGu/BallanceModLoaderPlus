# 19 修改前检查清单

## 动机

前面的章节从只读开始：读 DataArray 的表格内容、读物理参数表、读对象位置。然后逐步过渡到创建：生成新物理对象、施加冲量和持续力。每一步都在自己创建的对象上操作，风险可控。

但下一步呢？你可能想修改游戏里已有的对象，改写 DataArray 的值，或者干预原版行为图的运行结果。这些操作的风险远高于"读"和"创建自己的东西"。改错了，轻则当前关卡行为异常，重则游戏崩溃。

本章建立一套检查框架。在你动手修改任何游戏状态之前，用这五个问题过一遍，就能判断这个操作属于低风险、中风险还是高风险。

## 三个风险层级

### 低风险（初学者可放心做）

这些操作不会改变游戏状态，最坏情况是输出一条错误日志：

- 打印日志、显示 BML 消息
- 注册命令、读取配置
- Timer 定时输出
- 只读查找对象（`BorrowXxxByName` 后只读取不修改）
- 只读打印 group 成员数量
- 只读打印 DataArray 内容

**共同特征**：所有操作都是 read-only，不调用任何 Set/Write/Physicalize 类 API。

### 中风险（需要验证后再做）

这些操作会修改非关键的游戏状态，或者保存了对象引用供后续使用：

- 临时 Show/Hide 非关键的调试对象
- 创建自己的调试标记物
- 读取当前球的位置（读本身无害，但如果你缓存了引用，关卡切换后引用失效）
- 读 CurrentLevel 表（同上，表对象在关卡切换后可能被销毁重建）
- 用 `CKObject@` 或 `CK3dEntity@` 保存对象引用供跨帧使用
- 关卡加载后通过 GameEvent 重新查找对象

**共同特征**：操作本身影响有限，但如果忽略生命周期管理，可能在关卡切换时崩溃。

### 高风险（必须完成所有检查才能做）

这些操作直接修改游戏核心状态，出错可能导致不可恢复的异常：

- 写入 CurrentLevel 表（改关卡号、改活动球名称）
- 写入 IngameParameter 表
- 修改 PH_Groups 的成员
- 移动当前活动球的位置
- 对原版对象执行 Re-Physicalize
- 修改 Gameplay.nmo 的行为图
- 干预检查点/复活点流程
- 改变 Sector 激活逻辑
- 直接操作物理控制器

**共同特征**：修改的数据会被原版行为图持续引用。你的修改可能在下一帧就被原版逻辑覆盖，或者原版逻辑读到异常值后进入未定义状态。

## 五个核心问题

在动手写任何修改代码之前，逐条回答这五个问题：

| 序号 | 问题 | 目的 |
| --- | --- | --- |
| 1 | 我是否已经用只读代码观察过目标的当前状态？ | 确认目标存在、值符合预期 |
| 2 | 我是否清楚目标对象的生命周期？ | 知道它何时创建、何时销毁 |
| 3 | 如果目标是 DataArray，我是否清楚每一列的含义和类型？ | 防止写错列、写错类型 |
| 4 | 我是否知道哪个原版行为图也在写这个值？ | 防止被覆盖或产生竞争 |
| 5 | 如果操作出错，我能否回滚到安全状态？ | 保证玩家不需要重启游戏 |

如果第 1 题的答案是"否"，停下来，先写一个只读版本。
如果第 2、3、4 题有任何一个答案是"不确定"，这个操作至少是高风险。
如果第 5 题的答案是"不能"，你需要设计一个恢复机制再动手。

## 对象验证清单

每次获取一个游戏对象后，在使用它之前检查以下条件：

```angelscript
// 对象验证：使用前必须通过所有检查
private bool ValidateTarget(const BML::ModContext &in ctx,
                            CK3dEntity@ target,
                            const string &in expectedName) {
    // 检查 1：是否为 null
    if (target is null) {
        LogWarn(ctx, "Target is null");
        return false;
    }

    // 检查 2：对象是否仍然有效（可能已被引擎销毁）
    if (!BML::CK::IsValid(target)) {
        LogWarn(ctx, "Target is no longer valid");
        return false;
    }

    // 检查 3：名字是否匹配预期
    string actualName = BML::CK::GetName(target);
    if (actualName != expectedName) {
        LogWarn(ctx, "Name mismatch: expected=" + expectedName
                     + " actual=" + actualName);
        return false;
    }

    return true;
}
```

每项检查的意义：

| 检查 | 失败时说明什么 |
| --- | --- |
| `is null` | 查找失败，对象不存在于当前上下文 |
| `IsValid` | 对象曾经存在但已被销毁（关卡切换、行为图删除） |
| 名字匹配 | 可能找到了同名但不同用途的对象，或名字被修改 |
还有两项根据操作需要额外检查：

```angelscript
// 如果要执行物理操作，脚本侧没有“是否已物理化”的查询函数。
// 对自己创建的对象，用一个 bool 记录 PhysicalizeBall 是否成功。
// 对游戏原有对象，检查 WakeUp / Impulse / SetForce 的返回值。

// 如果是动态对象（你创建的），确认 Dynamic 标记
// 静态对象（关卡自带的地形）不应被移动
```

## DataArray 验证清单

### 读取前验证

```angelscript
private bool ValidateTableForRead(const BML::ModContext &in ctx,
                                   const string &in tableName,
                                   int expectedColumns,
                                   int minRows) {
    CKDataArray@ table = ctx.BorrowDataArrayByName(tableName);

    // 表是否存在
    if (table is null) {
        LogWarn(ctx, "Table not found: " + tableName);
        return false;
    }

    // 列数是否匹配预期
    int cols = BML::CK::GetColumnCount(table);
    if (cols < expectedColumns) {
        LogWarn(ctx, tableName + " columns=" + cols
                     + " expected>=" + expectedColumns);
        return false;
    }

    // 行数是否满足最低要求
    int rows = BML::CK::GetRowCount(table);
    if (rows < minRows) {
        LogWarn(ctx, tableName + " rows=" + rows
                     + " expected>=" + minRows);
        return false;
    }

    return true;
}
```

### 写入前的额外检查

读取验证通过后，如果你打算写入，还需要回答：

| 检查项 | 问自己 | 后果 |
| --- | --- | --- |
| 写入时机 | 我在哪个回调里写？原版行为图会不会在下一帧覆盖我写的值？ | 写了白写，或者产生一帧闪烁 |
| 写入列类型 | 我写的值类型和列声明类型一致吗？整数列写入浮点会怎样？ | 类型不匹配时写入失败或值截断 |
| 连锁影响 | 原版行为图会读这个值做决策吗？我写入的值在它的合法范围内吗？ | 行为图读到异常值可能跳转到错误分支 |
| 恢复策略 | 关卡重新开始时，这张表会被重建吗？还是我的修改会残留？ | 残留的错误值影响下一局 |

## 物理验证清单

在对任何对象执行物理操作前：

```angelscript
private bool ValidateForPhysics(const BML::ModContext &in ctx,
                                 CK3dEntity@ target,
                                 bool knownPhysicalized) {
    if (target is null || !BML::CK::IsValid(target)) {
        LogWarn(ctx, "Physics target invalid");
        return false;
    }

    // 目标必须是 CK3dEntity（不是 CK2dEntity、不是 CKGroup）
    // 如果你是通过 BorrowXxxByName 取得的，类型已经确定
    // 如果是通过遍历得到的 CKObject@，必须先 cast

    // 脚本 API 没有“是否已物理化”的查询函数。
    // 自己创建的对象应保存 PhysicalizeBall 的返回值。
    if (!knownPhysicalized) {
        LogWarn(ctx, BML::CK::GetName(target) + " is not physicalized");
        return false;
    }

    return true;
}
```

物理操作的特有注意事项：

| 操作 | 前提条件 | 常见陷阱 |
| --- | --- | --- |
| WakeUp | 对象已物理化，或你准备用返回值判断状态 | 对未物理化对象调用返回 false，不会崩溃 |
| Impulse | 对象已物理化且非休眠（或先 WakeUp） | 休眠对象可能忽略冲量 |
| SetForce | 同上 | 忘记 ClearForce 导致对象永远加速 |
| ClearForce | 之前调用过 SetForce | 对未设力的对象调用无害但浪费 |
| Physicalize | 对象是 CK3dEntity，尚未物理化 | 重复物理化可能导致异常 |
| Unphysicalize | 对象已物理化 | 必须在 Hide/Destroy 之前调用 |

## 决策流程

当你有一个想法"我想让 X 发生"时，按照以下流程判断：

```text
[我想做什么？]
   |
   v
[这个操作只读取数据，不修改任何东西？]
   |-- 是 -> 低风险。直接写代码，加 null 检查即可。
   |
   |-- 否
   v
[我操作的对象是自己创建的？]
   |-- 是 -> 中风险。确保有清理逻辑（OnGameEvent + OnUnload）。
   |
   |-- 否
   v
[我操作的对象是游戏原有的？]
   |-- 是
   v
[我知道原版哪个行为图也在操作这个对象？]
   |-- 否 -> 停下来。先用只读代码观察该对象的行为规律。
   |
   |-- 是
   v
[我的修改会被原版逻辑在下一帧覆盖？]
   |-- 是 -> 需要 hook 或者在正确时机写入。复杂度高，属于高风险。
   |
   |-- 否
   v
[关卡重新开始时，我的修改会被自然重置？]
   |-- 是 -> 高风险但可控。写入后如果出错，重启关卡即可恢复。
   |
   |-- 否 -> 最高风险。必须实现手动恢复逻辑。
```

## 常见错误及后果

| 错误 | 后果 | 正确做法 |
| --- | --- | --- |
| 缓存了 `CK3dEntity@` 引用，关卡切换后继续使用 | 访问已释放内存，崩溃 | 在 `GAME_EVENT_PRE_EXIT_LEVEL` 中清空引用，下次使用前重新查找 |
| 写入 CurrentLevel 的 ActiveBall 列为不存在的球名 | 原版变球逻辑找不到对应对象，变球失败或崩溃 | 只写入已知合法值：`Ball_Wood`、`Ball_Paper`、`Ball_Stone` |
| 对原版球执行 Unphysicalize 后不恢复 | 球不再受重力影响，悬浮在空中，玩家无法继续游戏 | 在操作完成后立刻 Re-Physicalize，或在关卡重启时让原版流程自动恢复 |
| SetForce 作用于原版球后忘记 ClearForce | 球持续加速直到飞出地图，触发坠落判定反复重生 | 帧计数器模式（第 18 章的 TickForce），到期自动清除 |
| 修改了 IngameParameter 的分数列但不知道原版何时读它 | 分数显示和实际值不一致，可能影响通关结算 | 先用只读代码在不同时机打印该值，观察原版的读写频率 |
| 在 OnLoad 中查找游戏对象 | 此时关卡尚未加载，对象不存在，返回 null | 在 `GAME_EVENT_START_LEVEL` 之后才查找关卡内对象 |

## 检查清单总结

动手前，在脑中快速过一遍：

1. **已观察**：我用只读代码看过目标的值/行为了吗？
2. **生命周期**：目标对象在哪个回调之后才存在？在哪个事件时被销毁？
3. **数据含义**：如果是 DataArray，我确认过列号、列名、类型了吗？
4. **原版写者**：有没有原版行为图也在写同一个字段？它的写入频率是什么？
5. **回滚策略**：出错时怎么恢复？关卡重启能重置吗？需要我手动恢复吗？

五项全部确认后再写修改代码。如果有任何一项回答不上来，先写只读观察脚本收集信息。

## 本章要点

- 所有操作分为三个风险层级：只读（低）、操作自己的对象（中）、操作原版对象（高）
- 五个核心问题覆盖了"是否准备好修改"的全部维度
- 对象验证四步：非 null、IsValid、名字匹配、在当前场景
- DataArray 写入前必须确认：时机、列类型、连锁影响、恢复策略
- 物理操作前必须确认对象已物理化
- 决策流程的核心逻辑：越靠近"原版也在操作的状态"，风险越高
- 无法回答的问题用只读观察来填补，观察永远是安全的第一步

## 完成状态

能够对任何"我想做 X"的想法快速判断风险等级。掌握了对象验证、DataArray 验证、物理验证三套检查代码模板。建立了"先观察后修改"的习惯。

下一步：[20 完整 Mod](tutorial-20-complete-mod.md)

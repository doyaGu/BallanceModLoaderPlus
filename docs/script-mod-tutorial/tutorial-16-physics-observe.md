# 16 物理观察

## 游戏现象

打开 Ballance，进入第一关。纸球落在轨道上，接触面后反弹了一下，然后开始沿斜面滚动。换成石球，它几乎不弹，但滚动时更难停住。换成木球，弹性介于两者之间，质量适中。

这些行为不是动画。它们是物理仿真的结果。物理引擎根据摩擦系数、弹性系数、质量等参数，实时计算每一帧球的位置和旋转。

但是，不是所有你在画面里看到的东西都参与物理仿真。那些装饰性的柱子、背景墙壁、文字标牌，它们只是摆在那里的 3D 模型。它们不会响应重力，不会被推动，不会和球碰撞（除非另有设置）。

**区分的关键**：一个 3D 对象是否参与物理，取决于它是否经过了"物理化"(Physicalize) 这个步骤。物理化就像给一个模型注入灵魂，告诉物理引擎："这个对象存在，它有这些物理属性，请开始对它做仿真。"

本章的目标：用脚本观察这个过程，看看哪些对象被物理化了，参数是什么。

## 物理参数存在哪里

Ballance 的物理参数不是写死在代码里的。它们存储在名为 `Physicalize_Balls` 的 DataArray 中（上一阶段教程介绍过 DataArray）。这张表的结构是：

| 列索引 | 内容 | 例子 |
| --- | --- | --- |
| 0 | 对象名称（字符串） | `P_Ball_Paper` |
| 1 | 摩擦系数（浮点） | `0.3` |
| 2 | 弹性系数（浮点） | `0.5` |
| 3 | 质量（浮点） | `1.0` |

当关卡加载完成后，原版的行为图（Behavior Graph）会读取这张表，对表中列出的每个对象执行物理化操作。也就是说，DataArray 是"配方"，行为图是"厨师"，物理化是"做菜"的动作。

## 第一步：读取物理参数表

先写一个脚本，在关卡开始时读取 `Physicalize_Balls` 表的内容。这让我们看到"配方"本身：

```angelscript
void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
    if (event == BML::GAME_EVENT_START_LEVEL) {
        CKDataArray@ physBalls = ctx.BorrowDataArrayByName("Physicalize_Balls");
        if (physBalls !is null) {
            int rows = BML::CK::GetRowCount(physBalls);
            for (int i = 0; i < rows; i++) {
                string name = BML::CK::GetString(physBalls, i, 0, "?");
                float friction = BML::CK::GetFloat(physBalls, i, 1, 0.0f);
                float elasticity = BML::CK::GetFloat(physBalls, i, 2, 0.0f);
                float mass = BML::CK::GetFloat(physBalls, i, 3, 0.0f);
                LogInfo(ctx, name + " friction=" + friction
                             + " elasticity=" + elasticity
                             + " mass=" + mass);
            }
        }
    }
}
```

注意这里用的是 `BML::CK::GetString` 和 `BML::CK::GetFloat`，不是 `ReadString` / `ReadFloat`。参数含义是：表对象、行号、列号、默认值。

运行后，日志中会出现类似：

```text
P_Ball_Paper friction=0.3 elasticity=0.5 mass=1
P_Ball_Wood friction=0.6 elasticity=0.2 mass=2
P_Ball_Stone friction=0.7 elasticity=0.1 mass=10
```

对照游戏里的实际感受：纸球弹性最高（0.5），所以弹得最明显；石球质量最大（10），所以推起来很沉。这些数字直接决定了手感。

## 第二步：观察物理化事件

读表只是看到了"配方"。真正的物理化发生在什么时候？BML 提供了 `OnPhysicalize` 回调函数。每当游戏中有对象被物理引擎接管时，BML 会调用这个函数，并传入一个 `PhysicalizeEvent` 对象。

```angelscript
void OnPhysicalize(const BML::ModContext &in ctx,
                   const BML::PhysicalizeEvent &in event) {
    LogInfo(ctx, "Physicalize: " + event.TargetName +
                 " friction=" + event.Friction +
                 " elasticity=" + event.Elasticity +
                 " mass=" + event.Mass);
}
```

`PhysicalizeEvent` 提供以下信息：

| 属性 | 含义 | 说明 |
| --- | --- | --- |
| `TargetName` | 被物理化的对象名 | 比如 `P_Ball_Paper_01` |
| `Friction` | 摩擦系数 | 0 到 1，越大越粗糙 |
| `Elasticity` | 弹性系数 | 0 到 1，越大反弹越高 |
| `Mass` | 质量 | 越大越难推动 |
| `BallCount` | 球形碰撞体数量 | 通常为 1（球形物理化） |
| `CollisionSurface` | 碰撞面名称 | 用于确定碰撞形状的 Mesh 名 |

如果 `BallCount > 0`，还可以用 `event.GetBallRadius(index)` 获取碰撞球的半径。

## Level 01 加载时会发生什么

当你进入第一关时，以下事情按顺序发生：

1. 关卡文件加载完成，所有 3D 对象进入场景（此时它们只是"摆设"）
2. 行为图开始执行，读取 `Physicalize_Balls` 表
3. 对表中每个对象执行物理化，此时 `OnPhysicalize` 触发
4. 玩家球出现在起点，开始响应重力

你会在日志中看到多次 `OnPhysicalize` 触发。第一关通常有纸球、木球、石球各一个实例（命名带后缀如 `_01`），再加上地面、轨道等碰撞体。一个关卡可能触发 5-15 次物理化事件。

通过对比"表中的数据"和"事件中的数据"，你可以验证两者是否一致。如果一致，说明行为图确实按表里的参数执行了物理化。

## 概念理解：为什么要区分"3D 对象"和"物理对象"

你可能会问：为什么不让所有对象都参与物理？原因有两个：

**性能**：物理仿真是昂贵的计算。一个关卡里可能有几百个 3D 对象（装饰物、背景、粒子效果），如果全部参与物理计算，帧率会直接崩溃。

**控制**：有些对象你不希望它被推走。比如轨道本身，它是"固定的"。再比如 UI 文字，它压根不需要物理。

所以 Ballance 的设计是：默认情况下，加载进场景的对象只是视觉存在，不参与物理。你需要明确地物理化一个对象，它才会响应力、碰撞、重力。

这也是为什么下一章（创建物理对象）里会有"加载"和"物理化"两个独立步骤，它们是不同的事情。

## 完整观察脚本

新建 `ModLoader/Mods/PhysObserve.mod.as`：

```angelscript
[bml.mod id="phys.observe" name="Physics Observer" version="1.0.0" author="Tutorial" bml="0.3.12" description="Observe physics parameters"]
class PhysObserve {
    private int eventCount = 0;

    void OnLoad(const BML::ModContext &in ctx) {
        LogInfo(ctx, "PhysObserve loaded");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_START_LEVEL) {
            ReadPhysTable(ctx);
            LogInfo(ctx, "Total physicalize events so far: " + eventCount);
        }
    }

    void OnPhysicalize(const BML::ModContext &in ctx,
                       const BML::PhysicalizeEvent &in event) {
        eventCount++;
        string info = "#" + eventCount + " " + event.TargetName +
                      " friction=" + event.Friction +
                      " elasticity=" + event.Elasticity +
                      " mass=" + event.Mass;

        if (event.BallCount > 0) {
            info += " radius=" + event.GetBallRadius(0);
        }

        LogInfo(ctx, info);
    }

    private void ReadPhysTable(const BML::ModContext &in ctx) {
        CKDataArray@ physBalls = ctx.BorrowDataArrayByName("Physicalize_Balls");
        if (physBalls is null) {
            LogInfo(ctx, "Physicalize_Balls not found");
            return;
        }

        int rows = BML::CK::GetRowCount(physBalls);
        LogInfo(ctx, "Physicalize_Balls rows=" + rows);

        for (int i = 0; i < rows; i++) {
            string name = BML::CK::GetString(physBalls, i, 0, "?");
            float friction = BML::CK::GetFloat(physBalls, i, 1, 0.0f);
            float elasticity = BML::CK::GetFloat(physBalls, i, 2, 0.0f);
            float mass = BML::CK::GetFloat(physBalls, i, 3, 0.0f);
            LogInfo(ctx, name + " friction=" + friction + " elasticity=" + elasticity + " mass=" + mass);
        }
    }

    private void LogInfo(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info(message);
        }
    }
}
```

这个脚本做了两件事：

1. **关卡开始时**读取 `Physicalize_Balls` 表（看"配方"）
2. **实时监听** `OnPhysicalize` 事件（看"做菜"过程）

`eventCount` 的作用是给每个事件编号，方便在日志中追踪顺序。

## 运行验证

启动 Player，进入第一关。打开 `ModLoader/ModLoader.log`，应该能看到两组输出。

第一组是表的内容：

```text
[phys.observe/INFO]: PhysObserve loaded
[phys.observe/INFO]: Physicalize_Balls rows=3
[phys.observe/INFO]: P_Ball_Paper friction=0.3 elasticity=0.5 mass=1
[phys.observe/INFO]: P_Ball_Wood friction=0.6 elasticity=0.2 mass=2
[phys.observe/INFO]: P_Ball_Stone friction=0.7 elasticity=0.1 mass=10
[phys.observe/INFO]: Total physicalize events so far: 5
```

第二组是物理化事件（可能出现在表输出之前，因为物理化发生在关卡加载过程中）：

```text
[phys.observe/INFO]: #1 P_Ball_Paper_01 friction=0.3 elasticity=0.5 mass=1 radius=2
[phys.observe/INFO]: #2 P_Ball_Wood_01 friction=0.6 elasticity=0.2 mass=2 radius=2
[phys.observe/INFO]: #3 P_Ball_Stone_01 friction=0.7 elasticity=0.1 mass=10 radius=2
```

注意事件中的名称带后缀（`_01`），而表中的名称不带。这是因为表记录的是"模板名"，而实际场景里的对象是模板的实例。

## 失败诊断

| 现象 | 可能原因 | 解决方法 |
| --- | --- | --- |
| 脚本不加载 | 文件名不是 `.mod.as` 结尾 | 改为 `PhysObserve.mod.as` |
| 表为 null | 拼写错误或关卡未完全加载 | 检查 `Physicalize_Balls` 拼写，确认在 `GAME_EVENT_START_LEVEL` 时读取 |
| 事件数为 0 | `OnPhysicalize` 函数签名不正确 | 确认参数类型完全匹配：`const BML::ModContext &in ctx, const BML::PhysicalizeEvent &in event` |
| 看到事件但没有 radius | 某些对象不用球形碰撞体 | 正常现象，检查 `BallCount` 是否为 0 |
| 日志中表数据和事件参数不一致 | 可能有自定义行为图修改了参数 | 对于原版关卡，两者应该一致 |

## 本章要点

- 3D 对象不会自动参与物理仿真。它必须经过"物理化"才能响应重力、碰撞、推力
- `Physicalize_Balls` 等 DataArray 是物理参数的"配方表"
- `OnPhysicalize` 回调让你观察到物理化发生的瞬间和具体参数
- 一个关卡加载后，多个对象会依次被物理化
- 观察回调只是被动接收信息，不改变任何游戏状态，适合用来理解系统运作方式
- 日志比 UI 更适合记录这种密集的事件流

## 完成状态

日志中能同时看到物理参数表的内容和物理化事件流。能够理解"不是所有 3D 对象都参与物理"这个核心概念。

下一步：[17 生成物理对象](tutorial-17-physics-create.md)

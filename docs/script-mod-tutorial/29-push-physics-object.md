# 第 29 章：推动物理对象

第 28 章已经能生成一个物理化木球。这一章不换对象，继续操作那个木球。

本章做成这样：

```text
按 J 生成木球
按 U 唤醒最近一个木球
按 I 给它一次冲量
按 F 给它一个短时间持续力
按 C 手动清掉持续力
按 K 清理木球
```

这里的重点还是物理 API，不是按键。按键只是让你不用反复输入命令。

## 先分清四个动作

| API | 先按什么理解 | 画面上可能看到什么 |
| --- | --- | --- |
| `WakeUp` | 让休眠的物理对象重新参与仿真 | 可能没明显变化 |
| `Impulse` | 一次性推一下 | 木球突然动一下 |
| `SetForce` | 持续施加一个力 | 木球持续被推一小段时间 |
| `ClearForce` | 停止持续力 | 持续推动停止 |

这四个 API 都要求目标已经物理化。只加载出 `CK3dEntity` 不够。

第 28 章里 `physical=true` 的木球，才是本章的目标。

## 在第 28 章脚本上新增状态

打开：

```text
ModLoader/Mods/WoodObjectMod.mod.as
```

在成员变量里增加：

```angelscript
private CK3dEntity@ forceTarget;
private bool forceActive = false;
private int forceFramesLeft = 0;
```

这三个变量只为 `SetForce` 服务：

| 变量 | 用途 |
| --- | --- |
| `forceTarget` | 记住当前被持续力推动的对象 |
| `forceActive` | 记录持续力是否还没清 |
| `forceFramesLeft` | 自动清理前还剩多少帧 |

`Impulse` 是一次性的，不需要保存状态。`SetForce` 会留下持续力，所以必须保存状态并安排清理。

## 修改 OnProcess

第 28 章的 `OnProcess` 只处理输入。现在多加一个自动清理：

```angelscript
void OnProcess(const BML::ModContext &in ctx) {
    HandleInput(ctx);
    TickForce(ctx);
}
```

`TickForce` 每帧递减计数，到 0 时调用 `ClearForce`：

```angelscript
private void TickForce(const BML::ModContext &in ctx) {
    if (!forceActive) {
        return;
    }

    forceFramesLeft--;
    if (forceFramesLeft <= 0) {
        ClearWoodForce(ctx, "timer");
    }
}
```

## 修改输入处理

把第 28 章的 `HandleInput` 改成：

```angelscript
private void HandleInput(const BML::ModContext &in ctx) {
    BML::InputHook@ input = ctx.BorrowInputManager();
    if (input is null) {
        return;
    }

    if (input.IsKeyPressed(CKKEY_J)) {
        CreatePhysicalWoodBall(ctx);
    }

    if (input.IsKeyPressed(CKKEY_U)) {
        WakeLastWoodBall(ctx);
    }

    if (input.IsKeyPressed(CKKEY_I)) {
        ImpulseLastWoodBall(ctx);
    }

    if (input.IsKeyPressed(CKKEY_F)) {
        SetForceOnLastWoodBall(ctx);
    }

    if (input.IsKeyPressed(CKKEY_C)) {
        ClearWoodForce(ctx, "key");
    }

    if (input.IsKeyPressed(CKKEY_K)) {
        CleanupWoodBalls(ctx, "key");
        ctx.SendIngameMessage("Wood balls cleaned.");
    }
}
```

操作顺序建议先这样：

```text
J -> I
J -> F
J -> U
J -> F -> C
```

不要一开始就连按很多次。先看每个动作单独会发生什么。

## 找到最近生成的木球

所有操作都指向最近生成的木球。新增这个函数：

```angelscript
private CK3dEntity@ BorrowLastWoodBall() {
    for (int i = int(woodBalls.length()) - 1; i >= 0; i--) {
        CK3dEntity@ entity = woodBalls[i];
        if (entity !is null && BML::CK::IsValid(entity)) {
            return entity;
        }
    }
    return null;
}
```

从数组末尾往前找，是因为最近生成的木球在最后。

如果返回 `null`，说明还没有可用木球。先按 `J`。

## WakeUp

新增：

```angelscript
private void WakeLastWoodBall(const BML::ModContext &in ctx) {
    CK3dEntity@ entity = BorrowLastWoodBall();
    if (entity is null) {
        ctx.SendIngameMessage("No wood ball. Press J first.");
        return;
    }

    bool ok = BML::Physics::WakeUp(entity);
    LogInfo(ctx, "wood wakeup name=" + BML::CK::GetName(entity) +
                 " ok=" + BoolText(ok));
}
```

`WakeUp` 成功时日志会有 `ok=true`。画面上可能没变化，因为木球可能本来就在运动。

## Impulse

新增：

```angelscript
private void ImpulseLastWoodBall(const BML::ModContext &in ctx) {
    CK3dEntity@ entity = BorrowLastWoodBall();
    if (entity is null) {
        ctx.SendIngameMessage("No wood ball. Press J first.");
        return;
    }

    bool ok = BML::Physics::Impulse(
        entity,
        VxVector(0.0f, 0.0f, 0.0f), null,
        VxVector(1.0f, 0.0f, 0.0f), null,
        1.5f);

    LogInfo(ctx, "wood impulse name=" + BML::CK::GetName(entity) +
                 " ok=" + BoolText(ok));
    ctx.SendIngameMessage("Impulse sent to wood ball.");
}
```

参数按顺序读：

| 参数 | 本章写法 | 含义 |
| --- | --- | --- |
| `entity` | 最近生成的木球 | 受力对象 |
| `position` | `(0, 0, 0)` | 作用位置 |
| `positionReference` | `null` | 使用默认参考 |
| `direction` | `(1, 0, 0)` | 往 X 方向推 |
| `directionReference` | `null` | 使用默认参考 |
| `impulse` | `1.5` | 冲量大小 |

按 `I` 后，木球应该明显动一下。不同关卡位置、碰撞状态不同，方向和距离可能不一样。

## SetForce 和 ClearForce

`SetForce` 会持续作用一段时间，所以它要配套 `ClearForce`。

新增：

```angelscript
private void SetForceOnLastWoodBall(const BML::ModContext &in ctx) {
    CK3dEntity@ entity = BorrowLastWoodBall();
    if (entity is null) {
        ctx.SendIngameMessage("No wood ball. Press J first.");
        return;
    }

    ClearWoodForce(ctx, "replace");

    bool ok = BML::Physics::SetForce(
        entity,
        VxVector(0.0f, 0.0f, 0.0f), null,
        VxVector(1.0f, 0.0f, 0.0f), null,
        0.6f);

    if (ok) {
        @forceTarget = entity;
        forceActive = true;
        forceFramesLeft = 90;
    }

    LogInfo(ctx, "wood setforce name=" + BML::CK::GetName(entity) +
                 " ok=" + BoolText(ok));
    ctx.SendIngameMessage("Force set on wood ball.");
}
```

`forceFramesLeft = 90` 表示大约 90 帧后自动清理。帧率不同时实际秒数会不同，但入门阶段先按“短时间”理解。

再新增清理函数：

```angelscript
private void ClearWoodForce(const BML::ModContext &in ctx,
                            const string &in reason) {
    if (!forceActive || forceTarget is null) {
        return;
    }

    if (BML::CK::IsValid(forceTarget)) {
        bool ok = BML::Physics::ClearForce(forceTarget);
        LogInfo(ctx, "wood clearforce name=" + BML::CK::GetName(forceTarget) +
                     " reason=" + reason +
                     " ok=" + BoolText(ok));
    }

    @forceTarget = null;
    forceActive = false;
    forceFramesLeft = 0;
}
```

按 `F` 后，木球应该被持续推一小段时间。按 `C` 可以提前停止。

## 清理时也要清力

把第 28 章的 `CleanupWoodBalls` 开头改成：

```angelscript
private void CleanupWoodBalls(const BML::ModContext &in ctx,
                              const string &in reason) {
    ClearWoodForce(ctx, reason);

    uint cleaned = 0;
    for (uint i = 0; i < woodBalls.length(); i++) {
        CK3dEntity@ entity = woodBalls[i];
        if (entity is null || !BML::CK::IsValid(entity)) {
            continue;
        }

        BML::Physics::Unphysicalize(entity);
        BML::CK::Show(entity, CKHIDE, true);
        cleaned++;
    }

    woodBalls.resize(0);
    LogInfo(ctx, "wood cleanup reason=" + reason + " count=" + cleaned);
}
```

这一步很重要。只要用过 `SetForce`，清理对象前就先 `ClearForce`。

## 运行后看什么

进入关卡后，按下面顺序试：

```text
J
I
```

画面上应该看到木球被推一下。日志里应该有：

```text
wood impulse name=Tutorial_WoodBall_1_BMLLoad_1 ok=true
```

再试：

```text
J
F
```

木球会被持续推一小段时间。自动清理时日志里应该有：

```text
wood clearforce name=Tutorial_WoodBall_2_BMLLoad_1 reason=timer ok=true
```

最后按：

```text
K
```

确认木球被隐藏，日志里有：

```text
wood cleanup reason=key count=...
```

## 失败时先查哪里

| 现象 | 先查 |
| --- | --- |
| 按 `I` 没反应 | 是否先按了 `J`，日志里有没有 `physical=true` |
| `WakeUp` 返回 `false` | 目标可能已经无效，先按 `K` 清理再按 `J` |
| `Impulse` 返回 `true` 但看不出动 | 木球可能卡在物体里，换个空旷位置再试 |
| `SetForce` 后一直被推 | 是否调用了 `ClearForce`，清理函数开头是否加了 `ClearWoodForce` |
| 关卡退出后还有异常 | `OnGameEvent` 和 `OnUnload` 是否都调用清理 |

不要先改当前活动球。当前球受 Gameplay、输入、相机、复活和变球流程控制。刚开始直接推它，很难判断现象来自脚本还是原版流程。

## 本章先记住

`WakeUp` 只唤醒，通常不产生明显画面变化。

`Impulse` 是一次性推动，适合确认“按一下动一下”。

`SetForce` 是持续推动，必须设计 `ClearForce`。

物理操作先落在脚本创建的物理化对象上。等创建、推动、清理都稳定，再考虑更复杂的玩法修改。

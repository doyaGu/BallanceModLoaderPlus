# 18 控制物理对象

## 动机

上一章你生成了一个物理球，它落下来、弹起、停住。但这只是被动的，球在重力和碰撞作用下自然运动。

现在我们要主动控制它。想象这样的场景：按一个键，球被"打"向右边；按住另一个键，有一股风持续吹着球跑。这就是 Impulse（冲量）和 Force（力）的区别。

物理学里：
- **冲量**像用锤子敲一下，瞬间给一个速度变化，之后不再管了
- **力**像风扇一直吹，每一帧都在施加影响，停止施加后效果才消失

本章用键盘操作这两种物理控制，让你直观感受它们的区别。

## 四个物理操作 API

| API | 物理类比 | 效果 | 是否需要配套清理 |
| --- | --- | --- | --- |
| `BML::Physics::WakeUp(target)` | 推一下正在睡觉的人 | 让休眠对象重新参与仿真 | 不需要 |
| `BML::Physics::Impulse(...)` | 用锤子敲一下 | 瞬间改变速度 | 不需要 |
| `BML::Physics::SetForce(...)` | 开风扇 | 每帧持续施加力 | 必须 `ClearForce` |
| `BML::Physics::ClearForce(target)` | 关风扇 | 停止持续力 | 不需要 |

### WakeUp：唤醒休眠

物理引擎有一个优化策略：如果一个对象已经静止了一段时间（速度为零且没有力作用），引擎会把它标记为"休眠"，不再计算它的物理。这节省了大量 CPU 时间。

但是，如果你想推一个已经休眠的球，必须先唤醒它。否则 Impulse 和 SetForce 可能没有效果（引擎认为它在睡觉，不参与本帧计算）。

```angelscript
bool ok = BML::Physics::WakeUp(spawnedBall);
```

返回 `true` 表示成功唤醒。`false` 表示对象可能未物理化或已无效。

唤醒后如果没有外力，球不会动，它只是从"不计算"变成"要计算"。但因为没有力，计算结果就是保持静止。

### Impulse：瞬间推力

```angelscript
bool ok = BML::Physics::Impulse(
    spawnedBall,                        // 目标对象
    VxVector(0.0f, 0.0f, 0.0f), null,  // 作用位置, 位置参考系
    VxVector(3.0f, 0.0f, 0.0f), null,  // 方向, 方向参考系
    1.5f);                              // 冲量大小
```

六个参数的含义：

| 参数 | 含义 | 说明 |
| --- | --- | --- |
| `target` | 推哪个对象 | 必须已物理化 |
| `position` | 力作用在对象的哪个点 | `(0,0,0)` 表示对象中心 |
| `positionReference` | 位置是相对于谁的坐标系 | `null` 表示世界坐标系 |
| `direction` | 推力方向 | `(3,0,0)` 表示正 X 方向 |
| `directionReference` | 方向是相对于谁的坐标系 | `null` 表示世界坐标系 |
| `impulse` | 冲量大小 | 越大推力越猛 |

**关于 null 参考系**：当 positionReference 或 directionReference 为 `null` 时，坐标按世界坐标系解释。世界坐标系是固定的，X 轴始终指向同一方向，不管对象怎么旋转。如果传入一个对象作为参考系，坐标就是相对于那个对象的局部坐标。

**作用点的影响**：如果作用点不在对象重心，推力会同时产生旋转。比如推球的顶部，球会向下滚动同时平移。推重心则只平移不转。本教程用 `(0,0,0)` 推重心。

冲量的直觉：`1.5f` 配合方向向量 `(3,0,0)`，最终的推力效果是 `1.5 * 3 = 4.5` 的 X 方向冲量。球会瞬间获得一个向右的速度，然后在摩擦和重力作用下逐渐停下来。

### SetForce：持续力

```angelscript
bool ok = BML::Physics::SetForce(
    spawnedBall,
    VxVector(0.0f, 0.0f, 0.0f), null,   // 作用位置, 位置参考系
    VxVector(1.0f, 0.0f, 0.0f), null,    // 方向, 方向参考系
    0.6f);                                // 力大小
```

参数结构和 Impulse 完全一样（6 个参数），区别在于语义：

- Impulse 执行一次就完事，不需要后续操作
- SetForce 设置后**每帧都生效**，直到你调用 ClearForce

这意味着如果你 SetForce 后忘记 ClearForce，球会一直被推，越来越快，最终飞出场景。

### ClearForce：停止持续力

```angelscript
BML::Physics::ClearForce(spawnedBall);
```

调用后，之前 SetForce 施加的力立刻消失。球不会瞬间停下，它保留已有的速度（惯性），但不再加速。之后会在摩擦和阻尼作用下逐渐减速停住。

## TickForce 模式：帧计数自动清理

持续力通常有时限。实现方式是用帧计数器：SetForce 成功后设 `forceActive = true` 和 `forceFramesLeft = 90`（约 1.5 秒），在 `OnProcess` 里每帧递减，到 0 时调用 ClearForce。完整逻辑见下方脚本中的 `TickForce` 方法。

这个模式的优点：不会忘记清理，可以精确控制持续时间。

## 键位设计

J=生成球，K=清理，U=唤醒，I=向右推(Impulse)，H=向左推(Impulse)，F=持续力(90帧)，C=清除力

## 完整脚本

新建 `ModLoader/Mods/ControlBall.mod.as`：

```angelscript
[bml.mod id="control.ball" name="Control Ball" version="1.0.0" author="Tutorial" bml="0.3.12" description="Control a physicalized ball with keyboard"]
class ControlBall {
    private CK3dEntity@ spawnedBall = null;
    private bool forceActive = false;
    private int forceFramesLeft = 0;

    void OnLoad(const BML::ModContext &in ctx) {
        LogInfo(ctx, "ControlBall loaded. J=spawn K=cleanup U=wake I/H=impulse F=force C=clear");
    }

    void OnUnload(const BML::ModContext &in ctx) {
        CleanupBall(ctx);
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL ||
            event == BML::GAME_EVENT_PRE_LOAD_LEVEL ||
            event == BML::GAME_EVENT_EXIT_GAME) {
            CleanupBall(ctx);
        }
    }

    void OnProcess(const BML::ModContext &in ctx) {
        HandleInput(ctx);
        TickForce(ctx);
    }

    private void HandleInput(const BML::ModContext &in ctx) {
        BML::InputHook@ input = ctx.BorrowInputManager();
        if (input is null) return;

        if (input.IsKeyPressed(CKKEY_J)) {
            SpawnBall(ctx);
        }
        if (input.IsKeyPressed(CKKEY_K)) {
            CleanupBall(ctx);
            ctx.SendIngameMessage("Ball cleaned.");
        }
        if (input.IsKeyPressed(CKKEY_U)) {
            WakeBall(ctx);
        }
        if (input.IsKeyPressed(CKKEY_I)) {
            ImpulseBall(ctx, VxVector(3.0f, 0, 0));
        }
        if (input.IsKeyPressed(CKKEY_H)) {
            ImpulseBall(ctx, VxVector(-3.0f, 0, 0));
        }
        if (input.IsKeyPressed(CKKEY_F)) {
            SetBallForce(ctx);
        }
        if (input.IsKeyPressed(CKKEY_C)) {
            ClearBallForce(ctx, "key");
        }
    }

    private void TickForce(const BML::ModContext &in ctx) {
        if (!forceActive) return;

        forceFramesLeft--;
        if (forceFramesLeft <= 0) {
            ClearBallForce(ctx, "timer");
        }
    }

    // ---- 物理操作 ----

    private void WakeBall(const BML::ModContext &in ctx) {
        if (spawnedBall is null) {
            ctx.SendIngameMessage("No ball. Press J first.");
            return;
        }

        bool ok = BML::Physics::WakeUp(spawnedBall);
        LogInfo(ctx, "WakeUp ok=" + BoolText(ok));
    }

    private void ImpulseBall(const BML::ModContext &in ctx, VxVector direction) {
        if (spawnedBall is null) {
            ctx.SendIngameMessage("No ball. Press J first.");
            return;
        }

        BML::Physics::WakeUp(spawnedBall);
        bool ok = BML::Physics::Impulse(
            spawnedBall,
            VxVector(0.0f, 0.0f, 0.0f), null,
            direction, null,
            1.5f);

        LogInfo(ctx, "Impulse direction=(" + direction.x + "," + direction.y + "," + direction.z + ") ok=" + BoolText(ok));
        ctx.SendIngameMessage("Impulse sent.");
    }

    private void SetBallForce(const BML::ModContext &in ctx) {
        if (spawnedBall is null) {
            ctx.SendIngameMessage("No ball. Press J first.");
            return;
        }

        ClearBallForce(ctx, "replace");

        bool ok = BML::Physics::SetForce(
            spawnedBall,
            VxVector(0.0f, 0.0f, 0.0f), null,
            VxVector(1.0f, 0.0f, 0.0f), null,
            0.6f);

        if (ok) {
            forceActive = true;
            forceFramesLeft = 90;
        }

        LogInfo(ctx, "SetForce ok=" + BoolText(ok) + " frames=90");
        ctx.SendIngameMessage("Force applied for ~90 frames.");
    }

    private void ClearBallForce(const BML::ModContext &in ctx, const string &in reason) {
        if (!forceActive) return;

        if (spawnedBall !is null && BML::CK::IsValid(spawnedBall)) {
            BML::Physics::ClearForce(spawnedBall);
        }

        forceActive = false;
        forceFramesLeft = 0;
        LogInfo(ctx, "ClearForce reason=" + reason);
    }

    // ---- 生成和清理（与上一章相同，区别是 CleanupBall 先调 ClearBallForce）----

    private void SpawnBall(const BML::ModContext &in ctx) {
        if (spawnedBall !is null) {
            CleanupBall(ctx);
        }

        string relativePath = "3D Entities\\PH\\P_Ball_Wood.nmo";
        string resourcePath = BML::Path::Combine(ctx.GetDirectoryUtf8(BML::DIR_GAME), relativePath);
        if (!BML::Path::IsFile(resourcePath)) { LogWarn(ctx, "Resource missing"); return; }

        BML::ObjectLoadOptions options;
        options.File = resourcePath;  options.Rename = true;
        options.MasterName = "Tutorial_ControlBall";  options.AddToScene = true;
        options.ReuseMeshes = true;  options.ReuseMaterials = true;  options.Dynamic = true;

        BML::ObjectLoadResult@ result = BML::CK::LoadObject(options);
        if (result is null || !result.Success || result.Count <= 0) { LogWarn(ctx, "Load failed"); return; }
        @spawnedBall = FindFirstEntity(result);
        if (spawnedBall is null) return;

        CK3dEntity@ playerBall = BorrowActiveBall(ctx);
        if (playerBall !is null && BML::CK::IsValid(playerBall)) {
            VxVector pos = BML::CK::GetPosition(playerBall);
            pos.y += 5.0f;  BML::CK::SetPosition(spawnedBall, pos);
        } else { BML::CK::SetPosition(spawnedBall, VxVector(0.0f, 10.0f, 0.0f)); }

        BML::PhysicalizeDefinition physics;
        physics.Fixed = false;  physics.Friction = 0.6f;  physics.Elasticity = 0.2f;
        physics.Mass = 2.0f;  physics.EnableCollision = true;
        physics.LinearDamp = 0.6f;  physics.RotDamp = 0.1f;
        physics.CollisionSurface = "P_Ball_Wood_Mesh";
        BML::Physics::PhysicalizeBall(spawnedBall, physics, VxVector(0.0f, 0.0f, 0.0f), 2.0f);
        BML::CK::Show(spawnedBall, CKSHOW, true);
        ctx.SendIngameMessage("Ball spawned.");
    }

    private void CleanupBall(const BML::ModContext &in ctx) {
        ClearBallForce(ctx, "cleanup");
        if (spawnedBall is null) return;
        if (BML::CK::IsValid(spawnedBall)) {
            BML::Physics::Unphysicalize(spawnedBall);
            BML::CK::Show(spawnedBall, CKHIDE, true);
        }
        @spawnedBall = null;
    }

    private CK3dEntity@ BorrowActiveBall(const BML::ModContext &in ctx) {
        CKDataArray@ currentLevel = ctx.BorrowDataArrayByName("CurrentLevel");
        if (currentLevel is null) return null;

        int col = BML::CK::FindColumn(currentLevel, "ActiveBall");
        if (col < 0) return null;

        CKObject@ object = currentLevel.GetElementObject(0, col);
        return cast<CK3dEntity>(object);
    }

    private CK3dEntity@ FindFirstEntity(BML::ObjectLoadResult@ result) {
        for (int i = 0; i < result.Count; i++) {
            CKObject@ obj = result.BorrowObject(i);
            CK3dEntity@ e = cast<CK3dEntity>(obj);
            if (e !is null) return e;
        }
        return null;
    }

    private string BoolText(bool v) { return v ? "true" : "false"; }
    private void LogInfo(const BML::ModContext &in ctx, const string &in msg) {
        BML::Logger@ l = ctx.BorrowLogger(); if (l !is null) l.Info(msg);
    }
    private void LogWarn(const BML::ModContext &in ctx, const string &in msg) {
        BML::Logger@ l = ctx.BorrowLogger(); if (l !is null) l.Warn(msg);
    }
}
```

## 测试步骤

不要一上来就乱按。逐个验证：

1. **生成**：按 J。球出现在上方，落下来，静止
2. **唤醒**：等球静止后按 U。日志 `WakeUp ok=true`，球不动（唤醒不产生运动）
3. **Impulse**：按 I。球瞬间向右弹出，像弹珠台弹簧弹走，然后靠摩擦减速停住。按 H 向左
4. **SetForce**：按 F。球逐渐加速向右（不是瞬间弹走），约 1.5 秒后日志 `ClearForce reason=timer`，球不再加速但靠惯性继续滑行
5. **手动清除**：按 F 后立刻按 C。球还没加速多少就停止。日志 `ClearForce reason=key`
6. **清理**：按 K。球消失

## Impulse 和 SetForce 对比

| | Impulse | SetForce |
| --- | --- | --- |
| 类比 | 锤子敲 | 风扇吹 |
| 作用时间 | 一帧 | 每帧，直到清除 |
| 速度变化 | 立刻跳变 | 逐帧累加 |
| 清理 | 不需要 | 必须 ClearForce |
| 适用场景 | 弹射、爆炸、跳跃 | 推进器、风场、传送带 |

"爆炸推开附近物体"用 Impulse。"风区让球偏移"用 SetForce。

## 防御性设计要点

脚本中有三个防御性写法值得注意：

- **Impulse 前先 WakeUp**：休眠对象可能忽略冲量，先唤醒确保物理引擎处理它
- **SetForce 前先 ClearForce**：避免新旧力叠加导致意外行为
- **CleanupBall 前先 ClearForce**：保持 `forceActive` 状态标志和实际物理状态一致

## 失败诊断

| 现象 | 可能原因 | 解决方法 |
| --- | --- | --- |
| Impulse 返回 true 但球不动 | 球可能卡在地形里面 | 按 K 清理后在空旷平面上按 J 重新生成 |
| Impulse 返回 true 但方向不对 | X/Y/Z 轴和你预想的不同 | Ballance 的 Y 轴是上方向，X 和 Z 是水平面。尝试改方向向量 |
| WakeUp 返回 false | 对象未物理化或已无效 | 先按 K 再按 J 重新生成 |
| SetForce 后球一直加速不停 | TickForce 没在 OnProcess 里调用 | 确认 `OnProcess` 里有 `TickForce(ctx)` |
| 按 C 但球还在动 | ClearForce 只停止加力，不停止已有速度 | 这是正确行为，球在惯性作用下继续滑行直到摩擦让它停下 |
| 关卡切换后崩溃 | 清理逻辑有遗漏 | 确认 `OnGameEvent` 处理了三种退出事件 |
| 按 F 后日志显示 ok=false | 对象可能已在物理化过程中出问题 | 检查生成时 `physical=true` |

## 本章要点

- 物理控制只能作用于已物理化的对象，对未物理化的对象调用这些 API 无效
- WakeUp 让休眠对象重新参与仿真，是其他操作的前提
- Impulse 是一次性的瞬间推力，适合"弹射"类效果
- SetForce 是持续力，必须配套 ClearForce，否则球永远加速
- TickForce 模式（帧计数器 + 自动清除）是管理持续力生命周期的常用方法
- 清理对象前先清力，先取消物理化再隐藏再释放句柄
- 先在自己创建的对象上练习，确认理解后再尝试影响游戏中的活动球

## 完成状态

按键能控制自己创建的球。能观察到 Impulse 的瞬间推动效果和 SetForce 的持续推动效果。能区分两者的行为差异。理解了帧计数自动清除的模式。

下一步：[19 修改前检查清单](tutorial-19-safety-checklist.md)

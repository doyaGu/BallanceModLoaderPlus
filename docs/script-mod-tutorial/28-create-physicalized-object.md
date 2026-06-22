# 第 28 章：生成物理化对象

第 27 章只看原版对象什么时候接入物理系统。现在换成脚本自己做一次。

这一章做成这样：

```text
进入关卡
按 J
玩家球上方出现一个木球
日志里出现 physical=true
按 K
脚本把自己生成的木球清理掉
```

这里的重点不是按键。按键只是触发方式。真正要分清的是这四步：

```text
加载资源
把对象放到看得见的位置
显示对象
把对象接入物理系统
```

`LoadObject` 只负责加载。`SetPosition` 和 `Show` 只负责摆放和显示。`PhysicalizeBall` 才负责让对象进入物理系统。

## 先看核心流程

去掉日志、按键和清理以后，核心流程只有几行：

```angelscript
CK3dEntity@ entity = LoadWoodBall(ctx);
BML::CK::SetPosition(entity, dropPosition);
BML::CK::Show(entity, CKSHOW, true);
bool physical = PhysicalizeWoodBall(entity);
woodBalls.insertLast(entity);
```

这几行对应游戏里的动作：

| 代码 | 画面或状态 |
| --- | --- |
| `LoadWoodBall` | 资源变成运行时对象 |
| `SetPosition` | 对象被放到玩家球上方 |
| `Show` | 画面里能看到它 |
| `PhysicalizeWoodBall` | 它进入物理系统 |
| `insertLast` | 脚本记住它，后面可以清理 |

完整脚本只是把这几步包起来，补上失败检查和清理。

## 新建脚本

新建：

```text
ModLoader/Mods/WoodObjectMod.mod.as
```

写入：

```angelscript
[bml.mod id="wood.object" name="Wood Object Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="Create and clean physicalized wood balls"]
class WoodObjectMod {
    private array<CK3dEntity@> woodBalls;
    private int nextWoodIndex = 1;

    void OnLoad(const BML::ModContext &in ctx) {
        LogInfo(ctx, "WoodObjectMod loaded");
        ctx.SendIngameMessage("WoodObjectMod loaded. Enter a level: J create, K cleanup.");
    }

    void OnUnload(const BML::ModContext &in ctx) {
        CleanupWoodBalls(ctx, "unload");
        LogInfo(ctx, "WoodObjectMod unloaded");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_PRE_LOAD_LEVEL ||
            event == BML::GAME_EVENT_PRE_EXIT_LEVEL ||
            event == BML::GAME_EVENT_EXIT_GAME) {
            CleanupWoodBalls(ctx, "game-event");
        }
    }

    void OnProcess(const BML::ModContext &in ctx) {
        HandleInput(ctx);
    }

    private void HandleInput(const BML::ModContext &in ctx) {
        BML::InputHook@ input = ctx.BorrowInputManager();
        if (input is null) {
            return;
        }

        if (input.IsKeyPressed(CKKEY_J)) {
            CreatePhysicalWoodBall(ctx);
        }

        if (input.IsKeyPressed(CKKEY_K)) {
            CleanupWoodBalls(ctx, "key");
            ctx.SendIngameMessage("Wood balls cleaned.");
        }
    }

    private void CreatePhysicalWoodBall(const BML::ModContext &in ctx) {
        VxVector dropPosition;
        string anchorName;
        if (!FindDropPosition(ctx, dropPosition, anchorName)) {
            LogWarn(ctx, "cannot find Ball or Cam_Target");
            ctx.SendIngameMessage("Wood ball failed: no visible anchor.");
            return;
        }

        CK3dEntity@ entity = LoadWoodBall(ctx);
        if (entity is null) {
            ctx.SendIngameMessage("Wood ball failed: load error.");
            return;
        }

        // 先摆放和显示，再物理化。这样失败时更容易判断卡在哪一步。
        BML::CK::SetPosition(entity, dropPosition);
        BML::CK::Show(entity, CKSHOW, true);

        bool physical = PhysicalizeWoodBall(entity);
        woodBalls.insertLast(entity);

        string name = BML::CK::GetName(entity);
        LogInfo(ctx,
                "wood created name=" + name +
                " anchor=" + anchorName +
                " physical=" + BoolText(physical) +
                " count=" + woodBalls.length());

        ctx.SendIngameMessage("Wood ball created above " + anchorName + ".");
    }

    private bool FindDropPosition(const BML::ModContext &in ctx,
                                  VxVector &out position,
                                  string &out anchorName) {
        CK3dEntity@ ball = ctx.Borrow3dEntityByName("Ball");
        if (ball !is null && BML::CK::IsValid(ball)) {
            VxVector ballPosition = BML::CK::GetPosition(ball);
            position = VxVector(ballPosition.x, ballPosition.y + 5.0f, ballPosition.z);
            anchorName = "Ball";
            return true;
        }

        CK3dEntity@ camTarget = ctx.Borrow3dEntityByName("Cam_Target");
        if (camTarget !is null && BML::CK::IsValid(camTarget)) {
            VxVector targetPosition = BML::CK::GetPosition(camTarget);
            position = VxVector(targetPosition.x, targetPosition.y + 5.0f, targetPosition.z);
            anchorName = "Cam_Target";
            return true;
        }

        return false;
    }

    private CK3dEntity@ LoadWoodBall(const BML::ModContext &in ctx) {
        string relativePath = "3D Entities\\PH\\P_Ball_Wood.nmo";
        string resourcePath = BML::Path::Combine(ctx.GetDirectoryUtf8(BML::DIR_GAME), relativePath);
        if (!BML::Path::IsFile(resourcePath)) {
            LogWarn(ctx, "resource missing: " + relativePath);
            return null;
        }

        BML::ObjectLoadOptions options;
        options.File = resourcePath;
        options.Rename = true;
        options.MasterName = "Tutorial_WoodBall_" + nextWoodIndex;
        options.AddToScene = true;
        options.ReuseMeshes = true;
        options.ReuseMaterials = true;
        options.Dynamic = true;
        nextWoodIndex++;

        BML::ObjectLoadResult@ result = BML::CK::LoadObject(options);
        if (result is null || !result.Success || result.Count <= 0) {
            LogWarn(ctx, "wood ball load failed");
            return null;
        }

        return FindFirstEntity(result);
    }

    private CK3dEntity@ FindFirstEntity(BML::ObjectLoadResult@ result) {
        for (int i = 0; i < result.Count; i++) {
            CKObject@ object = result.BorrowObject(i);
            CK3dEntity@ entity = cast<CK3dEntity>(object);
            if (entity !is null) {
                return entity;
            }
        }
        return null;
    }

    private bool PhysicalizeWoodBall(CK3dEntity@ entity) {
        BML::PhysicalizeDefinition physics;
        physics.Fixed = false;
        physics.Friction = 0.6f;
        physics.Elasticity = 0.2f;
        physics.Mass = 2.0f;
        physics.CollisionGroup = "";
        physics.EnableCollision = true;
        physics.LinearDamp = 0.6f;
        physics.RotDamp = 0.1f;
        physics.CollisionSurface = "P_Ball_Wood_Mesh";

        return BML::Physics::PhysicalizeBall(entity,
                                             physics,
                                             VxVector(0.0f, 0.0f, 0.0f),
                                             2.0f);
    }

    private void CleanupWoodBalls(const BML::ModContext &in ctx,
                                  const string &in reason) {
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

    private string BoolText(bool value) {
        return value ? "true" : "false";
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
}
```

## 代码怎么读

先看 `CreatePhysicalWoodBall`。它只做一条直线：

```text
找位置
加载木球
移动到那个位置
显示
物理化
保存句柄
```

找位置时先借当前球：

```angelscript
CK3dEntity@ ball = ctx.Borrow3dEntityByName("Ball");
```

找到后往上抬 5 个单位：

```angelscript
position = VxVector(ballPosition.x, ballPosition.y + 5.0f, ballPosition.z);
```

这样生成出来的木球应该在当前球上方。固定坐标不适合入门观察，因为你可能根本看不到对象掉在哪里。

加载资源时使用相对路径：

```angelscript
string relativePath = "3D Entities\\PH\\P_Ball_Wood.nmo";
```

教程代码不要写本机绝对路径。脚本运行时用游戏目录和相对路径拼出完整路径。

物理化参数沿用第 22 章读到的木球参数：

```angelscript
physics.Friction = 0.6f;
physics.Elasticity = 0.2f;
physics.Mass = 2.0f;
physics.CollisionSurface = "P_Ball_Wood_Mesh";
```

这一步成功以后，第 27 章的 `OnPhysicalize` 也会收到一条事件。

## 运行后看什么

进入关卡后按 `J`。

先看画面：玩家球上方应该出现一个木球。它可能下落、滚动，也可能很快离开视野。先确认“出现过”。

再看日志：

```text
wood created name=Tutorial_WoodBall_1_BMLLoad_1 anchor=Ball physical=true count=1
```

这里最重要的是：

```text
physical=true
```

它说明 `PhysicalizeBall` 调用成功。第 27 章脚本如果也在运行，还能看到对应的 `OnPhysicalize` 记录。

按 `K` 后，日志应出现：

```text
wood cleanup reason=key count=1
```

对象会取消物理化并隐藏。脚本不再保存这些句柄。

## 失败时先查哪里

看不到木球时，按这个顺序查：

| 现象 | 先查 |
| --- | --- |
| 没有加载日志 | 文件名是不是 `.mod.as`，脚本是否编译失败 |
| 提示找不到锚点 | 是否已经进入关卡，`Ball` / `Cam_Target` 是否存在 |
| 提示资源缺失 | `3D Entities\PH\P_Ball_Wood.nmo` 是否存在 |
| 日志有 `physical=false` | 先查物理化参数，不要急着改 Impulse |
| 生成后看不见 | 先看日志里的 `anchor=...`，再试着让当前球停在开阔位置 |

## 本章边界

本章生成的是脚本创建的物理化对象。它还没有进入原版 Gameplay 的完整管理流程。

DebugUtilities 那种完整召唤流程还会处理：

```text
PH 表
DepthTest 组
关卡重置时的对象管理
```

这章先不做这些。先把“加载 -> 显示 -> 物理化 -> 清理”跑稳。下一章继续用这个木球做 `WakeUp`、`Impulse`、`SetForce`。

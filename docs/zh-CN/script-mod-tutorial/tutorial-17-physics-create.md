# 17 生成物理对象

## 动机

上一章你观察了物理系统：对象必须被物理化才能参与仿真。行为图从 DataArray 读取参数，然后执行物理化。

现在反过来，如果脚本想凭空生成一个球，让它从天上掉下来，需要做什么？

答案是四个步骤，缺一不可，顺序不能乱。

## 四步流水线

| 步骤 | 做什么 | 不做会怎样 |
| --- | --- | --- |
| 1. 加载资源 | 把 `.nmo` 文件变成运行时对象 | 没有对象可操作 |
| 2. 设置位置 | 把对象放到 3D 空间中的指定坐标 | 对象在原点(0,0,0)，可能卡在地下 |
| 3. 物理化 | 告诉物理引擎开始仿真这个对象 | 对象不受重力，不碰撞，不会动 |
| 4. 显示 | 让渲染管线把它画出来 | 物理正常运行但画面上看不见 |

很多初学者会困惑"为什么加载了就不动"或"为什么物理化了看不见"，原因就是步骤没做完。

## 步骤 1：加载资源（ObjectLoadOptions）

Ballance 的 3D 对象存储在 `.nmo` 文件中。要把文件变成运行时对象，使用 `BML::CK::LoadObject`：

```angelscript
BML::ObjectLoadOptions options;
options.File = resourcePath;        // 资源文件完整路径
options.Rename = true;              // 自动重命名避免冲突
options.MasterName = "Tutorial_SpawnBall";  // 重命名基础名
options.AddToScene = true;          // 加入当前场景
options.ReuseMeshes = true;         // 复用已有同名网格（节省内存）
options.ReuseMaterials = true;      // 复用已有同名材质（节省显存）
options.Dynamic = true;             // 标记为动态对象（可移动）

BML::ObjectLoadResult@ result = BML::CK::LoadObject(options);
```

`Rename = true` 配合 `MasterName`，加载后对象名变成类似 `Tutorial_SpawnBall_BMLLoad_1`，避免和场景中已有对象重名。`Dynamic = true` 告诉引擎这个对象会移动，不要做静态优化。

`LoadObject` 返回 `BML::ObjectLoadResult@`，检查三个条件：

```angelscript
if (result is null || !result.Success || result.Count <= 0) {
    // 加载失败
}
```

一个 `.nmo` 文件可能包含多个对象（3D 实体和它的 Mesh）。我们要找类型为 `CK3dEntity` 的那个：

```angelscript
private CK3dEntity@ FindFirstEntity(BML::ObjectLoadResult@ result) {
    for (int i = 0; i < result.Count; i++) {
        CKObject@ object = result.BorrowObject(i);
        CK3dEntity@ entity = cast<CK3dEntity>(object);
        if (entity !is null) return entity;
    }
    return null;
}
```

## 步骤 2：设置位置

加载出的对象默认位置不确定。我们把它放在玩家球上方 5 个单位，这样能看到它落下来：

```angelscript
CK3dEntity@ playerBall = BorrowActiveBall(ctx);
if (playerBall !is null && BML::CK::IsValid(playerBall)) {
    VxVector pos = BML::CK::GetPosition(playerBall);
    pos.y += 5.0f;
    BML::CK::SetPosition(spawnedBall, pos);
} else {
    BML::CK::SetPosition(spawnedBall, VxVector(0.0f, 10.0f, 0.0f));
}
```

Y 轴加 5 大约是球直径的 2.5 倍，足够看清落下过程。如果找不到玩家球就用固定坐标作为备选。

## 步骤 3：物理化（PhysicalizeDefinition）

填一个 `BML::PhysicalizeDefinition` 结构体，描述物理属性：

```angelscript
BML::PhysicalizeDefinition physics;
physics.Fixed = false;           // false=可动, true=固定（地面/墙壁用）
physics.Friction = 0.6f;        // 摩擦系数，越大越粗糙
physics.Elasticity = 0.2f;      // 弹性系数，越大反弹越高
physics.Mass = 2.0f;            // 质量，越大越难推
physics.CollisionGroup = "";    // 碰撞组，空=默认
physics.EnableCollision = true; // 参与碰撞检测
physics.LinearDamp = 0.6f;      // 线性阻尼，越大越快停下
physics.RotDamp = 0.1f;         // 旋转阻尼，越大越快停转
physics.CollisionSurface = "P_Ball_Wood_Mesh";  // 碰撞面网格名
```

`CollisionSurface` 是物理引擎用来做碰撞检测的简化网格。对于球，它是一个球形 Mesh。这个名字必须对应场景中已存在的 Mesh，它随 .nmo 文件一起加载进来。名字写错则物理化失败。

然后调用：

```angelscript
bool physical = BML::Physics::PhysicalizeBall(spawnedBall, physics,
                                               VxVector(0.0f, 0.0f, 0.0f), 2.0f);
```

四个参数：目标对象、物理定义、碰撞球中心偏移（相对于对象位置）、碰撞球半径。返回 `true` 表示成功。

## 步骤 4：显示

```angelscript
BML::CK::Show(spawnedBall, CKSHOW, true);
```

`true` 表示递归显示子对象。到这一步，画面上就能看到球落下了。

## 清理

创建了物理对象就必须负责清理。不清理会导致：物理残留引发崩溃、句柄悬空、对象堆积导致性能下降。

正确清理顺序：

```angelscript
private void CleanupBall(const BML::ModContext &in ctx) {
    if (spawnedBall is null) return;
    if (BML::CK::IsValid(spawnedBall)) {
        BML::Physics::Unphysicalize(spawnedBall);   // 1. 从物理引擎移除
        BML::CK::Show(spawnedBall, CKHIDE, true);   // 2. 从渲染移除
    }
    @spawnedBall = null;                             // 3. 释放句柄
}
```

顺序很重要：先 Unphysicalize，再 Hide，最后 null。反过来就找不到对象了。

清理时机：关卡退出前、加载新关卡前、游戏退出时、脚本卸载时、用户手动按 K。

## 完整脚本

新建 `ModLoader/Mods/SpawnBall.mod.as`：

```angelscript
[bml.mod id="spawn.ball" name="Spawn Ball" version="1.0.0" author="Tutorial" bml="0.3.12" description="Spawn a physicalized ball"]
class SpawnBall {
    private CK3dEntity@ spawnedBall = null;

    void OnLoad(const BML::ModContext &in ctx) {
        LogInfo(ctx, "SpawnBall loaded");
    }

    void OnUnload(const BML::ModContext &in ctx) {
        CleanupBall(ctx);
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_START_LEVEL) {
            SpawnNewBall(ctx);
        } else if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL ||
                   event == BML::GAME_EVENT_PRE_LOAD_LEVEL ||
                   event == BML::GAME_EVENT_EXIT_GAME) {
            CleanupBall(ctx);
        }
    }

    void OnProcess(const BML::ModContext &in ctx) {
        BML::InputHook@ input = ctx.BorrowInputManager();
        if (input is null) return;

        if (input.IsKeyPressed(CKKEY_J)) {
            SpawnNewBall(ctx);
        }
        if (input.IsKeyPressed(CKKEY_K)) {
            CleanupBall(ctx);
            ctx.SendIngameMessage("Ball cleaned up.");
        }
    }

    private void SpawnNewBall(const BML::ModContext &in ctx) {
        if (spawnedBall !is null) {
            CleanupBall(ctx);
        }

        // 1. 加载资源
        string relativePath = "3D Entities\\PH\\P_Ball_Wood.nmo";
        string resourcePath = BML::Path::Combine(ctx.GetDirectoryUtf8(BML::DIR_GAME), relativePath);
        if (!BML::Path::IsFile(resourcePath)) {
            LogWarn(ctx, "Resource missing: " + relativePath);
            return;
        }

        BML::ObjectLoadOptions options;
        options.File = resourcePath;
        options.Rename = true;
        options.MasterName = "Tutorial_SpawnBall";
        options.AddToScene = true;
        options.ReuseMeshes = true;
        options.ReuseMaterials = true;
        options.Dynamic = true;

        BML::ObjectLoadResult@ result = BML::CK::LoadObject(options);
        if (result is null || !result.Success || result.Count <= 0) {
            LogWarn(ctx, "Load failed");
            return;
        }

        @spawnedBall = FindFirstEntity(result);
        if (spawnedBall is null) {
            LogWarn(ctx, "No entity in loaded result");
            return;
        }

        // 2. 设置位置（放在玩家球上方）
        CK3dEntity@ playerBall = BorrowActiveBall(ctx);
        if (playerBall !is null && BML::CK::IsValid(playerBall)) {
            VxVector pos = BML::CK::GetPosition(playerBall);
            pos.y += 5.0f;
            BML::CK::SetPosition(spawnedBall, pos);
        } else {
            BML::CK::SetPosition(spawnedBall, VxVector(0.0f, 10.0f, 0.0f));
        }

        // 3. 物理化
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

        bool physical = BML::Physics::PhysicalizeBall(spawnedBall, physics,
                                                       VxVector(0.0f, 0.0f, 0.0f), 2.0f);

        // 4. 显示
        BML::CK::Show(spawnedBall, CKSHOW, true);

        string name = BML::CK::GetName(spawnedBall);
        LogInfo(ctx, "Spawned: " + name + " physical=" + BoolText(physical));
        ctx.SendIngameMessage("Ball spawned above player.");
    }

    private void CleanupBall(const BML::ModContext &in ctx) {
        if (spawnedBall is null) return;

        if (BML::CK::IsValid(spawnedBall)) {
            BML::Physics::Unphysicalize(spawnedBall);
            BML::CK::Show(spawnedBall, CKHIDE, true);
        }

        @spawnedBall = null;
        LogInfo(ctx, "Ball cleaned up");
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
            CKObject@ object = result.BorrowObject(i);
            CK3dEntity@ entity = cast<CK3dEntity>(object);
            if (entity !is null) {
                return entity;
            }
        }
        return null;
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

## 运行验证

进入任意关卡后观察：

1. 玩家球上方出现一个木球
2. 木球受重力落下，碰到轨道后弹起（弹性 0.2，弹得不高）
3. 弹几下后停住

日志里应该看到：

```text
[spawn.ball/INFO]: Spawned: Tutorial_SpawnBall_BMLLoad_1 physical=true
```

关键是 `physical=true`。如果为 `false`，物理化失败，球不会落下。

## 失败诊断

| 现象 | 可能原因 | 怎么查 |
| --- | --- | --- |
| "Resource missing" 警告 | 文件路径不对 | 确认游戏目录下有 `3D Entities\PH\P_Ball_Wood.nmo` |
| "Load failed" 警告 | 文件存在但加载出错 | 路径可能有中文，尝试英文路径安装游戏 |
| "No entity in loaded result" | 文件里没有 3D 实体 | 文件可能损坏，换一个 .nmo 试试 |
| `physical=false` | CollisionSurface 不存在 | 确认 `P_Ball_Wood_Mesh` 随 .nmo 一起加载了 |
| 球出现了但不落下 | `Fixed = true` | 确认 `physics.Fixed = false` |
| 球落下了但看不见 | Show 没调用 | 检查日志确认四个步骤都执行了 |
| 关卡切换时崩溃 | 没清理 | 确认 `OnGameEvent` 处理了退出事件 |

## 参数练习

修改 `PhysicalizeDefinition` 参数观察不同效果：

- `Elasticity = 0.9f`：球像乒乓球一样弹很高
- `Mass = 0.1f`：球极轻，容易被弹飞
- `Friction = 0.01f`：球在平面上几乎不停下
- `LinearDamp = 0.0f`：速度衰减极慢

通过修改参数可以直接看到每个字段的物理效果。

## 完成状态

游戏里凭空出现一个木球。它受重力落下，碰到地面会弹起。再次触发生成时，旧球会先被清理，新球重新出现在玩家球上方。到这里应该理解加载、定位、物理化、显示这四步为什么都不能省。

下一步：[18 控制物理对象](tutorial-18-physics-control.md)

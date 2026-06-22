# 参考 M：场景对象、层级和可见性

参考 K讲了怎么记住对象身份。这一章继续往前走：拿到对象以后，先看它在场景里的基本状态。

本章只处理 3D 实体最基础的几件事：

```text
对象是否有效
对象是否显示
对象在哪里
对象有没有父对象和子对象
```

这些都属于“场景对象状态”。它们还没有进入物理、行为图和 Gameplay 规则。

## 本章用哪些 API

先看这一组函数：

| API | 作用 |
| --- | --- |
| `BML::CK::IsValid(object)` | 对象句柄当前是否还能用 |
| `BML::CK::IsVisible(object)` | 对象当前是否显示 |
| `BML::CK::Show(object, CKSHOW)` | 显示对象 |
| `BML::CK::Show(object, CKHIDE)` | 隐藏对象 |
| `BML::CK::GetPosition(entity)` | 读取 3D 实体位置 |
| `BML::CK::SetPosition(entity, position)` | 设置 3D 实体位置 |
| `BML::CK::GetChildCount(entity)` | 子对象数量 |
| `BML::CK::BorrowChild(entity, index)` | 借第 `index` 个子对象 |
| `BML::CK::BorrowParent(entity)` | 借父对象 |

这些 API 都接收 Virtools 对象句柄，例如 `CK3dEntity@`。

参考 K已经讲过：原始 `CK3dEntity@` 不长期保存。要用的时候，先确认 `Entity3DRef@` 仍然有效，再按名字临时借原始实体。

## 本章目标对象

继续使用：

```text
PC_TwoFlames_MF
```

它是第一关里检查点火焰相关的运行时 3D 实体。参考 K已经验证过，第一关开始后可以找到它。

本章会对它做两种操作：

```text
ckscene inspect   只打印状态
ckscene pulse     做一次立即恢复的小动作
```

`pulse` 会读取原始状态，隐藏再显示，移动一点再移回原位。动作很短，只用来验证 API 调用路径，不留下关卡修改。

## 完整脚本

把下面脚本放到：

```text
ModLoader/Mods/SceneMod.mod.as
```

```angelscript
[bml.mod id="scene.script" name="Scene Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="Observe scene object state"]
class SceneMod {
    private string targetName = "PC_TwoFlames_MF";
    private Entity3DRef@ targetRef;

    void OnLoad(const BML::ModContext &in ctx) {
        BML::CommandDefinition def;
        def.Name = "ckscene";
        def.Description = "Inspect scene object state";
        def.Usage = "ckscene [inspect|pulse]";

        BML::CommandCallback@ execute = BML::CommandCallback(this.OnSceneCommand);
        BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompleteSceneCommand);
        ctx.RegisterCommand(def, execute, complete);

        ctx.BorrowLogger().Info("SceneMod loaded; use ckscene after level start");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL ||
            event == BML::GAME_EVENT_PRE_LOAD_LEVEL) {
            // 旧关卡对象在离开关卡时不再使用。
            @targetRef = null;
        }
    }

    private void OnSceneCommand(const BML::ModContext &in ctx,
                                const BML::CommandEvent &in event) {
        string action = "inspect";
        if (event.ArgCount > 0) {
            action = event.GetArg(0);
        }

        if (!EnsureTarget(ctx)) {
            return;
        }

        if (action == "inspect") {
            InspectTarget(ctx);
        } else if (action == "pulse") {
            PulseTarget(ctx);
        } else {
            ctx.BorrowLogger().Warn("Unknown ckscene action: " + action);
        }
    }

    private void CompleteSceneCommand(const BML::ModContext &in ctx,
                                      const BML::CommandEvent &in event,
                                      BML::CommandCompletion &inout completions) {
        completions.Add("inspect");
        completions.Add("pulse");
    }

    private bool EnsureTarget(const BML::ModContext &in ctx) {
        if (targetRef !is null && targetRef.valid) {
            return true;
        }

        CKContext@ ck = ctx.BorrowCKContext();
        if (ck is null) {
            ctx.BorrowLogger().Warn("BorrowCKContext returned null");
            return false;
        }

        @targetRef = Scene::FindEntity3D(ck, targetName, 0, true);
        if (targetRef is null || !targetRef.valid) {
            string reason = targetRef is null ? "null ref" : targetRef.Error();
            ctx.BorrowLogger().Warn("Scene::FindEntity3D failed: " + reason);
            return false;
        }

        return true;
    }

    private void InspectTarget(const BML::ModContext &in ctx) {
        BML::Logger@ log = ctx.BorrowLogger();
        CK3dEntity@ entity = BorrowTargetEntity(ctx, log);
        if (entity is null) {
            return;
        }

        VxVector pos = BML::CK::GetPosition(entity);
        log.Info("Scene target: name=" + BML::CK::GetName(entity) +
                 " id=" + BML::CK::GetId(entity) +
                 " class=" + BML::CK::GetClassId(entity) +
                 " visible=" + BoolText(BML::CK::IsVisible(entity)) +
                 " dynamic=" + BoolText(BML::CK::IsDynamic(entity)) +
                 " pos=" + VectorText(pos));

        CK3dEntity@ parent = BML::CK::BorrowParent(entity);
        if (parent is null) {
            log.Info("Scene parent: none");
        } else {
            log.Info("Scene parent: " + BML::CK::GetName(parent) +
                     " id=" + BML::CK::GetId(parent));
        }

        int childCount = BML::CK::GetChildCount(entity);
        log.Info("Scene child count: " + childCount);
        if (childCount > 0) {
            CK3dEntity@ child = BML::CK::BorrowChild(entity, 0);
            if (child !is null) {
                log.Info("Scene first child: " + BML::CK::GetName(child) +
                         " id=" + BML::CK::GetId(child));
            }
        }
    }

    private void PulseTarget(const BML::ModContext &in ctx) {
        BML::Logger@ log = ctx.BorrowLogger();
        CK3dEntity@ entity = BorrowTargetEntity(ctx, log);
        if (entity is null) {
            return;
        }

        bool wasVisible = BML::CK::IsVisible(entity);
        VxVector oldPos = BML::CK::GetPosition(entity);
        VxVector newPos = oldPos + VxVector(0.0f, 0.2f, 0.0f);

        // 先做很小的改变，再立刻恢复。
        BML::CK::Show(entity, CKHIDE);
        BML::CK::SetPosition(entity, newPos);
        BML::CK::SetPosition(entity, oldPos);
        BML::CK::Show(entity, wasVisible ? CKSHOW : CKHIDE);

        log.Info("Scene pulse restored: visible=" + BoolText(wasVisible) +
                 " pos=" + VectorText(oldPos));
    }

    private CK3dEntity@ BorrowTargetEntity(const BML::ModContext &in ctx,
                                           BML::Logger@ log) {
        if (targetRef is null || !targetRef.valid) {
            log.Warn("target ref is invalid");
            return null;
        }

        CK3dEntity@ entity = ctx.Borrow3dEntityByName(targetName);
        if (entity is null || !BML::CK::IsValid(entity)) {
            log.Warn("Borrow3dEntityByName failed: " + targetName);
            return null;
        }

        int entityId = BML::CK::GetId(entity);
        if (entityId != targetRef.Id()) {
            log.Warn("Borrowed entity id changed: ref=" + targetRef.Id() +
                     " borrowed=" + entityId);
            return null;
        }

        return entity;
    }

    private string BoolText(bool value) {
        return value ? "true" : "false";
    }

    private string VectorText(const VxVector &in value) {
        return "(" + value.x + ", " + value.y + ", " + value.z + ")";
    }
}
```

## 先看查找流程

这章没有直接用 `ctx.Borrow3dEntityByName(targetName)`。

脚本先用 CKAS `Scene::FindEntity3D` 保存 `Entity3DRef@`：

```angelscript
@targetRef = Scene::FindEntity3D(ck, targetName, 0, true);
```

然后每次真正操作前，再按名字临时借原始实体，并用 id 对照：

```angelscript
CK3dEntity@ entity = ctx.Borrow3dEntityByName(targetName);
if (entity is null || !BML::CK::IsValid(entity)) {
    log.Warn("Borrow3dEntityByName failed: " + targetName);
    return;
}

if (BML::CK::GetId(entity) != targetRef.Id()) {
    log.Warn("Borrowed entity id changed");
    return;
}
```

这就是参考 K的规则：

```text
保存身份用 Entity3DRef@
实际操作前按名字借 CK3dEntity@
每次操作前重新检查，并确认 id 没变
```

## 读取位置

位置使用 `VxVector`：

```angelscript
VxVector pos = BML::CK::GetPosition(entity);
```

`VxVector` 有三个字段：

```text
x
y
z
```

所以日志里可以写：

```angelscript
"pos=" + VectorText(pos)
```

辅助函数只是把三个数字拼成一行：

```angelscript
private string VectorText(const VxVector &in value) {
    return "(" + value.x + ", " + value.y + ", " + value.z + ")";
}
```

## 读取父子层级

Virtools 里的 3D 实体可以有父子关系。

```angelscript
CK3dEntity@ parent = BML::CK::BorrowParent(entity);
```

父对象可能没有，所以先判空。

子对象按数量和索引读取：

```angelscript
int childCount = BML::CK::GetChildCount(entity);
if (childCount > 0) {
    CK3dEntity@ child = BML::CK::BorrowChild(entity, 0);
}
```

这里的 `BorrowParent` 和 `BorrowChild` 也返回原始 `CK3dEntity@`。打印完就结束，不保存到成员变量。

## 显示和隐藏

显示状态用：

```angelscript
bool wasVisible = BML::CK::IsVisible(entity);
```

隐藏和显示用：

```angelscript
BML::CK::Show(entity, CKHIDE);
BML::CK::Show(entity, CKSHOW);
```

`pulse` 里最后用原始状态恢复：

```angelscript
BML::CK::Show(entity, wasVisible ? CKSHOW : CKHIDE);
```

这样写的意思是：如果它原本显示，就恢复显示；如果它原本隐藏，就恢复隐藏。

## 移动和恢复

`pulse` 里保存原始位置：

```angelscript
VxVector oldPos = BML::CK::GetPosition(entity);
```

算一个很小的新位置：

```angelscript
VxVector newPos = oldPos + VxVector(0.0f, 0.2f, 0.0f);
```

然后立即恢复：

```angelscript
BML::CK::SetPosition(entity, newPos);
BML::CK::SetPosition(entity, oldPos);
```

本章这样写，是为了验证 `SetPosition` 的调用方式，不是为了做玩法效果。真正要移动对象时，要先判断对象是不是原版流程会控制的位置，是否有物理对象，是否需要同步行为图或 DataArray。

## 运行后看什么

进入第一关后执行：

```text
ckscene inspect
```

应当看到对象状态、父对象、子对象数量。例如一次实际运行里输出：

```text
Scene target: name=PC_TwoFlames_MF id=46378 class=33 visible=false dynamic=true pos=(0, 0, 0)
Scene parent: none
Scene child count: 3
Scene first child: PC_TwoFlames_Flame_SmallB id=46008
```

这个结果很有用：`PC_TwoFlames_MF` 本体不是直接显示出来的火焰，它更像一个父实体。真正能看到的火焰在它的子对象里。

再执行：

```text
ckscene pulse
```

应当看到恢复日志：

```text
Scene pulse restored: visible=false pos=(0, 0, 0)
```

这说明脚本完成了：

```text
读取可见性
读取位置
隐藏再恢复
移动再恢复
```

如果还在菜单里执行，可能找不到目标对象。等关卡开始后再运行命令。

## 这一章的边界

`Show` 和 `SetPosition` 看起来很直接，但它们只改 Virtools 对象当前状态。

它们不会自动改这些东西：

```text
DataArray 里的关卡状态
行为图里的流程
物理对象的位置和速度
检查点、复活点、机关的 Gameplay 逻辑
```

所以本章只做立即恢复的小动作。下一章讲 DataArray 写入和回滚时，会继续强调：每次修改前先保存旧值，退出关卡或卸载脚本时恢复。


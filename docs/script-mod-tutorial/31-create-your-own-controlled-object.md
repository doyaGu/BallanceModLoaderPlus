# 第 31 章：创建自己的可控对象

上一章讲风险分级。这一章进入中风险练习：操作一个自己加载出来、自己清理的可控对象。

原则很简单：

```text
先碰自己的对象
再碰原版对象
先能清理
再做玩法修改
```

当前球、检查点、复活点、Gameplay 里的关键对象，都有原版流程在管理。刚开始练习移动、显示、物理化时，直接拿它们练习，排错会很困难。

## 本章目标

这一章做一个小练习：

```text
加载一个 3D 资源
从加载结果里找到一个 CK3dEntity
移动它
显示它
给它做一次球体物理化
取消物理化
隐藏它
清空脚本保存的句柄
```

这段脚本定位为练习对象，用来给后面的可回滚修改打基础。

## 需要哪些 API

本章用到这些 API：

| API | 用途 |
| --- | --- |
| `BML::ObjectLoadOptions` | 描述要加载的资源 |
| `BML::CK::LoadObject` | 加载资源 |
| `BML::ObjectLoadResult` | 查看加载出来的对象 |
| `cast<CK3dEntity>(object)` | 从 `CKObject@` 尝试转成 3D 实体 |
| `BML::CK::SetPosition` | 移动 3D 实体 |
| `BML::CK::Show` | 显示或隐藏对象 |
| `BML::Physics::PhysicalizeBall` | 给对象建立球体物理 |
| `BML::Physics::Unphysicalize` | 移除物理表示 |

脚本侧没有暴露通用的 `DestroyObject`。所以本章的清理动作是：

```text
取消物理化
隐藏对象
清空脚本保存的句柄
```

这能避免对象继续参与物理或显示。真正销毁对象不放在这章处理。

## 先看核心流程

完整代码先不用急着看。这个练习的流程是：

```angelscript
CK3dEntity@ entity = LoadControlledObject(ctx);
BML::CK::SetPosition(entity, VxVector(0.0f, 8.0f, 0.0f));
BML::CK::Show(entity, CKSHOW, true);
bool physical = PhysicalizeControlledObject(entity);
CleanupControlledObject(ctx);
```

对应到动作就是：

| 步骤 | 目的 |
| --- | --- |
| 加载资源 | 创建一个脚本自己控制的对象 |
| 找到 `CK3dEntity` | 确认它能被移动和显示 |
| `SetPosition` | 放到容易观察的位置 |
| `Show` | 让它出现在画面里 |
| `PhysicalizeBall` | 给它建立物理表示 |
| `Unphysicalize` / `Show(CKHIDE)` | 把可控对象收掉 |

第 28 章已经用木球做过一遍。这里换成更普通的资源，是为了练“可控对象”的创建和清理形状。

## 最小练习脚本

新建：

```text
ModLoader/Mods/ControlledObjectMod.mod.as
```

写入：

```angelscript
[bml.mod id="controlled.object" name="Controlled Object" version="1.0.0" author="Tutorial" bml="0.3.12" description="Loads and cleans a private controlled object"]
class ControlledObjectMod {
    private CK3dEntity@ controlledEntity;
    private bool hasPhysics = false;

    void OnLoad(const BML::ModContext &in ctx) {
        LoadControlledObject(ctx);
    }

    void OnUnload(const BML::ModContext &in ctx) {
        CleanupControlledObject(ctx);
    }

    private void LoadControlledObject(const BML::ModContext &in ctx) {
        string relativePath = "3D Entities\\PH\\P_Modul_01.nmo";
        string resourcePath = BML::Path::Combine(ctx.GetDirectoryUtf8(BML::DIR_GAME), relativePath);

        if (!BML::Path::IsFile(resourcePath)) {
            LogInfo(ctx, "resource missing: " + relativePath);
            return;
        }

        BML::ObjectLoadOptions options;
        options.File = resourcePath;
        options.Rename = true;
        options.MasterName = "Tutorial_Controlled_Object";
        options.AddToScene = true;
        options.ReuseMeshes = true;
        options.ReuseMaterials = true;
        options.Dynamic = true;

        BML::ObjectLoadResult@ result = BML::CK::LoadObject(options);
        if (result is null || !result.Success) {
            LogInfo(ctx, "load failed");
            return;
        }

        @controlledEntity = FindFirstEntity(result);
        if (controlledEntity is null) {
            LogInfo(ctx, "no CK3dEntity found");
            return;
        }

        LogInfo(ctx, "entity name=" + BML::CK::GetName(controlledEntity));

        // 把可控对象放到一个容易观察的位置，再显示它和子对象。
        BML::CK::SetPosition(controlledEntity, VxVector(0.0f, 8.0f, 0.0f));
        BML::CK::Show(controlledEntity, CKSHOW, true);

        PhysicalizeControlledObject(ctx);
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

    private void PhysicalizeControlledObject(const BML::ModContext &in ctx) {
        if (controlledEntity is null) {
            return;
        }

        BML::PhysicalizeDefinition physics;
        physics.Fixed = true;
        physics.Friction = 0.5f;
        physics.Elasticity = 0.1f;
        physics.Mass = 1.0f;
        physics.CollisionGroup = "Floor";
        physics.EnableCollision = true;

        hasPhysics = BML::Physics::PhysicalizeBall(controlledEntity,
                                                   physics,
                                                   VxVector(0.0f, 0.0f, 0.0f),
                                                   2.0f);
        LogInfo(ctx, "physicalize=" + BoolText(hasPhysics));
    }

    private void CleanupControlledObject(const BML::ModContext &in ctx) {
        if (controlledEntity is null) {
            return;
        }

        if (hasPhysics) {
            LogInfo(ctx, "unphysicalize=" + BoolText(BML::Physics::Unphysicalize(controlledEntity)));
            hasPhysics = false;
        }

        BML::CK::Show(controlledEntity, CKHIDE, true);

        // 脚本不再使用这个借来的对象句柄。
        @controlledEntity = null;
        LogInfo(ctx, "controlled object hidden and released");
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
}
```

正常结果会看到：

```text
Loading Mod controlled.object[Controlled Object] v1.0.0 by Tutorial
entity name=P_Modul_01_Pusher_BMLLoad_1
physicalize=true
BML script mod summary: loaded=1 failed=0
```

验证清理路径时，会看到：

```text
unphysicalize=true
controlled object hidden and released
```

## 代码怎么读

先看入口：

```angelscript
void OnLoad(const BML::ModContext &in ctx) {
    LoadControlledObject(ctx);
}

void OnUnload(const BML::ModContext &in ctx) {
    CleanupControlledObject(ctx);
}
```

`OnLoad` 负责创建可控对象。  
`OnUnload` 负责把对象状态收掉。

这种结构比“写一堆代码在 `OnProcess` 里”更容易排错。对象创建、使用、清理分别有位置。

## 加载资源

加载资源分三步：

```angelscript
string relativePath = "3D Entities\\PH\\P_Modul_01.nmo";
string resourcePath = BML::Path::Combine(ctx.GetDirectoryUtf8(BML::DIR_GAME), relativePath);

if (!BML::Path::IsFile(resourcePath)) {
    LogInfo(ctx, "resource missing: " + relativePath);
    return;
}
```

先用相对游戏目录的路径描述资源，再拼成完整路径。教程正文不要写自己的本机绝对路径；脚本运行时用 `ctx.GetDirectoryUtf8(BML::DIR_GAME)` 取得当前游戏目录。

`BML::Path::IsFile` 是路径检查。  
`BML::CK::LoadObject` 是执行加载。  
这两个步骤分开写，错误日志会清楚很多。

## ObjectLoadOptions

本章的加载选项：

```angelscript
BML::ObjectLoadOptions options;
options.File = resourcePath;
options.Rename = true;
options.MasterName = "Tutorial_Controlled_Object";
options.AddToScene = true;
options.ReuseMeshes = true;
options.ReuseMaterials = true;
options.Dynamic = true;
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `File` | 要加载的资源路径 |
| `Rename` | 加载时给对象改名，避免和已有对象重名 |
| `MasterName` | 改名时使用的主名前缀 |
| `AddToScene` | 把对象加入当前场景 |
| `ReuseMeshes` | 复用网格资源 |
| `ReuseMaterials` | 复用材质资源 |
| `Dynamic` | 作为动态对象加载 |

`Rename = true` 很重要。可控对象不应该抢原版对象名。

## 从结果里找 3D 实体

`LoadObject` 返回的是一组 `CKObject@`。里面可能有组、材质、行为、3D 实体等不同类型。

所以要遍历：

```angelscript
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
```

`cast<CK3dEntity>(object)` 成功，说明这个对象能当作 3D 实体使用。失败时返回 `null`。

拿到实体后再移动：

```angelscript
BML::CK::SetPosition(controlledEntity, VxVector(0.0f, 8.0f, 0.0f));
BML::CK::Show(controlledEntity, CKSHOW, true);
```

`true` 表示连同层级里的子对象一起显示。

## 物理化和清理

物理化用 `PhysicalizeDefinition`：

```angelscript
BML::PhysicalizeDefinition physics;
physics.Fixed = true;
physics.Friction = 0.5f;
physics.Elasticity = 0.1f;
physics.Mass = 1.0f;
physics.CollisionGroup = "Floor";
physics.EnableCollision = true;

hasPhysics = BML::Physics::PhysicalizeBall(controlledEntity,
                                           physics,
                                           VxVector(0.0f, 0.0f, 0.0f),
                                           2.0f);
```

这段只为练习对象生命周期。`Fixed = true` 能降低物理练习的干扰。刚开始不要让可控对象满场乱动。

清理时按相反顺序：

```angelscript
if (hasPhysics) {
    BML::Physics::Unphysicalize(controlledEntity);
    hasPhysics = false;
}

BML::CK::Show(controlledEntity, CKHIDE, true);
@controlledEntity = null;
```

`@controlledEntity = null` 清掉的是脚本保存的句柄。它不销毁 Virtools 对象。

## 为什么不从当前球开始

当前球很诱人，因为它最容易看到效果。但当前球同时被多套原版流程管理：

```text
输入控制
摄像机
物理化
变球
死亡复活
重开关卡
过关结算
```

刚开始用当前球练习，出问题时很难判断是哪一层冲突。

自己的可控对象边界更清楚：

```text
对象由脚本加载
位置由脚本设置
物理由脚本添加
清理由脚本触发
不写 CurrentLevel
不改 Gameplay 行为图
```

这就是中风险练习该有的形状。

## 本章检查清单

写可控对象练习时至少检查：

```text
资源文件存在
LoadObject 返回成功
加载结果里确实有 CK3dEntity
移动前确认实体不为空
物理化结果写日志
清理时先 Unphysicalize
隐藏对象
清空脚本保存的句柄
```

如果下一步想让可控对象长期存在，还要继续补：

```text
进入关卡时创建
退出关卡时清理
重开关卡时重新创建
命令或配置开关控制显示
避免每帧重复加载资源
```

第 32 章会把这些收进一个完整小 mod：命令、配置、UI、查看状态和可回滚的小修改。



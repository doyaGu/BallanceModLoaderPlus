# 参考 Q：球、地板和模块注册

对象、表和行为图处理的是已经存在的东西。内容注册换一个角度：让 BML 在游戏启动时登记新的“类型”。

这里的“类型”指启动期登记记录，不指当前场景里的某一个物体：

```text
启动时登记：
  新球类型
  新地板物理类型
  新模块物理类型
  新变球器名字

关卡加载时：
  BML 把这些登记记录写入原版使用的 DataArray 和行为图补丁里
```

所以本参考只讲 BMLPlus 提供的内容注册入口。移动物体、临时改当前球，属于运行时受控修改。

## 注册时机

这些 API 只能在 `OnLoad` 里成功：

```text
ctx.RegisterBallType(...)
ctx.RegisterFloorType(...)
ctx.RegisterModule(...)
```

原因很直接：BML 要在后续加载 `Levelinit`、`Gameplay_Ingame` 等原版脚本时，把登记过的类型补进去。等关卡已经开始，再登记就赶不上这些补丁时机。

如果在 `OnProcess`、`OnStartLevel` 这类回调里注册，返回值会是 `false`。

## 最小可运行示例

先写一个只登记地板类型、模块名和变球器名的脚本。它不需要准备新的球资源文件，适合作为第一段测试代码。

新建：

```text
ModLoader/Mods/RegisterDemo.mod.as
```

写入：

```angelscript
[bml.mod id="register.demo" name="Register Demo" version="1.0.0" author="Tutorial" bml="0.3.12" description="Tests content registration"]
class RegisterDemo {
    private bool checkedLateCall = false;

    void OnLoad(const BML::ModContext &in ctx) {
        BML::FloorTypeDefinition floor;
        floor.Name = "Tutorial_Debug_Floor";
        floor.Friction = 0.55f;
        floor.Elasticity = 0.15f;
        floor.Mass = 1.0f;
        floor.CollisionGroup = "Floor";
        floor.EnableCollision = true;

        // RegisterFloorType 必须在 OnLoad 里调用。
        LogInfo(ctx, "floor registered=" + BoolText(ctx.RegisterFloorType(floor)));

        BML::ModuleDefinition module;
        module.Name = "Tutorial_Debug_Module";

        // ModuleDefinition 只登记一个普通模块物理组名字。
        LogInfo(ctx, "module registered=" + BoolText(ctx.RegisterModule(module)));

        BML::TrafoDefinition trafo;
        trafo.Name = "Tutorial_Debug_Trafo";

        // TrafoDefinition 登记一个变球器物理组名字。
        LogInfo(ctx, "trafo registered=" + BoolText(ctx.RegisterModule(trafo)));
    }

    void OnProcess(const BML::ModContext &in ctx) {
        if (checkedLateCall) {
            return;
        }
        checkedLateCall = true;

        BML::FloorTypeDefinition floor;
        floor.Name = "Tutorial_Late_Floor";

        // 这里已经离开 OnLoad，注册会失败。
        LogInfo(ctx, "late floor registered=" + BoolText(ctx.RegisterFloorType(floor)));
    }

    private void LogInfo(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info(message);
        }
    }

    private string BoolText(bool value) {
        return value ? "true" : "false";
    }
}
```

正常结果：

```text
Loading Mod register.demo[Register Demo] v1.0.0 by Tutorial
Registered New Floor Type: Tutorial_Debug_Floor
floor registered=true
Registered New Modul Type: Tutorial_Debug_Module
module registered=true
Registered New Ball Transformer Type: Tutorial_Debug_Trafo
trafo registered=true
BML script mod summary: loaded=1 failed=0
late floor registered=false
New Modul & Floor Types Registered
```

这一段结果里有两个重点：

```text
OnLoad 里的三个注册都成功。
OnProcess 里的晚注册失败。
```

`Registered New ...` 是内置 `NewBallType` 服务打印的日志。脚本自己的 `... registered=true` 说明 `ctx.Register...` 返回了成功。

## FloorTypeDefinition

`FloorTypeDefinition` 登记一种地板物理类型：

```angelscript
BML::FloorTypeDefinition floor;
floor.Name = "Tutorial_Debug_Floor";
floor.Friction = 0.55f;
floor.Elasticity = 0.15f;
floor.Mass = 1.0f;
floor.CollisionGroup = "Floor";
floor.EnableCollision = true;
ctx.RegisterFloorType(floor);
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `Name` | 写入物理表的类型名，也是后续对象分组要对上的名字 |
| `Friction` | 摩擦 |
| `Elasticity` | 弹性 |
| `Mass` | 质量 |
| `CollisionGroup` | 碰撞组名 |
| `EnableCollision` | 是否启用碰撞 |

`Name` 不能为空。它是脚本侧第一道必填检查。名字为空时，注册函数直接返回 `false`。

## ModuleDefinition 和 TrafoDefinition

模块注册有两个最轻的定义：

```angelscript
BML::ModuleDefinition module;
module.Name = "Tutorial_Debug_Module";
ctx.RegisterModule(module);

BML::TrafoDefinition trafo;
trafo.Name = "Tutorial_Debug_Trafo";
ctx.RegisterModule(trafo);
```

`ModuleDefinition` 对应普通模块物理组。
`TrafoDefinition` 对应变球器物理组。

它们只有一个字段：

| 字段 | 含义 |
| --- | --- |
| `Name` | 写入 `PH_Groups` 的名字 |

脚本侧只负责登记这个名字。真正有没有同名对象组、关卡里有没有用到它，要到关卡内容和原版脚本处理阶段才会体现。

## ModuleBallDefinition

`ModuleBallDefinition` 用来登记按球体方式物理化的模块：

```angelscript
BML::ModuleBallDefinition moduleBall;
moduleBall.Name = "Tutorial_Ball_Module";
moduleBall.Fixed = false;
moduleBall.Friction = 0.45f;
moduleBall.Elasticity = 0.15f;
moduleBall.Mass = 1.0f;
moduleBall.CollisionGroup = "Ball";
moduleBall.StartFrozen = false;
moduleBall.EnableCollision = true;
moduleBall.CalcMassCenter = false;
moduleBall.LinearDamp = 0.0f;
moduleBall.RotDamp = 0.0f;
moduleBall.Radius = 2.0f;

ctx.RegisterModule(moduleBall);
```

字段可以按三组理解：

| 字段组 | 字段 |
| --- | --- |
| 名字 | `Name` |
| 物理基本参数 | `Fixed`、`Friction`、`Elasticity`、`Mass`、`CollisionGroup` |
| 运行状态 | `StartFrozen`、`EnableCollision`、`CalcMassCenter` |
| 球体参数 | `LinearDamp`、`RotDamp`、`Radius` |

`Name` 仍然是必填字段。其余数值不要随手乱填，先从接近原版的参数开始。

## ModuleConvexDefinition

`ModuleConvexDefinition` 用来登记按凸体方式物理化的模块：

```angelscript
BML::ModuleConvexDefinition moduleConvex;
moduleConvex.Name = "Tutorial_Convex_Module";
moduleConvex.Fixed = true;
moduleConvex.Friction = 0.5f;
moduleConvex.Elasticity = 0.1f;
moduleConvex.Mass = 1.0f;
moduleConvex.CollisionGroup = "Floor";
moduleConvex.StartFrozen = false;
moduleConvex.EnableCollision = true;
moduleConvex.CalcMassCenter = false;
moduleConvex.LinearDamp = 0.0f;
moduleConvex.RotDamp = 0.0f;

ctx.RegisterModule(moduleConvex);
```

它和 `ModuleBallDefinition` 很接近，少了 `Radius`。选择哪个定义，取决于这个模块要按球体还是凸体处理。

## BallTypeDefinition

新球类型比地板和模块定义更重。它不只登记几项物理参数，还要提供一个能被加载的资源文件。

结构如下：

```angelscript
BML::BallTypeDefinition ball;
ball.BallFile = "TutorialBall.nmo";
ball.BallId = "tutorial.ball";
ball.BallName = "Tutorial Ball";
ball.ObjectName = "Tutorial_Ball";
ball.Friction = 0.45f;
ball.Elasticity = 0.2f;
ball.Mass = 1.0f;
ball.CollisionGroup = "Ball";
ball.LinearDamp = 0.0f;
ball.RotDamp = 0.0f;
ball.Force = 20.0f;
ball.Radius = 2.0f;

ctx.RegisterBallType(ball);
```

必填字符串：

| 字段 | 含义 |
| --- | --- |
| `BallFile` | 新球资源文件名 |
| `BallId` | BML 层识别用 id |
| `BallName` | 显示和日志里的名字 |
| `ObjectName` | 资源里主球对象的名字 |

`BallFile` 要指向按 BMLPlus 约定准备好的新球资源。内置服务会在加载球资源阶段读取它，并在资源里查找一组固定对象：

```text
All_<ObjectName>
<ObjectName>
<ObjectName>_Pieces
<ObjectName>Pieces_Frame
Ball_Explosion_<BallName>
Ball_ResetPieces_<BallName>
```

这些对象缺任何一个，新球类型后续初始化都会失败。

所以新球类型不适合作为第一段注册验证。先把地板和模块注册跑通，再准备完整球资源。

## 注册和运行时修改的区别

注册类 API 做的是“告诉 BML 需要补哪些类型”。它不会立刻在当前场景里变出一个物体。

对比一下：

| 目标 | 应该看哪类 API |
| --- | --- |
| 增加一种可被原版物理脚本识别的地板/模块/球类型 | 本参考的 `Register...` |
| 查找当前场景里的对象 | `ctx.BorrowObjectByName`、`Scene::*` |
| 临时显示、隐藏、移动一个对象 | `BML::CK` 或 CKAS 场景对象 API |
| 运行 Building Block | CKAS 的 `BBConfig` / `BBInstance` |
| 修改 DataArray 中当前关卡状态 | DataArray API，需要更高风险控制 |

不要用注册 API 解决运行时对象问题。它们的职责不一样。

## 注册失败时查什么

注册函数返回 `false`，先查三件事：

| 检查项 | 说明 |
| --- | --- |
| 调用位置 | 只能在 `OnLoad` 里注册 |
| 必填字段 | `Name`，以及新球类型的 `BallFile/BallId/BallName/ObjectName` |
| 目标服务 | 日志里应该先加载内置 `NewBallType` |

注册函数返回 `true`，仍然可能出问题。尤其是新球类型，资源文件和对象命名必须和 BMLPlus 期待的结构一致。

## 速查结论

本参考只需要形成一个模型：

```text
Register... 是启动期登记。
登记成功不等于当前场景立刻出现对象。
新球类型依赖完整资源结构，地板和模块注册更适合作为起步验证。
```

# 第 27 章：对象接入物理系统

第 22 章读过 `Physicalize_Balls`：

```text
P_Ball_Wood   friction=0.6  elasticity=0.2  mass=2   radius=2
P_Ball_Stone  friction=0.7  elasticity=0.1  mass=10  radius=2
```

这些值在表里，还不等于已经用到了运行中的对象上。进入关卡以后，原版行为图会调用物理化 Building Block，把 3D 对象接入物理系统。BMLPlus 能在这个时机调用脚本的 `OnPhysicalize`。

本章做成这样：

```text
进入关卡
原版开始物理化对象
脚本收到 OnPhysicalize
日志里记录木球事件
把事件里的 friction / elasticity / mass / radius 和 Physicalize_Balls 对上
```

本章不画事件列表。这类事件很多，窗口里只显示“最新一条”没有多少分析价值。要记录事件，写日志；要看状态，输出摘要。

## 先分层

物理相关内容先分成三层：

| 层 | 例子 | 本章处理吗 |
| --- | --- | --- |
| 参数表 | `Physicalize_Balls` | 第 22 章已经读过 |
| 对象接入物理系统的事件 | `OnPhysicalize` | 本章处理 |
| 物理操作 | `WakeUp`、`Impulse`、`SetForce` | 后面再处理 |

`Physicalize` 可以先理解成：

```text
把一个 3D 对象接入物理系统。
```

接入时会带上摩擦、弹性、质量、碰撞形状等信息。脚本收到的 `PhysicalizeEvent` 就是这次动作的快照。

## 先看核心流程

去掉命令和日志格式以后，核心流程是：

```angelscript
void OnPhysicalize(const BML::ModContext &in ctx,
                   const BML::PhysicalizeEvent &in event) {
    eventCount++;
    string line = DescribeEvent(event);
    CaptureKnownBall(ctx, event, line);
}
```

这一章不创建对象，不推动对象，只在对象接入物理系统时记一笔。

| 步骤 | 目的 |
| --- | --- |
| `eventCount++` | 知道发生过多少次物理化 |
| `DescribeEvent` | 把事件字段整理成一行日志 |
| `CaptureKnownBall` | 抓住木球、石球这类容易对照的事件 |

下一章会反过来：脚本自己创建一个木球，再主动调用 `PhysicalizeBall`。

## OnPhysicalize

BML 脚本可以写这个回调：

```angelscript
void OnPhysicalize(const BML::ModContext &in ctx,
                   const BML::PhysicalizeEvent &in event)
```

每发生一次物理化，BMLPlus 就调用一次这个函数。

`event` 里常用字段有这些：

| 字段 | 含义 |
| --- | --- |
| `TargetName` | 被物理化的对象名 |
| `Friction` | 摩擦 |
| `Elasticity` | 弹性 |
| `Mass` | 质量 |
| `CollisionGroup` | 碰撞分组 |
| `CollisionSurface` | 碰撞表面 |
| `BallCount` | 球形碰撞体数量 |
| `ConvexCount` | 凸体碰撞数量 |
| `ConcaveCount` | 凹体碰撞数量 |

如果 `BallCount > 0`，还能读第一个球形碰撞体：

```angelscript
VxVector center = event.GetBallCenter(0);
float radius = event.GetBallRadius(0);
```

先检查 `BallCount`，再读半径。不是每个对象都用球形碰撞。

## 完整脚本

新建：

```text
ModLoader/Mods/PhysEventMod.mod.as
```

写入：

```angelscript
[bml.mod id="physevent.script" name="Physicalize Event" version="1.0.0" author="Tutorial" bml="0.3.12" description="Observe Physicalize events"]
class PhysEventMod {
    private BML::CommandRef@ commandRef;

    private int eventCount = 0;
    private int borrowedTargets = 0;
    private int ballShapes = 0;
    private int convexShapes = 0;
    private int concaveShapes = 0;

    private int detailLimit = 12;
    private int detailLogs = 0;

    private bool woodFound = false;
    private bool stoneFound = false;
    private string woodLine = "not found";
    private string stoneLine = "not found";

    void OnLoad(const BML::ModContext &in ctx) {
        RegisterCommand(ctx);
        LogInfo(ctx, "PhysEventMod loaded");
        ctx.SendIngameMessage("PhysEventMod loaded. Enter a level and check ModLoader log.");
    }

    void OnUnload(const BML::ModContext &in ctx) {
        LogSummary(ctx, "unload");
        LogInfo(ctx, "PhysEventMod unloaded");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_START_LEVEL) {
            LogSummary(ctx, "level-start");
            ctx.SendIngameMessage("Physicalize summary written to log.");
        }
    }

    void OnPhysicalize(const BML::ModContext &in ctx,
                       const BML::PhysicalizeEvent &in event) {
        eventCount++;
        ballShapes += event.BallCount;
        convexShapes += event.ConvexCount;
        concaveShapes += event.ConcaveCount;

        string line = DescribeEvent(event);

        // BorrowTarget() 只在当前回调里借用目标，不保存到成员变量。
        CK3dEntity@ target = event.BorrowTarget();
        if (target !is null) {
            borrowedTargets++;
            line +=
                " targetValid=" + BoolText(BML::CK::IsValid(target)) +
                " targetClass=" + BML::CK::GetClassId(target);
        } else {
            line += " target=null";
        }

        if (detailLogs < detailLimit) {
            detailLogs++;
            LogInfo(ctx, "PhysEvent " + line);
        }

        CaptureKnownBall(ctx, event, line);
    }

    private void CaptureKnownBall(const BML::ModContext &in ctx,
                                  const BML::PhysicalizeEvent &in event,
                                  const string &in line) {
        if (event.BallCount <= 0) {
            return;
        }

        if (!woodFound && event.CollisionSurface == "P_Ball_Wood_Mesh") {
            woodFound = true;
            woodLine = line;
            LogInfo(ctx, "PhysEvent wood " + line);
        }

        if (!stoneFound && event.CollisionSurface == "P_Ball_Stone_Mesh") {
            stoneFound = true;
            stoneLine = line;
            LogInfo(ctx, "PhysEvent stone " + line);
        }
    }

    private void RegisterCommand(const BML::ModContext &in ctx) {
        BML::CommandDefinition def;
        def.Name = "ckphy";
        def.Description = "Show Physicalize event statistics";
        def.Usage = "ckphy [summary|reset]";
        def.Category = "Tutorial";
        def.Enabled = true;

        BML::CommandCallback@ execute = BML::CommandCallback(this.OnPhysCommand);
        BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompletePhysCommand);

        @commandRef = ctx.RegisterCommand(def, execute, complete);
        LogInfo(ctx, "PhysEvent command registered=" + BoolText(commandRef !is null && commandRef.IsValid));
    }

    void OnPhysCommand(const BML::ModContext &in ctx,
                       const BML::CommandEvent &in event) {
        string action = "summary";
        if (event.ArgCount > 0) {
            action = event.GetArg(0);
        }

        if (action == "summary") {
            LogSummary(ctx, "command");
            ctx.SendIngameMessage("Physicalize summary written to log.");
            return;
        }

        if (action == "reset") {
            ResetStats();
            ctx.SendIngameMessage("Physicalize stats reset.");
            return;
        }

        LogWarn(ctx, "PhysEvent usage: ckphy [summary|reset]");
    }

    void CompletePhysCommand(const BML::ModContext &in ctx,
                             const BML::CommandEvent &in event,
                             BML::CommandCompletion &inout completions) {
        completions.Add("summary");
        completions.Add("reset");
    }

    private string DescribeEvent(const BML::PhysicalizeEvent &in event) {
        string text =
            "#" + eventCount +
            " name=" + event.TargetName +
            " id=" + event.TargetId +
            " fixed=" + BoolText(event.Fixed) +
            " collision=" + BoolText(event.EnableCollision) +
            " friction=" + event.Friction +
            " elasticity=" + event.Elasticity +
            " mass=" + event.Mass +
            " group=" + event.CollisionGroup +
            " surface=" + event.CollisionSurface +
            " shapes=" + ShapeText(event);

        if (event.BallCount > 0) {
            text += " ball0.radius=" + event.GetBallRadius(0);
        }

        return text;
    }

    private string ShapeText(const BML::PhysicalizeEvent &in event) {
        return "balls=" + event.BallCount +
            " convex=" + event.ConvexCount +
            " concave=" + event.ConcaveCount;
    }

    private void LogSummary(const BML::ModContext &in ctx, const string &in reason) {
        LogInfo(ctx,
                "PhysEvent summary reason=" + reason +
                " events=" + eventCount +
                " borrowedTargets=" + borrowedTargets +
                " ballShapes=" + ballShapes +
                " convexShapes=" + convexShapes +
                " concaveShapes=" + concaveShapes +
                " woodFound=" + BoolText(woodFound) +
                " stoneFound=" + BoolText(stoneFound));
        LogInfo(ctx, "PhysEvent woodLine " + woodLine);
        LogInfo(ctx, "PhysEvent stoneLine " + stoneLine);
    }

    private void ResetStats() {
        eventCount = 0;
        borrowedTargets = 0;
        ballShapes = 0;
        convexShapes = 0;
        concaveShapes = 0;
        detailLogs = 0;
        woodFound = false;
        stoneFound = false;
        woodLine = "not found";
        stoneLine = "not found";
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

`OnPhysicalize` 做三件事。

第一，计数：

```angelscript
eventCount++;
ballShapes += event.BallCount;
convexShapes += event.ConvexCount;
concaveShapes += event.ConcaveCount;
```

第二，把事件转成一行日志：

```angelscript
string line = DescribeEvent(event);
```

第三，识别木球和石球：

```angelscript
if (!woodFound && event.CollisionSurface == "P_Ball_Wood_Mesh") {
    woodFound = true;
    woodLine = line;
}
```

这里用 `CollisionSurface` 判断，因为运行时对象名可能带后缀，表里的碰撞面名字更稳定。

## 运行结果

启动 Player，进入第一关。日志里能看到脚本加载：

```text
Loading Mod physevent.script[Physicalize Event] v1.0.0 by Tutorial
PhysEvent command registered=true
PhysEventMod loaded
BML script mod summary: loaded=1 failed=0
```

进入关卡后，会看到摘要：

```text
PhysEvent summary reason=level-start events=132 borrowedTargets=132 ballShapes=1 convexShapes=102 concaveShapes=40 woodFound=true stoneFound=false
```

不同启动状态下数字可能不同。不要记数字，先看这几件事：

```text
events 有没有增加
borrowedTargets 是否接近 events
woodFound 是否为 true
woodLine 里的 friction / elasticity / mass / radius 是否和表值一致
```

木球记录类似这样：

```text
PhysEvent wood #57 name=P_Ball_Wood_MF ... friction=0.6 elasticity=0.2 mass=2 ... surface=P_Ball_Wood_Mesh shapes=balls=1 convex=0 concave=0 ball0.radius=2
```

这就和第 22 章对上了：

```text
P_Ball_Wood: friction=0.6 elasticity=0.2 mass=2 radius=2
```

## 本章边界

`OnPhysicalize` 收到事件，只说明某个对象正在被物理化。本章不在这个回调里改对象。

本章也不做“生成一个能玩、能碰撞、能进入原版流程的物体”。那需要处理更多内容，例如 `PH` 表、`DepthTest` 组、关卡重置清理。下一章再拆开讲。

## 这一章先记住

`Physicalize` 是对象接入物理系统的动作。

`OnPhysicalize` 是脚本观察这个动作的入口。

`PhysicalizeEvent` 是事件快照，适合保存数字和字符串。

日志比 UI 更适合记录这类事件。

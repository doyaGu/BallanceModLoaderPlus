# 第 19 章：查找对象和组

现在开始写运行时观察脚本。

前面已经把心智模型铺好：

```text
对象      具体东西
组        对象名单
DataArray 运行时表
行为图    原版流程
```

这仍然是 BML 脚本 mod。区别是：前面主要使用日志、命令、配置和 Timer；这一章开始通过 CKAS/CK 绑定接触 Virtools 运行时对象。

这一章只做第一步：

```text
按名字查找组和 3D 实体
记录 name / id / class id
找不到时清楚显示
```

它不修改对象，也不保存长期句柄。

## 本章用一个独立脚本

新建：

```text
ModLoader/Mods/FindMod.mod.as
```

写入：

```angelscript
[bml.mod id="find.script" name="Find Mod" version="1.0.0" author="Tutorial" bml="0.3.12" description="Find groups and 3D entities by name"]
class FindMod {
    private BML::CommandRef@ commandRef;

    void OnLoad(const BML::ModContext &in ctx) {
        RegisterCommand(ctx);
        LogInfo(ctx, "FindMod loaded");
        ctx.SendIngameMessage("FindMod loaded. Use ckfind scan after entering a level.");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_START_LEVEL) {
            ScanLevel(ctx, "level-start");
        }
    }

    void OnUnload(const BML::ModContext &in ctx) {
        LogInfo(ctx, "FindMod unloaded");
    }

    private void RegisterCommand(const BML::ModContext &in ctx) {
        BML::CommandDefinition def;
        def.Name = "ckfind";
        def.Description = "Find tutorial CK objects";
        def.Usage = "ckfind scan";
        def.Category = "Tutorial";
        def.Enabled = true;

        BML::CommandCallback@ execute = BML::CommandCallback(this.OnFindCommand);
        BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.CompleteFindCommand);

        @commandRef = ctx.RegisterCommand(def, execute, complete);
        bool valid = commandRef !is null && commandRef.IsValid;
        LogInfo(ctx, "FindMod command registered=" + BoolText(valid));
    }

    void OnFindCommand(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        string action = "scan";
        if (event.ArgCount > 0) {
            action = event.GetArg(0);
        }

        if (action == "scan") {
            ScanLevel(ctx, "command");
            return;
        }

        ctx.SendIngameMessage("Usage: ckfind scan");
        LogWarn(ctx, "FindMod unknown command action: " + action);
    }

    void CompleteFindCommand(const BML::ModContext &in ctx,
                             const BML::CommandEvent &in event,
                             BML::CommandCompletion &inout completions) {
        completions.Add("scan");
    }

    private void ScanLevel(const BML::ModContext &in ctx, const string &in reason) {
        LogInfo(ctx, "FindMod scan begin reason=" + reason +
                     " inLevel=" + BoolText(ctx.GetIsInLevel()));

        // 组是一份对象名单，用 BorrowGroupByName 查。
        DescribeGroup(ctx, "P_Extra_Point");
        DescribeGroup(ctx, "PC_Checkpoints");
        DescribeGroup(ctx, "PR_Resetpoints");
        DescribeGroup(ctx, "PE_Levelende");
        DescribeGroup(ctx, "PS_Levelstart");
        DescribeGroup(ctx, "Sector_01");

        // 运行时可见的检查点相关实体。不要假设作者标记名一定就是运行时 3D 实体名。
        Describe3dEntity(ctx, "PC_TwoFlames_Flame_Big");
        Describe3dEntity(ctx, "PC_TwoFlames_MF");
        Describe3dEntity(ctx, "PC_TwoFlames_Flame_SmallA");
        Describe3dEntity(ctx, "PC_TwoFlames_Flame_SmallB");

        // 故意查一个不存在的名字，确认 null 分支安全。
        DescribeGroup(ctx, "FindMod_Missing_Group");
        Describe3dEntity(ctx, "FindMod_Missing_Object");

        LogInfo(ctx, "FindMod scan end");
        ctx.SendIngameMessage("FindMod scan finished. Check ModLoader log.");
    }

    private void DescribeGroup(const BML::ModContext &in ctx, const string &in name) {
        CKGroup@ group = ctx.BorrowGroupByName(name);
        if (group is null) {
            LogWarn(ctx, "FindMod group not found: " + name);
            return;
        }

        LogInfo(ctx, "FindMod group " + DescribeObject(group));
    }

    private void Describe3dEntity(const BML::ModContext &in ctx, const string &in name) {
        CK3dEntity@ entity = ctx.Borrow3dEntityByName(name);
        if (entity is null) {
            LogWarn(ctx, "FindMod entity not found: " + name);
            return;
        }

        LogInfo(ctx, "FindMod entity " + DescribeObject(entity));
    }

    private string DescribeObject(CKObject@ object) {
        string name = BML::CK::GetName(object);
        int id = BML::CK::GetId(object);
        int classId = BML::CK::GetClassId(object);
        return name + ": id=" + id + " class=" + classId;
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

    private string BoolText(bool value) {
        if (value) {
            return "true";
        }
        return "false";
    }
}
```

## 这份脚本做了什么

它有两个入口：

```text
进入关卡时
  OnGameEvent 收到 GAME_EVENT_START_LEVEL
  自动 ScanLevel

手动命令
  ckfind scan
  手动 ScanLevel
```

扫描时分两类查：

```text
组
  BorrowGroupByName

3D 实体
  Borrow3dEntityByName
```

两类查找都可能失败。

所以脚本先判断 `null`：

```angelscript
if (group is null) {
    LogWarn(ctx, "FindMod group not found: " + name);
    return;
}
```

`CK3dEntity@` 也一样：

```angelscript
if (entity is null) {
    LogWarn(ctx, "FindMod entity not found: " + name);
    return;
}
```

不做这个判断，后面访问空句柄会出错。

## 为什么组和实体分开查

`PC_Checkpoints` 是组。

应该这样查：

```angelscript
CKGroup@ group = ctx.BorrowGroupByName("PC_Checkpoints");
```

`PC_TwoFlames_Flame_Big` 是 3D 实体。

应该这样查：

```angelscript
CK3dEntity@ entity = ctx.Borrow3dEntityByName("PC_TwoFlames_Flame_Big");
```

如果把组名拿去查 3D 实体，通常会得到 `null`。  
如果把 3D 实体名拿去查组，也通常会得到 `null`。

找不到时，先检查三件事：

```text
时机是不是已经进入关卡
名字有没有写对
查找入口是不是对应正确类型
```

## 作者内容名和运行时实体名

第 18 章讲过：

```text
PC_TwoFlames_01
PC_TwoFlames_02
PC_TwoFlames_03
```

这些是第一关内容里检查点标记的名字。

在 Player 运行时，Levelinit 会把作者内容整理成运行时对象和表。  
直接用 `Borrow3dEntityByName("PC_TwoFlames_01")` 不一定能拿到 3D 实体。

本章查的是已经实测更适合运行时观察的名字：

```text
PC_TwoFlames_Flame_Big
PC_TwoFlames_MF
PC_TwoFlames_Flame_SmallA
PC_TwoFlames_Flame_SmallB
```

第 18 章的名字仍然有用。它们处在不同层：

| 名字 | 更接近哪一层 |
| --- | --- |
| `PC_TwoFlames_01` | 关卡作者内容里的检查点标记 |
| `PC_Checkpoints` | 检查点对象名单 |
| `PC_TwoFlames_Flame_Big` | Player 运行时可查的检查点相关 3D 实体 |

写脚本时要以运行时实际能查到的对象为准。

## 读取 name / id / class

找到对象以后，本章只读三项：

```angelscript
string name = BML::CK::GetName(object);
int id = BML::CK::GetId(object);
int classId = BML::CK::GetClassId(object);
```

它们对应第 13 章的概念：

| 字段 | 含义 |
| --- | --- |
| `name` | 对象名 |
| `id` | 本次运行里的对象编号 |
| `classId` | 类型编号 |

常见结果：

```text
class=23  CKGroup
class=33  CK3dEntity
```

`id` 只适合日志和临时确认。  
不要把它写死进脚本逻辑。

## 不长期保存 raw 句柄

本章的 `CKGroup@` 和 `CK3dEntity@` 都只在函数内部使用。

例如：

```angelscript
private void Describe3dEntity(const BML::ModContext &in ctx, const string &in name) {
    CK3dEntity@ entity = ctx.Borrow3dEntityByName(name);
    ...
}
```

函数结束后，脚本不把 `entity` 存到成员变量里。

原因是关卡对象会随关卡加载、退出、重置而变化。  
更稳的做法是保存名字或显示文本，下次需要时重新查。

## 运行

保存脚本，启动 Player。

在开始菜单阶段，脚本会加载，并注册命令：

```text
FindMod command registered=true
FindMod loaded
```

进入第一关后，脚本会在 `GAME_EVENT_START_LEVEL` 自动扫描。

也可以打开 BML 命令栏，输入：

```text
ckfind scan
```

如果已经进入关卡，日志里应该能看到下面这种结果：

```text
FindMod scan begin reason=level-start inLevel=true
FindMod group P_Extra_Point: id=... class=23
FindMod group PC_Checkpoints: id=... class=23
FindMod group PR_Resetpoints: id=... class=23
FindMod group PE_Levelende: id=... class=23
FindMod group PS_Levelstart: id=... class=23
FindMod group Sector_01: id=... class=23
FindMod entity PC_TwoFlames_Flame_Big: id=... class=33
FindMod entity PC_TwoFlames_MF: id=... class=33
FindMod entity PC_TwoFlames_Flame_SmallA: id=... class=33
FindMod entity PC_TwoFlames_Flame_SmallB: id=... class=33
FindMod group not found: FindMod_Missing_Group
FindMod entity not found: FindMod_Missing_Object
FindMod scan end
```

如果还在菜单阶段手动执行 `ckfind scan`，很多关卡对象会显示 `not found`。  
这通常说明关卡内容还没加载，不代表脚本坏了。

## 本章结果

现在脚本已经能做最基础的运行时对象观察：

```text
按名字找组
按名字找 3D 实体
处理 null
记录 name / id / class id
避免长期保存 raw CK 句柄
```

下一章读第一张 DataArray：`CurrentLevel`。

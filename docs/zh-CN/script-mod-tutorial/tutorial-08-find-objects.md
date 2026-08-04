# 08 查找对象

前面几章都在 BML 自带的 API 里操作：日志、消息、ImGui、命令、配置、Timer。这些功能和游戏内容本身无关，不管 Ballance 里有几个球、几块地板，上面那些代码都能工作。

从这一章开始，脚本进入游戏运行时。你将学会找到 Ballance 里的具体对象，那个木球、那片地板、那组检查点，并读取它们的信息。

---

## 游戏世界是怎么组织的

Ballance 使用 Virtools 引擎。Virtools 把游戏世界里的一切都用"对象"来表示，每个对象有类型、名字和运行时 ID。BML 通过 CKAS（CK AngelScript 绑定）把这些对象暴露给脚本。

你在游戏里看到的东西和这些类型的对应关系：

| 你在游戏里看到的 | CK 对象类型 | 举例 |
| --- | --- | --- |
| 球、地板、机关、变换器 | `CK3dEntity` | `P_Trafo_Wood_01`、`P_Extra_Point_01` |
| 一组相关对象的集合 | `CKGroup` | `PC_Checkpoints`（检查点）、`P_Extra_Point`（分数球） |
| 运行时数据表格 | `CKDataArray` | `CurrentLevel`（当前关卡信息）、`Energy`（分数） |

现在不需要记住所有类型。本章只用到前两个：`CK3dEntity` 和 `CKGroup`。

### Ballance 的命名约定

Ballance 的关卡文件里，对象名有固定的前缀规律：

| 前缀 | 含义 | 举例 |
| --- | --- | --- |
| `P_` | 物理实体（受物理引擎控制） | `P_Ball_Paper_01`、`P_Modul_01` |
| `PR_` | 重生点 | `PR_Resetpoint_01` |
| `PC_` | 检查点 | `PC_TwoFlames_01` |
| `PS_` | 起始标记 | `PS_Levelstart` |
| `P_Trafo_` | 变换器 | `P_Trafo_Stone_01` |
| `P_Extra_` | 拾取物 | `P_Extra_Point_01` |

知道命名规律后，你能大致判断一个名字指向什么东西。真正写脚本时，仍然以日志和组遍历结果为准。

---

## 为什么时机很重要

关卡对象的生命周期是：

```text
菜单阶段 -> 关卡加载 -> 关卡运行 -> 关卡退出 -> 回到菜单
```

关键点：**对象只在关卡运行期间存在**。在菜单阶段或 `OnLoad` 回调里查找关卡对象，结果一定是 `null`。原因很简单：对象还没有被创建出来。

正确的时机是监听 `OnGameEvent` 回调里的 `GAME_EVENT_START_LEVEL` 事件。BML 在关卡内容完全加载后才触发这个事件，此时所有对象已经就位。

---

## 查找函数

两个最常用的查找入口：

```angelscript
CK3dEntity@ entity = ctx.Borrow3dEntityByName("P_Extra_Point_01");
CKGroup@ group = ctx.BorrowGroupByName("PC_Checkpoints");
```

找不到时返回 `null`。这是正常情况（比如还没进关卡），不是错误。使用前必须判空。

注意类型和函数要匹配：

- 单个分数球是 `CK3dEntity`，用 `Borrow3dEntityByName` 查
- `PC_Checkpoints` 是 `CKGroup`，用 `BorrowGroupByName` 查
- 用错了函数（比如对组名用 `Borrow3dEntityByName`）会得到 `null`

---

## 完整示例

保存为 `ModLoader/Mods/FindDemo.mod.as`：

```angelscript
[bml.mod id="find.demo" name="Find Demo" version="1.0.0" author="Tutorial" bml="0.3.12" description="Find objects demo"]
class FindDemo {
    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_START_LEVEL) {
            FindBall(ctx);
            FindGroup(ctx);
        }
    }

    private void FindBall(const BML::ModContext &in ctx) {
        CK3dEntity@ ball = BorrowActiveBall(ctx);
        if (ball is null) {
            LogInfo(ctx, "ActiveBall not found");
            return;
        }

        string name = BML::CK::GetName(ball);
        int id = BML::CK::GetId(ball);
        LogInfo(ctx, "ActiveBall: " + name + " id=" + id);
    }

    private CK3dEntity@ BorrowActiveBall(const BML::ModContext &in ctx) {
        CKDataArray@ currentLevel = ctx.BorrowDataArrayByName("CurrentLevel");
        if (currentLevel is null) return null;

        int col = BML::CK::FindColumn(currentLevel, "ActiveBall");
        if (col < 0) return null;

        CKObject@ object = currentLevel.GetElementObject(0, col);
        return cast<CK3dEntity>(object);
    }

    private void FindGroup(const BML::ModContext &in ctx) {
        CKGroup@ checkpoints = ctx.BorrowGroupByName("PC_Checkpoints");
        if (checkpoints is null) {
            LogInfo(ctx, "PC_Checkpoints not found");
            return;
        }

        int count = checkpoints.GetObjectCount();
        LogInfo(ctx, "PC_Checkpoints has " + count + " members");

        for (int i = 0; i < count; i++) {
            CKBeObject@ member = checkpoints.GetObject(i);
            if (member !is null) {
                LogInfo(ctx, "  [" + i + "] " + BML::CK::GetName(member));
            }
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

### 代码逐段解读

**OnGameEvent**：BML 在关卡开始、退出、重生等事件发生时调用此回调。我们只关心 `GAME_EVENT_START_LEVEL`，此时关卡对象已经全部加载完毕。

**FindBall**：从 `CurrentLevel` 表里取 `ActiveBall` 列。Ballance 的玩家球会在木球、纸球、石球之间切换，实际对象名可能是 `Ball_Wood`、`Ball_Paper` 或 `Ball_Stone`。不要写死其中一个名字；运行时表里记录的才是当前正在控制的球。

**BorrowActiveBall**：`ctx.BorrowDataArrayByName("CurrentLevel")` 取得当前关卡状态表；`FindColumn(..., "ActiveBall")` 找到活动球所在列；`GetElementObject(0, col)` 读取这一格里的 CK 对象；最后用 `cast<CK3dEntity>` 转成 3D 实体。

**FindGroup**：`PC_Checkpoints` 是 Ballance 里包含检查点标记的组。`checkpoints.GetObjectCount()` 返回成员数量，`checkpoints.GetObject(i)` 按索引取出第 i 个成员。

---

## 读取对象信息

找到对象后，最基础的三项信息：

```angelscript
string name = BML::CK::GetName(object);    // 对象名
int id = BML::CK::GetId(object);           // 运行时编号
int classId = BML::CK::GetClassId(object); // 类型编号
```

关于 ID 的重要说明：`id` 每次运行都会变。同一个球在这次启动时 id 可能是 1234，下次启动可能是 5678。不要在逻辑里写死 ID。要定位对象，始终用名字。

常见 class id 对照：`23` = CKGroup，`33` = CK3dEntity。

---

## 遍历组成员

组（`CKGroup`）是 Ballance 里组织相关对象的方式。比如 `P_Extra_Point` 组包含关卡里所有的分数球，`PC_Checkpoints` 包含所有检查点。

遍历组用两个成员方法：

```angelscript
int count = group.GetObjectCount();
for (int i = 0; i < count; i++) {
    CKBeObject@ member = group.GetObject(i);
    if (member !is null) {
        // 使用 member
    }
}
```

`GetObjectCount()` 和 `GetObject(i)` 是 `CKGroup` 自身的成员方法（由 CKAS 绑定提供），直接在 group 对象上调用。这和读取名字用的 `BML::CK::GetName(obj)` 不同，后者是 BML 命名空间下的全局函数。

`GetObject(i)` 的返回类型是 `CKBeObject@`，这是 Virtools 对象体系中一个通用基类。如果你知道组里的成员都是 3D 实体，可以用 `cast<CK3dEntity>(member)` 转型：

```angelscript
CK3dEntity@ entity = cast<CK3dEntity>(member);
if (entity !is null) {
    VxVector pos = BML::CK::GetPosition(entity);
}
```

转型失败时返回 `null`，不会崩溃，所以仍然要判空。

---

## 运行结果

进入第一关后，`ModLoader.log` 会出现类似：

```text
[find.demo/INFO]: ActiveBall: Ball_Wood id=42648
[find.demo/INFO]: PC_Checkpoints has 3 members
[find.demo/INFO]:   [0] PC_Checkpoints
[find.demo/INFO]:   [1] PC_Checkpoint_01
[find.demo/INFO]:   [2] PC_Checkpoint_02
...
```

成员数量和名称取决于关卡。

如果看到 `not found`：

- 在菜单阶段看到是正常的，还没进关卡
- 进了关卡还是 `not found`：检查名字是否拼错，注意大小写敏感

---

## 不要长期保存句柄

本章示例里，对象引用只在函数内部使用，用完就丢弃，不存成成员变量。这是最安全的做法。

为什么不能随意保存？考虑这个场景：

1. 进入第 1 关，取得当前活动球，得到句柄，存成 `@ball`
2. 第 1 关结束，退出到菜单
3. 进入第 2 关
4. 脚本仍然持有第 1 关时保存的 `@ball`

此时 `@ball` 指向的对象已经随第 1 关卸载而被销毁。对一个已销毁的对象调用任何方法，行为未定义，可能返回垃圾值，可能崩溃。

安全规则：

- 函数内临时使用：查到就用，函数结束后自然释放，永远安全
- 存成成员变量：必须配合关卡事件管理生命周期（下一章会讲具体做法）

---

## 更多 Ballance 对象名称

常见对象名：

| 对象名 | 类型 | 含义 |
| --- | --- | --- |
| `CurrentLevel.ActiveBall` | CK3dEntity | 当前玩家球，需从 DataArray 取出 |
| `P_Ball_Paper_01` | CK3dEntity | 纸球对象名，是否存在取决于关卡和运行时状态 |
| `P_Ball_Wood_01` | CK3dEntity | 木球对象名，是否存在取决于关卡和运行时状态 |
| `P_Ball_Stone_01` | CK3dEntity | 石球对象名，是否存在取决于关卡和运行时状态 |
| `P_Trafo_Wood_01` | CK3dEntity | 木球变换器 |
| `P_Extra_Point_01` | CK3dEntity | 第一个分数球 |
| `P_Extra_Point` | CKGroup | 所有分数球 |
| `P_Extra_Life` | CKGroup | 所有生命球 |
| `PC_Checkpoints` | CKGroup | 所有检查点 |
| `PR_Resetpoints` | CKGroup | 所有重生点 |

---

## 常见问题诊断

| 现象 | 原因 | 解决 |
| --- | --- | --- |
| 所有查找都返回 `null` | 在 `OnLoad` 或菜单阶段调用了查找 | 把查找代码移到 `OnGameEvent` 的 `GAME_EVENT_START_LEVEL` 分支里 |
| 名字确认正确但仍然 `null` | 用错了查找函数（对组名用了 `Borrow3dEntityByName`） | 确认对象类型和函数匹配 |
| 编译报 `GetObjectCount` 不存在 | 变量类型声明错误，不是 `CKGroup@` | 检查变量声明 |
| 组遍历时部分 `member` 是 `null` | 正常现象，组里可能有空槽位 | 在循环里判空后再使用 |
| 名字大小写拼错 | Ballance 对象名大小写敏感 | 用日志遍历组成员，从输出里复制准确的名字 |

---

## 完成状态

脚本现在能在正确时机找到游戏世界里的对象，读取名字和 ID，遍历组成员列表。日志里打印出了对象名。

下一步要把查找到的对象信息实时显示在画面上，用前面学过的 ImGui 窗口，做一个坐标显示器。

下一步：[09 坐标显示器 Mod](tutorial-09-coordinate-mod.md)

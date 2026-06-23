# 09 坐标显示器 Mod

前面 8 章积累了一系列基础能力：日志、ImGui 窗口、回调、对象查找。现在把它们组合起来，做你的第一个实用 mod：一个在游戏画面上实时显示玩家球坐标的小工具。

这种 mod 在 Ballance 自制地图测试时很有用。地图作者需要知道某个机关放在什么位置、球从哪里掉下去、变换器的触发范围有多大。坐标显示器能直接告诉你答案。

---

## 设计思路

做这个 mod 之前，先想清楚几个问题：

**什么时候找球？** 进入关卡时找一次，把句柄存起来。不要每帧都找，查找有性能开销。

**什么时候清空？** 退出关卡时清空句柄。上一章讲过，退出关卡后对象被销毁，旧句柄不能继续使用。

**怎么显示？** 每帧用 `OnProcess` 读取球的当前位置，用 ImGui 窗口画出来。

**怎么切换？** 用按键（F10）控制窗口显示和隐藏。

这四个问题分别对应前面学过的：`OnGameEvent` 关卡事件、成员变量缓存、`OnProcess` + ImGui、`BorrowInputManager` 输入检测。

---

## VxVector 和 Ballance 坐标系

读取 3D 实体的位置会得到一个 `VxVector`，它有三个分量：

```text
VxVector {
    float x;   // 左右方向
    float y;   // 上下方向（高度）
    float z;   // 前后方向
}
```

Ballance 使用的坐标系是：

- **Y 轴朝上**，Y 值越大，位置越高
- **X 和 Z** 是水平面的两个方向

在第 1 关起点，球的大致坐标约为：

```text
X: 大约 -5 到 5
Y: 大约 15 到 20（起点平台高度）
Z: 大约 0 到 10
```

掉落时 Y 值快速减小。到达终点时 X/Z 会有较大变化。你可以通过观察这些数值的变化来验证 mod 是否正常工作。

---

## 完整脚本

保存为 `ModLoader/Mods/CoordMod.mod.as`：

```angelscript
[bml.mod id="coord.script" name="Coordinate Display" version="1.0.0" author="Tutorial" bml="0.3.12" description="Shows player ball position"]
class CoordMod {
    private CK3dEntity@ ball = null;
    private bool showWindow = true;

    void OnLoad(const BML::ModContext &in ctx) {
        LogInfo(ctx, "CoordMod loaded");
        ctx.SendIngameMessage("CoordMod loaded. Enter a level to see coordinates.");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_START_LEVEL) {
            @ball = BorrowActiveBall(ctx);
            if (ball !is null) {
                LogInfo(ctx, "ActiveBall found: " + BML::CK::GetName(ball));
            } else {
                LogInfo(ctx, "ActiveBall not found at level start");
            }
        } else if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL) {
            @ball = null;
        }
    }

    void OnProcess(const BML::ModContext &in ctx) {
        HandleInput(ctx);

        if (!showWindow || ball is null) return;

        ImGui::SetNextWindowPos(ImVec2(10.0f, 60.0f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(280.0f, 0.0f), ImGuiCond_Once);

        if (ImGui::Begin("Coordinates")) {
            VxVector pos = BML::CK::GetPosition(ball);
            ImGui::TextUnformatted("X: " + pos.x);
            ImGui::TextUnformatted("Y: " + pos.y);
            ImGui::TextUnformatted("Z: " + pos.z);
        }
        ImGui::End();
    }

    private void HandleInput(const BML::ModContext &in ctx) {
        BML::InputHook@ input = ctx.BorrowInputManager();
        if (input !is null && input.IsKeyPressed(CKKEY_F10)) {
            showWindow = !showWindow;
        }
    }

    private CK3dEntity@ BorrowActiveBall(const BML::ModContext &in ctx) {
        CKDataArray@ currentLevel = ctx.BorrowDataArrayByName("CurrentLevel");
        if (currentLevel is null) return null;

        int col = BML::CK::FindColumn(currentLevel, "ActiveBall");
        if (col < 0) return null;

        CKObject@ object = currentLevel.GetElementObject(0, col);
        return cast<CK3dEntity>(object);
    }

    private void LogInfo(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info(message);
        }
    }
}
```

---

## 代码逐段解析

### 句柄缓存模式

```angelscript
private CK3dEntity@ ball = null;
```

成员变量保存球的引用。和上一章"用完即弃"不同，这里有意把句柄存起来，因为 `OnProcess` 每帧都要读取位置，每帧都重新查找太浪费了。

但存句柄有规则：

```angelscript
void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
    if (event == BML::GAME_EVENT_START_LEVEL) {
        @ball = BorrowActiveBall(ctx);
        // ...
    } else if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL) {
        @ball = null;
    }
}
```

- **进入关卡时赋值**：此时对象刚创建，句柄有效
- **退出关卡时清空**：此时对象即将销毁，必须把旧句柄丢掉

这就是"缓存句柄的正确方式"，用关卡事件管理生命周期。如果漏掉了 `@ball = null` 那一行，退出关卡后 `OnProcess` 仍然会尝试对已销毁的对象调用 `GetPosition`，结果不可预测。

### 为什么从 CurrentLevel 取球

Ballance 运行时会在 `CurrentLevel` 表的 `ActiveBall` 列记录当前玩家正在控制的球。第一关开始时通常是 `Ball_Wood`；过变球器之后，可能变成 `Ball_Paper` 或 `Ball_Stone`。

所以坐标显示器不写死球名，而是每次进入关卡时从 `CurrentLevel.ActiveBall` 取对象。这样无论当前是木球、纸球还是石球，`ball` 都指向实际受玩家控制的实体。

`GetElementObject(0, col)` 是 CKAS 暴露的 `CKDataArray` 方法。它直接读取表格单元格里的 CK 对象；`cast<CK3dEntity>` 把通用对象转成可以读取位置的 3D 实体。

### 每帧读取位置

```angelscript
void OnProcess(const BML::ModContext &in ctx) {
    HandleInput(ctx);

    if (!showWindow || ball is null) return;

    ImGui::SetNextWindowPos(ImVec2(10.0f, 60.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(280.0f, 0.0f), ImGuiCond_Once);

    if (ImGui::Begin("Coordinates")) {
        VxVector pos = BML::CK::GetPosition(ball);
        ImGui::TextUnformatted("X: " + pos.x);
        ImGui::TextUnformatted("Y: " + pos.y);
        ImGui::TextUnformatted("Z: " + pos.z);
    }
    ImGui::End();
}
```

流程：

1. 先处理输入（F10 按键检测）
2. 如果窗口被隐藏或者球还没找到，直接返回，不绘制窗口也不读位置
3. 设置窗口初始位置（`ImGuiCond_Once` 表示只在第一次生效，之后用户可以拖动）
4. `BML::CK::GetPosition(ball)` 返回球此刻的世界坐标
5. 用 `ImGui::TextUnformatted` 逐行显示三个分量

`GetPosition` 是 `BML::CK` 命名空间下的全局函数，不是 `ball` 的成员方法。这和上一章的 `group.GetObjectCount()` 风格不同，前者是 BML 包装的工具函数，后者是 CKAS 直接暴露的 Virtools 对象方法。

### 按键切换

```angelscript
private void HandleInput(const BML::ModContext &in ctx) {
    BML::InputHook@ input = ctx.BorrowInputManager();
    if (input !is null && input.IsKeyPressed(CKKEY_F10)) {
        showWindow = !showWindow;
    }
}
```

`IsKeyPressed` 只在按键按下的那一帧返回 `true`，后续帧即使按住不放也不会再触发。所以每次按 F10 只会切换一次，不会出现按一下连续切换多次的问题。

---

## 运行验证

### 预期结果

1. 启动 Player，日志出现 `CoordMod loaded`
2. 游戏内消息栏显示 "CoordMod loaded. Enter a level to see coordinates."
3. 进入第 1 关（选纸球），画面左上角出现 Coordinates 窗口：

```text
X: -2.34567
Y: 18.12345
Z: 3.78901
```

4. 推动球，三个数值实时变化。球向右移动时 X 变大，球下落时 Y 减小
5. 按 F10，窗口消失。再按 F10，窗口重新出现
6. 退出到菜单，窗口自动消失（因为 `ball` 被清空为 `null`）
7. 重新进入关卡，窗口重新出现（因为球被重新找到）

### 具体数值参考

在第 1 关起始平台上静止不动时，你看到的坐标大致在这个范围：

- X: -10 到 10
- Y: 12 到 25
- Z: -5 到 15

如果数值完全不动或者显示异常的极大/极小值，说明句柄可能有问题。

---

## 常见问题诊断

| 现象 | 原因 | 解决 |
| --- | --- | --- |
| 窗口完全不出现 | 多种可能 | 先检查日志有没有 `CoordMod loaded`。没有则 mod 文件名或路径不对 |
| 日志有 `loaded` 但窗口不出现 | `ball is null`，没进关卡或活动球没有取到 | 检查日志有没有 `ActiveBall found`。如果是 `ActiveBall not found`，确认 `CurrentLevel` 已经存在 |
| 窗口出现但坐标不动 | 可能拿到的不是玩家球 | 确认查找名是 `Ball`，并且进入了正在游玩的关卡 |
| 进入第二关后崩溃 | 漏掉了退出时清空句柄 | 确认 `GAME_EVENT_PRE_EXIT_LEVEL` 分支里有 `@ball = null` |
| 编译报 `VxVector` 未定义 | 极少见，一般是 BML 版本太旧 | 确认 bml 版本属性写的是 `0.3.12` 或更高 |
| F10 按了没反应 | `input is null` | 确认 `BorrowInputManager()` 调用存在，且判空逻辑正确 |

---

## 和前面章节的联系

这个 mod 里用到的每个概念都来自前面的章节：

| 用到的能力 | 来自章节 |
| --- | --- |
| `OnLoad` 初始化 | 02 - Hello Mod |
| ImGui 窗口 | 04 - ImGui 窗口 |
| `OnProcess` 每帧更新 | 03 - 输出与输入 |
| `OnGameEvent` 关卡事件 | 06 - 回调模型 |
| 按键检测 | 03 - 输出与输入 |
| 对象查找和句柄 | 08 - 查找对象 |

如果某个部分看不懂，可以回到对应章节复习。

---

## 下一步的改进方向

当前版本有几个局限：

- 坐标精度太高，小数点后五六位其实没有意义，实际使用时截取两位更易读
- 没有配置，无法调整显示位置或切换按键

这些改进可以用前面学过的知识（Config、条件判断）继续做。后面的章节会介绍 DataArray，到时可以把当前关卡号、分数、球型等状态一起显示出来。

---

**完成状态**：第一个实用 mod 完成。画面上能看到实时更新的坐标数值。

---

下一步：[10 Ballance 内部结构](tutorial-10-ballance-internals.md)

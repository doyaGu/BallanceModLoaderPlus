# 09 坐标显示器 Mod

前面 8 章积累了一系列基础能力：日志、ImGui 窗口、回调、对象查找。现在把它们组合起来，做你的第一个实用 mod：一个在游戏画面上实时显示玩家球坐标的小工具。

这种 mod 在 Ballance 自制地图测试时很有用。地图作者需要知道某个机关放在什么位置、球从哪里掉下去、变换器的触发范围有多大。坐标显示器能直接告诉你答案。

---

## 设计思路

做这个 mod 之前，先想清楚几个问题：

**什么时候找球？** 进入关卡时缓存 `CurrentLevel` 表和 `ActiveBall` 列号；每帧
直接读取该单元格。这样没有按名搜索开销，变球后也能立即取得新的活动球。

**什么时候清空？** 退出关卡时清空表句柄和列号。活动球句柄只在当前
`OnProcess` 调用中临时使用，不保存到成员变量。

**怎么显示？** 每帧用 `OnProcess` 读取球的当前位置，用 ImGui 窗口画出来。

**怎么切换？** 用按键（F10）控制窗口显示和隐藏。

这四个问题分别对应前面学过的：`OnGameEvent` 关卡事件、稳定数据源缓存、
`OnProcess` + ImGui、`BorrowInputManager` 输入检测。

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
[bml.mod id="coord.script" name="Coordinate Display" version="1.0.0" author="Tutorial" bml="0.3.13" description="Shows player ball position"]
class CoordMod {
    private CKDataArray@ currentLevel = null;
    private int activeBallColumn = -1;
    private bool showWindow = true;

    void OnLoad(const BML::ModContext &in ctx) {
        ctx.LogInfo("CoordMod loaded");
        BML::UI::AddMessage("CoordMod loaded. Enter a level to see coordinates.");
    }

    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_START_LEVEL) {
            @currentLevel = ctx.BorrowDataArrayByName("CurrentLevel");
            activeBallColumn = currentLevel is null
                ? -1
                : BML::CK::FindColumn(currentLevel, "ActiveBall");

            CK3dEntity@ ball = BorrowActiveBall();
            if (ball !is null) {
                ctx.LogInfo("ActiveBall ready: " + BML::CK::GetName(ball));
            } else {
                ctx.LogInfo("CurrentLevel.ActiveBall is unavailable");
            }
        } else if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL) {
            @currentLevel = null;
            activeBallColumn = -1;
        }
    }

    void OnProcess(const BML::ModContext &in ctx) {
        HandleInput(ctx);

        if (!showWindow) return;

        CK3dEntity@ ball = BorrowActiveBall();
        if (ball is null) return;

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

    private CK3dEntity@ BorrowActiveBall() {
        if (currentLevel is null || activeBallColumn < 0) return null;
        CKObject@ object = currentLevel.GetElementObject(0, activeBallColumn);
        return cast<CK3dEntity>(object);
    }

}
```

---

## 代码逐段解析

### 缓存稳定的数据源

```angelscript
private CKDataArray@ currentLevel = null;
private int activeBallColumn = -1;
```

成员变量保存的是关卡内稳定的 `CurrentLevel` 表和列号，不是活动球本身。按名查表
和按列名查索引只在进入关卡时做一次；`OnProcess` 只读取已知单元格。

生命周期仍由关卡事件管理：

```angelscript
void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
    if (event == BML::GAME_EVENT_START_LEVEL) {
        @currentLevel = ctx.BorrowDataArrayByName("CurrentLevel");
        activeBallColumn = currentLevel is null
            ? -1
            : BML::CK::FindColumn(currentLevel, "ActiveBall");
    } else if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL) {
        @currentLevel = null;
        activeBallColumn = -1;
    }
}
```

- **进入关卡时赋值**：表已经建立，可以解析列号。
- **退出关卡前清空**：表即将失效，不能带到菜单或下一关。

活动球可能在同一关中因变球器或重生而更换，所以不能把 `CK3dEntity@` 长期保存。
每帧从缓存表读取一次对象指针，既能跟随 `ActiveBall` 变化，也把借用句柄限制在
当前回调内。

### 为什么从 CurrentLevel 取球

Ballance 运行时会在 `CurrentLevel` 表的 `ActiveBall` 列记录当前玩家正在控制的球。第一关开始时通常是 `Ball_Wood`；过变球器之后，可能变成 `Ball_Paper` 或 `Ball_Stone`。

所以坐标显示器不写死球名，而是在每帧显示前从 `CurrentLevel.ActiveBall` 取对象。
无论当前是木球、纸球还是石球，局部变量 `ball` 都对应当前受玩家控制的实体。

`GetElementObject(0, col)` 是 CKAS 暴露的 `CKDataArray` 方法。它直接读取表格单元格里的 CK 对象；`cast<CK3dEntity>` 把通用对象转成可以读取位置的 3D 实体。

### 每帧读取位置

```angelscript
void OnProcess(const BML::ModContext &in ctx) {
    HandleInput(ctx);

    if (!showWindow) return;

    CK3dEntity@ ball = BorrowActiveBall();
    if (ball is null) return;

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
2. 窗口隐藏时直接返回。
3. 从缓存表读取当前活动球；单元格无有效对象时直接返回。
4. 设置窗口初始位置（`ImGuiCond_Once` 表示只在第一次生效，之后用户可以拖动）。
5. `BML::CK::GetPosition(ball)` 返回球此刻的世界坐标。
6. 用 `ImGui::TextUnformatted` 逐行显示三个分量。

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
6. 通过变球器后，窗口继续显示新活动球的坐标。
7. 退出到菜单，窗口自动消失（因为 `currentLevel` 被清空）。
8. 重新进入关卡，窗口重新出现（因为表和列号被重新解析）。

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
| 日志有 `loaded` 但窗口不出现 | 没进关卡，或 `CurrentLevel.ActiveBall` 尚不可用 | 检查日志中的 `CurrentLevel.ActiveBall is unavailable`，并确认已进入正在游玩的关卡 |
| 变球后坐标不更新 | 错误地把活动球保存成了成员变量 | 保持 `ball` 为 `OnProcess` 内的局部借用句柄，每帧重新读取单元格 |
| 进入第二关后崩溃 | 漏掉了退出关卡前的清理 | 确认 `GAME_EVENT_PRE_EXIT_LEVEL` 中清空 `currentLevel` 并重置列号 |
| 编译报 `VxVector` 未定义 | 极少见，一般是 BML 版本太旧 | 确认 bml 版本属性写的是 `0.3.13` 或更高 |
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
| 对象查找和借用句柄 | 08 - 查找对象 |

如果某个部分看不懂，可以回到对应章节复习。

---

## 下一步的改进方向

当前版本有几个局限：

- 坐标精度太高，小数点后五六位其实没有意义，实际使用时截取两位更易读
- 没有配置，无法调整显示位置或切换按键

这些改进可以用前面学过的知识（Config、条件判断）继续做。本章只把
`CurrentLevel` 当作取得活动球的数据源；第 11 章会系统介绍 DataArray，届时可以
把当前关卡号、分数、球型等状态一起显示出来。

---

**完成状态**：第一个实用 mod 完成。画面上能看到实时更新的坐标数值。

---

下一步：[10 Ballance 运行结构](tutorial-10-ballance-internals.md)

# 22 排错与调试参考

这是你的调试手册。脚本不按预期工作时，按症状查找对应的解决方案。

排错核心原则：**先看日志，再改代码**。日志会告诉你问题出在哪一层。跳过日志直接猜测是最常见的时间浪费。

---

## 通用调试方法论

按以下步骤逐层缩小范围。前一层不成立时，后一层的表现没有参考意义。

1. **打开日志**：`ModLoader/ModLoader.log`
2. **在游戏内看结构化日志**：用 `script logs error` 或 `script panel`
3. **确认脚本被发现**：搜索你的 mod id，找 `Loading Mod` 行，或用 `script list`
4. **确认编译通过**：紧跟其后有没有 ERROR 行，或用 `script diag <id>`
5. **确认 OnLoad 执行**：OnLoad 第一行放日志输出
6. **确认 OnProcess 在跑**：加一次性日志（用 bool 标记只打一次）
7. **在出问题的位置前后加日志**：入口出现但出口没有，问题在两行之间
8. **一次只改一个变量**：不要同时改文件名、代码、命令名和对象名

确认 OnProcess 的一次性日志写法：

```angelscript
private bool processLogged = false;

void OnProcess(const BML::ModContext &in ctx) {
    if (!processLogged) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info("OnProcess first frame");
        }
        processLogged = true;
    }
    // ... 其余代码
}
```

如果日志里出现了 `OnProcess first frame`，说明每帧逻辑在正常运行，问题在 OnProcess 内部的某个分支里。

---

## 理解日志文件

**位置**：`ModLoader/ModLoader.log`（游戏根目录下 ModLoader 文件夹内）

每行格式：`[来源/级别]: 消息内容`

| 来源 | 含义 |
| --- | --- |
| `ModLoader` | BML 自身 |
| `hello.script` | 你的 mod（等于 id 字段的值） |

| 级别 | 含义 |
| --- | --- |
| `INFO` | 正常信息 |
| `WARN` | 警告，功能可能受影响 |
| `ERROR` | 错误，某操作已失败 |

**阅读顺序**：从文件末尾往上找最近的 `Loading Mod` 行；定位第一条 ERROR（后续 ERROR 常是连锁反应）；对比上次正常时的日志。

**一次正常加载的完整日志示例**：

```text
[ModLoader/INFO]: Loading Mod hello.script[Hello Mod] v1.0.0 by Tutorial
[hello.script/INFO]: HelloMod loaded from ModLoader/Mods/HelloMod.mod.as
[ModLoader/INFO]: BML script mod summary: loaded=1 failed=0
```

**一次编译失败的日志示例**：

```text
[ModLoader/INFO]: Loading Mod hello.script[Hello Mod] v1.0.0 by Tutorial
[ModLoader/ERROR]: Failed to compile script mod 'HelloMod.mod.as'
[ModLoader/ERROR]: (line 5, col 1): Expected ';'
[ModLoader/ERROR]: (line 8, col 12): 'logger' is not declared
[ModLoader/INFO]: BML script mod summary: loaded=0 failed=1
```

第二条及之后的 ERROR 可能是第一条的连锁结果。修复第一条后重启，后续错误往往自动消失。

---

## 游戏内脚本诊断命令

打开 BML 命令栏后可以用 `script` 命令查询脚本状态：

| 命令 | 用途 |
| --- | --- |
| `script status` | 查看 watcher、pending reload、loaded/failed 数量 |
| `script list` | 列出脚本 mod |
| `script info <id>` | 查看单个 mod 摘要 |
| `script diag <id>` | 查看最近诊断、编译错误、reload 失败原因 |
| `script logs` | 查看最近日志 |
| `script logs error` | 只看错误日志 |
| `script panel` | 打开/关闭 Script Developer Tools 面板 |
| `script reload` | 热重载全部脚本 mod |
| `script reload <id>` | 热重载单个脚本 mod |
| `script reload <id> --dry-run` | 只编译和验证，不替换 runtime |
| `script reload <id> --dry-run --check-state` | 额外执行状态迁移检查，不替换 runtime |
| `script watch on` / `script watch off` | 打开/关闭自动热重载 watcher |

命令返回 `queued` 时表示动作已排队，实际结果会在下一帧安全点完成。最终结果看 `script diag <id>`、`script logs error` 或面板的 Logs/Diag 页。

---

## 编译错误与运行时错误

| | 编译错误 | 运行时错误 |
| --- | --- | --- |
| 脚本是否加载 | 否 | 是 |
| 何时发生 | 启动时 | 执行过程中 |
| 日志特征 | `Failed to compile` + 行号 | `Null pointer access` 或异常行为 |
| 修复方式 | 修正语法/类型 | 添加判空或修正逻辑 |

编译错误示例：
```text
[ModLoader/ERROR]: Failed to compile script mod 'HelloMod.mod.as'
[ModLoader/ERROR]: (line 5, col 1): Expected ';'
```

运行时错误示例：
```text
[hello.script/INFO]: === OnLoad entered ===
... (进入关卡后)
[ModLoader/ERROR]: Null pointer access in script 'HelloMod.mod.as' at line 45
```

---

## 热重载失败

热重载失败时，游戏内消息通常只提示 reload failed，详细原因看 `script diag <id>` 和 `script logs error`。

常见情况：

- **编译失败**：旧 runtime 仍继续运行；修正脚本后再次 `script reload <id>`。
- **启动时首次编译失败**：BML 会保留 failed placeholder。修好文件后运行 `script reload` 或 `script reload <id>`，可以恢复到真实 mod id。
- **运行中新增 `.mod.as` 文件**：不会加载新 mod。BML 不在运行中新增 registry 节点，需要重启 Player。
- **改了 mod id 或 dependency 声明**：热重载会拒绝。依赖图变化需要重启。
- **删除或改签名旧 export**：默认拒绝。只有确认调用方能接受时才用 `--force-exports`。
- **状态迁移失败**：检查 `SaveState`、`MigrateState`、`RestoreState` 签名和日志。`--dry-run --check-state` 可以在不替换 runtime 的情况下验证迁移代码。

状态迁移方法只能搬运纯数据。不要在 `SaveState`、`MigrateState`、`RestoreState` 里注册命令/Timer、写 DataShare/config、执行命令、调用 export、或修改 CK/game-world 对象。BML 回滚只能恢复自己持有的脚本资源，不能撤销脚本已经改过的游戏世界状态。

---

## 常见 AngelScript 语法错误

### 缺少分号

```text
[ModLoader/ERROR]: (line 5, col 1): Expected ';'
```

看到 `Expected ';'` 时去看**报错行的上一行**末尾，编译器直到读到下一行才发现少了分号。

### 未声明的变量

```text
[ModLoader/ERROR]: (line 10, col 5): 'couner' is not declared
```

拼写错误或忘记声明。AngelScript 区分大小写。

### 句柄类型不匹配（缺少 @）

```text
[ModLoader/ERROR]: (line 8, col 12): Can't implicitly convert from 'CK3dEntity@' to 'CK3dEntity'
```

返回句柄类型的函数，接收变量也要加 `@`：`CK3dEntity@ entity = ...`

### 句柄判空写法错误

```text
[ModLoader/ERROR]: (line 12, col 16): No matching operator that takes the types 'CK3dEntity@' and 'const int'
```

句柄判空用 `is` / `!is`，不用 `==` / `!=`：`if (entity !is null)`

### 回调签名不匹配

```text
[ModLoader/WARN]: No matching signatures to 'OnLoad(const BML::ModContext&in)'
```

不会编译失败，但回调不会被调用。签名必须精确：`void OnLoad(const BML::ModContext &in ctx)`。缺少 `const`、缺少 `&in`、多余参数都会导致不匹配。

### &in 与 &inout 混用

```text
[ModLoader/ERROR]: (line 30, col 5): No matching signatures to 'BML::CommandCompletionCallback'
```

补全回调第三个参数必须是 `BML::CommandCompletion &inout`，不是 `&in`。

---

## 脚本没有加载

**日志表现**：找不到你的 mod id，加载摘要显示 `loaded=0 failed=0`。

```text
[ModLoader/INFO]: BML script mod summary: loaded=0 failed=0
```

**排查清单**：

- 文件必须在 `ModLoader/Mods/` 下（不是游戏根目录的 `Mods/`，也不是 `ModLoader/` 根目录）
- 扩展名必须是 `.mod.as`。常见错误：
  - `.as`（缺少 `.mod` 前缀）
  - `.mod.as.txt`（Windows 隐藏了真实扩展名）
- 在资源管理器中点「查看」，勾选「文件扩展名」，确认实际文件名
- 文件不能为空（0 字节文件会被跳过）
- 编码应为 UTF-8（记事本默认保存格式即可，不要选 UTF-16）

---

## OnLoad 没有执行

**日志表现**：有 `Loading Mod` 行，编译成功，但 OnLoad 内部的日志输出缺失。

```text
[ModLoader/INFO]: Loading Mod hello.script[Hello Mod] v1.0.0 by Tutorial
[ModLoader/INFO]: BML script mod summary: loaded=1 failed=0
```

（上面没有出现脚本应该打出的 Info 行。）

**排查清单**：

- 函数签名必须精确匹配：`void OnLoad(const BML::ModContext &in ctx)`
- 以下变体全部无效，BML 不会调用：
  - `void OnLoaded(...)`：函数名错
  - `void OnLoad()`：缺参数
  - `int OnLoad(...)`：返回值类型错
  - `void OnLoad(BML::ModContext &in ctx)`：缺 `const`
- `[bml.mod ...]` 元数据行必须在 class 定义之前
- 最小化验证：OnLoad 只留一行日志，确认能执行再加回代码

---

## ImGui 窗口不显示

OnLoad 正常但看不到窗口。

- 确认 OnProcess 在跑（加一次性日志）
- 确认 `showWindow` 变量为 `true`
- `ImGui::End()` 必须在 `if (ImGui::Begin(...))` 的**外面**：

```angelscript
if (ImGui::Begin("Window")) {
    ImGui::TextUnformatted("content");
}
ImGui::End();  // 必须在 if 外面
```

- 窗口坐标是否超出屏幕分辨率
- 按过 F9 会隐藏所有 ImGui 窗口，再按一次恢复
- Begin/End 之间如果没有控件，窗口可能小到看不见

---

## 命令没有响应

输入命令名后按回车无反应。

- 确认 `RegisterCommand` 在 OnLoad 里调用
- 日志中如果出现 `Command 'xxx' already registered, skipping` 说明重复注册
- 命令栏输入的是 `def.Name` 或 `def.Alias`，不是 AngelScript 函数名
- 确认 `BML::CommandCallback@` 绑定了正确的成员函数
- 补全不出现：检查补全函数第三个参数是否为 `BML::CommandCompletion &inout`

---

## 对象查找失败

`ctx.Borrow3dEntityByName(...)` 或 `ctx.BorrowGroupByName(...)` 返回 `null`。

**排查清单**：

- **时机**：菜单阶段查找关卡对象一定返回 null。必须在 `OnGameEvent` 收到 `BML::GAME_EVENT_START_LEVEL` 之后查找
- **名字大小写**：`PC_Checkpoints` 和 `pc_checkpoints` 是不同的名字。Ballance 命名约定：`P_` 物理实体，`PR_` 复活点，`PC_` 检查点
- **函数选择**：组用 `BorrowGroupByName`，3D 实体用 `Borrow3dEntityByName`，用错函数一定返回 null
- **关卡切换后重新查找**：旧句柄在新关卡中无效，每次进关需要重新获取

添加诊断日志定位具体原因：

```angelscript
CKGroup@ checkpoints = ctx.BorrowGroupByName("PC_Checkpoints");
if (checkpoints is null) {
    BML::Logger@ logger = ctx.BorrowLogger();
    if (logger !is null) {
        logger.Info("PC_Checkpoints not found, IsInLevel=" + ctx.IsInLevel);
    }
}
```

如果日志显示 `IsInLevel=false`，说明查找时机太早。

---

## 物理对象不运动

Impulse/SetForce 没效果。

**排查清单**：

- `BML::Physics::PhysicalizeBall` 必须返回 `true`，返回 `false` 则对象不参与物理仿真，所有力操作无效
- 施力前调用 `BML::Physics::WakeUp`。物理引擎会让静止对象进入"睡眠"以节省计算，睡眠对象不响应力
- 确认对象没有被设为 `Fixed = true`（固定对象不会移动）
- 对象可能卡在地板或墙壁内部，换空旷位置再试

**持续力不停止**：

- 确认 `TickForce` 在 `OnProcess` 中被调用
- 确认帧计数器 `forceFramesLeft` 正确递减到 0
- 确认 `ClearForce` 在计数归零时执行
- 退出关卡前先调用 `ClearForce`，否则物理引擎可能在对象销毁后仍尝试施力

---

## 配置不保存

改了配置但重启后恢复默认值。

- 修改后必须调用 `messagesEnabledProp.SetBoolean(value)` 写回；只改成员变量不够
- `GetProperty` 返回值可能为 null，调用 Set 前判空
- `SetDefaultBoolean` 不覆盖已有值，只在键不存在时写入
- 用记事本打开 `ModLoader/Configs/xxx.cfg` 验证值是否写入

---

## 关卡切换后崩溃

切关后脚本行为异常或崩溃。

- 在 `GAME_EVENT_PRE_EXIT_LEVEL` 中清理句柄：`@spawnedBall = null`
- 持续力在关卡退出前必须 `ClearForce`
- 关卡相关的状态变量（计数器、标志位）需要重置
- 安全模板：

```angelscript
void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
    if (event == BML::GAME_EVENT_START_LEVEL) {
        SetupLevel(ctx);
    } else if (event == BML::GAME_EVENT_PRE_EXIT_LEVEL ||
               event == BML::GAME_EVENT_PRE_LOAD_LEVEL ||
               event == BML::GAME_EVENT_EXIT_GAME) {
        CleanupLevel(ctx);
    }
}
```

---

## 性能问题

游戏帧率下降，加载 mod 后明显卡顿。

**常见原因**：

1. **OnProcess 中每帧查找对象**：`Borrow3dEntityByName` 每次调用都要遍历对象表。应当在 `OnGameEvent` 中查找一次，结果保存到成员变量
2. **每帧写日志**：日志涉及文件 I/O，每秒 60 次写入会拖慢帧率。调试完后移除频繁日志
3. **ImGui 控件过多**：数百个 `TextUnformatted` 调用会累积渲染开销
4. **每帧创建大量临时字符串**：字符串拼接如果在 OnProcess 中频繁执行，会产生内存压力

**定位方法**：

1. 临时移除 `.mod.as` 文件，确认不加载 mod 时 FPS 正常
2. 把文件放回，逐步注释 `OnProcess` 中的逻辑块
3. 每次注释一段后重启观察。FPS 恢复时说明被注释的部分是瓶颈
4. 对瓶颈代码进行优化：缓存查找结果、减少字符串操作、降低日志频率

---

## 如何向社区提问

提问时需要提供的信息：

1. **BML 版本和操作系统**
2. **日志片段**：从 `Loading Mod` 行到第一条 ERROR，包含上下文
3. **相关源代码**：至少贴出错误行号附近的代码
4. **复现步骤**：进了哪一关、按了什么键、做了什么
5. **期望结果 vs 实际结果**

模板：

```text
## 问题
[一句话概括]

## 环境
BML 版本 / 游戏版本 / 操作系统

## 日志片段
[粘贴]

## 源代码（相关部分）
[粘贴]

## 复现步骤
1. ...

## 期望 / 实际
```

无效提问示例："mod 不工作了"（没有日志、没有代码、没有上下文）。

---

## 结语

调试是脚本开发的日常。通过日志定位问题层次，通过最小化复现缩小范围，通过一次改一处验证假设。大部分问题的答案都在 `ModLoader/ModLoader.log` 里。

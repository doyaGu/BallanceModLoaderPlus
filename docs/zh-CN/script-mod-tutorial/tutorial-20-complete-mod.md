# 20 完整 Mod：把所有技能整合到一起

前面的章节分别练习了配置读写、命令注册、ImGui 窗口绘制、物理对象操作。每一章都聚焦于单一功能，但真正可用的 mod 需要把这些能力协调地组织在一起。本章构建一个"完整 mod"，它拥有配置、命令、按键输入、ImGui 状态窗口和物理球体管理，同时演示生命周期清理的完整流程。

## 这个 mod 做什么

启动游戏后你会看到一个悬浮窗口，显示当前状态（是否在关卡中、球体是否存在、持续力是否活跃）。你可以通过三种方式控制它：

| 触发方式 | 创建球体 | 推动球体 | 销毁球体 | 切换窗口 |
| --- | --- | --- | --- | --- |
| 按键 | J | I | K | - |
| 命令 | `cmod spawn` | `cmod push` | `cmod despawn` | `cmod window` |
| ImGui 按钮 | Spawn | Push | Despawn | - |

可验证的结果：

1. 进入关卡后按 J，玩家球上方 5 单位处出现一个木球并受重力下落
2. 按 I，木球向 X 正方向弹出
3. 按 K，木球消失；日志输出 `cleaned by key`
4. 退出关卡时即使忘记按 K，木球也会被自动清理
5. 在配置中将 `AutoSpawn` 设为 `true` 并重新进入关卡，木球自动出现

## 架构设计：为什么这样组织代码

### 四个生命周期回调各管一件事

| 回调 | 触发时机 | 适合做什么 |
| --- | --- | --- |
| `OnLoad` | 脚本被加载时执行一次 | 读配置、注册命令，这些只做一次的初始化 |
| `OnGameEvent` | 游戏状态变化时 | 关卡开始/退出时创建或清理对象 |
| `OnProcess` | 每一帧 | 输入检测、持续力倒计时、绘制窗口 |
| `OnUnload` | 脚本卸载前 | 最终清理，释放所有资源 |

为什么不把所有逻辑放在 `OnProcess` 里？因为帧回调每秒执行几十次，把"读配置"这种一次性工作放在里面既浪费又容易出错。分离关注点让每个回调的责任清晰：`OnLoad` 保证初始化只跑一次，`OnGameEvent` 保证场景切换时资源被正确管理，`OnProcess` 专注于持续性的每帧行为。

### 为什么把逻辑拆分成 private 方法

观察代码中的 `SpawnBall`、`CleanupBall`、`PushBall`，它们是独立的操作函数。命令回调 `OnCommand` 调用它们，按键处理 `HandleInput` 也调用它们，ImGui 按钮点击同样调用它们。如果在三个地方各写一份创建球体的逻辑，修改摩擦力参数时就需要改三处。

这就是"单一入口"原则：每个动作（创建、推动、清理）在代码中只有一个实现，所有触发路径汇聚到同一个函数。`reason` 参数记录触发来源，方便在日志中追踪是谁触发了这个操作。

## 完整脚本

将 [CompleteMod.mod.as](assets/CompleteMod.mod.as) 保存为
`ModLoader/Mods/CompleteMod.mod.as`。脚本包含配置、命令、按键、ImGui 和
物理球体管理。先运行并按上表逐项验收，再结合下面的说明阅读源码。

## 逐段解读

### 生命周期回调

`OnLoad` 只做两件事：读配置和注册命令。这里不会创建任何游戏对象，因为此时关卡可能尚未加载，`CurrentLevel` 和当前玩家球都还不存在。如果在这里就尝试读取 `CurrentLevel.ActiveBall`，得到的会是 `null`。

`OnGameEvent` 监听三个清理时机（退出关卡前、加载新关卡前、退出游戏）和一个创建时机（关卡开始）。三个清理事件看似重复，但它们覆盖了不同的退出路径，玩家可能通过菜单退出关卡、可能因死亡触发重载、也可能直接关闭游戏。任何一个路径遗漏都会导致残留对象。

`OnProcess` 每帧执行，先处理输入，再推进持续力计时器，最后绘制窗口。顺序有讲究：先处理输入可以让本帧的操作立即反映到窗口显示中。

`OnUnload` 先清理球体再注销命令。注意 `commandRef` 的空值检查，如果 `RegisterCommand` 因某种原因失败（比如命令名冲突），`commandRef` 可能是 `null`。

### 配置系统

`LoadConfig` 获取配置对象后，为每个属性设定默认值和注释。`GetBoolean(true)` 中的参数 `true` 是默认值，如果配置文件中没有这个条目，返回 `true`。这与 `SetDefaultBoolean(true)` 配合使用：前者决定运行时行为，后者决定配置文件中写入什么。

`SaveBoolean` 在用户通过命令切换窗口开关时调用，确保偏好在下次启动时仍然有效。

### 命令系统

命令注册使用了一个 `BML::CommandDefinition` 结构体来描述命令的元信息。`Alias = "cm"` 意味着玩家可以输入 `cm spawn` 代替 `cmod spawn`。`CompleteCommand` 回调提供 Tab 补全列表，当玩家输入 `cmod ` 后按 Tab，控制台会列出五个子命令。

`OnCommand` 的分发逻辑像一个路由器：根据第一个参数决定执行哪个操作函数。如果没有参数，默认执行 `status` 显示当前状态。

### ImGui 窗口

`DrawWindow` 每帧被调用（只要 `showWindow` 为 `true`）。`SetNextWindowSize` 加上 `ImGuiCond_Once` 表示窗口大小只在第一次出现时设置，之后玩家可以自由调整。

窗口内显示四行状态文本和三个操作按钮。`ImGui::SameLine()` 让按钮排列在同一行。按钮点击后调用的是同一批操作函数，与命令、按键使用完全相同的入口。

### 输入处理

`HandleInput` 检测三个按键。`IsKeyPressed` 在按键被按下的那一帧返回 `true`，之后持续按住不会重复触发。这是"按一次执行一次"的正确方式，如果误用 `IsKeyDown`（持续按住时每帧都为 `true`），一次按键就会重复创建或销毁对象。

### 物理对象管理

`SpawnBall` 是最复杂的函数，它包含完整的防御性编程：

1. 检查球体是否已存在（防止重复创建）
2. 检查资源文件是否在磁盘上
3. 加载对象后检查结果是否成功
4. 从加载结果中查找第一个 3D 实体
5. 尝试获取玩家球位置作为参考点，找不到则使用默认坐标
6. 赋予物理属性并记录结果

每一步失败都会通过 `SetLast` 记录原因并提前返回，不会在异常状态下继续执行。

`CleanupBall` 的清理顺序是：先清除力 -> 检查对象是否有效 -> 如果有物理属性则移除 -> 隐藏对象 -> 置空引用。先清力再反物理化，因为对一个已经不在物理系统中的对象调用 `ClearForce` 会失败。

### 工具函数

`SetLast` 做三件事：更新内部状态字符串、写入日志、发送游戏内消息。这保证了无论查看日志文件、ImGui 窗口还是游戏内聊天区域，都能看到最新操作的结果。

`BoolText` 把布尔值转为字符串。AngelScript 不会自动把 `bool` 转为 `"true"/"false"` 来拼接字符串，所以需要这个辅助函数。

## 预期游戏流程

以下是从启动到退出的完整操作序列和对应输出：

```text
-- 游戏启动，脚本加载 --
[complete.mod/INFO]: CompleteMod loaded

-- 进入第 1 关 --
（如果 AutoSpawn=true）
[complete.mod/INFO]: spawned by auto physical=true

-- 按下 J 键 --
[complete.mod/INFO]: spawned by key physical=true
游戏内消息: "CompleteMod: spawned by key physical=true"

-- 按下 I 键 --
[complete.mod/INFO]: push ok=true
（木球向右弹出）

-- 再次按 J --
[complete.mod/INFO]: cleaned by respawn
[complete.mod/INFO]: spawned by key physical=true
（旧球先消失，新球重新出现在玩家球上方）

-- 按下 K 键 --
[complete.mod/INFO]: cleaned by key
（木球消失）

-- 输入命令 cmod status --
[complete.mod/INFO]: status ball=false physical=false force=false

-- 退出关卡 --
（如果此时有球体存在）
[complete.mod/INFO]: cleaned by game-event

-- 关闭游戏 --
[complete.mod/INFO]: CompleteMod unloaded
```

ImGui 窗口中的 `Last:` 行会实时更新为最后一次操作的结果文本。

## 设计原则总结：操作与触发解耦

这个 mod 的核心设计可以用一张图理解：

```text
触发层               操作层                 副作用层
─────────           ─────────             ─────────
命令 "cmod spawn"  ─┐
按键 J             ─┼─> SpawnBall() ────> 日志 + 消息 + 状态更新
ImGui 按钮 Spawn   ─┘
自动（关卡开始）    ─┘

命令 "cmod push"   ─┐
按键 I             ─┼─> PushBall()  ────> 日志 + 消息 + 状态更新
ImGui 按钮 Push    ─┘

命令 "cmod despawn"─┐
按键 K             ─┼─> CleanupBall() ──> 日志 + 消息 + 状态更新
ImGui 按钮 Despawn ─┤
关卡退出事件       ─┤
脚本卸载           ─┘
```

添加新的触发方式（比如计时器自动推动）时，只需在 `OnProcess` 中加一个条件调用 `PushBall`，不需要重写推动逻辑。

## 试试扩展

在理解了这个架构之后，可以尝试以下修改来加深理解：

**添加新命令参数 `cmod force`**：让球体受到持续向上的力 60 帧。需要在 `OnCommand` 的分发中增加一个分支，设置 `forceActive = true` 和 `forceFramesLeft = 60`，并调用 `BML::Physics::SetForce`。`TickForce` 已经在处理倒计时，到时间后会自动调用 `ClearBallForce`。

**修改物理参数观察效果**：把 `Elasticity` 从 `0.2f` 改为 `0.9f`，球体会变得像弹力球一样弹跳。把 `Mass` 从 `2.0f` 改为 `0.5f`，推力效果会变得更明显。

**添加生成位置配置**：增加一个 `SpawnHeight` 配置项，替代硬编码的 `pos.y += 5.0f`。仿照 `showWindowProp` 的模式：在 `LoadConfig` 中注册属性、读取值到成员变量、在 `SpawnBall` 中使用该变量。

**增加第二种球体**：尝试加载 `P_Ball_Stone.nmo`，用一个配置项让玩家选择球体类型。`CollisionSurface` 需要同步更改为对应的 mesh 名称。

## 故障诊断

**按 J 后日志输出 `resource missing`**

游戏目录中缺少 `3D Entities\PH\P_Ball_Wood.nmo` 文件。这个文件属于 Ballance 原版安装的一部分。检查游戏目录下是否存在该路径。如果你使用的是精简版安装，可能需要从完整版复制这些资源文件。

**按 J 后日志输出 `load failed`**

文件存在但加载失败。可能原因：文件已损坏，或者 Virtools 版本不兼容。检查 BML 日志窗口中是否有更详细的错误信息。

**按 J 后日志输出 `spawned by key physical=false`**

对象创建成功但物理化失败。通常是因为 `CollisionSurface` 指定的 mesh 名称与实际加载的 mesh 不匹配。可以先用 `cmod status` 确认球体确实存在，然后检查日志中是否有物理系统的警告信息。

**球体出现但不受重力**

`ballPhysical` 为 `false` 时球体没有物理属性，自然不会下落。参考上一条排查物理化失败的原因。

**按 I 后日志输出 `push ok=false`**

物理系统未能施加冲量。常见原因：球体处于休眠状态但 `WakeUp` 失败，或者球体已经不在物理世界中（被其他 mod 干扰）。

**退出关卡后球体仍然可见**

检查 `OnGameEvent` 是否确实被触发，在 `CleanupBall` 的 `SetLast` 输出中应该看到 `cleaned by game-event`。如果没有这条日志，说明事件未到达。可能是脚本加载顺序问题导致事件在你的回调注册之前就已经发出。

**ImGui 窗口不显示**

检查 `showWindow` 的值。输入 `cmod window` 切换一次，观察日志中输出的 `window=true` 或 `window=false`。如果配置文件中被设为 `false` 且你没注意到，窗口就不会出现。

**命令输入后没有反应**

确认文件名为 `.mod.as` 后缀，确认 `[bml.mod ...]` 属性行在文件最顶部且没有前导空格。如果命令注册失败，`cmod` 不会被识别为有效命令。

## 完成状态

完整 mod 骨架完成。你已经掌握了：

- 生命周期管理：`OnLoad` 初始化、`OnGameEvent` 响应场景变化、`OnProcess` 处理帧逻辑、`OnUnload` 清理
- 配置系统：注册属性、读取默认值、运行时写回
- 命令系统：定义、分发、Tab 补全
- ImGui 窗口：状态显示和交互按钮
- 物理操作：加载对象、物理化、施力、清理
- 防御性编程：每一步都检查前置条件并记录失败原因
- 架构原则：触发与操作解耦、单一入口、多路径清理

下一步：[21 发布](tutorial-21-publish.md)

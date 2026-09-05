# Behavior 编写

`BML/Behavior.hpp` 是 Native Mod 使用 Virtools Behavior 的 C++ interface。它既能创建和执行任意已注册 Building Block，也能读取和修改已有 Behavior graph。`BML/Behavior.h` 是同一能力的 C seam；一般 C++ 作者不需要直接操作其中的 wire DTO。该 interface 仍在发布前开发阶段，目前只支持 Win32 Native C++ Mod，源码兼容性尚未冻结。

## 1. 对象模型

```text
Prototype -> Block -> Call / Task / Instance -> Frames
                         |
                         +--------------------> live CKBehavior

existing CKBehavior graph -> Graph snapshot -> Edit -> Patch / Plan
```

| 对象 | 含义 |
| --- | --- |
| `Prototype` | 一次具体的 BB provider 注册，由 GUID 和 provider generation 标识 |
| `Block` | 尚未实例化的 BB 配置；可复制，修改采用 copy-on-write |
| `Call` / `Task` / `Instance` | 各自拥有一个真实 `CKBehavior`，区别只是由谁继续 Execute |
| `Frames` | 从 native Execute 复制出的控制流结果和可选 Pout 值 |
| `Graph` | 某个时刻的 immutable Behavior graph snapshot |
| `Edit` | 尚未安装的 symbolic graph transformation |
| `Patch` | 应用于一个确定 graph snapshot 的 Edit |
| `Plan` | 跨 world 按 script selector 反复 reconcile 的 Edit |

## 2. 打开 Session

在 Mod 初始化时打开一个 `Session`：

```cpp
#include <BML/Behavior.hpp>
using namespace BML::Behavior;

auto opened = Session::Open();
if (!opened) {
    GetLogger()->Error("Behavior unavailable: %s",
                       opened.GetStatus().Message.c_str());
    return;
}
m_Behavior = std::move(opened).Value();
```

Loader 会核对调用 DLL，并把 Session 绑定到当前 Mod generation。Session 跨 world reset 保持有效；run、Watch 和 Patch 等 world-bound 对象不会。Mod 卸载时，Loader 会先停止新 admission，并在 DLL 释放前完成 callback 和 native Behavior 的退役。

由 Session 创建的对象持有自己所需的 Session lease。移动或关闭最初的 `Session` 值，不会使仍存活的 Block、run、Watch、Patch 或 Plan 立即失效。

### 查找 Prototype

已知 GUID 时可以直接 `Use(CKGUID)`。不知道 GUID，或需要固定当前 provider 时，先查询 catalog：

```cpp
PrototypeQuery query;
query.Name = "Physicalize";

auto found = m_Behavior.Prototypes(query);
if (!found || found->size() != 1)
    return;

Prototype prototype = found->front().Ref;
auto declared = m_Behavior.Layout(prototype);
```

空 `PrototypeQuery` 返回当前可发现的全部注册。`Session::Layout(prototype)` 读取 declared Layout，不创建 native Instance，也不发送 lifecycle callback。

## 3. 配置任意 Building Block

`Session::Use` 从 Prototype 创建 `Block`：

```cpp
auto block = m_Behavior.Use(prototype)
    .TargetOwner()
    .Settings({{"Mode", 2}, {"Detail", 4}})
    .Settings({{"Created Later", 9}})
    .Pins({{"Strength", 12.0f}})
    .Locals({{"Accumulator", 0.0f}})
    .Frames(Signals(64).Pouts());
```

每次 `Settings({...})` 是一个 Setting stage。一个 stage 写完后，Runtime 会发送 `SETTINGSEDITED`；BB 可以在 callback 中重建 interface，所以下一个 stage 必须在新的 live Layout 上重新解析。全部 Setting stage 完成后，Runtime 才应用 Pin 和 Local。

Target 有三种形式：`TargetOwner()`、`Target(type, object)` 和 `NullTarget(type)`。slot selector 也必须明确：`At(index)`、`Named(name, occurrence)` 或 `Unique(name)`。字符串重载等价于 `Unique(name)`，重名时不会擅自选择第一项。

`Block::Validate()` 是可选的 declared-Layout 检查，不创建 `CKBehavior`。它只能验证 Prototype 初始声明中已有的 Target、selector 和 type；Setting callback 动态创建的 slot 仍由真正打开 run 时的 native lifecycle 验证。即使没有先调用 `Validate()`，run admission 也不会跳过这些检查。

第一次能够可靠识别 provider 的验证或 admission 会固定 provider generation。之后若同一 GUID 被另一 provider 替换，旧 Block 会返回 stale，不会静默换用新实现。无法可靠跟踪 provider retirement 时，即时 run 仍可使用 generation-zero provider，但这种 Block 不能进入跨 world 的 durable Edit。

## 4. 选择 Execute 的所有权

三种 run 都拥有一个 native Behavior；它们不是三种 Behavior 类型。

| 创建方式 | 首次 Execute | 后续 Execute |
| --- | --- | --- |
| `Call(input)` | 立即一次 | 只有把 `Call` 移入 `Continue()` 后才由 Loader 托管 |
| `Start(input)` | 立即一次 | Loader 按 game frame 推进 native continuation |
| `Spawn()` | 不执行 | Mod 用 `Pulse(input)` 驱动 |
| `SpawnIn(graph)` | 不执行 | 与 `Spawn` 相同，但 Behavior 作为未连接节点留在 graph 中 |

```cpp
auto once = block.Call("Run");
auto task = block.Start("Run", Latest());
auto instance = block.Spawn();
auto parked = block.SpawnIn(graph);

if (instance)
    instance->Pulse("Reset");
```

传给创建函数的 Frame policy 只覆盖本次 run。`Call::Continue()` 保留同一个 native Instance；它不是重新激活或重新创建。

同一 Instance 每个 game frame 最多 Execute 一次。同 frame 或 callback 重入的 Pulse 会排队；相同 logical In 合并，不同 In 保持首次 admission 顺序。`Ready` 只表示没有 native continuation 和 queued In，不表示 BB 已经释放 Local、manager 注册或其他持久状态。

三种 run 都提供 `Info()`、`Take()`、`Layout()`、`Inspect()`、`Set()`、`Bind()`、`Settings()` 和 `Close()`。只有 `Call` 提供 `Continue()`；`Task` 和 `Instance` 提供 `Pulse()`。

## 5. 读取 Frames

每次 native Execute 都会在清除本次 active Out 前形成一个 immutable Frame。Frame 记录 sequence、game frame、native result、continuation、active Outs 和 diagnostic。

Frame retention 与 Pout capture 是两个独立选择：

| Policy | 保留内容 |
| --- | --- |
| `Signals(n)` | 首次、出现 active Out、失败和 non-continuing Frame |
| `EachFrame(n)` | 每次 Execute |
| `Latest()` | 最新 continuing、failure 和 non-continuing Frame |
| `Ignore()` | 不保留普通 Frame，但保留 failure 与 non-continuing Frame |

只有在 policy 上调用 `.Pouts()`，Runtime 才读取并复制 Pout：

```cpp
auto task = block.Start("Run", Signals(64).Pouts());

Frames frames;
frames.Reserve(16, 4096);
if (auto taken = task->Take(frames)) {
    for (Frame frame : frames) {
        if (frame.HasOut("Done")) {
            auto speed = frame.Pout<float>("Speed");
            if (speed)
                Use(speed.Value());
        }
    }
}
```

`Take()` 是自动分配版本；`Take(Frames&)` 会复用已有 header 和 payload buffer。容量足够时，它只调用一次 C seam，且不为 record 或 string 另行分配。`Frame`、`Out` 和 `Pout` 都是所属 `Frames` 的只读 view；下一次修改该 `Frames` 后，旧 view 全部失效。

object Pout 会在对象仍 live 时签发 `ObjectRef`，之后读取 Frame 不再访问原 CK parameter 或 CK object。任何 Pout 读取或 encoding 失败都会丢弃该次不完整的 Pout batch，但保留同一 Frame 的 active Outs 和 diagnostic。

没有 continuation 的 Frame 不等于 run handle 已经关闭：处于 `Ready` 的 Instance 仍可接受下一次 Pulse。只有失败、显式关闭或队列溢出才会停止 admission。

有界队列满时不会丢弃旧 Frame；run 会停止，并在独立 failure slot 中报告 `FrameQueueFull`。成功 Take 后会释放普通队列容量。

## 6. 使用 live Layout

run 的 `Layout()` 返回当前配置后的 native interface。`Slot` 带有 Instance identity 和 layout generation，可直接交给 `Set()` 或 `Bind()`：

```cpp
auto layout = instance->Layout();
if (layout) {
    if (auto strength = layout->Find(SlotKind::Pin, "Strength"))
        instance->Set(*strength, 20.0f);
}
```

lifecycle callback 边界会使旧 Slot 失效，因为 provider 可能重建了相同形状的 interface。普通 Execute 只有在 Target、In/Out、Pin/Pout、Setting 或 Local identity 实际变化时才推进 layout generation。旧 Slot 或 Port 会返回 `LayoutChanged`，不会按旧 ordinal 写错参数。

`Settings({...})` 可对 live run 应用新的 Setting stage。Runtime 会重新取得 Layout，并恢复仍然唯一且类型匹配的 Target、Pin、Local 和 source relation。

## 7. 读取 graph

`Session::Inspect` 和 run 的 `Inspect` 返回 immutable `Graph` snapshot：

```cpp
auto snapshot = m_Behavior.Inspect(script);
if (!snapshot)
    return;

auto counter = snapshot->Find("Counter_Active");
if (counter)
    auto value = snapshot->Read(counter->Pout("Count"));
```

`Graph::Find` 要求名称恰好匹配一个 Node；`FindAll` 返回全部匹配。Node、Port 和 Link 是指向共享 snapshot allocation 的轻量 view。Port 保留所属 Node 的 layout generation；把旧 snapshot 的 Port 用于新的 live Layout 会失败。

`Logical()` 重新读取作者可见的 graph，`Live()` 重新读取实际 CK graph。Logical view 保留显式 Add 和 Flow，隐藏由 Tap、Before、After 与 Splice 安装的精确基础设施，并恢复 splice anchor 的 logical endpoints。若其他代码改坏了 Patch 所声明的 after-image，Runtime 返回 `GraphChanged`，不会根据名称或形状猜测。

`Graph::Read` 跟随 stored、direct 和 shared source，但不会为了读取而执行 Parameter Operation；operation value 会报告 indeterminate。

Watch 每个 game frame 采样一次 graph、layout 或 value。callback 或观察失败后，Watch 保留第一条 `Status`，进入 `Failed` 并停止后续 callback。

## 8. 修改 graph

`Edit` 是唯一的 symbolic graph transformation：

```cpp
Edit edit;
auto source = edit.Require("Counter_Active");
auto added = edit.Add(block);
edit.Flow(source.Out(), added.In());

auto patch = graph.Apply("extra-life", edit);
auto plan = m_Behavior.Plan(
    "extra-life", Scripts::One("Gameplay_Events"), edit);
```

`Edit::Node`、`Edit::Port`、`Edit::Link` 和 `Edit::Path` 是 authoring symbol，不是 graph snapshot view。symbol 只能在创建它的那个 Edit 中使用。`Use(snapshotNode)` 和 `Use(snapshotLink)` 可以导入精确的 world-bound identity，因此只适用于 Patch；Plan 必须能在未来 world 重新解析，会拒绝这些 identity 和非 null ObjectRef。

`Add(block)` 会复制 Block 配置和已经固定的 provider generation；之后修改原 Block 不影响 Edit。Block 的 Frame policy 不属于 graph authoring。

常用 transformation：

- `Flow` / `FlowCycle`：增加 Behavior Link；
- `Bind` / `Share` / `Push`：声明 parameter relation；
- `Tap` / `Before` / `After`：安装 callback；
- `Splice`：让现有 Link 经过新增 Block；
- `Redirect`：暂时改变 Link destination；
- `AppendIn/Out/Pin/Pout/Local`：扩展 dynamic interface。

`Graph::Apply` 将 Edit 应用到一个精确 fingerprint，返回一次性 `Patch`。`Session::Plan` 使用 `Scripts::Each(name)` 或 `Scripts::One(name)`，在匹配 script 出现、删除或 world reset 后重新 reconcile。

`Patch` 和 `Plan` 都用 `Info()` 查看状态，用 `Close()` 退役。Close 会比较它仍然拥有的 Link、source 和 graph after-image；外部修改导致 `RevertConflict` 时，handle 保持可读，作者修复冲突后可以再次 Close。

Hook callback 在 game thread 执行。异常不会穿过 DLL seam；callback 内 self-close 只关闭后续 admission，graph restore、native teardown 和 callback state release 会在 safe point 完成，不会等待当前 callback。

## 9. 已命名的 retail BB

`BML/Behavior/Blocks.hpp` 汇总 BML+ 已知的 header-only adapter，例如 Object Load、Physicalize、Physics Force、Physics Impulse、Physics Wake Up、Send Message 和 2D Text。每个 header 只封装该 BB 的 Prototype、Options 和 slot knowledge：

```cpp
#include <BML/Behavior/Blocks/Text2D.hpp>

Blocks::Text2D::Options options;
options.Text = "score";
options.FontIndex = 2;

auto made = Blocks::Text2D::Make(m_Behavior, options);
if (made)
    auto text = std::move(made).Value().SpawnIn(graph);
```

这些 adapter 仍返回普通 `Block`，不会绕过统一的 lifecycle、execution 或 teardown。新 Native Mod 不应再使用 legacy ExecuteBB interface。

## 10. 生命周期、错误与性能

| 事件 | Session | Run | Watch / Patch | Plan |
| --- | --- | --- | --- | --- |
| 显式 Close | 最后一个 lease 关闭 native Session | 停止 admission，safe point 完成 teardown | closure/conflict 期间仍可读 | installation 退役期间仍可读 |
| world reset | 保持有效 | 关闭 | 随旧 graph 关闭 | 保持有效，在新 world reconcile |
| Mod unload/reload | owner generation 退出 | DLL unload 前关闭 | callback 和 graph state 先退役 | callback code unload 前退役 |

除 Close 外，Behavior 操作要求 game thread。所有 `Result<T>` 都同时包含稳定错误类别和 `Status`；控制流只应判断 error/phase，不应解析 message 文本。

高频路径应复用 `Block`、`Frames` 和已有 graph snapshot。Block 会共享已编译的 C descriptor；`Take(Frames&)` 在容量足够时避免额外分配；Node、Port、Link 和 Frame 都是 view，不复制 record 或 string。

当前公开 interface 不包含 Transform expression lowering、通用 Node replacement、AngelScript Behavior projection 或第三方 parameter format registration。缺少这些能力时会明确返回 unavailable/unsupported，不会把未知 Virtools parameter 当作任意 bytes 复制。

# Behavior 编写

`BML/Behavior.hpp` 是 Native Mod 使用 Virtools Behavior 的 C++ interface。它能创建和执行任意已注册 Building Block、从零创建顶层 Script graph，也能读取和修改 Behavior graph。`BML/Behavior.h` 是同一能力的 C seam；一般 C++ 作者不需要直接操作其中的 wire DTO。该 interface 仍在发布前开发阶段，目前只支持 Win32 Native C++ Mod，源码兼容性尚未冻结。

## 1. 对象模型

```text
Prototype -> Block -> Call / Task / Instance -> Frames
                         |
                         +--------------------> live CKBehavior

existing CKBehavior graph -> Graph snapshot -> Edit -> Patch / Plan

CKBeObject -> Script -> Graph snapshot -> Edit -> Patch
```

| 对象 | 含义 |
| --- | --- |
| `Prototype` | 一次具体的 BB provider 注册，由 GUID 和 provider generation 标识 |
| `Block` | 尚未实例化的 BB 配置；可复制，修改采用 copy-on-write |
| `Call` / `Task` / `Instance` | 各自拥有一个真实 `CKBehavior`，区别只是由谁继续 Execute |
| `Frames` | 从 native Execute 复制出的控制流结果和可选 Pout 值 |
| `Script` | 一个 owner-scoped 的顶层 graph-backed `CKBehavior` |
| `Graph` | 某个时刻的 immutable Behavior graph snapshot |
| `NodePattern` | 在一个 graph scope 内解析的结构条件 |
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
m_Behavior = opened.Take();
```

Loader 会核对调用 DLL，并把 Session 绑定到当前 Mod generation。Session 跨 world reset 保持有效；run、Script、Watch 和 Patch 等 world-bound 对象不会。Mod 卸载时，Loader 会先停止新 admission，并在 DLL 释放前完成 callback 和 native Behavior 的退役。

对具名 Result 调用 `Result<T>::Value()` 只借用其中的值；临时 Result 可以直接按值返回可复制对象。move-only 领域对象需要转移所有权时使用 `Take()`。`Take()` 会清空结果中的值，但 `Code()` 和 `GetStatus()` 仍可用于读取诊断。

由 Session 创建的对象持有自己所需的 Session lease。移动或关闭最初的 `Session` 值，不会使仍存活的 Block、run、Script、Watch、Patch 或 Plan 立即失效。

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
    .Locals({{"Accumulator", 0.0f}});

auto task = block.Start("Run", Signals(64).Pouts());
```

每次 `Settings({...})` 是一个 Setting stage。一个 stage 写完后，Runtime 会发送 `SETTINGSEDITED`；BB 可以在 callback 中重建 interface，所以下一个 stage 必须在新的 live Layout 上重新解析。全部 Setting stage 完成后，Runtime 才应用 Pin 和 Local。Frame retention 属于每次 Call、Task 或 Instance，而不是可复用的 Block。

Target 有三种形式：`TargetOwner()`、`Target(type, object)` 和 `NullTarget(type)`。slot selector 也必须明确：`At(index)`、`Named(name, occurrence)` 或 `Unique(name)`。字符串重载等价于 `Unique(name)`，重名时不会擅自选择第一项。

`Block::Validate()` 是可选的 declared-Layout 检查，不创建 `CKBehavior`。它只能验证 Prototype 初始声明中已有的 Target、selector 和 type；Setting callback 动态创建的 slot 仍由真正打开 run 时的 native lifecycle 验证。即使没有先调用 `Validate()`，run admission 也不会跳过这些检查。

第一次能够可靠识别 provider 的验证或 admission 会固定 provider generation。之后若同一 GUID 被另一 provider 替换，旧 Block 会返回 stale，不会静默换用新实现。无法可靠跟踪 provider retirement 时，即时 run 仍可使用 generation-zero provider，但这种 Block 不能存入 Plan。

## 4. 选择 Execute 的所有权

三种 run 都拥有一个 native Behavior；它们不是三种 Behavior 类型。

| 创建方式 | 首次 Execute | 后续 Execute |
| --- | --- | --- |
| `Call(input)` | 立即一次 | 只有 `Continue()` 成功后才由 Loader 托管 |
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

Frame policy 只属于本次 run；可复用的 Block 不保存观察策略。`Call::Continue()` 保留同一个 native Instance；它不是重新激活或重新创建。成功时 Call 被消费并返回对应 Task；失败时 Call 仍可继续使用。

同一 Instance 每个 game frame 最多 Execute 一次。同 frame 或 callback 重入的 Pulse 会排队；相同 logical In 合并，不同 In 保持首次 admission 顺序。`Ready` 只表示没有 native continuation 和 queued In，不表示 BB 已经释放 Local、manager 注册或其他持久状态。

三种 run 都提供 `Info()`、`TakeFrames()`、`Layout()`、`Inspect()`、`Set()`、`Bind()`、`Settings()` 和 `Close()`。只有 `Call` 提供 `Continue()`；`Task` 和 `Instance` 提供 `Pulse()`。

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
if (auto taken = task->TakeFrames(frames)) {
    for (Frame frame : frames) {
        if (frame.HasOut("Done")) {
            auto speed = frame.Pout<float>("Speed");
            if (speed)
                Use(speed.Value());
        }
    }
}
```

`TakeFrames()` 是自动分配版本；`TakeFrames(Frames&)` 会复用已有 header 和 payload buffer。容量足够时，它只调用一次 C seam，且不为 record 或 string 另行分配。这个名字也明确区分了它的消费语义与 `Result<T>::Take()`。`Frame`、`Out` 和 `Pout` 都是所属 `Frames` 的只读 view；下一次修改该 `Frames` 后，旧 view 全部失效。

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

`Settings({...})` 可对 live run 应用新的 Setting stage。Runtime 会重新取得 Layout，并恢复仍然唯一且类型匹配的 Target、Pin、Local 和 source relation。native mutation 一旦开始，任何写入、callback、Layout 或 relation 失败都会让 run 进入 `Failed`：`Info()` 保留首个 `Status`，后续 mutation 和 execution 返回同一 diagnostic，而 native Instance 仍由该 run 持有，直到 `Close()`。

## 7. 读取 graph

`Session::Inspect` 和 run 的 `Inspect` 返回 immutable `Graph` snapshot：

```cpp
auto snapshot = m_Behavior.Inspect(script);
if (!snapshot)
    return;

auto counter = snapshot->Find(Named("Counter_Active", 0));
if (counter) {
    auto value = snapshot->Read(counter->Pout("Count"));
    // 在 snapshot 有效期间使用 value。
}
```

`Find` 和 `FindAll` 与 Block slot 使用同一组 selector：`At(index)`、
`Named(name, occurrence)` 和 `Unique(name)`，还可附加 Prototype GUID 进一步
限定。`Node::Index()` 是该 graph 中的直接子节点位置，`Node::IsGraph()` 用来
区分 graph-backed Behavior 和 function-backed BB。`Graph::Inspect(node)` 进入
graph-backed child，返回以该 Node 为 root 的新 snapshot。原生指针只通过
`Session::Inspect(CKBehavior *)` 进入根 graph；child Node 和 Link 必须来自
snapshot，因此一个 Edit 不会暗中混用不同 graph 的 identity。

`Incoming` 和 `Outgoing` 返回零分配 Link view。`Entering`、`Leaving`、
`Previous` 和 `Next` 只接受唯一的 topology 结果；遇到分支时报告歧义，不会
随意挑一条。Node、Port、Link 和 ParameterOperation 都是指向同一个共享
snapshot allocation 的轻量 view。`Graph::Operations()` 列出每个真实
`CKParameterOperation`，包括 operation GUID、精确的 result/input type tuple、
graph owner 和 object identity。Port 保留所属 Node 的 layout generation；把
旧 snapshot 的 Port 用于新的 live Layout 会失败。

`Logical()` 重新读取作者可见的 graph，`Live()` 重新读取实际 CK graph。Logical view 保留显式 Add 和 Flow，隐藏由 Tap、Before、After 与 Splice 安装的精确基础设施，并恢复 splice anchor 的 logical endpoints。若其他代码改坏了 Patch 所声明的 after-image，Runtime 返回 `GraphChanged`，不会根据名称或形状猜测。

`Graph::Read` 跟随 stored、direct 和 shared source，但不会为了读取而执行 Parameter Operation；operation value 会报告 indeterminate。

Watch 每个 game frame 采样一次 graph、layout 或 value。callback 或观察失败后，Watch 保留第一条 `Status`，进入 `Failed` 并停止后续 callback。

## 8. 创建 Script graph

`Session::CreateScript` 在当前 Scene 中创建一个由 live `CKBeObject` 拥有的顶层 Script，并在返回前安装初始 `Edit`。Script 初始不激活，因此 Virtools 不会调度一个只完成了一半的 graph：

```cpp
Edit body;
auto graph = body.Root();
graph.AppendIn("Start");
graph.AppendOut("Done");
auto left = graph.AppendLocal("Left", CKPGUID_FLOAT);
auto sum = graph.AddOperation(
    addition, CKPGUID_FLOAT, CKPGUID_FLOAT, CKPGUID_FLOAT);
graph.Bind(left, 2.0f)
    .Bind(sum.Input(0), left)
    .Bind(sum.Input(1), 3.0f);

auto created = m_Behavior.CreateScript(owner, "My Script", body);
if (!created)
    return;
Script script = created.Take();
script.Activate(true);
```

`CreateScript` 只执行一次 native `AddScript`，同时建立 owner 与 Scene membership，然后编译并应用完整 Edit。validation、lifecycle、callback 或 graph 任一阶段失败都不会返回 Script handle；未发布的 root 会被移除，也不会进入 Plan。安装后的初始 graph 由 Script 自身拥有，不要求作者额外保存第二个 Patch handle。

`Edit::Root()` 返回正在编写的 symbolic graph root scope；这里不能用 `Use()` 导入 snapshot root。root 可以拥有 In、Out、Pin、Pout 和 Local。`AddOperation` 创建一个由 graph 拥有的真实 `CKParameterOperation`；Virtools 根据 operation GUID 和精确的 result/input type tuple 选择函数，并在 consumer 读取 result 时惰性求值。每个已声明的 operation input 都必须绑定。后续增量修改仍使用 `Script::Apply()`。激活请求在下一个 Behavior safe point 应用，并排在待处理 Patch reconcile 之后。即使 Script 已经 active，传入 `true` 仍会明确请求 Virtools reset 语义。

Script 关闭也在 safe point 完成：首次 `Close()` 可能返回 `CloseState::Closing`；完成 deactivate、从 owner 移除和 native destruction 后，再次调用返回 `Closed`。应先关闭 Script，再销毁 owner；Mod unload 和 world reset 会自动执行相同的退役流程。

## 9. 修改 graph

`Edit` 是带显式 graph scope 的唯一 symbolic transformation：

```cpp
Edit edit;
auto root = edit.Root();
auto dispatch = root.Require(
    NodePattern("Switch On Message")
        .Kind(BehaviorKind::Function)
        .Ins(2).Outs(11).Pins(11).Pouts(0));
auto checkpoint = root.Require(
    NodePattern("Wait Message").Pin(
        0, Value::As(CKPGUID_MESSAGE, checkpointMessage)));
auto highscoreNode = root.Require("Highscore");
auto highscore = highscoreNode.Graph();

auto done = highscore.AppendOut("Done");
auto activators = highscore.Each("Activate Script");
highscore.Flow(activators.Out(), done);
root.After(highscoreNode.Out("Done"), hook);
```

`NodePattern` 是 `Require` 使用的结构词汇。它可以组合 index/name
selector、Prototype、Behavior kind、各类 port 的精确数量，以及对 Target、Pin、
Pout、Setting 或 Local 值的 non-forcing 观察。所有条件共同标识一个 Node；找
不到或结果不唯一时，Plan 保持 unsatisfied，不会猜测。Pattern 不保存 native
identity，也不接受作者 predicate，因此 Script 在另一个 world 出现时，Plan
可以用同一份 Edit 重新解析。只有已经通过廉价结构条件的候选 Node 才会读取值。

`Require(pattern)` 必须唯一选中一个 Node。`Each(pattern)` 选择非空 Node 集合，
按原生 child index 顺序对其 `Ports` 重复 operation；它不是作者 callback，也不
保留可执行 predicate。`Next`、`Previous` 取得一条 control Link 另一端的 Node；
可选的 `NodePattern` 会先过滤相连 Node，再要求 relation 唯一，因而可以准确表达
合法的 fan-out。`Leaving`、`Entering` 和 `To` 按 topology 标识 Link。Plan 每次
安装时都会从 logical graph 重新解析这些 relation，调用者无需缓存 snapshot 或
slot index。`Redirect(incoming, leaving)` 会把第一条 Link 送到第二条 Link 当前的
destination，可直接表达绕过一个 Block。Redirect 本身是作者期望的 topology，
所以仍出现在 Logical view 中；隐藏的只是实现它的物理 Link chain infrastructure。

`Edit` 拥有 transformation program，`Edit::Graph` 是唯一公开的 authoring
interface。`Edit::Root()` 返回 root scope，`Edit::Node::Graph()` 进入一个
graph-backed Node；`Edit` 本身不再镜像 graph-local operation。graph scope
是可低成本复制的值，修改操作也返回值，因此从 `Root()` 临时值开始的链式调用可以
安全保存。所有 scope 和 symbol 的生命周期都受所属 Edit 限制。父 graph 只能连接 child 的 public port；不同 scope 的
internal port 不能直接相连。`AddGraph(name, priority)` 创建真实的 graph-backed
child，其 nested scope 在同一个 transaction 中定义 public interface 与内部
body。root 和所有 nested scope 一起验证，恢复顺序固定为 child 在前、parent 在后。

`Edit::Node`、`Edit::Port`、`Edit::Link` 和 `Edit::Path` 是 authoring symbol，
不是 graph snapshot view。symbol 同时属于创建它的 Edit 和 graph scope。
`Require(snapshotNode)` 与 `Require(snapshotLink)` 复制结构身份：child 位置、
名称、Prototype、graph/function kind、port layout 与 Link endpoint。因此它们
可以在新 world 重新解析；结构一旦漂移就明确失败。`Use(snapshotNode)` 和
`Use(snapshotLink)` 保留精确 ObjectRef，只适用于 exact Patch。Plan 会拒绝这些
identity，以及 Edit 或 Block 中的所有 non-null ObjectRef。

`Add(block)` 会复制 Block 配置和已经固定的 provider generation；之后修改原 Block 不影响 Edit。Block 的 Frame policy 不属于 graph authoring。

安装时，所有新增 Block 都会先完成 `CREATE`、`ATTACH` 和 Setting stages，再建立 graph parameter relation。每个新增 Block 唯一一次最终 `EDITED` callback 能看到这些 relation；只有 callback 完成并重新验证 Layout 后，Patch 才建立 control Flow。被修改 value 或 parameter relation 的已有 Block 同样在安装后收到一次 `EDITED`，并在成功恢复后再收到一次；只有 control Flow 的 Patch 不发送 block-level `EDITED`。

常用 transformation：

- `Flow` / `FlowCycle`：增加 Behavior Link；
- `Bind` / `Share` / `Push`：声明 parameter relation；
- `Tap` / `Before` / `After`：安装 callback；
- `Splice`：让现有 Link 经过新增 Block；
- `Redirect`：暂时改变 Link destination；
- `Next` / `Previous` 与 `Leaving` / `Entering` / `To`：描述每个 world 中重新解析的 topology；
- `Each`：对 Pattern 匹配的所有 Node 应用同一个 operation；
- `AppendIn/Out/Pin/Pout`：扩展 dynamic interface；
- `AppendLocal`：用于 graph root 或同一 Edit 新增的 Block。Local 属于其实现，
  Edit 不能向借用的既有 Node 添加 Local；
- `AddGraph`：新增 graph-backed child，并直接编写它的 nested scope；
- `AddOperation`：增加由 graph 拥有、惰性求值的 Parameter Operation；
- `Replace`：用 public interface 完全相同的 configured Block 替换一个 idle child Node；
- `Remove`：在 Patch 存续期间，将一个既有 child Node 及其所有入/出 Behavior Link
  移出 graph。

`Replace` 保留原有 Link 对象及其 delay、Pin 与 Target source、Pout destination、
名称、priority 和 owner。Settings 与 Locals 是实现私有状态，因此 replacement
使用自身 Block 配置，不复制原 Node 的私有状态。关闭 Patch 时，会先恢复同一个
原 Node 及上述全部 relation，再销毁 replacement Block。若 Node 仍 active 或
public interface 不一致，替换会直接失败，不会按位置猜测适配。

`Remove` 改变的是可恢复的 graph membership，不是销毁对象。Patch 保留同一个 Node
和所有关联 Link；Link 在停放期间与原 endpoint 断开，关闭 Patch 时再恢复原 endpoint
与 delay。这样，仍在运行的兄弟 Block 就无法从自己的 output port 遍历一条已经不属于
graph 的 Link。应用 Remove 时，目标 Node、它的 control port 以及每条关联 Link 的
source 都必须 idle；
非目标 sink 已经 active 时不再依赖这条 Link。零售 SDK 不公开 CK2 delayed list 的
成员关系；在一次执行边界上，不在该 list 中的 Link，其 remaining delay
只会是 0 或 initial delay，因此其他正数状态一律按 in-flight 处理并拒绝。即使 graph
通过不带 reset 的方式被 deactivate，这条规则仍成立，因为该操作不会清空 delayed
list。graph 内无关的工作可以保持 active。Node edit 与已有 Link overlay 互斥，并在
关闭前拒绝后续 Patch，避免 overlay 的物理 chain 穿过已经移出的 Node。

使用 `On(graph, edit)` 将多个当前 world 的 graph 和 Edit 组合起来。Session 为整个
功能返回一个 Patch：

```cpp
auto patch = session.Apply(
    "overclock",
    On(deactivateGraph, deactivateEdit),
    On(newBallGraph, newBallEdit),
    On(energyGraph, energyEdit));
```

`On(...)` 只负责在这些调用中连接目标和 Edit，不引入新的公开 graph 或 patch 类型。

第一个 graph 改变前，所有 target 都会完成解析和静态检查。随后按参数顺序提交；
后面的 target 失败时，前面的 target 按逆序恢复。同一个 graph 不能在顶层出现两次。
任一 exact target 被删除时，整个 Patch 退役，其余仍存在的 graph 会在下一个
Behavior safe point 恢复。`Graph::Apply` 和 `Script::Apply` 是这项操作的单 graph
便利入口。

一个 Plan 可以拥有多条相互独立的 Script rule：

```cpp
auto plan = session.Plan(
    "game-events",
    On(Scripts::One("Event_handler"), eventEdit),
    On(Scripts::One("Gameplay_Ingame"), gameplayEdit),
    On(Scripts::One("Gameplay_Energy"), energyEdit));
```

每条 rule 只在对应 Script 名称发生变化时 reconcile，不会逐帧扫描全部 graph。
rule 可选择 `One` 或 `Each`；其 root 与 nested scope 作为一个原子 Patch 安装。
`Partial` 表示至少一条 rule 已安装、但仍有 rule 没有匹配；`Unsatisfied` 表示当前
没有任何安装。定义会跨 world reset 保留，并在下一 world 重新 reconcile。

Patch 和 Plan 都提供 `Enable()`、`Disable()`、`Replace(...)`、`Info()` 与
`Close()`。Disable 恢复 native graph，但保留 handle 和 owned definition；Enable
重新验证后再安装。Replace 保留未变化的前缀，先逆序恢复变化后的旧后缀，再安装
新后缀。新内容失败时恢复上一份完整定义；若连旧定义也因外部冲突无法恢复，
`Info()` 报告 `Conflicted` 并保留 journal。safe point 前的多次请求以最后一份
definition 和最后一个 active state 为准。

Close 会比较 installation 仍然拥有的 Link、source 和 graph after-image；外部修改
导致 `RevertConflict` 时，handle 保持可读，作者修复冲突后可以再次 Close。

Hook callback 在 game thread 执行。异常不会穿过 DLL seam；callback 内 self-close 只关闭后续 admission，graph restore、native teardown 和 callback state release 会在 safe point 完成，不会等待当前 callback。

## 10. 已命名的 retail BB

`BML/Behavior/Blocks.hpp` 汇总 BML+ 已知的 header-only adapter，例如 Object Load、Physicalize、Physics Force、Physics Impulse、Physics Wake Up、Send Message 和 2D Text。每个 header 只封装该 BB 的 Prototype、Options 和 slot knowledge：

```cpp
#include <BML/Behavior/Blocks/Text2D.hpp>

Blocks::Text2D::Options options;
options.Text = "score";
options.FontIndex = 2;

auto made = Blocks::Text2D::Make(m_Behavior, options);
if (made) {
    auto text = made.Take().SpawnIn(graph);
}
```

这些 adapter 仍返回普通 `Block`，不会绕过统一的 lifecycle、execution 或 teardown。新 Native Mod 不应再使用 legacy ExecuteBB interface。

## 11. 生命周期、错误与性能

| 事件 | Session | Run | Script | Watch / Patch | Plan |
| --- | --- | --- | --- | --- | --- |
| 显式 Close | 最后一个 lease 关闭 native Session | 停止 admission，safe point 完成 teardown | 先关闭初始 graph，再在 safe point deactivate、离开 owner，然后销毁 | closure/conflict 期间仍可读 | rule 退役期间仍可读 |
| world reset | 保持有效 | 关闭 | 随旧 world 关闭 | 随旧 graph 关闭 | 保持有效，在新 world reconcile |
| Mod unload/reload | owner generation 退出 | DLL unload 前关闭 | 离开 owner，并在 DLL unload 前关闭 | callback 和 graph state 先退役 | callback code unload 前退役 |

除 Close 外，Behavior 操作要求 game thread。所有 `Result<T>` 都同时包含稳定错误类别和 `Status`；控制流只应判断 error/phase，不应解析 message 文本。

高频路径应复用 `Block`、`Frames` 和已有 graph snapshot。Block 会共享已编译的 C descriptor；`TakeFrames(Frames&)` 在容量足够时避免额外分配；Node、Port、Link、ParameterOperation、LinkRange 和 Frame 都是 view，不复制 record 或 string。Plan 只处理 Loader 报告为已变化的 Script 名称；disabled definition 和 Replace 中未变化的前缀不会重建 native graph。

当前公开 interface 直接暴露 native Parameter Operation，但不在其上另造一套 expression language；它尚不包含 AngelScript Behavior projection 或第三方 parameter format registration。缺少这些能力时会明确返回 unavailable/unsupported，不会把未知 Virtools parameter 当作任意 bytes 复制。

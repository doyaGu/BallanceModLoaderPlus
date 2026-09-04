# Behavior 编写

`BML/Behavior.hpp` 是原生 C++ Mod 使用和修改 Virtools Behavior 的接口；
`BML/Behavior.h` 是它下面的 C ABI。C++ 层直接表达 Virtools 对象模型，不把 wire
结构暴露给作者：

```text
Prototype -> configured Block -> Call / Task / Instance -> Frames
```

- `Prototype` 表示一个已注册的 Building Block 实现。
- `Block` 是可复制的配置值，此时还没有创建 `CKBehavior`。
- `Call`、`Task`、`Instance` 各自拥有一个 live `CKBehavior`。
- `Frames` 持有 native Execute 后复制出的结果。
- `Node` 表示 graph snapshot 中已经存在的 Behavior。

## 打开 Session

在 Mod 初始化阶段打开一个 Session。Loader 会核对调用 DLL，并把它绑定到该 Mod
的当前 generation。可选 owner id 只能确认身份，不能冒充另一个 Mod。

```cpp
using namespace BML::Behavior;

auto opened = Session::Open();
if (!opened) {
    GetLogger()->Error("Behavior unavailable: %s",
                       opened.GetStatus().Message.c_str());
    return;
}
m_Behavior = std::move(opened).Value();
```

通过 Session 创建的对象会保留自己所需的底层 Session。移动或关闭最初的
`Session` 值不会使仍存活的 Block、run、Watch、Patch 或 Plan 失效；最后一个持有者
才会关闭 native Session。

Session 跨 world reset 保持有效，run 与绑定当前 world 的 graph 对象不会。Mod
generation 退出后，所属对象全部 stale。

## 配置 Block

`Session::Use` 直接返回 `Block`。配置方法修改当前值，并返回自身以便链式调用：

```cpp
auto block = m_Behavior.Use(prototype)
    .TargetOwner()
    .Settings({{"Mode", 2}, {"Detail", 4}})
    .Settings({{"Created Later", 9}})
    .Pins({{"Strength", 12.0f}})
    .Locals({{"Accumulator", 0.0f}})
    .Frames(Signals(64));

auto checked = block.Validate();       // 可选
auto call = block.Call("Run");          // 这里也会自动完成同样的验证
```

每次 `Settings({...})` 调用就是一个 Setting stage。Virtools 会在一个 stage 后发送
`SETTINGSEDITED`，BB 可以在 callback 中重建 layout，因此下一 stage 必须在新 layout
上重新解析。所有 Setting stage 完成后才应用 Pin 和 Local。

`Block` 使用 copy-on-write。副本在未修改时共享配置和已验证的 wire 表示；修改某个
副本的 `Target`、`Settings`、`Pins`、`Locals` 或 `Frames`，只会让该副本的缓存失效。
首次验证成功时会固定 Prototype provider generation；provider 被替换后，旧 Block
会明确 stale，不会悄悄换成另一份 native 实现。

selector 必须明确：`At(index)`、`Named(name, occurrence)` 或 `Unique(name)`。
unique selector 遇到重名会失败，不会选择第一个。

## 选择由谁驱动 Execute

`Call`、`Task`、`Instance` 是三个独立的 move-only 类型。它们都只拥有一个 native
Behavior，区别只是后续 native Execute 由谁推进：

- `Call` 只 Execute 一次；若仍有 native continuation，把它移动到 `Continue()`，
  Loader 会继续托管同一个 Instance。
- `Start` 首次 Execute 后返回 `Task`，后续 native continuation 由 Loader 按 game
  frame 推进。
- `Spawn` 创建 idle `Instance`，Mod 用 `Pulse` 驱动。
- `SpawnIn(graph)` 创建同样的受控 Instance，但把它作为未连接节点放入 live graph；
  Close 时会从 graph 中移除。

```cpp
auto once = block.Call("Run");
auto task = block.Start("Run", Latest());
auto instance = block.Spawn();
auto parked = block.SpawnIn(graph);

if (instance)
    instance->Pulse("Reset");
```

传给 `Call`、`Start`、`Spawn` 或 `SpawnIn` 的可选 policy 只覆盖本次 run，不会修改
Block 的默认 Frame policy。

同一 Instance 在一个 game frame 内最多 Execute 一次。同 frame 或重入的 Pulse 会
排队；同一个 logical In 的重复 admission 会合并，不同 In 保持首次 admission 顺序。
`Ready` 只表示没有 native continuation 和 queued In，不表示 BB 已释放 Local 或
manager 中的状态。

三种 run 都提供 `Info`、`Take`、`Layout`、`Inspect`、`Set`、`Bind`、`Settings` 和
`Close`。只有 `Call` 提供 `Continue`；只有 `Task` 和 `Instance` 提供 `Pulse`。live
`Slot` 带有 layout generation；Setting 或 callback 改变 layout 后，旧 Slot 会失败。

## 取得 Frames

每次 native Execute 都会在清除 active Out 前形成一个 immutable Frame，记录
sequence、game frame、native return code、continuation、active Out、已复制的 Pout
和 diagnostic。object Pout 在对象仍 live 时签发 `ObjectRef`；之后读取它不会再次访问
原参数或 CK object。

```cpp
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

`Take()` 是方便的自动分配版本。`Take(Frames&)` 优先复用已有 header/payload 容量；
容量足够时只调用一次 C seam，不分配 record 或 string。`Frame`、`Out`、`Pout` 都是
所属 `Frames` 的轻量只读 view；任何对该 `Frames` 的修改都会使这些 view 失效。只有
整个 wire batch 通过验证后，才会产生公开 view。

Frame policy 的含义是：

- `Signals(n)`：首次 Execute、产生信号的 Execute、失败和 terminal Frame。
- `EachFrame(n)`：每次 Execute。
- `Latest()`：最新 continuing、failure、terminal Frame；sequence 不同时分别保留。
- `Ignore()`：不保留普通 Frame，但 failure 与 terminal 状态仍可见。

有界存储不会为新 Frame 丢弃旧 Frame。容量满时会停止 run，并在独立 terminal slot
记录 `FrameQueueFull`。Take 后会释放普通容量。

## 读取 graph

`Session::Inspect` 和 run 的 `Inspect` 返回 immutable graph snapshot。
`Graph::Root()` 返回 `Node`；Node 直接提供 `In`、`Out`、`Pin`、`Pout`、`Setting`、
`Local` 和 `Target` port。

```cpp
auto snapshot = m_Behavior.Inspect(script);
auto counter = snapshot->Find("Counter_Active");
if (counter) {
    auto value = snapshot->Read(counter->Pout("Count"));
}
```

`Logical()` 重新读取作者视图，`Live()` 重新读取物理 CK graph。Logical 保留显式 Block
和 Link，但会隐藏 Tap/Before/After 所属的精确 HookBlock 与 continuation Link，并把
splice anchor 恢复为 logical endpoint。若受 Patch 管理的基础设施被外部修改，则返回
`GraphChanged`，不会猜测结果。

Node name 不是 identity。`FindAll` 返回全部匹配；`Find` 要求恰好一个。参数读取会跟随
stored、direct、shared source，但不会为了取值执行 Parameter Operation。

字符串重载使用 unique-name selector：当多个 Pout 同名时，`node.Pout("Count")` 会保持
未解析，由后续操作报告歧义，而不是擅自选择第一项。需要指定同名 occurrence 时使用
`Named("Count", n)`；需要当前 layout 中的稳定 index 时使用 `At(n)`。

Watch 每个 game frame 采样一次 graph、layout 或 value：

```cpp
auto watch = snapshot->Watch(GraphChanged{}, [](const Change &change) {
    OnGraphChanged(change);
});
```

用 `Info()` 读取 Watch 状态。观察失败或 callback 失败后，Watch 留在 `Failed`，保留
首个 `Status`，并停止后续 callback。

## 用一个 Edit 表达 graph 修改

`Edit` 是唯一的 symbolic graph transformation。同一 Edit 既能应用到一个精确
snapshot，也能作为跨 world Plan 保留：

```cpp
Edit edit;
auto source = edit.Require("Counter_Active");
auto added = edit.Add(block);
edit.Flow(source.Out(), added.In());

auto patch = graph.Apply("extra-life", edit);
auto plan = m_Behavior.Plan(
    "extra-life", Scripts::One("Gameplay_Events"), edit);
```

`Edit::Node`、`Edit::Port`、`Edit::Link`、`Edit::Path` 是 symbolic value，与 snapshot
的 `Node`、`Port`、`Link` 有意分开。`Use(snapshotNode)` 和 `Use(snapshotLink)` 可把
精确 live identity 引入一次性 Patch。Plan 必须在未来 world 的新 script 中重新解析，
因此会拒绝这些 world-bound identity。

`Add(block)` 在调用时复制 Block 的 native 配置和已选定的 Prototype provider
generation。之后修改原 Block 不会影响 Edit，后续 installation 也不能悄悄换用另一份
provider。Block 的 Frame policy 不属于 graph authoring。typed null 是 durable literal；
非空 object reference 仍然绑定当前 world，因此 Plan 会拒绝。`Graph::Apply` 在 mutation
前核对 snapshot fingerprint，并返回一次性 `Patch`。`Session::Plan` 接受
`Scripts::Each(name)` 或 `Scripts::One(name)`，返回随匹配 script 出现、reset、删除而
reconcile 的 `Plan`。

Patch 和 Plan 都用 `Info()` 与 `Close()`。Close 会执行带检查的 inverse；若有外部改动，
返回 `RevertConflict` 并保持 handle 可读，作者修复冲突后可以再次 Close。

Hook、splice、redirect、dynamic port、data relation 和 ordering 都是 `Edit` 的方法。
Hook callback 在 game thread 执行。异常不会越过 DLL seam；Close 会立即停止新的
callback admission，graph restore、native teardown 则在 Behavior safe point 完成，
不会等待发起 Close 的当前 callback。

## 已命名的零售 Building Block

`BML/Behavior/Blocks.hpp` 汇总一组 header-only retail BB adapter：Object Load、
Physicalize、Physics Force、Physics Impulse、Physics Wake Up、Send Message 和 2D Text。
每个独立 header 都定义 `Options`，以及 `Make(Session, Options) -> Result<Block>`。

```cpp
#include <BML/Behavior/Blocks/Text2D.hpp>

Blocks::Text2D::Options options;
options.Text = "score";
options.FontIndex = 2;

auto made = Blocks::Text2D::Make(m_Behavior, options);
if (made) {
    auto text = std::move(made).Value().SpawnIn(graph);
}
```

adapter 只保存对应 BB 的 Prototype 和参数知识。创建、lifecycle、execution、teardown
仍与任意 `Session::Use` Block 一样经过 Behavior Runtime。新的 Native Mod 不应再使用
legacy ExecuteBB API。

## 生命周期摘要

| 事件 | Session | Run | Watch/Patch | Plan |
| --- | --- | --- | --- | --- |
| 显式 Close | 最后一个持有者关闭 native Session | 关闭 admission；teardown 可能在 safe point 完成 | closure 或 conflict 未结束时仍可读 | installation 退出期间仍可读 |
| world reset | 保持有效 | 关闭 | 随旧 graph 关闭 | 保持有效，在新 world reconcile |
| Mod unload/reload | owner generation 退出 | DLL unload 前关闭 | callback 与 graph 状态在 DLL unload 前退出 | callback code unload 前退出 |

除 Close 请求外，Behavior 操作都要求 game thread。不要在调用结束后保存 callback 中的
borrowed pointer 或 C descriptor。

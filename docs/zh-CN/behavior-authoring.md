# Behavior 编写

`BML/Behavior.hpp` 是原生 C++ Mod 编写 Virtools Building Block 的主要接口；
`BML/Behavior.h` 是供其他语言和工具链使用的稳定 C seam。两者表达的是同一组
Session、Prototype、Layout、Block、Run、Frame、Graph、Watch、Patch 和 Plan。

## 从 Session 开始

在 Mod 初始化阶段打开一个 Session。Loader 会核对调用 DLL，并把 Session 绑定到
该 Mod 的当前 generation；可选 `ownerId` 只能用于确认身份，不能冒充另一个 Mod。

```cpp
auto opened = BML::Behavior::Session::Open();
if (!opened) {
    GetLogger()->Error("Behavior unavailable: %s",
                       opened.Detail().Message.c_str());
    return;
}
m_Behavior = std::move(opened).Value();
```

从 Session 创建的值共同持有底层 Session。释放最初的 `Session` 值，不会让仍存活的
Block、Run、Watch、Patch 或 Plan 失效；最后一个持有者才关闭底层 Session。

Prototype 发现和 declared Layout 读取不会创建 live `CKBehavior`。发现结果带有
provider generation；把它保存在交给 `Use` 的 `Prototype` 中，provider 重载后就会
明确失败，而不是把旧定义静默绑定到新的 DLL。

## 编译可复用的 Block

Builder 描述 Target、Setting stage、Pin、Local 和 Frame policy。Setting 必须分 stage，
因为一次 `SETTINGSEDITED` callback 可能重建 native Layout，后续值必须在新 Layout 上
重新解析。

```cpp
using namespace BML::Behavior;

auto compiled = m_Behavior.Use(Prototype(myPrototype, providerGeneration))
    .TargetOwner()
    .Setting("Mode", 2)
    .NextStage()
    .Setting("Detail", 4)
    .Pin("Strength", 12.0f)
    .Frames(signals(64))
    .Compile();
```

Builder 适合一次性调用。会重复或逐帧使用时，应在初始化阶段调用一次 `Compile()`，
保存 immutable Block。编译后的 Block 自己持有字符串和值，复用同一份 C descriptor，
不会为每个 Run 重新发现 declared Layout。

selector 必须明确：可以使用 index、name + occurrence 或 unique name。unique-name
selector 遇到重名会失败，绝不擅自选择第一个。live Layout 的 Slot 还携带 Layout
generation；Setting 或 native callback 改变 Layout 后，旧 Slot 会明确 stale。

## 根据 Behavior 选择 Run

- `Call` 只 Execute 一次；若 native continuation 仍存在，必须显式把它移动进
  `Continue()`，并继续使用同一个 native Instance。
- `Start` 首次 Execute 后由 Loader 在后续 game frame 推进 native continuation。
- `Spawn` 创建 idle Instance，Mod 按需用 `Pulse` 提供 logical In。

同一 Instance 每个 game frame 最多 Execute 一次。同 frame 的第二次或重入 Pulse 会
排队；同一个 logical In 的重复 Pulse 会合并，不同 In 保留首次进入顺序。`Ready`
只表示没有 native 或 queued continuation，不表示 BB 已经释放 Local 中的状态或从
manager 注销；Run 会一直持有 native Instance，直到显式关闭或当前 world 结束。

native Execute 的错误写入 Frame。若在安全 Execute 之前就失败，例如 selector 无效、
类型不兼容、Layout stale 或 Prototype 不可用，则返回失败的 `Result`，不会创建 Run。

## 读取 Frame

每次 native Execute 都会在清除 active Out 之前形成一个 immutable Frame。Frame 包含
sequence、game frame、native result、continuation、active Out、已复制的 Pout 值和
diagnostic。object Pout 在对象仍 live 时签发 `BML_ObjectRef`；之后读取 Frame 不会再
访问原 CK object。

四种 policy 的准确保留语义如下：

- `signals(n)`：保留首次 Execute、带 active Out 的 Execute、失败以及不再 continuation
  的 Frame，普通容量为 `n`。
- `eachFrame(n)`：保留每次 Execute，普通容量为 `n`。
- `latest()`：分别保留最新 continuing Frame、最新失败和不再 continuation 的 Frame；
  三者 sequence 不同时可能同时读到。
- `ignore()`：忽略普通 Frame，但仍保留失败与不再 continuation 的 Frame，执行错误不会
  静默消失。

有界存储不会为新 Frame 丢掉旧 Frame。容量满时会在独立 terminal slot 写入
`FrameQueueFull` 并停止 Run。需要完整序列时及时 Take；不需要每个中间值时使用
`latest()` 或 `ignore()`。

C 的 `TakeFrames` 使用不消费的两阶段协议：第一次只测量完整 header 与 payload；容量
不足时只写 required count/size，不写半条 Frame，也不消费。第二次成功时按 sequence
顺序复制完整 batch，并原子消费那一批。C++ 的 `Take()` 负责持有和解码返回数据。

## 检查和观察行为图

`Inspect` 返回不含 CK pointer 的 owned Logical 或 Live Graph。Node name 不是 identity，
而且可以重名；应枚举全部匹配，或请求 unique match 并显式处理歧义。Parameter 读取
沿 stored/direct/shared source 取值，但不会求值 Parameter Operation。

Watch 每个 game frame 采样一次 graph、Layout 或 value。portable CK2.1 没有可靠的
exact parameter-data notification seam，因此公开接口只提供 sampled value change。
读取源或调用作者 callback 失败后，Watch 进入 `Failed`、停止 polling，并保留第一个
diagnostic，直到作者读取并关闭；它不会再静默消失。

## Patch 一个 Graph，或维护一个 Plan

Patch 修改一个确定的 live Graph，并持有精确恢复所需的 journal。Plan 保存针对 exact
script name 的 symbolic intent，world 或 script instance 变化后重新建立新的 Patch
installation。两类 Builder 有意分开：Patch 以 `Apply` 结束；Plan 还要描述 target，
并以 `Submit` 结束。

Graph edit 使用明确的 Node、Port、Link 与 Path query。重名、歧义 Path、跨 Graph
引用、未经确认的 same-frame cycle、source conflict 和 ordering cycle 都会在 native
mutation 前失败。关闭时会把 live graph 与 Patch after-image 对比；foreign edit 会产生
`RevertConflict`，作者应恢复期望关系后重试，Loader 不会强行执行破坏性 inverse。

Hook 与 Watch callback 在 game thread 运行。C++ thunk 会在异常跨越 C/DLL seam 前捕获
全部异常，并报告 callback failure。callback 内或其他线程都可以请求 Close：新的 callback
admission 立即停止，graph 恢复、native teardown 与 callback Release 则在后续 game-thread
Behavior safe point 完成；Close 永远不会等待自己所在的 callback。

## 生命周期

| 事件 | Session | Run | Watch | Graph Patch | Plan |
| --- | --- | --- | --- | --- | --- |
| 显式 Close | 最后一个 facade owner 释放后关闭 | 立即 stale；必要时延后 native teardown | 立即 stale；callback state 在 safe point 退休 | restore pending/conflicted 时仍可读 | retirement pending/conflicted 时仍可读 |
| world reset | 保留 | 关闭 | 关闭 | 随旧 Graph 关闭 | 保留，并在新 world 重新 reconcile |
| Mod unload/reload | owner generation 退休，全部 handle stale | DLL 卸载前关闭 | callback 代码卸载前关闭 | 先恢复；无法立即完成时由 Loader 持有到安全点 | callback 代码卸载前退休 |

除 Close 请求外，所有操作都要求 game thread。callback 或 C descriptor 中的 borrowed
pointer 只在本次调用期间有效，不能保存。

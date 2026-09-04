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
Live 是物理 CK graph；Logical 是作者实际编辑的 graph：显式添加的 Block 与 Link 仍然
可见，Loader 会把 splice anchor 恢复为原始 endpoint 与 delay，并隐藏它精确记录的
continuation Link 和 Tap/Before/After HookBlock。Redirect 是唯一的例外：它是有意改变
控制流去向，因此 Logical 报告新的目标。如果这些由 Patch 占有的物理关系被外部改写，
Logical inspection 会返回 `GraphChanged`，不会靠名称或图形状猜测。Node name 不是
identity，而且可以重名；应枚举全部匹配，或请求 unique match 并显式处理歧义。
Parameter 读取沿 stored/direct/shared source 取值，但不会求值 Parameter Operation。

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
全部异常。抛出异常的 Hook callback 报告为 `HookResult::Fault`：Loader 保留第一个 fault
作为 Hook 诊断，停止再调用该 callback，Hook Block 则透明放行这次激活，因此 Mod 的 bug
不会停掉宿主脚本的链。返回 `HookResult::Error` 是显式决定，会让所有 Out 保持未激活，
callback 仍然保留安装。callback 内或其他线程都可以请求 Close：新的 callback
admission 立即停止，graph 恢复、native teardown 与 callback Release 则在后续 game-thread
Behavior safe point 完成；Close 永远不会等待自己所在的 callback。这一延后规则同样适用于
Loader 自己的逆操作内部：native teardown 或 EDITED callback 若再次关闭正在拆除的 Patch，
会得到 `Busy`；若关闭或应用另一个 Patch，该请求会排队到下一个 safe point，而不会嵌套在
正在进行的恢复之下。

## 指名 Graph 中已存在的对象

`Reference` 把 `CK_ID` 或活的 `CKObject *` 转成 object reference，而不会把 pointer
交还给作者；`Inspect(CKBehavior *)` 直接在 Mod 已经持有的 Behavior 上打开 Graph
视图。之后 Patch 用 `UseNode` 与 `UseLink` 直接指名这些对象，不必再按名字查询，这
正是 Mod 编辑自己创建的 Graph 时需要的方式。Plan 会拒绝 identity reference：durable
Plan 要装进尚不存在的 script，一个针对某个 live world 发出的 reference 无法描述它们；
在那里应改用名字或 Prototype 查询。

`Apply` 之后，`Resolve` 报告 program handle 实际编译成的 live object，作者因此可以
读回 Patch 刚刚添加的 Block：

```cpp
auto edit = m_Behavior.Patch("extra-life");
const auto counter = edit.Require("Counter_Active");
const auto added = edit.Add(CKGUID(0x3333, 3),
                            {pin("Amount", 1), setting("Mode", 2)});
edit.Flow(counter.Out(), added.In());

auto applied = edit.Apply(graph);
const auto block = applied.Value().Resolve(added);
```

`Add` 在同一条语句里接收 literal，每个 literal 会推导出该 parameter 将要持有的
Virtools 类型：任意宽度、任意符号的整数和 enumerator 变成 Int，`float` 与 `double`
变成 Float。若某个 slot 想用同样的 bit 但不同的 Virtools 类型，写
`Value::As(type, literal)`。

写入的值可以落到 Pin、Local 或 Target。CK2 把这种值保存在 parameter 自己的 buffer 里，
Local 持有状态正是这种方式，所以 `local(...)` literal 就是一次写入，不需要来源。
Setting 不是后续写入可以指名的目标：编辑一个 Setting 可能让 Block 重建整个 layout，
因此 Setting 只能声明在同一个 program 添加的 Block 上，并随该 Block 的创建一起生效，
Block 在那里收到一次 settings-edited 消息并重新获取一次 layout。给已经存在的 Block
指名 Setting 会被拒绝；在同一个 Patch 里既给某个 parameter 写值又对它 `Push`
同样会被拒绝。

## 在 Link 上挂代码与改道

三个原语覆盖了 Mod 插入他人写的链的方式：

- `Tap(port, hook)` 在离开某个 Out 的每条 Link 上调用 callback。
- `Before(link, hook)` 在一条 Link 内部、在该 Link 指向的 node 之前调用 callback。
- `After(path, hook)` 在离开某个 Port 的链执行完毕后调用一次 callback。

三者都是基础设施：它们的 Hook Block 与 continuation Link 不出现在 Logical 视图里，
因此其他 Mod 检查该 Graph 时看到的仍是原作者写下的形状。

`Splice(link, block)` 把一条 Link 改道穿过一个 Block，并把原目标保留在插入链的末端，
所以多个 Patch 可以 splice 同一条 Link，由 Loader 排序。`Redirect(link, port)` 是相反
的选择：它把 Link 送去别处，并在 Patch 打开期间丢弃原目标。同一条 Link 同时只允许一个
Patch redirect，第二个会以 `RedirectConflict` 被拒绝，而不是悄悄丢掉一个目标。关闭
Patch 会把 Link 放回 journal 记录的目标；如果该目标被外部改写，则报告
`RevertConflict`。

Patch 可以给任何 Behavior 追加 In、Out、Pin 或 Pout。CK2 追加这些 port 时并不检查
variable-interface flag，Loader 也不检查：这些 flag 说明的是该 Block 自己的代码会不会
读取新 slot，与 CK2 是否允许追加是两个不同的问题。Local 是例外：Block 按 index 把
Local 当作私有状态使用，因此追加 Local 会以 `InterfaceUnsupported` 被拒绝。

## 把 Block 停放进 Graph

`Spawn` 创建由 Mod 自己代码驱动、位于任何 script 之外的 Instance。`Attach` 则把 Block
停放进一个 live Graph，且不连接任何 Link：Graph 永远不会激活一个停放的 Block，所以写
它的 Pin 和 pulse 它的仍然是返回的 Instance。需要存在于 script 中、随 script 保存、
或能被按名字找到的 Block，由此仍然处在 Mod 控制之下。关闭 Instance 会把该 Block 从
Graph 中移除。

跨帧自续（`CKBR_ACTIVATENEXTFRAME`）的 Block 也不例外。Ballanced 会调度所有 active
的 sub-behavior——无论有没有 Link 接到它——因此每次驱动执行之后，Loader 都会在 Run
记下延续的同时清掉 Block 的原生 active flag：Instance 始终是唯一的驱动者，Graph 自己
的调度器永远不会接管这个 Block。

```cpp
auto parked = m_Behavior.Use(textPrototype)
    .Setting("Font", 0)
    .Attach(graph);
parked.Value().Pulse("In");
```

## 生命周期

| 事件 | Session | Run | Watch | Graph Patch | Plan |
| --- | --- | --- | --- | --- | --- |
| 显式 Close | 最后一个 facade owner 释放后关闭 | 立即 stale；必要时延后 native teardown | 立即 stale；callback state 在 safe point 退休 | restore pending/conflicted 时仍可读 | retirement pending/conflicted 时仍可读 |
| world reset | 保留 | 关闭 | 关闭 | 随旧 Graph 关闭 | 保留，并在新 world 重新 reconcile |
| Mod unload/reload | owner generation 退休，全部 handle stale | DLL 卸载前关闭 | callback 代码卸载前关闭 | 先恢复；无法立即完成时由 Loader 持有到安全点 | callback 代码卸载前退休 |

除 Close 请求外，所有操作都要求 game thread。callback 或 C descriptor 中的 borrowed
pointer 只在本次调用期间有效，不能保存。

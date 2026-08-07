# 跨 Mod 通信（IMC）

IMC 是 BML+ 为 Mod 及 Loader 服务提供的类型化进程内通信机制，包含两种操作：

- RPC：请求/响应调用；
- Topic：发布/订阅通知。

当两个独立构建的原生模块需要稳定接口，又不能共享 C++ 对象、STL 容器、
分配器或 Virtools 指针时，应使用 IMC。完整示例参见
[创建类型化 IMC API](imc-author-guide.md)。

## 编程模型

IMC 将传输机制与业务术语分开：

| API 意图 | IMC 表达方式 |
| --- | --- |
| 读取当前状态 | 无请求载荷的 RPC |
| 查找对象 | 接收不透明 `BML_ObjectRef` 的 RPC |
| 执行查询 | 请求/响应 RPC |
| 执行命令 | 带显式结果的请求/响应 RPC |
| 返回集合 | 响应中包含计数数组的 RPC |
| 通知观察者 | Topic |

资源、组件、命令和集合属于 API 设计模式，不是额外的传输类型。保持传输层精简，
可以让线程、生命周期和错误处理在所有接口中保持一致。

## 推荐工作流

1. 在带版本的 `.imc` 文件中定义 Record、RPC 和 Topic。
2. 首次执行 `--update-lock`，生成并提交相邻的 `.imc.lock`。
3. 使用 `bml_target_imc_api()` 将接口加入 CMake 目标。
4. 实现生成的 `Provider` 回调。
5. 通过生成的 `Client` 调用接口。
6. 在关闭或释放成功前，保持 Provider、Client、Subscription、Future 和回调数据存活。

生成的绑定是常规使用入口。它们负责编解码和校验载荷、缓存路由 ID、管理不透明
句柄并返回 BML 状态码。大多数 Mod 不应手工构造 `BML_ImcMessage`。

## 公开 API 层次

IMC 有三层公开接口：

| 层次 | 用途 |
| --- | --- |
| 生成的 `*_imc.hpp` | 类型化载荷、编解码器、Client、Provider、Future 和 Subscription |
| `BML/ImcCpp.hpp` | 面向自定义集成的通用 C++ RAII 包装 |
| `BML/Imc.h` | 跨 DLL 使用的固定布局 C ABI |

C ABI 只导出 `BML_Imc_*` 函数，并只使用 C 标量、固定布局结构体、字节区间、
回调和不透明句柄。C++ 类、异常、RTTI 对象和由分配器持有的值不会跨模块边界。

`BML/ImcWire.hpp` 定义生成绑定使用的小端字段编码。回调只能在本次回调期间借用
`BML_ImcMessage` 的字节；生成的解码器会把字符串、数组和 Blob 复制到类型化结果。
消息本身已经携带载荷类型，因此载荷不会重复保存 Schema ID 或描述哈希。字段使用
类似 Protobuf 的 Varint Tag，将永久字段 ID 和物理线类型组合起来。定长标量没有
冗余长度，只有字符串、复合值和打包数组使用长度分隔。

## 接口身份与兼容性

API ID 使用小写字母和数字组成的点分段形式，例如 `example.echo`。对应的生成头为
`example_echo_imc.hpp`，C++ 命名空间为
`BML::Imc::Generated::Example::Echo`。

作者不在 `.imc` 中手写字段编号。生成器把永久字段 ID 分配到相邻的 `.imc.lock`，
该文件必须随接口源码一起提交。接口发布后：

- 不要修改现有字段的类型或 required/optional 状态；
- 新字段必须是 optional；
- 保留现有 RPC、Topic 名称和载荷 Record；
- 保留 Enum 名称、数值和底层类型。

兼容的小版本更新需要增加接口 minor 版本，并执行一次 `--update-lock`。生成器会校验
演进规则、保留现有 ID，并为删除的 optional 字段保留墓碑。普通构建只校验 Lock。
未知字段会被跳过，未知 Enum 数值会被保留，由具体 API 决定回退行为。

不能遵守这些规则时增加 major 版本。运行时路由 ID 只是进程内缓存键，不能作为兼容
身份；真正决定互操作的是 API ID、major 版本、端点载荷类型和冻结后的字段布局。

## RPC 执行与等待

每个 RPC Provider 都要选择执行方式：

- `BML_IMC_EXECUTION_CALLER_THREAD`：立即在调用线程执行，只适用于短小且线程安全的工作；
- `BML_IMC_EXECUTION_GAME_THREAD`：排入 BML 游戏线程 Pump，适用于 Virtools 对象、
  BML UI 和其他仅限游戏线程的状态。

同步生成调用会在指定超时时间内等待类型化结果。生成的 `Begin*` 方法返回只可移动的
类型化 Future，可用于轮询、取消或有界等待。

不要在游戏线程上用非零超时等待游戏线程任务。零超时等待是安全轮询；任务尚未完成时
返回 `BML_ERROR_BUSY`。

同一个 RPC 名称最多只有一个存活 Provider。即使先做了可用性检查，也必须处理
`BML_ERROR_IMC_ENDPOINT_NOT_FOUND`，因为 Provider 随时可能卸载。

## Topic 投递与背压

一个 Topic 可以有任意数量的订阅者。调用线程订阅会内联执行；游戏线程订阅使用每个
Subscription 独立的有界队列。

订阅时选择溢出策略：

- `BML_IMC_BACKPRESSURE_DROP_OLDEST`：保留较新的消息；
- `BML_IMC_BACKPRESSURE_DROP_NEWEST`：保留已经排队的消息；
- `BML_IMC_BACKPRESSURE_FAIL`：向发布者返回 `BML_ERROR_WOULD_BLOCK`。

容量应匹配消费者排空队列的速度。通过 Subscription 的丢弃计数检测持续过载。构造
事件载荷代价较高时，可先查询订阅者数量。

## 所有权与关闭顺序

Client 和 Provider 都关联一个 Mod Owner。BML 会在 Mod 卸载时撤销其 IMC 状态，
但正常流程仍应显式关闭：

1. 停止发起新调用或发布新消息；
2. 取消或释放未完成的 Future；
3. 在销毁回调数据前关闭 Subscription；
4. 注销 Provider 回调；
5. 关闭 Provider 和 Client。

关闭 Client、注销 Provider 或关闭 Subscription 都是回调静止边界。从当前正在执行
的回调中递归执行同一关闭操作会返回 `BML_ERROR_BUSY`，而不是死锁。

不透明句柄在成功释放后立即失效。`BML_ERROR_INVALID_HANDLE` 表示过期 Owner 或重复
释放等编程错误，不是可恢复的路由失败。

## 内置 API

BML+ 提供以下类型化服务门面：

| 命名空间 | 服务 |
| --- | --- |
| `BML::Runtime` | 运行状态、时钟和分数 |
| `BML::Scene` | 对象信息、变换和查找 |
| `BML::Gameplay` | 关卡、能量、检查点和重置点数据 |
| `BML::UI` | HUD 状态和 UI 命令 |
| `BML::Speedrun` | 共享计时器状态和控制 |
| `BML::Events` | 类型化事件 Topic |

已有门面能覆盖需求时直接使用它。只有能力确实由你的 Mod 拥有时，才创建新接口。

## 性能特征

生成的 Client 只解析一次名称，之后复用数字路由 ID。小载荷使用内联存储；直接调用
线程 RPC 不经过队列；游戏线程任务使用有界队列和每帧 Pump 预算；Topic 发布具有
零订阅者快速路径。

这些优化不会放宽公开规则：回调仍需明确执行线程，队列仍需背压策略，载荷仍需稳定
Record。

## 参考

- [创建类型化 IMC API](imc-author-guide.md)
- C ABI：`BML/Imc.h`
- C++ 包装：`BML/ImcCpp.hpp`
- 线格式编解码：`BML/ImcWire.hpp`

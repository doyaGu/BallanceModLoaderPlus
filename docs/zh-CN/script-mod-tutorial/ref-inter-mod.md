# 跨 Mod 通信

IMC 用于一个 Mod 发布给其他 Mod 的接口，不承载任何 Loader 能力。脚本 Mod
直接使用 Loader 提供的类型化能力接口；IMC Provider 只能由原生 Mod 实现。

## 脚本 Mod 如何选择

脚本中读取 BML 自带能力时，直接使用有类型的接口：

```angelscript
BML::Runtime::State runtime = BML::Runtime::GetState();
if (runtime.InLevel) {
  // 使用 runtime 的复制快照。
}

array<BML::Gameplay::Checkpoint>@ checkpoints;
if (BML::Gameplay::ReadCheckpoints(checkpoints) == BML::ERROR_OK) {
  for (uint i = 0; i < checkpoints.length(); ++i) {
    CKObject@ checkpoint = checkpoints[i].BorrowObject();
  }
}

BML::Events::Stream@ events;
if (BML::Events::Open(events, 256) == BML::ERROR_OK) {
  // 在 OnProcess 中 Poll；用完后 Close。
}
```

可用的内置命名空间包括 `BML::Runtime`、`BML::Gameplay`、`BML::UI`、
`BML::Events` 和 `BML::Speedrun`。Virtools 场景查找和对象标识应使用
CKAngelScript 的 `Scene` 命名空间及其可重新验证的引用类型。其中 Runtime
状态、时钟和分数读取直接返回 Loader 进程内状态的值，不经过任何传输层，
也不要求脚本处理传输状态码。在有效脚本回调之外调用这些函数会触发脚本
异常。Gameplay 读取也直接复用进程内的数据读取器，但对应的 Ballance
数据数组可能尚不可用或布局不受支持，因此仍返回明确的状态码。脚本只处理
类型化数据，不直接管理原始消息或原生 IMC 句柄。`ReadCatalog`、
`ReadCheckpoints` 和 `ReadResetpoints` 返回标准 AngelScript 数组形式的完整
快照；读取失败时输出句柄为 `null`，不需要游标、`Next` 循环或显式 `Close`。
每次调用都会重新读取源数据并创建快照，因此数据稳定时应复用已返回的数组，
不要每帧重复构建。

`BML::Events::Stream` 按 Hook 顺序提供不可变事件快照。仅当 `Poll` 返回
`BML::ERROR_OK` 时读取事件；`BML::ERROR_NOT_FOUND` 表示流当前为空，其他状态
表示流未打开（`BML::ERROR_INVALID_HANDLE`）或复制事件失败
（`BML::ERROR_OUT_OF_MEMORY`）。`GetDroppedCount` 统计队列容量和背压造成的丢失。

两个脚本 Mod 只需交换少量状态时，使用 DataShare。DataShare 适合有明确
类型和所有权的一次性或延迟读取，不应被包装成通用函数调用机制。

## 何时需要原生 IMC Provider

IMC 只用于一个 Mod 发布给其他 Mod 的接口。这类服务满足下面任一条件时，应把它
实现为原生 Mod：

- 需要请求/响应 RPC；
- 需要高频或有背压策略的事件流；
- 需要显式选择 caller thread 或 game thread；
- 需要稳定的跨 DLL ABI，供多个独立 Mod 使用。

原生实现流程是：编写版本化 `.imc` 接口定义，用 `imc_codegen.py` 或
`bml_target_imc_api()` 生成 C++ 绑定，实现生成的 provider，并让消费者
使用生成的 client。不要手写字段编码，也不要跨 DLL 传递 C++ 对象、STL
容器、allocator 所有权或 `CKObject*`。

完整示例与兼容演进规则见：

- [跨 Mod 通信](../imc.md)
- [创建类型化 IMC API](../imc-author-guide.md)

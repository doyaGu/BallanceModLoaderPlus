# 跨 Mod 通信

BML+ 只保留一套 RPC/事件传输：IMC。旧的 `[bml.export]`、`ExportRef`、
`CallFrame` 和 `BML::Interop` Record/Registry 接口已经移除，不存在兼容层。

## 脚本 Mod 如何选择

脚本中读取 BML 自带能力时，直接使用有类型的浅层接口：

```angelscript
BML::Runtime::State runtime;
if (BML::Runtime::ReadState(runtime) == BML::ERROR_OK && runtime.InLevel) {
  // 使用 runtime 的复制快照。
}

BML::Events::Stream@ events;
if (BML::Events::Open(events, 256) == BML::ERROR_OK) {
  // 在 OnProcess 中 Poll；用完后 Close。
}
```

可用的内置命名空间包括 `BML::Runtime`、`BML::Scene`、
`BML::Gameplay`、`BML::UI` 和 `BML::Events`。这些接口内部使用 IMC，
但不会把原始消息、provider 或 subscription 句柄暴露给脚本。

两个脚本 Mod 只需交换少量状态时，使用 DataShare。DataShare 适合有明确
类型和所有权的一次性或延迟读取，不应被包装成通用函数调用机制。

## 何时需要原生 IMC Provider

下面任一条件成立时，应把服务实现为原生 Mod：

- 需要请求/响应 RPC；
- 需要高频或有背压策略的事件流；
- 需要显式选择 caller thread 或 game thread；
- 需要稳定的跨 DLL ABI，供多个独立 Mod 使用。

原生实现流程是：编写版本化 `.imc` 接口定义，用 `imc_codegen.py` 或
`bml_target_imc_api()` 生成 C++ 绑定，实现生成的 provider，并让消费者
使用生成的 client。不要手写字段编码，也不要跨 DLL 传递 C++ 对象、STL
容器、allocator 所有权或 `CKObject*`。

完整示例与兼容演进规则见：

- [Inter-mod communication](../imc.md)
- [Create a typed IMC API](../imc-author-guide.md)

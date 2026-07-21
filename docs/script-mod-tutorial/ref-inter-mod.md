# 跨 Mod 通信

`[bml.export]`、`ExportRef` 和 `CallFrame` 已随旧实验 ABI 移除。不要把任意
脚本方法当作跨 Mod ABI。

按数据形态选择接口：

- 一次原子只读快照：声明 `Resource<T>`。
- 按 `ObjectRef` 读取的快照：声明 `Component<T>`。
- 分页结果：声明 `Collection<T>`。
- 有序事件或可缓存变化：声明 `Stream<T>`。
- 只有已有 UI 等明确命令能力才使用 `Command<Req, Resp>`。
- 简单、延后读取的状态可继续使用 DataShare。

普通消费者应使用生成的浅层 API，例如 `BML::Runtime::ReadState`、
`BML::Scene::Find`、`BML::Gameplay::ReadLevel` 和 `BML::Events::Open`。
不要手写记录字段或调用底层 C ABI。

要提供新能力时，先写版本化 `.bmlapi`：包 ID、主/次版本、稳定 record/field
ID 和 endpoint。生成器会同时给 native 和 AngelScript 生成绑定。AngelScript
provider 只能在 `OnLoad` 中注册：

```angelscript
int ReadState(const BML::Interop::Request &in request,
              BML::Interop::RecordWriter@ writer) {
  return writer.SetBool(1, true);
}

void OnLoad(const BML::ModContext &in ctx) {
  // 实际项目使用由 .bmlapi 生成的 CreateApi()。
  BML::Interop::ApiBuilder@ api = BML::Interop::CreateApi(
      "example.provider", 1, 0, uint64(0x1234567890abcdef));
  api.AddSchema(1, "state");
  api.AddField(1, 1, "enabled", BML::Interop::FIELD_BOOL);
  api.AddEndpoint("state", BML::Interop::ENDPOINT_RESOURCE, 0, 1);

  BML::Interop::Provider@ provider = BML::Interop::CreateProvider();
  provider.SetRead("state", ReadState);
  BML::Interop::RegisterProvider(api, provider);
}
```

注册成功后 schema、endpoint 和 callback 配置冻结。卸载或热重载会撤销 provider；
消费者已有的复制快照仍可读到释放，但 stream/cursor 等 session 句柄会返回
stale 或 provider-unloaded 诊断。`ObjectRef` 只是 `domain + slot + generation`，
绝不保存或传递 `CKObject*`。

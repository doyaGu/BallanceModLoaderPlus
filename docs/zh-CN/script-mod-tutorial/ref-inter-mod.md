# 跨 Mod 通信

IMC 用于一个 Mod 发布给其他 Mod 的接口，不承载任何 Loader 能力。同一份
`.imc` 可以生成 C++ 和 AngelScript 门面；原生 Mod 与脚本 Mod 都可以成为
Client 或 Provider，并共享相同的 Record、路由、状态码和版本规则。

## 脚本 Mod 如何选择

脚本中读取 BML 自带能力时，直接使用有类型的接口：

```angelscript
if (ctx.IsInLevel()) {
  // 当前在关卡内，可继续读取 Gameplay 数据。
}

int checkpointCount = 0;
if (BML::Gameplay::ReadCheckpointCount(checkpointCount) == BML::ERROR_OK) {
  for (int i = 0; i < checkpointCount; ++i) {
    BML::Gameplay::Checkpoint checkpoint;
    if (BML::Gameplay::ReadCheckpoint(i, checkpoint) == BML::ERROR_OK) {
      CKObject@ object = checkpoint.BorrowObject();
    }
  }
}
```

可用的内置命名空间包括 `BML::Gameplay`、`BML::UI` 和
`BML::Speedrun`。状态、时钟和作弊状态可从脚本回调的 `ModContext` 读取。
Virtools 场景查找和对象标识应使用 CKAngelScript 的 `Scene` 命名空间及其
可重新验证的引用类型。Gameplay 读取直接复用进程内的数据读取器，但对应的 Ballance
数据数组可能尚不可用或布局不受支持，因此仍返回明确的状态码。脚本只处理
类型化数据，不直接管理原始消息或原生 IMC 句柄。目录、检查点和重置点使用
`Read*Count` 加 `Read*(index, value)` 读取；count 是当次读取的行数，随后每次
按索引读取都会重新检查当前数据数组，关卡切换后失效的索引会返回
`BML::ERROR_NOT_FOUND`。需要跨回调保留数据时，应复制实际需要的值，不要长期
缓存行号。

Loader 事件通过 `OnGameEvent` 同步回调到达。需要跨回调保留信息时，只复制
Mod 后续真正需要的状态；脚本侧没有需要打开或轮询的事件队列。

两个脚本 Mod 只需交换少量状态时，使用 DataShare。DataShare 适合有明确
类型和所有权的一次性或延迟读取，不应被包装成通用函数调用机制。

## 生成式 IMC

服务需要下面任一能力时使用 IMC：

- 需要请求/响应 RPC；
- 需要高频或有背压策略的事件流；
- 需要稳定字段与端点标识，供独立发布的 Mod 使用。

编写版本化 `.imc` 接口定义后，用 `bml_target_imc_api()` 的
`SCRIPT_OUTPUT_DIR` 生成 `*_imc.as`。脚本入口在 `[bml.mod]` 之前包含该文件，
然后使用生成的 `Is*Available`、`BeginCall*`、`Subscribe*`、`Publish*`、
`Handlers` 和 `Provider`。不要直接使用 `BML::Detail` 或下划线开头的
`ModContext` 方法，它们只服务于生成代码。

脚本 RPC 一律异步发起，回调和脚本 Provider Handler 一律在游戏线程执行。
需要 caller-thread Handler、高频循环或原生内存所有权时，把实现放在原生 Mod；
脚本仍可通过同一生成式 IMC 接口调用它，不需要再写 CKAngelScript 包装层。

只有必须直接借用插件专有原生对象或调用无法表示为 IMC Record 的引擎原语时，
才增加 CKAngelScript 扩展。不要手写字段编码，也不要跨 DLL 传递 C++ 对象、
STL 容器、allocator 所有权或裸 `CKObject*`。

完整示例与兼容演进规则见：

- [跨 Mod 通信](../imc.md)
- [创建类型化 IMC API](../imc-author-guide.md)

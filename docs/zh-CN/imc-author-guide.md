# 创建类型化 IMC API

本文演示一个原生 Mod 如何向另一个 Mod 提供类型化进程内 API。开始前先阅读
[跨 Mod 通信](imc.md)，了解传输、线程、兼容性和生命周期模型。

示例需要已安装的 BML+ SDK、CMake 3.14 或更新版本、C++20 编译器，以及
Python 3.10 或更新版本。新接口由 RPC 和 Topic 组成。

## 1. 编写接口

文件名应与 API ID 对应，使生成文件名一目了然，例如
`api/example.echo.imc`：

```text
api example.echo 1.0

enum echo_mode {
    normal = 0
    uppercase = 1
}

rpc echo(string text, enum<echo_mode> mode) -> (string text)
topic changed(int sequence, string text)
```

该文件使用小型 IDL，并不是 JSON。声明由空白分隔，逗号和分号可省略，同时支持
`# comment` 和 `// comment`。只有 Enum Value 需要显式编号。字段 ID 由生成器分配，
并冻结在相邻的 `example.echo.imc.lock` 中，不要把传输层记账信息写进接口源码。

只被一个端点使用时，优先使用内联 Request、Response 和 Topic 载荷。生成器会把
对应值类型命名为 `EchoRequestValue`、`EchoReplyValue` 和
`ChangedEventValue`。多个端点共享载荷时再使用命名 Record：

```text
record object_query {
    object target
    optional string label
}

rpc inspect(object_query) -> (string description)
```

无输入 RPC 使用空括号：`rpc state() -> runtime_state`。演进已有 Record 时，在新
字段类型前加 `optional`，例如 `optional string label`。

RPC 只需要报告成功或失败时省略响应：`rpc clear_cache()`。生成的 Client 直接返回
RPC 状态，不会为空响应分配、注册、编码或解码载荷。

| `.imc` 声明 | 生成的 IMC 接口 |
| --- | --- |
| `rpc name() -> response` | 无输入 `Call*` RPC |
| `rpc name(request) -> response` | 请求/响应 `Call*` RPC |
| `rpc name(request)` | 无响应 `Call*` RPC |
| `topic name(message)` | `Publish*` / `Subscribe*` Topic |

对象查询就是包含 `object` 字段的普通请求 Record。查询或命令意图应体现在 RPC 名称
和载荷类型中，不要增加新的传输种类。一个 Record 最多使用 64 个永久字段 ID；超限
会在生成阶段报错，而不是生成不完整编解码器。

Enum、Record、Field、Enum Value、RPC 和 Topic 名称可以使用 ASCII 字母、数字、
`_`、`-` 和 `.`，并且至少包含一个字母或数字。API ID 要求更严格：每个点分段非空，
并且只能包含小写 ASCII 字母和数字。这样可以保证独立生成 API 的文件名和 C++
命名空间没有歧义。

名称可以数字开头：生成的 C++ 标识符会添加 `_`，原接口名称和路由保持不变。生成器
会在调用编译器前拒绝纯标点名称、空 API 分段、非规范 API ID，以及折叠后产生相同
C++ 标识符的名称。

语法是封闭的：未知声明和错误字段会报告输入路径、行号和列号。说明文字或项目元数据
应放在注释或 `.imc` 文件之外。

| 接口类型 | 生成的 C++ 类型 |
| --- | --- |
| `bool`, `int`, `float` | `bool`、32 位 `int`、`float` |
| `int64`, `uint64`, `double` | `std::int64_t`、`std::uint64_t`、`double` |
| `string`, `bytes` | `std::string`、`std::vector<std::uint8_t>` |
| `object`, `vec2`, `vec3`, `mat4` | BML 不透明对象/数学值 |
| `array<T>` | Bool、数值、字符串、对象和数学类型对应的 `std::vector<T>` |
| `enum<name>` | 使用固定整数底层类型的 `enum class Name` |

宽数值数组支持 `array<int64>`、`array<uint64>` 和 `array<double>`。二进制 Blob 使用
`bytes`，没有冗余的 `array<bytes>`。CMake Helper 会自动选择类型化 IMC 生成流程，
手工生成也使用同一个 IMC Generator。

Enum 可指定 `: int`、`: int64` 或 `: uint64`，默认是 `int`。名称和数值必须唯一，
并落在底层类型范围内。Record 用 `enum<echo_mode>` 引用它。生成器会输出
`enum class EchoMode` 和 `IsKnownEchoMode()`，但沿用相应整数线格式，不增加运行时
Registry、Reflection 或 Wire Tag。当前只支持标量 Enum 字段；重复 Enum 可由具体
API 使用数值数组和自己的转换 Helper 表达。

解码遇到未知 Enum 数值时会保留底层数值，不会让整条消息失败。发送方可能更新时，
在 `switch` 前调用 `IsKnownEchoMode(value)`。兼容 minor 可以增加命名值，但不能删除、
改名、改号、修改底层类型或增加数值别名；接口 Lock 会校验这些规则。

### 演进兼容 minor 版本

同一 major 下保留现有 Record、Field、RPC 和 Topic，新字段必须为 optional。增加
minor 版本后，显式更新接口 Lock：

```text
python imc_codegen.py --update-lock \
  --input api/example.echo.imc \
  --out-dir generated
```

相邻的 `.imc.lock` 拥有永久字段 ID，并为删除的 optional 字段保留墓碑，防止旧 ID
被重新使用。调整声明顺序不会改变 ID。更新操作会拒绝 required 新字段、修改旧字段或
端点、删除 required 字段、修改 Enum Value，以及未增加 minor 的结构变化。

`.imc` 与 `.imc.lock` 必须一起提交。像审查其他 Lock File 一样审查它的 Diff，但不要
手工编辑。普通生成和 `--check` 不会修改 Lock；缺失或过期时会输出可执行的修复命令。
不兼容修改必须增加 major；新的 major 使用独立的字段 ID 空间。

## 2. 从 CMake 生成

安装后的 BML Package 包含 Generator 和 `bml_target_imc_api()`。该 Helper 将生成头
加入目标、添加构建目录到 Include Path，并启用 C++20。配置时需要 Python 3.10 或
更新版本，接口旁必须存在已提交的 `.imc.lock`。

```cmake
find_package(BML CONFIG REQUIRED)

bml_add_mod(EchoMod EchoMod.cpp)

bml_target_imc_api(EchoMod
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/api/example.echo.imc"
)
```

默认输出为 `${CMAKE_CURRENT_BINARY_DIR}/bml-imc/example_echo_imc.hpp`。默认情况下，
`.imc` 文件名必须与 `api` 相同；不同时传入 `API_ID example.echo`。Helper 会将期望 ID
交给 Codegen，使拼写错误直接报告两个 ID 和输入路径。`OUTPUT_DIR` 可以覆盖输出目录。

手工或提交生成结果时，Package 提供 `BML_IMC_CODEGEN` 路径：

```text
python imc_codegen.py \
  --input api/example.echo.imc \
  --expected-api-id example.echo \
  --out-dir generated
```

在 CI 中加入 `--check`，检查已提交的生成头和接口 Lock 是否过期。只有接口作者控制的
更新步骤才使用 `--update-lock`。`--expected-api-id` 在 CMake 外可省略，但对预测输出
文件名的脚本很有用。解析和校验错误始终包含对应输入路径。

## 3. 实现 Provider

生成命名空间来自 API ID。Provider Handler 接收类型化值并返回普通 BML 状态码：

```cpp
#include "example_echo_imc.hpp"

namespace Echo = BML::Imc::Generated::Example::Echo;

Echo::Provider g_EchoProvider;

int HandleEcho(const Echo::EchoRequestValue &request,
               Echo::EchoReplyValue &reply,
               void *) {
    reply.Text = request.Text;
    return BML_OK;
}

int StartEchoProvider() {
    int status = g_EchoProvider.Open();
    if (status == BML_OK) {
        status = g_EchoProvider.RegisterEcho(
            &HandleEcho, nullptr, BML_IMC_EXECUTION_CALLER_THREAD);
    }
    return status;
}

void StopEchoProvider() {
    (void)g_EchoProvider.UnregisterEcho();
    (void)g_EchoProvider.Close();
}
```

对于 `rpc clear_cache()` 这样的无响应声明，Handler 是
`int ClearCache(void *userdata)`，Client 调用是 `client.CallClearCache()`。存在请求时，
Handler 在 `userdata` 前接收 `const RequestValue &`；两种形式都没有虚假的输出参数。

调用线程模式只用于短小且线程安全的代码。访问 Virtools、BML UI 或其他游戏线程状态的
Handler 必须使用 `BML_IMC_EXECUTION_GAME_THREAD`，这也是生成接口的默认值。

不传 Owner ID 的 `Open()` 会从调用 DLL 解析原生 Mod。一个 DLL 包含多个 Mod 时，
传入明确的 Owner ID。在注销和关闭成功前，保持 Provider 与 Callback Userdata 存活。

## 4. 同步或异步调用

普通生成方法完成整个类型化调用：

```cpp
Echo::Client client;
int status = client.Open();

Echo::EchoRequestValue request;
request.Text = "hello";
request.Mode = Echo::EchoMode::Normal;
Echo::EchoReplyValue reply;
if (status == BML_OK)
    status = client.CallEcho(request, reply, 1000);
```

可选集成可以先做提示性的可用性检查：

```cpp
bool available = false;
if (client.IsEchoAvailable(available) == BML_OK && available) {
    // 显示或启用集成功能；正常调用仍需处理 Provider 卸载。
}
```

`IsEchoAvailable` 只查询已缓存 RPC ID 的当前状态，不枚举 Provider，也不创建 Future。
Provider 可能在检查后立即卸载，因此 `CallEcho` 和 `BeginCallEcho` 仍需处理
`BML_ERROR_IMC_ENDPOINT_NOT_FOUND`。

每个生成 RPC 还提供类型化 Future 方法，无需手工编码请求、检查载荷或解码结果：

```cpp
Echo::Client::EchoFuture pending;
int status = client.BeginCallEcho(request, pending, 1000);

// 在后续 Tick 或 Worker 迭代中：
if (status == BML_OK) {
    status = pending.AwaitResult(reply, 0); // 0 表示轮询
    if (status == BML_ERROR_BUSY) {
        // 尚未完成；保留 Future，之后再次检查。
    }
}
```

`RpcFuture<T>` 只可移动，并自动释放原始 Future；还提供 `GetState`、`Await`、
`Cancel`、`GetError`、`GetResult` 和显式 `Release`。调用方仍持有未完成 Future 时，
再次执行 `Begin*` 会返回 `BML_ERROR_BUSY`，不会静默覆盖。

不要在游戏线程上以非零超时等待尚未完成的游戏线程任务；零超时轮询是安全的。

## 5. 发布与订阅

```cpp
std::size_t subscribers = 0;
if (client.GetChangedSubscriberCount(subscribers) == BML_OK && subscribers) {
    Echo::ChangedEventValue event;
    event.Sequence = 1;
    event.Text = "updated";
    (void)client.PublishChanged(event);
}
```

构造事件本身代价较高时，可以先检查订阅者数量。没有订阅者时发布仍然有效。

```cpp
void OnChanged(int status, Echo::ChangedEventValue *event,
               const BML_ImcMessage *, void *) noexcept {
    if (status == BML_OK && event) {
        // 类型化值只在本次回调期间有效。
    }
}

Echo::ChangedSubscription subscription;
int status = client.SubscribeChanged(subscription, &OnChanged, nullptr, 256,
    BML_IMC_BACKPRESSURE_DROP_OLDEST,
    BML_IMC_EXECUTION_GAME_THREAD);
```

在 `Close()` 成功前保持 Subscription 和 Callback Userdata 存活。使用
`DroppedCount()` 观察背压导致的消息丢失。

## 6. 诊断失败

所有生成方法都返回 BML 状态码。日志中同时记录状态码和
`BML_GetErrorString(status)`。常见集成错误包括：

- `BML_ERROR_IMC_ENDPOINT_NOT_FOUND`：没有 Provider 注册该 RPC 路由；
- `BML_ERROR_IMC_API_MISMATCH`：收到的载荷类型与生成端点不一致；
- `BML_ERROR_WRONG_THREAD`：在游戏线程同步等待游戏线程 Future；
- `BML_ERROR_WOULD_BLOCK`：使用 FAIL 背压的有界队列已满；
- `BML_ERROR_BUSY`：关闭或替换 Future 会违反生命周期规则。

销毁 Callback Userdata 前先关闭 Subscription 和 Provider，卸载原生 DLL 前先关闭
Client。Owner Cleanup 是最后的安全网，不是正常生命周期机制。

## 7. 发布检查清单

发布接口前确认：

- API、Record、Field、RPC、Topic 和 Enum 身份已经确定；
- 生成的 `.imc.lock` 与 `.imc` 一起提交；
- 提交生成头时，CI 使用 `--check`；
- 兼容 minor 更新已经审查接口 Lock Diff；
- 所有访问 Virtools 或 BML UI 的回调都使用游戏线程模式；
- RPC Timeout 和 Topic Queue/Backpressure 设置明确；
- 已测试 Client、Future 和 Subscription 存活时 Provider 卸载；
- 正确记录并处理 BML 状态码，没有把可用性检查当作永久保证。

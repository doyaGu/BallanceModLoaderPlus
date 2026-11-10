# 命令、配置、定时器

以下为当前 API。操作返回错误码，成功是 `BML::ERROR_OK`；旧源码逐项迁移见
[迁移指南](migration.md)。完整签名见[运行时导出的声明](https://doyagu.github.io/BallanceModLoaderPlus/api/bml-script-mod-api.as)。

## 命令

```angelscript
BML::CommandRef@ command;
void Setup(const BML::ModContext &in ctx) {
    BML::CommandDefinition definition;
    definition.Name = "hello";
    definition.Description = "Hello command";
    int code = ctx.Commands().Register(definition, BML::CommandExecute(Execute), command,
                                       BML::CommandComplete(Complete));
    if (code != BML::ERROR_OK) ctx.LogError("Register failed: " + code);
}
int Execute(const BML::ModContext &in ctx, const BML::CommandInvocation &in call) {
    string action;
    if (call.ArgumentCount > 0) call.ReadArgument(0, action);
    return call.Write("hello " + action) == BML::ERROR_OK ? 0 : 1;
}
int Complete(const BML::ModContext &in ctx,
             const BML::CommandCompletionRequest &in request,
             const BML::CommandCompletion &in candidates) {
    return candidates.Add("status");
}
```

执行回调返回非负业务退出码，负值按命令失败处理。Commands.ExecuteLine(line,status)
返回 API 状态，业务退出码在 status 中。Invocation 的 Name 是规范名，InvokedAs 是
用户实际输入的名称或别名，ArgumentCount/ReadArgument 不包含命令名。Input 是管道输入，
HasInput() 区分没有输入与空输入。Write 只在回调期间可用。

补全请求另有 ActiveArgument 和 Prefix；候选通过 Add/ReadCount/ReadCandidate 访问。
失败会丢弃本次候选集合。Invocation、补全请求与 collector 即使复制也不能延长回调寿命。
通知用的 CommandEvent 与这两种 authoring 回调不同，保留 OnCommandEvent 及前后通知点。

`Find(name, CommandInfo &out)` 和 `ReadCommands(? &out)` 返回拥有数据的快照；后者传入
array<BML::CommandInfo> 对象。CommandRef 的 Read/SetEnabled/Unregister 返回错误码。
结束操作成功后，同一 owner 再次 Unregister 为幂等成功。

## 配置

```angelscript
BML::ConfigPropertyDefinition definition;
definition.Category = "MyMod";
definition.Key = "ShowWindow";
definition.DefaultValue = BML::ConfigValue(true);
definition.Comment = "Show the window";
BML::ConfigProperty@ property;
if (ctx.Config().DeclareProperty(definition, property) == BML::ERROR_OK) {
    bool show = property.GetBoolean(true);
    int code = property.Set(BML::ConfigValue(!show));
}
```

ConfigValue 拥有 string/bool/int/float/CKKEYBOARD 或 NONE。严格 ReadString/ReadBoolean/
ReadInteger/ReadFloat/ReadKey 不做隐式转换；Get*(fallback) 是属性上的显式回退读取。
普通 int 不是按键类型。Read(ConfigPropertyInfo &out) 拥有 Definition 与 CurrentValue。
Choices 被宿主复制；修改快照不会修改宿主。SetChoices 配合 CONFIG_EDITOR_CHOICE 提供
字符串候选；CONFIG_EDITOR_COLOR 支持颜色文本。

FindProperty 不创建属性。同一运行期重复声明不同类型会失败；第一次具体类型声明、
NONE 初始化及异型 SetDefault 可能替换已加载的值。要先保留并检查旧类型，使用 NONE
声明后严格 Read，再决定迁移。SetDefault(NONE) 也不是无操作。配置仍保存到
ModLoader/Configs/<mod-id>.cfg。

## 定时器

```angelscript
BML::TimerRef@ timer;
int code = ctx.Timers().SetIntervalTicks(1, BML::TimerLoopCallback(Tick), timer, "heartbeat");
bool Tick(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
    BML::TimerInfo info;
    if (event.Read(info) != BML::ERROR_OK) return false;
    return info.CompletedIterations < 10;
}
```

SetTimeout/SetTimeoutTicks 使用 void TimerCallback；SetInterval/SetIntervalTicks 使用
bool TimerLoopCallback。完整 Definition 通过 Register(definition,callback,out) 注册。
毫秒为非负有限 double，ticks、Handle、CompletedIterations 为 uint64，RemainingIterations
为 int64。零延迟被接受但在后续 pump 执行。DEBOUNCE/THROTTLE 尚未实现。

TimerEvent.Read 读取捕获快照，退休后仍可读。TimerRef.Read/Pause/Resume 需要当前运行期；
Cancel 成功后阻止后续分发，已经进入的回调可完成，其返回 true 不会复活定时器。
同 owner 的结束操作可重复成功。脚本引用不是 RAII，丢弃 Ref 不取消；运行期退休会清理。
固定回调不可挂起或 Await；CKAngelScript Async 任务不受 BML Mod 生命周期管理。

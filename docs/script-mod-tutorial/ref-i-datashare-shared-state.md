# 参考 I：DataShare 和跨脚本共享状态

参考 H讲了导出。导出适合“现在调用一个函数，马上拿结果”。

DataShare 解决的是另一类问题：

```text
一个脚本把轻量状态放到一个名字下面
另一个脚本按名字读取
值还没出现时，可以先登记一个请求，等它出现再回调
```

它适合共享状态，不适合保存复杂对象。

## DataShare 的心智模型

可以先把 DataShare 理解成一张全局小表：

```text
DataShare namespace
  key -> value
  key -> value
  key -> value
```

本章用一个单独的命名空间：

```text
tutorial.datashare
```

再放几个键：

```text
tutorial.status
tutorial.enabled
tutorial.count
tutorial.progress
tutorial.delayed
```

DataShare 的值只有几种基础类型：

```text
string
bool
int
float
```

不要把 `CKObject@`、`CK3dEntity@`、`CKDataArray@` 这类运行时句柄塞进 DataShare。  
需要共享对象身份时，先共享名字或 id，使用方再按时机重新查对象。

## 写入脚本

新建：

```text
ModLoader/Mods/DataShareWriter.mod.as
```

写入：

```angelscript
[bml.mod id="datashare.writer" name="DataShare Writer" version="1.0.0" author="Tutorial" bml="0.3.12" description="Publishes tutorial DataShare values"]
class DataShareWriter {
    private string shareName = "tutorial.datashare";

    void OnLoad(const BML::ModContext &in ctx) {
        PublishInitial(ctx);

        BML::TimerCallback@ callback = BML::TimerCallback(this.OnDelayedTimer);
        ctx.SetTimeout(800.0f, callback, "datashare-delayed");

        LogInfo(ctx, "Writer loaded");
    }

    private void PublishInitial(const BML::ModContext &in ctx) {
        bool statusOk = BML::DataShareSetString("tutorial.status", "ready", shareName);
        bool enabledOk = BML::DataShareSetBool("tutorial.enabled", true, shareName);
        bool countOk = BML::DataShareSetInt("tutorial.count", 1, shareName);
        bool progressOk = BML::DataShareSetFloat("tutorial.progress", 0.25f, shareName);

        BML::DataShareSetString("tutorial.temp", "remove me", shareName);
        bool hasTempBefore = BML::DataShareHas("tutorial.temp", shareName);
        BML::DataShareRemove("tutorial.temp", shareName);
        bool hasTempAfter = BML::DataShareHas("tutorial.temp", shareName);

        LogInfo(ctx, "Writer publish status=" + BoolText(statusOk) +
                     " enabled=" + BoolText(enabledOk) +
                     " count=" + BoolText(countOk) +
                     " progress=" + BoolText(progressOk));

        LogInfo(ctx, "Writer readback status=" +
                     BML::DataShareGetString("tutorial.status", "missing", shareName) +
                     " count=" + BML::DataShareGetInt("tutorial.count", -1, shareName) +
                     " progress=" + BML::DataShareGetFloat("tutorial.progress", -1.0f, shareName) +
                     " hasTempBefore=" + BoolText(hasTempBefore) +
                     " hasTempAfter=" + BoolText(hasTempAfter) +
                     " statusSize=" + BML::DataShareSizeOf("tutorial.status", shareName));
    }

    void OnDelayedTimer(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
        BML::DataShareSetString("tutorial.delayed", "delayed-ready", shareName);
        BML::DataShareSetInt("tutorial.count", 2, shareName);
        LogInfo(ctx, "Writer delayed values published");
    }

    private void LogInfo(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info(message);
        }
    }

    private string BoolText(bool value) {
        return value ? "true" : "false";
    }
}
```

这里用到的写入和读取函数：

```angelscript
BML::DataShareSetString(...)
BML::DataShareSetBool(...)
BML::DataShareSetInt(...)
BML::DataShareSetFloat(...)

BML::DataShareGetString(...)
BML::DataShareGetBool(...)
BML::DataShareGetInt(...)
BML::DataShareGetFloat(...)
```

第三个参数是 DataShare 命名空间。  
本章统一传 `tutorial.datashare`，避免和别的脚本共用默认空间时撞键。

## 读取脚本

新建：

```text
ModLoader/Mods/DataShareReader.mod.as
```

写入：

```angelscript
[bml.require id="datashare.writer" version="1.0.0"]
[bml.mod id="datashare.reader" name="DataShare Reader" version="1.0.0" author="Tutorial" bml="0.3.12" description="Reads tutorial DataShare values"]
class DataShareReader {
    private string shareName = "tutorial.datashare";

    private BML::DataShareRequestRef@ statusRequest;
    private BML::DataShareRequestRef@ delayedRequest;

    void OnLoad(const BML::ModContext &in ctx) {
        ReadImmediate(ctx);
        RequestValues(ctx);
        LogInfo(ctx, "Reader loaded");
    }

    private void ReadImmediate(const BML::ModContext &in ctx) {
        bool hasStatus = BML::DataShareHas("tutorial.status", shareName);
        string status = BML::DataShareGetString("tutorial.status", "missing", shareName);
        bool enabled = BML::DataShareGetBool("tutorial.enabled", false, shareName);
        int count = BML::DataShareGetInt("tutorial.count", -1, shareName);
        float progress = BML::DataShareGetFloat("tutorial.progress", -1.0f, shareName);

        LogInfo(ctx, "Reader immediate hasStatus=" + BoolText(hasStatus) +
                     " status=" + status +
                     " enabled=" + BoolText(enabled) +
                     " count=" + count +
                     " progress=" + progress);
    }

    private void RequestValues(const BML::ModContext &in ctx) {
        BML::DataShareCallback@ statusCallback = BML::DataShareCallback(this.OnStatusValue);
        @statusRequest = ctx.RequestDataShare("tutorial.status",
                                              BML::DATASHARE_STRING,
                                              statusCallback,
                                              shareName);

        BML::DataShareCallback@ delayedCallback = BML::DataShareCallback(this.OnDelayedValue);
        @delayedRequest = ctx.RequestDataShare("tutorial.delayed",
                                               BML::DATASHARE_STRING,
                                               delayedCallback,
                                               shareName);

        LogInfo(ctx, "Reader requests statusValid=" + BoolText(statusRequest !is null && statusRequest.IsValid) +
                     " delayedValid=" + BoolText(delayedRequest !is null && delayedRequest.IsValid));
    }

    void OnStatusValue(const BML::ModContext &in ctx, const BML::DataShareEvent &in event) {
        LogInfo(ctx, "Reader status callback exists=" + BoolText(event.Exists) +
                     " key=" + event.Key +
                     " type=" + DataShareTypeText(event.Type) +
                     " value=" + event.StringValue);
    }

    void OnDelayedValue(const BML::ModContext &in ctx, const BML::DataShareEvent &in event) {
        LogInfo(ctx, "Reader delayed callback exists=" + BoolText(event.Exists) +
                     " key=" + event.Key +
                     " type=" + DataShareTypeText(event.Type) +
                     " value=" + event.StringValue);
    }

    private string DataShareTypeText(int type) {
        if (type == BML::DATASHARE_STRING) {
            return "string";
        }
        if (type == BML::DATASHARE_BOOL) {
            return "bool";
        }
        if (type == BML::DATASHARE_INT) {
            return "int";
        }
        if (type == BML::DATASHARE_FLOAT) {
            return "float";
        }
        return "unknown";
    }

    private void LogInfo(const BML::ModContext &in ctx, const string &in message) {
        BML::Logger@ logger = ctx.BorrowLogger();
        if (logger !is null) {
            logger.Info(message);
        }
    }

    private string BoolText(bool value) {
        return value ? "true" : "false";
    }
}
```

`Reader` 依赖 `Writer`：

```angelscript
[bml.require id="datashare.writer" version="1.0.0"]
```

这样 `Writer` 会先加载并发布初始值，`Reader` 再读取。

## 立即读取

立即读取适合“值已经存在，缺失时用默认值”的场景：

```angelscript
string status = BML::DataShareGetString("tutorial.status", "missing", shareName);
int count = BML::DataShareGetInt("tutorial.count", -1, shareName);
```

如果键不存在，返回默认值。

想先判断有没有键，用：

```angelscript
bool hasStatus = BML::DataShareHas("tutorial.status", shareName);
```

想删除键，用：

```angelscript
BML::DataShareRemove("tutorial.temp", shareName);
```

想看底层存了多少字节，用：

```angelscript
int size = BML::DataShareSizeOf("tutorial.status", shareName);
```

这个大小用于诊断。脚本里一般不需要依赖它做业务判断。

## 请求回调

如果值还没出现，可以先请求：

```angelscript
BML::DataShareCallback@ delayedCallback = BML::DataShareCallback(this.OnDelayedValue);
@delayedRequest = ctx.RequestDataShare("tutorial.delayed",
                                       BML::DATASHARE_STRING,
                                       delayedCallback,
                                       shareName);
```

`tutorial.delayed` 在 `Writer` 的 Timer 里才写入。  
写入发生后，`Reader` 的 `OnDelayedValue` 会被调用一次。

请求是一次性的。回调触发后，这个请求就结束。  
如果想继续等待下一次变化，需要重新请求。

如果请求时值已经存在，回调可能会在 `RequestDataShare(...)` 返回前执行。所以日志里可能看到：

```text
Reader status callback ...
Reader requests statusValid=false delayedValid=true
```

这表示 `tutorial.status` 已经存在，请求立即完成，返回的请求句柄已经无效。  
`tutorial.delayed` 还不存在，所以请求句柄保持有效，等待后面的写入。

## DataShareEvent 怎么读

回调收到：

```angelscript
const BML::DataShareEvent &in event
```

常用字段：

| 字段 | 含义 |
| --- | --- |
| `event.Exists` | 这次是否拿到了值 |
| `event.Key` | 触发的键 |
| `event.Type` | 请求的值类型 |
| `event.StringValue` | 字符串值 |
| `event.BoolValue` | 布尔值 |
| `event.IntValue` | 整数值 |
| `event.FloatValue` | 浮点值 |

按请求类型读取对应字段。  
请求的是 `DATASHARE_STRING`，就读 `StringValue`。请求的是 `DATASHARE_INT`，就读 `IntValue`。

## 运行后看日志

保存两个脚本，重启 Player。

可以看到：

```text
[ModLoader/INFO]: Loading Mod datashare.writer[DataShare Writer] v1.0.0 by Tutorial
[datashare.writer/INFO]: Writer publish status=true enabled=true count=true progress=true
[datashare.writer/INFO]: Writer readback status=ready count=1 progress=0.25 hasTempBefore=true hasTempAfter=false statusSize=6
[datashare.writer/INFO]: Writer loaded
[ModLoader/INFO]: Loading Mod datashare.reader[DataShare Reader] v1.0.0 by Tutorial
[datashare.reader/INFO]: Reader immediate hasStatus=true status=ready enabled=true count=1 progress=0.25
[datashare.reader/INFO]: Reader status callback exists=true key=tutorial.status type=string value=ready
[datashare.reader/INFO]: Reader requests statusValid=false delayedValid=true
[datashare.reader/INFO]: Reader loaded
[ModLoader/INFO]: BML script mod summary: loaded=2 failed=0
[datashare.reader/INFO]: Reader delayed callback exists=true key=tutorial.delayed type=string value=delayed-ready
[datashare.writer/INFO]: Writer delayed values published
```

这里有两个细节：

```text
status 回调立即执行，所以 statusRequest 很快失效
delayed 回调等 Writer 的 Timer 写入后执行
```

## 和导出的区别

参考 H的导出是函数调用：

```text
调用 Greeting("Ballance")
马上得到 "Hello, Ballance"
```

DataShare 是状态共享：

```text
Writer 写 tutorial.status = ready
Reader 之后按 key 读取
Reader 也可以先请求，等 key 出现
```

按需求选：

| 需求 | 用什么 |
| --- | --- |
| 调一个函数并拿返回值 | 导出 |
| 发布当前状态，让别人稍后读 | DataShare |
| 等某个状态第一次出现 | `RequestDataShare` |
| 必须控制加载顺序 | `bml.require` |
| 只是可选增强 | `bml.optional` + `FindMod` 或 DataShare |

## DataShare 适合放什么

适合：

```text
当前模式名
调试开关
状态字符串
计数器
进度值
某个对象的名字或 id
```

不适合：

```text
CKObject@ 句柄
CK3dEntity@ 句柄
CKDataArray@ 句柄
大块文件内容
复杂对象图
需要强一致的事务数据
```

DataShare 是轻量共享状态。它不替你管理 Virtools 对象生命周期。

## 本章结果

现在可以把 DataShare 看成：

```text
命名空间 + key + 基础类型值
```

使用顺序：

```text
写入方选择 namespace 和 key
写入基础类型值
读取方用同一个 namespace 和 key 读取
值可能晚到时使用 RequestDataShare
回调触发一次后请求结束
```

下一章讲 BML HUD、菜单和消息服务。那一章会把 BML 自带 UI/HUD 和 ImGui 调试窗口分开。

# 跨 Mod 通信

## 依赖声明

```angelscript
[bml.require id="provider.mod" version="1.0.0"]   // 必须依赖
[bml.optional id="debug.mod" version="0.1.0"]     // 可选依赖
```

require 缺失 = mod 不加载。optional 缺失 = 不阻止加载。

## 导出声明

```angelscript
[bml.mod id="provider.mod" name="Provider" version="1.0.0" author="Tutorial" bml="0.3.12" description="Export sample"]
class ProviderMod {
    [bml.export name="Greeting"]
    string Greeting(const string &in name) {
        return "Hello " + name;
    }
}
```

`bml.export` 必须标在入口类的方法上，不能标在全局函数上。

## 查找和调用导出

```angelscript
BML::ModRef@ provider = ctx.FindMod("provider.mod");
if (provider is null) return;

BML::ExportRef@ greeting;
int lookup = provider.TryFindExport("Greeting", greeting);
if (lookup != BML::ERROR_OK) return;

// Narrow call:
string result;
int status = greeting.CallString("World", result);

// Or via CallFrame (multi-arg):
BML::CallFrame@ frame = BML::CallFrame();
frame.SetString(0, "World");
int status2 = greeting.Call(frame);
if (status2 == BML::ERROR_OK) {
    frame.GetResultString(result);
}
```

## CallFrame 方法

SetString/SetInt/SetBool/SetFloat(index, value)
GetResultString/GetResultInt/GetResultBool/GetResultFloat(out)
Clear(), ResultType

## ModRef 属性

Id, Name, Version, Author, Description, BMLVersion, IsScript, IsFailed, Diagnostic, GetState(), GetKind()

## DataShare

### 写入

```angelscript
BML::DataShareSetString("key", "value", "my.namespace");
BML::DataShareSetBool("key", true, "my.namespace");
BML::DataShareSetInt("key", 42, "my.namespace");
BML::DataShareSetFloat("key", 3.14f, "my.namespace");
```

### 读取

```angelscript
string v = BML::DataShareGetString("key", "default", "my.namespace");
bool v2 = BML::DataShareGetBool("key", false, "my.namespace");
int v3 = BML::DataShareGetInt("key", 0, "my.namespace");
float v4 = BML::DataShareGetFloat("key", 0.0f, "my.namespace");
```

### 工具

```angelscript
bool BML::DataShareHas("key", "ns");
void BML::DataShareRemove("key", "ns");
```

### 请求回调

```angelscript
BML::DataShareCallback@ cb = BML::DataShareCallback(this.OnValue);
ctx.RequestDataShare("key", BML::DATASHARE_STRING, cb, "ns");
```

One-shot. Script callbacks are queued to the BML main-thread safe point; if the
value already exists, the callback runs on a later process tick rather than
inside `RequestDataShare`.

Types: DATASHARE_STRING, DATASHARE_BOOL, DATASHARE_INT, DATASHARE_FLOAT

### 选择指南

| 需求 | 用什么 |
|------|--------|
| 调用函数取返回值 | Export |
| 发布状态供其他 mod 读 | DataShare |
| 等待状态首次出现 | RequestDataShare |
| 控制加载顺序 | bml.require |

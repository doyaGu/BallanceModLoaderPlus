# 命令、配置、定时器

## 命令注册

```angelscript
BML::CommandDefinition def;
def.Name = "hello";
def.Alias = "h";
def.Description = "Hello command";
def.Usage = "hello [status|toggle]";
def.Category = "Tutorial";
def.Enabled = true;

BML::CommandCallback@ execute = BML::CommandCallback(this.OnCommand);
BML::CommandCompletionCallback@ complete = BML::CommandCompletionCallback(this.OnComplete);
BML::CommandRef@ commandRef = ctx.RegisterCommand(def, execute, complete);
```

## 命令回调签名

```angelscript
funcdef void CommandCallback(const BML::ModContext &in ctx, const BML::CommandEvent &in event);
funcdef void CommandCompletionCallback(const BML::ModContext &in ctx, const BML::CommandEvent &in event, BML::CommandCompletion &inout completions);
```

## CommandEvent 字段

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| CommandName | string | 命令名 |
| ArgsText | string | 参数原文 |
| ArgCount | int | 参数数量 |
| GetArg(index) | string | 第 N 个参数 |
| Phase | BML::CommandEventPhase | 执行阶段 |

阶段值：`COMMAND_EVENT_PRE`、`COMMAND_EVENT_POST`、`COMMAND_EVENT_EXECUTE`、`COMMAND_EVENT_COMPLETE`。

## 补全

```angelscript
private void OnComplete(const BML::ModContext &in ctx, const BML::CommandEvent &in event, BML::CommandCompletion &inout completions) {
    completions.Add("status");
    completions.Add("toggle");
}
```

## 配置

```angelscript
BML::Config@ config = ctx.BorrowConfig();
config.SetCategoryComment("MyMod", "Settings");

BML::ConfigProperty@ prop = config.GetProperty("MyMod", "ShowWindow");
prop.SetDefaultBoolean(true);
prop.SetComment("Show the window");
bool show = prop.GetBoolean(true);
// Later: prop.SetBoolean(newValue);
```

### ConfigProperty 方法

| 方法 | 说明 |
| --- | --- |
| SetDefaultBoolean/String/Integer/Float(v) | 设置默认值 |
| SetComment(text) | 设置注释 |
| GetBoolean/String/Integer/Float(default) | 读取，如未设置返回 default |
| SetBoolean/String/Integer/Float(v) | 写入 |

类型值：`CONFIG_PROPERTY_BOOLEAN`、`CONFIG_PROPERTY_STRING`、`CONFIG_PROPERTY_INTEGER`、`CONFIG_PROPERTY_FLOAT`、`CONFIG_PROPERTY_KEY`。

配置文件自动生成在 `ModLoader/Configs/<mod-id>.cfg`。

## 定时器

### 一次性

```angelscript
BML::TimerCallback@ cb = BML::TimerCallback(this.OnTimeout);
BML::TimerRef@ timerRef = ctx.SetTimeout(1000.0f, cb, "my-timeout");
```

回调签名：
```angelscript
private void OnTimeout(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
    // event.Name = "my-timeout"
}
```

### 循环

```angelscript
BML::TimerLoopCallback@ cb = BML::TimerLoopCallback(this.OnInterval);
BML::TimerRef@ timerRef = ctx.SetInterval(500.0f, cb, "my-interval");
```

回调签名：
```angelscript
private bool OnInterval(const BML::ModContext &in ctx, const BML::TimerEvent &in event) {
    return true;  // true = 继续, false = 停止
}
```

### TimerRef

```angelscript
timerRef.IsValid  // bool property
timerRef.Cancel() // 取消定时器
```

### TimerEvent 字段

| 字段 | 类型 |
| --- | --- |
| Name | string |

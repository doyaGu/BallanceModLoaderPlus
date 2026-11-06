# 输入与 ImGui

## 输入 API

```angelscript
BML::InputHook@ input = ctx.BorrowInputManager();
if (input is null) return;
```

| 方法 | 行为 |
|------|------|
| `input.IsKeyDown(CKKEY_X)` | 按住期间每帧返回 true |
| `input.IsKeyPressed(CKKEY_X)` | 仅按下瞬间返回 true |

规则：
- 开关用 IsKeyPressed（一次一动作）
- 持续效果用 IsKeyDown
- 用 IsKeyDown 做开关会每帧触发，窗口闪烁

## 屏蔽与释放

脚本 `InputHook` 读取经过 BML 屏蔽的状态；读取不会推进事件或输入快照。
ImGui 单独观察 SDL 事件，因而可以在阻止游戏输入时继续操作菜单。
`input.IsValid` 是属性，使用时不加括号。

```angelscript
uint64 token = input.AcquireBlock(BML::INPUT_BLOCK_KEYBOARD);
// 关闭界面或 OnUnload 时，重新借用当前 input 再释放：
if (token != 0) input.ReleaseBlock(token);
```

掩码包括 `INPUT_BLOCK_KEYBOARD`、`INPUT_BLOCK_MOUSE`、`INPUT_BLOCK_JOYSTICK`
和 `INPUT_BLOCK_ALL`。0 表示获取失败。令牌只是整数，丢弃它不会自动释放，须保存在
成员中并在关闭界面、卸载/重载时释放；重复释放同一令牌无副作用。原生 SDK 的
`Input::Block` 才是 RAII 拥有型对象。`Block`/`Unblock` 与 `SetBlock(true/false)`
也按计数配对，不是绝对布尔赋值。不要解除别人的屏蔽；BML 屏蔽不改变引擎 Pause。

## 常用键名

常用值包括 `CKKEY_F9`、`CKKEY_F10`、`CKKEY_J`、`CKKEY_K`、
`CKKEY_U`、`CKKEY_F` 和 `CKKEY_SPACE` 等。

## ImGui 窗口模板

```angelscript
ImGui::SetNextWindowPos(ImVec2(80.0f, 80.0f), ImGuiCond_Once);
ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f), ImGuiCond_Once);
if (ImGui::Begin("Window Title")) {
    ImGui::TextUnformatted("Label: " + value);
}
ImGui::End();
```

## ImGui 规则

- 即时模式：每帧提交内容
- 跨帧状态必须存在成员变量中
- Begin/End 必须配对（End 在 if 之外）
- 不要在 OnProcess 里做重计算

## FPS 计算

```angelscript
ImGuiIO@ io = ImGui::GetIO();
if (io !is null && io.DeltaTime > 0.0f) {
    fps = 1.0f / io.DeltaTime;
}
```

## 每帧模型

```
Process: 默认引擎处理 -> BML PostProcess: 读输入/更新 Mod/提交 ImGui -> 输入收尾
Render: 独立调度 -> post-render 通知
```

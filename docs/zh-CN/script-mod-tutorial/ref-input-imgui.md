# 输入与 ImGui

## 快照读取

```angelscript
BML::KeyboardState@ keyboard;
if (ctx.Input().ReadKeyboard(keyboard) != BML::ERROR_OK) return;
if (keyboard.IsKeyPressed(uint(CKKEY_F9))) ctx.LogInfo("F9 pressed");
```

ReadKeyboard、ReadMouse、ReadJoystick 是三个状态读取入口。读取不处理引擎、不消费事件。
默认读取经过屏蔽的状态；最后一个 flags 参数传 uint(BML::INPUT_RAW) 可读取原始状态。
没有设备时成功且 Attached=false；没有输入管理器时 ERROR_UNAVAILABLE。

KeyboardState 拥有不可变共享快照；同一快照可查询多次，跨帧或退休后仍读取原捕获。
IsKeyPressed 用于开关，IsKeyDown 用于持续操作。ReadKey/ReadEvent 提供严格错误码；
按键越界时 IsKeyUp=true，其余便利谓词 false。原生 C/C++ 键盘 span 仍仅借用至最外层
宿主调用结束，不具有脚本快照的拥有寿命。Input 服务视图的有效性使用 IsValid()。

## 屏蔽与释放

```angelscript
BML::InputBlock@ block;
int code = ctx.Input().AcquireBlock(uint(BML::INPUT_BLOCK_KEYBOARD), block);
// 关闭界面时：
if (code == BML::ERROR_OK) code = block.Release();
```

掩码包括 INPUT_BLOCK_KEYBOARD/MOUSE/JOYSTICK/ALL。保存 InputBlock，显式 Release 或由
原运行期退休清理；丢弃脚本 Ref 不释放。多次请求按计数叠加，只释放自己的请求。
同 owner 重复结束成功，异 owner 无权操作。InputBlock.IsValid() 检查当前运行期。

ImGui 独立观察 SDL 事件，可在游戏被屏蔽时继续操作菜单。Pause、键盘重复、光标可见性
和形状是共享状态，修改者负责恢复。屏蔽本身不改变引擎 Pause。旧 InputHook/token
入口不再注册，见[迁移指南](migration.md)。

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

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

## 常用键名

CKKEY_F9, CKKEY_F10, CKKEY_J, CKKEY_K, CKKEY_U, CKKEY_F, CKKEY_SPACE, etc.

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
OnProcess -> 读输入 -> 更新成员变量 -> 提交 ImGui -> 引擎渲染
```

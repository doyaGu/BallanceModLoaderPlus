# BML+ 脚本 API 参考

以下参考文件描述 BML+ 提供的 AngelScript 接口。它们用于编辑器补全和 API
查询，不是运行时脚本，也不应包含在 Mod 发布包中。

## 绑定声明

<ul>
  <li><a href="https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/api/as.predefined"><code>as.predefined</code></a> — AngelScript Language Server 使用的完整定义，包含 CKAngelScript 与 BML+。</li>
  <li><a href="https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/api/bml-script-mod-api.as"><code>bml-script-mod-api.as</code></a> — BML+ 脚本 Mod 的类型、函数、回调和属性。</li>
  <li><a href="https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/api/bml-imgui-api.as"><code>bml-imgui-api.as</code></a> — 脚本可用的 Dear ImGui 绑定。</li>
</ul>

SDK 中的本地副本位于 `docs/api`。为编辑器配置
`<BML-SDK>/docs/api/as.predefined` 时不需要网络连接。

编写脚本时，建议将 `as.predefined` 配置给 AngelScript Language Server。
BML+ 不会在运行时加载这些参考文件。

声明中的 `get_Name()` 表示 AngelScript 属性访问器，脚本中通常写成
`object.Name`，如 `ctx.IsInLevel`、`input.IsValid`，不加括号。即使还有同名
兼容方法，属性形式也不会遇到属性遮蔽函数调用的问题。`@+` 是 native 绑定边界的
AngelScript auto-handle 标记，具体保留和释放规则仍以相应服务契约为准。

## Mod 菜单页面

脚本与原生 Mod 使用同一页面注册表。以下完整示例在 `OnLoad` 注册可见入口和隐藏子页，
页面只属于当前 Mod；同一个 Mod 的 id 不能同时装多个副本。

```angelscript
void DrawEntry(BML::MenuPageFrame &inout frame) {
  if (BML::UI::MainButton("Details")) frame.Push("child");
}

void DrawChild(BML::MenuPageFrame &inout frame) {
  ImGui::TextUnformatted("More information");
  if (BML::UI::MainButton("Return to overview")) frame.Back();
}

[bml.mod id="example.menu.script" name="Menu Example" version="1.0.0"]
class MenuExampleMod {
  BML::MenuPageRef@ entryPage;
  BML::MenuPageRef@ childPage;

  void OnLoad(const BML::ModContext &in ctx) {
    BML::MenuPageDefinition entry;
    entry.Id = "overview";
    entry.Label = "Overview";
    @entryPage = ctx.RegisterMenuPage(entry, DrawEntry);

    BML::MenuPageDefinition child;
    child.Id = "child";
    child.Label = "Details";
    child.ShowInDetails = false;
    @childPage = ctx.RegisterMenuPage(child, DrawChild);
  }

  void OnUnload(const BML::ModContext &in ctx) {
    if (childPage !is null) childPage.Unregister();
    if (entryPage !is null) entryPage.Unregister();
  }
}
```

`Push` 保留返回历史，`Replace` 替换当前页，`Back` 返回，`Close` 关闭 Mods 菜单。
页面 id 最多 255 字节，只能路由到当前 Mod 注册的页面。卸载或成功热重载时自动注销；
旧 `MenuPageRef` 随移除失效。绘制回调运行在 Mods 菜单的内容区域，直接提交控件，
不要再次创建顶层 ImGui 窗口。普通 ImGui 绘制在 `OnProcess`，菜单回调使用其自己的
有效绘制作用域。

## ImGui 属性

脚本绑定中的 `ImGuiIO` 只读；`ImGuiStyle` 的已有可写属性同时提供 getter/setter
声明。两者都借用当前 ImGui context。局部外观优先使用成对的 Push/Pop 样式操作，
避免留下影响其他 Mod 的共享样式修改。

## 声明基线

当前合并声明于 2026-11-06 从 macOS ARM64 SDL 动态宿主导出，AngelScript 为
2.38.0，DynCall 关闭，再追加 BML+ 与 ImGui 声明。已同步 BehaviorFrame/Scene
签名及 64 位指针大小别名。CKAS 平台别名与可选 FFI 接口随宿主配置变化；
这是一份编辑器快照，不是通用 ABI 定义。依赖这些接口时，应从目标宿主重新生成，
脚本能否编译以实际运行时注册的声明为准。

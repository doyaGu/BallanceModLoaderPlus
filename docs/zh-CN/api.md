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
`object.Name`。声明中的 `@+` 表示对应注册成功后，引用由 BML+ 保持。

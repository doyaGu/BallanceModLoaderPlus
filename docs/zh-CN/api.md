# BML 脚本 API

这里放脚本教程中会用到的 BML 脚本绑定声明。

## 绑定声明

- [as.predefined](as.predefined)
- [BML 脚本 mod API](bml-script-mod-api.as)
- [BML ImGui API](bml-imgui-api.as)

`as.predefined` 是给 AngelScript Language Server 使用的完整预定义文件，包含 CKAngelScript 导出的基础 API、BML 脚本 mod API 和 BML ImGui API。写脚本时建议把它放到 VS Code 工作区里。它不是运行时脚本，BML 不会加载它。

下面两个 `.as` 文件是拆分出来的 BML 声明，适合快速查 BML 自己提供的类型、函数、回调签名。声明里的 `get_XXX()` 表示 AngelScript 属性访问器，脚本里通常写成 `obj.XXX`。

这些文件是 API reference stub，不是运行时源码。打包 mod 时不要把它们放进 `ModLoader/Mods`。

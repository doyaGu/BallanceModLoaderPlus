# 使用 BML+

本文面向安装 BML+ 运行时和 Mod 的玩家，不需要编译器或 BML+ SDK。

## 运行要求

- Windows 上的 BallancePlayer。BML+ 不支持原版 Ballance Player。
- x86 Microsoft Visual C++ 2015–2022 Redistributable。
- 与准备安装的 Mod 兼容的 BML+ 版本。

替换 `BMLPlus.dll`、`AngelScript.dll` 或原生 Mod 前先关闭 Player。游戏正在使用
DLL 时，Windows 无法安全替换它。

## 安装 BML+

1. 从 [BML+ Releases](https://github.com/doyaGu/BallanceModLoaderPlus/releases)
   下载 `BMLPlus-<version>.zip`。
2. 将压缩包内的全部文件解压到 Ballance 安装目录。不要在外面额外套一层
   `BMLPlus-<version>` 目录。
3. 检查目录结构：

   ```text
   <Ballance>/
     Bin/Player.exe
     BuildingBlocks/BMLPlus.dll
     BuildingBlocks/AngelScript.dll   # 支持脚本 Mod 的发布包中存在
     ModLoader/
   ```

4. 启动 `Bin/Player.exe`。
5. 确认游戏窗口顶部显示 BML+ 版本。打开 `ModLoader/ModLoader.log`，确认初始化
   完成且没有 Loader 错误。

按 `/` 打开 BML+ 命令栏。输入 `bml` 可以查看 Loader 版本和已加载的 Mod。

## 安装 Mod

将支持的 Mod 包放入 `ModLoader/Mods`：

- 原生 Mod 通常是 `.bmodp` DLL，或者包含原生 Mod 及其文件的受支持 zip 包。
- 脚本 Mod 可以是单个 `*.mod.as` 文件、包含且只包含一个 `*.mod.as` 入口的
  目录，或者包含且只包含一个入口的受支持 zip 包。
- 脚本 Mod 需要与当前 BML+ 发布包配套的 `AngelScript.dll`。

依赖和附加文件以 Mod 自身的 README 为准。不要在 `Mods` 中同时保留同一 Mod
id 的开发目录和 zip 包。新增 Mod 或修改依赖后应重启 Player。

通过 `ModLoader/ModLoader.log` 和 `bml` 命令确认 Mod 已被发现并加载。脚本作者
还可以使用 `script status` 和 `script diag <id>`。

## BML+ 创建的文件

| 路径 | 用途 |
| --- | --- |
| `ModLoader/Mods` | 已安装的原生 Mod 和脚本 Mod |
| `ModLoader/Configs` | BML+ 与各 Mod 的配置 |
| `ModLoader/ModLoader.log` | Loader、依赖和 Mod 诊断信息 |
| `ModLoader/Fonts` | BML+ 界面使用的可选字体 |
| `ModLoader/Themes` | 控制台输出使用的 ANSI 调色板主题 |

## 命令行

按 `/` 打开命令栏。它支持下列 shell 风格语法：

- 未加引号的空格分隔单词。`'...'` 按字面保留，`"..."` 保留空格并允许其中的
  `$NAME`，`$'...'` 解析 `\n`、`\e` 之类的转义。反斜杠只转义 shell 字符，
  因此 `map load C:\Maps\x.nmo` 不需要加引号。
- `a; b` 依次运行两者，`a && b` 只在 `a` 成功时运行 `b`，`a || b` 只在失败时
  运行，`a | b` 把 `a` 的输出交给 `b`。`$?` 是上一条命令的状态。`# ...` 是注释。
- `set NAME VALUE` 定义本次会话的变量，`set -U NAME VALUE` 跨会话保留，
  `set -e NAME` 删除，`$NAME` 或 `${NAME}` 插入其值。`$(cmd)` 插入 `cmd` 的输出。
- `alias name='cmd args'` 为一行的第一个词定义缩写；`alias` 列出全部，
  `unalias name` 删除一个。别名会持久保存。
- `history` 给历史行编号；`!!`、`!n`、`!-n`、`!text` 重新执行它们。
- 管道过滤器：`grep`、`head`、`tail`、`wc`、`sort`、`uniq`、`xargs`。`true`
  和 `false` 只设置状态，不做别的。

编辑按键：Tab 补全命令、别名、`$` 变量和命令自己的参数，在 `;` 或 `|` 之后同样
有效。上下方向键遍历历史，保留正在输入的文字并按它过滤。光标后面会出现来自
历史的灰色建议，按 Right 或 End 接受。Ctrl+R 搜索历史，Ctrl+A 与 Ctrl+E 跳到
行首行尾，Ctrl+U、Ctrl+K、Ctrl+W 分别剪切到行首、到行尾或一个词，Ctrl+Y 粘贴
剪切的内容，Ctrl+L 清空消息板。未写完的行（例如没有闭合的引号或末尾的 `&&`）
会要求再输入一行，而不是直接运行。续行向上扩展，因此补全、搜索或输入法候选栏
始终位于当前编辑行下方，消息板位于整条命令上方；这些物理行在历史中仍是一条命令。

变量、别名与历史保存在 `ModLoader/CommandBar.shell.json` 和
`ModLoader/CommandBar.history`。在 `BML.cfg` 中设置 `CommandBar.KeepOpen`
可以让命令运行后命令栏保持打开。输入 `help` 可以查看当前安装提供的命令。

`CommandBar` 配置分类还提供独立开关，可分别控制语法高亮、Tab 补全及候选栏、
历史内联建议、Ctrl+R 反向搜索，以及上下方向键的历史导航。关闭命令栏时的定时通知
和打开命令栏时的历史消息也可以通过 `ShowNotifications` 与 `ShowScrollback` 分别控制。
这些设置可以在 Mod 菜单中修改，并会立即生效。

命令栏打开时，输入框支持常规的鼠标框选、复制、剪切和粘贴。消息板为只读区域：
拖动鼠标可以框选历史消息，按 Ctrl+C 会复制不含 ANSI 颜色控制码的纯文本；先点击
消息板后按 Ctrl+A 可以选择全部历史消息。拖出消息板上边缘或下边缘后，会在继续
框选的同时自动滚动。点击或框选消息板不会关闭当前命令会话。

语法高亮默认采用 Atom One Dark 配色。可以在 `CommandBarTheme` 分类中用
`#RRGGBB` 或 `#RRGGBBAA` 分别修改每一种语法角色；格式无效时会回退到该角色的
One Dark 默认颜色。有效的颜色值在 Mod 菜单中会使用调色盘按钮，主题修改同样会
立即生效。

## 更新 BML+

手动覆盖是默认且便于恢复的更新方式：

1. 关闭 Player。
2. 下载新的 `BMLPlus-<version>.zip`。
3. 解压到 Ballance 安装目录，并允许覆盖同名运行时文件。
4. 启动 Player，重新检查版本显示和日志。

不要手动解压 `BMLPlus-Update-<version>.zip`。它是 Updater 使用的更新载荷，
会有意省略不参与原地更新的文件。

可选的 Updater 只修改 BML+ 运行时文件，不会安装、删除或更新 Mod 和配置：

```powershell
Bin\Updater.exe check
Bin\Updater.exe update
Bin\Updater.exe doctor
```

如果 Player 可以启动，但问题与当前渲染器、字体、输入法或已安装 Mod 有关，请在
命令栏运行 `bml system`，并将输出与 `ModLoader/ModLoader.log` 一起提交。

如果 Updater 校验失败或 Updater 本身太旧，先手动安装一次最新完整包，再重新使用
Updater。

## 卸载 BML+

1. 关闭 Player。
2. 删除 `BuildingBlocks/BMLPlus.dll`。
3. 只有在 `AngelScript.dll` 随 BML+ 安装且没有其他 CKAngelScript 集成需要它时，
   才删除 `BuildingBlocks/AngelScript.dll`。

如果需要保留 Mod、配置、地图和日志，请保留 `ModLoader`。只有确定要同时清除这些
数据时才删除整个目录。

## 排查问题

### 没有显示 BML+ 版本

- 确认 `BMLPlus.dll` 直接位于 `BuildingBlocks`。
- 确认启动的是 BallancePlayer 的 `Bin/Player.exe`。
- 安装 x86 Visual C++ 2015–2022 Redistributable。
- 清理旧 Loader 混装的运行时文件，再从一个完整 BML+ 包重新安装。

### 原生 Mod 没有加载

- 确认文件使用 `.bmodp` 扩展名，或采用作者说明的包格式。
- 在 `ModLoader/ModLoader.log` 中搜索 Mod id、缺少的依赖、不兼容的 BML+ 版本
  或 DLL 加载错误。
- 只保留该 Mod 及其必需依赖进行测试。

### 脚本 Mod 没有加载

- 确认存在配套的 `BuildingBlocks/AngelScript.dll`。
- 确认包中只有一个 `*.mod.as` 入口。
- 依次使用 `script status`、`script diag <id>` 和 `script logs error`。
- 删除同一 Mod id 的重复目录、单文件或 zip 副本。

### 安装 Mod 后变慢或不稳定

- 修改 BML+ 设置前，先禁用该 Mod 复测。
- 每次只恢复一个 Mod，以定位冲突。
- 提交问题时附上 `ModLoader/ModLoader.log`、BML+ 版本、Mod 版本和复现步骤。

可稳定复现的 BML+ 问题请提交到
[Issue Tracker](https://github.com/doyaGu/BallanceModLoaderPlus/issues)。

# 在 Ballanced 中使用 BML+

## 要求

使用与 Ballanced SDL Player 的平台、架构、宿主链接方式和构建配置匹配的 BML+。
原生 Mod 要匹配 C 协议与目标描述；直接 Engine/ImGui 扩展还须匹配依赖、编译器和运行库 ABI。当前支持动态宿主，静态宿主
集成尚未完成。

## 安装

关闭 Player，将匹配的运行包解压到游戏根目录。BML+ 安装在 `Managers`，
CKAngelScript 在 `BuildingBlocks`，Mod 在 `ModLoader/Mods`。CK2、VxMath、SDL
由 Player 提供，进程中应只有一份引擎与 SDL。支持 `Bin/Player` 和扁平 `Player`
布局，Windows 可执行文件名为 `Player.exe`。

`ModLoader/BML.abi.json` 记录运行时目标。启动后检查 `ModLoader/ModLoader.log`，
并在 Mod 菜单核对版本。原生包须同时包含 `.bmodp` 与 `.bmodp.abi.json`；资源包和
脚本包使用 ZIP，脚本项目也可作为目录放入 `ModLoader/Mods`。

## 配置与诊断

用户配置在 `ModLoader/Configs`，更新时应保留。游戏内控制台的 `bml system`
与 Loader 日志用于实时诊断。字体位于 `ModLoader/Fonts`。中文组合输入由 SDL
接入；Windows 的 IMM/TSF 实现保留，其他平台使用系统输入服务。

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

## 更新与卸载

更新流程见[跨平台更新器和 doctor（英文）](../en/updater-sdl.md)。签名通道区分运行时、平台、
架构和链接方式；更新器保留用户配置、备份，并支持中断恢复。也可在关闭 Player 后
手动安装匹配的完整运行包。

卸载时删除 `Managers` 中的 BML+ 及其所属运行文件；可保留 `ModLoader` 以保存
配置和 Mod。保留 Player 自身的引擎及 SDL 库。

## 排查

原生 Mod 缺少或不匹配 ABI 描述时会被拒绝，请使用匹配的 SDL SDK 重编译。
缺少依赖时运行 `Updater doctor` 并检查日志。大小写敏感文件系统要求资源与脚本
文件名精确匹配。交叉编译成功不等于该目标已通过真实游戏验收。

旧 SDL BMLEntry/IMod 产物需迁移并重编译；schema 2 包使用 C 协议。
参见[原生迁移（英文）](../en/native-mod-migration.md)。

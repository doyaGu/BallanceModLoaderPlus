# 脚本 Mod 示例

这些示例用于完成 `templates/script-mod-template` 之后的下一步。每个目录都是一
个完整 Mod，并且只有一个 `*.mod.as` 入口。

| 示例 | 演示内容 | 运行结果 |
| --- | --- | --- |
| `command-config` | 注册命令并持久化设置 | `/examplefeature toggle` 切换并保存开关 |
| `input-ui` | 读取键盘输入并绘制 ImGui 窗口 | F9 隐藏或显示小窗口 |
| `game-state` | 读取类型化游戏状态并安全借用 CK 对象 | 进入关卡时在日志中输出当前活动球 |

把一个示例目录复制到 `<Ballance>/ModLoader/Mods`，启动 Player，先确认得到上表
中的结果，再修改源码。不要一次安装整个 `script-mod` 目录：每个示例都有独立
的 Mod 身份。

这些是单一主题示例，不是完整项目骨架。实际项目仍应从模板开始，只复制所需的
部分。完整指南位于 `share/BML/docs/en/script-mod` 和
`share/BML/docs/zh-CN/script-mod-tutorial`。

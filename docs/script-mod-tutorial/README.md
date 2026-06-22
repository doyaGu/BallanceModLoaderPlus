# Ballance 脚本 Mod 创作教程

## 先建立这张图

写脚本 mod 时，脑子里先放这几层：

```text
Ballance Player
  BMLPlus
    BML 脚本 mod
      AngelScript 语法
      BML API：日志、窗口、命令、配置、Timer、事件
      CKAS API：对象、组、DataArray、行为图、物理
        Virtools / Ballance 原版流程
```

前几章只站在 BML 脚本 mod 这一层。后面才逐步碰 CKAS、Virtools、DataArray、行为图和物理。

## 学习目标

走完主线后，应当能做到：

- 写出能加载的 BML 脚本 mod。
- 知道 BMLPlus、BML、AngelScript、CKAS、Virtools 的关系。
- 用脚本观察对象、组、DataArray 和物理事件。
- 生成脚本自己的物理化对象，并能清理。
- 做一个有命令、配置、ImGui 面板和清理路径的小 mod。
- 知道哪些修改低风险，哪些修改要先停下来分析。

## 怎么读

这套教程分成主线和进阶参考。

第一次学，按主线读：

```text
脚本先跑起来
看懂游戏运行时
进入行为图和物理
做一个可控小 mod
```

已经会写脚本，只想查能力，可以直接看进阶参考。

| 层级 | 推荐读法 |
| --- | --- |
| 只想知道脚本 mod 怎么跑 | 第一篇 |
| 想看懂 Ballance 运行时对象 | 第一篇 + 第二篇 |
| 想做物理和可控对象练习 | 第一篇到第三篇 |
| 想做一个完整小 mod | 第一篇到第四篇 |
| 已经有基础，想查 CKAS 能力 | 第五篇和附录 |

## 主线目录

文件名已经按下面的学习路线整理。

### 第一篇：脚本先跑起来

目标：得到一个能加载、能显示窗口、能接命令的小脚本。

0. [几个名字到底指什么](00-background.md)
1. [脚本 mod 的范围](01-script-mod-scope.md)
2. [AngelScript 最小语法](02-angelscript-minimum-syntax.md)
3. [第一个可加载脚本](03-first-loadable-script.md)
4. [BML 怎么调用脚本](04-bml-callback-model.md)
5. [让脚本在游戏里可见](05-logs-window-fps.md)
6. [命令](06-command.md)
7. [配置](07-config.md)
8. [Timer](08-timer.md)
9. [第一篇排错流程](09-first-stage-troubleshooting.md)
10. [怎么查脚本 API](10-how-to-read-script-api.md)
11. [第一篇收束：基础脚本模板](11-basic-script-template.md)

### 第二篇：看懂游戏运行时

目标：把 Ballance 里的球、机关、检查点、表和行为图放到同一张图里。

12. [Ballance 文件各自负责什么](12-ballance-file-roles.md)
13. [对象、实体和名字](13-objects-entities-and-names.md)
14. [组是对象名单](14-groups-are-object-lists.md)
15. [DataArray 是运行时表](15-dataarray-runtime-tables.md)
16. [行为图和 Building Block](16-behavior-graphs-and-building-blocks.md)
17. [用检查点串起四个概念](17-checkpoint-mental-model.md)
18. [第一关内容个案](18-level01-content-case-study.md)
19. [查找对象和组](19-find-objects-and-groups.md)
20. [DataArray 入门：CurrentLevel](20-currentlevel-dataarray.md)
21. [关卡配置表 AllLevel](21-alllevel-config-table.md)
22. [物理参数表 Physicalize_Balls](22-physicalize-balls.md)
23. [从 base.cmo 到一局游戏](23-runtime-flow-overview.md)
24. [Levelinit 如何处理作者标记](24-levelinit-author-markers.md)
25. [Gameplay 如何维持运行中状态](25-gameplay-runtime-state.md)

### 第三篇：行为图和物理

目标：从观察原版物理化，到生成并推动脚本自己的物理化对象。

26. [行为图阅读法](26-reading-behavior-graphs.md)
27. [对象接入物理系统](27-object-enters-physics.md)
28. [生成物理化对象](28-create-physicalized-object.md)
29. [推动物理对象](29-push-physics-object.md)

### 第四篇：做一个可控小 mod

目标：把命令、配置、UI、可控对象和清理路径组合起来。

30. [修改前风险分级](30-risk-classification-before-changes.md)
31. [创建自己的可控对象](31-create-your-own-controlled-object.md)
32. [一个完整小 mod](32-complete-small-mod.md)
33. [发布、版本和兼容性](33-release-version-compatibility.md)

## 进阶参考

这些章节不要求第一次按顺序读。遇到对应问题时再查。

A. [BML 生命周期总览](ref-a-bml-lifecycle-overview.md)
B. [游戏事件和关卡时机](ref-b-game-events-and-level-timing.md)
C. [ModContext 服务地图](ref-c-modcontext-service-map.md)
D. [路径、资源和日志诊断](ref-d-paths-resources-logs.md)
E. [输入、UI 和每帧状态](ref-e-input-ui-frame-state.md)
F. [脚本 mod 的边界](ref-f-script-mod-boundary.md)
G. [BML 进阶回调和事件对象](ref-g-advanced-callbacks-events.md)
H. [mod 信息、依赖和导出](ref-h-mod-info-dependencies-exports.md)
I. [DataShare 和跨脚本共享状态](ref-i-datashare-shared-state.md)
J. [BML HUD、菜单和消息服务](ref-j-bml-hud-menu-message-services.md)
K. [CKAS 三种脚本入口](ref-k-ckas-script-entrypoints.md)
L. [句柄、引用和对象生命周期](ref-l-handles-refs-lifetime.md)
M. [场景对象、层级和可见性](ref-m-scene-objects-hierarchy-visibility.md)
N. [DataArray 写入和回滚](ref-n-dataarray-write-rollback.md)
O. [资源、文本和 UI](ref-o-resources-text-ui.md)
P. [行为调用和脚本库组织](ref-p-behavior-calls-and-script-libraries.md)
Q. [球、地板和模块注册](ref-q-ball-floor-module-registration.md)

## 每章应该回答什么

后续维护正文时，每章都按这四个问题检查：

```text
这一章站在哪一层？
这一章要做成什么？
运行后看什么结果？
失败时先查哪里？
```

如果一章只能列 API，不能回答这些问题，就应该挪到进阶参考，或者拆成更小的任务。

## 附录

A. [术语表](appendix-a-glossary.md)
B. [常用 BML 回调和服务](appendix-b-bml-callbacks-services.md)
C. [常用 Ballance 对象和组名](appendix-c-ballance-names.md)
D. [常用 DataArray](appendix-d-dataarrays.md)

# Ballance Mod Loader Plus（BML+）

[English](README.md) | 简体中文

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Platform](https://img.shields.io/badge/platform-Windows-blue.svg)]()

面向 Ballance 的现代化 Mod Loader。基于 Gamepiaynmo 的 BallanceModLoader，全面重构与增强，提供更稳定的运行时、可扩展的 Mod API，以及更友好的游戏内 UI 与开发体验。

本仓库包含 BML+ 核心运行库（`BMLPlus.dll`）、公共头文件（`include/BML`），以及若干内建模块（HUD、命令栏、Mod 列表、物理与对象加载钩子等）。

**提示：BML+ 仅支持新版 Player（BallancePlayer）。不保证对原版 Player 的兼容性。**

## 目录

- [Ballance Mod Loader Plus（BML+）](#ballance-mod-loader-plusbml)
  - [目录](#目录)
  - [功能亮点](#功能亮点)
  - [截图展示](#截图展示)
  - [架构与模块](#架构与模块)
  - [运行时目录结构](#运行时目录结构)
  - [安装与卸载](#安装与卸载)
  - [使用与快捷键](#使用与快捷键)
  - [配置项速览（BML 内建）](#配置项速览bml-内建)
  - [从源码构建](#从源码构建)
  - [Mod 开发快速上手](#mod-开发快速上手)
  - [API 参考](#api-参考)
    - [核心接口](#核心接口)
    - [实用工具 API](#实用工具-api)
    - [事件系统](#事件系统)
  - [故障排除](#故障排除)
    - [常见问题](#常见问题)
      - [游戏无法启动或立即崩溃](#游戏无法启动或立即崩溃)
      - [Mod 未加载](#mod-未加载)
      - [性能问题](#性能问题)
      - [Unicode/字体问题](#unicode字体问题)
    - [调试信息](#调试信息)
  - [贡献指南](#贡献指南)
    - [开发环境设置](#开发环境设置)
    - [贡献指南](#贡献指南-1)
    - [代码风格](#代码风格)
    - [测试](#测试)
  - [测试与质量](#测试与质量)
  - [常见问题（FAQ）](#常见问题faq)
  - [许可与致谢](#许可与致谢)
  - [关联项目](#关联项目)
  - [支持与社区](#支持与社区)
  - [路线图](#路线图)
    - [计划功能](#计划功能)
    - [当前限制](#当前限制)
  - [性能说明](#性能说明)

## 功能亮点

**现代化 UI 覆盖层**：集成 ImGui，自适应 DPI，支持窗口化控件与高分辨率；内置消息板（支持 ANSI 256 色与滚动）与命令栏。

**深度引擎集成**：基于 Virtools CK2，引入渲染引擎钩子、物理钩子与对象加载拦截，支持全流程事件广播与行为树插桩（Hook Block）。

**完整 Mod 生态**：统一 Mod 生命周期与依赖管理，版本校验，稳定的 ABI/头文件。

**开发者工具**：命令系统（自动补全/历史/彩色输出）、定时器/调度、日志与配置系统（UTF-8/UTF-16）。

**画面与玩法增强**：可选解锁帧率、设置帧率上限、宽高比修正（Widescreen Fix）、灯笼材质优化、出生/重生延迟移除（Overclock）。

**多语言与 Unicode**：完善的字符串处理工具与编码转换，适配中/日/韩/西里尔等字库装载策略。

## 截图展示

*即将推出：游戏内界面截图，展示 Mod 界面、命令栏和 HUD 元素。*

## 架构与模块

- 入口与引导（`src/BML.cpp`）
  - 使用 MinHook 对 CK2/CK2_3D 与 Win32 消息循环进行注入。
  - 在 DLL 加载时安装渲染与窗口消息钩子，提供 ImGui 上下文与渲染后处理。
- 管理器与上下文（`src/ModManager.*`, `src/ModContext.*`）
  - `ModManager` 作为 CKBaseManager 驻留于引擎生命周期；`ModContext` 实现 `IBML` 接口，负责 Mod 装载、事件广播、命令、日志与配置管理。
  - Mod 发现与加载：扫描 `ModLoader/Mods` 下的 `.bmodp`（或压缩包解出后）并通过 `BMLEntry(IBML*)` 实例化。可选 `BMLExit(IMod*)` 进行释放。
  - 依赖管理：注册必选/可选依赖与版本约束，进行拓扑排序，避免循环依赖与不满足版本。
- 覆盖层与后端（`src/Overlay.*`, `src/imgui_impl_ck2.*`）
  - 通过 Win32 消息钩子（PeekMessage/GetMessage）注入输入，结合 CK2 渲染后端完成 UI 绘制。
- 引擎钩子（`src/RenderHook.*`, `src/PhysicsHook.cpp`, `src/ObjectLoadHook.cpp`）
  - 渲染：重定向 `CKRenderContext::Render` 和投影矩阵更新，提供可选宽屏修正。
  - 物理：截获 Physicalize 行为以向 Mod 广播物理化/反物理化回调参数。
  - 对象加载：扩展 Object Load，支持标记自定义地图名，并在脚本加载时广播回调。
- 行为树插桩（`src/HookBlock.cpp`, `src/ExecuteBB.cpp`, `include/BML/ScriptHelper.h`）
  - 通过 Hook Block 在关键行为图（如 `Event_handler`、`Gameplay_Ingame` 等）插入回调，向 Mod 广播关卡/菜单/计时等事件。
- 内建模块（`src/BMLMod.*`, `src/NewBallTypeMod.*`）
  - BMLMod：HUD/命令栏/消息板/Mods 菜单/自定义地图入口/帧率与画质调优等。
  - NewBallTypeMod：提供注册新球型/地面/模块的能力，并注入对应行为逻辑。
- 公共 API（`include/BML`）
  - `IMod`/`IBML`：Mod 生命周期、管理器访问、消息广播、依赖、注册能力。
  - `ICommand`：命令系统接口，带补全/解析工具。
  - `InputHook`：统一键鼠/手柄输入封装，可屏蔽输入并拿到原始状态。
  - `Timer`：多类型定时器（Once/Loop/Repeat/Interval/Debounce/Throttle）与链式构建器。
  - `DataShare API (BML_DataShare_*)`：跨 Mod 数据共享与订阅。
  - 工具：路径/字符串/内存/Zip/ANSI 调色板 等。

## 运行时目录结构

游戏根目录下将包含：
- `BuildingBlocks/BMLPlus.dll`：BML+ 主 DLL（CK 插件）。
- `ModLoader/`：BML+ 运行目录（自动创建）
  - `ModLoader/ModLoader.log`：日志输出。
  - `ModLoader/Configs/*.cfg`：各 Mod 配置（按 Mod ID 命名，例如 `BML.cfg`）。
  - `ModLoader/Mods/`：放置 `.bmodp` 包或压缩包。
  - `ModLoader/Fonts/`：可选字体（默认 `unifont.otf`）。
  - `ModLoader/Themes/` 与 `ModLoader/palette.ini`：ANSI 调色板主题与配置。

## 安装与卸载

1. 前往 Releases 页面下载最新构建。
2. 解压后将全部文件拷贝到 Ballance 安装目录。
   - `BuildingBlocks` 下应当出现 `BMLPlus.dll`。
   - 目录下应出现/创建 `ModLoader/`。
3. 运行 `Player.exe`。

卸载：删除 `BuildingBlocks/BMLPlus.dll`。若需清理所有数据（Mod/地图/配置等），一并删除 `ModLoader/` 目录。

## 使用与快捷键

- 命令栏：默认按下 `/` 打开/关闭。支持历史上下、Tab 补全与 ANSI 彩色输出。
- Mods 菜单：在游戏“选项”菜单中自动注入“Mod List”按钮。
- 自定义地图：主菜单右侧会显示“Enter_Custom_Maps”按钮，可浏览 `ModLoader/Maps`（运行期生成的临时目录）。

内置命令示例：
- `help`/`?`：列出所有命令与说明。
- `bml`：显示 BML+ 版本与已加载 Mod。
- `cheat on|off`：启用/关闭作弊模式（会广播到 Mod）。
- `hud [title|fps|sr] [on|off]`：切换 HUD 元素。
- `palette ...`：管理 ANSI 256 色主题（导入/应用/调整）。
- 其他：`echo`、`clear`、`history`、`exit` 等。

## 配置项速览（BML 内建）

可在 `ModLoader/Configs/BML.cfg` 中修改，主要包含：
- GUI：主/次字体文件与大小、字形范围（Chinese/ChineseFull 等）、是否保存 ImGui 布局。
- HUD：标题/FPS/SR 计时显示。
- Graphics：
  - `UnlockFrameRate`：解锁帧率（关闭同步/限制）。
  - `SetMaxFrameRate`：帧率上限；设为 0 则启用垂直同步。
  - `WidescreenFix`：宽屏视野修正。
- Tweak：`LanternAlphaTest`、`FixLifeBallFreeze`、`Overclock`（去除重生/生成延迟）。
- CommandBar：消息停留时长、制表宽度、背景透明度与最大淡入透明度。
- CustomMap：默认关卡编号、是否显示悬浮提示、递归目录深度。

ANSI 调色板：使用 `ModLoader/palette.ini` 与 `ModLoader/Themes/` 自定义；支持 #RRGGBB/#AARRGGBB、RGB(A) 数值与多主题混合。

## 从源码构建

依赖环境：
- Windows（仅支持）
- Visual Studio 2019+（C++20）
- CMake 3.14+
- 已安装 Virtools SDK（设置 `VIRTOOLS_SDK_PATH` 或 CMake 变量）

拉取与编译：
```bash
git clone --recursive https://github.com/doyaGu/BallanceModLoaderPlus.git
cd BallanceModLoaderPlus

# Release 构建
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# 可选：启用测试
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DBML_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release
```

生成物：`build/bin/BMLPlus.dll`、头文件位于 `include/BML`。

## Mod 开发快速上手

入口与导出：
- 每个 Mod 为一个原生 DLL，导出 `BMLEntry(IBML*) -> IMod*`；可选导出 `BMLExit(IMod*)` 做清理。

示例（最小实现）：
```cpp
#include <BML/IMod.h>
using namespace std;

class HelloMod final : public IMod {
public:
    explicit HelloMod(IBML* bml) : IMod(bml) {}
    const char* GetID() override        { return "Hello"; }
    const char* GetVersion() override   { return "1.0.0"; }
    const char* GetName() override      { return "Hello Mod"; }
    const char* GetAuthor() override    { return "You"; }
    const char* GetDescription() override { return "Sample mod"; }
    DECLARE_BML_VERSION; // 声明兼容当前 BML 版本

    void OnLoad() override {
        GetLogger()->Info("Hello from HelloMod!");
        m_BML->SendIngameMessage("\x1b[32mHello BML+!\x1b[0m");
        m_BML->AddTimer(1000ul, [](){ /* 1 秒后执行 */ });
    }
};

extern "C" __declspec(dllexport) IMod* BMLEntry(IBML* bml) {
    return new HelloMod(bml);
}

extern "C" __declspec(dllexport) void BMLExit(IMod* mod) {
    delete mod;
}
```

关键 API：
- `IBML`：获取 CK 管理器、发送游戏内消息、注册命令、添加定时器、设置作弊、注册球体/地面/模块、依赖管理等（见 `include/BML/IBML.h`）。
- `ICommand`：实现自定义命令（见 `include/BML/ICommand.h`）。
- `InputHook`：读取/屏蔽输入（见 `include/BML/InputHook.h`）。
- `Timer`：高级计时与节流/防抖（见 `include/BML/Timer.h`）。
- `DataShare API (BML_DataShare_*)`：跨 Mod 数据共享（见 `include/BML/DataShare.h` 与 `src/DataShare.*`）。

依赖声明：
- 在 Mod 构造或 `OnLoad` 阶段通过 `AddDependency("OtherMod", {1,2,3})` 与 `AddOptionalDependency(...)` 注册。
- 使用 `DECLARE_BML_VERSION` 告诉 Loader 你的 Mod 需要的 BML 版本。

CMake 集成示例（可选安装 BML 后）：
```cmake
find_package(BML REQUIRED)
add_library(MyMod SHARED MyMod.cpp)
target_include_directories(MyMod PRIVATE ${BML_INCLUDE_DIRS})
target_link_libraries(MyMod PRIVATE BML)
set_target_properties(MyMod PROPERTIES OUTPUT_NAME "MyMod")
```

开箱即用模板：见 templates/mod-template（包含 CMake、BMLEntry/BMLExit 与示例命令）。

## API 参考

### 核心接口

- **IBML**：主要接口，提供 CK 管理器访问（渲染、输入、时间等）、定时器管理、作弊控制、对象查找、球体/地面/模块注册和 Mod 依赖系统
- **IMod**：所有 Mod 的基类，包含生命周期方法（OnLoad、OnUnload、OnProcess 等）
- **ICommand**：创建自定义命令的接口，支持 Tab 自动补全
- **IConfig**：配置管理，支持 UTF-8/UTF-16
- **ILogger**：日志系统，支持多级别和 ANSI 彩色输出

### 实用工具 API

- **InputHook**：全面的输入处理，支持键盘、鼠标、手柄输入，输入屏蔽和原始状态访问
- **Timer**：高级定时系统，支持多种类型（Once、Loop、Repeat、Interval、Debounce、Throttle）、流式构建器模式和链式调用
- **DataShare**：C 风格的跨 Mod 数据共享 API，支持引用计数和回调订阅
- **StringUtils**：Unicode 字符串处理和转换工具
- **PathUtils**：文件系统操作和路径处理

### 事件系统

Mod 可以注册各种游戏事件：
- 关卡开始/结束事件
- 玩家出生/死亡事件
- 物理事件（对象物理化）
- 渲染事件（渲染前/后）
- 自定义 Mod 事件

详细的 API 文档请参见 `include/BML/` 中的头文件。

## 故障排除

### 常见问题

#### 游戏无法启动或立即崩溃
- 确保 `BuildingBlocks/BMLPlus.dll` 在正确位置
- 检查是否安装了 Visual C++ Redistributable 2015-2022
- 移除之前的 Mod Loader 残留文件
- 查看 `ModLoader/ModLoader.log` 中的错误消息

#### Mod 未加载
- 确认 Mod 文件放置在 `ModLoader/Mods/` 目录中
- 检查 Mod 是否有 `.bmodp` 扩展名或正确解压
- 查看日志文件中的依赖关系
- 确保 Mod 与您的 BML+ 版本兼容

#### 性能问题
- 在 `ModLoader/Configs/BML.cfg` 中禁用不必要的视觉效果
- 如果出现卡顿，降低最大帧率
- 检查可能影响性能的冲突 Mod

#### Unicode/字体问题
- 将适当的字体文件放置在 `ModLoader/Fonts/` 中
- 在 `BML.cfg` 的 GUI 部分配置字体设置
- 确保为您的语言设置了正确的字形范围

### 调试信息

排除故障时的帮助：
1. 查看 `ModLoader/ModLoader.log` 中的详细错误消息
2. 在游戏内使用 `/bml` 命令验证 BML+ 版本和已加载的 Mod
3. 如果可用，在 Mod 配置中启用详细日志记录
4. 使用最少的 Mod 集合进行测试以隔离问题

## 贡献指南

我们欢迎对 BML+ 的贡献！以下是您可以帮助的方式：

### 开发环境设置

1. Fork 该仓库
2. 克隆子模块：`git clone --recursive <your-fork>`
3. 设置构建环境（Visual Studio 2019+，CMake 3.14+）
4. 安装 Virtools SDK 并设置 `VIRTOOLS_SDK_PATH`

### 贡献指南

- 遵循现有的代码风格和约定
- 为新功能编写测试
- 更新 API 变更的文档
- 使用多个 Mod 进行测试以确保兼容性
- 向 `main` 分支提交 Pull Request

### 代码风格

- 适当使用 C++20 特性
- 遵循 RAII 原则
- 使用智能指针进行内存管理
- 保持公共 API 稳定且文档完善
- 遵循现有命名约定

### 测试

- 运行完整测试套件：`ctest --test-dir build -C Release`
- 使用真实 Mod 和游戏场景进行测试
- 验证与现有 Mod 的兼容性
- 检查内存泄漏和性能影响

## 测试与质量

- 测试框架：GoogleTest（`tests/`）。核心工具类（Timer/PathUtils/StringUtils/Config/AnsiPalette）均有单元测试。
- 启用：`-DBML_BUILD_TESTS=ON` 并在构建后运行 `ctest`。

## 常见问题（FAQ）

- 如何确认 BML+ 是否正确加载？
  - 进入游戏后左上角会显示版本信息；`ModLoader/ModLoader.log` 中包含初始化日志；命令栏输入 `bml` 查看。
- 游戏未加载/崩溃？
  - 确认 `BuildingBlocks/BMLPlus.dll` 放置正确，移除旧版残留文件；安装 Visual C++ 2015–2022 运行库；仍有问题请附日志到 Issues。
- BML+ 是否兼容旧版 `.bmod`？
  - 不兼容。BML+ 使用全新机制，Mod 以 DLL（可使用 `.bmodp` 扩展名）形式，通过 `BMLEntry` 入口加载。
- 默认快捷键？
  - 命令栏默认 `/`，其余操作依游戏 UI 与 Mod 而定。

## 许可与致谢

- 许可：MIT（见 LICENSE）。
- 鸣谢：Gamepiaynmo（BallanceModLoader 原作者）、Ballance 社区的贡献者与测试者。

## 关联项目

- 新版 Player：https://github.com/doyaGu/BallancePlayer

## 支持与社区

- **问题报告与 Bug 反馈**：https://github.com/doyaGu/BallanceModLoaderPlus/issues
- **功能请求**：使用 Issue 跟踪器并添加"enhancement"标签
- **文档**：查看 `include/BML/` 头文件和本 README
- **社区 Mod**：查看 releases 页面获取社区贡献的 Mod

## 路线图

### 计划功能

- [ ] 开发期间 Mod 热重载支持
- [ ] 增强脚本支持与 AngelScript 集成
- [ ] 内置 Mod 浏览器和自动更新
- [ ] 扩展物理模拟钩子
- [ ] 网络多人游戏支持框架
- [ ] 高级调试工具和性能分析器

### 当前限制

- 仅支持 Windows（由于 Virtools CK2 依赖）
- 需要新版 Player（BallancePlayer）
- 不向后兼容旧版 `.bmod` 文件
- 限于 Virtools CK2 引擎功能

## 性能说明

BML+ 设计为最小性能影响：
- 渲染钩子每帧增加 <1ms
- 内存开销通常 <10MB
- Mod 加载通过延迟初始化优化
- 后台操作尽可能使用独立线程

最佳性能建议：
- 生产环境使用 Release 构建
- 限制活动定时器数量
- 优化 Mod 更新频率
- 高效使用 DataShare API

遇到问题或建议，欢迎提交 Issue：https://github.com/doyaGu/BallanceModLoaderPlus/issues

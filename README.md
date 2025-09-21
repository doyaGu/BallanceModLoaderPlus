# Ballance Mod Loader Plus (BML+)

English | [简体中文](README_zh-CN.md)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Platform](https://img.shields.io/badge/platform-Windows-blue.svg)]()

Modern mod loader for Ballance. BML+ is a reworked and enhanced successor to the original BallanceModLoader, featuring a stable runtime, extensible modding APIs, and developer‑friendly in‑game tooling.

This repository contains the core runtime (`BMLPlus.dll`), public headers (`include/BML`), and several built‑in modules (HUD, command bar, mod list, engine hooks for rendering/physics/object‑loading, etc.).

**Note: BML+ targets the New Player (BallancePlayer). The original player is not supported.**

## Table of Contents

- [Ballance Mod Loader Plus (BML+)](#ballance-mod-loader-plus-bml)
  - [Table of Contents](#table-of-contents)
  - [Highlights](#highlights)
  - [Screenshots](#screenshots)
  - [Architecture \& Modules](#architecture--modules)
  - [Runtime Layout](#runtime-layout)
  - [Install \& Uninstall](#install--uninstall)
  - [Usage \& Hotkeys](#usage--hotkeys)
  - [Config Quick Reference (core BML)](#config-quick-reference-core-bml)
  - [Build from Source](#build-from-source)
  - [Mod Development Quickstart](#mod-development-quickstart)
  - [API Reference](#api-reference)
    - [Core Interfaces](#core-interfaces)
    - [Utility APIs](#utility-apis)
    - [Event System](#event-system)
  - [Troubleshooting](#troubleshooting)
    - [Common Issues](#common-issues)
      - [Game won't start or crashes immediately](#game-wont-start-or-crashes-immediately)
      - [Mods not loading](#mods-not-loading)
      - [Performance issues](#performance-issues)
      - [Unicode/Font issues](#unicodefont-issues)
    - [Debug Information](#debug-information)
  - [Contributing](#contributing)
    - [Development Setup](#development-setup)
    - [Guidelines](#guidelines)
    - [Code Style](#code-style)
    - [Testing](#testing)
  - [Testing \& Quality](#testing--quality)
  - [FAQ](#faq)
  - [License \& Acknowledgments](#license--acknowledgments)
  - [Related](#related)
  - [Support and Community](#support-and-community)
  - [Roadmap](#roadmap)
    - [Planned Features](#planned-features)
    - [Current Limitations](#current-limitations)
  - [Performance Notes](#performance-notes)

## Highlights

**Modern overlay UI**: Built on ImGui, DPI‑aware, high‑DPI/hi‑res friendly; includes a scrollable message board (ANSI 256‑color) and an in‑game command bar.

**Deep engine integration**: Hooks into Virtools CK2 for render, physics and object loading; full event broadcast and behavior‑graph instrumentation (Hook Block).

**Complete mod ecosystem**: Unified mod lifecycle and dependency management with version checks; stable headers/ABI.

**Developer tools**: Command system (tab completion/history/colored output), timers/scheduling, logging and configuration (UTF‑8/UTF‑16).

**Visual/gameplay tweaks**: Unlock framerate, max framerate limit, widescreen FOV fix, lantern material tweaks, and respawn/spawn delay removal (Overclock).

**Unicode/i18n**: Robust string/path utilities and conversion helpers; flexible font loading for CJK/Cyrillic, etc.

## Screenshots

*Coming soon: In-game screenshots showing the mod interface, command bar, and HUD elements.*

## Architecture & Modules

- Entry & bootstrap (`src/BML.cpp`)
  - Uses MinHook to patch CK2/CK2_3D and Win32 message loop.
  - Installs render and Win32 message hooks; creates ImGui context and render pass.
- Manager & context (`src/ModManager.*`, `src/ModContext.*`)
  - `ModManager` lives as a CKBaseManager; `ModContext` implements `IBML` and handles mod loading, event broadcast, commands, logging and configuration.
  - Mod discovery/loading: scans `ModLoader/Mods` for `.bmodp` (or extracted zips) and instantiates via `BMLEntry(IBML*)`. Optional `BMLExit(IMod*)` for cleanup.
  - Dependency management: required/optional deps with version bounds, stable topological ordering; detects cycles/missing deps.
- Overlay & backends (`src/Overlay.*`, `src/imgui_impl_ck2.*`)
  - Hooks PeekMessage/GetMessage to inject inputs; renders through CK2 backend.
- Engine hooks (`src/RenderHook.*`, `src/PhysicsHook.cpp`, `src/ObjectLoadHook.cpp`)
  - Render: redirects `CKRenderContext::Render` and projection updates; optional widescreen FOV fix.
  - Physics: intercepts Physicalize to broadcast on‑physicalize/unphysicalize with detailed params.
  - Object loading: extends Object Load, tracks custom map names, and broadcasts script load events.
- Behavior instrumentation (`src/HookBlock.cpp`, `src/ExecuteBB.cpp`, `include/BML/ScriptHelper.h`)
  - Inserts callbacks into key graphs (e.g. `Event_handler`, `Gameplay_Ingame`) to broadcast menu/level/timer events.
- Built‑ins (`src/BMLMod.*`, `src/NewBallTypeMod.*`)
  - BMLMod: HUD/command bar/message board/Mods menu/custom map entry/framerate & visual tweaks.
  - NewBallTypeMod: register new ball/floor/module types and inject related logic.
- Public API (`include/BML`)
  - `IMod`/`IBML`: lifecycle, managers access, messaging, deps, registration.
  - `ICommand`: command interface with helpers for parsing/completion.
  - `InputHook`: unified input access with blocking/original state helpers.
  - `Timer`: Once/Loop/Repeat/Interval/Debounce/Throttle with fluent builder and chaining.
  - `DataShare API (BML_DataShare_*)`: inter‑mod data sharing and subscriptions.
  - Utilities: path/string/memory/zip/ANSI palette tools.

## Runtime Layout

Under the game root you will see:
- `BuildingBlocks/BMLPlus.dll`: the BML+ CK plugin.
- `ModLoader/` (auto‑created):
  - `ModLoader/ModLoader.log`: runtime log.
  - `ModLoader/Configs/*.cfg`: per‑mod config files (named by mod ID, e.g. `BML.cfg`).
  - `ModLoader/Mods/`: drop `.bmodp` packages (or zips which will be extracted to a temp location).
  - `ModLoader/Fonts/`: optional fonts (default `unifont.otf`).
  - `ModLoader/Themes/` and `ModLoader/palette.ini`: ANSI palette themes and config.

## Install & Uninstall

1) Download the latest release. 2) Extract to Ballance’s install folder.
- `BuildingBlocks/BMLPlus.dll` must exist.
- `ModLoader/` will be created on first run.
- Launch `Player.exe`.

Uninstall: remove `BuildingBlocks/BMLPlus.dll`. To wipe data (mods/maps/configs), remove the `ModLoader/` folder.

## Usage & Hotkeys

- Command bar: press `/` to toggle. Supports history (Up/Down), tab completion and ANSI colored output.
- Mods menu: a “Mod List” button is injected into the game’s Options menu.
- Custom maps: a “Enter_Custom_Maps” button appears at the main menu to browse maps (temporary folder managed by BML+).

Built‑in commands:
- `help`/`?`: list commands.
- `bml`: show BML+ version and loaded mods.
- `cheat on|off`: toggle cheat mode (broadcast to mods).
- `hud [title|fps|sr] [on|off]`: toggle HUD elements.
- `palette ...`: manage ANSI 256‑color themes.
- Others: `echo`, `clear`, `history`, `exit`.

## Config Quick Reference (core BML)

Edit `ModLoader/Configs/BML.cfg`:
- GUI: primary/secondary font files & sizes, glyph ranges (Chinese/ChineseFull, etc.), ImGui ini saving.
- HUD: ShowTitle/ShowFPS/ShowSRTimer.
- Graphics:
  - `UnlockFrameRate`: disable sync/limit.
  - `SetMaxFrameRate`: framerate cap; set 0 for VSync.
  - `WidescreenFix`: widescreen FOV fix.
- Tweak: `LanternAlphaTest`, `FixLifeBallFreeze`, `Overclock`.
- CommandBar: message lifetime, tab width, window/message alpha, fade max alpha.
- CustomMap: default level number, tooltip visibility, max directory depth.

ANSI palette is controlled by `ModLoader/palette.ini` and `ModLoader/Themes/`. Supports #RRGGBB/#AARRGGBB and numeric RGBA, with mixing options.

## Build from Source

Requirements:
- Windows only, Visual Studio 2019+ (C++20)
- CMake 3.14+
- Virtools SDK installed (`VIRTOOLS_SDK_PATH` or CMake cache)

Build:
```bash
git clone --recursive https://github.com/doyaGu/BallanceModLoaderPlus.git
cd BallanceModLoaderPlus

cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Optional: tests
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DBML_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release
```

Artifacts: `build/bin/BMLPlus.dll`; public headers under `include/BML`.

## Mod Development Quickstart

Entry points:
- Export `BMLEntry(IBML*) -> IMod*`; optionally export `BMLExit(IMod*)` for cleanup.

Minimal example:
```cpp
#include <BML/IMod.h>

class HelloMod final : public IMod {
public:
    explicit HelloMod(IBML* bml) : IMod(bml) {}
    const char* GetID() override        { return "Hello"; }
    const char* GetVersion() override   { return "1.0.0"; }
    const char* GetName() override      { return "Hello Mod"; }
    const char* GetAuthor() override    { return "You"; }
    const char* GetDescription() override { return "Sample mod"; }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        GetLogger()->Info("Hello from HelloMod!");
        m_BML->SendIngameMessage("\x1b[32mHello BML+!\x1b[0m");
        m_BML->AddTimer(1000ul, [](){});
    }
};

extern "C" __declspec(dllexport) IMod* BMLEntry(IBML* bml) { return new HelloMod(bml); }
extern "C" __declspec(dllexport) void BMLExit(IMod* mod) { delete mod; }
```

Key APIs:
- `IBML`: managers, in‑game message, commands, timers, cheat, ball/floor/module registration, dependency APIs.
- `ICommand`: create custom commands.
- `InputHook`: read/block inputs.
- `Timer`: advanced timing and debounce/throttle helpers.
- `DataShare API (BML_DataShare_*)`: cross‑mod data sharing.

Declaring deps:
- Call `AddDependency("OtherMod", {major,minor,patch})` or `AddOptionalDependency(...)` during construction/`OnLoad`.
- Use `DECLARE_BML_VERSION` to declare your BML requirement.

Optional CMake integration (after installing BML):
```cmake
find_package(BML REQUIRED)
add_library(MyMod SHARED MyMod.cpp)
target_include_directories(MyMod PRIVATE ${BML_INCLUDE_DIRS})
target_link_libraries(MyMod PRIVATE BML)
set_target_properties(MyMod PROPERTIES OUTPUT_NAME "MyMod")
```

Quick start template: see templates/mod-template for a ready-to-build example with CMake, BMLEntry/BMLExit and a sample command.

## API Reference

### Core Interfaces

- **IBML**: Main interface providing access to CK managers (render, input, time, etc.), timer management, cheat controls, object lookup, ball/floor/module registration, and mod dependency system
- **IMod**: Base class for all mods with lifecycle methods (OnLoad, OnUnload, OnProcess, etc.)
- **ICommand**: Interface for creating custom commands with tab completion support
- **IConfig**: Configuration management with UTF-8/UTF-16 support
- **ILogger**: Logging system with multiple levels and ANSI color support

### Utility APIs

- **InputHook**: Comprehensive input handling with keyboard, mouse, joystick support, input blocking, and original state access
- **Timer**: Advanced timing system with multiple types (Once, Loop, Repeat, Interval, Debounce, Throttle), fluent builder pattern, and chaining support
- **DataShare**: C-style API for cross-mod data sharing with reference counting and callback subscriptions
- **StringUtils**: Unicode string processing and conversion utilities
- **PathUtils**: File system operations and path manipulation

### Event System

Mods can register for various game events:
- Level start/end events
- Player spawn/death events
- Physics events (object physicalization)
- Render events (pre/post render)
- Custom mod events

For detailed API documentation, see the headers in `include/BML/`.

## Troubleshooting

### Common Issues

#### Game won't start or crashes immediately
- Ensure `BuildingBlocks/BMLPlus.dll` is in the correct location
- Check that you have Visual C++ Redistributable 2015-2022 installed
- Remove any leftover files from previous mod loaders
- Check `ModLoader/ModLoader.log` for error messages

#### Mods not loading
- Verify mods are placed in `ModLoader/Mods/` directory
- Check that mods have the `.bmodp` extension or are properly extracted
- Review mod dependencies in the log file
- Ensure mods are compatible with your BML+ version

#### Performance issues
- Disable unnecessary visual effects in `ModLoader/Configs/BML.cfg`
- Lower the maximum frame rate if experiencing stuttering
- Check for conflicting mods that might affect performance

#### Unicode/Font issues
- Place appropriate font files in `ModLoader/Fonts/`
- Configure font settings in the GUI section of `BML.cfg`
- Ensure proper glyph ranges are set for your language

### Debug Information

To help with troubleshooting:
1. Check `ModLoader/ModLoader.log` for detailed error messages
2. Use the `/bml` command in-game to verify BML+ version and loaded mods
3. Enable verbose logging in mod configurations if available
4. Test with a minimal set of mods to isolate issues

## Contributing

We welcome contributions to BML+! Here's how you can help:

### Development Setup

1. Fork the repository
2. Clone with submodules: `git clone --recursive <your-fork>`
3. Set up the build environment (Visual Studio 2019+, CMake 3.14+)
4. Install Virtools SDK and set `VIRTOOLS_SDK_PATH`

### Guidelines

- Follow the existing code style and conventions
- Write tests for new functionality
- Update documentation for API changes
- Test with multiple mods to ensure compatibility
- Submit pull requests against the `main` branch

### Code Style

- Use C++20 features appropriately
- Follow RAII principles
- Use smart pointers for memory management
- Keep the public API stable and well-documented
- Follow the existing naming conventions

### Testing

- Run the full test suite: `ctest --test-dir build -C Release`
- Test with real mods and game scenarios
- Verify compatibility with existing mods
- Check memory leaks and performance impact

## Testing & Quality

- Uses GoogleTest (`tests/`). Core utilities (Timer/PathUtils/StringUtils/Config/AnsiPalette) are covered.
- Enable with `-DBML_BUILD_TESTS=ON`, then run `ctest`.

## FAQ

- How to verify BML+ is loaded?
  - Version text at top‑left; see `ModLoader/ModLoader.log`; run `bml` in the command bar.
- Game doesn’t load or crashes?
  - Ensure `BuildingBlocks/BMLPlus.dll` is in place; remove leftovers from older loaders; install MSVC 2015–2022 VC++ Redistributable; attach logs when filing issues.
- Is BML+ compatible with legacy `.bmod`?
  - No. BML+ uses new mechanisms; mods are DLLs (often shipped as `.bmodp`) loaded via `BMLEntry`.
- Default hotkey?
  - Command bar: `/`. Others depend on the UI/mods.

## License & Acknowledgments

- License: MIT (see LICENSE).
- Thanks: Gamepiaynmo (original BallanceModLoader) and the Ballance modding community.

## Related

- New Player: https://github.com/doyaGu/BallancePlayer

## Support and Community

- **Issues and Bug Reports**: https://github.com/doyaGu/BallanceModLoaderPlus/issues
- **Feature Requests**: Use the issue tracker with the "enhancement" label
- **Documentation**: See `include/BML/` headers and this README
- **Community Mods**: Check the releases page for community-contributed mods

## Roadmap

### Planned Features

- [ ] Hot-reload support for mods during development
- [ ] Enhanced scripting support with AngelScript integration
- [ ] Built-in mod browser and automatic updates
- [ ] Extended physics simulation hooks
- [ ] Network multiplayer support framework
- [ ] Advanced debugging tools and profiler

### Current Limitations

- Windows-only (due to Virtools CK2 dependency)
- Requires New Player (BallancePlayer)
- No backward compatibility with legacy `.bmod` files
- Limited to Virtools CK2 engine features

## Performance Notes

BML+ is designed for minimal performance impact:
- Rendering hooks add <1ms per frame
- Memory overhead is typically <10MB
- Mod loading is optimized with lazy initialization
- Background operations use separate threads when possible

For best performance:
- Use Release builds for production
- Limit the number of active timers
- Optimize mod update frequencies
- Use the DataShare API efficiently

Issues and suggestions: https://github.com/doyaGu/BallanceModLoaderPlus/issues

# Ballance Mod Loader Plus (BML+)

English | [简体中文](README_zh-CN.md)

Current version: v0.3.13

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
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
  - [Current Limitations](#current-limitations)
  - [Performance Notes](#performance-notes)

## Highlights

**Modern overlay UI**: Built on ImGui, DPI‑aware, high‑DPI/hi‑res friendly; includes a scrollable message board (ANSI 256‑color) and an in‑game command bar.

**Deep engine integration**: Hooks into Virtools CK2 for render, physics and object loading; full event broadcast and behavior‑graph instrumentation (Hook Block).

**Complete mod ecosystem**: Unified mod lifecycle and dependency management with version checks and an explicit installed SDK surface.

**Developer tools**: Command system (tab completion/history/colored output), timers/scheduling, logging, typed configuration properties, and optional AngelScript script mods.

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
  - `IBML::AddTimer*`: loader-owned one-shot and loop scheduling.
  - `DataShare API (BML_DataShare_*)`: inter‑mod data sharing and subscriptions.
  - IMC: typed cross-mod RPC/Topic transport plus built-in Runtime, Scene, Gameplay, UI, and Events facades.
  - UI: Ballance-styled ImGui helpers (`Bui`), legacy Virtools UI wrappers (`BGui`), and the `BML::UI` IMC service.
  - `BML.h`: C-callable version, memory, string, path, file, and zip helpers.

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
- Python 3.10+ (interface binding generation)
- Virtools SDK installed (`VIRTOOLS_SDK_PATH` or CMake cache)
- CKAngelScript API 6+ only when configuring with `-DBML_ENABLE_ANGELSCRIPT=ON`

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
- `DataShare API (BML_DataShare_*)`: cross‑mod data sharing.
- IMC (`BML::Imc` and generated bindings): typed cross-mod RPC and Topics.

Declaring deps:
- Call `AddDependency("OtherMod", {major,minor,patch})` or `AddOptionalDependency(...)` during construction/`OnLoad`.
- Use `DECLARE_BML_VERSION` to declare your BML requirement.

Optional CMake integration (after installing BML):
```cmake
find_package(BML CONFIG REQUIRED)
bml_add_mod(MyMod MyMod.cpp)
```

`bml_add_mod` links `BML::BML`, enables C++20, and emits `MyMod.bmodp` directly.
Quick start template: see [templates/native-mod-template](templates/native-mod-template) for a ready-to-build example with CMake, BMLEntry/BMLExit and a sample command.

For a typed cross-mod RPC or Topic, the installed SDK also provides
`bml_target_imc_api()` and the IMC code generator. Start with the
[IMC overview](docs/imc.md), then follow the
[API authoring guide](docs/imc-author-guide.md) for a complete interface
definition, provider, client, and Topic example.
The [native API overview](docs/native-mod-api.md) maps the installed headers to
their intended use and explains the three UI surfaces.

## API Reference

### Core Interfaces

- **IBML**: Main interface providing access to CK managers (render, input, time, etc.), timer management, cheat controls, object lookup, ball/floor/module registration, and mod dependency system
- **IMod**: Base class for all mods with lifecycle methods (OnLoad, OnUnload, OnProcess, etc.)
- **ICommand**: Interface for creating custom commands with tab completion support
- **IConfig / IProperty**: Typed string, boolean, integer, float, and keyboard-key configuration properties
- **ILogger**: Info, warning, and error logging

### Utility APIs

- **InputHook**: Comprehensive input handling with keyboard, mouse, joystick support, input blocking, and original state access
- **IBML::AddTimer / AddTimerLoop**: Loader-owned scheduling by tick delay or time delay
- **DataShare**: C-style API for cross-mod data sharing with reference counting and callback subscriptions
- **IMC**: Typed RPC and Topic bindings generated from `.imc` interface files
- **BML.h**: C-callable string, encoding, path, file, memory, and zip helpers
- **Bui / BGui / BML::UI**: ImGui widgets, Virtools UI wrappers, and remote UI control respectively

### Event System

Native mods receive lifecycle and gameplay callbacks by overriding `IMod` and
`IMessageReceiver`. These include menu and level transitions, death/checkpoint/
life events, object and script loading, physicalize/unphysicalize, configuration
changes, command pre/post hooks, cheat changes, per-tick processing, and one
`OnRender(CK_RENDER_FLAGS)` callback. Consumers that do not need to inherit
`IMod` can subscribe to the built-in `BML::Events::Stream` IMC Topic.

For detailed API documentation, see the [native API overview](docs/native-mod-api.md)
and the headers installed by the BML SDK.

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

- Uses GoogleTest and Python integration tests (`tests/`) for runtime utilities, IMC generation/runtime behavior, scripting services, configuration, and input handling.
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

## Current Limitations

- Windows-only (due to Virtools CK2 dependency)
- Requires New Player (BallancePlayer)
- No backward compatibility with legacy `.bmod` files
- Limited to Virtools CK2 engine features

## Performance Notes

BML callbacks and synchronous IMC RPC execute on the game thread. Keep hot-path
work bounded and move expensive preparation outside `OnProcess`, `OnRender`, and
event callbacks. Topics use bounded queues and expose dropped-message counts, so
consumers should drain them regularly.

For production mods:
- Use Release builds for production
- Keep timer and per-frame callbacks short
- Cache object lookups that are stable across a level
- Prefer generated IMC bindings over hand-written payload handling
- Choose bounded Topic capacities and monitor `DroppedCount`

Issues and suggestions: https://github.com/doyaGu/BallanceModLoaderPlus/issues

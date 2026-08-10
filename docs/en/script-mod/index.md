# Script mods

A BML+ script mod is an AngelScript class loaded through CKAngelScript. BML+
adds mod identity, dependency ordering, lifecycle callbacks, resources,
configuration, commands, logging, timers, loader UI, DataShare, and typed
built-in services. CKAngelScript continues to own scene, behavior graph,
component, message, async, and raw CK/Vx APIs.

Use a native mod instead when the feature needs unsafe engine hooks,
performance-critical native loops, or a custom generated IMC Provider.

## Read by task

| Task | Page |
| --- | --- |
| Create, load, and package the first script mod | This page |
| Declare dependencies, implement callbacks, or use hot reload | [Lifecycle and reload](lifecycle.md) |
| Use ModContext, config, logging, input, timers, or commands | [BML services](services.md) |
| Work with CK objects, behavior graphs, physics, Hook Block, or UI | [Engine and UI](engine-ui.md) |
| Share data or design a reusable cross-mod service | [Communication](communication.md) |
| Look up exact declarations | [Script API reference](api.md) |

The API files linked from the reference page are editor declarations. Configure
them for completion and static checks; do not include them as runtime script
code.

## Requirements

- A BML+ release built with script support.
- The matching `BuildingBlocks/AngelScript.dll` from that release.
- Basic AngelScript class and function syntax. The first Mod does not require
  handles, interfaces, or delegates; learn those when an API first needs them.
- CKAngelScript documentation when working with Virtools objects, behavior
  graphs, components, messages, asynchronous work, or CK/Vx bindings.

Missing or incompatible CKAngelScript support is reported as a `ckas-host`
diagnostic. Native-only BML+ features remain available when the script host
cannot start.

## Create the first mod

Copy `templates/script-mod-template` from the BML+ SDK to
`ModLoader/Mods/HelloScript`. Its entry is deliberately small:

```angelscript
[bml.mod id="example.hello.script"
         name="Hello Script"
         version="1.0.0"
         author="Your Name"
         description="Minimal BML+ script mod"
         bml="0.3.13"]
class HelloScript {
    void OnLoad(const BML::ModContext &in ctx) {
        ctx.LogInfo("Hello Script loaded");
        BML::UI::AddMessage("Hello from your first script mod!");
    }
}
```

Start `Bin/Player.exe` once without editing the template. The in-game greeting
proves that `OnLoad` ran; the `Hello Script loaded` line in
`ModLoader/ModLoader.log` proves which Mod produced it. Together they confirm
that BML+ found the entry, compiled it through CKAngelScript, accepted the
metadata, created the main class, and called `OnLoad`.

After that first success, replace the example id, name, and author. Restart
Player once because the Mod identity changed; ordinary source edits after that
use automatic hot reload.

If it fails, run:

```text
script status
script diag example.hello
script logs error
```

Fix the first `ckas-host`, `compile`, or `metadata` error before debugging
callback behavior.

## Package shapes

BML+ accepts three script package forms under `ModLoader/Mods`:

- one `Foo.mod.as` file;
- a directory containing exactly one `*.mod.as` entry; or
- a zip containing exactly one `*.mod.as` entry.

Directory package:

```text
MyScriptMod/
  MyScript.mod.as       # the only BML+ entry
  libs/                 # files included by the entry through CKAngelScript
  Resources/            # textures, NMO/CMO/VMO assets, and data
  README.md
```

Zip package:

```text
MyScriptMod.zip
  MyScript.mod.as
  Resources/...
  README.md
```

The entry filename can be any name ending in `*.mod.as`. The class marked with
`[bml.mod]` can live in any AngelScript namespace. BML+ reads metadata from
CKAngelScript reflection; there is no separate BML manifest language.

Keep helper files under the owning mod directory. Do not use a shared global
`ModLoader/Mods/libs` directory: discovery and hot reload track one mod's entry
and source root. For a single-file mod, resources live in the sibling directory
named after the entry stem, such as `Mods/Foo/Resources` for `Foo.mod.as`.

`.bmodp` is reserved for native DLL mods.

## AngelScript rules used by BML+

- `T@` is a handle. Use `is null` and `!is null` for null and identity checks.
- Use `@field = handle` when rebinding a handle member.
- Script classes are reference types. BML+ retains a registered Timer, Command,
  DataShare request, callback, or delegate until it completes, is cancelled, or
  the mod unloads.
- Value classes such as event snapshots and definitions are copied by value.
  A method named `Borrow*` still returns a non-owning handle.
- Fixed callbacks use CKAngelScript no-suspend execution. Do not call an API
  that suspends from a BML+ callback, timer, command, or DataShare callback.
- Interface signatures must match exactly, including `const`, `&in`, return
  type, and method name.

The `@+` markers in editor declarations document native retention. They do not
change the syntax used when passing an ordinary script object or delegate.

## Develop and package

Directory and single-file mods use automatic source watching by default.
Saving a loaded source queues a reload. Adding a new mod, changing its id, or
changing its dependency graph still requires a Player restart. Zip packages
use manual reload and are intended for distribution testing.

Run the SDK tool from the Mod directory:

```powershell
Set-Location "<Ballance>/ModLoader/Mods/MyScriptMod"
& "<BML-SDK>/scripts/Pack-BMLScriptMod.ps1" -Force
```

It writes `dist/MyScriptMod.zip`. `-Source` and `-Output` remain available for
automation. The default package excludes editor settings, version-control
metadata, `as.predefined`, Python cache files, and `dist` itself.

Test the zip in a clean `ModLoader/Mods` directory without the development
directory or a single-file copy of the same mod id. Do not package editor API
stubs as runtime source.

Next: [Lifecycle and reload](lifecycle.md).

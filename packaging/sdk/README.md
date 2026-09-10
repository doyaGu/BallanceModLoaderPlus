# BML+ SDK

[简体中文](README_zh-CN.md)

This archive is for Mod development. It is not the BML+ runtime package and
must not be extracted into the Ballance directory.

## Choose a route

### Script Mod

Start here unless you specifically need native hooks, native memory access, a
generated IMC Provider, or a performance-critical native loop.

1. Open [`share/BML/docs/en/modding.md`](share/BML/docs/en/modding.md).
2. Create the Mod in `ModLoader/Mods`:

   ```powershell
   Set-Location "<Ballance>/ModLoader/Mods"
   & "<BML-SDK>/scripts/New-BMLScriptMod.ps1" `
     -Id "yourname.my-mod" -Name "My Mod" -Author "Your Name"
   ```

3. Follow the generated README and run the Mod unchanged before editing it.

You can copy [`templates/script-mod-template`](templates/script-mod-template)
manually when scripting the setup yourself.

After the template works, copy one directory from
[`examples/script-mod`](examples/script-mod) to learn commands and config,
input and UI, or game-state access from a runnable example.

Script support is present when `templates/script-mod-template` and
`docs/api/as.predefined` exist. The latter is an editor declaration file, not
runtime source.

### Native Mod

Use the native route for operations that genuinely require C++, the Virtools
SDK, or generated IMC services.

1. Open [`share/BML/docs/en/modding.md`](share/BML/docs/en/modding.md).
2. Create the project in your source workspace. Name and author are inferred
   when omitted:

   ```bat
   "<BML-SDK>\scripts\bml.cmd" new yourname.my-mod
   ```

3. Enter the project and run the complete development loop. Paths are remembered
   locally after the first run:

   ```bat
   .\bml run
   ```

   The first run asks for the Virtools SDK and Ballance folders, then remembers
   them. It builds and deploys the Mod, starts Player, waits for exit, and prints
   only this Mod's new log lines. `bml.cmd` is a small Windows launcher for the
   Python 3.10+ tool copied into the project as `bml.py`; it does not invoke
   PowerShell.

Already have a native Mod? Keep its source and CMake files:

```bat
"<BML-SDK>\scripts\bml.cmd" init owner.existing-mod --project "C:\path\to\mod"
```

This adds the local Python workflow without rewriting the project. Continuing
to use CMake directly is also supported.

You can also copy [`templates/native-mod-template`](templates/native-mod-template)
manually.

For a native-only provider interface, see the two independent projects in
[`examples/native-interface-provider`](examples/native-interface-provider) and
[`examples/native-interface-consumer`](examples/native-interface-consumer).
The provider uses `bml_add_interface_package`; the consumer obtains only the
installed header target and never links the provider binary.

## Where things are

| Path | Purpose |
| --- | --- |
| `templates/` | Runnable starting projects |
| `examples/` | Focused follow-up examples |
| `share/BML/docs/en/` | English Mod author documentation |
| `share/BML/docs/zh-CN/` | Chinese Mod author documentation |
| `docs/api/` | AngelScript editor declarations, when script support is enabled |
| `include/`, `lib/` | Native headers, libraries, and CMake package files |
| `share/BML/tools/` | Native interface and IMC code generators |
| `scripts/` | Mod creation and script Mod packaging tools |

Use the runtime release `BMLPlus-<version>.zip` to install BML+ into the game.
Do not use an SDK archive as a runtime package.

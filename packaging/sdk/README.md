# BML+ SDK

[简体中文](README_zh-CN.md)

This archive is for Mod development. It is not the BML+ runtime package and
must not be extracted into the Ballance directory.

## Choose a route

### Script Mod

Start here unless you specifically need native hooks, native memory access, a
generated IMC Provider, or a performance-critical native loop.

1. Open [`share/BML/docs/en/modding.md`](share/BML/docs/en/modding.md).
2. Copy [`templates/script-mod-template`](templates/script-mod-template) to
   `<Ballance>/ModLoader/Mods/<YourMod>`.
3. Follow the template README to run it unchanged before editing it.

Script support is present when `templates/script-mod-template` and
`docs/api/as.predefined` exist. The latter is an editor declaration file, not
runtime source.

### Native Mod

Use the native route for operations that genuinely require C++, the Virtools
SDK, or generated IMC services.

1. Open [`share/BML/docs/en/modding.md`](share/BML/docs/en/modding.md).
2. Copy [`templates/native-mod-template`](templates/native-mod-template).
3. Follow its README to configure an x86 build against this SDK and the
   Virtools SDK.

## Where things are

| Path | Purpose |
| --- | --- |
| `templates/` | Runnable starting projects |
| `share/BML/docs/en/` | English Mod author documentation |
| `share/BML/docs/zh-CN/` | Chinese Mod author documentation |
| `docs/api/` | AngelScript editor declarations, when script support is enabled |
| `include/`, `lib/` | Native headers, libraries, and CMake package files |
| `share/BML/tools/` | Generated IMC tooling |
| `scripts/` | Script Mod packaging tools |

Use the runtime release `BMLPlus-<version>.zip` to install BML+ into the game.
Do not use an SDK archive as a runtime package.

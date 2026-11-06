# Support and validation

This page separates available build targets from completed game acceptance.
The record below is a **2026-11-06 local validation snapshot**, not a promise
about an arbitrary release archive or future commit.

## Runtime scope

- This branch accepts `BML_RUNTIME=sdl` only and uses Ballanced SDL CK2/VxMath.
- Dynamic hosts are implemented. `BML_HOST_LINKAGE=static` is rejected at
  configure time; updater support for static-package metadata is not static
  Loader integration.
- Native Mods use `bml.mod` major 1 and schema-2 ABI sidecars. Old SDL C++ entry
  binaries must be rebuilt. The original Windows backend is separate.
- Script support is enabled by default; a script-free build is also tested.
- IVP v1 is absent from this branch. IVP v2 is not being implemented.

| Target | Build target described by SDK | Native Player acceptance in this record |
| --- | --- | --- |
| Windows x86, x64, ARM64 | `windows` / `x86`, `x64`, `arm64` | Not run |
| Linux x64, ARM64 | `linux` / `x64`, `arm64` | Not run; X11/Wayland acceptance pending |
| macOS x64 | `macos` / `x64` | Not run |
| macOS ARM64 | `macos` / `arm64` | Dynamic, scripting on/off |
| Static host, any target | Not integrated | Pending |

## Completed local checks

On macOS ARM64, Release dynamic builds passed the complete CTest suites:
**107/107 with scripting and 100/100 without scripting**. These counts are a
snapshot; optional test configuration and later tests can change them.

The checks cover installed/relocated SDK consumers, C and C++ Native examples,
Native/Interface/IMC/Script project workflows, every in-tree Native Mod's build,
32/64-bit ABI goldens (executed on the host word size), selected-engine
fingerprints, Behavior/IMC/config/command/input contracts and updater fixtures.
This does not claim that 32-bit code ran on the 64-bit host.

The installed-SDK Player probe passed repeated independent launches with script
support enabled and disabled. It checks engine-before-Mod PostProcess ordering,
input press/release/blocking, one pending Render skip, command retirement,
configuration notifications, timers, script values and CK borrowing after
deletion. Each run also checks Mod unload, clean shutdown and identical original
and staged module SHA-256 hashes. Staging does not patch the Mod binary.

An opt-in `--benchmark-api` run records common API timings. Five local before/
after samples, including alternating pairs, reduced median directory reads from
13.5 to 6.8 µs, key-name reads from 63.6 to 32.5 ns and blocked keyboard reads from
40.8 to 34.9 ns. Raw keyboard controls remained around 40 ns. These are warm local
microbenchmarks, not frame-rate or cross-platform guarantees. Header-only SDK
optimizations require rebuilding the Mod to take effect.

## Documentation examples

The 2026-11-06 documentation refresh compiled and loaded 21 complete script
examples in a disposable macOS ARM64 Player, including both IMC guides, the
Chinese course, three SDK examples and the complete tutorial asset. Staged copies
used unique IDs; generated IMC includes were built from the guide’s IDL. Native C,
C++, interface provider/consumer, the complete native entry and IMC C++ excerpts
compiled against the installed SDK. An additional 18 service, scene, Hook and
state-migration excerpts compiled in isolated script modules with no gameplay
side effects. Both language sites pass strict MkDocs link
and anchor checks. These are compilation/startup checks, not gameplay acceptance
of every example branch.

## Still pending

Real Windows/Linux/macOS x64 runs; all static integration; X11/Wayland and full
IME/focus/controller/DPI/fullscreen coverage; complete level/death/reset/map/UI
journeys and per-Mod gameplay acceptance; production channel/update acceptance;
and platform distribution signing/dependency-deployment checks beyond local
fixtures. A successful build or doctor report cannot substitute for these runs.

See [Contributing](contributing.md) for build commands and the repository
[Player probe](https://github.com/doyaGu/BallanceModLoaderPlus/tree/sdl/tests/player/sdl)
for repeatable acceptance and benchmark commands.

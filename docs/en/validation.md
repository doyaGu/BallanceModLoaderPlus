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
**109/109 with scripting and 102/102 without scripting**. These counts are a
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

## SDL follow-up audit (2026-11-06)

Enabling optional UI automation reproduced two BML+-only build blockers: the
headless Bui target lacked CK2/VxMath usage requirements, and the script-reload
journey still cast a composed `ModRecord` to `ScriptMod`. Both are fixed using
existing targets and the record’s existing `Script` field. Script-on and
script-off instrumented Loader builds and their three UI tests pass on macOS
ARM64. These checks compile the journeys but do not drive them in a real Player.

Scenario/Session contracts now run in default builds and standalone without
engine packages or the optional Test Engine. CI is configured to run them on
Windows, Linux and macOS; this local audit executed macOS only. The full-suite
counts above include these two additional default tests.

| Remaining item | Concrete gap and next step |
| --- | --- |
| Real Player UI and game journeys | No portable input/capture runner is registered. Reuse the existing SDL input path and CK2 capture API in BML test tooling first; verify capture timing and actual output before counting visual acceptance. |
| Full native platform matrix | CI runs standalone SDK/CLI/UI contracts, not complete Loader/CKAngelScript/Mods builds. Supply matching dependency packages, then build and run native targets; x86 layout goldens do not run x86 code. |
| Static Player | Configuration explicitly rejects it and source integration selects shared CK2/VxMath. Host entry/export work needs a separately approved Ballanced integration; it is outside this BML+-only audit. |
| Input and each Mod’s gameplay | Virtual-gamepad/input component checks exist. Actual IME, focus, controllers, DPI/fullscreen, X11/Wayland and complete level/reset/map/per-Mod journeys remain unaccepted. |
| Distribution and update packages | Signed fixtures pass; unpacked target packages, production channel/restart/rollback and platform signing/dependency deployment still need target-machine acceptance. |
| Reference regeneration | Full ImGui regeneration lacks the cimgui input catalog; CKAS editor aliases still reflect a recorded macOS ARM64 host. Supply matching inputs and generate/check target-specific references. |

Default engine physics scheduling is retained and is not an unresolved
BML scheduling redesign. IVP v2 remains out of scope. No Ballanced source was
changed for this audit.

## 0.4.0 version synchronization (2026-11-09)

CMake's 0.4.0 release version now agrees with the installed SDK header, CMake
package version and the loaded binary's string/numeric version APIs. The Windows
resource continues to use the same generated macros; no Windows build was run
for this audit. Native templates and examples declare their SDK's `BML_VERSION`
as the loader minimum. Script templates, examples, smoke fixtures and publishing
commands now target 0.4.0. Individual Mod versions and historical configuration
migrations retain their separate meanings.

The native protocol remains `bml.mod` major 1, the ABI descriptor remains schema
2 and update manifests remain schema 2. The publisher now accepts current
schema-2 runtimes, preserves their extension metadata and rejects unsupported C
protocols; schema-1 packaging compatibility also passes. An actual macOS ARM64
0.4.0 runtime produced a local test-signed package whose runtime, manifest and
channel versions all match 0.4.0. The test-key updater verified that package.
This did not publish a release or validate a production signing key.

The 0.4.0 macOS ARM64 suites passed **111/111** with scripting and **102/102**
without scripting. The Player loaded all 21 updated documentation/template
script examples. Each script mode also passed three independent probe runs with
all twelve in-tree Mods. Other-platform and full gameplay acceptance remain
pending as listed below.

## Still pending

Real Windows/Linux/macOS x64 runs; all static integration; X11/Wayland and full
IME/focus/controller/DPI/fullscreen coverage; complete level/death/reset/map/UI
journeys and per-Mod gameplay acceptance; production channel/update acceptance;
and platform distribution signing/dependency-deployment checks beyond local
fixtures. A successful build or doctor report cannot substitute for these runs.

See [Contributing](contributing.md) for build commands and the repository
[Player probe](https://github.com/doyaGu/BallanceModLoaderPlus/tree/sdl/tests/player/sdl)
for repeatable acceptance and benchmark commands.

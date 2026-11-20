# Support and validation

This page separates available build targets from completed game acceptance.
The dated records below are local validation snapshots, not promises about an
arbitrary release archive or future commit.

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

Windows SDL builds attach the existing IMM/TSF presentation to the SDL window.
SDL supplies committed text to ImGui. The native release workflow builds and
tests Windows x64 and Linux x64 artifacts on configured runners. Chinese IME
composition, cancellation, candidate navigation and focus changes still need
target-machine checks. See the [release runbook](../../RELEASING.md).

## Completed local checks

On 2026-11-06, macOS ARM64 Release dynamic builds passed the complete CTest suites:
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

## Updated engine baseline (2026-11-08)

This audit uses Ballanced `a330482`, CK2 `0f414789`, VxMath `91aed4e1`,
RenderEngine `ce0432bc` and Player `f3a34f0c`. SdlInputManager remains at
`f070cf67`. The runtime uses SDL 3.4.20 and VxMath's default extended-arithmetic
emulation. No Ballanced source or submodule pointer was changed.

The newer VxMath package exports `VxMath::VxMath`. BML now accepts that target
through an alias at its two package entry points, retaining source-tree and
older package support. Engine ABI checks remain strict: Mods using `BML::Engine`
need rebuilding against the new headers and public settings. The core C protocol
is unchanged. The macOS SDK fixture now locates its already-loaded SDL image,
accepting both absolute and `@rpath` install names without patching staged Mods.
Both the new relocated SDK and an older installed SDK passed the consumer test.

On macOS ARM64, the script-enabled source build passed all **111** registered
tests; the script-free build using installed engine packages passed all **102**.
Both include the updater. Three process/loopback-network tests in each suite
initially hit sandbox restrictions and passed when rerun with those permissions;
the other tests passed in the original runs. The separate engine/render and
input component tests passed **2/2**. Optional headless UI instrumentation was
off in these configurations.

Disposable copies of the updated Player passed **three independent launches in
each script mode**. Every launch loaded all twelve in-tree Mods and verified
120 Process/physics ticks, 119 render callbacks, seven input phases, Mod unload,
clean shutdown and identical original/staged probe bytes. This is startup and
runtime coverage, not per-Mod gameplay or visual acceptance.

The installation instructions also now use the SDK installation's actual `Mods`
directory. The previously documented `BMLMods` install component does not exist.
All remaining acceptance items above still apply.

## libx87 baseline recheck (2026-11-09)

Ballanced is now `ac8e25f`, with CK2 `f07579aa`, VxMath `a0b437ed`, RenderEngine
`88a47ff6`, BuildingBlocks `6ead8209` and Plugins `0ae7b9cb`. Player and
SdlInputManager are unchanged from the previous snapshot; SDL remains 3.4.20.
VxMath now uses the `libx87` submodule (`b91aa6bc`) for extended arithmetic.

The existing BML package integration required no additional code changes.
The installed SDK includes libx87's static library, CMake package and licenses;
its relocated consumers pass, and the checked CMake package files contain no
absolute source/build paths. Engine extension fingerprints distinguish this
baseline from the previous one; the BML C protocol is unchanged.

macOS ARM64 Release checks passed **111/111** with scripting, **102/102** without
scripting using the newly installed engine packages, and **2/2** engine/render
and input component tests. Both temporary Player configurations passed three
independent launches each, loading all twelve Mods and checking the same
120 Process/physics ticks, 119 render callbacks, seven input phases, unload,
clean shutdown and unchanged staged probe bytes. Full gameplay, visual and
other-platform acceptance remain pending. Ballanced was read-only throughout.

## ImGui pixel alignment (2026-11-09)

A real 640×480 Player capture reproduced blurred ImGui output at 1× scale:
a 1:1 black/white checkerboard became uniform gray (both pixel classes were 128).
The CK2 backend now converts ImGui coordinates to framebuffer pixels and applies
CK2's half-pixel offset. The same capture then preserved 0/255 checkerboard
pixels. Vertex positions, display origin and viewport now use the same scale.

Two new component regressions cover pixel centers, 2×/fractional framebuffer
scales, nonzero display origin and caller viewport restoration. Engine/input
components and five related UI test executables passed. The corrected Loader
also passed three Player probe runs with all twelve Mods. These captures verify
the 1× macOS ARM64 case; scaled geometry is additionally covered by component
tests. No Ballanced changes or new public APIs were needed.

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

## Native/script authoring alignment (2026-11-10)

The macOS ARM64 dynamic Release build passes all 110 registered CTest tests,
including installed-SDK consumers, native C ABI layout checks and the migrated
command wrapper/service lifetime tests. All in-tree native Mods compile. The
script-disabled Loader also builds successfully.

Disposable real Player runs pass three cycles with the aligned script API,
three with the extension registered but no script consumers, and three with
scripting disabled. Each validates 120 Process/physics ticks, 119 Render
callbacks, seven input phases, Mod unload, clean exit and unchanged staged probe
bytes. Script runs additionally cover typed config, array type rejection,
keyboard capture ownership, failed-candidate rollback, successful rebind, stale
views, completed-request cleanup, individual/combined event precedence and
instant dependency evaluation after a provider failure.

A separate Player launch compiles/loads 21 complete examples and five reload
candidate source variants. A combined launch also loads all twelve in-tree Mods
and verifies the actual ZIP resource fixture reports successful reads. The latter have lifecycle methods renamed only in
the disposable test copy; this is compilation coverage, not execution of their
intentional fault paths. English/Chinese strict MkDocs builds and the runtime
API declaration publisher checks pass.

These checks do not execute other platforms/architectures or static hosts.
Same-engine extension removal/re-registration and forced allocation-failure
injection remain pending; independent clean launches are not substitutes for
those cases. Existing level-transition and per-Mod gameplay acceptance limits
above still apply.

## Command contract review fixes (2026-11-11)

The three review findings are fixed: command registration prepares metadata
before publication and rolls back retained entries on host failure; script
ExecuteLine uses the native wrapper; CommandEvent.Cheat is registered. The shared
UTF-8 validator additionally rejects surrogate code points and values above
U+10FFFF, so both native and script paths enforce the same text contract.

An allocation-failure sweep, host exception/rejection tests and Unicode scalar
boundary tests were reproduced failing before their fixes and now pass. The
matching installed-engine SDK build passes all 109 registered tests (the source
engine export/consumer test is not registered in this configuration). Player
checks reject six invalid byte patterns without executing the command, preserve
valid Chinese text and business status, verify false/true Cheat event values,
and retain the existing input/render/reload and clean-shutdown checks. Public
API reference files were regenerated from that runtime. This uses the existing
prepared Player baseline, not the external engine source reorganization in
progress; the unexecuted targets listed above remain pending.

## Command API failure paths (2026-11-12)

Script command registration now applies the native UTF-8 metadata rules, and
Find uses the shared name validator. Error-returning command/timer reads contain
allocation exceptions and publish complete record copies only. Completion
collectors retain their first failure status. Native registration prepares all
metadata before publishing an internal Entry. Public signatures and ABI layouts
are unchanged.

Five new allocation-failure tests reproduced exceptions before their fixes and
now verify error codes, unchanged outputs, retry behavior and collector failure
retention. The script-enabled installed-engine build passes all 109 CTests.
Three disposable macOS ARM64 Player cycles compare native/script metadata and
lookup rejection, retry rejected registrations, and pass the existing
input/render/reload, unload and clean-exit checks with unchanged probe bytes.
The runtime declaration publisher check passes without reference changes.
These unit injections cover script bridges; native host allocation failure has
not been injected in Player. Its publication ordering was checked in source and
its normal/rejected registrations run through the native Player probe. The
previous platform and engine-baseline limits still apply.

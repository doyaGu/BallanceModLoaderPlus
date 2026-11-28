# Player acceptance tests

## Virtools Win32

Use a copy of the Ballance install with the dynamically linked BallancePlayer
`52bcdc9` build. Extract the Win32 runtime into the copy and the SDK into a
separate directory. Build the probe with VS 2022 and the shared SDK contract.

```powershell
cmake -S tests/player/virtools -B build/virtools-player-probe -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/acceptance/sdk -DVIRTOOLS_SDK_PATH=C:/sdk/Virtools-SDK-2.1
cmake --build build/virtools-player-probe --config Release
cmake --install build/virtools-player-probe --config Release --prefix C:/acceptance/game/ModLoader
```

Start Player with the copy's `Bin` as its working directory. The Mod logs its
verdict and exits after the first level starts. It opens the stock level
menu and verifies that buttons from both menus are hidden in gameplay. For
a second run, set
`BML_PORT_CHECK_CONFIG=1` to check the UTF-8 value saved in the first run. Set
`BML_PORT_CHECK_MAP=1` and place a valid `中文地图.nmo` under `ModLoader/Maps`
to exercise the custom-map route. The probe reloads its companion Building
Block to check prototype identity. Set `BML_PORT_CHECK_EVENTS=1` to exercise
pause, resume and restart through the live event handler. Set
`BML_PORT_CHECK_RENDER=1` to skip every other frame for 60 updates. Set
`BML_PORT_CHECK_TWEAKS=0` or `1` to check Lantern Alpha Test and the three
Overclock paths against the corresponding Overclock value in `BML.cfg`.

Set `BML_VIRTOOLS_PLAYER_ROOT` to the prepared runtime to enable
`BMLVirtoolsProjectionTest`. It runs the SDK probe in two Player copies and
checks the retail renderer and the fallback for an altered fingerprint.

Enable `BML_TEST_VIRTOOLS_IME` for the Pinyin test. Install the resulting
`VirtoolsImeProbe.bmodp` and its ABI sidecar in the disposable runtime and
keep Player in the foreground. The test requires an enabled Microsoft Pinyin
profile. It uses the profile within the Player process, types and cancels
a composition, then verifies that a second composition commits exactly one
`你好` to an ImGui input. Check the log for the Pinyin result. The Mod exits
Player on completion, so run it separately from the gameplay probe.
[Windows TSF profile activation](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfinputprocessorprofilemgr-activateprofile)

## SDL Player

The dynamic SDL regression builds `SdlPostProcessProbe` as an independent
consumer of an **installed** SDK and runs disposable copies of a complete,
matching Player runtime with real game data.

```sh
cmake -S tests/player/sdl -B build/sdl-postprocess-test \
  -DCMAKE_PREFIX_PATH="/path/to/BML-SDK;/path/to/engine-sdk;/path/to/SDL-prefix" \
  -DCMAKE_BUILD_TYPE=Release -DBML_SDL_PLAYER_ROOT="/path/to/prepared/runtime"
cmake --build build/sdl-postprocess-test --config Release
ctest --test-dir build/sdl-postprocess-test -C Release --output-on-failure
```

Use the CK2/VxMath exports and SDL3 package used to build BML+. Match their
architecture, compiler/runtime and configuration because this probe uses
engine/ImGui extensions. CTest runs Player three times. It requires
a native desktop and working renderer, unlike `tests/ck2/sdl` dummy-video tests.
Do not run multiple Player probes or update tests against the same fixture at
once.

The probe checks late PostProcess after default manager/physics work, Behavior
safe-point ordering, 120 Process/physics ticks, and a one-shot request consumed
by the next Render (119 render callbacks). Input checks cover seven phases of
raw/filtered keyboard and mouse state, release edges, nested blocks and cleanup.
It injects SDL events through Player’s normal queue and exits through the public
`BML::Gameplay::ExitGame`. No menu driving is needed.

The SDK-built Mod must find Player’s SDL window. The runner records SHA-256 of
original and staged binaries; they must match. It never patches binary
relocations, signatures or load commands, and never changes the supplied runtime.
Each run retains its verdict, Loader/Player logs and disposable runtime.

## Additional Mods and performance

Use the same runner with previously built Mods and explicit expected identities:

```sh
python tests/player/sdl/RunPostProcessTest.py \
  --runtime "/path/to/prepared/runtime" \
  --probe "/path/to/SdlPostProcessProbe.bmodp" \
  --mods-from "/path/to/test-mods" --expect-mod example.friendly \
  --output "/path/to/artifacts" --repeat 3 --timeout 60
```

`--expect-mod` is repeatable and requires each identity to reach the loaded state.
Include `DebugUtilities` in a script-enabled run to exercise cheat notifications
before its ball force parameters exist.
Include native ABI sidecars. Script-enabled runs automatically stage the probe’s
script helpers; test native-only runtime/SDK builds separately. These helpers
check strict ConfigValue reads and choices ownership, keyboard snapshot COW and
retirement, zero-delay timers, full command notifications, successful reload and
failed-candidate rollback, stale-view output clearing, terminal cleanup and an
instant dependency query after the provider fails. Command contracts also reject
malformed UTF-8 and embedded NUL before dispatch, preserve valid Chinese text
and business exit codes, and read both ordinary and cheat command event flags.
Native and script checks reject malformed metadata in all three descriptive
fields, retry the same registration name, and distinguish invalid lookup names
from valid names that are absent. The extended
SDK fixture covers C/C++, all four native profiles, inter-Mod interfaces,
configuration notifications, timer/command lifetimes, CK borrowing after object
deletion, script dependencies and the twelve in-tree Mods.

`--benchmark-api` adds repeated directory/key-name/blocked-keyboard measurements,
with raw-keyboard reads as a control. Compare repeated runs with the same
configuration, prepared assets and machine; do not turn one timing sample into
a cross-platform performance claim.

Loading every Mod is startup coverage. Full level transitions, each Mod’s game
behavior, visual correctness and all input devices remain separate acceptance
work. [Current evidence and pending targets](../../docs/en/validation.md).

## Script API registration and examples

`--without-script-consumers` leaves the extension enabled while staging no probe
script modules. It checks registration and clean engine teardown, including the
native value type that exposes ConfigPropertyDefinition.Choices through owned
array accessors. This is separate from a build with scripting disabled.

`--script-examples` compiles and loads the three SDK examples, template and
positive smoke scripts (including a real ZIP with resources). It also
compiles five reload/recovery source variants with their lifecycle methods
renamed in the staged copy, so intentional exit/throw paths cannot run. Real
runtime replacement and rollback are exercised by ScriptRuntimeProbe instead.
`--tutorial-examples` additionally enables the complete tutorial asset and
complete course code blocks; use it with documentation matching the API.
Each expected identity must reach Loaded; a script compile failure cannot pass
because Player still starts. These checks cover compilation/startup, not every
gameplay path in the examples.

When a prepared runtime is supplied to the standalone CMake project, CTest
registers the normal three-cycle probe, three-cycle empty-consumer registration
probe, and a one-cycle example check for script-enabled runtimes.

To refresh editor declarations from the actual registered runtime:

```sh
python tests/player/sdl/RunPostProcessTest.py \
  --runtime "/path/to/prepared/runtime" \
  --probe "/path/to/SdlPostProcessProbe.bmodp" \
  --output build/script-api-runs --repeat 1 \
  --dump-api build/bml-script-api.as
python tools/generate_script_api.py --export build/bml-script-api.as
python tools/generate_script_api.py --export build/bml-script-api.as --check
```

The dump requires headers matching the runtime CKAngelScript. For a default BML
build, set `BML_TEST_CKAS_INCLUDE` to `build/sdl/_deps/ckangelscript-src/include`
and `BML_TEST_AS_INCLUDE` to the `angelscript/include` directory under
`CKAS_ANGELSCRIPT_ROOT` in that BML build's cache. For a local checkout next to
BML+, the probe can discover those paths automatically. The probe uses
public runtime/engine reflection, not another registration list. The publisher
restores @+ ownership markers from native declarations because reflection omits
them, and replaces only the BML section of the editor baseline.

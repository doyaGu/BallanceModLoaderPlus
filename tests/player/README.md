# SDL Player acceptance tests

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

The dump requires headers matching the runtime CKAngelScript, discovered from
the sibling checkout or BML_TEST_CKAS_INCLUDE/BML_TEST_AS_INCLUDE. The probe uses
public runtime/engine reflection, not another registration list. The publisher
restores @+ ownership markers from native declarations because reflection omits
them, and replaces only the BML section of the editor baseline.

# SDL Player acceptance tests

The dynamic SDL regression builds `SdlPostProcessProbe` as an independent
consumer of an **installed** SDK and runs disposable copies of a complete,
matching Player runtime with real game data.

```sh
cmake -S tests/player/sdl -B build/sdl-postprocess-test \
  -DCMAKE_PREFIX_PATH="/path/to/BML-SDK;/path/to/SDL-prefix" \
  -DCMAKE_BUILD_TYPE=Release -DBML_SDL_PLAYER_ROOT="/path/to/prepared/runtime"
cmake --build build/sdl-postprocess-test --config Release
ctest --test-dir build/sdl-postprocess-test -C Release --output-on-failure
```

Match architecture, compiler/runtime and configuration to the SDK because this
probe uses engine/ImGui extensions. CTest runs Player three times. It requires
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
Include native ABI sidecars. Script-enabled runs automatically stage the probe’s
script helpers; test native-only runtime/SDK builds separately. The extended
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

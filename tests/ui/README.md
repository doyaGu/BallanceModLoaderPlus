# SDL UI test infrastructure

The current SDL build provides two default framework tests and one optional
ImGui component test:

| Target / CTest name | What it validates |
| --- | --- |
| `BuiAutomationTest` | Headless ImGui/Bui controls using the pinned ImGui Test Engine |
| `UiFrameworkTest` | Default: scenario catalog, source registrations, descriptors and result parsing |
| `UiAutomationSessionTest` | Default: checkpoint ordering, atomic publication, waits and acknowledgements |

The framework tests need only CMake and a C++20 compiler. They also run in the
standalone CI matrix without Ballanced, SDL or ImGui Test Engine:

```sh
cmake -S tests/ui -B build/ui-framework
cmake --build build/ui-framework --config Release
ctest --test-dir build/ui-framework -C Release --output-on-failure
```

To additionally build Bui automation and the test-instrumented Loader, configure
from the project root with matching engine development packages:

```sh
cmake -S . -B build/ui -DBML_RUNTIME=sdl \
  -DBML_BUILD_TESTS=ON -DBML_BUILD_UI_AUTOMATION=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/ui --config Release \
  --target BuiAutomationTest UiFrameworkTest UiAutomationSessionTest
ctest --test-dir build/ui -C Release \
  -R '^(BuiAutomationTest|UiFrameworkTest|UiAutomationSessionTest)$' --output-on-failure
```

`BML_BUILD_UI_AUTOMATION` defaults to OFF and requires `BML_BUILD_TESTS`.
Enabling it fetches the pinned dependency in `cmake/ImGuiTestEngine.cmake`.
Ordinary builds do not fetch or compile that test engine. The component and
framework checks run without a desktop. They do not execute
the retained Player journeys. Framework-only configurations do not fetch ImGui
Test Engine, including when enabled through the normal `BML_BUILD_TESTS` suite.

## Scenario and Session boundaries

- `player/UiAutomation.cpp` owns the test-only in-game Test Engine lifecycle.
- `player/UiTestFramework.h` defines bounded waits, UI navigation helpers and
  `BML_REGISTER_UI_SCENARIO`.
- `player/journeys/<feature>` contains business journeys. A journey owns its
  actions, assertions, fixtures and restoration; it uses visible UI actions.
- `scenarios/*.scenario` describes identity, source, input profile, fixtures,
  capture name, requirements and expected log evidence in `key=value` format.
- `runner/UiAutomationProtocol.cpp` parses metadata and `bml-ui-result-v1` files.
- `session/UiAutomationSession.*` implements sequenced checkpoint/acknowledgement
  exchange. This is the input/capture control channel; human-readable logs are
  diagnostic evidence.

The retained journeys cover Mods menu/configuration, custom maps, HUD, console,
script tools, reload and shutdown persistence. Catalog checks establish that
these definitions are internally consistent; they do not execute those journeys
in Player.

**There is no portable real-Player UI runner in this branch.** The old Windows
`UiPlayerRunner`, Windows installation adapter and `UiPlayerAutomation.*` CTest
registrations were removed. Setting `BML_BALLANCE_ROOT` does not recreate them.
Do not use the old `.exe` commands or report catalog checks as game acceptance.
The SDL probe in [player tests](../player/README.md) starts the actual Player and
validates scheduling/input/SDK contracts, but is not a replacement for complete
menu, Chinese IME, focus, controller, fullscreen, X11/Wayland or visual tests.
See the [validation record](../../docs/en/validation.md).

## Maintaining a journey

1. Put its source under `player/journeys/<feature>` and register it with
   `BML_REGISTER_UI_SCENARIO`.
2. Add a same-named descriptor under `scenarios`. `source` is relative to
   `tests/ui`; `input` is `mod-list`, `level-one`, or `custom-map`.
3. Use supported `fixture` (`custom-map`, `public-authoring`) and `requires`
   (`angelscript`) values only when needed. Keep identity and result names unique.
4. Run all three infrastructure tests. A future platform driver must separately
   demonstrate foreground input, unobstructed captures, bounded failure and
   exact installation restoration before a journey can count as Player coverage.

A result file has the following form; only a completed real journey may emit
`status=passed` as gameplay evidence:

```text
format=bml-ui-result-v1
scenario=console
test=console_command_and_message_board
status=passed
failures=0
```

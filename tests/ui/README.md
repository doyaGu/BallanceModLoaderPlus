# UI automation

This directory contains two layers of UI tests:

- `BuiAutomationTest` is a fast, headless ImGui component test.
- `UiPlayerAutomation.<scenario>` is a visible 800x600 acceptance test that
  starts the real Ballance Player and runs exactly one business scenario.

There is intentionally no aggregate Player test. Select a scenario explicitly
so failures, screenshots, logs, setup and cleanup belong to one user journey.
The Player path is native C++; it does not use PowerShell, and its result file
is the small project-owned format documented below rather than a test-report
framework.

## Architecture

- `BML_BUILD_UI_AUTOMATION` is the dedicated build switch for this directory
  and the Player-side Test Engine runtime. It is available only when
  `BML_BUILD_TESTS` is enabled, and is independent of the Behavior test
  interface compiled for other Player integration tests.
- `src/UI/UiAutomation.cpp` owns the Player-side ImGui Test Engine lifecycle.
  It arms only after `On Message PostStartMenu`, waits for real keyboard
  navigation from the native Main and Options menus, registers the selected
  scenario, writes the small `bml-ui-result-v1` result, then exits cleanly.
- `src/UI/Automation/UiTestFramework.h` is the stable scenario interface. It
  provides bounded waits, wall-clock-bounded cross-process actions,
  native/ImGui transition helpers and `BML_REGISTER_UI_SCENARIO`.
- `src/UI/Automation/UiAutomationSession.*` is the shared cross-process Module.
  Player publishes one immutable, sequenced checkpoint and yields while keeping
  that UI state visible. The native runner performs the requested foreground
  input or capture and atomically publishes the matching acknowledgement.
- `src/UI/Automation/Scenarios/*.cpp` contains one business journey per file.
  A scenario owns its setup, actions, assertions and restoration.
- `tests/ui/scenarios/*.scenario` is runner metadata in plain `key=value`
  text: identity, source, input profile, fixtures, capture name, requirements
  and required log evidence.
- `UiAutomationProtocol.cpp` owns parsing and validation for scenario metadata
  and result files, including names, duplicate identities and supported values.
- `UiPlayerHarness.cpp` is the native Windows adapter behind one small run
  interface. It installs the built DLL transactionally, isolates external Mods,
  starts Player visibly, accepts the render setup dialog, keeps Ballance in the
  foreground, sends physical keyboard events, captures the client area only
  while it is unobstructed, acknowledges each Session checkpoint, and verifies
  restored files by content fingerprint.
- `UiPlayerRunner.cpp` evaluates shared acceptance checks plus the selected
  scenario's evidence.
- `UiFrameworkTest.cpp` checks catalog completeness, uniqueness, source
  registration, descriptor validity and result parsing without starting Player.
- `UiAutomationSessionTest.cpp` verifies checkpoint sequencing, atomic
  publication, waiting semantics and successful/failed acknowledgements without
  starting Player.

The Player adapter is separate from scenario code. Logs remain human-readable
diagnostics and optional business evidence; they are never the control channel
for input or capture timing. New UI surfaces reuse
foreground control, resolution checks, capture, cleanup and result validation
instead of cloning process-driving logic.

## Running one scenario

Configure and build with a real installation:

    cmake -S . -B build-dev -A Win32 -DBML_BUILD_TESTS=ON -DBML_BUILD_UI_AUTOMATION=ON -DBML_BALLANCE_ROOT=C:/Users/kakut/Games/Ballance
    cmake --build build-dev --config RelWithDebInfo --target BML UiPlayerRunner

`BML_BUILD_UI_AUTOMATION` defaults to `ON` when `BML_BUILD_TESTS` is enabled.
Set it to `OFF` to build the rest of the test suite without fetching or
compiling ImGui Test Engine.

List available cases without launching Player:

    ctest --test-dir build-dev -C RelWithDebInfo -N -R "^UiPlayerAutomation\."

Run one case only:

    ctest --test-dir build-dev -C RelWithDebInfo -R "^UiPlayerAutomation\.console$" --output-on-failure

The native runner can also be invoked directly for debugging:

    build-dev/tests/ui/RelWithDebInfo/UiPlayerRunner.exe --scenario-dir tests/ui/scenarios --source-root src/UI/Automation/Scenarios --ballance-root C:/Users/kakut/Games/Ballance --build-dll build-dev/bin/RelWithDebInfo/BMLPlus.dll --scenario console --artifacts build-dev/tests/ui/RelWithDebInfo/UiAutomation/console

Each scenario gets its own artifact directory containing a `.result` file,
ModLoader trace, Player trace, 800x600 BMP captures and one isolated `session-*`
directory. That directory preserves every `checkpoint-*.msg` and matching
`ack-*.msg`, so the exact cross-process interaction remains inspectable after a
run. The result is deliberately small and framework-owned:

    format=bml-ui-result-v1
    scenario=console
    test=console_command_and_message_board
    status=passed
    failures=0

## Adding or changing a scenario

1. Add `src/UI/Automation/Scenarios/<Surface>Scenario.cpp`.
2. Implement one registration function. Use `UiAutomation::Test` helpers for
   native menu transitions and game-thread state changes, and ImGui Test Engine
   actions/checks for UI interaction.
3. Register it with `BML_REGISTER_UI_SCENARIO`.
4. Add `tests/ui/scenarios/<name>.scenario` with:
   - `name`: descriptor filename without `.scenario`.
   - `test`: registered ImGui test name.
   - `surface`: log/layout identity.
   - `capture`: artifact stem for the business surface.
   - `input`: `mod-list` or `level-one`.
   - `source`: scenario C++ filename.
   - optional repeated `fixture`, `requires` and `required_log` entries.
   - supported fixture: `custom-map`; supported requirement: `angelscript`.
5. Reconfigure CMake. `CONFIGURE_DEPENDS` discovers both files and creates only
   `UiPlayerAutomation.<name>`; no central list needs editing.
6. Run `UiFrameworkTest`, then run only the new or changed Player scenario.

## Current business coverage

- `mod-menu`: every category and page, editable values, revert and the real
  New Ball Type surface.
- `custom-maps`: Start-menu entry, search, fixture discovery, back navigation
  and the subsequent Level 1 transition.
- `hud`: title, FPS and speedrun timer in Level 1, followed by state restoration.
- `console`: console interaction, a real command and message-board output.
- `script-tools`: developer-tool window, tabs, filters and controls.

Every Player scenario verifies the native Main -> Options -> ImGui round trip,
foreground keyboard injection, unobstructed 800x600 captures, absence of
native-menu overlap, no ImGui assertion/frame errors, clean exit and exact
installation restoration.

Run the framework-only checks with:

    ctest --test-dir build-dev -C RelWithDebInfo -R "UiFrameworkTest|UiAutomationSessionTest" --output-on-failure

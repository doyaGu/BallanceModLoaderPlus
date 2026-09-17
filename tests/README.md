# Tests

`tests/CMakeLists.txt` only sets up the shared GoogleTest target helper and
connects the suites. Keep a test's source, fixtures, and target declaration in
the suite that owns it:

- `unit/<domain>/`: in-process C++ tests of one implementation domain.
- `contracts/abi/`: C and C++ compile checks, public-header checks, and DLL
  export baselines.
- `contracts/codegen/`: generated-interface fixtures and code generator checks.
- `contracts/sdk/`: CMake, installed-package, and Native Mod consumer checks.
- `player/`: probe Mods and scripts for real Player acceptance runs.
- `ui/`: headless ImGui checks and visible Player UI automation; see
  [UI automation](ui/README.md).
- `smoke/`: installed-game and script smoke assets.
- `updater/`: updater tests.

Keep existing CTest names when moving tests. The Player acceptance scripts also
rely on selected artifact directories under `build-dev/tests/`; preserve those
paths explicitly when changing a suite's CMake directory. The `player/` probe
flow and `ui/` checkpoint Session have different interfaces and remain
separate.

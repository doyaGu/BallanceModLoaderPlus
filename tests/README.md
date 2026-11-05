# SDL tests

- `tests/sdk`: standalone SDK/CLI contracts; installed SDK consumer when engine
  development packages are available.
- `tests/ck2/sdl`: real CK2 and input component tests with SDL dummy video.
- `tests/platform`: filesystem, module identity, ABI and script watching tests.
- `tests/updater/sdl`: updater/doctor unit and signed transaction tests.
- `tests/player/sdl`: installed-SDK Mod in disposable real Player runtimes.
- `tests/ui`: shared headless ImGui and automation-session infrastructure.

`cmake -S tests/sdk -B build/sdk-tests` followed by
`ctest --test-dir build/sdk-tests --output-on-failure` runs without an engine.
Native Player tests require a desktop and matching game data. Record unexecuted
platforms as unverified. The retail Windows backend tests belong to the other
release branch.

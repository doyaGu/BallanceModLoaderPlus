# SDL Player acceptance tests

For the dynamic SDL backend, the PostProcess regression uses an installed SDK
and disposable copies of a prepared Player runtime containing the matching BML
and game assets:

```sh
cmake -S tests/player/sdl -B build/sdl-postprocess-test \
  -DCMAKE_PREFIX_PATH=/path/to/sdk -DCMAKE_BUILD_TYPE=Debug \
  -DBML_SDL_PLAYER_ROOT=/path/to/prepared/runtime
cmake --build build/sdl-postprocess-test
ctest --test-dir build/sdl-postprocess-test --output-on-failure
```

Match the build configuration to the SDK. The test runs Player three times,
asserts the real loader runs after the built-in managers, observes completed
physics profiling in the current tick, and checks keyboard/mouse release edges
with and without device blocks. The probe injects SDL events through the normal
event queue and exits through `IBML::ExitGame`; no manual menu or keyboard actions
are needed. The runner checks the verdict, Mod unload, and clean Player exit and
preserves each run's logs. It never modifies the supplied runtime. A native
desktop session and working Player rasterizer are required; this is separate
from the dummy-video component tests in `tests/ck2/sdl`.

The probe also checks that its SDL can retrieve Player's window, detecting a
second SDL instance. The runner copies the SDK-built binary without changing
dependencies or signatures, and records matching source/staged SHA-256 hashes.

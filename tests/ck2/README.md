# SDL engine and input tests

```sh
cmake -S tests/ck2/sdl -B build/sdl-components \
  -DNEMO2_ROOT=/path/to/NeMo2 -DCMAKE_PREFIX_PATH=/path/to/SDL-prefix
cmake --build build/sdl-components --target BMLSdlEngineTest BMLSdlInputTest BMLSdlBehaviorCompile
ctest --test-dir build/sdl-components -R '^BMLSdl(Engine|Input)Test$' --output-on-failure
```

Use the NeMo2 revision pinned by the target Player. The tests need its source
for the recording rasterizer and SDL input fixtures. They exercise CK2 render
options, the Behavior editor and the input virtual-table adapter. SDL dummy
video runs without a desktop.
Real Player acceptance is documented in `tests/player/README.md`.

# SDL engine and input tests

```sh
cmake -S tests/ck2/sdl -B build/sdl-components \
  -DBALLANCED_ROOT=/path/to/Ballanced
cmake --build build/sdl-components --target BMLSdlEngineTest BMLSdlInputTest BMLSdlBehaviorCompile
ctest --test-dir build/sdl-components -R '^BMLSdl(Engine|Input)Test$' --output-on-failure
```

These tests compile the real CK2, render option adapter, Behavior editor,
SDL input manager and input virtual-table adapter. The recording rasterizer
belongs to the engine test fixture. SDL dummy video runs without a desktop.
Real Player acceptance is documented in `tests/player/README.md`.

# SDL native Mods

The twelve supported Mods build with `-DBML_BUILD_MODS=ON` and the matching
SDL CK2/VxMath SDK. Each remains a separate native module using the C entry and module-local C++
wrappers. Existing gameplay logic and resource packages are retained. IVP v1 belongs to the
original runtime and is absent from this branch; IVP v2 is not planned.

```sh
cmake -S . -B build-mods -DBML_RUNTIME=sdl -DBML_BUILD_MODS=ON
cmake --build build-mods --config Release
cmake --install build-mods --config Release --prefix build-mods-install
```

Install `.bmodp` with its `.abi.json`, or the resource ZIP, into
`ModLoader/Mods`. `BallStickyPackage` and `BMLModulsPackage` regenerate resource
archives. The installed inventory is `share/BML/mods.txt`.

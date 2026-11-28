# __MOD_NAME__

This is a working starter Mod. Edit `src/HelloMod.cpp`, then run:

```sh
python bml.py run
```

The generated Mod already logs a load message and adds a `hello` command, so
you can verify the toolchain before changing any code.

Use the SDL SDK matching Player’s platform, architecture, dynamic host and
protocol ABI. Core API calls cross a C seam; direct engine/ImGui extensions also
require a matching compiler, C++ runtime and build configuration.
On POSIX run `./bml`; on Windows run `bml.cmd`.

```sh
python bml.py build
python bml.py run --ballance-root "/path/to/game"
python bml.py pack
```

For additional interface or engine development packages, pass
`--package-root /path/to/prefix`. The native package contains `.bmodp` and its
`.abi.json`; install both together. Player supplies CK2, VxMath and SDL. The SDK
finalizes macOS load commands and signatures during the build.

For a manual build:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/sdk -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix /path/to/game/ModLoader
```

Choose the configuration shipped by the SDK and its target architecture.
Match the full compiler/runtime ABI when opting into engine or ImGui extensions.

`BML_DEFINE_MOD` at the end of the source holds the metadata in named fields.
Edit it directly; no separate factory or identity methods are needed. Keep the
command Registration as a member: the host and RAII handle its cleanup.

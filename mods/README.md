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

## Inventory

| Mod ID | Function |
| --- | --- |
| [BMLModuls](BMLModuls/) | Additional game modules and resources |
| [BallSticky](BallSticky/) | A wall-climbing ball type and resources |
| [CameraUtilities](CameraUtilities/) | Camera controls |
| [DebugUtilities](DebugUtilities/) | Game debugging tools |
| [DeformedWB](DeformedWB/) | Visual deformation of the wooden ball |
| [DualBallControl](DualBallControl/) | Dual-ball map support |
| [DynamicFov](DynamicFov/) | Camera FOV based on ball speed |
| [MapScripts](MapScripts/) | Map callback scripts |
| [NewSpawn](NewSpawn/) | Saved spawn/teleport positions |
| [Segment](Segment/) | Per-segment gameplay performance |
| [SpiritTrail](SpiritTrail/) | A translucent best-record replay |
| [TravelMode](TravelMode/) | Travel mode |

All twelve build and load in the macOS ARM64 SDK/Player startup fixture. This
is not per-Mod gameplay acceptance. Test map transitions, resets, hotkeys and
feature behavior separately; see the [validation record](../docs/en/validation.md).

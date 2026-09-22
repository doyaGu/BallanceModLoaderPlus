# IVP

IVP is an independent BML base Mod for the Ipion Virtual Physics revision
embedded in Ballance's original `physics_RT.dll`. It provides reconstructed
headers and a retail-locked runtime bridge; it is not a replacement physics
engine and does not wholesale import another IVP revision.

The verified retail image is `BuildingBlocks/physics_RT.dll` under a Ballance
installation, identified by:

- x86, PE timestamp `0x3DAC380C`, image size `0x81000`
- SHA-256 `E72E4AFCFA5C33A7D3D27776137F8C997B3C52D89D8A8A4745F1CA21E45893EC`

## Runtime boundary

The runtime artifact is `IVP.bmodp`. It exports only BML's standard
`BMLEntry` and `BMLExit` lifecycle entries. The Mod publishes `bml.ivp`
through BML's provider-interface registry; consumers query BML and never link
to or import an IVP API symbol.

This is a process-local MSVC x86 ABI. Returned engine pointers are borrowed.
Objects that cross the retail DLL boundary must use the allocation and release
routes documented by the API rather than a consuming Mod's CRT.

## Build and test

From the BML+ repository root, build IVP with the other in-tree Mods using a
Win32 generator:

```powershell
cmake -S . -B build-mods -A Win32 `
  -DBML_BUILD_MODS=ON `
  -DVIRTOOLS_SDK_PATH="<Virtools-SDK-2.1>" `
  -DIVP_BALLANCE_ROOT="<Ballance installation>" `
  -DIVP_BUILD_TESTS=ON
cmake --build build-mods --config RelWithDebInfo
ctest --test-dir build-mods -C RelWithDebInfo -L ivp --output-on-failure
```

IVP can also be configured directly from `mods/IVP` against an installed BML+
SDK when an independent IVP build is needed.

`IVP_BUILD_TESTS=ON` always builds the host and header checks. Retail-DLL
tests are enabled by default only when `IVP_BALLANCE_ROOT` contains
`BuildingBlocks/physics_RT.dll`; use `IVP_BUILD_RETAIL_TESTS=ON` or `OFF` to
choose explicitly. Evidence audits are controlled independently by
`IVP_BUILD_EVIDENCE_TESTS` (default `ON`). For a host-only build, set both
optional switches to `OFF`; no Ballance installation is required. CTest's
`ivp` label includes host, retail, and evidence tests that were enabled in
the current build.
Retail cases run as separate CTest processes because the DLL keeps
process-global state; the aggregate test remains disabled.

The IVP evidence audits require Python 3.10 or newer and `clang++` on `PATH`.
Individual audit scripts also accept `--clang` to select a compiler explicitly.
Set `IVP_REFERENCE_ROOT` to an absolute source path to enable the optional
neighbor-source audits. The source tree is not assumed to sit beside BML+.

The guarded IDA transaction still invokes one correction script. Its ordered
stages check the retail image, correct type definitions and layouts, annotate
types, correct vtables, apply names, and record ownership/lifetime evidence.
The stage-order test parses the script without requiring IDA. The curated
`retail-contract.json` remains the source for generated addresses and evidence
views; neither an IDA export nor nearby IVP source is promoted to ground truth.

Set `IVP_BUILD_PLAYER_TESTS=ON` together with `IVP_BUILD_TESTS=ON` to build the
four real-game cases: ball speed governing, state round-trip, surface raycast,
and live constraint/release. Their PowerShell runners temporarily stage the
required Mods, execute the normal menu/tutorial/level flow, and restore the
game installation afterwards.

## Consume

```cmake
find_package(BML CONFIG REQUIRED)
find_package(IVP CONFIG REQUIRED)

bml_add_mod(MyPhysicsMod src/MyPhysicsMod.cpp)
target_link_libraries(MyPhysicsMod PRIVATE IVP::API)
```

Include the complete reconstructed surface with:

```cpp
#include <BML/IVP/IVP.h>

class MyPhysicsMod final : public IMod {
public:
    explicit MyPhysicsMod(IBML *bml) : IMod(bml) {
        AddDependency(IVP_MOD_ID);
    }
};
```

The required dependency makes BML load IVP before the consumer and unload it
after the consumer. `IVP::API` is header-only and carries only the BML SDK
dependency; it does not create an import library for `IVP.bmodp`.

## Repository layout

- `include/BML/IVP.h`: versioned BML provider interface and lightweight facade.
- `include/BML/IVP/`: reconstructed Ballance-compatible C++ IVP surface.
- `src/`: provider Mod, exact-image validation, and symbol resolution.
- `tests/`: host, retail-DLL, ABI, installed-SDK, and Player scenarios.
- `tools/ivp/`: Retail Contract, evidence ledgers, IDA audits, and guarded IDB updates.
- `docs/`: detailed API, coverage, and retail-version differences.

The primary API documentation is available in
[English](docs/en/ivp-api.md) and [Chinese](docs/zh-CN/ivp-api.md).

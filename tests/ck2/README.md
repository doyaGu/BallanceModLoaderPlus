# Behavior integration tests with reference CK2

These tests compile the production Behavior runtime and graph editor against
the reference CK2 implementation. They cover native parameter propagation,
graph callbacks that rearrange Run slots, and default-source retyping. They
complement the unit tests and do not replace retail Player acceptance tests.

The nine cases cover:

- Pout chains and converging destinations, conflict retries, later destinations,
  and overlapping writes.
- Stale Run Slots after Apply and Close, plus stable Slots and Ports when an
  edit changes only values.
- PinType defaults for detached and graph-resident Blocks, shared defaults,
  and incompatible foreign sources.

Build reference CK2 and VxMath in Win32 Release first, then run from this repo:

```powershell
cmake -S tests/ck2 -B build-behavior-ck2 -G 'Visual Studio 17 2022' -A Win32 `
  -DCK2_SOURCE_DIR=C:/path/to/CK2
cmake --build build-behavior-ck2 --config Release --parallel 4
ctest --test-dir build-behavior-ck2 -C Release --output-on-failure
```

`CK2_BUILD_DIR` defaults to `CK2_SOURCE_DIR/build`; `VXMATH_SOURCE_DIR` defaults
to the sibling `VxMath` directory. Override either cache entry when needed.

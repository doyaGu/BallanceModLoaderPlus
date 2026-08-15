# Frozen SDK snapshot: v0.3.12

This directory is a byte-for-byte snapshot of `include/BML/` as it was released.
Never edit anything under `include/` here: the whole point is that this tree stays
exactly what mods built against the released v0.3.12 SDK saw. If a file looks
wrong, the fix is a new snapshot directory for a newer tag, never an edit.

- Tag: `v0.3.12`
- Commit: `6c9a69379d75440b95f56f2e8b5d48212019db31` (2026-06-20)
- Scope: the public C++ SDK headers only (`include/BML/`). The v0.3.12 release
  predates the versioned interface structs (`BML/Interface.h`, `bml.*`) and IMC;
  those shipped later and get their own snapshot when a release carries them.
  Until then the interface-struct layout is guarded by
  `tests/InterfaceStructOffsetsTest.cpp`.
- `include/BML/Version.h` is the one file not taken from the tag: the real one is
  generated at configure time from `src/Version.h.in`, so the snapshot carries a
  reconstruction with the v0.3.12 values. Treat it as frozen like the rest.
- Consumer: `tests/FrozenSdkConsumerTest.cpp` compiles against this tree
  (its include path points here, not at the current `include/`).

Reproduce with:

```powershell
git archive v0.3.12 include/BML | tar -x -C tests/abi/frozen-sdk/v0.3.12
```

Verify nothing drifted:

```powershell
git archive v0.3.12 include/BML | git hash-object --stdin
Get-ChildItem -Recurse -File tests/abi/frozen-sdk/v0.3.12/include |
    Get-FileHash -Algorithm SHA256
```

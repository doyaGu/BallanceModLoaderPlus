# BML_UI

Status: Complete

## Owns

- ImGui overlay lifecycle and backend bootstrap
- shared ANSI palette/text API surface
- `BML_UI` extension registration
- `ParseAnsiText` / `FreeAnsiSegments`

## Active implementation

- `modules/BML_UI/src/UIMod.cpp`
- `modules/BML_UI/src/Overlay.*`
- `modules/BML_UI/src/imgui_impl_ck2.*`
- shared helpers in:
  - `src/Utils/AnsiPalette.*`
  - `src/Utils/AnsiText.*`
  - `src/Utils/AnsiSegments.*`
  - `modules/BML_UI/src/imconfig.h`

## Migration notes

- Module-local duplicate ANSI/UI headers and sources were removed.
- The module now consumes the canonical shared ANSI/UI implementation from `src/Utils`.
- Public ABI stayed stable while `ParseAnsiText` and `FreeAnsiSegments` gained real behavior.
- `BML_UI` now consumes `bml.core.context`, `bml.core.logging`, `bml.core.module`, and
  `bml.core.host_runtime` directly.
- The active UI runtime no longer relies on removed global runtime pointers such as `bmlGetGlobalContext`,
  `bmlVirtoolsGet*`, `bmlLog`, `bmlInterfaceRegister`, `bmlInterfaceUnregister`, or
  `bmlGetModId`.

# Keep BML+ font domains separate

Status: Accepted; scope clarified for the SDL branch on 2026-11-06.

## Context

BML+ renders text through three unrelated systems: Dear ImGui's dynamic atlas,
Virtools menu-font indices created by the current world, and `CKSpriteText`
objects backed by the engine’s platform font implementation. Treating these as one registry would hide
different ownership, scaling, fallback, and failure rules behind a false common
interface.

## Decision

- Built-in ImGui surfaces use the loader-owned Built-in UI Font Runtime. It
  resolves an ordered profile, applies it between frames, reports coverage, and
  replaces only its own atlas font rather than fonts registered by Native Mods.
  Its initial profile is available before later Native Mods enter `OnLoad()`;
  live changes remain staged until the next frame boundary.
- Ballance menu widgets resolve semantic roles through the Game Font Catalog.
  The catalog binds the names created by `Menu_Init` to current-world indices.
- The legacy `BML::Gui::Text` API keeps a private per-sprite style registry. Its
  default style scales with the viewport; explicit styles survive a resize.
- No public BML, Native Mod, or AngelScript font API is added by this change.
- The Dear ImGui configuration is an optional Native Mod extension ABI,
  distributed through `BML::ImGui` and the installed ImGui configuration. Core
  `BML::BML` Mods do not import that C++ ABI.

## Consequences

Each path can fail and recover according to its real renderer without coupling
world resets to the ImGui atlas or font-file reloads to legacy sprites. Native
Mods that use Dear ImGui types must rebuild when this ABI configuration changes.
SDL fallback lookup uses bundled fonts and platform candidates. Missing fonts
are reported by the built-in font runtime; fallback coverage is not evidence
of color emoji support. No new universal font registry is needed.

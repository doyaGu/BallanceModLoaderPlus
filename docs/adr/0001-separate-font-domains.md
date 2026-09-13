# Keep BML+ font domains separate

Status: Accepted

## Context

BML+ renders text through three unrelated systems: Dear ImGui's dynamic atlas,
Virtools menu-font indices created by the current world, and `CKSpriteText`
objects backed by Windows font faces. Treating these as one registry would hide
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
- The Dear ImGui codepoint width is a Native Mod ABI choice distributed through
  the BML CMake target and installed ImGui configuration.

## Consequences

Each path can fail and recover according to its real renderer without coupling
world resets to the ImGui atlas or font-file reloads to legacy sprites. Native
Mods that use Dear ImGui types must rebuild when this ABI configuration changes.
A universal FontManager will only be introduced if at least two real,
interchangeable renderer adapters eventually require one.

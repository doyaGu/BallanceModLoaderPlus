# Require a dedicated rasterizer Adapter for color emoji

Status: Accepted

## Context

The CK2 ImGui renderer already uploads RGBA dynamic atlas textures, but the
bundled stb_truetype loader does not rasterize the COLR/CPAL layers in Windows
emoji fonts. Finding an emoji codepoint in the atlas therefore proves outline
coverage, not color rendering.

## Decision

BML+ reports common emoji coverage and color emoji support as separate
capabilities. The current runtime merges Windows outline fallbacks and reports
color emoji as unsupported. Color support requires a pinned, redistributable
rasterizer adapter such as FreeType with `LoadColor`; it must not depend on a DLL
found only on a developer machine.

## Acceptance criteria

The future adapter must preserve the dynamic-atlas lifecycle, build in supported
Win32 configurations, package its runtime dependencies, and pass Player visual
checks for color glyphs, fallback ordering, live replacement, and device reset.
It remains part of the Built-in UI Font Runtime, not the Game Font Catalog or
Legacy GUI Text.

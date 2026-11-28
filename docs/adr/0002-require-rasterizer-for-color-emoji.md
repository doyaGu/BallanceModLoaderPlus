# Require a font glyph rasterizer for color emoji

Status: Accepted constraint; color emoji implementation remains deferred.
SDL scope clarified on 2026-11-06.

## Context

The CK2 ImGui renderer already uploads RGBA dynamic atlas textures, but the
bundled stb_truetype loader does not rasterize the color glyph layers or bitmap color emoji strikes in platform fonts. Finding an emoji codepoint in the atlas therefore proves outline
coverage, not color rendering.

## Decision

BML+ reports common emoji coverage and color emoji support as separate
capabilities. The current runtime merges bundled and platform outline fallbacks and reports
color emoji as unsupported. Color support requires a pinned, redistributable
font glyph rasterizer with explicit color-glyph support; this decision does not
add a dependency. It must not depend on libraries found only on a developer
machine. “Rasterizer” here means font glyph production for ImGui’s atlas, not
CKRasterizer, a new engine API, or direct BML access to engine driver internals.

## Acceptance criteria

The future adapter must preserve the dynamic-atlas lifecycle, build on each claimed SDL platform/architecture, package its runtime dependencies, and pass Player visual
checks for color glyphs, fallback ordering, live replacement, and device reset.
It remains part of the Built-in UI Font Runtime, not the Game Font Catalog or
Legacy GUI Text.

# Mod Manifest Schema (mod.toml)

Every v0.4.0+ module must ship a `mod.toml` manifest beside its DLL. The microkernel reads this file during discovery to decide load order, resolve dependencies, and validate metadata. This document captures the schema enforced by `src/core/ModManifest.cpp`.

## File Placement

Place `mod.toml` at the root of the module directory (e.g. `ModLoader/Mods/MyCoolMod/mod.toml`). All paths referenced inside the file (such as `entry`) are resolved relative to this directory.

### Distributing as `.bp` archives

- v0.4 modules can be zipped and renamed to the `.bp` extension, then dropped directly into the `Mods/` directory.
- During discovery the loader extracts each archive into `Mods/.bp-cache/<archive-name>/` before parsing `mod.toml`, so you do **not** need to unpack them manually.
- Keep `mod.toml` in the root of the archive. The loader tolerates a single leading folder (when you zipped an entire directory) and will automatically descend into it.
- Avoid shipping unrelated files at the top level of the archive; everything inside the extracted folder is treated as part of the module payload.

## `[package]` table (required)

| Field        | Type                | Required | Notes |
|--------------|---------------------|----------|-------|
| `id`         | string               | Yes      | Unique identifier (reverse-DNS or simple slug). Must be non-empty and ASCII-safe.
| `name`       | string               | Yes      | Human readable display name.
| `version`    | string (semver)      | Yes      | Parsed with the semantic version parser (`major.minor.patch[-prerelease]`). Supports prerelease suffixes like `1.0.0-alpha` or `2.0.0-rc.1`. Invalid versions abort loading.
| `entry`      | string               | No       | DLL filename or relative path. Defaults to `<id>.dll` when omitted.
| `description`| string               | No       | Free-form text shown in UIs/logs.
| `authors`    | array of strings     | No       | Each entry must be a non-empty string.

### Additional rules

- `version` must parse according to `SemanticVersion` (see `src/Core/SemanticVersion.*`). The parser accepts both full (`major.minor.patch`) and partial (`major` or `major.minor`) version strings, treating missing components as `0`. Prerelease suffixes (e.g., `-alpha`, `-beta.1`, `-rc.2`) are also supported.
- Negative version numbers are not allowed and will cause parsing to fail.
- `id` is used as the capability handle and log filename, so prefer lowercase + dots (e.g. `com.example.supermod`).
- When `entry` points to a relative path, it is resolved against the directory that contains `mod.toml`.

## `[dependencies]` table (optional)

Dependencies are expressed as a TOML table whose keys are the target module IDs. Each value may be either:

1. **String shorthand** — interpreted as a semantic version range expression.
   ```toml
   [dependencies]
   "bml.core" = ">=0.4.0"
   ```
2. **Table form** — add metadata such as `optional`.
   ```toml
   [dependencies]
   "com.example.physics" = { version = "^2.1", optional = true }
   ```

Validation rules:
- `version` (string) is required in table form.
- `optional` defaults to `false` when omitted.
- Version ranges support the following operators:
  - `>=` — greater than or equal to
  - `>` — strictly greater than
  - `<=` — less than or equal to
  - `<` — strictly less than
  - `^` — compatible (caret), allows changes that do not modify the left-most non-zero digit
  - `~` — approximately equivalent (tilde), allows patch-level changes
  - `=` — exact match (optional prefix)
- Parsing errors are surfaced through the bootstrap diagnostics.

## `[conflicts]` table (optional)

Declare modules that must **not** be loaded together with the current one. The syntax mirrors the dependency table:

| Field        | Type            | Required | Notes |
|--------------|-----------------|----------|-------|
| Key          | string          | Yes      | Module ID to block when present.
| Value        | string/table    | Yes      | Version constraint and metadata.
| `version`    | string          | No       | Semantic version range. Omit or set to `"*"` to match all versions.
| `reason`     | string          | No       | Human-readable explanation surfaced in diagnostics.

Examples:

```toml
[conflicts]
"legacy.loader" = "*"                      # Any version conflicts
"com.example.renderer" = { version = "^2", reason = "Requires v1 renderer API" }
```

During resolution the loader checks each conflict entry against the installed modules. If the version range matches, the entire bootstrap halts with a descriptive error so players immediately know which pair of mods cannot co-exist.

## `capabilities` array (optional)

`capabilities` is an array of strings describing features the module exposes, used by `BML_RequestCapability`. Each entry must be a non-empty string. Example:

```toml
capabilities = ["bml.render.overlay", "com.example.debug-panel"]
```

## Generated metadata

During parsing, the loader augments the manifest with:
- `manifest_path`: absolute path to `mod.toml`.
- `directory`: directory that contains the manifest; used as the base for entry paths.
- Parsed `SemanticVersion` and normalized dependency constraints.

These fields are internal and not part of the TOML schema, but explaining them clarifies error messages.

## Complete example

```toml
[package]
id = "com.example.supermod"
name = "Super Mod"
version = "1.2.0-beta.1"
authors = ["Example Studios", "Jane Dev"]
description = "Adds a HUD overlay and rich diagnostics"
entry = "bin/SuperMod.dll"

[dependencies]
"bml.core" = ">=0.4.0"
"bml.render" = { version = "^1.0.0" }
"com.example.physics" = { version = "~2.1.3", optional = true }
"com.example.utils" = ">1.0.0"  # Requires version greater than 1.0.0

capabilities = [
  "com.example.supermod.hud",
  "com.example.supermod.telemetry"
]
```

## Troubleshooting

If parsing fails, the loader reports the offending file, line, and column via `ManifestParseError`. Common causes:
- Missing `[package]` table.
- Non-semver `version` strings (e.g., `1.0a`, `abc`). Note that partial versions like `1` or `1.0` are accepted.
- Negative version numbers (e.g., `-1.0.0`).
- Dependency table missing the `version` field.
- `capabilities` declared as a table instead of an array.

Keep this document in sync with `ModManifest.cpp` whenever new fields or validation rules are introduced.

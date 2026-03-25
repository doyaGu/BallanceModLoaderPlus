# Mod Manifest Schema (mod.toml)

Every v0.4.0+ module must ship a `mod.toml` manifest beside its DLL. The microkernel reads this file during discovery to decide load order, resolve dependencies, and validate metadata.

## File Placement

Place `mod.toml` at the root of the module directory (e.g. `ModLoader/Mods/MyCoolMod/mod.toml`). All paths referenced inside the file (such as `entry`) are resolved relative to this directory.

### Distributing as `.bp` archives

- v0.4 modules can be zipped and renamed to the `.bp` extension, then dropped directly into the `Mods/` directory.
- During discovery the loader extracts each archive into `Mods/.bp-cache/<archive-name>/` before parsing `mod.toml`, so you do **not** need to unpack them manually.
- Keep `mod.toml` in the root of the archive. The loader tolerates a single leading folder (when you zipped an entire directory) and will automatically descend into it.
- Avoid shipping unrelated files at the top level of the archive; everything inside the extracted folder is treated as part of the module payload.

## Recognized Top-Level Fields

The parser recognizes these top-level table names:

```
package, dependencies, conflicts, requires, interfaces, assets
```

Any other top-level key must be a TOML table (used for custom fields). Top-level scalars and arrays outside `[package]` are rejected.

---

## `[package]` table (required)

| Field          | Type             | Required | Notes |
|----------------|------------------|----------|-------|
| `id`           | string           | Yes      | Unique identifier (reverse-DNS or simple slug). Must be non-empty and must not contain whitespace. |
| `name`         | string           | Yes      | Human-readable display name. |
| `version`      | string (semver)  | Yes      | Parsed with the semantic version parser (`major.minor.patch[-prerelease]`). Supports prerelease suffixes like `1.0.0-alpha` or `2.0.0-rc.1`. Invalid versions abort loading. |
| `entry`        | string           | No       | DLL filename or relative path. Defaults to `<id>.dll` when omitted. Must be a relative path that stays within the module directory (absolute paths and `../` traversal are rejected). |
| `description`  | string           | No       | Free-form text shown in UIs/logs. |
| `authors`      | array of strings | No       | Each entry must be a non-empty string. Non-string entries are rejected. |
| `capabilities` | array of strings | No       | Feature tags declared by this module. Each entry must be a non-empty string. Queried at runtime via `BML_RequestCapability` / `BML_CheckCapability`. |

### Additional rules

- `version` must be a valid semantic version. The parser accepts both full (`major.minor.patch`) and partial (`major` or `major.minor`) version strings, treating missing components as `0`. Prerelease suffixes (e.g., `-alpha`, `-beta.1`, `-rc.2`) are also supported.
- Negative version numbers are not allowed and will cause parsing to fail.
- `id` is used as the capability handle and log filename, so prefer lowercase + dots (e.g. `com.example.supermod`).
- `entry` is resolved relative to the directory containing `mod.toml`. Absolute paths and paths that escape the module directory (e.g. `../outside.dll`) are rejected at parse time.

## `[dependencies]` table (optional)

Dependencies are expressed as a TOML table whose keys are the target module IDs. Each key must be non-empty and must not contain whitespace. Each value may be either:

1. **String shorthand** — interpreted as a semantic version range expression.
   ```toml
   [dependencies]
   "com.bml.ui" = ">=0.4.0"
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

Declare modules that must **not** be loaded together with the current one. Each key must be non-empty and must not contain whitespace. The syntax mirrors the dependency table:

| Field     | Type         | Required | Notes |
|-----------|--------------|----------|-------|
| Key       | string       | Yes      | Module ID to block when present. |
| Value     | string/table | Yes      | Version constraint and metadata. |
| `version` | string       | No       | Semantic version range. Omit or set to `"*"` to match all versions. |
| `reason`  | string       | No       | Human-readable explanation surfaced in diagnostics. |

Examples:

```toml
[conflicts]
"legacy.loader" = "*"                      # Any version conflicts
"com.example.renderer" = { version = "^2", reason = "Requires v1 renderer API" }
```

During resolution the loader checks each conflict entry against the installed modules. If the version range matches, the entire bootstrap halts with a descriptive error so players immediately know which pair of mods cannot co-exist.

## `[requires]` table (optional)

Declare interface-level dependencies. Unlike `[dependencies]` which resolves against module IDs, `[requires]` resolves against interface IDs exported by other modules via `[interfaces]`. Each key must be non-empty and must not contain whitespace.

1. **String shorthand**:
   ```toml
   [requires]
   "bml.ui.draw_registry" = ">=1.0.0"
   ```
2. **Table form**:
   ```toml
   [requires]
   "bml.menu.runtime" = { version = ">=1.0.0", optional = true }
   ```

Validation rules:
- `version` (string) is required in table form.
- `optional` defaults to `false` when omitted.
- The dependency resolver will find the module that exports the required interface and add a load-order edge (the provider loads first).
- If no module exports the interface and the requirement is not optional, bootstrap halts with an error.
- A module may require its own exported interface without creating a cycle.

## `[interfaces]` table (optional)

Declare interfaces that this module exports for other modules to consume via `[requires]`. Each key must be non-empty and must not contain whitespace.

1. **String shorthand**:
   ```toml
   [interfaces]
   "bml.ui.draw_registry" = "1.0.0"
   ```
2. **Table form**:
   ```toml
   [interfaces]
   "bml.menu.runtime" = { version = "1.0.0", description = "Menu framework runtime API" }
   ```

Validation rules:
- `version` must be a valid semantic version string (not a range).
- `description` is optional free-form text.
- Interface IDs must not collide with any module ID. If a collision is detected, the dependency resolver treats it as a hard error.
- Duplicate interface IDs across modules are also a hard error.
- After a module attaches, the loader verifies that each declared interface was actually registered via the extension API. A mismatch produces a warning.

## `[assets]` table (optional)

Declare asset paths for the module.

| Field   | Type   | Required | Notes |
|---------|--------|----------|-------|
| `mount` | string | No       | Relative path to an asset directory within the module. Must be non-empty, relative, and stay within the module directory. |

```toml
[assets]
mount = "data"
```

Validation rules:
- `[assets]` must be a TOML table (not a scalar or array).
- `mount` must be a non-empty string when present.
- Absolute paths and `../` traversal are rejected.

## Custom fields

Any top-level TOML table that is not one of the recognized sections is collected into a flattened `custom_fields` map. This allows modules to embed their own configuration metadata.

```toml
[mymod]
greeting = "Hello"
max_retries = 3
debug = true

[mymod.advanced]
deep_value = "found it"
```

These are flattened with dot-separated keys: `mymod.greeting`, `mymod.max_retries`, `mymod.debug`, `mymod.advanced.deep_value`.

Supported value types: **string**, **integer**, **float**, **boolean**.

Rejected value types:
- **Arrays** within custom tables are not supported and will cause a parse error.
- **Date/time** TOML types are not supported and will cause a parse error.

Top-level custom keys that are not tables (e.g. a bare `custom_value = "hello"` or `tags = ["a", "b"]`) are rejected.

## Generated metadata

During parsing, the loader augments the manifest with:
- `manifest_path`: absolute path to `mod.toml`.
- `directory`: directory that contains the manifest; used as the base for entry and asset paths.
- Parsed `SemanticVersion` and normalized dependency constraints.

These fields are internal and not part of the TOML schema, but explaining them clarifies error messages.

## Complete example

```toml
[package]
id = "com.example.supermod"
name = "Super Mod"
version = "1.2.0-beta.1"
authors = ["Example Studios", "Jane Dev"]
capabilities = ["com.example.supermod.hud", "com.example.supermod.telemetry"]
description = "Adds a HUD overlay and rich diagnostics"
entry = "bin/SuperMod.dll"

[dependencies]
"com.bml.ui" = ">=0.4.0"
"com.bml.render" = { version = "^1.0.0" }
"com.example.physics" = { version = "~2.1.3", optional = true }

[conflicts]
"legacy.loader" = "*"

[requires]
"bml.ui.draw_registry" = ">=1.0.0"
"bml.menu.runtime" = { version = ">=1.0.0", optional = true }

[interfaces]
"com.example.supermod.api" = "1.0.0"
"com.example.supermod.telemetry" = { version = "1.0.0", description = "Telemetry data API" }

[assets]
mount = "data"

[supermod]
theme = "dark"
max_panels = 4
```

## Troubleshooting

If parsing fails, the loader reports the offending file, line, and column via `ManifestParseError`. Common causes:

- Missing `[package]` table.
- Non-semver `version` strings (e.g., `1.0a`, `abc`). Note that partial versions like `1` or `1.0` are accepted.
- Negative version numbers (e.g., `-1.0.0`).
- IDs containing whitespace (spaces, tabs, newlines) in package, dependency, conflict, requires, or interfaces keys.
- `entry` or `[assets] mount` using absolute paths or `../` directory traversal.
- Dependency/requires table missing the `version` field.
- `capabilities` declared at the top level instead of inside `[package]`.
- `[assets]` declared as a non-table value.
- Custom field sections containing arrays or date/time values.
- Top-level scalars or arrays that are not recognized section names.
- Interface ID colliding with an existing module ID.
- Duplicate interface ID declared by multiple modules.

Keep this document in sync with the manifest parser whenever new fields or validation rules are introduced.

# BML+ Updater Design And Rules

This document records the current updater design. It describes what is already
implemented and the release rules that must be followed for official packages.

## Scope

`Bin/Updater.exe` updates BML+ runtime files only. It is not a mod manager.

Allowed scope:

- `BuildingBlocks/BMLPlus.dll`
- related BML+ runtime DLLs, currently `BuildingBlocks/AngelScript.dll`
- runtime assets such as `ModLoader/Fonts/unifont.otf`
- top-level release files such as `LICENSE`, `README.md`, `README_zh-CN.md`

Forbidden scope:

- `ModLoader/Mods/**`
- `ModLoader/Configs/**`
- `ModLoader/Updater/**`, except updater state written by the updater itself
- user saves, screenshots, logs, and unknown user files
- `Bin/Updater.exe` in v1 updater packages

The first updater release does not self-update `Bin/Updater.exe`. If the updater
itself must be updated, users bootstrap it through the manual package. A future
self-update design must use a separate two-stage flow.

## Runtime Layout

Runtime executable:

```text
Bin/Updater.exe
```

Updater state under each Ballance install:

```text
ModLoader/Updater/
  state.json
  sources.json
  pending.json
  installed.manifest.json
  downloads/<channel>/<version>/
  transactions/<timestamp-id>/
  logs/
```

`state.json` is status only. The trusted installed file set is
`installed.manifest.json`.

## CLI Surface

The updater is CLI-only for v1.

Commands:

```text
Updater.exe overview
Updater.exe status
Updater.exe doctor
Updater.exe configure <https-base-url> [--channel stable|beta]
Updater.exe source [show]
Updater.exe source set <https-base-url> [--channel stable|beta]
Updater.exe source clear
Updater.exe check [-c stable|beta]
Updater.exe download [-c stable|beta] [--force]
Updater.exe plan [-c stable|beta] [--force]
Updater.exe plan-remote [-c stable|beta] [--force]
Updater.exe apply-remote [-c stable|beta] [--force]
Updater.exe update [-c stable|beta] [--force]
Updater.exe verify-local <package>
Updater.exe plan-local <package>
Updater.exe apply-local <package>
Updater.exe rollback
Updater.exe --help
```

No command should wait for a key press after printing output. CLI commands must
print and exit.

## Game Root Detection

When launched from the installed location:

```text
<game-root>/Bin/Updater.exe
```

the updater treats `<game-root>` as the Ballance install root.

When testing a build-tree updater, do not run it directly from a path ending in
`Bin/Updater.exe`; that makes the build directory look like the game root. Copy
the executable to a temporary path that is not under a `Bin` directory, then run
it with the Ballance install as the working directory:

```powershell
$runner = "C:\path\to\Updater-test-runner.exe"
Copy-Item "C:\path\to\build\Bin\Updater.exe" $runner -Force
Push-Location "C:\Users\kakut\Games\Ballance"
& $runner check
Pop-Location
```

## Package Types

There are three different release package types. Do not mix them.

### Manual Install Package

Name:

```text
BMLPlus-vX.Y.Z.zip
```

Purpose:

- used by users for manual install or bootstrap
- may include `Bin/Updater.exe`
- includes `ModLoader/Updater/sources.json`
- includes BML+ runtime files

Rules:

- must not include starter mods as updater-managed files
- current package policy: no `Docs/**` and no `Templates/**` in the plain
  manual package
- docs and templates belong in SDK packages

### Updater Package

Name:

```text
BMLPlus-Update-vX.Y.Z.zip
```

Purpose:

- used only by `Updater.exe`
- not a manual install package

Rules:

- must not contain `Bin/Updater.exe`
- must not contain `ModLoader/Updater/**`
- must not contain `ModLoader/Mods/**`
- must not contain `ModLoader/Configs/**`
- every zip entry must appear in the signed manifest `managedFiles`
- package hash must equal `manifest.package.sha256`

### SDK Packages

Names:

```text
BMLPlus-SDK-vX.Y.Z-Release.zip
BMLPlus-SDK-vX.Y.Z-Debug.zip
```

Purpose:

- developer packages
- may include docs, templates, headers, import libs, CMake config, and debug PDBs

## Fixed Updater Source

The updater source is a stable channel endpoint. It must not be a versioned
release asset URL.

Current official source:

```text
https://doyagu.github.io/BallanceModLoaderPlus/updates
```

The updater reads:

```text
<baseUrl>/stable.json
<baseUrl>/stable.json.sig
```

GitHub Pages is served from the `gh-pages` branch. The updater channel files
live under `/updates/`:

```text
gh-pages
  README.md
  index.html
  .nojekyll
  updates/
    index.html
    stable.json
    stable.json.sig
```

`stable.json` points to versioned release assets:

```json
{
  "schemaVersion": 1,
  "version": "v0.3.12",
  "packageUrl": "https://github.com/doyaGu/BallanceModLoaderPlus/releases/download/v0.3.12/BMLPlus-Update-v0.3.12.zip",
  "manifestUrl": "https://github.com/doyaGu/BallanceModLoaderPlus/releases/download/v0.3.12/BMLPlus-Update-v0.3.12.manifest.json",
  "revokedVersions": [],
  "revokedManifestHashes": []
}
```

When releasing `v0.3.13`, the user's local source remains unchanged. Only
`gh-pages:/updates/stable.json` and `gh-pages:/updates/stable.json.sig` are
updated to point at `v0.3.13` release assets.

## `sources.json`

Manual packages install:

```text
ModLoader/Updater/sources.json
```

Official contents:

```json
{
  "schemaVersion": 1,
  "baseUrl": "https://doyagu.github.io/BallanceModLoaderPlus/updates",
  "defaultChannel": "stable"
}
```

Rules:

- `baseUrl` must be HTTPS.
- `baseUrl` is the fixed channel source, not a versioned package location.
- `defaultChannel` is normally `stable`.
- Users may override it with `Updater.exe source set`.

## Signing Scheme

The updater uses ECDSA P-256 over SHA-256.

Signed objects:

- update manifest: `BMLPlus-Update-vX.Y.Z.manifest.json`
- channel file: `stable.json`

Signature files:

```text
BMLPlus-Update-vX.Y.Z.manifest.json.sig
stable.json.sig
```

Signature format:

- base64 text
- raw 64-byte ECDSA P-256 signature in IEEE P1363 format: `r || s`
- signature is over the SHA-256 digest of the exact JSON file bytes

The public key is embedded in `Updater.exe`. The production private key must not
be committed to the repository and must not be uploaded to GitHub Actions.

Current production signing method is scheme B:

- CI may build unsigned artifacts.
- The production signing key lives only on the local release machine.
- Official release packages and channel files are signed locally.
- The signed artifacts are uploaded to GitHub releases.
- The `gh-pages` branch is updated with signed channel files under `updates/`.

## Remote Update Flow

`check`:

1. Read `ModLoader/Updater/sources.json`.
2. Fetch `<baseUrl>/<channel>.json`.
3. Fetch `<baseUrl>/<channel>.json.sig`.
4. Verify channel signature.
5. Validate HTTPS package and manifest URLs.
6. Check `revokedVersions`.
7. Compare latest version with trusted installed manifest.
8. Print whether an update is available.

`download`:

1. Run the same fresh signed channel validation as `check`.
2. Download manifest and manifest signature.
3. Verify manifest signature.
4. Check manifest hash against `revokedManifestHashes`.
5. Check manifest version equals channel version.
6. Download package.
7. Check package hash equals `manifest.package.sha256`.
8. Verify package zip entries and staged file hashes.
9. Store files under `ModLoader/Updater/downloads/<channel>/<version>/`.

`apply-remote`:

1. Requires a fresh signed channel in the same command path.
2. Does not trust old cached remote packages by themselves.
3. Builds an apply plan from the verified package.
4. Checks write access.
5. Backs up replaced or removed files.
6. Applies files from staging.
7. Writes `installed.manifest.json` and `state.json`.

`update`:

```text
check -> download -> plan -> apply
```

Same trusted installed version is skipped by default. Use `--force` to reinstall
the same version and refresh `installed.manifest.json`.

## Local Package Flow

Local packages are supported for testing and recovery:

```powershell
Bin\Updater.exe verify-local BMLPlus-Update-vX.Y.Z.zip
Bin\Updater.exe plan-local BMLPlus-Update-vX.Y.Z.zip
Bin\Updater.exe apply-local BMLPlus-Update-vX.Y.Z.zip
```

The local package must be an updater package, not the manual install package.
Manual packages are rejected because their file set does not match the updater
manifest rules.

## Path Safety Rules

Every manifest path and zip entry path must pass strict validation.

Reject:

- absolute paths
- backslashes
- drive prefixes such as `C:`
- UNC paths
- `\\?\` paths
- ADS paths containing `:`
- `.` or `..` segments
- trailing dots or spaces
- DOS reserved names
- duplicate normalized paths

The zip wrapper extracts only into staging. It never writes directly into the
game root.

## `removeFiles`

`removeFiles` is supported, but only for files that were previously trusted.

Rules:

- the path must be present in the previous trusted `installed.manifest.json` or
  a matching signed baseline manifest
- the current file hash must match the previous trusted hash
- the file must not be under a preserved path
- the file must be backed up before removal

If there is no trusted previous manifest and no matching baseline manifest, the
remove operation is skipped and reported diagnostically.

## Transactions And Rollback

Every apply creates a transaction under:

```text
ModLoader/Updater/transactions/<timestamp-id>/
```

The transaction stores backups and enough metadata to roll back replaced files.

Apply rules:

- verify manifest and package before staging
- build the plan before touching game files
- backup every replaced or removed file
- write pending state only as a recovery hint
- do not trust `pending.json` as authority; re-verify package and manifest
- on failure, attempt automatic rollback
- keep manual `rollback` available for recovery

## Revocation Rules

The signed channel file may contain:

```json
{
  "revokedVersions": [],
  "revokedManifestHashes": []
}
```

Rules:

- a revoked version must not be applied
- a revoked manifest hash must not be applied
- cached downloads do not bypass revocation
- remote apply requires a fresh signed channel

## Release Source Rules

Do:

- keep `sources.json` pointed at the fixed GitHub Pages `/updates` URL
- keep package and manifest URLs versioned under GitHub releases
- update and sign `stable.json` for every official release
- upload the signed channel files to both the release assets and
  `gh-pages:/updates/` for inspection/reproducibility

Do not:

- use `https://github.com/.../releases/download/vX.Y.Z` as `sources.json.baseUrl`
- use `https://raw.githubusercontent.com/.../updater-source` as the official
  source for new packages
- point a channel at a manual install package
- publish an updater package without a matching manifest and signature
- update the release tag before the public artifacts have been tested

# BML+ Release Packaging And Deployment Process

This document records the official BML+ release packaging flow. It covers
CKAngelScript input, MSVC builds, package types, production signing, fixed
updater source deployment, and real update validation.

## Required Locations

Repository:

```text
C:\Users\kakut\Works\Ballance\BallanceModLoaderPlus
```

Ballance install used for validation:

```text
C:\Users\kakut\Games\Ballance
```

CKAngelScript local repository:

```text
C:\Users\kakut\Works\Virtools\CKAngelScript
```

BML+ CKAngelScript release staging used by current packaging:

```text
C:\Users\kakut\Works\Ballance\BallanceModLoaderPlus\build-ci-ckas-release
```

Official updater source:

```text
https://doyagu.github.io/BallanceModLoaderPlus/updates
```

Production signing key name on the local release machine:

```text
BMLPlus-Updater-Production-v1
```

Use a `vX.Y.Z` version string, for example `v0.3.12`.

## Release Artifacts

Official release assets:

```text
BMLPlus-vX.Y.Z.zip
BMLPlus-Update-vX.Y.Z.zip
BMLPlus-Update-vX.Y.Z.manifest.json
BMLPlus-Update-vX.Y.Z.manifest.json.sig
BMLPlus-SDK-vX.Y.Z-Release.zip
BMLPlus-SDK-vX.Y.Z-Debug.zip
stable.json
stable.json.sig
SHA256SUMS.txt
```

Package rules:

- `BMLPlus-vX.Y.Z.zip` is the manual install/bootstrap package.
- `BMLPlus-Update-vX.Y.Z.zip` is only for `Updater.exe`.
- `BMLPlus-SDK-*` packages are for developers.
- The plain manual package must not include `Docs/**` or `Templates/**`.
- SDK packages may include docs and templates.
- The updater package must not include `Bin/Updater.exe`,
  `ModLoader/Updater/**`, `ModLoader/Mods/**`, or `ModLoader/Configs/**`.

## Production Signing Model

Use scheme B.

The production private key stays on the local release machine. GitHub Actions
does not receive the private key.

CI can build unsigned artifacts, but official update trust is created locally:

1. Build or obtain final binaries.
2. Run `scripts\Package-BMLRelease.ps1` locally with
   `-SigningCngKeyName BMLPlus-Updater-Production-v1`.
3. Generate and sign `stable.json` locally.
4. Upload signed artifacts to the GitHub release.
5. Push signed channel files to `gh-pages:/updates/`.

The updater trusts only the embedded public key, signed manifests, signed
channel files, and package hashes.

## Verify Production Key

Check the local CNG key before packaging:

```powershell
$key = [System.Security.Cryptography.CngKey]::Open("BMLPlus-Updater-Production-v1")
$key.Algorithm
$key.Dispose()
```

The algorithm must be `ECDSA_P256`.

## Build With MSVC

Use MSVC x86. Do not build release packages with MinGW.

Visual Studio 2022 path currently used:

```text
C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat
```

Example release build from a clean release worktree:

```powershell
$wt = "C:\Users\kakut\Works\Ballance\BallanceModLoaderPlus-release"
$vs = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
$ckas = "C:\Users\kakut\Works\Ballance\BallanceModLoaderPlus\build-ci-ckas-release"
$sdk = "C:\Users\kakut\Works\Virtools\Virtools-SDK-2.1"

cmd /c "`"$vs`" -arch=x86 && cmake -S `"$wt`" -B `"$wt\build-release-msvc`" -G Ninja -DCMAKE_BUILD_TYPE=Release -DVIRTOOLS_SDK_PATH=`"$sdk`" -DBML_ENABLE_ANGELSCRIPT=ON -DCKANGELSCRIPT_ROOT=`"$ckas`" -DCMAKE_INSTALL_PREFIX=`"$wt\dist-release-msvc`" && cmake --build `"$wt\build-release-msvc`" && cmake --install `"$wt\build-release-msvc`" --prefix `"$wt\dist-release-msvc`""
```

Debug build:

```powershell
cmd /c "`"$vs`" -arch=x86 && cmake -S `"$wt`" -B `"$wt\build-debug-msvc`" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DVIRTOOLS_SDK_PATH=`"$sdk`" -DBML_ENABLE_ANGELSCRIPT=ON -DCKANGELSCRIPT_ROOT=`"$ckas`" -DCMAKE_INSTALL_PREFIX=`"$wt\dist-debug-msvc`" && cmake --build `"$wt\build-debug-msvc`" && cmake --install `"$wt\build-debug-msvc`" --prefix `"$wt\dist-debug-msvc`""
```

## Package

Use the fixed updater source for `sources.json`:

```powershell
$version = "v0.3.12"
$repo = "C:\Users\kakut\Works\Ballance\BallanceModLoaderPlus"
$wt = "C:\Users\kakut\Works\Ballance\BallanceModLoaderPlus-release"
$out = "$wt\package-$version"

& "$wt\scripts\Package-BMLRelease.ps1" `
  -Version $version `
  -ReleaseInstallDir "$wt\dist-release-msvc" `
  -DebugInstallDir "$wt\dist-debug-msvc" `
  -ReleaseBinaryDir "$wt\build-release-msvc\bin" `
  -DebugBinaryDir "$wt\build-debug-msvc\bin" `
  -OutputDir $out `
  -CKAngelScriptRuntimeDir "$repo\build-ci-ckas-release" `
  -IncludeAngelScript `
  -UpdaterBaseUrl "https://doyagu.github.io/BallanceModLoaderPlus/updates" `
  -UpdaterDefaultChannel stable `
  -SigningCngKeyName "BMLPlus-Updater-Production-v1"
```

The package script signs:

```text
BMLPlus-Update-vX.Y.Z.manifest.json.sig
```

It does not sign `stable.json`; generate that separately.

## Generate `stable.json`

The fixed source branch contains channel files. `stable.json` points at the
versioned release assets.

Example for `v0.3.12`:

```powershell
$version = "v0.3.12"
$out = "C:\Users\kakut\Works\Ballance\BallanceModLoaderPlus-release\package-$version"
$base = "https://github.com/doyaGu/BallanceModLoaderPlus/releases/download/$version"

$stable = [ordered]@{
  schemaVersion = 1
  version = $version
  packageUrl = "$base/BMLPlus-Update-$version.zip"
  manifestUrl = "$base/BMLPlus-Update-$version.manifest.json"
  revokedVersions = @()
  revokedManifestHashes = @()
}

$stablePath = Join-Path $out "stable.json"
$text = ($stable | ConvertTo-Json -Depth 6) + "`n"
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($stablePath, $text, $utf8NoBom)
```

Sign `stable.json` with the production CNG key:

```powershell
$key = [System.Security.Cryptography.CngKey]::Open("BMLPlus-Updater-Production-v1")
$ecdsa = [System.Security.Cryptography.ECDsaCng]::new($key)
try {
  $bytes = [System.IO.File]::ReadAllBytes($stablePath)
  $sha = [System.Security.Cryptography.SHA256]::Create()
  try {
    $hash = $sha.ComputeHash($bytes)
  } finally {
    $sha.Dispose()
  }

  $sig = $ecdsa.SignHash($hash)
  if ($sig.Length -ne 64) {
    throw "unexpected signature length $($sig.Length)"
  }

  [System.IO.File]::WriteAllText(
    "$stablePath.sig",
    [Convert]::ToBase64String($sig),
    [Text.Encoding]::ASCII)
} finally {
  $ecdsa.Dispose()
  $key.Dispose()
}
```

## Generate Checksums

```powershell
Get-ChildItem -LiteralPath $out -File |
  Where-Object { $_.Name -ne "SHA256SUMS.txt" } |
  Sort-Object Name |
  ForEach-Object {
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant()
    "$hash  $($_.Name)"
  } |
  Set-Content -LiteralPath (Join-Path $out "SHA256SUMS.txt") -Encoding ascii
```

## Verify Package Contents

Manual package source and content:

```powershell
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zipPath = Join-Path $out "BMLPlus-$version.zip"
$zip = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
try {
  $entry = $zip.GetEntry("ModLoader/Updater/sources.json")
  $reader = New-Object System.IO.StreamReader($entry.Open())
  try {
    $reader.ReadToEnd()
  } finally {
    $reader.Dispose()
  }

  $zip.Entries |
    Where-Object { $_.FullName -match "^(Docs|Templates)/" } |
    Select-Object -First 10 -ExpandProperty FullName
} finally {
  $zip.Dispose()
}
```

Expected:

- `sources.json` uses the fixed GitHub Pages `/updates` URL
- no `Docs/` entries
- no `Templates/` entries

Verify updater package:

```powershell
$updater = "$wt\build-release-msvc\Bin\Updater.exe"
& $updater verify-local (Join-Path $out "BMLPlus-Update-$version.zip")
```

Expected:

```text
OK: local package verified
```

## Publish Release Assets

Upload the final assets to the GitHub release:

```powershell
$assets = @(
  "BMLPlus-$version.zip",
  "BMLPlus-Update-$version.zip",
  "BMLPlus-Update-$version.manifest.json",
  "BMLPlus-Update-$version.manifest.json.sig",
  "BMLPlus-SDK-$version-Release.zip",
  "BMLPlus-SDK-$version-Debug.zip",
  "stable.json",
  "stable.json.sig",
  "SHA256SUMS.txt"
) | ForEach-Object { Join-Path $out $_ }

gh release upload $version @assets --clobber --repo doyaGu/BallanceModLoaderPlus
```

## Publish Fixed Updater Source

Push `stable.json` and `stable.json.sig` to `gh-pages:/updates/`.

GitHub Pages serves the stable updater source. Release packages remain on
GitHub Releases; Pages contains only channel metadata.

```powershell
$repo = "C:\Users\kakut\Works\Ballance\BallanceModLoaderPlus"
$readmePath = Join-Path $env:TEMP "bmlplus-gh-pages-readme.md"
$indexPath = Join-Path $env:TEMP "bmlplus-gh-pages-index.html"
$nojekyllPath = Join-Path $env:TEMP "bmlplus-gh-pages-nojekyll"
$readmeText = "# BML+ update metadata`n`nSigned updater channel files are under /updates/.`n"
[System.IO.File]::WriteAllText($readmePath, $readmeText, [System.Text.UTF8Encoding]::new($false))
[System.IO.File]::WriteAllText($indexPath, "<!doctype html><meta charset=`"utf-8`"><title>BML+ updates</title><p>BML+ update metadata.</p>`n", [System.Text.UTF8Encoding]::new($false))
[System.IO.File]::WriteAllText($nojekyllPath, "", [System.Text.UTF8Encoding]::new($false))

$stableBlob = git -C $repo hash-object -w --no-filters (Join-Path $out "stable.json")
$sigBlob = git -C $repo hash-object -w --no-filters (Join-Path $out "stable.json.sig")
$readmeBlob = git -C $repo hash-object -w --no-filters $readmePath
$indexBlob = git -C $repo hash-object -w --no-filters $indexPath
$nojekyllBlob = git -C $repo hash-object -w --no-filters $nojekyllPath

$oldIndex = $env:GIT_INDEX_FILE
$tmpIndex = Join-Path $env:TEMP ("bmlplus-gh-pages-" + [guid]::NewGuid().ToString("N") + ".index")
$env:GIT_INDEX_FILE = $tmpIndex
try {
  git -C $repo read-tree --empty
  git -C $repo update-index --add --cacheinfo 100644,$readmeBlob,README.md
  git -C $repo update-index --add --cacheinfo 100644,$indexBlob,index.html
  git -C $repo update-index --add --cacheinfo 100644,$nojekyllBlob,.nojekyll
  git -C $repo update-index --add --cacheinfo 100644,$stableBlob,updates/stable.json
  git -C $repo update-index --add --cacheinfo 100644,$sigBlob,updates/stable.json.sig
  $tree = (git -C $repo write-tree).Trim()
  $commit = (git -C $repo commit-tree $tree -m "Publish updater channel $version").Trim()
} finally {
  if ($null -eq $oldIndex) {
    Remove-Item Env:GIT_INDEX_FILE -ErrorAction SilentlyContinue
  } else {
    $env:GIT_INDEX_FILE = $oldIndex
  }
  if (Test-Path -LiteralPath $tmpIndex) {
    Remove-Item -LiteralPath $tmpIndex -Force
  }
}

git -C $repo update-ref refs/heads/gh-pages $commit
git -C $repo push origin refs/heads/gh-pages --force
```

If GitHub Pages has not been enabled, enable it once with source branch
`gh-pages` and path `/`.

## Configure GitHub Repository Variables

These variables are used by CI packaging jobs so generated manual packages do
not embed a versioned release URL:

```powershell
gh variable set BML_UPDATER_BASE_URL `
  --body "https://doyagu.github.io/BallanceModLoaderPlus/updates" `
  --repo doyaGu/BallanceModLoaderPlus

gh variable set BML_UPDATER_DEFAULT_CHANNEL `
  --body "stable" `
  --repo doyaGu/BallanceModLoaderPlus
```

## Validate Public Release

Download the public manual package and check `sources.json`:

```powershell
$download = Join-Path $env:TEMP "BMLPlus-$version-source-check.zip"
Invoke-WebRequest `
  -UseBasicParsing `
  -Uri "https://github.com/doyaGu/BallanceModLoaderPlus/releases/download/$version/BMLPlus-$version.zip" `
  -OutFile $download

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($download)
try {
  $entry = $zip.GetEntry("ModLoader/Updater/sources.json")
  $reader = New-Object System.IO.StreamReader($entry.Open())
  try {
    $reader.ReadToEnd()
  } finally {
    $reader.Dispose()
  }
} finally {
  $zip.Dispose()
}
```

Run a real remote check from the validation game install:

```powershell
$runner = "$wt\Updater-test-runner.exe"
Copy-Item "$wt\build-release-msvc\Bin\Updater.exe" $runner -Force

Push-Location "C:\Users\kakut\Games\Ballance"
try {
  & $runner source set "https://doyagu.github.io/BallanceModLoaderPlus/updates" --channel stable
  & $runner check
} finally {
  Pop-Location
  Remove-Item -LiteralPath $runner -Force
}
```

Expected:

```text
channel signature verified
OK: remote channel checked
```

For a same-version real apply test:

```powershell
Push-Location "C:\Users\kakut\Games\Ballance"
try {
  & $runner update --force
  & $runner status
} finally {
  Pop-Location
}
```

Expected:

```text
OK: apply complete
installedVersion=vX.Y.Z
pendingTransaction=false
```

Then verify file hashes against `ModLoader/Updater/installed.manifest.json`.

## Move Tag After Validation

Only move the release tag after public artifacts have been validated.

```powershell
git tag -fa $version <commit-sha> -m "BML+ $version"
git push origin refs/tags/$version --force
git ls-remote origin refs/tags/$version "refs/tags/$version^{}"
```

For annotated tags, `refs/tags/<version>` is the tag object and
`refs/tags/<version>^{}` is the commit. The peeled commit must be the validated
release commit.

## Common Mistakes

- Do not use `https://github.com/.../releases/download/vX.Y.Z` as
  `sources.json.baseUrl`. That URL changes every release.
- Do not use `https://raw.githubusercontent.com/.../updater-source` for new
  official packages. Use GitHub Pages `/updates`.
- Do not put `Docs/**` or `Templates/**` in the plain manual package.
- Do not test a build-tree `Bin\Updater.exe` directly, because it may treat the
  build directory as the game root.
- Do not publish an updater package without matching manifest and signature.
- Do not upload the production private key to GitHub Actions.
- Do not move the release tag before the public update path has been tested.
- Do not use MinGW for official release builds; use MSVC x86.
